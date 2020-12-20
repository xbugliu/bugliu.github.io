+++
title = "Octopress Custom 404"
date = "2013-08-04"
slug = "2013/08/04/octopress-custom-404"
Categories = ["404","公益404","octopress","博客","blog"]
description = "octopress定制公益404页面"
+++
一向不认为自己是一个有社会责任感的人，但为什么要接入公益404页面？那是因为我的博客托管在Github上，有一次输错自己博客文章的链接，一下子跑到了Github的默认404上，感觉很突兀。所以自己想定制一个404，而现在公益404很流行，举手之劳，何乐不为。

##1. 创建404页面
首先，需要创建自定义的404页面，最初我是这样创建的，将404错误页面创建到source目录下的error目录
```bash
rake new_page[error/404.html]
```
创建一个新的目录error专门保存错误页面，有两个好处。1是存储结构清晰，2是可以在robots.txt将整个目录Disallow掉。

但后来看到github自定义404页面的[说明][1]，404页面必须放在网站根目录下，所以必须将404页面创建到source目录下：
```bash
rake new_page[404.html]
```

##2. Robots文件隐藏404页面
404页面异常页面，所谓家丑不可外扬，此页面无需搜索引擎爬虫知道。编辑robots.txt文件，添加404页面的Disallow：
```bash
+++
+++
User-agent: *
Disallow: /404.html

Sitemap: {{ site.url }}/sitemap.xml 
```
##3. 网站地图中隐藏掉404页面
同样网站地图也不需要包含添加404页面。打开plugins/sitemap_generator.rb，找到EXCLUDED_FILES，然后添加404.html。
```ruby
# Change SITEMAP_FILE_NAME if you would like your sitemap file
# to be called something else
SITEMAP_FILE_NAME = "sitemap.xml"

# Any files to exclude from being included in the sitemap.xml
EXCLUDED_FILES = ["atom.xml", "404.html"]

# Any files that include posts, so that when a new post is added, the last
# modified date of these pages should take that into account
PAGES_INCLUDE_POSTS = ["index.html"]
```

##4. 定制公益404页面
我选择的是[腾讯公益404][2]，进入这个[网站][2]拷贝其提供的JS代码，只需将其嵌入到我们的404页面(404.html)即可。下面是我修改过的404页面，把header、footer、comments等都去掉，只显示一个标题和公益广告：
```bash
+++
title = "Octopress Custom 404"
header: false
date = "2013-08-04"
slug = "2013/08/04/octopress-custom-404"
sharing: false
footer: false
+++
<center><h1>ToWriting.com 404!</h1></center>
<script type="text/javascript" src="http://www.qq.com/404/search_children.js?edition=small" charset="utf-8"></script>
```



  [1]:https://help.github.com/articles/custom-404-pages
  [2]:http://www.qq.com/404/
