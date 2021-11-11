#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <string>
#include <memory>
#include <exception>
#include "log.hpp"
#include "timer.hpp"
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

namespace tinyRPC {
    using json = nlohmann::json;

    static int _uesfd[2]; // file descriptors for unified event source

    static tinyRPC::logger _logger;    /* Logger */

    void _ues_handler(int);

    void _timeout_default_handler(int, int);

    std::string _get_ip_from_fd(int);

    class server {
    private:
        using function_json = std::function<json(const json&)>;
        /* Max client num */
        uint _max_client_num;
        /* Server socket file description */
        int _serverfd;
        /* Epoll file description */
        int _epfd;
        /* Epoll event */
        epoll_event _ee;
        /* Function list */
        std::map<std::string, function_json> _func_list;
        /* Thread pool */
        tinyRPC::thread_pool _pool;
        /* Call method */
        class invoker : public tinyRPC::task_base {
        private:
            int _client;
            /* Server */
            tinyRPC::server* _server;

        private:
            /* Receive string */
            std::string _receive();
            /* Serialization and build a return packet */
            std::string _serialization(const std::string&, json);
            /* Deserialization a received string */
            std::pair<std::string, json> _deserialization(const std::string&);

        public:
            invoker(int, tinyRPC::server*);
            virtual void start() override;
        };

        friend class invoker;

    private:
        /* Init network connection */
        void _init(uint, uint);
        /* Thread pool + Dispatch = Reactor */
        void _dispatch(uint, timer_container&, bool&, bool&, timer*, epoll_event*);
        /* Create a function_json from a original function */
        template<typename Ret, typename ... Args, std::size_t... N>
        function_json register_procedure_helper(std::function<Ret(Args...)>, std::index_sequence<N...>);

    public:
        server(uint port, uint max_pool_size = std::thread::hardware_concurrency(), uint max_task_num = 100);

        ~server();

        template<typename Ret, typename ... Args>
        void register_procedure(const std::string&, std::function<Ret(Args...)>);

        void run(uint);

    public:
        /* Not allowed Operation */
        server(const server&) = delete;
        server(server&&) = delete;
        server& operator=(const server&) = delete;
        server& operator=(server&&) = delete;
    };

    /* ===================== Implemention ====================*/
    void _ues_handler(int sig) {
        int s = sig;
        write(tinyRPC::_uesfd[1], reinterpret_cast<char*>(&s), 1);
    }

