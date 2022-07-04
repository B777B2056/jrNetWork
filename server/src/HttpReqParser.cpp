#include "HttpReqParser.h"
#include <sstream>
#include <unordered_map>

namespace jrHTTP
{
    static bool peerIsClosed = false;
    static int innerRetCode = 0;
    static const std::string httpVersion = "HTTP/1.0";
    static std::unordered_map<std::string, std::string> reqTbl;
    static std::unordered_map<std::string, std::string> retTbl = { {"Server", "jrHTTP"},
                                                                   {"Connection", "Keep-Alive"} };
    static const std::unordered_map<int, std::string> statusTbl = { {200, "OK"},
                                                                    {400, "Bad Request"}, 
                                                                    {404, "Not Found"},
                                                                    {500, "Internal Server Error"}, 
                                                                    {501, "Not Implemented"} };

    static bool parserRequestLine(std::shared_ptr<jrNetWork::TCP::Socket> client)
    {
        enum State { METHOD, URL, VERSION, END, ERROR };
        State state = METHOD;
        bool stop = false;
        std::string method, url, version;
        while (!stop)
        {
            auto recv = client->recv(1);
            if (recv.empty())
            {
                peerIsClosed = true;
                break;
            }
            if ((state != URL) && (recv[0] >= 'A' && recv[0] <= 'Z'))
            {
                recv[0] = (recv[0] - 'A' + 'a');
            }
            switch (state)
            {
            case METHOD:
                if (recv[0] >= 'a' && recv[0] <= 'z')
                {
                    method += recv[0];
                    state = METHOD;
                }
                else if (recv[0] == ' ')
                {
                    state = URL;
                }
                break;
            case URL:
                if (recv[0] != ' ')
                {
                    url += recv[0];
                    state = URL;
                }
                else
                {
                    state = VERSION;
                }
                break;
            case VERSION:
                if (recv[0] != '\r')
                {
                    version += recv[0];
                    state = VERSION;
                }
                else
                {
                    state = END;
                }
                break;
            case END:
                if (recv[0] != '\n')
                {
                    innerRetCode = 400;
                    return false;
                }
                stop = true;
                break;
            case ERROR:
                innerRetCode = 400;
                return false;
            }
        }
        if (!stop)
        {
            innerRetCode = 500;
            return false;
        }
        else
        {
            innerRetCode = 200;
            reqTbl["method"] = method;
            reqTbl["url"] = url;
            reqTbl["version"] = version;
            return true;
        }
    }

    static bool parserRequestHead(std::shared_ptr<jrNetWork::TCP::Socket> client)
    {
        enum State { KEY, VALUE, NEXT_LINE, LINE_END, END, ERROR };
        State state = KEY;
        bool stop = false;
        std::string key, value;
        auto removeFrontSpace = [](std::string& str)->void
        {
            auto it = str.begin();
            for (; *it == ' '; ++it);
            str.erase(str.begin(), it);
        };
        while (!stop)
        {
            auto recv = client->recv(1);
            if (recv.empty())
            {
                peerIsClosed = true;
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
                    reqTbl[key] = value;
                    key = value = "";
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
                    innerRetCode = 400;
                    return false;
                }
                stop = true;
                break;
            case ERROR:
                innerRetCode = 400;
                return false;
            }
        }
        return true;
    }

    static std::string parserRequestBody(std::shared_ptr<jrNetWork::TCP::Socket> client, int contentLength)
    {
        std::string content;
        while (contentLength--)
        {
            auto recv = client->recv(1);
            if (recv.empty()) break;
            content += recv[0];
        }
        return content;
    }

    std::string HttpReqParser::buildReqResponse(int retCode, const std::string& content)
    {
        /* Set content length */
        retTbl["Content-Length"] = std::to_string(content.length());
        /* Build status line */
        std::stringstream ss;
        ss << httpVersion << " "
           << retCode << " "
           << statusTbl.at(retCode) << "\r\n";
        std::string ret(ss.str());
        /* Build response header */
        for (const auto& p : retTbl)
        {
            ret += (p.first + ":" + p.second + "\r\n");
        }
        ret += "\r\n";
        /* Attach response body */
        ret += content;
        return ret;
    }

    HttpReqParser::Result HttpReqParser::parserReq(std::shared_ptr<jrNetWork::TCP::Socket> client)
    {
        HttpReqParser::Result ret;
        if (parserRequestLine(client) && parserRequestHead(client))
        {
            if (reqTbl["method"] == "get")
            {
                ret.method = HttpMethod::GET;
            }
            else
            {
                ret.method = HttpMethod::POST;
            }
            ret.url = reqTbl["url"];
            if (reqTbl.count("content-length") != 0)
            {
                ret.content = parserRequestBody(client, std::stoi(reqTbl["content-length"]));
            }
        }
        ret.retCode = peerIsClosed ? 0 : innerRetCode;
        reqTbl.clear();
        return ret;
    }
}
