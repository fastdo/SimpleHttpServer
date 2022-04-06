#pragma once

namespace v2
{
class HttpApp;
class HttpClientCtx;

/** \brief HTTP服务器 */
class HttpServer : public Server
{
    // 处理一个WebMain逻辑
    _DEFINE_EVENT_RELATED(
        WebMain,
        ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ),
        ( httpClientCtxPtr, RSP )
    )

public:
    // 过径路由处理函数类型
    using CrossRouteHandlerFunction = std::function< bool ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP, winux::StringArray const & urlPathPartArr, size_t i ) >;
    // 终点路由处理函数类型
    using RouteHandlerFunction = std::function< void ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ) >;

    /** \brief 构造函数1，不会启动服务，必须手动调用startup() */
    HttpServer( HttpApp * app, HttpServerConfig const & httpConfig );

    /** \brief 构造函数2，会启动服务
     *
     *  \param app 应用对象
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 是否显示提示信息 */
    HttpServer(
        HttpApp * app,
        eiennet::ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0,
        double serverWait = 0.02,
        double verboseInterval = 1.0,
        bool verbose = true
    );

    /** \brief 注册过径路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void setCrossRouteHandler( winux::String const & method, winux::String const & path, CrossRouteHandlerFunction handler );

    /** \brief 注册普通路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void setRouteHandler( winux::String const & method, winux::String const & path, RouteHandlerFunction handler );

    /** \brief HTTP服务器配置对象 */
    HttpServerConfig config;
protected:
    virtual void onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 ) override;

    virtual ClientCtx * onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) override;

    // 应用程序对象指针
    HttpApp * _app;

    // 路由处理器
    // 过径路由器
    /*
        [ { pathpart: { GET: handleGet, POST: handlePost, ... }, ... }, ... ]
     */
    std::vector< std::unordered_map< winux::String, std::unordered_map< winux::String, CrossRouteHandlerFunction > > > _crossRouter;
    // 普通路由器
    /*
        { path: { GET: handleGet, POST: handlePost, ... }, ... }
     */
    std::unordered_map< winux::String, std::unordered_map< winux::String, RouteHandlerFunction > > _router;
private:
    void onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body );
};

}
