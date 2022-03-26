#pragma once

namespace v2
{

class HttpApp : public eienwebx::App
{
public:

    HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData = nullptr );

    virtual int run( void * runParam ) override;

    void onWebMainHandler( HttpServer::WebMainHandlerFunction handler ) { _server.onWebMainHandler(handler); }
protected:
    HttpServer _server;

};

}
