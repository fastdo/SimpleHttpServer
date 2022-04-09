
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
    _app(app),
    _staticFileCache(httpConfig.cacheLifeTime)
{

}

HttpServer::HttpServer(
    HttpApp * app,
    eiennet::ip::EndPoint const & ep,
    int threadCount,
    int backlog,
    double serverWait,
    double verboseInterval,
    bool verbose,
    int cacheLifeTime
) :
    Server( ep, threadCount, backlog, serverWait, verboseInterval, verbose ),
    config(app->settings),
    _app(app),
    _staticFileCache(cacheLifeTime)
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
                        httpClientCtxPtr->request.body.assign( data.getBuf<char>(), data.getSize() );
                        // 可以用线程池去处理调用onClientRequest事件
                        this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::ref(httpClientCtxPtr->request.body) ).post();
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
                    httpClientCtxPtr->request.body.clear();
                    // 可以用线程池去处理调用onClientRequest事件
                    this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::ref(httpClientCtxPtr->request.body) ).post();

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
                httpClientCtxPtr->request.body.assign( data.getBuf<char>(), data.getSize() );
                // 可以用线程池去处理调用onClientRequest事件
                this->_pool.task( &HttpServer::onClientRequestInternal, this, httpClientCtxPtr, std::ref(httpClientCtxPtr->request.header), std::ref(httpClientCtxPtr->request.body) ).post();
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

// 非文本数据用\xHH显示，如果数据过长则只显示len长度的数据
winux::AnsiString StrTruncateAndTextualize( winux::AnsiString const & content, size_t len )
{
    winux::AnsiString r;
    for ( auto ch : content )
    {
        if ( len == 0 ) break;
        if ( (winux::byte)ch > 31 && (winux::byte)ch < 128U || ch == '\r' || ch == '\n' || ch == '\t' )
        {
            r += ch;
        }
        else
        {
            r += winux::Format( "\\x%02X", (winux::byte)ch );
        }

        len--;
    }

    return r;
}

