#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpClientCtx.hpp"
#include "v2_HttpServer.hpp"

namespace v2
{
using namespace winux;
using namespace eiennet;

int startup()
{
    SocketLib initSock;
    HttpServer server( ip::EndPoint("127.0.0.1:18080") );

    server.onClientDataArrivedHandler( [] ( SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer ) {
        if ( v2::outputVerbose ) ColorOutput( fgAqua, "onClientDataArrived" );
    } );

    server.onClientRequestHeaderHandler( [&server] ( SharedPointer<HttpClientCtx> clientCtxPtr, http::Header const & header ) {
        auto str = header.toString();
        if ( v2::outputVerbose ) ColorOutput( fgYellow, str, str.length() );

        if ( header.getUrl() == "/favicon.ico" )
        {
            clientCtxPtr->clientSockPtr->send(
                "HTTP/1.1 401 Unauthorized\r\n"
                "WWW-Authenticate: Basic realm=\"Access the favicon.ico\"\r\n"
                "Content-Length: 0\r\n"
                //"Connection: keep-alive\r\n"
                "\r\n"
            );
            return;
        }
        else
        {
            clientCtxPtr->clientSockPtr->send(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 13\r\n"
                //"Connection: keep-alive\r\n"
                "\r\n"
                "Hello world!\n"
            );
        }
    } );

    return server.run();
}

}


int main()
{
    return v2::startup();
}
