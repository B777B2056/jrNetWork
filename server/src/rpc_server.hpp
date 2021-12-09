#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <string>
#include <memory>
#include <exception>
#include "log.hpp"
#include "timer.hpp"
#include "network.hpp"
#include "thread_pool.hpp"
#include <nlohmann/json.hpp>

extern "C" {
    #include <fcntl.h>
    #include <unistd.h>
    #include <signal.h>
    #include <errno.h>
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <sys/epoll.h>
}

namespace jrRPC {
    using json = nlohmann::json;
    using uint = unsigned int;

    class server {
    private:
        using function_json = std::function<json(const json&)>;
        /* Max client num */
        uint max_client_num;
        /* TCP Server API */
        jrNetWork::TCP_server tcps;
        /* Epoll events array */
        epoll_event* events;
        /* Timer heap */
        timer_container tc;
        /* Function list */
        std::map<std::string, function_json> func_list;
        /* Thread pool */
        jrThreadPool::thread_pool pool;
        /* File descriptors for unified event source */
        static int uesfd[2];
        /* Logger */
        static jrRPC::logger log;

    private:
        /* Init network connection */
        void init(uint, uint);
        /* Unified event source handler */
        static void ues_handler(int);
        /* Thread pool + Dispatch = Reactor */
        bool dispatch(uint);
        /* Create a function_json from a original function */
        template<typename Ret, typename ... Args, std::size_t... N>
        function_json regist_procedure_helper(std::function<Ret(Args...)>, std::index_sequence<N...>);
        /* Receive string */
        std::string receive(int);
        /* Serialization and build a return packet */
        std::string serialization(const std::string&, json);
        /* Deserialization a received string */
        std::pair<std::string, json> deserialization(const std::string&);
        /* Stub */
        void stub(int);

    public:
        server(uint port, uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());

        ~server();

        template<typename Ret, typename ... Args>
        void regist_procedure(const std::string&, std::function<Ret(Args...)>);

        void run(uint);

    public:
        /* Not allowed Operation */
        server(const server&) = delete;
        server(server&&) = delete;
        server& operator=(const server&) = delete;
        server& operator=(server&&) = delete;
    };

    /* ========================= Static variable init ========================= */

    int jrRPC::server::uesfd[2];
    jrRPC::logger jrRPC::server::log;

    /* ========================= Server init and close ========================= */

    server::server(uint port, uint max_task_num, uint max_pool_size)
        : max_client_num(max_task_num),
          tcps(),
          events(new epoll_event[max_task_num]),
          tc(),
          pool(max_pool_size, max_task_num) {
        this->init(port, max_task_num);
    }

    server::~server() {
        delete[] events;
        close(uesfd[0]);
        close(uesfd[1]);
    }

    /* ========================= Network ========================= */

