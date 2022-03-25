#pragma once

#include "v2_ClientCtx.hpp"
#include "v2_HttpApp.hpp"

namespace v2
{

class HttpRequest : public eienwebx::Request
{
public:
    HttpRequest( HttpApp * app ) : Request(app)
    {
    }

};

}
