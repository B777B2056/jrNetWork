#include <iostream>
#include "HttpRpcClt.h"

int main() {
    jrRPC::RPCClient client("127.0.0.1", 8888);
    std::vector<int> vec{3,4,2,1,4,5,3,2};
    client.call<std::vector<int> >("intSort", vec);
    return  0;
}
