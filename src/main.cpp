
#include <winux.hpp>
#include <eiennet.hpp>
#include <eientpl.hpp>

using namespace winux;
using namespace eiennet;
using namespace eientpl;
using namespace std;

namespace
{
    ConsoleAttr colorPrompt(fgYellow);
    ConsoleAttr colorAction(fgAqua);
    ConsoleAttr colorOk(fgGreen);
    ConsoleAttr colorWarnning(fgFuchsia);
    ConsoleAttr colorError(fgRed);
    bool __outputVerbose = true;
}

void ColorOutput( ConsoleAttr & ca )
{
    cout << endl;
}

template < typename _Ty, typename... _ArgType >
void ColorOutput( ConsoleAttr & ca, _Ty&& a, _ArgType&& ... arg )
{
    if ( __outputVerbose )
    {
        ca.modify();
        cout << a;
        ca.resume();
        ColorOutput( ca, forward<_ArgType>(arg)... );
    }
}

// 客户连接场景
struct ClientCtx
{
    uint64 clientId;
    ip::EndPoint clientEp;
    SharedPointer<ip::tcp::Socket> clientSockPtr;
    ClientCtx() : clientId(0) { }
    ClientCtx( uint64 clientId, ip::EndPoint ep, SharedPointer<ip::tcp::Socket> newSockPtr ) : clientId(clientId), clientEp(ep), clientSockPtr(newSockPtr)
    {
    }
};

// 服务器场景
struct ServerCtx
{
    // 服务器配置
    struct ServerConfig
    {
        String serverIp;
        ushort serverPort;
        String documentRoot;
        String documentIndex;

        int listenBacklog;
        int threadCount;
        int aliveTime;

        // 加载配置参数
        ServerConfig( Configure const & confObj )
        {
            serverIp = confObj.has("server_ip") ? confObj("server_ip") : "";
            serverPort = confObj.has("server_port") ? Mixed( confObj("server_port") ).toUShort() : 8080U;
            documentRoot = confObj.has("document_root") ? confObj("document_root") : "";
            documentIndex = confObj.has("document_index") ? confObj("document_index") : "index.html";
            __outputVerbose = true;
            if ( confObj.has("verbose") ) Mixed::ParseBool( confObj("verbose"), &__outputVerbose );
            listenBacklog = confObj.has("listen_backlog") ? Mixed( confObj("listen_backlog") ).toInt() : 10;
            threadCount = confObj.has("thread_count") ? Mixed( confObj("thread_count") ).toInt() : 8;
            aliveTime = confObj.has("alive_time") ? Mixed( confObj("alive_time") ).toInt() : 30;

            if ( __outputVerbose )
            {
                ColorOutput( colorPrompt, "server_ip: ", serverIp );
                ColorOutput( colorPrompt, "server_port: ", serverPort );
                ColorOutput( colorPrompt, "document_root: ", documentRoot );
                ColorOutput( colorPrompt, "document_index: ", documentIndex );
                ColorOutput( colorPrompt, "listen_backlog: ", listenBacklog );
                ColorOutput( colorPrompt, "thread_count: ", threadCount );
                ColorOutput( colorPrompt, "alive_time: ", aliveTime );
            }
        }
    } const config; // 配置参数

    ThreadPool pool; // 线程池
    Mutex mtxServer; // 互斥量保护服务共享数据
    uint64 cumulativeClientId; // 客户唯一标识
    bool stop; // 是否停止
    ip::tcp::Socket servSock; // 服务器监听套接字
    map< uint64, SharedPointer<ClientCtx> > clients;

    ServerCtx( Configure const & confObj ) :
        config(confObj),
        pool( config.threadCount, ThreadPool::modeWaitTimeRePost ),
        mtxServer(true),
        cumulativeClientId(0),
        stop(false)
    {
        servSock.bind( config.serverIp, config.serverPort );
        servSock.listen(config.listenBacklog);
    }

    // 接受一个连接并放入服务器客户表中
    uint64 acceptToClients()
    {
        ip::EndPoint epClient;
        auto clientSock = servSock.accept(&epClient);
        ColorOutput( colorPrompt, "Client `", epClient.toString(), "` 到来" );

        {
            ScopeGuard guard(mtxServer);
            ++cumulativeClientId;
            clients[cumulativeClientId].attachNew( new ClientCtx( cumulativeClientId, epClient, clientSock ) );
            return cumulativeClientId;
        }
    }

    // 移除一个客户
    void removeClient( uint64 clientId )
    {
        ScopeGuard guard(mtxServer);
        ColorOutput( colorError, "Client `", clients[clientId]->clientEp.toString(), "` 断开" );
        clients.erase(clientId);
    }

    // 根据客户ID取得客户场景
    SharedPointer<ClientCtx> at( uint64 clientId )
    {
        ScopeGuard guard(mtxServer);
        return clients[clientId];
    }
};

