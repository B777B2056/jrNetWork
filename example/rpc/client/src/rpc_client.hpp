#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP

#include "../../../../network/socket.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace jrRPC {
    class RPCClient {
    public:
        using json = nlohmann::json;
        jrNetWork::TCP::ClientSocket socket;

    private:
        template<typename T>
        void pack_helper(json&, T&&);

        template<typename T, typename... Args>
        void pack_helper(json&, T&&, Args&&...);

        template<typename... Args>
        json pack(const std::string& name, Args&&...);

        template<typename Ret>
        Ret unpack(const json&);

    public:
        RPCClient(const std::string& ip, uint port);

        ~RPCClient();

        template<typename Ret, typename... Args>
        Ret call(const std::string&, Args&&...);
    };

    RPCClient::RPCClient(const std::string& ip, uint port) : socket() {
        socket.connect(ip, port);
    }

    RPCClient::~RPCClient() {
        socket.disconnect();
    }

    template<typename T>
    void RPCClient::pack_helper(json& parameters, T&& para) {
        parameters.push_back(para);
    }

    template<typename T, typename... Args>
    void RPCClient::pack_helper(json& parameters, T&& para, Args&&... args) {
        parameters.push_back(para);
        pack_helper(parameters, args...);
    }

    template<typename... Args>
    RPCClient::json RPCClient::pack(const std::string& name, Args&&... args) {
        RPCClient::json packet;
        packet["name"] = name;
        packet["parameters"] = RPCClient::json::array();
        this->pack_helper(packet["parameters"], args...);
        return packet;
    }

    template<typename Ret>
    Ret RPCClient::unpack(const RPCClient::json& ret_json) {
        if(ret_json.at("error_flag").get<bool>()) {
            throw std::runtime_error(ret_json.at("error_msg").get<std::string>());
        } else {
            return ret_json.at("return_value").get<Ret>();
        }
    }

    template<typename Ret, typename... Args>
    Ret RPCClient::call(const std::string& name, Args&&... args) {
        // Pack target method's infomation
        RPCClient::json packet = pack(name, std::forward<Args>(args)...);
        // Send it to server
        socket.send(packet.dump() + "#");
        // Receive return value from server
        std::string str = "";
        std::pair<std::string, bool> result = socket.recv(1);
        std::string data = result.first;
        while(result.second && data!="" && data!="#") {
            str.append(data);
            result = socket.recv(1);
            data = result.first;
        }
        // Unpack return value
        return unpack<Ret>(RPCClient::json::parse(str));
    }
}

#endif // RPC_CLIENT_HPP
