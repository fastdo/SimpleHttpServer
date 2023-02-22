#include <winux.hpp>
#include <eiennet.hpp>
#include <eienwebx.hpp>
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
    myAppServerData.fastdoVer = "0.6.4";

    ConfigureSettings cfg;
    String exeFile;
    cfg["$ExeDirPath"] = FilePath( GetExecutablePath(), &exeFile );
    cfg["$WorkDirPath"] = RealPath("");
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
    eienwebx::startup();
    return 0;
}
