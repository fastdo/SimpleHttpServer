
#include <winux.hpp>
#include <eiennet.hpp>

using namespace winux;
using namespace eiennet;
using namespace std;

namespace
{
    ConsoleAttrT<int> colorPrompt( fgYellow, 0 );
    ConsoleAttrT<int> colorAction( fgAqua, 0 );
    ConsoleAttrT<int> colorOk( fgGreen, 0 );
    ConsoleAttrT<int> colorWarnning( fgFuchsia, 0 );
    ConsoleAttrT<int> colorError( fgRed, 0 );
    bool __outputVerbose = true;
}

void ColorOutput( ConsoleAttrT<int> & ca )
{
    cout << endl;
}

template < typename _Ty, typename... _ArgType >
void ColorOutput( ConsoleAttrT<int> & ca, _Ty&& a, _ArgType&& ... arg )
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
    ClientCtx( uint64 clientId, ip::EndPoint ep, SharedPointer<ip::tcp::Socket> newSockPtr ) :
        clientId(clientId),
        clientEp(ep),
        clientSockPtr(newSockPtr)
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

        int listenBacklog;
        int threadCount;
        int aliveTime;

        // 加载配置参数
        ServerConfig( Configure const & confObj )
        {
            serverIp = confObj.has("server_ip") ? confObj("server_ip") : "";
            serverPort = confObj.has("server_port") ? Mixed( confObj("server_port") ).toUShort() : 8080U;
            documentRoot = confObj.has("document_root") ? confObj("document_root") : "";
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
                ColorOutput( colorPrompt, "listen_backlog: ", listenBacklog );
                ColorOutput( colorPrompt, "thread_count: ", threadCount );
                ColorOutput( colorPrompt, "alive_time: ", aliveTime );
            }
        }
    } const config;

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
    istream sockIn(sockBuf);
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
            if ( ( ch = sockIn.get() ) == -1 )
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

// 一个请求任务例程
void RequestTaskRoutine( ServerCtx * server, SharedPointer<ClientCtx> client )
{
    SocketStreamBuf ssb( client->clientSockPtr.get() );

    // 读取头部完整
    string headerStr;
    if ( ReadHeader( server, client.get(), &ssb, &headerStr ) )
    {
        ColorOutput( colorOk, "Client `", client->clientEp.toString(), "` 接收头部(bytes:", headerStr.size(), ")" );

        ostream clientOut(&ssb);

        http::Header reqHdr;
        reqHdr.parse(headerStr);

        cout << reqHdr.toString();

        // url router
        if ( reqHdr.getUrl() == "/" )
        {
            ColorOutput( colorAction, "request_url: ", reqHdr.getUrl() );

            // 响应
            string rspBody = "Hello, My HTTP server!";
            http::Header rspHdr;
            rspHdr.setResponseLine( "HTTP/1.1 200 OK", false );
            rspHdr["Content-Type"] = "text/html";
            rspHdr("Content-Length") << rspBody.length();

            // 输出响应
            clientOut << rspHdr.toString() << rspBody;
        }
        else
        {
            ColorOutput( colorAction, "request_url: ", reqHdr.getUrl() );

            // 响应
            string rspBody = "HTTP 404 not found!";
            http::Header rspHdr;
            rspHdr.setResponseLine( "HTTP/1.1 404 Not Found", false );
            rspHdr["Content-Type"] = "text/html";
            rspHdr("Content-Length") << rspBody.length();

            // 输出响应
            clientOut << rspHdr.toString() << rspBody;
        }

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
