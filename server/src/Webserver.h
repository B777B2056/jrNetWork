#pragma once

#include <string>
#include <unordered_map>
#include "../network/EventLoop.h"

namespace jrHTTP {
    class HTTPServer {
    private:
        using HashMap = std::unordered_map<std::string, std::string>;

    private:
        jrNetWork::EventLoop<jrNetWork::TCP::Socket> _dispatcher;
        HashMap _retHeadTbl;
        const std::string _fileMappingPath;

    private:
        /* Parser http data by state machine, then send ret data */
        void _handleHttpMsg(std::shared_ptr<jrNetWork::TCP::Socket> client);
        /* Get static or dynamic resources */
        std::string _handleGetReq(const std::string& url, int& ret_code);
        /* Post resources */
        std::string _handlePostReq(const std::string& path, const std::string& body, int& ret_code);
        /* Execute CGI program */
        std::string _execCgi(const std::string& path, const std::string& parameters, int& ret_code, std::string method);

    public:
        /* Init network connection */
        HTTPServer(std::uint16_t port, std::uint16_t maxPoolSize = std::thread::hardware_concurrency());
        /* Start RPC server */
        int run(std::uint16_t timeoutMs = 300);
    };
}