// 读取一个请求头
bool ReadHeader( ServerCtx * server, ClientCtx * client, SocketStreamBuf * sockBuf, string * headerStr )
{
    istream clientIn(sockBuf);
    string nlnl = "\r\n\r\n";
    bool complete = true;
    int retryCount = server->config.aliveTime;
    // 如果接收的数据长度不到定界符长度或者还未收到定界符，则不停接收
    while ( headerStr->length() < nlnl.length() || headerStr->substr( headerStr->length() - nlnl.length() ) != nlnl )
    {
        // 缓冲区有数据可直接读或者监听到有数据到来
        if ( sockBuf->in_avail() > 0 || io::SelectRead( sockBuf->getSocket() ).wait(1) > 0 )
        {
            retryCount = server->config.aliveTime;

            int ch;
            if ( ( ch = clientIn.get() ) == -1 )
            {
                ColorOutput( colorError, "Client `", client->clientEp.toString(), "` 接收数据出错" );

                complete = false;
                break;
            }
            *headerStr += (char)ch;
        }
        else
        {
            ColorOutput( colorWarnning, "Client `", client->clientEp.toString(), "` 没有数据到来", retryCount, "s" );

            if ( --retryCount < 1 )
            {
                complete = false;
                break;
            }
        }
    }
    return complete;
}

// MIME
map< String, String > __mime{
    { "html", "text/html" },
    { "css", "text/css" },
    { "js", "text/javascript" },
    { "jpg", "image/jpeg" },
    { "png", "image/png" },
    { "gif", "image/gif" },
    { "ico", "image/x-icon" }
};

String GetMime( String const & extName )
{
    return isset(__mime, extName ) ? __mime[extName] : "application/octet-stream";
}

// url router
void UrlRouter( ServerCtx * server, ClientCtx * client, SocketStreamBuf * sockBuf, http::Header & reqHdr )
{
    ostream clientOut(sockBuf);

    http::Url url( reqHdr.getUrl() );
    ColorOutput( colorAction, "request_url: ", url.toString() );

    String urlPath = url.getPath();
    urlPath = !urlPath.empty() ? urlPath : server->config.documentIndex;
    String filePath = CombinePath( server->config.documentRoot, urlPath );

    ColorOutput( colorAction, "file_path: ", filePath );
    // 文件是否存在
    if ( DetectPath( filePath ) )
    {
        String extName;
        FileTitle( filePath, &extName );

        File docfile( filePath, "rb" );
        // 响应
        AnsiString rspBody = docfile.buffer();
        http::Header rspHdr;
        rspHdr.setResponseLine( "HTTP/1.1 200 OK", false );
        rspHdr["Content-Type"] = GetMime(extName);
        rspHdr( "Content-Length" ) << rspBody.length();

        // 输出响应
        clientOut << rspHdr.toString() << rspBody;
    }
    else
    {
        // 响应
        string rspBody = "HTTP 404 not found!";
        http::Header rspHdr;
        rspHdr.setResponseLine( "HTTP/1.1 404 Not Found", false );
        rspHdr["Content-Type"] = "text/html";
        rspHdr( "Content-Length" ) << rspBody.length();

        // 输出响应
        clientOut << rspHdr.toString() << rspBody;
    }
}

// 一个请求任务例程
void RequestTaskRoutine( ServerCtx * server, SharedPointer<ClientCtx> client )
{
    SocketStreamBuf sockBuf( client->clientSockPtr.get() );

    // 读取头部完整
    string headerStr;
    if ( ReadHeader( server, client.get(), &sockBuf, &headerStr ) )
    {
        ColorOutput( colorOk, "Client `", client->clientEp.toString(), "` 接收头部(bytes:", headerStr.size(), ")" );

        http::Header reqHdr;
        reqHdr.parse(headerStr);
        if ( __outputVerbose )
            cout << reqHdr.toString();

        UrlRouter( server, client.get(), &sockBuf, reqHdr );

        // 如果不保活就删除连接，否则继续投请求任务
        if ( StrLower( reqHdr["Connection"] ) != "keep-alive" )
        {
            server->removeClient(client->clientId);
        }
        else
        {
            server->pool.task( RequestTaskRoutine, server, client );
        }
    }
    else // 头部不完整，移除连接
    {
        ColorOutput( colorError, "Client `", client->clientEp.toString(), "` 接收头部不完整或不正确(bytes:", headerStr.size(), ")" );
        server->removeClient(client->clientId);
    }
}

int main()
{
    SocketLib init;
    ServerCtx serv( Configure("server.conf") );

    while ( !serv.stop )
    {
        // 投递处理请求的任务
        serv.pool.task( RequestTaskRoutine, &serv, serv.at( serv.acceptToClients() ) );
    }

    return 0;
}
