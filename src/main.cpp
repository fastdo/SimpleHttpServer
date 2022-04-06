#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"
#include "v2_HttpOutputMgr.hpp"
#include "v2_HttpClientCtx.hpp"
#include "fcgiserv.hpp"

namespace v2
{
using namespace std;
using namespace winux;
using namespace eiennet;

int startup()
{
    SocketLib initSock;

    xAppServerData myAppServerData;
    myAppServerData.fcgiservPath = GetExecutablePath();
    myAppServerData.pid = GetCurrentProcessId();

    ConfigureSettings cfg;
    String exeFile;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath(), &exeFile );
    cfg["$WorkDirPath"] = ( RealPath("") );
    cfg.load("server.settings");

    HttpServerConfig hcp{cfg};

    ColorOutputLine( fgYellow, cfg.val().myJson( true, "  ", "\n" ) );



    HttpApp app{ cfg, &myAppServerData };

    app.onWebMainHandler( [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ) {
        eienwebx::Request & REQ = RSP.request;
        eienwebx::App & APP = *REQ.app;

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

    app.setRouteHandler( "*", "/testdir/index/abc/xyz/123", [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ) {
        eienwebx::Request & REQ = RSP.request;
        eienwebx::App & APP = *REQ.app;

        RSP << "<h1>handle route</h1>\n";
        RSP << "GET: " << REQ.get.getVars().myJson(true,"  ","\n") << endl;
        RSP << "POST: " << REQ.post.getVars().myJson(true,"  ","\n") << endl;
        RSP << "COOKIES: " << REQ.cookies.dump().myJson(true,"  ","\n") << endl;
        RSP << "<hr/>\n";
        RSP << REQ.dumpEnv() << endl;
        RSP << "<hr/>\n";
        RSP << APP.dumpEnv() << endl;
    } );

    /*auto fn = [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP, winux::StringArray const & urlPathPartArr, size_t i ) -> bool {
        cout << Mixed(urlPathPartArr[i]).myJson() << endl;
        RSP
            << "<div>" << Mixed(urlPathPartArr[i]).myJson()
            << ", " << Mixed( StrJoinEx( "/", urlPathPartArr, i + 1 ) ).myJson()
            << "</div>\n";
        return true;
    };

    app.setCrossRouteHandler( "*", "/", fn );
    app.setCrossRouteHandler( "*", "/testdir", fn );
    //app.setCrossRouteHandler( "*", "/testdir/index", fn );
    //app.setCrossRouteHandler( "*", "/testdir/index/abc", fn );
    //app.setCrossRouteHandler( "*", "/testdir/index/abc/xyz", fn );
    app.setCrossRouteHandler( "*", "/testdir/index/abc/xyz/123", fn );//*/


    return app.run(nullptr);
}

}

int main()
{
    v2::startup();
    return 0;
}
