#pragma once

namespace v2
{
/** HTTP客户场景 */
class HttpClientCtx : public ClientCtx
{
public:
    DEFINE_FUNC_NEWINSTANCE(
        HttpClientCtx,
        ClientCtx,
        ( Server * server, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr ),
        ( server, clientId, clientEpStr, clientSockPtr )
    )

    using ClientCtx::ClientCtx;

    DataRecvSendCtx forClient;
    http::Header header;
};

}