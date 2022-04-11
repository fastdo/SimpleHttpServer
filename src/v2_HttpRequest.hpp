#pragma once



namespace v2
{
class HttpApp;

class HttpRequest : public eienwebx::Request
{
public:
    HttpRequest( HttpApp * app, HttpClientCtx * clientCtx );

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

private:
    HttpClientCtx * _clientCtx;
};

}
