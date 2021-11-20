---
title: "分布式向量检索系统Vearch应用实战"
date: 2021-10-07T19:29:25+08:00
draft: true
---

## 一、向量检索的工程特点

图像AI领域，其检索一般是基于特征。项目落地如何衡量优劣呢？主要关注两类指标，一类是算法效果，包括准确率、召回率等，一类是**工程效果**，包括耗时、资源占用。

### 1. 单个特征Size较大
具体到我们的REID算法，为了达到一个好的算法效果，采用了较高的维度（512维），这带来了单个特征Size较大(2K)，并且REID特征的不稳定性导致无法像人脸特征一样合并相同的特征。

### 2. 海量的冷数据
从统计规律看，项目中平均每路摄像头每天10W左右的特征数量(形体抓拍)。一个千路的项目，则每天产生的特征的大小为：`1000 * 10W * 512 * sizeof(float) = 200G`，客户一般要求数据保存6个月的数据，算一下是36T的特征，这个数据量是**海量**的。虽然总数据很大，但客户一般仅仅会检索最近7天的数据，而半个月前的数据往往是一直沉寂不用的。

### 3. 检索任务：耗时敏感、读IO密集型、计算密集型
检索是用户高频操作主流程，其速度直接关乎用户的体验。千万数据的检索可以1秒完成才算合格的产品体验。

检索的主要原理是从海量特征中对比查找和目标特征最相近的TopN的特征，而海量的数据导致检索任务：
* IO密集型，因为海量的数据注定不会全部加载内存，必然存在特征的按需从硬盘中加载。
* 计算密集型，特征的两两比对，一般采用[余弦距离][1]或者[欧式距离][2]，其算法复杂度和特征维度成O(N)的关系。而特征的检索，又和特征的总量成O(N)的关系。

IO和计算的密集型和检索速度是矛盾的，随着数据量的加大，势必严重制约着检索的速度。另外除了特征比对以外，检索需要支持**标签过滤**，能按一些业务维度比如时间、摄像头进行过滤。

### 4. 特征存储：顺序写入、不会修改、低频删除、低频回查

特征写入的TPS, 按实测数据每路摄像头1.2TPS, 峰值约为4TPS，千路的均值和峰值分别为1200TPS和4000TPS。单个特征大小2K, 千路的均值和峰值写入吞吐量分别为20Mbps和64Mbps。特征前后之间没有任何顺序关系，所以我们通过常规的缓存+顺序写入的方式是很容易满足性能要求的。

公安业务特征主要来自实时摄像头，特征不会修改，但需要删除，回收空间。业务上需要回查特征满足检索等产品需求。

## 二、向量检索系统方案

从前面向量检索系统的工程特点可知，一个优秀的向量检索系统需要具备**特征存储管理**和**检索**两项能力，并能应对**海量数据**的挑战。目前市面上主要方案如下：

1. 暴力检索。部分小团队（包括我们在19年）会采用这种方案，其主要方式就是将特征和相关数据都写入一个（或多个）文件中，然后检索时加载到内存。此方案好在简单，坏在暴力，在总数据量在百万一下可以应付，但无法应对千万以上的数据，耗时会在数分钟，是完全不可接受的。

2. [最邻近搜索][3]。KNN或ANN, 有大量优秀的开源项目，比如[faiss][4]。这些开源项目主要是以库的形式存在，提供基础的检索接口，基于这些库进行简单的封装即可实现一个满足千万数据的单机检索系统，检索耗时相比暴力会有极大的提升。

3. 分布式最邻近搜索。集成最邻近搜索库、特征存储管理、标签过滤、数据分区、数据备份、满足ACID的全功能、支持的多机部署的系统。能满足几十亿特征的管理和检索。此类的开源项目有[vearch][5], [milvus][6], 闭源的有阿里达摩院的[Proxima][7]

## 三、Vearch介绍


### 术语

