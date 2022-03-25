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
        ( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr ),
        ( clientId, clientEpStr, clientSockPtr )
    )

    HttpClientCtx( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr ) :
        ClientCtx( clientId, clientEpStr, clientSockPtr ),
        curRecvType(drtRequestHeader),
        hasHeader(false),
        requestContentLength(0L)
    {
    }

    // 数据接收类型
    enum DataRecvType
    {
        drtNone,
        drtRequestHeader,
        drtRequestBody,
    };

    DataRecvSendCtx forClient; // 接收数据的一些中间变量
    http::Header header; // 请求头
    //winux::AnsiString body; // 请求体

    DataRecvType curRecvType; // 当前要接收的类型
    bool hasHeader; // 标记是否读取到了请求头，这个用来避免请求体数据包含有\r\n从而导致请求头错误
    winux::ulong requestContentLength;
};

}
