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

1. 暴力检索。部分小团队（包括我们在19年）会采用这种方案，其主要方式就是将特征和相关数据都写入一个（或多个）文件中，然后检索时加载到内存。此方案好在简单，坏在暴力，在总数据量在百万以下可以应付，但无法应对千万以上的数据，耗时会在数分钟，是完全不可接受的。

2. [最邻近搜索][3]。KNN或ANN, 有大量优秀的开源项目，比如[faiss][4]。这些开源项目主要是以库的形式存在，提供基础的检索接口，基于这些库进行简单的封装即可实现一个满足千万数据的单机检索系统，检索耗时相比暴力会有极大的提升。

3. 分布式最邻近搜索。集成最邻近搜索库、特征存储管理、标签过滤、数据分区、数据备份、满足ACID的全功能、支持多机部署的系统。能满足几十亿特征的管理和检索。此类的开源项目有[vearch][5], [milvus][6], 闭源的有阿里达摩院的[Proxima][7]

## 三、Vearch介绍


### 术语

* Shared-Nothing  各子系统独立（数据、运算）的分布式方案。
* LSM-Tree 内存排序，顺序写入段，追加更新，写吞吐量高的文件存储算法，RocksDB的底层算法。
* B-tree  基于页的，覆盖写入，读友好的文件存储算法，常用于关系数据库索引。
* IVFPQ 一种ANN算法，核心的思路是分桶和降维，在可接受的精度损失下极大的提高检索的速度。
* ACID 可靠存储要求的三个特性，原子性、隔离性、持久性。
* WAL 保证单机场景数据的持久性。
* Raft 分布式一致性协议，保证多机场景数据的一致性。

### 架构

![image](d:\static\images\posts\vearch\arch.png "vearch的架构")


* Router 接口层，将特征的增删改查都转发给PartitionServer（简称PS）, 并合并检索结果返回给调用者。不存储任何信息，无状态。
* Master 元数据管理，通过etcd记录PS节点信息，database等信息
* PartitionServer 数据存储检索服务，可以部署多个实例，Shared-Nothing架构。

PS服务，采用golang+cpp编写，cpp编写的引擎名字叫做gamma, 属于vearch的核心，包括如下组件：
1. vector_manager 特征管理，支持rocksdb、memory、mmap三种存储方式，rocksdb适用于海量特征的保存，比mmap更灵活，数据安全性更高。memory适用于小数据量特征的管理，会定期存盘。
2. ivfpq_index ivfqp索引，基于faiss, 重新实现PQ倒排表部分代码，来支持实时索引。
3. id_map 业务特征ID到vearch内部docid的映射表，基于hashmap。
4. table-data 标签字段（比如时间戳）存储管理，基于mmap文件。
5. field_range_index 标签索引，基于B-Tree，负责检索时标签过滤。
6. del_bitmap 已删除特征管理，基于bitmap。

### 使用方法

vearch有database、space、document三种资源，分别对应于数据库的库、表、条目。space不支持更改表结构。三种资源分别有增删改查的http[接口][8]。database和space访问的是master服务，document访问的是router服务。插入数据流程比较简单，先创建database, 然后创建space, 最后向space中插入document。检索接口调用的是router服务。接口都较简单，不再示例，另外我们内部使用的是grpc接口，此部分没有文档，需要自己看代码研究。

下面是创建space的请求体，其中有几个参数需要特别说明：
* retrieval_type - 索引类型，vearch支持多种索引，目前我们主要采用IVFPQ
* partition_num - 分区的个数，一个space可以有多个分区
* nprobe - 去多少个桶里面检索，nprobe越大，召回率越高
* ncentroids - IVF时分桶的个数，不影响召回率，桶越多，检索QPS会越高，但初始内存占用越大
* nsubvector - PQ切片的个数，nsubvector越大，精度损失越小，但内存占用越大。假设nsubvector为32，512个float被压缩为32个Byte, 相当于压缩了512*4/32=64倍。
* bucket_init_size - PQ倒排索引，每个桶里面初始大小，IVFPQ索引初始占用内存为 `ncentroids*bucket_init_size*nsubvector`

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

### 关键代码解析 (基于3.2.x)

**创建space**

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

master服务的clusterAPI.createSpace是创建space的入口，其中会调用masterService的createSpaceService方法。

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

