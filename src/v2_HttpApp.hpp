#pragma once

namespace v2
{

class HttpApp : public eienwebx::App
{
public:
    /** \brief 构造函数1 */
    HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData = nullptr );

    virtual int run( void * runParam ) override;

    void onWebPageHandler( HttpServer::WebPageHandlerFunction handler ) { _server.onWebPageHandler(handler); }

protected:
    HttpServer _server;

    friend class HttpClientCtx;
};

}
