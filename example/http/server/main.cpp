#include "webserver.hpp"

int main() {
    uint port = 8000;
    std::string logger_path = "logger_web/";
    uint max_task_num = 10000;
    jrHTTP::HTTPServer server(port, logger_path, max_task_num);
    server.loop();
    return 0;
}