createSpaceService函数有关键的几个过程，filterAndSortServer和generatePartitionsInfo返回按包含Partition数从小到大排序的PS节点的地址，然后通过client.CreatePartition将请求发送到PS节点创建Partition，最后调用updateSpace将meta信息存储到etcd中。

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

  ## 省略部分不重要的代码

  int ret_vec = vec_manager_->CreateVectorTable(table, meta_jp);
  TableParams disk_table_params;
  if (meta_jp) {
    utils::JsonParser table_jp;
    meta_jp->GetObject("table", table_jp);
    disk_table_params.Parse(table_jp);
  }
  int ret_table = table_->CreateTable(table, disk_table_params);
  indexing_size_ = table.IndexingSize();
  if (ret_vec != 0 || ret_table != 0) {
    LOG(ERROR) << "Cannot create table!";
    return -2;
  }

  af_exector_ = new AsyncFlushExecutor();
  table_io_ = new TableIO(table_);
  int ret = table_io_->Init();
  if (ret) {
    return ret;
  }
  af_exector_->Add(static_cast<AsyncFlusher *>(table_io_));

  if (!meta_jp) {
    utils::JsonParser dump_meta_;
    dump_meta_.PutInt("version", 320);  // version=3.2.0

    utils::JsonParser table_jp;
    table_->GetDumpConfig()->ToJson(table_jp);
    dump_meta_.PutObject("table", std::move(table_jp));

    utils::JsonParser vectors_jp;
    for (auto &it : vec_manager_->RawVectors()) {
      DumpConfig *dc = it.second->GetDumpConfig();
      if (dc) {
        utils::JsonParser jp;
        dc->ToJson(jp);
        vectors_jp.PutObject(dc->name, std::move(jp));
      }
    }
    dump_meta_.PutObject("vectors", std::move(vectors_jp));

    utils::FileIO fio(dump_meta_path);
    fio.Open("w");
    string meta_str = dump_meta_.ToStr(true);
    fio.Write(meta_str.c_str(), 1, meta_str.size());
  }
  for (auto &it : vec_manager_->RawVectors()) {
    RawVectorIO *rio = it.second->GetIO();
    if (rio == nullptr) continue;
    AsyncFlusher *flusher = dynamic_cast<AsyncFlusher *>(rio);
    if (flusher) {
      af_exector_->Add(flusher);
    }
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
  TableSchemaIO tio(path);  // rewrite it if the path is already existed
  if (tio.Write(table)) {
    LOG(ERROR) << "write table schema error, path=" << path;
  }

  af_exector_->Start();

  LOG(INFO) << "create table [" << table_name << "] success!";
  created_table_ = true;
  return 0;
}
```
创建table的流程在gamma引擎中的GammaEngine::CreateTable方法，里面初始化了vec_manager, table_data, field_range_index, vec_manager里面又初始化了rocksdb_vector和ivfpqindex。

**document插入**

vearch用document表示一条记录，记录中除了有特征(vector)以外，还可以包含其他字段，一条document样子：

```json
{
"taskid":14,
"timestamp":1637487823,
"feature0":{
  "feature":[
    0.88658684,
    0.9873159,
    0.68632215,
    0.53622203
  ]
 }
}
```

```golang
func (docService *docService) addDoc(ctx context.Context, args *vearchpb.AddRequest) *vearchpb.AddResponse {
	reply := &vearchpb.AddResponse{Head: newOkHead()}
	request := client.NewRouterRequest(ctx, docService.client)
	docs := make([]*vearchpb.Document, 0)
	docs = append(docs, args.Doc)
	request.SetMsgID().SetMethod(client.BatchHandler).SetHead(args.Head).SetSpace().SetDocs(docs).SetDocsField().PartitionDocs()
	if request.Err != nil {
		return &vearchpb.AddResponse{Head: setErrHead(request.Err)}
	}
	items := request.Execute()
	reply.Head.Params = request.GetMD()
	if len(items) < 1 {
		return &vearchpb.AddResponse{Head: setErrHead(request.Err)}
	}
	if items[0].Err != nil {
		reply.Head.Err = items[0].Err
	}
	reply.PrimaryKey = items[0].GetDoc().GetPKey()
	return reply
}
```
router服务中的docService.addDoc是添加document的入口，第6行的PartitionDocs里面会根据document的id寻找一个PS节点，然后在第10行将请求通过RPC发送给PS节点。

```cpp
int GammaEngine::AddOrUpdate(Doc &doc) {
#ifdef PERFORMANCE_TESTING
  double start = utils::getmillisecs();
#endif
  std::vector<struct Field> fields_table, fields_vec;
  std::string key;

  std::vector<struct Field> &fields = doc.Fields();
  fields_table.reserve(fields.size());
  for (auto &field : fields) {
    if (field.datatype != DataType::VECTOR) {
      const string &name = field.name;
      if (name == "_id") {
        key = field.value;
      }
      fields_table.emplace_back(std::move(field));
    } else {
      fields_vec.emplace_back(std::move(field));
    }
  }
  // add fields into table
  int docid = -1;
  table_->GetDocIDByKey(key, docid);
  if (docid == -1) {
    int ret = table_->Add(fields_table, max_docid_);
    if (ret != 0) return -2;
#ifndef BUILD_GPU
    for (size_t i = 0; i < fields_table.size(); ++i) {
      struct Field &field = fields_table[i];
      int idx = table_->GetAttrIdx(field.name);
      field_range_index_->Add(max_docid_, idx);
    }
#endif  // BUILD_GPU
  } else {
    if (Update(docid, fields_table, fields_vec)) {
      LOG(ERROR) << "update error, key=" << key << ", docid=" << docid;
      return -3;
    }
    return 0;
  }
#ifdef PERFORMANCE_TESTING
  double end_table = utils::getmillisecs();
#endif

  // add vectors by VectorManager
  if (vec_manager_->AddToStore(max_docid_, fields_vec) != 0) {
    return -4;
  }
  if (not b_running_ and index_status_ == UNINDEXED) {
    if (max_docid_ >= indexing_size_) {
      LOG(INFO) << "Begin indexing.";
      this->BuildIndex();
    }
  }
  ++max_docid_;
#ifdef PERFORMANCE_TESTING
  double end = utils::getmillisecs();
  if (max_docid_ % 10000 == 0) {
    LOG(INFO) << "table cost [" << end_table - start << "]ms, vec store cost ["
              << end - end_table << "]ms";
  }
#endif
  return 0;
}



