#include "v2_base.hpp"
#include "v2_HttpAppConfig.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequestCtx.hpp"
#include "v2_HttpOutputMgr.hpp"

#include <appserv.hpp>

#if defined(OS_WIN)
    #include <process.h>

    #define getpid _getpid
#else
    #include <sys/types.h>
    #include <sys/ipc.h>
    #include <sys/msg.h>
    #include <unistd.h>
#endif

namespace v2
{
using namespace std;
using namespace winux;
using namespace eiennet;

int startup()
{
    SocketLib initSock;

    AppServerExternalData myAppServerData;
    myAppServerData.serverPath = GetExecutablePath();
    myAppServerData.pid = getpid();
    myAppServerData.fastdoVer = "0.6.1";

    ConfigureSettings cfg;
    String exeFile;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath(), &exeFile );
    cfg["$WorkDirPath"] = ( RealPath("") );
    cfg.load("server.settings");

    // 输出配置信息
    ColorOutputLine( fgYellow, cfg.val().myJson( true, "    ", "\n" ) );

    // 创建App
    HttpApp app{ cfg, &myAppServerData };

    String sessionsSavePath = cfg.get("SessionsPath");
    // 确保路径存在
    winux::MakeDirExists(sessionsSavePath);
    // 文件式session服务,构造参数为一个session文件存储路径
    eienwebx::FileSessionServer sessServ{ sessionsSavePath };
    app.setSessServ(&sessServ);

    /*app.onWebMainHandler( [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ) {
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
    } );//*/

    /*app.route( "GET,POST", "/testdir/index/abc/xyz/123", [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP ) {
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

    auto fn = [] ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, eienwebx::Response & RSP, winux::StringArray const & urlPathPartArr, size_t i ) -> bool {
        cout << Mixed(urlPathPartArr[i]).myJson() << ", " << Mixed( StrJoinEx( "/", urlPathPartArr, i + 1 ) ).myJson() << endl;
        //RSP
        //    << "<div>" << Mixed(urlPathPartArr[i]).myJson()
        //    << ", " << Mixed( StrJoinEx( "/", urlPathPartArr, i + 1 ) ).myJson()
        //    << "</div>\n";
        return true;
    };

    app.crossRoute( "*", "/", fn );
    app.crossRoute( "*", "/testdir", fn );
    app.crossRoute( "*", "/testdir/index", fn );
    app.crossRoute( "*", "/testdir/index/abc", fn );
    app.crossRoute( "*", "/testdir/index/abc/xyz", fn );
    app.crossRoute( "*", "/testdir/index/abc/xyz/123", fn );//*/


    return app.run(nullptr);
}

}

namespace eienwebx
{
using namespace eiennet;
using namespace winux;
using namespace std;

int startup()
{
    SocketLib initSock;

    AppServerExternalData myAppServerData;
    myAppServerData.serverPath = GetExecutablePath();
    myAppServerData.pid = getpid();
    myAppServerData.fastdoVer = "0.6.1";

    ConfigureSettings cfg;
    String exeFile;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath(), &exeFile );
    cfg["$WorkDirPath"] = ( RealPath("") );
    cfg.load("server.settings");

    // 输出配置信息
    ColorOutputLine( fgYellow, cfg.val().myJson( true, "    ", "\n" ) );

    // 创建App
    HttpApp app{ cfg, &myAppServerData };

    String sessionsSavePath = cfg.get("SessionsPath");
    // 确保路径存在
    MakeDirExists(sessionsSavePath);
    // 文件式session服务,构造参数为一个session文件存储路径
    FileSessionServer sessServ{ sessionsSavePath };
    app.setSessServ(&sessServ);

    return app.run(nullptr);
}

}

int main()
{
    //v2::startup();
    eienwebx::startup();
    return 0;
}
