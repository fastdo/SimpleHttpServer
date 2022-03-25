#pragma once

namespace v2
{
/** HTTP服务器 */
class HttpServer : public Server
{
    // 一个完整请求到来
    _DEFINE_EVENT_RELATED(
        ClientRequest,
        ( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body ),
        ( httpClientCtxPtr, header, body )
    )

public:

    /** \brief 构造函数1
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog) */
    HttpServer(
        ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0,
        ClientCtxConstructor clientConstructor = HttpClientCtx::NewInstance
    );

protected:
    virtual void onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 ) override;
private:
    void onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body );
};

}
