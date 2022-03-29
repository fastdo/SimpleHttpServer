#pragma once

namespace v2
{
/** \brief 服务器类基础
 *
 *  直接使用时，需要给定事件处理；继承时需要override相应的事件虚函数。\n
 *  事件：\n
 *  ClientDataArrived - 客户数据到达 */
class Server
{
    // 客户数据到达
    _DEFINE_EVENT_RELATED(
        ClientDataArrived,
        ( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data ),
        ( clientCtxPtr, std::move(data) )
    )

    // 当创建客户连接对象
    _DEFINE_EVENT_RETURN_RELATED_EX(
        ClientCtx *,
        CreateClient,
        ( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr )
    );

public:
    /** \brief 构造函数0，不会启动服务，必须手动调用startup() */
    Server();

    /** \brief 构造函数1，会启动服务
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 是否显示提示信息 */
    Server( eiennet::ip::EndPoint const & ep, int threadCount = 4, int backlog = 0, double serverWait = 0.02, double verboseInterval = 1.0, bool verbose = true );

    virtual ~Server();

    /** \brief 启动服务器
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog)
     *  \param serverWait 服务器IO等待时间
     *  \param verboseInterval verbose信息刷新间隔
     *  \param verbose 是否显示提示信息 */
    bool startup( eiennet::ip::EndPoint const & ep, int threadCount = 4, int backlog = 0, double serverWait = 0.02, double verboseInterval = 1.0, bool verbose = true );

    /** \brief 运行 */
    virtual int run();

    /** \brief 是否停止服务运行 */
    void stop( bool b = true );

    /** \brief 获取客户连接数 */
    size_t getClientsCount() const;

    /** \brief 移除客户连接 */
    void removeClient( winux::uint64 clientId );

protected:
    winux::SharedPointer<ClientCtx> & _addClient( eiennet::ip::EndPoint const & clientEp, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr );

    winux::ThreadPool _pool; // 线程池
    //winux::Mutex _mtxServer; // 互斥量保护服务器共享数据
    eiennet::ip::tcp::Socket _servSock; // 服务器监听套接字
    std::map< winux::uint64, winux::SharedPointer<ClientCtx> > _clients; // 客户表

    winux::uint64 _cumulativeClientId; // 客户唯一标识
    bool _stop; // 是否停止

    double _serverWait; // 服务器IO等待时间间隔（秒）
    double _verboseInterval; // verbose信息刷新间隔（秒）
    bool _verbose; // 显示提示信息

private:

    DISABLE_OBJECT_COPY(Server)
};

}
