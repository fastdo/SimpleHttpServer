#pragma once

namespace v2
{
class HttpApp;

/** HTTP客户请求场景 */
class HttpRequestCtx : public eiennet::ClientCtx, public eienwebx::Request
{
public:
    HttpRequestCtx( HttpApp * app, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr );

    virtual ~HttpRequestCtx();

    // 数据接收类型
    enum DataRecvType
    {
        drtNone,
        drtRequestHeader,
        drtRequestBody,
    };

    eiennet::DataRecvSendCtx forClient; // 接收数据的一些中间变量
    http::Url url; // 请求的URL

    DataRecvType curRecvType; // 当前要接收的类型
    bool hasHeader; // 标记是否读取到了请求头，这个用来避免请求体数据包含有\r\n从而导致请求头错误
    winux::ulong requestContentLength; // 请求包含的请求体内容大小

    /** \brief 处理每次请求的一些中间变量 */
    struct PerRequestData
    {
        winux::String urlPath; // URL路径，会以/开头
        winux::StringArray urlPathPartArr; // 分割url.getPath()，第一个元素始终是空串，表示起始根路径
        size_t iEndUrlPath; // 停止在urlPath所达到的那个部分的索引
        winux::String extName; // 扩展名
        bool isExist = true; // 路径在文档根目录是否存在
        bool isFile = false; // 是否为文件。由于扩展名可能是空，所以增加这个变量表示urlPath是否是文件
    };

    bool processData( void * data ) override;
};

}
