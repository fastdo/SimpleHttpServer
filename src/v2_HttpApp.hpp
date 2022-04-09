#pragma once

namespace v2
{

class HttpApp : public eienwebx::App
{
public:
    /** \brief 构造函数1 */
    HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData = nullptr );

    virtual int run( void * runParam ) override;

    void onWebMainHandler( HttpServer::WebMainHandlerFunction handler ) { _server.onWebMainHandler(handler); }

    /** \brief 注册过径路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void setCrossRouteHandler( winux::String const & method, winux::String const & path, HttpServer::CrossRouteHandlerFunction handler ) { _server.setCrossRouteHandler( method, path, handler ); }

    /** \brief 注册普通路由处理器 method可以是*表示通配所有HTTP方法，path需以/开头 */
    void setRouteHandler( winux::String const & method, winux::String const & path, HttpServer::RouteHandlerFunction handler ) { _server.setRouteHandler( method, path, handler ); }

    /** \brief 暴露HttpServer对象 */
    HttpServer & getServer() { return _server; }
protected:
    HttpServer _server;

    friend class HttpClientCtx;
};

}
