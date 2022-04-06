
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

static void __ProcessMultipartFormData( char const * buf, winux::ulong size, winux::String const & boundary, http::Vars * post )
{
    winux::String sep = "--" + boundary; // separator
    std::vector<int> sepNextVal = winux::KmpCalcNext( sep.c_str(), (int)sep.length() );
    int start = 0;
    int pos = winux::KmpMatchEx( buf, size, sep.c_str(), (int)sep.length(), start, sepNextVal );
    if ( pos != -1 ) // first separator is found
    {
        start = pos + (int)sep.length();
        sep = "\r\n--" + boundary;
        sepNextVal = winux::KmpCalcNext( sep.c_str(), (int)sep.length() );

        while ( ( pos = winux::KmpMatchEx( buf, size, sep.c_str(), (int)sep.length(), start, sepNextVal ) ) != -1 )
        {
            int skipN = 2; // \r\n
            winux::AnsiString data( buf + skipN + start, ( pos - start ) - skipN ); // buf + 2 for skip \r\n

            ////////////////////////////////////////////////////////////////////
            winux::AnsiString nlnl = "\r\n\r\n";
            int posBody = winux::KmpMatch( data.c_str(), (int)data.length(), nlnl.c_str(), (int)nlnl.length(), 0 );
            if ( posBody != -1 )
            {
                winux::String headerStr = data.substr( 0, posBody );

                char const * body = data.c_str() + posBody + nlnl.length();
                int bodySize = (int)data.length() - posBody - (int)nlnl.length();

                http::Header header; // 头部
                header.parse(headerStr);

                http::Header::ContentDisposition cttDpn;
                header.get( "Content-Disposition", &cttDpn );
                if ( cttDpn.hasOption("filename") ) // 含有filename是上传文件
                {
                    winux::String filename = cttDpn.getOption("filename");
                    winux::Mixed v; // 将其组成一个map作为post变量
                    v.createCollection();
                    v["name"] = filename;
                    v["size"] = bodySize;
                    v["mime"] = header["Content-Type"];

                    if ( !filename.empty() )
                    {
                        winux::SimpleHandle<char*> tmpPath( _tempnam( "", "upload_" ), (char*)NULL, free );
                        v["path"] = tmpPath.get(); // 临时文件路径
                        winux::File f( tmpPath.get(), "wb" ); // 将上传的内容存入临时文件
                        f.write( body, bodySize );
                    }
                    else
                    {
                        v["path"] = ""; // 临时文件路径
                    }

                    (*post)[ cttDpn.getOption("name") ] = v;
                }
                else // 是普通Post变量
                {
                    winux::String varVal;
                    varVal.resize( bodySize + 1 );
                    memcpy( &varVal[0], body, bodySize );
                    (*post)[ cttDpn.getOption("name") ] = varVal.c_str();
                }
            }
            ////////////////////////////////////////////////////////////////////
            start = pos + (int)sep.length();
        }
    }
}

// 拆解URL路径部分字符串为urlPath和requestPathInfo，以含扩展名的部分作分隔
void ProcessUrlPathOld( winux::String const & urlPathRaw, winux::String const & documentRootDir, winux::StringArray * urlPathArr, size_t * iEndUrlPath, winux::String * urlPath, winux::String * requestPathInfo, winux::String * extName )
{
    // url路径，子路径数组
    winux::StrSplit( urlPathRaw, "/", urlPathArr, false );

    urlPathArr->insert( urlPathArr->begin(), "" );
    *iEndUrlPath = 0;

    if ( urlPathArr->size() > 1 )
    {
        size_t i = 1;
        while ( i < urlPathArr->size() )
        {
            winux::String & comp = (*urlPathArr)[i];
            *urlPath += "/" + comp;
            *iEndUrlPath = i;

            winux::FileTitle( comp, extName );

            i++;

            if ( !extName->empty() )
                break;
        }

        while ( i < urlPathArr->size() )
        {
            winux::String & comp = (*urlPathArr)[i];
            *requestPathInfo += "/" + comp;

            if ( extName->empty() )
                *iEndUrlPath = i;

            i++;
        }
    }
    else
    {
        *urlPath += "/";
    }
}

