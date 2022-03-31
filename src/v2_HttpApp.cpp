#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"

namespace v2
{

HttpApp::HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData ) :
    App( settings, servData ),
    _server( this, HttpServerConfig(this->settings) )
{
    // Æô¶¯·şÎñ
    _server.startup(
        eiennet::ip::EndPoint( _server.config.serverIp, _server.config.serverPort ),
        _server.config.threadCount,
        _server.config.listenBacklog,
        _server.config.serverWait,
        _server.config.verboseInterval,
        _server.config.verbose
    );
}

int HttpApp::run( void * runParam )
{
    this->_runParam = runParam;

    return _server.run();
}

}
