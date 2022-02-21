
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpClientCtx.hpp"
#include "v2_HttpServer.hpp"

namespace v2
{

HttpServer::HttpServer(
    ip::EndPoint const & ep,
    int threadCount /*= 4*/,
    int backlog /*= 0*/,
    ClientCtxConstructor clientConstructor /*= HttpClientCtx::NewInstance */
) : Server( ep, threadCount, backlog, clientConstructor )
{

}

void HttpServer::onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer data0 )
{
    winux::SharedPointer<HttpClientCtx> httpClientCtxPtr = clientCtxPtr.ensureCast<HttpClientCtx>();
    winux::GrowBuffer & data = httpClientCtxPtr->forClient.data;
    winux::GrowBuffer & extraData = httpClientCtxPtr->forClient.extraData;

    data.append(data0);

    // 检测是否接收到了一个完整的HTTP-Header
    {
        // 指定标记
        thread_local winux::String target = "\r\n\r\n";
        thread_local auto targetNextVal = winux::_Templ_KmpCalcNext< char, short >(target.c_str(), (int)target.size());

        // 起始搜索位置
        int & startpos = httpClientCtxPtr->forClient.startpos;
        // 搜索到标记的位置
        int & pos = httpClientCtxPtr->forClient.pos;
        // 如果可搜索的数据小于标记长度 或者 搜不到标记 则 退出
        if ( data.getSize() - startpos < target.size() || ( pos = winux::_Templ_KmpMatchEx(data.getBuf<char>(), (int)data.getSize(), target.c_str(), (int)target.size(), startpos, targetNextVal) ) == -1 )
        {
            if ( data.getSize() >= target.size() ) startpos = static_cast<int>( data.getSize() - target.size() + 1 ); // 计算下次搜索起始

            return;
        }

        // 搜到指定标记时收到数据的大小（含指定标记）
        size_t searchedDataSize = pos + target.size();
        extraData._setSize(0);
        // 额外收到的数据
        extraData.append(data.getBuf<char>() + searchedDataSize, ( winux::uint )( data.getSize() - searchedDataSize ));
        data._setSize(( winux::uint )searchedDataSize);

        // 解析头部
        httpClientCtxPtr->header.clear();
        httpClientCtxPtr->header.parse(data.toAnsi());

        // 调用收到RequestHeader事件
        this->_pool.task( &HttpServer::onClientRequestHeader, this, httpClientCtxPtr, httpClientCtxPtr->header ).post();

        // 额外的数据移入主数据中
        httpClientCtxPtr->forClient.data = std::move(httpClientCtxPtr->forClient.extraData);
        httpClientCtxPtr->forClient.resetStatus(); // 重置数据收发场景
    }

    // 调用父类的处理
    Server::onClientDataArrived(clientCtxPtr, std::move(data0));
}

void HttpServer::onClientRequestHeader( winux::SharedPointer<HttpClientCtx> clientCtxPtr, http::Header const & header )
{
    //if ( outputVerbose ) winux::ColorOutput( winux::fgBlue, "HttpServer::onClientRequestHeader()" );


    if ( this->_ClientRequestHeaderHandler ) this->_ClientRequestHeaderHandler( clientCtxPtr, header );

    if ( winux::StrLower(header["Connection"]) != "keep-alive" ) // 连接不保活
    {
        clientCtxPtr->canRemove = true; // 标记为可移除
    }
}

}
