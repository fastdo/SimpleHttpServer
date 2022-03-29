#pragma once



namespace v2
{
class HttpApp;

class HttpRequest : public eienwebx::Request
{
public:
    HttpRequest( HttpApp * app );

    bool processData() override;
};

}
