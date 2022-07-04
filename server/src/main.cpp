#include "Webserver.h"
#include "Procedures.h"


int main() 
{
    jrHTTP::HTTPServer server(8888);
    return server.run();
}