```

经过一系列中间函数调用后，最终会执行到gamma引擎的GammaEngine::AddOrUpdate方法，这里面主要做了这些事情：

1. 25行table_->Add 将标签字段数据写入table_data, 第二个参数max_docid_会作为这一条document的docid, docid是gamma内部ivfpq_index, table, field_index使用的id, 是一个从0开始递增的值，会通过id_map将业务的documentid与内部docid关联。
2. 31行是将标签字段放入标签索引field_range_index中
3. 46行vec_manager_->AddToStore将特征保存到rocksdb(或内存)，并同时将特征插入ivfpq索引


```cpp
bool GammaIVFPQIndex::Add(int n, const uint8_t *vec) {

  ## 省略部分代码

  idx_t *idx0 = new idx_t[n];
  quantizer->assign(n, applied_vec, idx0);
  idx = idx0;
  del_idx.set(idx);

  uint8_t *xcodes = new uint8_t[n * code_size];
  utils::ScopeDeleter<uint8_t> del_xcodes(xcodes);

  const float *to_encode = nullptr;
  utils::ScopeDeleter<float> del_to_encode;
  
  if (by_residual) {
    to_encode = compute_residuals(quantizer, n, applied_vec, idx);
    del_to_encode.set(to_encode);
  } else {
    to_encode = applied_vec;
  }
  pq.compute_codes(to_encode, xcodes, n);

  size_t n_ignore = 0;
  long vid = indexed_vec_count_;
  for (int i = 0; i < n; i++) {
    long key = idx[i];
    assert(key < (long)nlist);
    if (key < 0) {
      n_ignore++;
      continue;
    }

    // long id = (long)(indexed_vec_count_++);
    uint8_t *code = xcodes + i * code_size;

    new_keys[key].push_back(vid++);

    size_t ofs = new_codes[key].size();
    new_codes[key].resize(ofs + code_size);
    memcpy((void *)(new_codes[key].data() + ofs), (void *)code, code_size);
  }

  /* stage 2 : add invert info to invert index */
  if (!rt_invert_index_ptr_->AddKeys(new_keys, new_codes)) {
    return false;
  }
  indexed_vec_count_ = vid;
#ifdef PERFORMANCE_TESTING
  add_count_ += n;
  if (add_count_ >= 100000) {
    double t1 = faiss::getmillisecs();
    LOG(INFO) << "Add time [" << (t1 - t0) / n << "]ms, count "
              << indexed_vec_count_;
    // rt_invert_index_ptr_->PrintBucketSize();
    add_count_ = 0;
  }
#endif
  return true;
}
```

GammaIVFPQIndex::Add是IVFPQ索引的添加过程，此时索引已经训练完成，这里面有关键的三步：
1. 第6行quantizer->assign 根据特征值计算应该插入到哪个桶中
2. 第22行pq.compute_codes，计算PQ编码的值
3. 第45行rt_invert_index_ptr_->AddKeys，将编码插入到ivfpq索引中去

内存中IVFQP倒排索引的布局如下图所示：
![image](d:\static\images\posts\vearch\ivfpq.png "ivfpq索引内存布局")


**document删除**

```cpp
int GammaEngine::Delete(std::string &key) {
  int docid = -1, ret = 0;
  ret = table_->GetDocIDByKey(key, docid);
  if (ret != 0 || docid < 0) return -1;

  if (bitmap::test(docids_bitmap_, docid)) {
    return ret;
  }
  ++delete_num_;
  bitmap::set(docids_bitmap_, docid);
  table_->Delete(key);

  vec_manager_->Delete(docid);

  return ret;
}
```

document的删除比较简单，只是用一个bitmap记录删除的docid, 并定期会将bitmap存盘。

**检索** 

```golang
func (docService *docService) bulkSearch(ctx context.Context, args []*vearchpb.SearchRequest) *vearchpb.SearchResponse {

	request := client.NewRouterRequest(ctx, docService.client)
	request.SetMsgID().SetMethod(client.BulkSearchHandler).SetHead(args[0].Head).SetSpace().BulkSearchByPartitions(args)
	if request.Err != nil {
		return &vearchpb.SearchResponse{Head: setErrHead(request.Err)}
	}

	sortOrders := make([]sortorder.SortOrder, 0, len(args))
	for _, req := range args {
		sortOrder := make([]sortorder.Sort, 0, len(req.SortFields))
		for _, sortF := range req.SortFields {
			sortOrder = append(sortOrder, &sortorder.SortField{Field: sortF.Field, Desc: sortF.Type})
		}
		sortOrders = append(sortOrders, sortOrder)
	}

	searchResponse := request.BulkSearchSortExecute(sortOrders)

	if searchResponse == nil {
		return &vearchpb.SearchResponse{Head: setErrHead(request.Err)}
	}
	if searchResponse.Head == nil {
		searchResponse.Head = newOkHead()
	}
	if searchResponse.Head.Err == nil {
		searchResponse.Head.Err = newOkHead().Err
	}

	return searchResponse
}
```

检索的入口在Router服务的docService.bulkSearch (对应vearch的Msearch接口，指多目标批量检索)，逻辑比较清晰，主要是根据检索的条件查询待检索的PS服务地址，然后将查询条件通过RPC传给PS实例，PS返回结果后合并后返回给调用者。

```cpp
int GammaEngine::Search(Request &request, Response &response_results) {

## 省略非核心代码

#ifndef BUILD_GPU
  MultiRangeQueryResults range_query_result;
  std::vector<struct RangeFilter> &range_filters = request.RangeFilters();
  size_t range_filters_num = range_filters.size();

  std::vector<struct TermFilter> &term_filters = request.TermFilters();
  size_t term_filters_num = term_filters.size();
  if (range_filters_num > 0 || term_filters_num > 0) {
    int num = MultiRangeQuery(request, gamma_query.condition, response_results,
                              &range_query_result);
    if (num == 0) {
      return 0;
    }
  }
#ifdef PERFORMANCE_TESTING
  gamma_query.condition->GetPerfTool().Perf("filter");
#endif
#endif

  size_t vec_fields_num = vec_fields.size();
  if (vec_fields_num > 0) {
    GammaResult gamma_results[req_num];
    int doc_num = GetDocsNum();

    for (int i = 0; i < req_num; ++i) {
      gamma_results[i].total = doc_num;
    }

    ret = vec_manager_->Search(gamma_query, gamma_results);
    if (ret != 0) {
      string msg = "search error [" + std::to_string(ret) + "]";
      for (int i = 0; i < req_num; ++i) {
        SearchResult result;
        result.msg = msg;
        result.result_code = SearchResultCode::SEARCH_ERROR;
        response_results.AddResults(std::move(result));
      }
      return -3;
    }

#ifdef PERFORMANCE_TESTING
    gamma_query.condition->GetPerfTool().Perf("search total");
#endif
    PackResults(gamma_results, response_results, request);
#ifdef PERFORMANCE_TESTING
    gamma_query.condition->GetPerfTool().Perf("pack results");
#endif

	## 省略非核心代码

  return ret;
}
```

经过一系列函数调用，最终会执行到gamma引擎中的GammaEngine::Search方法，此方法是PS节点中检索的主流程，关键过程非成三步：
1. 13行MultiRangeQuery 首先是根据查询条件的标签进行过滤，其结果是过滤结果的bitmap。其过程类似于Mysql Innodb索引的查询过程，但不同的是多标签采用的是多个单个索引+结果合并的方式，而Mysql推荐的是联合索引。
2. 33行是调用底层IVFPQ进行检索
3. 后面的代码主要是根据结果的docid列表，获取document信息，打包返回。

```cpp
int GammaIVFPQIndex::Search(RetrievalContext *retrieval_context, int n,
                            const uint8_t *x, int k, float *distances,
                            idx_t *labels) {

  ## 省略无关代码

  if (retrieval_params->IvfFlat() == true) {
    quantizer->search(n, xq, nprobe, coarse_dis.get(), idx.get());
  } else {
    quantizer->search(n, applied_xq, nprobe, coarse_dis.get(), idx.get());
  }
  this->invlists->prefetch_lists(idx.get(), n * nprobe);

  if (retrieval_params->IvfFlat() == true) {
    // just use xq
    search_ivf_flat(retrieval_context, n, xq, k, idx.get(), coarse_dis.get(),
                    distances, labels, nprobe, false);
  } else {
    search_preassigned(retrieval_context, n, xq, applied_xq, k, idx.get(), coarse_dis.get(),
                       distances, labels, nprobe, false);
  }
  return 0;
}

void GammaIVFPQIndex::search_preassigned(
    RetrievalContext *retrieval_context, int n, const float *x, const float *applied_x, int k,
    const idx_t *keys, const float *coarse_dis, float *distances, idx_t *labels,
    int nprobe, bool store_pairs, const faiss::IVFSearchParameters *params) {
  

  ## 省略非关键代码
	// loop over probes
	for (int ik = 0; ik < nprobe; ik++) {
		nscan += scan_one_list(
			scanner, keys[i * nprobe + ik], coarse_dis[i * nprobe + ik],
			recall_simi, recall_idxi, recall_num, this->nlist, this->invlists,
			store_pairs, retrieval_params->IvfFlat());

		if (max_codes && nscan >= max_codes) break;
	}

	ndis += nscan;
	compute_dis(k, vec_q + i * d, simi, idxi, recall_simi, recall_idxi, recall_num,
				context->has_rank, metric_type, vector_, retrieval_context);
	}
}  // namespace tig_gamma


