
#include <winux.hpp>
#include <eiennet.hpp>

using namespace winux;
using namespace eiennet;
using namespace std;

int main()
{
    SocketLib init;
    HttpServer server( winux::Configure("server.conf") );

    server.setHandler( "hello", [] (RESPONSE_HANDLER_PARAMS) {
        rspOut << "Hello world!";
    } );

    return server.run();
}
