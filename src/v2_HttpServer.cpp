
#include "v2_base.hpp"
#include "v2_ClientCtx.hpp"
#include "v2_Server.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequest.hpp"
#include "v2_HttpClientCtx.hpp"
#include "v2_HttpOutputMgr.hpp"

namespace v2
{

HttpServer::HttpServer( HttpApp * app, HttpServerConfig const & httpConfig ) :
    Server(),
    config(httpConfig),
    _app(app)
{

}

HttpServer::HttpServer(
    HttpApp * app,
    eiennet::ip::EndPoint const & ep,
    int threadCount,
    int backlog,
    double serverWait,
    double verboseInterval,
    bool verbose
) :
    Server( ep, threadCount, backlog, serverWait, verboseInterval, verbose ),
    config(app->settings),
    _app(app)
{

}

// 客户数据到达
void HttpServer::onClientDataArrived( winux::SharedPointer<ClientCtx> clientCtxPtr, winux::Buffer dataNew )
{
    winux::SharedPointer<HttpClientCtx> httpClientCtxPtr = clientCtxPtr.ensureCast<HttpClientCtx>();
    winux::GrowBuffer & data = httpClientCtxPtr->forClient.data;
    winux::GrowBuffer & extraData = httpClientCtxPtr->forClient.extraData;
    // 数据到达存下
    data.append(dataNew);

    switch ( httpClientCtxPtr->curRecvType )
    {
    case HttpClientCtx::drtRequestHeader:
        // 检测是否接收到了一个完整的HTTP-Header
        {
            // 指定标记
            thread_local winux::String target = "\r\n\r\n";
            thread_local auto targetNextVal = winux::_Templ_KmpCalcNext< char, short >(target.c_str(), (int)target.size());

            // 起始搜索位置
            int & startpos = httpClientCtxPtr->forClient.startpos;
            // 搜索到标记的位置
            int & pos = httpClientCtxPtr->forClient.pos;
            // 如果接收到的数据小于标记长度 或者 搜不到标记 则退出
            if ( data.getSize() - startpos < target.size() || ( pos = winux::_Templ_KmpMatchEx( data.getBuf<char>(), (int)data.getSize(), target.c_str(), (int)target.size(), startpos, targetNextVal ) ) == -1 )
            {
                if ( data.getSize() >= target.size() ) startpos = static_cast<int>( data.getSize() - target.size() + 1 ); // 计算下次搜索起始

            }
            else if ( !httpClientCtxPtr->hasHeader )
            {
                httpClientCtxPtr->hasHeader = true; // 标记此次请求已经收到请求头

                // 搜到指定标记时收到数据的大小（含指定标记）
                size_t searchedDataSize = pos + target.size();
                extraData._setSize(0);
                // 额外收到的数据
                extraData.append( data.getBuf<char>() + searchedDataSize, (winux::uint)( data.getSize() - searchedDataSize ) );
                data._setSize( (winux::uint)searchedDataSize );

                // 解析头部
                httpClientCtxPtr->request.header.clear();
                httpClientCtxPtr->request.header.parse( data.toAnsi() );

                // 额外的数据移入主数据中
                httpClientCtxPtr->forClient.data = std::move(httpClientCtxPtr->forClient.extraData);
                httpClientCtxPtr->forClient.resetStatus(); // 重置数据收发场景

                // 请求体大小
                httpClientCtxPtr->requestContentLength = httpClientCtxPtr->request.header.getHeader<winux::ulong>( "Content-Length", 0 );

                // 判断是否接收请求体
                if ( httpClientCtxPtr->requestContentLength > 0 )
                {
                    if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, httpClientCtxPtr->getStamp(), data.getSize(), ", ", httpClientCtxPtr->requestContentLength );
                    if ( data.getSize() == httpClientCtxPtr->requestContentLength ) // 收到的数据等于请求体数据的大小
                    {
                        winux::AnsiString body( data.getBuf<char>(), data.getSize() );
                        // 可以用线程池去处理调用onClientRequest事件
                        this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::move(body) ).post();
                        // 清空数据
                        data.free();

                        // 等待处理请求，设置接收类型为None
                        httpClientCtxPtr->curRecvType = HttpClientCtx::drtNone;
                    }
                    else // 请求体数据还没收完全，继续接收请求体
                    {
                        httpClientCtxPtr->curRecvType = HttpClientCtx::drtRequestBody;
                    }
                } 
                else // 没有请求体
                {
                    winux::AnsiString body;
                    // 可以用线程池去处理调用onClientRequest事件
                    this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::move(body) ).post();

                    // 等待处理请求，设置接收类型为None
                    httpClientCtxPtr->curRecvType = HttpClientCtx::drtNone;
                }
            }
        }
        break;
    case HttpClientCtx::drtRequestBody:
        {
            if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, httpClientCtxPtr->getStamp(), data.getSize(), ", ", httpClientCtxPtr->requestContentLength );
            if ( data.getSize() == httpClientCtxPtr->requestContentLength ) // 收到的数据等于请求体数据的大小
            {
                winux::AnsiString body( data.getBuf<char>(), data.getSize() );
                // 可以用线程池去处理调用onClientRequest事件
                this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::move(body) ).post();
                // 清空数据
                data.free();

                // 等待处理请求，设置接收类型为None
                httpClientCtxPtr->curRecvType = HttpClientCtx::drtNone;
            }
            else // 请求体数据还没收完全，继续接收请求体
            {
                httpClientCtxPtr->curRecvType = HttpClientCtx::drtRequestBody;
            }
        }
        break;
    default:
        break;
    }
}

 ClientCtx * HttpServer::onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr )
 {
     return new HttpClientCtx( _app, clientId, clientEpStr, clientSockPtr );
 }

