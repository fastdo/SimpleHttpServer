#include "v2_base.hpp"
#include "v2_HttpOutputMgr.hpp"

#include <strstream>

namespace eienwebx
{
#include "webx_OutputMgr_Data.inl"
}

namespace v2
{

HttpOutputMgr::HttpOutputMgr( eiennet::Socket * clientSock ) : OutputMgr(), _clientSock(clientSock)
{

}

void HttpOutputMgr::commit()
{
    if ( !_self->_headerCommited )
    {
        if ( _self->_header.getVersion().empty() )
            _self->_header.setVersion("HTTP/1.1");
        if ( _self->_header.getStatusCode().empty() )
            _self->_header.setStatusCode("200");
        if ( _self->_header.getStatusStr().empty() )
            _self->_header.setStatusStr("OK");

        if ( !_self->_header.hasHeader("Content-Length") )
        {
            _self->_header("Content-Length") << this->getConvertedOutputSize();
        }
    }

    OutputMgr::commit();
}

void HttpOutputMgr::_actualOutput( void const * data, size_t size )
{
    _clientSock->send( data, (int)size );
}

}