// 收到一个请求
void HttpServer::onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body )
{
    if ( this->_verbose )
    {
        auto hdrStr = header.toString();
        winux::ColorOutputLine( winux::fgYellow, httpClientCtxPtr->getStamp(), "\n", hdrStr, "body(bytes:", body.size(), ")\n", StrTruncateAndTextualize(body,256) /*Base64Encode(body)*/ );
    }

    HttpRequest & request = httpClientCtxPtr->request;

    // 处理请求数据
    HttpRequest::PerRequestData reqData;
    request.processData(&reqData);

    // 响应处理，首先判断路由响应处理器，其次判断do文件，最后判断静态文件，如果都没有则则404。
    if ( true )
    {
        // 创建响应
        eienwebx::Response rsp{ request, winux::MakeSimple( new HttpOutputMgr( httpClientCtxPtr->clientSockPtr.get() ) ) };
        // 调用WebMain处理函数
        //this->onWebMain( httpClientCtxPtr, rsp );

        // 路由处理，有两种路由处理，一种是过径路由，一种是普通路由
        // 过径路由是指URLPATH包含这个路径，这个路径注册的处理器会被调用，并且继续下去，直至找不到、或达到路径终止索引、或处理器返回false。
        for ( size_t i = 0; i < reqData.urlPathPartArr.size(); i++ )
        {
            if ( i < _crossRouter.size() )
            {
                auto & pathPartMap = _crossRouter[i];
                if ( pathPartMap.find( reqData.urlPathPartArr[i] ) != pathPartMap.end() )
                {
                    auto & methodMap = pathPartMap[ reqData.urlPathPartArr[i] ];

                    if ( methodMap.find( header.getMethod() ) != methodMap.end() )
                    {
                        if ( methodMap[ header.getMethod() ] )
                        {
                            if ( !methodMap[ header.getMethod() ]( httpClientCtxPtr, rsp, reqData.urlPathPartArr, i ) )
                                break;
                        }
                    }
                    else if (  methodMap.find("*") != methodMap.end() )
                    {
                        if ( methodMap["*"] )
                        {
                            if ( !methodMap["*"]( httpClientCtxPtr, rsp, reqData.urlPathPartArr, i ) )
                                break;
                        }
                    }

                    if ( i == reqData.iEndUrlPath )
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        // 普通路由是指调用URLPATH这个路径注册的处理器
        if ( _router.find(reqData.urlPath) != _router.end() )
        {
            auto & methodMap = _router[reqData.urlPath];
            if ( methodMap.find( header.getMethod() ) != methodMap.end() )
            {
                methodMap[ header.getMethod() ]( httpClientCtxPtr, rsp );
            }
            else if (  methodMap.find("*") != methodMap.end() )
            {
                methodMap["*"]( httpClientCtxPtr, rsp );
            }
            else
            {
                rsp.header.setResponseLine( "HTTP/1.1 501 Not implemented", false );
            }
        }
        else // 在普通路由里没有找到处理器
        {
            winux::String fullPath;
            if ( reqData.isFile ) // 是文件
            {
                fullPath = request.environVars["SCRIPT_FILENAME"];
            }
            else if ( reqData.isExist ) // 不是文件，但路径存在
            {
                if ( reqData.urlPathPartArr[reqData.iEndUrlPath].empty() )
                {
                    for ( auto const & indexFileName : this->config.documentIndex )
                    {
                        winux::FileTitle( indexFileName, &reqData.extName );
                        fullPath = winux::CombinePath( this->config.documentRoot + reqData.urlPath, indexFileName );
                        if ( winux::DetectPath(fullPath) )
                        {
                            reqData.isFile = true;
                            break;
                        }
                    }
                }
                else
                {
                    rsp.header.setStatusCode("301").setStatusStr("Moved Permanently");
                    rsp.header["Location"] = winux::PathNoSep(reqData.urlPath) + "/";
                    goto END_ROUTE;
                }
            }

            if ( reqData.isFile )
            {
                if ( reqData.extName == "do" )
                {
                    // 执行do文件
                    int rc;
                    _app->execWebMain( fullPath, &rsp, _app->getRunParam(), &rc );
                }
                else
                {
                    // 静态文件
                    if ( _staticFileCache.hasCache(fullPath) )
                    {
                        auto const & cacheItem = _staticFileCache.get(fullPath);

                        rsp.header["Content-Type"] = cacheItem.mime;
                        rsp.write(cacheItem.content);

                        if ( this->_verbose )
                            winux::ColorOutputLine( winux::fgAqua, "读取到了缓存`", fullPath, "`" );
                    }
                    else
                    {
                        auto & cacheItem = _staticFileCache.writeCache( fullPath, this->config.getMime(reqData.extName), winux::FileGetContentsEx( fullPath, false ) );

                        rsp.header["Content-Type"] = cacheItem.mime;
                        rsp.write(cacheItem.content);

                        if ( this->_verbose )
                            winux::ColorOutputLine( winux::fgAqua, "读取到了文件`", fullPath, "`" );
                        //winux::ColorOutputLine( winux::fgFuchsia, "Static file=", fullPath, ", size:", data.getSize() );
                    }
                }
            }
            else
            {
                //winux::ColorOutputLine( winux::fgFuchsia, "Static file=", fullPath );

                rsp.header.setResponseLine( "HTTP/1.1 404 Not found", false );
            }
        }
        //*/
    }
END_ROUTE:
    // 检测Connection是否保活
    if ( winux::StrLower(header["Connection"]) != "keep-alive" ) // 连接不保活
    {
        httpClientCtxPtr->canRemove = true; // 标记为可移除
    }

    // 处理完请求，清理header和body，开始接收下一个请求头
    httpClientCtxPtr->request.header.clear();
    httpClientCtxPtr->request.body.clear();
    httpClientCtxPtr->request.body.shrink_to_fit();
    httpClientCtxPtr->hasHeader = false;
    httpClientCtxPtr->curRecvType = HttpClientCtx::drtRequestHeader;
}

void HttpServer::setCrossRouteHandler( winux::String const & method, winux::String const & path, CrossRouteHandlerFunction handler )
{
    winux::StringArray urlPathPartArr;
    if ( path.length() > 0 && path[0] == '/' )
    {
        winux::StrSplit( path.substr(1), "/", &urlPathPartArr, false );
    }
    else
    {
        winux::StrSplit( path, "/", &urlPathPartArr, false );
    }

    urlPathPartArr.insert( urlPathPartArr.begin(), "" );

    // 记下每一个部分
    for ( size_t i = 0; i < urlPathPartArr.size(); i++ )
    {
        if ( i >= _crossRouter.size() ) _crossRouter.emplace_back();

        auto & pathPartMap = _crossRouter[i];

        auto & methodMap = pathPartMap[ urlPathPartArr[i] ];

        if ( i == urlPathPartArr.size() - 1 )
        {
            winux::StringArray methodArr;
            winux::StrSplit( method, ",", &methodArr, false );
            for ( auto & m : methodArr )
            {
                methodMap[m] = handler;
            }
        }
    }

}

void HttpServer::setRouteHandler( winux::String const & method, winux::String const & path, RouteHandlerFunction handler )
{
    auto & methodMap = _router[path];

    winux::StringArray methodArr;
    winux::StrSplit( method, ",", &methodArr, false );
    for ( auto & m : methodArr )
    {
        methodMap[m] = handler;
    }
}

}
