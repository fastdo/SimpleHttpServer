#pragma once

namespace v2
{
class HttpApp;

/** HTTP客户场景 */
class HttpClientCtx : public eiennet::ClientCtx
{
public:
    HttpClientCtx( HttpApp * app, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr );

    ~HttpClientCtx();

    // 数据接收类型
    enum DataRecvType
    {
        drtNone,
        drtRequestHeader,
        drtRequestBody,
    };

    eiennet::DataRecvSendCtx forClient; // 接收数据的一些中间变量
    http::Url url;
    HttpRequest request; // 存储请求

    DataRecvType curRecvType; // 当前要接收的类型
    bool hasHeader; // 标记是否读取到了请求头，这个用来避免请求体数据包含有\r\n从而导致请求头错误
    winux::ulong requestContentLength;
};

}
