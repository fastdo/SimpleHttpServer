#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
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
//            ColorOutputLine( fgYellow, str, body.size() /*Base64Encode(body)*/ );
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


int startup()
{
    SocketLib initSock;

    ConfigureSettings cfg;
    String exeFile;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath(), &exeFile );
    cfg["$WorkDirPath"] = ( RealPath("") );
    cfg.load("server.settings");

    HttpServerConfig hcp{cfg};
    //cout << hcp.documentIndex << endl;

    ColorOutputLine( fgYellow, cfg.val().myJson( true, "  ", "\n" ) );

    HttpApp app{cfg};

    app.onWebPageHandler( [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::App & APP, eienwebx::Request & REQ, eienwebx::Response & RSP ) {
        if ( REQ.header.getMethod() == "HEAD" )
        {
            RSP.header.setResponseLine( "HTTP/1.1 404 Not found" );
        }
        else
        {
            if ( REQ.header.getUrl() == "/favicon.ico" )
            {
                RSP.header.setResponseLine( "HTTP/1.1 404 Not found" );
            }
            else
            {
                RSP.setCharset("utf-8");
                //RSP << winux::StrMultipleA("Hello my response! 你好，我的响应\n", 1000);
                RSP << "Hello my response! 你好，我的响应<br/>\n";
                //RSP << "URL: " << httpClientCtxPtr->url.dump().myJson(true,"  ","\n") << endl;
                RSP << "GET: " << REQ.get.getVars().myJson(true,"  ","\n") << endl;
                RSP << "POST: " << REQ.post.getVars().myJson(true,"  ","\n") << endl;
                RSP << "COOKIES: " << REQ.cookies.dump().myJson(true,"  ","\n") << endl;
                RSP << "<hr/>\n";
                RSP << REQ.dumpEnv() << endl;
                RSP << "<hr/>\n";
                RSP << APP.dumpEnv() << endl;
            }
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
