# SimpleHttpServer

### 介绍
使用FastDo的异步任务功能编写的一个简易HTTP服务器，作为FastDo内置服务器的探索。

### 要求
#### Windows:

1. 安装 Microsoft Visual C++ 2017 x64
2. 安装 FastDo v0.6.1+

#### Linux:
1. 安装 GCC for C++ 4.8+
2. 安装 FastDo v0.6.1+

安装FastDo可以查看官方网站[作为C++库安装](https://fastdo.net/index.do?ps=document&doc_name=install_ascpp)教程。

注意，由于FastDo目前采取的是二进制发布，所以Windows上编译器版本应该要对应上才能编译使用。

### 示例

#### 简单应用
```C++
// 初始化Socket库
SocketLib initSock;
// 创建App，App需要传递一个配置对象，可以设置很多选项，一个空配置对象表示全部采用默认值
HttpApp app{ ConfigureSettings{} };

// 注册过径路由，过径路由是指URL只要路过就会触发，并且你可以控制是否继续。
// 第一个参数表示HTTP方法，"*"表示通配所有HTTP方法，也可以是逗号分隔的方法列表串，如：GET,POST,PUT
// 第二个参数表示路径，"/"是每一个URL都会过径的路径
app.crossRoute( "*", "/", [] ( auto clientCtx, eienwebx::Response & rsp, auto parts, auto i ) -> bool {
    // 执行一些操作
    return true; // 返回true表示继续查找并执行路由处理，返回false表示终止路由过程
} );

// 注册普通路由
// 第一个参数表示HTTP方法，"*"表示通配所有HTTP方法，也可以是逗号分隔的方法列表串，如：GET,POST,PUT
// 第二个参数表示路径
app.route( "GET,POST", "/hello", [] ( auto clientCtx, eienwebx::Response & rsp ) {
    rsp << "Hello, SimpleHttpServer!\n";
} );

// 运行服务
app.run(nullptr);
```

#### JSON响应
```C++
app.route( "GET", "/json", [] ( auto clientCtx, eienwebx::Response & rsp ) {
    Mixed v;
    v.addPair()
        ( "status", "Running..." )
        ( "error", 0 )
    ;

    rsp << v;
} );
```

#### 获取GET,POST,COOKIES变量
```C++
app.route( "GET,POST", "/vars", [] ( auto clientCtx, eienwebx::Response & rsp ) {
    eienwebx::Request & req = rsp.request;
    rsp << "GET: a = " << req.get["a"] << endl;
    rsp << "POST: b = " << req.post["b"].toInt() << endl;
    rsp << "COOKIES: c = " << req.cookies["c"] << endl;
} );
```

#### 文件上传
```C++
app.route( "POST", "/upload", [] ( auto clientCtx, eienwebx::Response & rsp ) {
    eienwebx::Request & req = rsp.request;
    Mixed & myfile = req.post["myfile"];
    if ( myfile["path"] )
    {
        // 从临时路径拷贝到自定义的路径
        FilePutContentsEx( CombinePath( "my-uploads", myfile["name"] ), FileGetContentsEx( myfile["path"], false ), false );
        rsp << "Upload ok!";
        // 删除临时文件
        CommonDelete(myfile["path"]);
    }
} );
```

#### 数据库查询
```C++
app.route( "*", "/database", [] ( auto clientCtx, eienwebx::Response & rsp ) {
    Mixed dbConfig;
    dbConfig.createCollection();
    dbConfig["driver"] = "mysql";
    dbConfig["host"] = "localhost";
    dbConfig["user"] = "root";
    dbConfig["pwd"] = "123";
    dbConfig["dbname"] = "test";
    dbConfig["charset"] = "utf-8";

    Database db{dbConfig};
    auto rs = db->query("select * from mytable1");
    Mixed row;
    while ( rs->fetchRow(&row) )
    {
        rsp << row["field1"] << ", " << row["field2"] << ", " << row["field3"] << "<br/>\n";
    }
} );
```
