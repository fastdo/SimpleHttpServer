﻿#pragma once

namespace v2
{

class Server;

/** \brief 客户场景类基础 */
class ClientCtx
{
public:
    DEFINE_FUNC_NEWINSTANCE(
        ClientCtx,
        ClientCtx,
        ( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr ),
        ( clientId, clientEpStr, clientSockPtr )
    )

    ClientCtx( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr );

    virtual ~ClientCtx();

    winux::String getStamp() const;

    winux::uint64 clientId;
    winux::String clientEpStr;
    winux::SharedPointer<ip::tcp::Socket> clientSockPtr;
    bool canRemove;

private:
    DISABLE_OBJECT_COPY(ClientCtx)
};

}