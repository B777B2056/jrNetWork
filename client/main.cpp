#include "src/rpc_client.hpp"

int main() {
    for(int i = 0; i < 100; ++i) {
        tinyRPC::client client("127.0.0.1", 8000);
        std::vector<int> vec{3,4,2,1,4,5,3,2};
        client.call<std::vector<int>>("int_sort", vec);
        std::cout << "LOOP " << i + 1 << std::endl;
    }
    return  0;
}