// 根据文档根目录内实际文件，拆解URL路径部分字符串为urlPath和requestPathInfo
void ProcessUrlPath(
    winux::String const & urlRawPathStr,
    winux::String const & documentRootDir,
    winux::StringArray * urlPathPartArr,
    size_t * iEndUrlPath,
    winux::String * urlPath,
    winux::String * requestPathInfo,
    winux::String * extName,
    bool * isFile
)
{
    // url路径，子路径数组
    winux::StrSplit( urlRawPathStr, "/", urlPathPartArr, false );

    urlPathPartArr->insert( urlPathPartArr->begin(), "" );
    *iEndUrlPath = 0;

    if ( urlPathPartArr->size() > 1 )
    {
        // 求urlPath和PATH_INFO：一层层路径判断，如果找到存在的文件，则说明后续的/...是PATH_INFO，如果是文件夹，则进行下一级判断，如果路径不存在，则当成urlPath。
        winux::String tmpPath;
        size_t i = 1;

        while ( i < urlPathPartArr->size() )
        {
            winux::String & comp = (*urlPathPartArr)[i];

            tmpPath += "/" + comp;
            *iEndUrlPath = i;

            bool isDir = false;
            if ( winux::DetectPath( documentRootDir + winux::DirSep + tmpPath, &isDir ) )
            {
                if ( isDir )
                {
                    *urlPath = tmpPath;
                    i++;
                }
                else
                {
                    winux::FileTitle( comp, extName );

                    *urlPath = tmpPath;
                    i++;
                    *isFile = true;
                    break;
                }
            }
            else
            {
                break;
            }
        }

        while ( i < urlPathPartArr->size() )
        {
            winux::String & comp = (*urlPathPartArr)[i];

            if ( !*isFile )
            {
                *urlPath += "/" + comp;
                *iEndUrlPath = i;
            }
            else
            {
                *requestPathInfo += "/" + comp;
            }
            i++;
        }
    }
    else
    {
        *urlPath += "/";
    }
}

