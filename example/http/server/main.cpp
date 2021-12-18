#include "webserver.hpp"

int main() {
    uint port = 8000;
    std::string logger_path = "logger/";
    std::string work_directory = "../../jrNetWork/example/http/server/source";
    uint max_task_num = 10000;
    jrHTTP::HTTPServer server(port, logger_path, work_directory, max_task_num);
    server.loop();
    return 0;
}

