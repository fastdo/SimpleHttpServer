
#include "v2_base.hpp"
#include "v2_HttpServerConfig.hpp"
#include "v2_HttpServer.hpp"
#include "v2_HttpApp.hpp"
#include "v2_HttpRequestCtx.hpp"

namespace v2
{

HttpRequestCtx::HttpRequestCtx( HttpApp * app, winux::uint64 clientId, winux::String const & clientEpStr, winux::SharedPointer<eiennet::ip::tcp::Socket> clientSockPtr ) :
    ClientCtx(clientId, clientEpStr, clientSockPtr),
    Request(app),
    url(http::Url::urlSimple),
    curRecvType(drtRequestHeader),
    hasHeader(false),
    requestContentLength(0L)
{

}

HttpRequestCtx::~HttpRequestCtx()
{
    if ( static_cast<HttpApp*>(this->app)->_server.config.verbose )
    {
        winux::ColorOutputLine( winux::fgBlue, this->getStamp(), "析构" );
    }
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

// 根据文档根目录内实际文件，拆解URL路径部分字符串为urlPath和requestPathInfo
static void __ProcessUrlPath(
    winux::String const & urlRawPathStr,
    winux::String const & documentRootDir,
    winux::StringArray * urlPathPartArr,
    size_t * iEndUrlPath,
    winux::String * urlPath,
    winux::String * requestPathInfo,
    winux::String * extName,
    bool * isExist,
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
                *isExist = false;
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

bool HttpRequestCtx::processData( void * data )
{
    // 解析URL信息
    this->url.clear();
    this->url.parse( this->header.getUrl(), false, true, true, true, true );

    // 服务器配置对象
    HttpServerConfig & serverConfig = static_cast<HttpApp *>(this->app)->getServer().config;

    // 应该处理GET/POST/COOKIES/ENVIRON
    // 清空原先数据
    this->environVars.clear();
    this->cookies.clear();
    this->get.clear();
    this->post.clear();

    // 拆解路径部分信息（如果路径信息包含具体文件，之后的信息会当作PATH_INFO，否则全为urlPath）
    winux::String const urlRawPathStr = this->url.getPath();

    PerRequestData & reqData = *reinterpret_cast<PerRequestData *>(data);
    winux::String requestPathInfo; // PATH_INFO
    __ProcessUrlPath( urlRawPathStr, serverConfig.documentRoot, &reqData.urlPathPartArr, &reqData.iEndUrlPath, &reqData.urlPath, &requestPathInfo, &reqData.extName, &reqData.isExist, &reqData.isFile );

    // 注册一些环境变量
    this->environVars["ORIG_PATH_INFO"] = "/" + urlRawPathStr;
    this->environVars["SERVER_SOFTWARE"] = "SimpleHttpServer/1.0";
    this->environVars["FASTDO_VERSION"] = "0.6.1";
    this->environVars["REQUEST_URI"] = this->header.getUrl();
    if ( this->header.hasHeader("Referer") ) this->environVars["HTTP_REFERER"] = this->header["Referer"];
    this->environVars["DOCUMENT_ROOT"] = serverConfig.documentRoot;
    this->environVars["SCRIPT_FILENAME"] = serverConfig.documentRoot + reqData.urlPath;
    //this->environVars["URL_PATH"] = 
    this->environVars["SCRIPT_NAME"] = reqData.urlPath;
    this->environVars["PATH_INFO"] = requestPathInfo;

    // 解析cookies
    this->cookies.loadCookies( this->header.getHeader("Cookie") );
    // 解析get querystring
    this->get.parse( this->url.getRawQueryStr() );
    // 解析post
    http::Header::ContentType ct;
    // 取得请求体类型
    if ( this->header.get( "Content-Type", &ct ) )
    {
        winux::String contentMimeType = ct.getMimeType();
        if ( contentMimeType == "application/x-www-form-urlencoded" )
        {
            this->post.parse(this->body);
        }
        else if ( contentMimeType == "multipart/form-data" )
        {
            __ProcessMultipartFormData( this->body.c_str(), (winux::ulong)this->body.size(), ct.getBoundary(), &this->post );
        }
    }

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
    return true;
}

}