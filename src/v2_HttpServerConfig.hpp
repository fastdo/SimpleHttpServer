#pragma once

namespace v2
{

struct HttpServerConfig
{
    winux::ConfigureSettings const & confSettings;

    /** \brief 服务器名，可留空 */
    winux::String serverName;
    /** \brief 服务器IP，可留空 */
    winux::String serverIp;
    /** \brief 服务器监听端口 */
    winux::ushort serverPort;
    /** \brief 监听积压数 */
    int listenBacklog;
    /** \brief 线程数 */
    int threadCount;
    /** \brief 服务器IO等待间隔时间(小数秒) */
    double serverWait;
    /** \brief verbose信息刷新间隔(小数秒) */
    double verboseInterval;
    /** \brief 显示冗长信息 */
    bool verbose;
    /** \brief 连接重试次数 */
    int retryCount;
    /** \brief 套接字超时时间(整数秒) */
    int sockTimeout;

    /** \brief 文档根目录 */
    winux::String documentRoot;
    /** \brief 文档首页 */
    winux::StringArray documentIndex;
    /** \brief 静态文件缓存生命期 */
    int cacheLifeTime;

    /** \brief 一些静态文件的MIME */
    std::map< winux::String, winux::String > mime; // MIME

    /** \brief 构造函数1，从配置对象加载参数 */
    HttpServerConfig( winux::ConfigureSettings const & settings );

    /** \brief 取得指定扩展名的MIME */
    winux::String getMime( winux::String const & extName ) const;

};


}
