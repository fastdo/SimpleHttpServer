#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"
#include "v2_HttpClientCtx.hpp"

namespace v2
{

HttpApp::HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData ) :
    App( settings, servData ),
    _server( eiennet::ip::EndPoint( "", settings["server"]["server_port"] ), settings["server"]["thread_count"], settings["server"]["listen_backlog"] )
{
    v2::outputVerbose = settings["server"]["verbose"];

    _server.onCreateClientHandler( [this] ( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) -> v2::ClientCtx * {
        return new v2::HttpClientCtx( this, clientId, clientEpStr, clientSockPtr );
    } );
}

int HttpApp::run( void * runParam )
{
    this->_runParam = runParam;

    return _server.run();
}

}
