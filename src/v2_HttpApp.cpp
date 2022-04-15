#include "v2_base.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequestCtx.hpp"
#include "v2_HttpOutputMgr.hpp"

namespace v2
{

HttpApp::HttpApp( winux::ConfigureSettings const & settings, struct xAppServerData * servData ) :
    App( settings, servData ),
    Server(),
    httpConfig(settings),
    _staticFileCache(httpConfig.cacheLifeTime)
{
    // 启动服务
    this->startup(
        eiennet::ip::EndPoint( httpConfig.serverIp, httpConfig.serverPort ),
        httpConfig.threadCount,
        httpConfig.listenBacklog,
        httpConfig.serverWait,
        httpConfig.verboseInterval,
        httpConfig.verbose
    );
}

HttpApp::HttpApp(
    winux::ConfigureSettings const & settings,
    struct xAppServerData * servData,
    eiennet::ip::EndPoint const & ep,
    int threadCount,
    int backlog,
    double serverWait,
    double verboseInterval,
    bool verbose,
    int cacheLifeTime
) :
    App( settings, servData ),
    Server(),
    httpConfig( settings, ep, threadCount, backlog, serverWait, verboseInterval, verbose, cacheLifeTime ),
    _staticFileCache(httpConfig.cacheLifeTime)
{
    // 启动服务
    this->startup(
        eiennet::ip::EndPoint( httpConfig.serverIp, httpConfig.serverPort ),
        httpConfig.threadCount,
        httpConfig.listenBacklog,
        httpConfig.serverWait,
        httpConfig.verboseInterval,
        httpConfig.verbose
    );
}

void HttpApp::crossRoute( winux::String const & method, winux::String const & path, CrossRouteHandlerFunction handler )
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

void HttpApp::route( winux::String const & method, winux::String const & path, RouteHandlerFunction handler )
{
    auto & methodMap = _router[path];

    winux::StringArray methodArr;
    winux::StrSplit( method, ",", &methodArr, false );
    for ( auto & m : methodArr )
    {
        methodMap[m] = handler;
    }
}

int HttpApp::run( void * runParam )
{
    this->_runParam = runParam;

    return Server::run(runParam);
}

// 客户数据到达
void HttpApp::onClientDataArrived( winux::SharedPointer<eiennet::ClientCtx> clientCtxPtr, winux::Buffer dataNew )
{
    winux::SharedPointer<HttpRequestCtx> requestCtxPtr = clientCtxPtr.ensureCast<HttpRequestCtx>();
    winux::GrowBuffer & data = requestCtxPtr->forClient.data;
    winux::GrowBuffer & extraData = requestCtxPtr->forClient.extraData;
    // 数据到达存下
    data.append(dataNew);

    switch ( requestCtxPtr->curRecvType )
    {
    case HttpRequestCtx::drtRequestHeader:
        // 检测是否接收到了一个完整的HTTP-Header
        {
            // 指定标记
            thread_local winux::String target = "\r\n\r\n";
            thread_local auto targetNextVal = winux::_Templ_KmpCalcNext< char, short >(target.c_str(), (int)target.size());

            // 起始搜索位置
            int & startpos = requestCtxPtr->forClient.startpos;
            // 搜索到标记的位置
            int & pos = requestCtxPtr->forClient.pos;
            // 如果接收到的数据小于标记长度 或者 搜不到标记 则退出
            if ( data.getSize() - startpos < target.size() || ( pos = winux::_Templ_KmpMatchEx( data.getBuf<char>(), (int)data.getSize(), target.c_str(), (int)target.size(), startpos, targetNextVal ) ) == -1 )
            {
                if ( data.getSize() >= target.size() ) startpos = static_cast<int>( data.getSize() - target.size() + 1 ); // 计算下次搜索起始

            }
            else if ( !requestCtxPtr->hasHeader )
            {
                requestCtxPtr->hasHeader = true; // 标记此次请求已经收到请求头

                // 搜到指定标记时收到数据的大小（含指定标记）
                size_t searchedDataSize = pos + target.size();
                extraData._setSize(0);
                // 额外收到的数据
                extraData.append( data.getBuf<char>() + searchedDataSize, (winux::uint)( data.getSize() - searchedDataSize ) );
                data._setSize( (winux::uint)searchedDataSize );

                // 解析头部
                requestCtxPtr->header.parse( data.toAnsi() );

                // 额外的数据移入主数据中
                requestCtxPtr->forClient.data = std::move(requestCtxPtr->forClient.extraData);
                requestCtxPtr->forClient.resetStatus(); // 重置数据收发场景

                // 请求体大小
                requestCtxPtr->requestContentLength = requestCtxPtr->header.getHeader<winux::ulong>( "Content-Length", 0 );

                // 判断是否接收请求体
                if ( requestCtxPtr->requestContentLength > 0 )
                {
                    if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, requestCtxPtr->getStamp(), data.getSize(), ", ", requestCtxPtr->requestContentLength );
                    if ( data.getSize() == requestCtxPtr->requestContentLength ) // 收到的数据等于请求体数据的大小
                    {
                        requestCtxPtr->body.assign( data.getBuf<char>(), data.getSize() );
                        // 可以用线程池去处理调用onClientRequest事件
                        this->_pool.task( &HttpApp::onClientRequestInternal, this, requestCtxPtr, std::ref(requestCtxPtr->header), std::ref(requestCtxPtr->body) ).post();
                        // 清空数据
                        data.free();

                        // 等待处理请求，设置接收类型为None
                        requestCtxPtr->curRecvType = HttpRequestCtx::drtNone;
                    }
                    else // 请求体数据还没收完全，继续接收请求体
                    {
                        requestCtxPtr->curRecvType = HttpRequestCtx::drtRequestBody;
                    }
                } 
                else // 没有请求体
                {
                    requestCtxPtr->body.clear();
                    // 可以用线程池去处理调用onClientRequest事件
                    this->_pool.task( &HttpApp::onClientRequestInternal, this, requestCtxPtr, std::ref(requestCtxPtr->header), std::ref(requestCtxPtr->body) ).post();

                    // 等待处理请求，设置接收类型为None
                    requestCtxPtr->curRecvType = HttpRequestCtx::drtNone;
                }
            }
        }
        break;
    case HttpRequestCtx::drtRequestBody:
        {
            if ( this->_verbose ) winux::ColorOutputLine( winux::fgGreen, requestCtxPtr->getStamp(), data.getSize(), ", ", requestCtxPtr->requestContentLength );
            if ( data.getSize() == requestCtxPtr->requestContentLength ) // 收到的数据等于请求体数据的大小
            {
                requestCtxPtr->body.assign( data.getBuf<char>(), data.getSize() );
                // 可以用线程池去处理调用onClientRequest事件
                this->_pool.task( &HttpApp::onClientRequestInternal, this, requestCtxPtr, std::ref(requestCtxPtr->header), std::ref(requestCtxPtr->body) ).post();
                // 清空数据
                data.free();

                // 等待处理请求，设置接收类型为None
                requestCtxPtr->curRecvType = HttpRequestCtx::drtNone;
            }
            else // 请求体数据还没收完全，继续接收请求体
            {
                requestCtxPtr->curRecvType = HttpRequestCtx::drtRequestBody;
            }
        }
        break;
    default:
        break;
    }
}

