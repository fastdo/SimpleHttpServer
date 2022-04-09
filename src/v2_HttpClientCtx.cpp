
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"
#include "v2_HttpClientCtx.hpp"

namespace v2
{

HttpClientCtx::HttpClientCtx( HttpApp * app, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) :
    ClientCtx(clientId, clientEpStr, clientSockPtr),
    url(http::Url::urlSimple),
    request( app, this ),
    curRecvType(drtRequestHeader),
    hasHeader(false),
    requestContentLength(0L)
{

}

HttpClientCtx::~HttpClientCtx()
{
    if ( static_cast<HttpApp*>(request.app)->_server.config.verbose )
    {
        winux::ColorOutputLine( winux::fgBlue, this->getStamp(), "析构" );
    }
}

}