size_t scan_one_list(GammaInvertedListScanner *scanner, idx_t key,
                     float coarse_dis_i, float *simi, idx_t *idxi, int k,
                     idx_t nlist, faiss::InvertedLists *invlists,
                     bool store_pairs, bool ivf_flat,
                     MemoryRawVector *mem_raw_vec = nullptr) {
  if (key < 0) {
    // not enough centroids for multiprobe
    return 0;
  }
  if (key >= (idx_t)nlist) {
    LOG(INFO) << "Invalid key=" << key << ", nlist=" << nlist;
    return 0;
  }

  size_t list_size = invlists->list_size(key);

  // don't waste time on empty lists
  if (list_size == 0) {
    return 0;
  }

  std::unique_ptr<faiss::InvertedLists::ScopedIds> sids;
  const idx_t *ids = nullptr;

  if (!store_pairs) {
    sids.reset(new faiss::InvertedLists::ScopedIds(invlists, key));
    ids = sids->get();
  }

  scanner->set_list(key, coarse_dis_i);

  // scan_codes need uint8_t *
  const uint8_t *codes = nullptr;

  if (ivf_flat) {
    codes = reinterpret_cast<uint8_t *>(mem_raw_vec);
  } else {
    faiss::InvertedLists::ScopedCodes scodes(invlists, key);
    codes = scodes.get();
  }
  scanner->scan_codes(list_size, codes, ids, simi, idxi, k);

  return list_size;
};

