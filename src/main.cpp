
#include <winux.hpp>
#include <eiennet.hpp>

using namespace winux;
using namespace eiennet;
using namespace std;

int main()
{
    SocketLib init;
    HttpServerConfig config( winux::Configure("server.conf") );
    HttpServer server(config);

    server.setHandler( "hello", [] (RESPONSE_HANDLER_PARAMS) {
        rspOut << "Hello world!";
    } );

    return server.run();
}
