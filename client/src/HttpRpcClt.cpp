#include "HttpRpcClt.h"
#include <sstream>

namespace jrRPC
{
    static const std::string httpVersion = "HTTP/1.0";
    static std::unordered_map<std::string, std::string> retTbl = { {"Server", "jrHTTP"},
                                                                   {"Connection", "Keep-Alive"} };

    RPCClient::RPCClient(const std::string& ip, std::uint16_t port)
        : _svrUrl(ip + ":" + std::to_string(port) + "/RPC")
    {
        _socket.connect(ip, port);
    }

    RPCClient::~RPCClient()
    {
        _socket.disconnect();
    }

    static std::string buildPostReq(const std::string& url, const std::string& content)
    {
        /* Set content length */
        retTbl["Content-Length"] = std::to_string(content.length());
        /* Build status line */
        std::stringstream ss;
        ss << "POST" << " "
            << url << " "
            << httpVersion << "\r\n";
        std::string ret(ss.str());
        /* Build response header */
        for (const auto& p : retTbl)
        {
            ret += (p.first + ":" + p.second + "\r\n");
        }
        ret += "\r\n";
        ret += content;
        return ret;
    }
    void RPCClient::_sendPostReq(const std::string& content)
    {
        _socket.send(buildPostReq(_svrUrl, content));
    }

    static bool parserResponseLine(jrNetWork::TCP::Socket* server, std::string& description)
    {
        enum State { VERSION, CODE, DESCRIPTION, END, ERROR };
        State state = VERSION;
        bool stop = false;
        std::string version, retCode;
        while (!stop)
        {
            auto recv = server->recv(1);
            if (recv.empty())
            {
                break;
            }
            if (recv[0] >= 'A' && recv[0] <= 'Z')
            {
                recv[0] = (recv[0] - 'A' + 'a');
            }
            switch (state)
            {
            case VERSION:
                if (recv[0] == ' ')
                {
                    state = CODE;
                }
                else
                {
                    version += recv[0];
                }
                break;
            case CODE:
                if (recv[0] >= '0' && recv[0] <= '9')
                {
                    retCode += recv[0];
                }
                else if (recv[0] == ' ')
                {
                    state = DESCRIPTION;
                }
                else
                {
                    state = ERROR;
                }
                break;
            case DESCRIPTION:
                if (recv[0] != '\r')
                {
                    description += recv[0];
                }
                else
                {
                    state = END;
                }
                break;
            case END:
                if (recv[0] != '\n')
                {
                    return false;
                }
                stop = true;
                break;
            case ERROR:
                return false;
            }
        }
        if (!stop)
        {
            return false;
        }
        else
        {
            return std::stoi(retCode) == 200;
        }
    }

    static int parserResponseHead(jrNetWork::TCP::Socket* server)
    {
        enum State { KEY, VALUE, NEXT_LINE, LINE_END, END, ERROR };
        State state = KEY;
        bool stop = false;
        int len = 0;
        std::string key, value;
        auto removeFrontSpace = [](std::string& str)->void
        {
            auto it = str.begin();
            for (; *it == ' '; ++it);
            str.erase(str.begin(), it);
        };
        while (!stop)
        {
            auto recv = server->recv(1);
            if (recv.empty())
            {
                break;
            }
            if (recv[0] >= 'A' && recv[0] <= 'Z')
            {
                recv[0] = (recv[0] - 'A' + 'a');
            }
            switch (state)
            {
            case KEY:
                if (recv[0] != ':')
                {
                    key += recv[0];
                    state = KEY;
                }
                else
                {
                    state = VALUE;
                }
                break;
            case VALUE:
                if (recv[0] != '\r')
                {
                    value += recv[0];
                    state = VALUE;
                }
                else
                {
                    state = NEXT_LINE;
                }
                break;
            case NEXT_LINE:
                if (recv[0] == '\n')
                {
                    removeFrontSpace(key);
                    removeFrontSpace(value);
                    if (key == "content-length")
                    {
                        len = std::stoi(value);
                    }
                    state = LINE_END;
                }
                else
                {
                    state = ERROR;
                }
                break;
            case LINE_END:
                if (recv[0] >= 'a' && recv[0] <= 'z')
                {
                    key += recv[0];
                    state = KEY;
                }
                else if (recv[0] == '\r')
                {
                    state = END;
                }
                else
                {
                    state = ERROR;
                }
                break;
            case END:
                if (recv[0] != '\n')
                {
                    return 0;
                }
                stop = true;
                break;
            case ERROR:
                return 0;
            }
        }
        return 0;
    }

    static std::string parserResponseBody(jrNetWork::TCP::Socket* server, int contentLength)
    {
        std::string content;
        while (contentLength--)
        {
            auto recv = server->recv(1);
            if (recv.empty()) break;
            content += recv[0];
        }
        return content;
    }

    std::string RPCClient::_recvResponse()
    {
        std::string description;
        if (!parserResponseLine(&_socket, description))
        {
            if (description.empty())
            {
                // Parser error

                return "";
            }
            else
            {
                // Http error
                return description;
            }
        }
        return parserResponseBody(&_socket, parserResponseHead(&_socket));
    }
}
