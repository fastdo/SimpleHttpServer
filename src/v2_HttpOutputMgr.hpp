#pragma once

namespace v2
{

class HttpOutputMgr : public eienwebx::OutputMgr
{
public:
    HttpOutputMgr( eiennet::Socket * clientSock );

    void commit() override;
protected:
    void _actualOutput( void const * data, size_t size ) override;
private:
    eiennet::Socket * _clientSock;
};

}