    void _timeout_default_handler(int epfd, int client) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client, nullptr);
        close(client);
    }

    std::string _get_ip_from_fd(int fd) {
        sockaddr_in addr;
        socklen_t addr_size = sizeof(sockaddr_in);
        int res = getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &addr_size);
        return res > 0 ? inet_ntoa(addr.sin_addr) : "";
    }

    server::invoker::invoker(int client, tinyRPC::server* s)
        : _client(client), _server(s) {

    }

    // Receive binary data
    std::string server::invoker::_receive() {
        char buffer;
        int recv_size;
        std::string str;
        while(true) {
            recv_size = recv(_client, &buffer, 1, 0);
            if(buffer == '#')
                break;
            if(-1 == recv_size) {
                // Ignore "Interrupted system call", reason see README.md
                if(errno != EINTR)
                    LOG_WARNING(_logger, std::string("Read error: ") + strerror(errno));
                break;
            } else if(0 == recv_size) {
                break;
            } else {
                str += buffer;
            }
        }
        return str;
    }

    std::string server::invoker::_serialization(const std::string& fun_name, json parameters) {
        // Invoke target method
        json ret_msg = json::object();
        if(_server->_func_list.find(fun_name) == _server->_func_list.end()) {
            // NOT FOUND TARGET FUNCTION
            ret_msg["error_flag"] = true;
            ret_msg["error_msg"] = "Target method NOT found";
        } else {
            try {
                // Call target method, and serialize target method's return value
                ret_msg["error_flag"] = false;
                ret_msg["return_value"] = _server->_func_list[fun_name](parameters);
            } catch (const std::exception& e) {
                ret_msg["error_flag"] = true;
                ret_msg["error_msg"] = e.what();
            }
        }
        return ret_msg.dump();
    }

    std::pair<std::string, json> server::invoker::_deserialization(const std::string& recv_str) {
        json json_str = json::parse(recv_str);
        return std::make_pair(json_str.at("name").get<std::string>(), json_str.at("parameters"));
    }

    void server::invoker::start() {
        std::string recv_str = this->_receive();
        if(0 == recv_str.length()) {
            return;
        }
        LOG_NOTICE(_logger, "Recv str: " + recv_str);
        // Server stub deserialization
        try {
            auto packet = this->_deserialization(recv_str);
            std::string ret = this->_serialization(packet.first, packet.second) + "#";
            // Send it to client
            if(-1 == send(_client, ret.c_str(), ret.length(), 0)) {
                  // Ignore "Interrupted system call", reason see README.md
                  if(errno != EINTR)
                    LOG_WARNING(_logger, std::string("Send error: ") + strerror(errno));
            } else {
                LOG_NOTICE(_logger, std::string("Send str: ") + ret);
            }
        } catch (const std::exception& e) {
            LOG_WARNING(_logger, std::string("JSON parser error: ") + e.what());
        }
    }

    server::server(uint port, uint max_pool_size, uint max_task_num)
        : _max_client_num(max_task_num),
          _pool(max_pool_size, max_task_num) {
        this->_init(port, max_task_num);
    }

    server::~server() {
        close(tinyRPC::_uesfd[0]);
        close(tinyRPC::_uesfd[1]);
        close(_epfd);
    }

    void server::_init(uint port, uint max_task_num) {
        // Create server socket file description
        _serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if(-1 == _serverfd) {
            LOG_FATAL(_logger, std::string("Socket error: ") + strerror(errno));
            exit(1);
        }
        // Bind ip address and port
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == bind(_serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(struct sockaddr_in))) {
            LOG_FATAL(_logger, std::string("Bind error: ") + strerror(errno));
            exit(1);
        }
        // Listen target port
        if(-1 == listen(_serverfd, max_task_num)) {
            LOG_FATAL(_logger, std::string("Listen error: ") + strerror(errno));
            exit(1);
        }
        // Unified event source set
        if(-1 == pipe(tinyRPC::_uesfd)) {
            LOG_FATAL(_logger, std::string("Pipe error: ") + strerror(errno));
            exit(1);
        }
        // Set ues fd write non-blocking
        int flag = fcntl(tinyRPC::_uesfd[1], F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(tinyRPC::_uesfd[1], F_SETFL, flag);
        // Alarm-time bind
        signal(SIGALRM, _ues_handler);
        // SIG sig bind
        signal(SIGINT, _ues_handler);
        signal(SIGTERM, _ues_handler);
        signal(SIGPIPE, _ues_handler);
        // Epoll init
        _epfd = epoll_create(max_task_num);
        // Register server socket
        _ee.events = EPOLLIN;
        _ee.data.fd = _serverfd;
        if(-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, _serverfd, &_ee)) {
            LOG_FATAL(_logger, std::string("Epoll server fd error: ") + strerror(errno));
            exit(1);
        }
        _ee.events = EPOLLIN;
        _ee.data.fd  = tinyRPC::_uesfd[0];
        // Register ues read pipe
        if(-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, tinyRPC::_uesfd[0], &_ee)) {
            LOG_FATAL(_logger, std::string("Epoll signal fd error: ") + strerror(errno));
            exit(1);
        }
        LOG_NOTICE(_logger, "Server started");
    }

    void server::_dispatch(uint timeout_period_sec, timer_container& tc, bool& is_stop,
                           bool& have_timeout, timer* timers, epoll_event* events) {
        int event_cnt = epoll_wait(_epfd, events, _max_client_num, -1);
        if(-1 == event_cnt) {
            if(errno != EINTR)
                LOG_WARNING(_logger, std::string("Epoll signal fd error: ") + strerror(errno));
            return;
        }
        for(int i = 0; i < event_cnt; ++i) {
            int curfd = events[i].data.fd;
            // Connection event
            if(curfd == _serverfd) {
                // Accept connection
                int client = accept(_serverfd, nullptr, nullptr);
                if(-1 == client) {
                    // Ignore "Interrupted system call", reason see README.md
                    if(errno != EINTR)
                        LOG_WARNING(_logger, std::string("Accept connection error: ") + strerror(errno));
                    continue;
                }
                // Register client into epoll
                _ee.events = EPOLLIN;
                _ee.data.fd = client;
                if(-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, client, &_ee)) {
                    LOG_WARNING(_logger, std::string("Client epoll error, client fd: ") + strerror(errno));
                    continue;
                }
                // Timer init
                timer t;
                t.fd = client;
                t.epfd = _epfd;
                t.run_time = tinyRPC::set_timeout(timeout_period_sec);
                t.timeout_handler = _timeout_default_handler;
                // Add new timer
                tc.add_timer(t);
                // Bind timer and fd
                timers[client] = t;
                LOG_NOTICE(_logger, "Connection accepted, client ip: " + tinyRPC::_get_ip_from_fd(client));
            } else if(events[i].events & EPOLLIN)  {
                if(curfd == tinyRPC::_uesfd[0]) {
                    // Handle signals
                    char sig[32];
                    bzero(sig, sizeof(sig));
                    int num = read(tinyRPC::_uesfd[0], sig, sizeof(sig));
                    if(num <= 0)
                        continue;
                    for(auto j = 0; j < num; ++j) {
                        switch(sig[j]) {
                            case SIGALRM:
                                have_timeout = true;    // timeout task
                                LOG_WARNING(_logger, "Client connection timeout, client ip: " + tinyRPC::_get_ip_from_fd(curfd));
                                break;
                            case SIGTERM:
                            case SIGINT:    // Stop server
                                is_stop = true;
                                LOG_NOTICE(_logger, "Server interrupt by system signal");
                                break;
                            case SIGPIPE:
                                break;
                        }
                    }
                } else {
                    // Add task into thread pool
                    if(!_pool.add_task(std::make_unique<server::invoker>(curfd, this))) {
                        LOG_WARNING(_logger, "thread pool is full");
                    }
                }
            }
            if(have_timeout) {
                // Handle timeout task
                tc.tick();
                alarm(timeout_period_sec);
                have_timeout = false;
            }
        }
    }

    template<typename Ret, typename ... Args, std::size_t... N>
    server::function_json server::register_procedure_helper(std::function<Ret(Args...)> fun, std::index_sequence<N...>) {
        server::function_json f = [fun](const json& parameters)->json {
                                        return fun(parameters[N].get<typename std::decay<Args>::type>()...);
                                  };
        return f;
    }

    template<typename Ret, typename ... Args>
    void server::register_procedure(const std::string& name, std::function<Ret(Args...)> fun) {
        this->_func_list[name] = this->register_procedure_helper(fun, std::index_sequence_for<Args...>{});
    }

    void server::run(uint timeout_period_sec = 300) {
        timer_container tc;
        bool is_stop = false;
        bool have_timeout = false;
        timer* timers = new timer[_max_client_num];    // Bind active fd and it's timer
        epoll_event* events = new epoll_event[_max_client_num];
        alarm(timeout_period_sec);
        while(true) {
            this->_dispatch(timeout_period_sec, tc, is_stop, have_timeout, timers, events);
            if(is_stop)
                break;
        }
        delete[] timers;
        delete[] events;
    }
}

#endif // RPC_SERVER_H