// 收到一个请求
void HttpServer::onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body )
{
    if ( this->_verbose )
    {
        auto hdrStr = header.toString();
        winux::ColorOutputLine( winux::fgYellow, hdrStr, "body(bytes:", body.size(), ")" /*Base64Encode(body)*/ );
    }

    // 解析URL信息
    http::Url & url = httpClientCtxPtr->url;
    url.clear();
    url.parse( header.getUrl() );

    url.getPath();

    // 应该处理GET/POST/COOKIES/ENVIRON
    HttpRequest & request = httpClientCtxPtr->request;
    // 清空原先数据
    request.environVars.clear();
    request.cookies.clear();
    request.get.clear();
    request.post.clear();

    // 解析cookies
    request.cookies.loadCookies( header.getHeader("Cookie") );
    // 解析get querystring
    request.get.parse( url.getRawQueryStr() );
    // 解析POST变量
    http::Header::ContentType ct;
    if ( header.get( "Content-Type", &ct ) )
    {
        if ( ct.getMimeType() == "application/x-www-form-urlencoded" )
        {
            request.post.parse(body);
        }
    }

    // 响应处理
    if ( true )
    {
        // 创建响应
        eienwebx::Response rsp{ request, winux::MakeSimple( new HttpOutputMgr( httpClientCtxPtr->clientSockPtr.get() ) ) };
        // 调用WebMain处理函数
        this->onWebMain( httpClientCtxPtr, &rsp, rsp.request.app->getRunParam() );
    }

    // 检测Connection是否保活
    if ( winux::StrLower(header["Connection"]) != "keep-alive" ) // 连接不保活
    {
        httpClientCtxPtr->canRemove = true; // 标记为可移除
    }

    winux::ColorOutputLine( winux::fgAqua, httpClientCtxPtr->getStamp() );

    // 处理完请求，开始接收下一个请求头
    httpClientCtxPtr->hasHeader = false;
    httpClientCtxPtr->curRecvType = HttpClientCtx::drtRequestHeader;
}

}
