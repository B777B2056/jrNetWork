#include "HttpParser.h"
#include <sstream>
#include <unordered_map>

namespace jrHTTP
{
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
        enum State { START, METHOD, URL, VERSION, END, ERROR };
        State state = START;
        bool stop = false;
        std::string method, url, version;
        while (!stop)
        {
            auto recv = client->recv(1);
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
            case START:
                if (recv[0] >= 'a' && recv[0] <= 'z')
                {
                    method += recv[0];
                    state = METHOD;
                }
                else if (recv[0] == ' ')
                {
                    state = START;
                }
                else
                {
                    state = ERROR;
                }
                break;
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
        enum State { START, KEY, VALUE, NEXT_LINE, LINE_END, END, ERROR };
        State state = START;
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
                break;
            }
            if (recv[0] >= 'A' && recv[0] <= 'Z')
            {
                recv[0] = (recv[0] - 'A' + 'a');
            }
            switch (state)
            {
            case START:
                if (recv[0] >= 'a' && recv[0] <= 'z')
                {
                    key += recv[0];
                    state = KEY;
                }
                else
                {
                    state = ERROR;
                }
                break;
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

    std::string HttpParser::buildReqResponse(int retCode, const std::string& content)
    {
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

    HttpParser::Result HttpParser::parserReq(std::shared_ptr<jrNetWork::TCP::Socket> client)
    {
        HttpParser::Result ret;
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
        ret.retCode = innerRetCode;
        reqTbl.clear();
        return ret;
    }

    static std::string buildReqHelper(const std::string& url, const std::string& content)
    {
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

    std::string HttpParser::buildGetReq(const std::string& url)
    {
        return buildReqHelper(url, "");
    }

    std::string HttpParser::buildPostReq(const std::string& url, const std::string& content)
    {
        return buildReqHelper(url, content);
    }
}