eiennet::ClientCtx * HttpApp::onCreateClient( winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr )
{
    return new HttpRequestCtx( this, clientId, clientEpStr, clientSockPtr );
}

// 非文本数据用\xHH显示，如果数据过长则只显示len长度的数据
static winux::AnsiString __StrTruncateAndTextualize( winux::AnsiString const & content, size_t len )
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
void HttpApp::onClientRequestInternal( winux::SharedPointer<HttpRequestCtx> requestCtxPtr, http::Header & header, winux::AnsiString & body )
{
    if ( this->_verbose )
    {
        auto hdrStr = header.toString();
        winux::ColorOutputLine( winux::fgYellow, requestCtxPtr->getStamp(), "\n", hdrStr, "body(bytes:", body.size(), ")\n", __StrTruncateAndTextualize(body,256) /*Base64Encode(body)*/ );
    }

    HttpRequestCtx & request = *requestCtxPtr.get();

    // 处理请求数据
    HttpRequestCtx::PerRequestData reqData;
    request.processData(&reqData);

    // 响应处理，首先判断路由响应处理器，其次判断do文件，最后判断静态文件，如果都没有则则404。
    if ( true )
    {
        // 创建响应
        eienwebx::Response rsp{ request, winux::MakeSimple( new HttpOutputMgr( requestCtxPtr->clientSockPtr.get() ) ) };
        // 调用WebMain处理函数
        //this->onWebMain( requestCtxPtr, rsp );

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
                            if ( !methodMap[ header.getMethod() ]( requestCtxPtr, rsp, reqData.urlPathPartArr, i ) )
                            {
                                //break;
                                goto END_ROUTE;
                            }
                        }
                    }
                    else if (  methodMap.find("*") != methodMap.end() )
                    {
                        if ( methodMap["*"] )
                        {
                            if ( !methodMap["*"]( requestCtxPtr, rsp, reqData.urlPathPartArr, i ) )
                            {
                                //break;
                                goto END_ROUTE;
                            }
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
                methodMap[ header.getMethod() ]( requestCtxPtr, rsp );
            }
            else if (  methodMap.find("*") != methodMap.end() )
            {
                methodMap["*"]( requestCtxPtr, rsp );
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
                    for ( auto const & indexFileName : this->httpConfig.documentIndex )
                    {
                        winux::FileTitle( indexFileName, &reqData.extName );
                        fullPath = winux::CombinePath( this->httpConfig.documentRoot + reqData.urlPath, indexFileName );
                        if ( winux::DetectPath(fullPath) )
                        {
                            reqData.isFile = true;
                            break;
                        }
                    }
                }
                else
                {
                    // 301重定向
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
                    this->execWebMain( fullPath, &rsp, this->_runParam, &rc );
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
                        auto & cacheItem = _staticFileCache.writeCache( fullPath, this->httpConfig.getMime(reqData.extName), winux::FileGetContentsEx( fullPath, false ) );

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

                auto content = winux::FileGetContentsEx( this->httpConfig.errorPages["404"], false );
                rsp.write(content);
            }
        }
        //*/
    }
END_ROUTE:
    // 检测Connection是否保活
    if ( winux::StrLower(header["Connection"]) != "keep-alive" ) // 连接不保活
    {
        requestCtxPtr->canRemove = true; // 标记为可移除
    }

    // 处理完请求，清理header和body，开始接收下一个请求头
    requestCtxPtr->header.clear();
    requestCtxPtr->body.clear();
    requestCtxPtr->body.shrink_to_fit();
    requestCtxPtr->hasHeader = false;
    requestCtxPtr->curRecvType = HttpRequestCtx::drtRequestHeader;
}

}
