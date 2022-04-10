
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"

namespace v2
{

Server::Server() :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _serverWait(0.02),
    _verboseInterval(1.0),
    _verbose(true)
{

}

Server::Server( eiennet::ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, bool verbose ) :
    _mtxServer(true),
    _cumulativeClientId(0),
    _stop(false),
    _serverWait(0.02),
    _verboseInterval(1.0),
    _verbose(true)
{
    this->startup( ep, threadCount, backlog, serverWait, verboseInterval, verbose );
}

Server::~Server()
{

}

bool Server::startup( eiennet::ip::EndPoint const & ep, int threadCount, int backlog, double serverWait, double verboseInterval, bool verbose )
{
    _pool.startup(threadCount);
    _servSock.setReUseAddr(true);
    _stop = !( _servSock.eiennet::Socket::bind(ep) && _servSock.listen(backlog) );

    _serverWait = serverWait;
    _verboseInterval = verboseInterval;
    _verbose = verbose;

    if ( this->_verbose )
    {
        if ( _stop )
        {
            winux::ColorOutputLine(
                winux::fgRed,
                "启动服务器失败",
                ", ep=", ep.toString(),
                ", threads=", threadCount,
                ", backlog=", backlog,
                ", serverWait=", serverWait,
                ", verboseInterval=", verboseInterval,
                ", verbose=", verbose,
                ""
            );
        }
        else
        {
            winux::ColorOutputLine(
                winux::fgGreen,
                "启动服务器成功",
                ", ep=", ep.toString(),
                ", threads=", threadCount,
                ", backlog=", backlog,
                ", serverWait=", serverWait,
                ", verboseInterval=", verboseInterval,
                ", verbose=", verbose,
                ""
            );
        }
    }

    return !_stop;
}

int Server::run()
{
    int counter = 0;
    while ( !_stop )
    {
        eiennet::io::Select sel;
        // 监视服务器sock
        sel.setExceptSock(_servSock);
        sel.setReadSock(_servSock);

        if ( true )
        {
            winux::ScopeGuard guard(this->_mtxServer);
            // 输出一些服务器状态信息
            if ( this->_verbose && ++counter % static_cast<int>( this->_verboseInterval / this->_serverWait ) == 0 )
            {
                winux::DateTimeL dtl;
                winux::ColorOutput(
                    winux::fgWhite,
                    dtl.fromCurrent(),
                    ", 总客户数:", this->_clients.size(),
                    ", 当前任务数:",
                    this->_pool.getTaskCount(),
                    std::string(20, ' '),
                    "\r"
                );
            }

            // 监视客户连接，移除标记为可移除的连接
            for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
            {
                if ( it->second->canRemove )
                {
                    it = this->_clients.erase(it);
                }
                else
                {
                    sel.setReadSock(*it->second->clientSockPtr.get());
                    sel.setExceptSock(*it->second->clientSockPtr.get());
                    ++it;
                }
            }
        }

        int rc = sel.wait(this->_serverWait); // 返回就绪的套接字数
        if ( rc > 0 )
        {
            if ( this->_verbose ) winux::ColorOutputLine(winux::fgSilver, "Select模型获取到就绪的socks数:", rc);

            // 处理服务器sock事件
            if ( sel.hasReadSock(_servSock) )
            {
                // 有一个客户连接到来
                eiennet::ip::EndPoint clientEp;
                auto clientSockPtr = _servSock.accept(&clientEp);

                if ( clientSockPtr )
                {
                    auto & clientCtxPtr = this->_addClient( clientEp, clientSockPtr );

                    if ( this->_verbose ) winux::ColorOutputLine(winux::fgFuchsia, clientCtxPtr->getStamp(), "新加入服务器");
                }

                rc--;
            }
            else if ( sel.hasExceptSock(_servSock) )
            {
                winux::ScopeGuard guard(this->_mtxServer);
                _stop = true;

                rc--;
            }

            // 分发客户连接的相关IO事件
            if ( rc > 0 )
            {
                winux::ScopeGuard guard(this->_mtxServer);
                //this->_mtxServer.lock();

                for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
                {
                    //this->_mtxServer.unlock();

                    if ( sel.hasReadSock(*it->second->clientSockPtr.get()) ) // 该套接字有数据可读
                    {
                        int arrivedSize = it->second->clientSockPtr->getAvailable();

                        if ( arrivedSize > 0 )
                        {
                            if ( this->_verbose ) winux::ColorOutputLine(winux::fgWhite, it->second->getStamp(), "有数据到达(bytes:", arrivedSize, ")");
                            // 收数据
                            winux::Buffer data = it->second->clientSockPtr->recv(arrivedSize);
                            if ( this->_verbose ) winux::ColorOutputLine(winux::fgGreen, it->second->getStamp(), "收到数据:", data.getSize());

                            // 不能用线程池去处理，要直接调用。因为要保证数据接收先后顺序
                            //this->_pool.task( &Server::onClientDataArrived, this, it->second, std::move(data) ).post();
                            this->onClientDataArrived( it->second, std::move(data) );
                        }
                        else // arrivedSize <= 0
                        {
                            if ( this->_verbose ) winux::ColorOutputLine(winux::fgRed, it->second->getStamp(), "有数据到达(bytes:", arrivedSize, ")，对方了可能关闭了连接");
                            if ( this->_verbose ) winux::ColorOutputLine(winux::fgMaroon, it->second->getStamp(), "移除");

                            //this->_mtxServer.lock();
                            it = this->_clients.erase(it);
                            //this->_mtxServer.unlock();
                        }

                        rc--;
                    }
                    else if ( sel.hasExceptSock(*it->second->clientSockPtr.get()) ) // 该套接字有错误
                    {
                        if ( this->_verbose ) winux::ColorOutputLine(winux::fgMaroon, it->second->getStamp(), "出错并移除");

                        //this->_mtxServer.lock();
                        it = this->_clients.erase(it);
                        //this->_mtxServer.unlock();

                        rc--;
                    }

                    //this->_mtxServer.lock();

                    // 已经没有就绪的套接字，跳出
                    if ( rc == 0 ) break;

                    // 如果已经是end则不能再++it
                    if ( it != this->_clients.end() ) ++it;
                }

                //this->_mtxServer.unlock();

            }
        }
        else
        {
            //cout << "select() = " << rc << "\n";
        }
    }

    _pool.whenEmptyStopAndWait();
    return 0;
}

void Server::stop( bool b )
{
    winux::ScopeGuard guard(_mtxServer);
    static_cast<volatile bool &>(_stop) = b;
}

size_t Server::getClientsCount() const
{
    winux::ScopeGuard guard( const_cast<winux::Mutex &>(_mtxServer) );
    return _clients.size();
}

void Server::removeClient( winux::uint64 clientId )
{
    winux::ScopeGuard guard(_mtxServer);
    _clients.erase(clientId);
}

winux::SharedPointer<ClientCtx> & Server::_addClient( eiennet::ip::EndPoint const & clientEp, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr )
{
    winux::SharedPointer<ClientCtx> * client;
    {
        winux::ScopeGuard guard(_mtxServer);
        ++_cumulativeClientId;
        client = &_clients[_cumulativeClientId];
    }
    client->attachNew( this->onCreateClient( _cumulativeClientId, clientEp.toString(), clientSockPtr ) );
    return *client;
}

ClientCtx * Server::onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr )
{
    if ( this->_CreateClientHandler )
    {
        return this->_CreateClientHandler( clientId, clientEpStr, clientSockPtr );
    }
    else
    {
        return new ClientCtx( clientId, clientEpStr, clientSockPtr );
    }
}

}
