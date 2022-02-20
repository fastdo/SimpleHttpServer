
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"

namespace v2
{

ClientCtx::ClientCtx(Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr) :
    server(server),
    clientId(clientId),
    clientEpStr(clientEpStr),
    clientSockPtr(clientSockPtr),
    canRemove(false)
{

}

ClientCtx::~ClientCtx()
{

}

winux::String ClientCtx::getStamp() const
{
    winux::String stamp;
    winux::OutStringStreamWrapper(&stamp) << "[客户-" << this->clientId << "]<" << this->clientEpStr << ">";
    return stamp;
}

}