    void server::init(uint port, uint max_task_num) {
        switch(tcps.init(port, max_task_num)) {
            case jrNetWork::ESOCKET:
                LOG_FATAL(log, std::string("Socket error: ") + strerror(errno));
                exit(1);
                break;
            case jrNetWork::EBIND:
                LOG_FATAL(log, std::string("Bind error: ") + strerror(errno));
                exit(1);
                break;
            case jrNetWork::ELISTEN:
                LOG_FATAL(log, std::string("Listen error: ") + strerror(errno));
                exit(1);
                break;
            case jrNetWork::EIO_INIT:
                LOG_FATAL(log, std::string("Epoll init error: ") + strerror(errno));
                exit(1);
                break;
            default:
                break;
        }
        // Unified event source set
        if(-1 == pipe(uesfd)) {
            LOG_FATAL(log, std::string("Pipe error: ") + strerror(errno));
            exit(1);
        }
        // Set ues fd write non-blocking
        int flag = fcntl(uesfd[1], F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(uesfd[1], F_SETFL, flag);
        // Alarm-time bind
        signal(SIGALRM, ues_handler);
        // SIG sig bind
        signal(SIGINT, ues_handler);
        signal(SIGTERM, ues_handler);
        signal(SIGPIPE, ues_handler);
        // Register server socket
        if(tcps.regist_event(tcps.get_serverfd()) == jrNetWork::EREGIST) {
            LOG_FATAL(log, std::string("Epoll server fd error: ") + strerror(errno));
            exit(1);
        }
        if(tcps.regist_event(uesfd[0]) == jrNetWork::EREGIST) {
            LOG_FATAL(log, std::string("Epoll System signal fd error: ") + strerror(errno));
            exit(1);
        }
        LOG_NOTICE(log, "Server started");
    }

    void server::ues_handler(int sig) {
        int s = sig;
        write(uesfd[1], reinterpret_cast<char*>(&s), 1);
    }

    bool server::dispatch(uint timeout_period_sec) {
        bool have_timeout = false;
        int event_cnt = epoll_wait(tcps.get_epollfd(), events, max_client_num, -1);
        if(-1 == event_cnt) {
            if(errno != EINTR)
                LOG_WARNING(log, std::string("Epoll signal fd error: ") + strerror(errno));
            return true;
        }
        for(int i = 0; i < event_cnt; ++i) {
            int curfd = events[i].data.fd;
            // Connection event
            if(curfd == tcps.get_serverfd()) {
                // Accept connection
                int client = accept(tcps.get_serverfd(), nullptr, nullptr);
                if(-1 == client) {
                    // Ignore "Interrupted system call", reason see README.md
                    if(errno != EINTR)
                        LOG_WARNING(log, std::string("Accept connection error: ") + strerror(errno));
                    continue;
                }
                // Register client into epoll
                if(tcps.regist_event(client) == jrNetWork::EREGIST) {
                    LOG_WARNING(log, std::string("Client epoll error, client fd: ") + strerror(errno));
                    continue;
                }
                // Timer init
                tc.add_timer(timer(client, tcps.get_epollfd(), timeout_period_sec,
                                   [](int client, int epfd)->void
                                    {
                                        epoll_ctl(epfd, EPOLL_CTL_DEL, client, nullptr);
                                        close(client);
                                    }
                                  )
                             );
                LOG_NOTICE(log, "Connection accepted, client ip: " + log.get_ip_from_fd(client));
            } else if(events[i].events & EPOLLIN)  {
                if(curfd == uesfd[0]) {
                    // Handle signals
                    char sig[32];
                    bzero(sig, sizeof(sig));
                    int num = read(uesfd[0], sig, sizeof(sig));
                    if(num <= 0)
                        continue;
                    for(auto j = 0; j < num; ++j) {
                        switch(sig[j]) {
                            case SIGALRM:
                                have_timeout = true;    // timeout task
                                LOG_WARNING(log, "Client connection timeout, client ip: " + log.get_ip_from_fd(curfd));
                                break;
                            case SIGTERM:
                            case SIGINT:    // Stop server
                                return false;
                                LOG_NOTICE(log, "Server interrupt by system signal");
                                break;
                            case SIGPIPE:
                                break;
                        }
                    }
                } else {
                    // Add task into thread pool
                    if(!pool.add_task(&jrRPC::server::stub, this, curfd)) {
                        LOG_WARNING(log, "thread pool is full");
                    }
                }
            }
            if(have_timeout) {
                // Handle timeout task
                tc.tick();
                alarm(timeout_period_sec);
            }
        }
        return true;
    }

    void server::run(uint timeout_period_sec = 300) {
        alarm(timeout_period_sec);
        while(this->dispatch(timeout_period_sec));
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
    std::string server::receive(int client) {
        char buffer;
        int recv_size;
        std::string str;
        while(true) {
            recv_size = recv(client, &buffer, 1, 0);
            if(buffer == '#')
                break;
            if(-1 == recv_size) {
                // Ignore "Interrupted system call", reason see README.md
                if(errno != EINTR)
                    LOG_WARNING(log, std::string("Read error: ") + strerror(errno));
                break;
            } else if(0 == recv_size) {
                break;
            } else {
                str += buffer;
            }
        }
        return str;
    }

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

    void server::stub(int client) {
        std::string recv_str = this->receive(client);
        if(0 == recv_str.length()) {
            return;
        }
        LOG_NOTICE(log, "Recv str: " + recv_str);
        // Server stub deserialization
        try {
            auto packet = this->deserialization(recv_str);
            std::string ret = this->serialization(packet.first, packet.second) + "#";
            // Send it to client
            if(-1 == send(client, ret.c_str(), ret.length(), 0)) {
                  // Ignore "Interrupted system call", reason see README.md
                  if(errno != EINTR)
                    LOG_WARNING(log, std::string("Send error: ") + strerror(errno));
            } else {
                LOG_NOTICE(log, std::string("Send str: ") + ret);
            }
        } catch (const std::exception& e) {
            LOG_WARNING(log, std::string("JSON parser error: ") + e.what());
        }
    }
}

#endif // RPC_SERVER_H
