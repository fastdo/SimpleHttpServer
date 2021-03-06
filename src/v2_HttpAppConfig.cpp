
#include "v2_base.hpp"
#include "v2_HttpAppConfig.hpp"

namespace v2
{

HttpAppConfig::HttpAppConfig( winux::ConfigureSettings const & settings ) :
    confSettings(settings)
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
    settings["site"].get("error_pages", winux::Mixed().createCollection() ).getMap(&this->errorPages);
    this->cacheLifeTime = settings["site"].get<int>( "cache_lifetime", 86400 );

    this->initMime();
}

HttpAppConfig::HttpAppConfig(
    winux::ConfigureSettings const & settings,
    eiennet::ip::EndPoint const & ep,
    int threadCount,
    int backlog,
    double serverWait,
    double verboseInterval,
    bool verbose,
    int cacheLifeTime
) :
    confSettings(settings)
{
    this->serverName = settings["server"].get<winux::String>( "server_name", "" );
    this->serverIp = settings["server"].get<winux::String>( "server_ip", ep.getIp() );
    this->serverPort = settings["server"].get<winux::ushort>( "server_port", ep.getPort() );
    this->serverWait = settings["server"].get<double>( "server_wait", serverWait );
    this->listenBacklog = settings["server"].get<int>( "listen_backlog", backlog );
    this->threadCount =  settings["server"].get<int>( "thread_count", threadCount );
    this->retryCount =  settings["server"].get<int>( "retry_count", 10 );
    this->sockTimeout = settings["server"].get<int>( "sock_timeout", 300 );
    this->verboseInterval = settings["server"].get<double>( "verbose_interval", verboseInterval );
    winux::Mixed::ParseBool( settings["server"].get<winux::String>( "verbose", winux::Mixed(verbose).json() ), &this->verbose );

    this->documentRoot = settings["site"].get<winux::String>( "document_root", "wwwroot" );
    settings["site"].get("document_index", winux::Mixed().createArray() ).getArray(&this->documentIndex);
    this->cacheLifeTime = settings["site"].get<int>( "cache_lifetime", cacheLifeTime );

    this->initMime();
}

winux::String HttpAppConfig::getMime( winux::String const & extName ) const
{
    return winux::isset( this->mime, extName ) ? this->mime.at(extName) : "application/octet-stream";
}

void HttpAppConfig::initMime()
{
    this->mime = {
        { "html", "text/html" },
        { "css", "text/css" },
        { "js", "text/javascript" },
        { "txt", "text/plain" },
        { "jpg", "image/jpeg" },
        { "png", "image/png" },
        { "gif", "image/gif" },
        { "ico", "image/x-icon" },
        { "svg", "image/svg+xml" }
    };
}

}
