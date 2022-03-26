#pragma once

namespace v2
{

class HttpRequest : public eienwebx::Request
{
public:
    HttpRequest( v2::HttpApp * app ) : Request(app)
    {
    }

    bool processData() override;
};

}