* Shared-Nothing  各子系统独立（数据、运算）的分布式方案。
* LSM-Tree 内存排序，顺序写入段，追加更新，写吞吐量高的文件存储算法，RocksDB的底层算法。
* B-tree  基于页的，覆盖写入，读友好的文件存储算法，常用于关系数据库索引。
* IVFPQ 一种ANN算法，核心的思路是分桶和降维，可以在可接受的精度损失下极大的提高检索的速度。
* ACID 可靠存储要求的三个特性，原子性、隔离性、持久性。

### 架构


* Router 特征操作接口层，将特征的增删改查都转发给PartitionServer（简称PS）, 并合并检索结果返回给调用者。不存储任何信息，无状态。
* Master 元数据管理，通过etcd记录PS节点信息，database等信息
* PartitionServer 数据存储检索服务，可以部署多个实例，Shared-Nothing架构。

### 使用方法

vearch有database、space、document三种资源，分别对应于数据库的库、表、条目。space不支持更改表结构。三种资源分别有增删改查的http[接口][8]。database和space访问的是master服务，document访问的是router服务。插入数据流程比较简单，先创建database, 然后创建space, 最后向space中插入document。检索接口调用的是router服务。接口都较简单，不再示例，另外我们内部使用的是grpc接口，此部分没有文档，需要自己看代码研究。

下面是创建space的请求体，其中有几个参数需要特别说明：
* retrieval_type - 索引类型，vearch支持多种索引，目前我们仅仅用到IVFPQ
* partition_num - 分区的个数，一个space可以有多个分区
* nprobe - 去多少个桶里面检索，nprobe越大，召回率越高
* ncentroids - IVF时分桶的个数，不影响召回率，但桶越多，初始内存占用越大，QPS会更高
* nsubvector - PQ切片的个数，nsubvector越大，内存占用越大，但精度损失越小，假设nsubvector为32，512个float被压缩为32个Byte, 相当于压缩了512*4/32=64倍。
* bucket_init_size - PQ倒排索引，每个桶里面初始大小，IVFPA索引初始占用内存为 ncentroids*bucket_init_size*nsubvector

```json
{
    "name": "{{.name}}",
    "partition_num": 3,
    "replica_num": 1,
    "engine": {
        "name": "gamma",
        "id_type": "Long",
        "index_size": 100000,
        "max_size": 100000000,
        "nprobe": 64,
        "retrieval_type": "IVFPQ",
        "retrieval_param": {
            "metric_type": "{{.metric_type}}",
            "ncentroids": 2048,
            "nsubvector": 32,
            "bucket_init_size": 200,
            "bucket_max_size": 40960000
        }
    },
    "properties": {
        "timestamp": {
            "type": "integer",
            "index": true
        },
        "groupid": {
            "type": "integer",
            "index": true
        },
        "objectid":{
                "type": "long",
                "index": false
        },
        "extra": {
            "type": "integer",
            "index": false
        },
        "feature": {
            "type": "vector",
            "dimension": "{{.dimension}}",
            "store_type": "{{.store_type}}",
            "store_param": {
                "cache_size": 1000
            }
        }
    }
}
```

### 流程

### 创建space

```golang
func (ca *clusterAPI) createSpace(c *gin.Context) {
    ## 省略准备工作
    log.Debug("create space, db: %s", c.Param(dbName))
	if err := ca.masterService.createSpaceService(c, dbName, space); err != nil {
		log.Debug("create space, db: %s", c.Param(dbName))
		log.Error("createSpaceService err: %v", err)
		ginutil.NewAutoMehtodName(c).SendJsonHttpReplyError(err)
	} else {
		ginutil.NewAutoMehtodName(c).SendJsonHttpReplySuccess(space)
	}
}
```

首先是gin实现的接口，接收到接口请求后，会调用masterService的createSpaceService方法。

