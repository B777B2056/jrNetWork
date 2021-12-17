#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "../../../network/dispatch.hpp"
#include <string>
#include <iostream>
#include <exception>
#include <unordered_map>
#include <sys/wait.h>

namespace jrHTTP {
    class HTTPServer {
    private:
        using uint = unsigned int;
        using hash_map = std::unordered_map<std::string, std::string>;

    private:
        jrNetWork::EventDispatch dispatch;
        hash_map ret_head_table;
        const std::string HTTP_Version;
        const std::string file_mapping_path;
        static std::unordered_map<int, std::string> status_table;

    private:
        /* Parser http data by state machine, then send ret data */
        void http_handler(jrNetWork::TCP::Socket* client);
        /* Get static or dynamic resources */
        std::string get_resource(const std::string& url, int& ret_code);
        /* Post resources */
        std::string post_resource(const std::string& path, const std::string& body, int& ret_code);
        /* Execute CGI program */
        std::string exec_cgi(const std::string& path, const std::string& parameters, int& ret_code, std::string method);
        /* Parser http data into key-value pair by state-machine */
        void parser_request_line(jrNetWork::TCP::Socket* client, hash_map& request_line_table, int& ret_code);
        void parser_request_head(jrNetWork::TCP::Socket* client, hash_map& request_head_table, int& ret_code);
        void parser_request_body(jrNetWork::TCP::Socket* client, int content_length, std::string& request_body);

    public:
        /* Init network connection */
        HTTPServer(uint port, std::string path, std::string work_directory,
                   uint max_task_num, uint max_pool_size = std::thread::hardware_concurrency());
        /* Add return data line 's key-value pair */
        void add_ret_line(std::string key, std::string value);
        /* Start RPC server */
        void loop(uint timeout_period_sec = 300);
    };
}

#endif

