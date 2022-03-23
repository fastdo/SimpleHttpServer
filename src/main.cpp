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

    server.onClientRequestHandler( [] ( SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header * header, Buffer * body ) {
        auto str = header->toString();
        if ( v2::outputVerbose )
        {
            ColorOutput( fgYellow, str, body->getSize() /*Base64Encode(body)*/ );
        }

        if ( header->getUrl() == "/favicon.ico" )
        {
            /*clientCtxPtr->clientSockPtr->send(
                "HTTP/1.1 401 Unauthorized\r\n"
                "WWW-Authenticate: Basic realm=\"Access the favicon.ico\"\r\n"
                "Content-Length: 0\r\n"
                //"Connection: keep-alive\r\n"
                "\r\n"
            );*/
            httpClientCtxPtr->clientSockPtr->send(
                "HTTP/1.1 404 Not found\r\n"
                //"Content-Type: text/html\r\n"
                "Content-Length: 0\r\n"
                //"Connection: keep-alive\r\n"
                "\r\n"
            );
        }
        else
        {
            std::cout
                << httpClientCtxPtr->body.getSize() << std::endl
                << httpClientCtxPtr->forClient.data.getCapacity() << std::endl
                << httpClientCtxPtr->forClient.extraData.getCapacity() << std::endl
                ;

            httpClientCtxPtr->clientSockPtr->send(
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
    using namespace winux;
    using namespace std;
    string s = "hi";
    auto f = MakeSimple( NewRunable( [] ( string & str ) { str+= "1";cout << str<<endl; }, ref(s) ) );
    f->invoke();

    cout << s << endl;

    return v2::startup();
}
