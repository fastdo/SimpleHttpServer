#pragma once

namespace v2
{
class HttpApp;
class HttpClientCtx;

/** \brief HTTP服务器 */
class HttpServer : public Server
{
    // 处理一个Web页面逻辑
    _DEFINE_EVENT_RELATED(
        WebPage,
        ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::App & APP, eienwebx::Request & REQ, eienwebx::Response & RSP ),
        ( httpClientCtxPtr, APP, REQ, RSP )
    )

public:

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

    /** \brief HTTP服务器配置对象 */
    HttpServerConfig config;
protected:
    virtual void onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 ) override;

    virtual ClientCtx * onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) override;

    // 应用程序对象指针
    HttpApp * _app;
private:
    void onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body );
};

}
