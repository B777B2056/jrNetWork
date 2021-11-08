#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include<string>
#include<memory>
#include<exception>
//#include"log.hpp"
#include"timer.hpp"
#include"thread_pool.hpp"
#include<nlohmann/json.hpp>

extern "C" {
    #include<fcntl.h>
    #include<unistd.h>
    #include<signal.h>
    #include<errno.h>
    #include<sys/time.h>
    #include<sys/socket.h>
    #include<arpa/inet.h>
    #include<sys/epoll.h>
}

namespace tinyRPC {
    int _uesfd[2]; // file descriptors for unified event source

    void _ues_handler(int sig);

    void disconnect(int epfd, int client);

    class invoker;

    class server {
    friend class invoker;

    private:
        using json = nlohmann::json;
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

    private:
        /* Init network connection */
        void _init(uint, uint);
        /* Thread pool + Dispatch = Reactor */
        void _dispatch(uint, timer_container&, bool&, bool&, timer*, epoll_event*);
        /* Create a function_json from a original function */
        template<typename Ret, typename ... Args, std::size_t... N>
        function_json register_procedure_helper(std::function<Ret(Args...)>, std::index_sequence<N...>);

    public:
        server(uint port, uint max_pool_size = 16, uint max_task_num = 100);

        ~server();

        template<typename Ret, typename ... Args>
        void register_procedure(const std::string&, std::function<Ret(Args...)>);

        void run(uint);
    };

    /* Call method */
    class invoker : public task_base {
    private:
        int clientfd;
        /* Server */
        tinyRPC::server* _server;

    public:
        invoker(int, tinyRPC::server*);
        virtual void start() override;
    };


    /* ===================== Implemention ====================*/
    void _ues_handler(int sig) {
        int s = sig;
        if(-1 == write(tinyRPC::_uesfd[1], reinterpret_cast<char*>(&s), 1)) {
//            std::cout << strerror(errno) << std::endl;
        }
    }

    void disconnect(int epfd, int client) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, client, nullptr);
        close(client);
    }

    invoker::invoker(int client, tinyRPC::server* s)
        : clientfd(client), _server(s) {

    }

    void invoker::start() {
        // Receive binary data
        char buffer[8092];
        bzero(buffer, sizeof(buffer));
        switch (recv(clientfd, buffer, 8092, 0)) {
            case -1:
//                std::cout << strerror(errno) << std::endl;
                return;
            case 0:
                return;
        }
        std::string str(buffer);
        std::cout << "Recv str: " << str << std::endl;
        // Server stub deserialization
        try {
            server::json json_str = server::json::parse(str);
            std::string fun_name = json_str.at("name").get<std::string>();
            server::json parameters = json_str.at("parameters");
            // Invoke target method
            server::json ret_msg;
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
            // Send it to client
            std::string ret_binary = ret_msg.dump();
            if(-1 == send(clientfd, ret_binary.c_str(), ret_binary.length(), 0)) {
    //            std::cout << strerror(errno) << std::endl;
            }
        } catch (...) {
            std::cout << "some error" << std::endl;
        }
    }

    server::server(uint port, uint max_pool_size, uint max_task_num)
        : _max_client_num(max_task_num), _pool(max_pool_size, max_task_num) {
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
//            std::cout << strerror(errno) << std::endl;
            return;
        }
        // Bind ip address and port
        sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if(-1 == bind(_serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(struct sockaddr_in))) {
//            std::cout << strerror(errno) << std::endl;
            return;
        }
        // Listen target port
        if(-1 == listen(_serverfd, max_task_num)) {
//            std::cout << strerror(errno) << std::endl;
            exit(1);
        }
        // Unified event source set
        if(-1 == pipe(tinyRPC::_uesfd)) {
//            std::cout << strerror(errno) << std::endl;
            return;
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
//            std::cout << strerror(errno) << std::endl;
            return;
        }
        _ee.events = EPOLLIN;
        _ee.data.fd  = tinyRPC::_uesfd[0];
        // Register ues read pipe
        if(-1 == epoll_ctl(_epfd, EPOLL_CTL_ADD, tinyRPC::_uesfd[0], &_ee)) {
//            std::cout << strerror(errno) << std::endl;
            return;
        }
    }

    void server::_dispatch(uint timeout_period_sec, timer_container& tc, bool& is_stop,
                           bool& have_timeout, timer* timers, epoll_event* events) {
        int event_cnt = epoll_wait(_epfd, events, _max_client_num, -1);
        if(-1 == event_cnt) {
//            std::cout << strerror(errno) << std::endl;
            return;
        }
        for(int i = 0; i < event_cnt; ++i) {
            int curfd = events[i].data.fd;
            // Connection event
            if(_serverfd == curfd) {
                // Accept connection
                int client = accept(_serverfd, nullptr, nullptr);
                std::cout << "Connection accepted" << std::endl;
                if(-1 == client) {
                    std::cout << strerror(errno) << std::endl;
                    return;
                }
                // Register client into epoll
                _ee.events = EPOLLIN;
                _ee.data.fd = client;
                epoll_ctl(_epfd, EPOLL_CTL_ADD, client, &_ee);
                std::cout << "Client register epoll" << std::endl;
                // Timer init
                timer t;
                t.fd = client;
                t.epfd = _epfd;
                t.run_time = tinyRPC::set_timeout(timeout_period_sec);
                t.timeout_handler = disconnect;
                // Add new timer
                tc.add_timer(t);
                // Bind timer and fd
                timers[client] = t;
                std::cout << "Connection timer init" << std::endl;
            }
            // Handle signals
            else if(tinyRPC::_uesfd[0] == curfd) {
                char sig[1024];
                auto num = read(tinyRPC::_uesfd[0], sig, sizeof(sig));
                if(num <= 0)
                    continue;
                for(auto j = 0; j < num; ++j) {
                    switch(sig[j]) {
                        case SIGALRM:
                            have_timeout = true;    // timeout task
                            break;
                        case SIGTERM:
                        case SIGINT:    // Stop server
                            is_stop = true;
                            break;
                        case SIGPIPE:
                            break;
                    }
                }
            }
            // Handle requests task
            else {
//                std::cout << "Pool add" << std::endl;
                if(!_pool.add_task(std::make_shared<invoker>(curfd, this))) {
//                    std::cout << "thread pool is full" << std::endl;
                }
            }
        }
        // Handle timeout task
        if(have_timeout) {
            tc.tick();
            alarm(timeout_period_sec);
            have_timeout = false;
        }
    }

    template<typename Ret, typename ... Args, std::size_t... N>
    server::function_json server::register_procedure_helper(std::function<Ret(Args...)> fun, std::index_sequence<N...>) {
        server::function_json f = [fun](const server::json& parameters)->server::json {
                                        return fun(parameters[N].get<typename std::decay<Args>::type>()...);
                                  };
        return f;
    }

    template<typename Ret, typename ... Args>
    void server::register_procedure(const std::string& name, std::function<Ret(Args...)> fun) {
        this->_func_list[name] = this->register_procedure_helper(fun, std::index_sequence_for<Args...>{});
    }

    void server::run(uint timeout_period_sec = 5) {
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
            if(have_timeout) {
                continue;
            }
        }
        delete[] timers;
        delete[] events;
    }
}

#endif // RPC_SERVER_H
