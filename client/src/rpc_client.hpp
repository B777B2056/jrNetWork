#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>

extern "C" {
    #include <errno.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
}

namespace tinyRPC {
    class client {
    public:
        using json = nlohmann::json;
        int _serverfd;

    private:
        template<typename T>
        void _pack_helper(json&, T&&);

        template<typename T, typename... Args>
        void _pack_helper(json&, T&&, Args&&...);

        template<typename... Args>
        json _pack(const std::string& name, Args&&...);

        template<typename Ret>
        Ret _unpack(const json&);

    public:
        client(const std::string& ip, uint port);

        ~client();

        template<typename Ret, typename... Args>
        Ret call(const std::string&, Args&&...);
    };

    client::client(const std::string& ip, uint port) {
        // create client TCP socket
        _serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == _serverfd) {
            std::cout << strerror(errno) << std::endl;
            return;
        }
        // init struct
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        // find host ip by name through DNS service
        auto hpk = gethostbyname(ip.c_str());
        if(!hpk) {
            std::cout << strerror(errno) << std::endl;
            return;
        }
        // fill the struct
        addr.sin_family = AF_INET;		// protocol
        addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr *)(hpk->h_addr_list[0])));		// service IP
        addr.sin_port = htons(port);		// target process port number
        std::cout << "HOST IP is " << inet_ntoa(addr.sin_addr) << std::endl;
        // connect
        if(-1 == connect(_serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            std::cout << strerror(errno) << std::endl;
            return;
        }
        std::cout << "TCP connection creates successed." << std::endl;
    }

    client::~client() {
        close(_serverfd);
    }

    template<typename T>
    void client::_pack_helper(json& parameters, T&& para) {
        parameters.push_back(para);
    }

    template<typename T, typename... Args>
    void client::_pack_helper(json& parameters, T&& para, Args&&... args) {
        parameters.push_back(para);
        this->_pack_helper(parameters, args...);
    }

    template<typename... Args>
    client::json client::_pack(const std::string& name, Args&&... args) {
        client::json packet;
        packet["name"] = name;
        packet["parameters"] = client::json::array();
        this->_pack_helper(packet["parameters"], args...);
        return packet;
    }

    template<typename Ret>
    Ret client::_unpack(const client::json& ret_json) {
        if(ret_json.at("error_flag").get<bool>()) {
            throw std::runtime_error(ret_json.at("error_msg").get<std::string>());
        } else {
            return ret_json.at("return_value").get<Ret>();
        }
    }

    template<typename Ret, typename... Args>
    Ret client::call(const std::string& name, Args&&... args) {
        // Pack target method's infomation
        client::json packet = this->_pack(name, std::forward<Args>(args)...);
        // Send it to server
        std::string binary = packet.dump();
        std::cout << "Sended json format data: " << binary << std::endl;
        if(-1 == write(_serverfd, binary.c_str(), binary.length())) {
            std::cout << strerror(errno) << std::endl;
        }
        // Receive return value from server
        char buffer[8092];
        bzero(buffer, sizeof(buffer));
        if(-1 == read(_serverfd, buffer, 8092)) {
            std::cout << strerror(errno) << std::endl;
        }
        std::string str(buffer);
        std::cout << str << std::endl;
        // Unpack return value
        return this->_unpack<Ret>(client::json::parse(str));
    }
}

#endif // RPC_CLIENT_HPP
