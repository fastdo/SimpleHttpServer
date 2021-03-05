
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
    ThreadPool pool;
    Mutex mtxServer;
    uint64 cumulativeClientId = 0; // 客户唯一标识
    ip::tcp::Socket servSock; // 服务器监听套接字
    map< uint64, SharedPointer<ClientCtx> > clients;
    bool stop = false;

    ServerCtx( int poolThreadCount ) : pool( poolThreadCount, ThreadPool::modeWaitTimeRePost ), mtxServer(true)
    {
        servSock.bind( "", 8080 );
        servSock.listen(10);
    }

    uint64 acceptToClients()
    {
        ip::EndPoint epClient;
        auto clientSock = servSock.accept(&epClient);
        colorPrompt.modify();
        cout << "Client `" << epClient.toString() << "` 到来" << endl;
        colorPrompt.resume();
        {
            ScopeGuard guard(mtxServer);
            ++cumulativeClientId;
            clients[cumulativeClientId].attachNew( new ClientCtx( cumulativeClientId, epClient, clientSock ) );
            return cumulativeClientId;
        }
    }

    void removeClient( uint64 clientId )
    {
        ScopeGuard guard(mtxServer);
        colorError.modify();
        cout << "Client `" << clients[clientId]->clientEp.toString() << "` 断开" << endl;
        colorError.resume();
        clients.erase(clientId);
    }

    SharedPointer<ClientCtx> at( uint64 clientId )
    {
        ScopeGuard guard(mtxServer);
        return clients[clientId];
    }
};

// 读取一个请求头
bool ReadHeader( ClientCtx * client, SocketStreamBuf * sockBuf, string * headerStr )
{
    istream sockIn(sockBuf);
    string nlnl = "\r\n\r\n";
    bool complete = true;
    constexpr int RETRY_COUNT = 30;
    int retryCount = RETRY_COUNT;
    // 如果接收的数据长度不到定界符长度或者还未收到定界符，则不停接收
    while ( headerStr->length() < nlnl.length() || headerStr->substr( headerStr->length() - nlnl.length() ) != nlnl )
    {
        // 缓冲区有数据可直接读或者监听到有数据到来
        if ( sockBuf->in_avail() > 0 || io::SelectRead( sockBuf->getSocket() ).wait(1) > 0 )
        {
            retryCount = RETRY_COUNT;

            int ch;
            if ( ( ch = sockIn.get() ) == -1 )
            {
                colorError.modify();
                cout << "Client `" << client->clientEp.toString() << "` 接收数据出错" << endl;
                colorError.resume();

                complete = false;
                break;
            }
            *headerStr += (char)ch;
        }
        else
        {
            colorWarnning.modify();
            cout << "Client `" << client->clientEp.toString() << "` 没有数据到来" << retryCount << "s\n";
            colorWarnning.resume();

            if ( --retryCount == 0 )
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
    if ( ReadHeader( client.get(), &ssb, &headerStr ) )
    {
        colorOk.modify();
        cout << "Client `" << client->clientEp.toString() << "` 接收头部(bytes:" << headerStr.size() << ")" << endl;
        colorOk.resume();

        ostream clientOut(&ssb);

        http::Header reqHdr;
        reqHdr.parse(headerStr);

        cout << reqHdr.toString();

        // url router
        if ( reqHdr.getUrl() == "/" )
        {
            colorAction.modify();
            cout << "request_url: " << reqHdr.getUrl() << endl;
            colorAction.resume();

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
            colorAction.modify();
            cout << "request_url: " << reqHdr.getUrl() << endl;
            colorAction.resume();

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
        colorError.modify();
        cout << "Client `" << client->clientEp.toString() << "` 接收头部不完整或不正确(bytes:" << headerStr.size() << ")" << endl;
        colorError.resume();
        server->removeClient(client->clientId);
    }
}

int main()
{
    SocketLib init;
    ServerCtx serv(10);

    while ( !serv.stop )
    {
        // 投递处理请求的任务
        serv.pool.task( RequestTaskRoutine, &serv, serv.at( serv.acceptToClients() ) );
    }

    return 0;
}
