#include "src/rpc_client.hpp"

int main() {
    tinyRPC::client client("127.0.0.1", 8000);
    std::vector<int> vec{3,4,2,1,4,5,3,2};
    std::vector<int>sorted_vec =  client.call<std::vector<int>>("int_sort", vec);
    for(int e : sorted_vec)
        std::cout << e << " ";
    std::cout << std::endl;
    return  0;
}
