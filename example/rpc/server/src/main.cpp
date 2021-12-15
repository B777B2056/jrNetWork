#include "rpc_server.hpp"
#include "../registed_fun/sort/sort.hpp"

int main() {
    jrRPC::RPCServer server(8000, "logger/", 1000);
    server.regist_procedure("int_sort", std::function<std::vector<int>(std::vector<int>)>(int_sort));
    server.loop();
    return 0;
}
