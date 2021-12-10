#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include "log.hpp"
#include "network.hpp"
#include <string>
#include <memory>
#include <exception>
#include <nlohmann/json.hpp>

extern "C" {
    #include <errno.h>
}

namespace jrRPC {
    using json = nlohmann::json;
    using uint = unsigned int;

    class server {
    private:
        using function_json = std::function<json(const json&)>;
        /* TCP Server API */
        jrNetWork::TCP_server tcps;
        /* Function list */
        std::map<std::string, function_json> func_list;
        /* Logger */
        static jrRPC::logger log;

    private:
        /* Create a function_json from a original function */
        template<typename Ret, typename ... Args, std::size_t... N>
        function_json regist_procedure_helper(std::function<Ret(Args...)>, std::index_sequence<N...>);
        /* Receive string */
//        std::string receive(int);
        /* Serialization and build a return packet */
        std::string serialization(const std::string&, json);
        /* Deserialization a received string */
        std::pair<std::string, json> deserialization(const std::string&);
        /* Stub */
        std::string stub(const std::string&);

    public:
        /* Init network connection */
        server(uint port, uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());
        /* Regist Procedure */
        template<typename Ret, typename ... Args>
        void regist_procedure(const std::string&, std::function<Ret(Args...)>);
        /* Start RPC server */
        void run(uint);

    public:
        /* Not allowed Operation */
        server(const server&) = delete;
        server(server&&) = delete;
        server& operator=(const server&) = delete;
        server& operator=(server&&) = delete;
    };

    /* ========================= Static variable init ========================= */

    jrRPC::logger jrRPC::server::log;

    /* ========================= Server init and close ========================= */

    server::server(uint port, uint max_task_num, uint max_pool_size)
        : tcps(max_task_num, max_pool_size) {
        switch(tcps.init(port)) {
            case jrNetWork::ESOCKET:
                LOG_FATAL(log, std::string("Socket error: ") + strerror(errno));
                exit(1);
            case jrNetWork::EBIND:
                LOG_FATAL(log, std::string("Bind error: ") + strerror(errno));
                exit(1);
            case jrNetWork::ELISTEN:
                LOG_FATAL(log, std::string("Listen error: ") + strerror(errno));
                exit(1);
            case jrNetWork::EIO_INIT:
                LOG_FATAL(log, std::string("Epoll init error: ") + strerror(errno));
                exit(1);
            case jrNetWork::EUES_PIPE:
                LOG_FATAL(log, std::string("PIPE error: ") + strerror(errno));
                exit(1);
            case jrNetWork::EREGIST:
                LOG_FATAL(log, std::string("UES regist event error: ") + strerror(errno));
                exit(1);
            default:
                break;
        }
        LOG_NOTICE(log, "Server started");
    }

    /* ========================= Network ========================= */

    void server::run(uint timeout_period_sec = 300) {
        tcps.event_loop(timeout_period_sec, &jrRPC::server::stub, this);
    }

    /* ========================= Regist procedure ========================= */

    template<typename Ret, typename ... Args, std::size_t... N>
    server::function_json server::regist_procedure_helper(std::function<Ret(Args...)> fun, std::index_sequence<N...>) {
        server::function_json f = [fun](const json& parameters)->json {
                                        return fun(parameters[N].get<typename std::decay<Args>::type>()...);
                                  };
        return f;
    }

    template<typename Ret, typename ... Args>
    void server::regist_procedure(const std::string& name, std::function<Ret(Args...)> fun) {
        this->func_list[name] = this->regist_procedure_helper(fun, std::index_sequence_for<Args...>{});
    }

    /* ========================= RPC Stub ========================= */

    // Receive binary data
//    std::string server::receive(int client) {
//        char buffer;
//        int recv_size;
//        std::string str;
//        while(true) {
//            recv_size = recv(client, &buffer, 1, 0);
//            if(buffer == '#')
//                break;
//            if(-1 == recv_size) {
//                // Ignore "Interrupted system call", reason see README.md
//                if(errno != EINTR)
//                    LOG_WARNING(log, std::string("Read error: ") + strerror(errno));
//                break;
//            } else if(0 == recv_size) {
//                break;
//            } else {
//                str += buffer;
//            }
//        }
//        return str;
//    }

    std::string server::serialization(const std::string& fun_name, json parameters) {
        // Invoke target method
        json ret_msg = json::object();
        if(this->func_list.find(fun_name) == this->func_list.end()) {
            // NOT FOUND TARGET FUNCTION
            ret_msg["error_flag"] = true;
            ret_msg["error_msg"] = "Target method NOT found";
        } else {
            try {
                // Call target method, and serialize target method's return value
                ret_msg["error_flag"] = false;
                ret_msg["return_value"] = this->func_list[fun_name](parameters);
            } catch (const std::exception& e) {
                ret_msg["error_flag"] = true;
                ret_msg["error_msg"] = e.what();
            }
        }
        return ret_msg.dump();
    }

    std::pair<std::string, json> server::deserialization(const std::string& recv_str) {
        json json_str = json::parse(recv_str);
        return std::make_pair(json_str.at("name").get<std::string>(), json_str.at("parameters"));
    }

    std::string server::stub(const std::string& recv_str) {
//        std::string recv_str = this->receive(client);
        if(0 == recv_str.length()) {
            return "";
        }
        LOG_NOTICE(log, "Recv str: " + recv_str);
        // Server stub deserialization
        try {
            auto packet = this->deserialization(recv_str);
            std::string ret = this->serialization(packet.first, packet.second) + "#";
            // Send it to client
            return ret;
//            if(-1 == send(client, ret.c_str(), ret.length(), 0)) {
//                  // Ignore "Interrupted system call", reason see README.md
//                  if(errno != EINTR)
//                    LOG_WARNING(log, std::string("Send error: ") + strerror(errno));
//            } else {
//                LOG_NOTICE(log, std::string("Send str: ") + ret);
//            }
        } catch (const std::exception& e) {
            LOG_WARNING(log, std::string("JSON parser error: ") + e.what());
            return "";
        }
    }
}

#endif // RPC_SERVER_H
