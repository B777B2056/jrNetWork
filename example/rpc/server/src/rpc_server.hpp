#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include "../../../../network/dispatch.hpp"
#include <string>
#include <exception>
#include <nlohmann/json.hpp>

namespace jrRPC {
    using json = nlohmann::json;
    using uint = unsigned int;

    class RPCServer {
    private:
        using function_json = std::function<json(const json&)>;
        /* TCP Server API */
        jrNetWork::EventDispatch<jrNetWork::IO_Model_EPOLL> dispatch;
        /* Function list */
        std::map<std::string, function_json> func_list;

    private:
        /* Create a function_json from a original function */
        template<typename Ret, typename ... Args, std::size_t... N>
        function_json regist_procedure_helper(std::function<Ret(Args...)> f, std::index_sequence<N...>);
        /* Serialization and build a return packet */
        std::string serialization(const std::string& name, json parameters);
        /* Deserialization a received string */
        std::pair<std::string, json> deserialization(const std::string& json_str);
        /* Stub */
        void stub(std::shared_ptr<jrNetWork::TCP::ClientSocket> client);

    public:
        /* Init network connection */
        RPCServer(uint port, std::string path, uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());
        /* Regist Procedure */
        template<typename Ret, typename ... Args>
        void regist_procedure(const std::string& name, std::function<Ret(Args...)> f);
        /* Start RPC server */
        void loop(uint timeout_period_sec = 300);

    public:
        /* Not allowed Operation */
        RPCServer(const RPCServer&) = delete;
        RPCServer(RPCServer&&) = delete;
        RPCServer& operator=(const RPCServer&) = delete;
        RPCServer& operator=(RPCServer&&) = delete;
    };

    /* ========================= Server init and close ========================= */

    RPCServer::RPCServer(uint port, std::string path, uint max_task_num, uint max_pool_size)
        : dispatch(port, max_task_num, max_pool_size, path) {
        /* Regist event handler */
        dispatch.set_task_handler(&jrRPC::RPCServer::stub, this);
    }

    /* ========================= Network ========================= */

    void RPCServer::loop(uint timeout_period_sec) {
        dispatch.event_loop(timeout_period_sec);
    }

    /* ========================= Regist procedure ========================= */

    template<typename Ret, typename ... Args, std::size_t... N>
    RPCServer::function_json RPCServer::regist_procedure_helper(std::function<Ret(Args...)> f, std::index_sequence<N...>) {
        return [f](const json& parameters)->json
               {
                    return f(parameters[N].get<typename std::decay<Args>::type>()...);
               };
    }

    template<typename Ret, typename ... Args>
    void RPCServer::regist_procedure(const std::string& name, std::function<Ret(Args...)> f) {
        this->func_list[name] = this->regist_procedure_helper(f, std::index_sequence_for<Args...>{});
    }

    /* ========================= RPC Stub ========================= */

    std::string RPCServer::serialization(const std::string& name, json parameters) {
        // Invoke target method
        json ret_msg = json::object();
        if(this->func_list.find(name) == this->func_list.end()) {
            // NOT FOUND TARGET FUNCTION
            ret_msg["error_flag"] = true;
            ret_msg["error_msg"] = "Target method NOT found";
        } else {
            try {
                // Call target method, and serialize target method's return value
                ret_msg["error_flag"] = false;
                ret_msg["return_value"] = this->func_list[name](parameters);
            } catch (const std::exception& e) {
                ret_msg["error_flag"] = true;
                ret_msg["error_msg"] = e.what();
            }
        }
        return ret_msg.dump();
    }

    std::pair<std::string, json> RPCServer::deserialization(const std::string& json_str) {
        json json_data = json::parse(json_str);
        return std::make_pair(json_data.at("name").get<std::string>(), json_data.at("parameters"));
    }

    void RPCServer::stub(std::shared_ptr<jrNetWork::TCP::ClientSocket> client) {
        /* Protocol analysis */
        std::string recv_str;
        auto result = client->recv(1);
        std::string data = result.first;
        while(true) {
            if(!result.second) {
                client->disconnect();   // Error, close connection
                return ;
            }
            if(data.empty() || data=="#") {
                break;  // Stop flag
            }
            recv_str.append(data);
            result = client->recv(1);
            data = result.first;
        }
        if(!recv_str.empty()) {
            LOG(jrNetWork::Logger::Level::NOTICE, std::string("Recv data: ") + recv_str);
            /* Business logic */
            std::string ret;
            try {
                auto packet = deserialization(recv_str);
                ret = serialization(packet.first, packet.second) + "#";
            } catch (const std::exception& e) {
                LOG(jrNetWork::Logger::Level::WARNING, std::string("JSON parser error: ") + e.what());
                return ;
            }
            /* Send data */
            if(!client->send(ret)) {
                if(errno != EINTR) {
                    LOG(jrNetWork::Logger::Level::WARNING, std::string("Send data failed: ") + strerror(errno));
                }
            } else {
                LOG(jrNetWork::Logger::Level::NOTICE, std::string("Send data: ") + ret);
            }
        } else if(!result.second) {
            if(errno != EINTR) {
                LOG(jrNetWork::Logger::Level::WARNING, std::string("Receive data failed: ") + strerror(errno));
            }
        }
    }
}

#endif // RPC_SERVER_H