```golang
// server/[serverAddr]:[serverBody]
// spaceKeys "space/[dbId]/[spaceId]:[spaceBody]"
func (ms *masterService) createSpaceService(ctx context.Context, dbName string, space *entity.Space) (err error) {
	
    ## 省略准备工作

	//find all servers for create space
	servers, err := ms.Master().QueryServers(ctx)
	if err != nil {
		return err
	}


	serverPartitions, err := ms.filterAndSortServer(ctx, space, servers)
	if err != nil {
		return err
	}

	
	//peak servers for space
	var paddrs [][]string
	for i := 0; i < len(space.Partitions); i++ {
		if addrs, err := ms.generatePartitionsInfo(servers, serverPartitions, space.ReplicaNum, space.Partitions[i]); err != nil {
			return err
		} else {
			paddrs = append(paddrs, addrs)
		}
	}

	var errChain = make(chan error, 1)
	//send create space to every space
	for i := 0; i < len(space.Partitions); i++ {
		go func(addrs []string, partition *entity.Partition) {
			//send request for all server
			defer func() {
				if r := recover(); r != nil {
					err := fmt.Errorf("create partition err: %v ", r)
					errChain <- err
					log.Error(err.Error())
				}
			}()
			for _, addr := range addrs {
				if err := client.CreatePartition(addr, space, partition.Id); err != nil {
					err := fmt.Errorf("create partition err: %s ", err.Error())
					errChain <- err
					log.Error(err.Error())
				}
			}
		}(paddrs[i], space.Partitions[i])
	}

    //update version
	err = ms.updateSpace(ctx, space)
}
```

createSpaceService函数有关键的几个过程，filterAndSortServer和generatePartitionsInfo返回按拥有的Partition数从小到大排序的PS节点的地址，然后通过client.CreatePartition将请求发送到PS节点创建Partition，最后调用updateSpace将meta信息存储到etcd中。

```
func (c *CreatePartitionHandler) Execute(ctx context.Context, req *vearchpb.PartitionData, reply *vearchpb.PartitionData) error {
	reply.Err = &vearchpb.Error{Code: vearchpb.ErrorEnum_SUCCESS}
	space := new(entity.Space)
	err := cbjson.Unmarshal(req.Data, space)
	if err != nil {
		log.Error("Create partition failed, err: [%s]", err.Error())
		return vearchpb.NewError(vearchpb.ErrorEnum_RPC_PARAM_ERROR, err)
	}
	c.server.partitions.Range(func(key, value interface{}) bool {
		fmt.Print(key, value)
		return true
	})

	if partitionStore := c.server.GetPartition(req.PartitionID); partitionStore != nil {
		return vearchpb.NewError(vearchpb.ErrorEnum_PARTITION_DUPLICATE, nil)
	}

	if err := c.server.CreatePartition(ctx, space, req.PartitionID); err != nil {
		c.server.DeletePartition(req.PartitionID)
		return err
	}
	return nil
}
```

client.CreatePartition通过RPC会调用到PS实例的CreatePartitionHandler.Execute方法, Execute中会调用server.CreatePartition，创建Partition

```
// Start start the store.
func (s *Store) Start() (err error) {
	s.Engine, err = register.Build(s.Space.Engine.Name, register.EngineConfig{
		Path:        s.DataPath,
		Space:       s.Space,
		PartitionID: s.Partition.Id,
		DWPTNum:     config.Conf().PS.EngineDWPTNum,
	})
	if err != nil {
		return err
	}

	raftStore, err := wal.NewStorage(s.RaftPath, nil)
	if err != nil {
		s.Engine.Close()
		return fmt.Errorf("start partition[%d] open raft store engine error: %s", s.Partition.Id, err.Error())
	}


	return nil
}
```

创建Partition主流程在raftstore.Store.Start函数，里面有2个主要步骤，通过register.Build创建table和创建raftServer。

