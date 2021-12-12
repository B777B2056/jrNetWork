#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include "dispatch.hpp"
#include <string>
#include <memory>
#include <exception>
#include <nlohmann/json.hpp>

namespace jrRPC {
    using json = nlohmann::json;
    using uint = unsigned int;

    class RPCServer {
    private:
        using function_json = std::function<json(const json&)>;
        /* TCP Server API */
        jrNetWork::EventDispatch dispatch;
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
        void stub(jrNetWork::TCPSocket* client);

    public:
        /* Init network connection */
        RPCServer(uint port, uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());
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

    RPCServer::RPCServer(uint port, uint max_task_num, uint max_pool_size)
        : dispatch(port, max_task_num, max_pool_size) {
        /* Regist event handler */
        dispatch.set_event_handler(&jrRPC::RPCServer::stub, this);
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

    void RPCServer::stub(jrNetWork::TCPSocket* client) {
        std::string recv_str;
        std::string data = client->recv(1);
        while(data!="" && data!="#") {
            recv_str.append(data);
            data = client->recv(1);
        }
        if(!recv_str.empty()) {
//            LOG_NOTICE(log, "Recv str: " + recv_str);
            // Server stub deserialization
            try {
                auto packet = this->deserialization(recv_str);
                std::string ret = this->serialization(packet.first, packet.second) + "#";
                // Send it to client
                if(!client->send(ret)) {
                      // Ignore "Interrupted system call", reason see README.md
//                      if(errno != EINTR)
//                        LOG_WARNING(log, std::string("Send error: ") + strerror(errno));
                } else {
//                    LOG_NOTICE(log, std::string("Send str: ") + ret);
                }
            } catch (const std::exception& e) {
//                LOG_WARNING(log, std::string("JSON parser error: ") + e.what());
            }
        }
    }
}

#endif // RPC_SERVER_H