// 收到一个请求
void HttpServer::onClientRequestInternal( winux::SharedPointer<HttpClientCtx> httpClientCtxPtr, http::Header & header, winux::AnsiString & body )
{
    if ( this->_verbose )
    {
        auto hdrStr = header.toString();
        winux::ColorOutputLine( winux::fgYellow, httpClientCtxPtr->getStamp(), "\n", hdrStr, "body(bytes:", body.size(), ")\n", StrTruncateAndTextualize(body,256) /*Base64Encode(body)*/ );
    }

    // 解析URL信息
    http::Url & url = httpClientCtxPtr->url;
    url.clear();
    url.parse( header.getUrl(), false, true, true, true, true );

    // 应该处理GET/POST/COOKIES/ENVIRON
    HttpRequest & request = httpClientCtxPtr->request;

    // 清空原先数据
    request.environVars.clear();
    request.cookies.clear();
    request.get.clear();
    request.post.clear();

    // 记下REQUEST_URI
    request.environVars["REQUEST_URI"] = header.getUrl();

    // 解析cookies
    request.cookies.loadCookies( header.getHeader("Cookie") );
    // 解析get querystring
    request.get.parse( url.getRawQueryStr() );
    // 解析POST变量
    http::Header::ContentType ct;
    // 取得请求体类型
    if ( header.get( "Content-Type", &ct ) )
    {
        winux::String contentMimeType = ct.getMimeType();
        if ( contentMimeType == "application/x-www-form-urlencoded" )
        {
            request.post.parse(body);
        }
        else if ( contentMimeType == "multipart/form-data" )
        {
            __ProcessMultipartFormData( body.c_str(), (winux::ulong)body.size(), ct.getBoundary(), &request.post );
        }
    }

    // 拆解路径部分信息（如果路径信息包含具体文件，之后的信息会当作PATH_INFO，否则全为urlPath）
    winux::String urlRawPathStr = url.getPath();
    request.environVars["ORIG_PATH_INFO"] = "/" + urlRawPathStr;

    winux::String urlPath; // URL路径，会以/开头
    winux::StringArray urlPathPartArr; // 分割urlRawPathStr，第一个元素始终是空串，表示起始根路径
    size_t iEndUrlPath; // 停止在urlPath所达到的那个部分的索引
    winux::String requestPathInfo; // PATH_INFO
    winux::String extName; // 扩展名
    bool isFile = false; // 是否为文件。由于扩展名可能是空，所以增加这个变量表示urlPath是否是文件
    ProcessUrlPath( urlRawPathStr, this->config.documentRoot, &urlPathPartArr, &iEndUrlPath, &urlPath, &requestPathInfo, &extName, &isFile );

    //winux::Mixed info;
    //info.addPair()
    //    ( "urlPath", urlPath )
    //    ( "urlPathPartArr", urlPathPartArr )
    //    ( "iEndUrlPath", iEndUrlPath )
    //    ( "requestPathInfo", requestPathInfo )
    //    ( "extName", extName )
    //    ( "isFile", isFile )
    //;
    //winux::ColorOutputLine( winux::fgGreen, info.myJson(true, "  ", "\n") );

    // 记下环境变量
    request.environVars["DOCUMENT_ROOT"] = this->config.documentRoot;
    request.environVars["SCRIPT_FILENAME"] = this->config.documentRoot + urlPath;
    request.environVars["URL_PATH"] = request.environVars["SCRIPT_NAME"] = urlPath;
    request.environVars["PATH_INFO"] = requestPathInfo;


    // 响应处理，首先判断路由响应处理器，其次判断do文件，最后判断静态文件，如果都没有则则404。
    if ( true )
    {
        // 创建响应
        eienwebx::Response rsp{ request, winux::MakeSimple( new HttpOutputMgr( httpClientCtxPtr->clientSockPtr.get() ) ) };
        // 调用WebMain处理函数
        //this->onWebMain( httpClientCtxPtr, rsp );

        // 路由处理，有两种路由处理，一种是过径路由，一种是普通路由
        // 过径路由是指URLPATH包含这个路径，这个路径注册的处理器会被调用，并且继续下去，直至找不到、或达到路径终止索引、或处理器返回false。
        for ( size_t i = 0; i < urlPathPartArr.size(); i++ )
        {
            if ( i < _crossRouter.size() )
            {
                auto & pathPartMap = _crossRouter[i];
                if ( pathPartMap.find( urlPathPartArr[i] ) != pathPartMap.end() )
                {
                    auto & methodMap = pathPartMap[ urlPathPartArr[i] ];

                    if ( methodMap.find( header.getMethod() ) != methodMap.end() )
                    {
                        if ( methodMap[ header.getMethod() ] )
                        {
                            if ( !methodMap[ header.getMethod() ]( httpClientCtxPtr, rsp, urlPathPartArr, i ) )
                                break;
                        }
                    }
                    else if (  methodMap.find("*") != methodMap.end() )
                    {
                        if ( methodMap["*"] )
                        {
                            if ( !methodMap["*"]( httpClientCtxPtr, rsp, urlPathPartArr, i ) )
                                break;
                        }
                    }

                    if ( i == iEndUrlPath )
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
        if ( _router.find(urlPath) != _router.end() )
        {
            auto & methodMap = _router[urlPath];
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
            bool found = isFile; // 是否找到文件
            winux::String fullPath;
            if ( !isFile )
            {
                winux::FileTitle( urlPathPartArr[iEndUrlPath], &extName );
                if ( extName == "" ) // 不是文件，并且没有扩展名，接上documentIndex
                {
                    for ( auto const & indexFileName : this->config.documentIndex )
                    {
                        winux::FileTitle( indexFileName, &extName );
                        fullPath = winux::CombinePath( this->config.documentRoot + urlPath, indexFileName );
                        if ( winux::DetectPath(fullPath) )
                        {
                            found = true;
                            break;
                        }
                    }
                }
            }
            else // 是文件
            {
                fullPath = request.environVars["SCRIPT_FILENAME"];
            }

            if ( found )
            {
                if ( extName == "do" )
                {
                    // 执行do文件
                    int rc;
                    _app->execWebMain( fullPath, &rsp, _app->getRunParam(), &rc );
                }
                else
                {
                    // 静态文件
                    winux::Buffer data = winux::FileGetContentsEx( fullPath, false );
                    rsp.header["Content-Type"] = this->config.getMime(extName);
                    rsp.write(data);

                    //winux::ColorOutputLine( winux::fgFuchsia, "Static file=", fullPath, ", size:", data.getSize() );
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
    winux::StringArray urlPathArr;
    if ( path.length() > 0 && path[0] == '/' )
    {
        winux::StrSplit( path.substr(1), "/", &urlPathArr, false );
    }
    else
    {
        winux::StrSplit( path, "/", &urlPathArr, false );
    }

    urlPathArr.insert( urlPathArr.begin(), "" );

    // 记下每一个部分
    for ( size_t i = 0; i < urlPathArr.size(); i++ )
    {
        if ( i >= _crossRouter.size() ) _crossRouter.emplace_back();

        auto & subPathRouter = _crossRouter[i];

        if ( i == urlPathArr.size() - 1 )
        {
            subPathRouter[ urlPathArr[i] ][method] = handler;
        }
        else
        {
            subPathRouter[ urlPathArr[i] ];
        }
    }

}

void HttpServer::setRouteHandler( winux::String const & method, winux::String const & path, RouteHandlerFunction handler )
{
    _router[path][method] = handler;
}

}
