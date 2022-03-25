#pragma once

namespace v2
{

class HttpApp : public eienwebx::App
{
public:
    using App::App;

    virtual int run( void * runParam ) override;
};

}