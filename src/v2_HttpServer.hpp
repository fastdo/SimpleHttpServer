#pragma once

namespace v2
{
class HttpClientCtx;

/** HTTP服务器 */
class HttpServer : public Server
{
    // 一个完整请求到来
    _DEFINE_EVENT_RELATED(
        WebMain,
        ( winux::SharedPointer<v2::HttpClientCtx> httpClientCtxPtr, eienwebx::Response * rsp, void * runParam ),
        ( httpClientCtxPtr, rsp, runParam )
    )

public:

    /** \brief 构造函数1
     *
     *  \param ep 服务监听的EndPoint
     *  \param threadCount 线程池线程数量
     *  \param backlog listen(backlog) */
    HttpServer(
        eiennet::ip::EndPoint const & ep,
        int threadCount = 4,
        int backlog = 0
    );

protected:
    virtual void onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 ) override;

    /*virtual ClientCtx * onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<ip::tcp::Socket> clientSockPtr ) override
    {
        return new HttpClientCtx( clientId, clientEpStr, clientSockPtr );
    }*/
private:
    void onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body );
};

}