template <class SearchResultType>
void scan_list_with_table(size_t ncode, const uint8_t *codes,
                          SearchResultType &res) const {
  size_t j = 0;
  for (; j < ncode; j++) {
    if (res.ids[j] & realtime::kDelIdxMask) {
      codes += this->pq.M;
      continue;
    }

    if (!retrieval_context_->IsValid(res.ids[j] &
                                      realtime::kRecoverIdxMask)) {
      codes += this->pq.M;
      continue;
    }

    float dis = this->dis0;
    const float *tab = this->sim_table;

    for (size_t m = 0; m < this->pq.M; m++) {
      dis += tab[*codes++];
      tab += this->pq.ksub;
    }

    res.add(j, dis);
  }
  assert(j == ncode);
}

bool IsValid(int id) const override {
  int docid = raw_vec->VidMgr()->VID2DocID(id);
  if ((range_query_result != nullptr && not range_query_result->Has(docid)) ||
      bitmap::test(docids_bitmap, docid) == true) {
    return false;
  }
  return true;
};



```

GammaIVFPQIndex::Search是IVFPQ的检索过程：
1. 第10行，quantizer->search先进行一次基于桶的粗查询，返回和目标特征最近的nprobe个桶
2. 第34行，scan_one_list去特定的桶里面去扫描topN（recall_num）的特征
3. 第49行，scan_one_list函数体里面，能看到首先是取某个桶的PQ码表数据，然后开始扫描。
4. 第95行，scan_list_with_table是扫描的过程，首先是retrieval_context_->IsValid判断docid是否是有效的，这个用到了前面标签过滤的结果bitmap(第124行)，然后113行是PQ的核心代码，查询PQ编码subvector中心点和目标特征的距离, 然后将他们累加求和，其值可以理解目标特征到当前特征的距离，最后将结果进行堆排序保留topN的docid。

```cpp
void compute_dis(int k, const float *xi, float *simi, idx_t *idxi,
                 float *recall_simi, idx_t *recall_idxi, int recall_num,
                 bool has_rank, faiss::MetricType metric_type,
                 VectorReader *vec, RetrievalContext *retrieval_context) {
  if (has_rank == true) {
    ScopeVectors scope_vecs;
    std::vector<idx_t> vids(recall_idxi, recall_idxi + recall_num);
    vec->Gets(vids, scope_vecs);
    int raw_d = vec->MetaInfo()->Dimension();
    for (int j = 0; j < recall_num; j++) {
      if (recall_idxi[j] == -1) continue;
      float dis = 0;
      const float *vec = reinterpret_cast<const float *>(scope_vecs.Get(j));
      if (metric_type == faiss::METRIC_INNER_PRODUCT) {
        dis = faiss::fvec_inner_product(xi, vec, raw_d);
      } else {
        dis = faiss::fvec_L2sqr(xi, vec, raw_d);
      }

      if (retrieval_context->IsSimilarScoreValid(dis) == true) {
        if (metric_type == faiss::METRIC_INNER_PRODUCT) {
          if (HeapForIP::cmp(simi[0], dis)) {
            faiss::heap_pop<HeapForIP>(k, simi, idxi);
            long id = recall_idxi[j];
            faiss::heap_push<HeapForIP>(k, simi, idxi, dis, id);
          }
        } else {
          if (HeapForL2::cmp(simi[0], dis)) {
            faiss::heap_pop<HeapForL2>(k, simi, idxi);
            long id = recall_idxi[j];
            faiss::heap_push<HeapForL2>(k, simi, idxi, dis, id);
          }
        }
      }
    }
    reorder_result(metric_type, k, simi, idxi);
  } 
}
```

IVFPQ检索的最后一步compute_dis方法是精确排序的过程，其主要目的是降低IVFPQ过程中精度损失的影响，其原理比较简单，根据前面返回的docid列表，取原始特征重新进行一次排序。


## 四、Vearch落地

### Bugfix

虽然vearch的整体架构是可以满足我们的需求，但存在着一些Bug，这些Bug分成三类：
1. 数据破坏问题。 PS进程异常退出有概率数据破坏，大部分表现为启动时Crash。根源是代码未支持ACID中的原子性和持久性。我们分别遇到Raft日志文件破坏、Table数据和Vector数据不一致、索引文件破坏、ps meta文件破坏等问题，针对这些问题我们进行了修复，但仅保证了原子性，暂未保证持久性。

2. 功能逻辑问题，这一类问题遇到的包括，范围索引出错、无法召回自己、批量删除无效等问题，相关问题已fix，部分代码提交官方。

3. 性能问题，主要存在特征入倒排索引慢，检索接口Response部分数据采用json导致OOM问题，日志写文件高频flush导致磁盘负载高。

### 改造

**分库分表**

分库方案是基于算法维度，相同的算法放到一库中。分表有2个方案，第一种是基于业务，比如布控库是一个表。第二种分表方案是按天分表，主要针对数据量极大的实时摄像头数据，按天分表才能高效的滚动删除、冷热数据隔离、按天快速检索。

分库分表不涉及对vearch代码的改动，仅是一种应用方案。
![image](d:\static\images\posts\vearch\db_space.png "vearch的分库分表")


**冷热隔离**

![image](d:\static\images\posts\vearch\memory.png "vearch内存占比")

上图是vearch内存占比图，其中ivfpq和标签索引内存占比较大。虽然IVFPQ之后内存占用相比原始特征已降低很多，但如果数月的实时数据都加载到内存中，还是不现实并浪费的，应该将一段时间之前的冷数据放在硬盘。其方案如下：

* table_data, MMap方案。table_data存储的是标签数据，其数据结构可以理解为数组，数组中的元素为标签的值，数组的index为docid, 这种结构很容易转换成MMap。
* rocksdb_vector, 通过参数调整：1. open改成OpenForReadOnly 2. 减小block_cache 3. 减小max_open_file。通过参数控制rocksdb默认占用的内存大小和运行时占用的内存大小。
* ivfpq-index, MMap方案。通过ivfpq索引内存布局可知，ivfqp的索引也是由数组构成，同样适宜切MMap。
* id_map, id_map存放的是业务的documentid到docid的映射，采用的是内存HashMap库[libcuckoo][9], 这里的改造方案是切换到基于硬盘的B-Tree。
* field_range_index, MMap方案。field_range_index基于Btree，主要的改造方案是把Btree里面原来存放在内存中的value，放到一个连续的MMap文件中，具体可以见下图：
![image](d:\static\images\posts\vearch\fileld_index.png "vearch field_index")


**预训练**

前面讲document的插入过程时，默认vearch已经训练完成，这里讲一下训练(train)，其核心是基于输入的样本，找到IVF桶的聚类中心点和PQ切片的聚类中心点。对于Vearch来说，每个Partition都需要单独的训练。训练是一个耗时很长的操作，因为我们采用按天分Space的方式，每天新的Space都要训练，训练的过程会持续数小时，此时只能走暴力检索，并且训练时CPU占用率极高，增加了系统负载。

所以预训练应运而生，预训练的改造方案是：
1. 导出基础索引文件，改造IVFPQ的Dump过程，只导出中心点，而不导出PQ编码数据。
2. 加载基础索引文件，改造IVFPQ的Load过程，跳过训练，直接加载第一步训练好的。

使用流程：
1. 测试人员搭建环境，上传离线的视频文件，预处理后，插入到vearch中。
2. 触发vearch的训练过程
3. 训练完成后，使用dump_index工具，导出基础索引
4. 将基础索引，随vearch镜像发布测试上线

**支持2个向量**

产生2个向量的原因是，1个向量的特征区分度不够，需要第2个向量来补充，距离计算公式为：`distance = sqrt(distance(feature1)) + alpha * sqrt(distance(feautre2))`，一种简单的加权。但ivfpq的原理是不支持2个特征的，需要将2个特征融合成一个特征，经过算法的推导，融合的方式为：`feature = feature1 + feature2 / sqrt(alpha)`，经验证排序结果和暴力计算一致。然后对vearch精确排序部分进行改造，保持计算出的分数可以和之前一致。


### 性能数据


**插入document**：

硬件环境：
* CPU：intel 银牌 4110 * 2
* 内存：256G
* 硬盘：INTEL SSDSC2KB01 SSD * 3

软件配置：
* 特征维度：2048
* 插入并发：5线程
* vearch配置：1个router, 3个ps
* space配置：nsubvector 128

测试结果：
* 插入耗时，平均：4.1ms, tp99: 437.6ms
* 插入Tps: 2068
* CPU占用率, ps: 800~900%, router：200%
* 硬盘占用，硬盘写入带宽 < 50MBps, util负载 < 20%。
* 内存占用，1600W数据量时，内存占用约6.5G

测试结论：
插入是一个CPU密集型操作，4110本身属于性能比较差的服务器CPU。升级CPU到金牌6230R插入耗时的tp99能降低到200+ms。 

**检索document**

硬件环境：
* CPU：intel 银牌 6226R * 2
* 内存：256G
* 硬盘：INTEL SSDSC2KB01 SSD * 3

软件配置：
* 特征维度：512
* vearch配置：1个router, 3个ps
* space配置：nsubvector 32, ncentroids 2048，nprobe: 64
* 检索参数：topN 60000
* 总数据量：5000W

测试结果：
* 检索耗时：5000W全量检索耗时约为3.6s
* 检索IO占用：磁盘带宽使用 1.4GB, 磁盘util> 95%
* CPU占用：PS服务总CPU峰值约 1500%
* 检索各阶段耗时占比：精排序读原始特征 60%，PQ扫描 20%, 标签过滤 15%
* 相对暴力召回率：> 98%

测试结论：

* 检索是一个CPU密集型和IO密集型的操作
* IO是检索耗时的瓶颈

**内存占用**

vearch的内存占用和space的配置有很大的关系，和数据的分布也有关系，我们目前在维度：512，nsubvector: 32, ncentroids为：2048时，2个Int类型的标签，每1000W的特征，内存占用约2.5~3G。

**冷热隔离**

1. 冷模式，每个Partition初始占用内存约400MB, 和数据量没有线性关系
2. 冷模式，平均检索耗时为普通模式的5-10倍（此处测试人力原因，测试数据不充足，也未区分首次和非首次）

### 未来的优化

1. 完善监控系统
2. 友好错误码
3. 支持ACID
4. 删除能回收（复用）空间
5. ivfpq索引文件支持增量更新

### 一些不足之处

1. 测试人力的不足，测试数据不够详尽。

**参考**:
* [蚂蚁金服 ZSearch 在向量检索上的探索](https://www.infoq.cn/article/rb1dzi4t69ivvqfvjco7)
* [深入理解大数据架构之——事务及其ACID特性](https://www.cnblogs.com/cciejh/p/acid.html)
* [IVFPQ算法原理](https://zhou-yuxin.github.io/articles/2020/IVFPQ%E7%AE%97%E6%B3%95%E5%8E%9F%E7%90%86/index.html)
* [Faiss索引文件格式详解](https://zhuanlan.zhihu.com/p/39803468)
* [负样本为王：评Facebook的向量化召回算法](https://zhuanlan.zhihu.com/p/165064102)

[1]: https://zh.wikipedia.org/zh-hans/%E4%BD%99%E5%BC%A6%E7%9B%B8%E4%BC%BC%E6%80%A7
[2]: https://zh.wikipedia.org/zh-hans/%E6%AC%A7%E5%87%A0%E9%87%8C%E5%BE%97%E8%B7%9D%E7%A6%BB
[3]: https://zh.wikipedia.org/wiki/%E6%9C%80%E9%82%BB%E8%BF%91%E6%90%9C%E7%B4%A2
[4]: https://github.com/facebookresearch/faiss
[5]: https://developer.aliyun.com/article/783110
[6]: https://github.com/milvus-io/milvus
[7]: https://developer.aliyun.com/article/783110
[8]: https://github.com/vearch/vearch/blob/master/docs/APILowLevel.md
[9]: https://github.com/efficient/libcuckoo