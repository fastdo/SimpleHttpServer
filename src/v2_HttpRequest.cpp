#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"

namespace v2
{

HttpRequest::HttpRequest( HttpApp * app ) : Request(app)
{

}

bool HttpRequest::processData()
{
    return true;
}

}