
#include "v2_base.hpp"
#include "v2_HttpServerConfig.hpp"

namespace v2
{

HttpServerConfig::HttpServerConfig( winux::ConfigureSettings const & settings ) : confSettings(settings)
{
    this->serverName = settings["server"].get<winux::String>( "server_name", "" );
    this->serverIp = settings["server"].get<winux::String>( "server_ip", "" );
    this->serverPort = settings["server"].get<winux::ushort>( "server_port", 18080 );
    this->serverWait = settings["server"].get<double>( "server_wait", 0.02 );
    this->listenBacklog = settings["server"].get<int>( "listen_backlog", 10 );
    this->threadCount =  settings["server"].get<int>( "thread_count", 4 );
    this->retryCount =  settings["server"].get<int>( "retry_count", 10 );
    this->sockTimeout = settings["server"].get<int>( "sock_timeout", 300 );
    this->verboseInterval = settings["server"].get<double>( "verbose_interval", 1.0 );
    winux::Mixed::ParseBool( settings["server"].get<winux::String>( "verbose", "true" ), &this->verbose );

    this->documentRoot = settings["site"].get<winux::String>( "document_root", "wwwroot" );
    settings["site"].get("document_index", winux::Mixed().createArray() ).getArray(&this->documentIndex);
    this->cacheLifeTime = settings["site"].get<int>( "cache_lifetime", 86400 );

    this->mime = {
        { "html", "text/html" },
        { "css", "text/css" },
        { "js", "text/javascript" },
        { "txt", "text/plain" },
        { "jpg", "image/jpeg" },
        { "png", "image/png" },
        { "gif", "image/gif" },
        { "ico", "image/x-icon" }
    };
}

winux::String HttpServerConfig::getMime( winux::String const & extName ) const
{
    return winux::isset( this->mime, extName ) ? this->mime.at(extName) : "application/octet-stream";
}

}
