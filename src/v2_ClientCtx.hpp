#pragma once

namespace v2
{

class Server;

/** \brief 客户场景类基础 */
class ClientCtx
{
public:
    ClientCtx( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr );

    virtual ~ClientCtx();

    winux::String getStamp() const;

    winux::uint64 clientId; ///< 客户Id
    winux::String clientEpStr; ///< 客户终端字符串
    winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr; ///< 客户套接字
    bool canRemove; ///< 是否标记为可以移除

private:
    DISABLE_OBJECT_COPY(ClientCtx)
};

}
