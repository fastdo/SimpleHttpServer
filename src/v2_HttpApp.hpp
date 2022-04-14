#pragma once

namespace v2
{
class HttpRequestCtx;

class HttpApp : public eienwebx::App, public eiennet::Server
{
    // 处理一个WebMain逻辑
    DEFINE_CUSTOM_EVENT(
        WebMain,
        ( winux::SharedPointer<HttpRequestCtx> requestCtxPtr, eienwebx::Response & RSP ),
        ( requestCtxPtr, RSP )
    )

public:
    // 过径路由处理函数类型
    using CrossRouteHandlerFunction = std::function< bool ( winux::SharedPointer<HttpRequestCtx> requestCtxPtr, eienwebx::Response & RSP, winux::StringArray const & urlPathPartArr, size_t i ) >;
    // 终点路由处理函数类型
    using RouteHandlerFunction = std::function< void ( winux::SharedPointer<HttpRequestCtx> requestCtxPtr, eienwebx::Response & RSP ) >;

    /** \brief 构造函数1 */
    HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData );

    /** \brief 构造函数2
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 是否显示提示信息
     *  \param cacheLifeTime 静态文件缓存时间 */
    HttpApp(
        winux::ConfigureSettings const & settings,
        struct xAppServerData * servData,
        eiennet::ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0,
        double serverWait = 0.02,
        double verboseInterval = 1.0,
        bool verbose = true,
        int cacheLifeTime = 86400
    );

    /** \brief 注册过径路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void crossRoute( winux::String const & method, winux::String const & path, CrossRouteHandlerFunction handler );

    /** \brief 注册普通路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void route( winux::String const & method, winux::String const & path, RouteHandlerFunction handler );

    /** \brief 运行 */
    virtual int run( void * runParam ) override;

    /** \brief HTTP服务器配置对象 */
    HttpServerConfig httpConfig;

protected:
    virtual void onClientDataArrived( winux::SharedPointer<eiennet::ClientCtx> clientCtxPtr, winux::Buffer data0 ) override;

    virtual eiennet::ClientCtx * onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) override;

    void onClientRequestInternal( winux::SharedPointer<HttpRequestCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body );

    // 静态文件缓存
    eiennet::StaticFileMemoryCache _staticFileCache;
    // 路由处理器
    // 过径路由器
    /* [ { pathpart: { GET: handleGet, POST: handlePost, ... }, ... }, ... ] */
    std::vector< std::unordered_map< winux::String, std::unordered_map< winux::String, CrossRouteHandlerFunction > > > _crossRouter;
    // 普通路由器
    /* { path: { GET: handleGet, POST: handlePost, ... }, ... } */
    std::unordered_map< winux::String, std::unordered_map< winux::String, RouteHandlerFunction > > _router;

    friend class HttpRequestCtx;
};

}
