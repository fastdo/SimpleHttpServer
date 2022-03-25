
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"

namespace v2
{

Server::Server( ip::EndPoint const & ep, int threadCount, int backlog, ClientCtxConstructor clientConstructor ) :
    _cumulativeClientId(0),
    _stop(false),
    _pool(threadCount),
    //_mtxServer(true),
    _clientConstructor(clientConstructor)
{
    _servSock.setReUseAddr(true);
    _stop = !( _servSock.eiennet::Socket::bind(ep) && _servSock.listen(backlog) );
}

Server::~Server()
{

}

int Server::run()
{
    int counter = 0;
    while ( !_stop )
    {
        io::Select sel;
        // 监视服务器sock
        sel.setExceptSock(_servSock);
        sel.setReadSock(_servSock);

        // 监视客户连接，移除标记为可移除的连接
        if ( true )
        {
            //winux::ScopeGuard guard(this->_mtxServer);

            if ( ++counter % 50 == 0 && outputVerbose ) winux::ColorOutput(winux::fgWhite, "总客户:", this->_clients.size(), ", 当前任务数:", this->_pool.getTaskCount());

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

        int rc = sel.wait(0.02); // 返回就绪的套接字数
        if ( rc > 0 )
        {
            if ( outputVerbose ) winux::ColorOutput(winux::fgSilver, "Select模型获取到就绪的socks数:", rc);

            // 处理服务器sock事件
            if ( sel.hasReadSock(_servSock) )
            {
                ip::EndPoint clientEp;
                auto clientSockPtr = _servSock.accept(&clientEp);

                if ( clientSockPtr )
                {
                    auto & clientCtxPtr = this->_addClient(clientEp, clientSockPtr);

                    if ( outputVerbose ) winux::ColorOutput(winux::fgFuchsia, clientCtxPtr->getStamp(), "新加入服务器");
                }

                rc--;
            }
            else if ( sel.hasExceptSock(_servSock) )
            {
                _stop = true;

                rc--;
            }

            // 分发客户连接的相关IO事件
            if ( rc > 0 )
            {
                //ScopeGuard guard(this->_mtxServer);
                //this->_mtxServer.lock();

                for ( auto it = this->_clients.begin(); it != this->_clients.end(); )
                {
                    //this->_mtxServer.unlock();

                    if ( sel.hasReadSock(*it->second->clientSockPtr.get()) ) // 该套接字有数据可读
                    {
                        int arrivedSize = it->second->clientSockPtr->getAvailable();

                        if ( arrivedSize > 0 )
                        {
                            if ( outputVerbose ) winux::ColorOutput(winux::fgWhite, it->second->getStamp(), "有数据到达(bytes:", arrivedSize, ")");
                            // 收数据
                            winux::Buffer data = it->second->clientSockPtr->recv(arrivedSize);
                            if ( outputVerbose ) winux::ColorOutput(winux::fgGreen, it->second->getStamp(), "收到数据:", data.getSize());

                            // 不能用线程池去处理，要直接调用。因为要保证数据接收先后顺序
                            //this->_pool.task( &Server::onClientDataArrived, this, it->second, std::move(data) ).post();
                            this->onClientDataArrived( it->second, std::move(data) );

                        }
                        else // arrivedSize <= 0
                        {
                            if ( outputVerbose ) winux::ColorOutput(winux::fgRed, it->second->getStamp(), "有数据到达(bytes:", arrivedSize, ")，对方了可能关闭了连接");
                            if ( outputVerbose ) winux::ColorOutput(winux::fgMaroon, it->second->getStamp(), "关闭并移除");

                            //this->_mtxServer.lock();
                            it = this->_clients.erase(it);
                            //this->_mtxServer.unlock();
                        }

                        rc--;
                    }
                    else if ( sel.hasExceptSock(*it->second->clientSockPtr.get()) ) // 该套接字有错误
                    {
                        if ( outputVerbose ) winux::ColorOutput(winux::fgMaroon, it->second->getStamp(), "出错并移除");

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
    static_cast<volatile bool &>( _stop ) = b;
}

size_t Server::getClientsCount() const
{
    //winux::ScopeGuard guard(const_cast<winux::Mutex &>( _mtxServer ));
    return _clients.size();
}

void Server::removeClient( winux::uint64 clientId )
{
    //winux::ScopeGuard guard(_mtxServer);
    _clients.erase(clientId);
}

winux::SharedPointer<v2::ClientCtx> & Server::_addClient( ip::EndPoint const & clientEp, winux::SharedPointer<ip::tcp::Socket> clientSockPtr )
{
    winux::SharedPointer<ClientCtx> * client;
    {
        //winux::ScopeGuard guard(_mtxServer);
        ++_cumulativeClientId;
        client = &_clients[_cumulativeClientId];
    }
    client->attachNew( (*_clientConstructor)( _cumulativeClientId, clientEp.toString(), clientSockPtr ) );
    return *client;
}

}
