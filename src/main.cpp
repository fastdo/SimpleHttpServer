#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"
#include "v2_HttpOutputMgr.hpp"
#include "v2_HttpClientCtx.hpp"

namespace v2
{
using namespace std;
using namespace winux;
using namespace eiennet;

//int startup_1()
//{
//    SocketLib initSock;
//    HttpServer server( ip::EndPoint(":18080") );
//
//    server.onClientRequestHandler( [] ( SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, AnsiString & body ) {
//        auto str = header.toString();
//        if ( v2::outputVerbose )
//        {
//            ColorOutput( fgYellow, str, body.size() /*Base64Encode(body)*/ );
//        }
//
//        if ( header.getUrl() == "/favicon.ico" )
//        {
//            /*clientCtxPtr->clientSockPtr->send(
//                "HTTP/1.1 401 Unauthorized\r\n"
//                "WWW-Authenticate: Basic realm=\"Access the favicon.ico\"\r\n"
//                "Content-Length: 0\r\n"
//                //"Connection: keep-alive\r\n"
//                "\r\n"
//            );*/
//            httpClientCtxPtr->clientSockPtr->send(
//                "HTTP/1.1 404 Not found\r\n"
//                //"Content-Type: text/html\r\n"
//                "Content-Length: 0\r\n"
//                //"Connection: keep-alive\r\n"
//                "\r\n"
//            );
//        }
//        else
//        {
//            /*std::cout
//                << httpClientCtxPtr->body.capacity() << std::endl
//                << httpClientCtxPtr->forClient.data.getCapacity() << std::endl
//                << httpClientCtxPtr->forClient.extraData.getCapacity() << std::endl
//                ;*/
//
//            httpClientCtxPtr->clientSockPtr->send(
//                "HTTP/1.1 200 OK\r\n"
//                "Content-Type: text/html\r\n"
//                "Content-Length: 13\r\n"
//                //"Connection: keep-alive\r\n"
//                "\r\n"
//                "Hello world!\n"
//            );
//        }
//    } );
//
//    return server.run();
//}

//ColorOutput( fgYellow, cfg.val().myJson( true, "  ", "\n" ) );

int startup()
{
    SocketLib initSock;

    ConfigureSettings cfg;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath() );
    cfg.load("server.settings");
    
    v2::HttpApp app{cfg};

    app.onWebMainHandler( [] ( winux::SharedPointer<v2::HttpClientCtx> httpClientCtxPtr, eienwebx::Response *rsp, void * runParam ) {
        eienwebx::Request & REQ = rsp->request;
        eienwebx::Response & RSP = *rsp;
        eienwebx::App & APP = *REQ.app;

        auto str = REQ.header.toString();
        if ( v2::outputVerbose )
        {
            winux::ColorOutput( winux::fgYellow, str/*, body.size()*/ /*Base64Encode(body)*/ );
        }

        if ( REQ.header.getUrl() == "/favicon.ico" )
        {
            RSP.header.setResponseLine( "HTTP/1.1 404 Not found", false );
        }
        else
        {
            
            RSP << winux::StrMultipleA("Hello my response! 你好，我的响应\n", 1000);
        }
    } );

    return app.run(nullptr);
}

}

int main()
{
    v2::startup();
    return 0;
}
