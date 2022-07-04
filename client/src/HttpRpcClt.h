#pragma once

#include <string>
#include <cstdint>
#include "../network/Socket.h"
#include "../third/json.hpp"

namespace jrRPC 
{
    class RPCClient 
    {
    private:
        std::string _svrUrl;
        jrNetWork::TCP::Socket _socket;

    private:
        void _sendPostReq(const std::string& content);
        std::string _recvResponse();

        template<typename T>
        void _packHelper(nlohmann::json& parameters, T&& para)
        {
            parameters.push_back(para);
        }

        template<typename T, typename... Args>
        void _packHelper(nlohmann::json& parameters, T&& para, Args&&... args)
        {
            parameters.push_back(para);
            this->_packHelper(parameters, args...);
        }

        std::string _pack(const std::string& name);

        template<typename... Args>
        std::string _pack(const std::string& name, Args&&... args)
        {
            nlohmann::json pkg;
            pkg["name"] = name;
            pkg["parameters"] = nlohmann::json::array();
            this->_packHelper(pkg["parameters"], args...);
            return pkg.dump();
        }

        template<typename Ret>
        Ret _unpack(const nlohmann::json& ret)
        {
            if (ret.at("error").get<bool>()) 
            {
                throw std::runtime_error(ret.at("error_msg").get<std::string>());
            }
            else 
            {
                return ret.at("return_val").get<Ret>();
            }
        }

    public:
        RPCClient(const std::string& ip, std::uint16_t port);
        ~RPCClient();

        template<typename Ret>
        Ret call(const std::string& name)
        {
            // Pack target method's infomation, then send it to server as HTTP's body
            _sendPostReq(this->_pack(name));
            // Receive return value from server, then unpack return value
            return this->_unpack<Ret>(nlohmann::json::parse(_recvResponse()));
        }

        template<typename Ret, typename... Args>
        Ret call(const std::string& name, Args&&... args)
        {
            // Pack target method's infomation, then send it to server as HTTP's body
            _sendPostReq(this->_pack(name, std::forward<Args>(args)...));
            // Receive return value from server, then unpack return value
            return this->_unpack<Ret>(nlohmann::json::parse(_recvResponse()));
        }
    };
}
