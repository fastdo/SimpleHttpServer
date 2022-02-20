#pragma once

namespace v2
{
/** \brief 服务器类基础
 *
 *  直接使用时，需要给定事件处理；继承时需要override相应的事件虚函数。\n
 *  事件：
 *  ClientDataArrived - 客户数据到达 */
class Server
{
    // 客户数据到达
    _DEFINE_EVENT_RELATED(
        ClientDataArrived,
        ( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data ),
        ( clientCtxPtr, std::move(data) )
    )

public:
    using ClientCtxConstructor = ClientCtx * (*)( Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr );

    /** \brief 构造函数1
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog) */
    Server( ip::EndPoint const & ep, int threadCount = 4, int backlog = 0, ClientCtxConstructor clientConstructor = ClientCtx::NewInstance );

    virtual ~Server();

    virtual int run();

    /** \brief 是否停止服务运行 */
    void stop( bool b = true );

    size_t getClientsCount() const;

    void removeClient( winux::uint64 clientId );

protected:
    winux::SharedPointer<ClientCtx> & _addClient( ip::EndPoint const & clientEp, winux::SharedPointer<ip::tcp::Socket> clientSockPtr );

    winux::uint64 _cumulativeClientId; // 客户唯一标识
    bool _stop; // 是否停止
    winux::ThreadPool _pool; // 线程池
    winux::Mutex _mtxServer; // 互斥量保护服务器共享数据
    ip::tcp::Socket _servSock; // 服务器监听套接字
    std::map< winux::uint64, winux::SharedPointer<ClientCtx> > _clients; // 客户表
    ClientCtxConstructor _clientConstructor; // 客户场景构造器

private:
    DISABLE_OBJECT_COPY(Server)
};

}