```cpp
int GammaEngine::CreateTable(TableInfo &table) {
  if (!vec_manager_ || !table_) {
    LOG(ERROR) << "vector and table should not be null!";
    return -1;
  }
  int ret_vec = vec_manager_->CreateVectorTable(table);
  int ret_table = table_->CreateTable(table);

  indexing_size_ = table.IndexingSize();

  if (ret_vec != 0 || ret_table != 0) {
    LOG(ERROR) << "Cannot create table!";
    return -2;
  }

#ifndef BUILD_GPU
  field_range_index_ = new MultiFieldsRangeIndex(index_root_path_, table_);
  if ((nullptr == field_range_index_) || (AddNumIndexFields() < 0)) {
    LOG(ERROR) << "add numeric index fields error!";
    return -3;
  }

  auto func_build_field_index = std::bind(&GammaEngine::BuildFieldIndex, this);
  std::thread t(func_build_field_index);
  t.detach();
#endif
  std::string table_name = table.Name();
  std::string path = index_root_path_ + "/" + table_name + ".schema";
  TableIO tio(path);  // rewrite it if the path is already existed
  if (tio.Write(table)) {
    LOG(ERROR) << "write table schema error, path=" << path;
  }
  LOG(INFO) << "create table [" << table_name << "] success!";
  created_table_ = true;
  return 0;
}
```
创建table的流程在gamma引擎中的GammaEngine::CreateTable方法，里面初始化了vec_manager, table_data, field_range_index, vec_manager里面又初始化了rocksdb_vector和ivfpqindex。

#### 特征插入


#### 检索 

#### 删除

## 四、Vearch落地

### Bugfix

虽然vearch的整体架构是可以满足我们的需求，但存在着一些Bug，这些Bug分成三类：
1. 数据破坏问题。 PS进程异常退出有概率数据破坏，大部分表现为启动时Crash。根源是代码未支持ACID中的原子性和持久性。我们分别遇到Raft日志文件破坏、Table数据和Vector数据不一致、索引文件破坏、ps meta文件破坏等问题，针对这些问题我们进行了修复，但仅保证了原子性，暂未保证持久性。

2. 功能逻辑问题，这一类问题遇到的包括，范围索引出错、无法召回自己、批量删除无效等问题，相关问题已fix，部分代码提交官方。

3. 性能问题，主要存在特征入倒排索引慢，检索接口Response部分数据采用json导致OOM问题，日志写文件高频flush导致磁盘负载高。

### 改造

* 分库分表

分库方案是基于算法维度，相同的算法放到一库中。分表有2个方案，第一种是基于业务，比如布控库是一个表。第二种分表方案是按天分表，主要针对数据量极大的实时摄像头数据，按天分表才能高效的滚动删除、冷热数据隔离、按天快速检索。

分库分表不涉及对vearch代码的改动，仅是一种应用方案。
![image](/images/posts/vearch/db_space.png "vearch的分库分表")


* 冷热隔离

虽然IVFPQ之后内存占用相比原始特征已降低很多，但如果半年以上的数据都加载到内存中，还是不现实并浪费的，应该将一段时间之前的冷数据放在硬盘。


* 预训练
* 支持2个向量


### 性能数据

### 未来的优化

1. 完善监控系统
2. 友好错误码
3. 支持ACID
4. 删除能回收（服用）空间
5. ivfpq索引文件支持增量更新

### 一些不足之处

1. 测试人力的不足，测试数据不够详尽。

**参考**:
* [蚂蚁金服 ZSearch 在向量检索上的探索](https://www.infoq.cn/article/rb1dzi4t69ivvqfvjco7)
* [深入理解大数据架构之——事务及其ACID特性](https://www.cnblogs.com/cciejh/p/acid.html)
* [IVFPQ算法原理](https://zhou-yuxin.github.io/articles/2020/IVFPQ%E7%AE%97%E6%B3%95%E5%8E%9F%E7%90%86/index.html)
* [Faiss索引文件格式详解](https://zhuanlan.zhihu.com/p/39803468)


[1]: https://zh.wikipedia.org/zh-hans/%E4%BD%99%E5%BC%A6%E7%9B%B8%E4%BC%BC%E6%80%A7
[2]: https://zh.wikipedia.org/zh-hans/%E6%AC%A7%E5%87%A0%E9%87%8C%E5%BE%97%E8%B7%9D%E7%A6%BB
[3]: https://zh.wikipedia.org/wiki/%E6%9C%80%E9%82%BB%E8%BF%91%E6%90%9C%E7%B4%A2
[4]: https://github.com/facebookresearch/faiss
[5]: https://developer.aliyun.com/article/783110
[6]: https://github.com/milvus-io/milvus
[7]: https://developer.aliyun.com/article/783110
[8]: https://github.com/vearch/vearch/blob/master/docs/APILowLevel.md