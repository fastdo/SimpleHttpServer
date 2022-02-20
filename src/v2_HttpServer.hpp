#pragma once

namespace v2
{
/** HTTP服务器 */
class HttpServer : public Server
{
    // 收到一个请求头，应该分析是否需要继续收额外的头或者消息体
    _DEFINE_EVENT_RELATED_EX(
        ClientRequestHeader,
        ( winux::SharedPointer<HttpClientCtx> clientCtxPtr, http::Header const & header )
    )
    {
        if ( outputVerbose ) winux::ColorOutput( winux::fgBlue, "HttpServer::onClientRequestHeader()" );


        if ( this->_ClientRequestHeaderHandler ) this->_ClientRequestHeaderHandler( clientCtxPtr, header );

        if ( winux::StrLower(header["Connection"]) != "keep-alive" ) // 连接不保活
        {
            clientCtxPtr->canRemove = true; // 标记为可移除
        }
    }

    // 一个完整请求到来
    _DEFINE_EVENT_RELATED(
        ClientRequest,
        (),
        ()
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
    ) : Server( ep, threadCount, backlog, clientConstructor )
    {
    }

protected:
    virtual void onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 ) override
    {
        winux::SharedPointer<HttpClientCtx> httpClientCtxPtr =  clientCtxPtr.ensureCast<HttpClientCtx>();
        winux::GrowBuffer & data = httpClientCtxPtr->forClient.data;
        winux::GrowBuffer & extraData = httpClientCtxPtr->forClient.extraData;

        data.append(data0);

        // 检测是否接收到了一个完整的Http-Header
        {
            thread_local winux::String target = "\r\n\r\n";
            thread_local auto targetNextVal = winux::_Templ_KmpCalcNext< char, short >( target.c_str(), (int)target.size() );

            // 起始搜索位置
            int & startpos = httpClientCtxPtr->forClient.startpos;
            int & pos = httpClientCtxPtr->forClient.pos;

            if ( data.getSize() - startpos < target.size() || ( pos = winux::_Templ_KmpMatchEx( data.getBuf<char>(), (int)data.getSize(), target.c_str(), (int)target.size(), startpos, targetNextVal ) ) == -1 )
            {
                if ( data.getSize() >= target.size() ) startpos = static_cast<int>(data.getSize() - target.size() + 1); // 计算下次搜索起始

                return;
            }

            size_t searchedDataSize = pos + target.size(); // 搜到指定数据时的大小（含指定数据）
            extraData._setSize(0);
            extraData.append( data.getBuf<char>() + searchedDataSize, (winux::uint)(data.getSize() - searchedDataSize) ); // 额外收到的数据
            data._setSize((winux::uint)searchedDataSize);

            // 解析头部
            httpClientCtxPtr->header.clear();
            httpClientCtxPtr->header.parse( data.toAnsi() );

            // 调用收到Header事件
            this->_pool.task( &HttpServer::onClientRequestHeader, this, httpClientCtxPtr, httpClientCtxPtr->header ).post();

            // 额外的数据移入主数据中
            httpClientCtxPtr->forClient.data = std::move(httpClientCtxPtr->forClient.extraData);
            httpClientCtxPtr->forClient.resetStatus(); // 重置数据收发场景
        }

        // 调用父类的处理
        Server::onClientDataArrived( clientCtxPtr, std::move(data0) );
    }
};

}