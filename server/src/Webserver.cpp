#include "Webserver.h"
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include "HttpReqParser.h"
#include "../network/Log.h"
#include "Provider.h"

namespace jrHTTP 
{
    void handleSIGPIPE()
    {
        LOGWARN() << "Connection closed by peer" << std::endl;
    }

    HTTPServer::HTTPServer(std::uint16_t port, std::uint16_t maxPoolSize)
        : _dispatcher(port, maxPoolSize)
        , _fileMappingPath(std::string(__FILE__).substr(0, std::string(__FILE__).find_last_of('/'))+"/source") 
    {
        _dispatcher.setSignalEventHandler(SIGPIPE, handleSIGPIPE);
        /* Set http handler */
        _dispatcher.setReadEventHandler(&HTTPServer::_handleHttpMsg, this);
    }

    int HTTPServer::run(std::uint16_t timeoutMs) 
    {
        return _dispatcher.run(timeoutMs);
    }

    void HTTPServer::_handleHttpMsg(std::shared_ptr<jrNetWork::TCP::Socket> client) 
    {
        /* Get the parser result */
        HttpReqParser::Result result = HttpReqParser::parserReq(client);
        int retCode = result.retCode;
        std::string content;
        switch (result.method)
        {
        case HttpMethod::GET:
            content = _handleGetReq(result.url, retCode);
            break;
        case HttpMethod::POST:
            if (result.url.substr(result.url.length() - 3, 3) == "RPC")
            {
                content = _handleRpcCall(result.content);
            }
            else
            {
                LOGNOTICE() << "Normal POST Req:" << result.url << std::endl;
            }
            break;
        default:
            break;
        }
        /* Send ret data */
        if (retCode != 0)
        {
            auto s = HttpReqParser::buildReqResponse(retCode, content);
            LOGNOTICE() << "Send:\n" << s << std::endl;
            client->send(s);
        }
        else
        {
            LOGNOTICE() << "Peer is closed!" << std::endl;
        }
    }

    std::string HTTPServer::_handleGetReq(const std::string &url, int &ret_code) 
    {
        std::size_t pos = url.find('?');
        if(pos == std::string::npos) 
        {
            /* Static resource */
            std::ifstream file(_fileMappingPath+url);
            if(file.is_open()) 
            {
                ret_code = 200;
                return std::string(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
            } 
            else 
            {
                ret_code = 404;
                return "";
            }
        } 
        else 
        {
            /* Dynamic resource */
            std::string path = url.substr(0, pos);
            std::string parameters = url.substr(pos+1, url.length());
            return _execCgi(path, parameters, ret_code, "GET");
        }
    }

    std::string HTTPServer::_handleRpcCall(const std::string& content)
    {
        return jrRPC::Provider::instance().callProc(content);
    }

    std::string HTTPServer::_execCgi(const std::string &path, const std::string &parameters, int &ret_code, std::string method) 
    {
        std::string cgi_path = _fileMappingPath + path;
        int cgi_input[2];
        int cgi_output[2];
        if(-1 == pipe(cgi_input)) 
        {
             ret_code = 500;
             LOGWARN() << "Pipe failed, " << ::strerror(errno) << std::endl;
             return "";
        }
        if(-1 == pipe(cgi_output)) 
        {
             ret_code = 500;
             LOGWARN() << "Pipe failed, " << ::strerror(errno) << std::endl;
             return "";
        }
        pid_t pid = fork();
        if(-1 == pid) 
        {
             ret_code = 500;
             LOGWARN() << "Process create failed, " << ::strerror(errno) << std::endl;
             return "";
        } 
        else if(0 == pid) 
        {
             ::close(cgi_input[1]);
             ::close(cgi_output[0]);
             ::dup2(cgi_input[0], STDIN_FILENO);
             ::dup2(cgi_output[1], STDOUT_FILENO);
             ::setenv("REQUEST_METHOD", method.c_str(), 1);
             ::setenv("QUERY_STRING", parameters.c_str(), 1);
             if(-1 == ::execl(cgi_path.c_str(), cgi_path.c_str(), NULL)) {
                 ret_code = 500;
                 std::cout << strerror(errno) << std::endl;
             }
             ::_exit(0);
         } 
        else 
        {
             ::close(cgi_input[0]);
             ::close(cgi_output[1]);
             std::string content("");
             char ch;
             while(::read(cgi_output[0], &ch, 1) > 0) {
                 content += ch;
             }
             ret_code = 200;
             ::close(cgi_input[1]);
             ::close(cgi_output[0]);
             ::waitpid(pid, NULL, WNOHANG);
             return content;
        }
    }
}
