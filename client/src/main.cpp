#include <iostream>
#include "HttpRpcClt.h"

int main() {
    jrRPC::RPCClient client("127.0.0.1", 8888);
    std::vector<int> vec{3,4,2,1,4,5,3,2};
    client.call<std::vector<int> >("intSort", vec);
    client.call<int>("noparamFunc");
    try
    {
        client.call<int>("notfoundTest");
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    std::future<std::vector<int> > res = client.asyncCall<std::vector<int> >("intSort", vec);
    res.wait();
    for (int n : res.get())
    {
        std::cout << n << " ";
    }
    std::cout << std::endl;
    return  0;
}
