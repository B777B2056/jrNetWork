#include "webserver.hpp"

namespace jrHTTP {
    std::unordered_map<int, std::string> HTTPServer::status_table = {{200, "OK"},
                                                                     {400, "Bad Request"}, {404, "Not Found"},
                                                                     {500, "Internal Server Error"}, {501, "Not Implemented"}};

    HTTPServer::HTTPServer(uint port, std::string path, std::string work_directory, uint max_task_num, uint max_pool_size)
        : dispatch(port, max_task_num, max_pool_size, path),
          HTTP_Version("HTTP/1.0"), file_mapping_path(work_directory) {
        /* Init response header */
        add_ret_line("Server", "jrHTTP");
        add_ret_line("Connection", "close");   // Default setting: close TCP connection
        /* Set http handler */
        dispatch.set_event_handler(&HTTPServer::http_handler, this);
    }

    void HTTPServer::add_ret_line(std::string key, std::string value) {
        ret_head_table[key] = value;
    }

    void HTTPServer::loop(uint timeout_period_sec) {
        dispatch.event_loop(timeout_period_sec);
    }

    void HTTPServer::http_handler(jrNetWork::TCP::Socket *client) {
        int ret_code = 200;
        /* HTTP request; data */
        hash_map request_line_table;
        hash_map request_head_table;
        std::string request_body;
        /* Parser request line, get METHOD, URL and VERSION */
        parser_request_line(client, request_line_table, ret_code);
        if(ret_code != 200) {
            goto SEND_RET;
        }
        /* Parser request head, get key-value pair */
        parser_request_head(client, request_head_table, ret_code);
        if(ret_code != 200) {
            goto SEND_RET;
        }
        /* Parser request body by Content-Length(GET method always doesn't have Content-Length tag) */
        if(request_head_table.find("content-length") != request_head_table.end()) {
            parser_request_body(client, std::stoi(request_head_table["content-length"]), request_body);
        }
    SEND_RET:
        /* Ret body build */
        std::string ret_body;
        if(ret_code == 200) {
            /* Prepare related resources
             * (execute the CGI program and get the result, or package the corresponding static resource into a string)
             */
            if(request_line_table["method"] == "get") {
                ret_body = get_resource(request_line_table["url"], ret_code);
            } else if (request_line_table["method"] == "post") {
                ret_body = post_resource(request_line_table["url"], request_body, ret_code);
            } else {
                ret_code = 501;
                LOG(jrNetWork::Logger::NOTICE, "Wrong method: " + request_line_table["method"]);
            }
        }
        /* Build status line */
        std::string ret;
        ret += HTTP_Version + " "
             + std::to_string(ret_code) + " "
             + status_table[ret_code]
             + "\r\n";
        /* Build response header */
        for(auto& p : ret_head_table)
            ret += (p.first+":"+p.second+"\r\n");
        ret += "\r\n";
        /* Attach response body */
        ret += ret_body;
        /* Send ret data */
        client->send(ret);
        client->disconnect();   // close connection
        LOG(jrNetWork::Logger::NOTICE, "Status code: " + std::to_string(ret_code));
    }

    std::string HTTPServer::get_resource(const std::string &url, int &ret_code) {
        std::size_t pos = url.find('?');
        if(pos == std::string::npos) {
            /* Static resource */
            std::ifstream file(file_mapping_path+url);
            if(file.is_open()) {
                ret_code = 200;
                return std::string(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
            } else {
                ret_code = 404;
                return "";
            }
        } else {
            /* Dynamic resource */
            std::string path = url.substr(0, pos);
            std::string parameters = url.substr(pos+1, url.length());
            return exec_cgi(path, parameters, ret_code, "GET");
        }
    }

    std::string HTTPServer::post_resource(const std::string &path, const std::string &body, int &ret_code) {
        return exec_cgi(path, body, ret_code, "POST");
    }

    std::string HTTPServer::exec_cgi(const std::string &path, const std::string &parameters, int &ret_code, std::string method) {
        std::string cgi_path = file_mapping_path + path;
        int cgi_input[2];
        int cgi_output[2];
        if(-1 == pipe(cgi_input)) {
             ret_code = 500;
             return "";
        }
        if(-1 == pipe(cgi_output)) {
             ret_code = 500;
             return "";
        }
        pid_t pid = fork();
        if(-1 == pid) {
             ret_code = 500;
             return "";
        } else if(0 == pid) {
             close(cgi_input[1]);
             close(cgi_output[0]);
             dup2(cgi_input[0], STDIN_FILENO);
             dup2(cgi_output[1], STDOUT_FILENO);
             setenv("REQUEST_METHOD", method.c_str(), 1);
             setenv("QUERY_STRING", parameters.c_str(), 1);
             if(-1 == execl(cgi_path.c_str(), cgi_path.c_str(), NULL)) {
                 ret_code = 500;
                 std::cout << strerror(errno) << std::endl;
             }
             _exit(0);
         } else {
             close(cgi_input[0]);
             close(cgi_output[1]);
             std::string content("");
             char ch;
             while(read(cgi_output[0], &ch, 1) > 0) {
                 content += ch;
             }
             ret_code = 200;
             close(cgi_input[1]);
             close(cgi_output[0]);
             waitpid(pid, NULL, WNOHANG);
             return content;
        }
    }

    void HTTPServer::parser_request_line(jrNetWork::TCP::Socket *client, hash_map &request_line_table, int& ret_code) {
        enum State {START, METHOD, URL, VERSION, END, ERROR};
        State state = START;
        bool stop = false;
        std::string method, url, version;
        while(!stop) {
            auto recv = client->recv(1);
            if(recv.first.empty()) {
                break;
            }
            if(recv.first[0]>='A' && recv.first[0]<='Z')
                recv.first[0] = (recv.first[0]-'A'+'a');
            switch (state) {
            case START:
                if(recv.first[0]>='a' && recv.first[0]<='z') {
                    method += recv.first[0];
                    state = METHOD;
                } else if(recv.first[0]==' ') {
                    state = START;
                } else {
                    state = ERROR;
                }
                break;
            case METHOD:
                if(recv.first[0]>='a' && recv.first[0]<='z') {
                    method += recv.first[0];
                    state = METHOD;
                } else if(recv.first[0]==' ') {
                    state = URL;
                }
                break;
            case URL:
                if(recv.first[0]!=' ') {
                    url += recv.first[0];
                    state = URL;
                } else {
                    state = VERSION;
                }
                break;
            case VERSION:
                if(recv.first[0]!='\r') {
                    version += recv.first[0];
                    state = VERSION;
                } else {
                    state = END;
                }
                break;
            case END:
                if(recv.first[0]!='\n') {
                    ret_code = 400;
                    LOG(jrNetWork::Logger::WARNING, "Request line invalid");
                }
                stop = true;
                break;
            case ERROR:
                ret_code = 400;
                LOG(jrNetWork::Logger::WARNING, "Request line invalid");
                stop = true;
                break;
            }
        }
        if(!stop) {
            ret_code = 500;
        } else {
            request_line_table["method"] = method;
            request_line_table["url"] = url;
            request_line_table["version"] = version;
        }
    }

    void HTTPServer::parser_request_head(jrNetWork::TCP::Socket *client, hash_map &request_head_table, int& ret_code) {
        enum State {START, KEY, VALUE, NEXT_LINE, LINE_END, END, ERROR};
        State state = START;
        bool stop = false;
        std::string key, value;
        while(!stop) {
            auto recv = client->recv(1);
            if(recv.first.empty()) {
                break;
            }
            switch (state) {
            case START:
                if((recv.first[0]>='a' && recv.first[0]<='z') || (recv.first[0]>='A' && recv.first[0]<='Z')) {
                    key += recv.first[0];
                    state = KEY;
                } else {
                    state = ERROR;
                }
                break;
            case KEY:
                if(recv.first[0]!=':') {
                    key += recv.first[0];
                    state = KEY;
                } else {
                    state = VALUE;
                }
                break;
            case VALUE:
                if(recv.first[0]!='\r') {
                    value += recv.first[0];
                    state = VALUE;
                } else {
                    state = NEXT_LINE;
                }
                break;
            case NEXT_LINE:
                if(recv.first[0]=='\n') {
                    /* remove front space */
                    auto remove_front_space = [](std::string& str)->void {
                        auto it = str.begin();
                        while(*it == ' ')
                            ++it;
                        str.erase(str.begin(), it);
                    };
                    remove_front_space(key);
                    remove_front_space(value);
                    request_head_table[key] = value;
                    key = value = "";
                    state = LINE_END;
                } else {
                    state = ERROR;
                }
                break;
            case LINE_END:
                if((recv.first[0]>='a' && recv.first[0]<='z') || (recv.first[0]>='A' && recv.first[0]<='Z')) {
                    key += recv.first[0];
                    state = KEY;
                } else if(recv.first[0]=='\r') {
                    state = END;
                } else {
                    state = ERROR;
                }
                break;
            case END:
                if(recv.first[0]!='\n') {
                    ret_code = 400;
                    LOG(jrNetWork::Logger::WARNING, "Request head invalid");
                }
                stop = true;
                break;
            case ERROR:
                ret_code = 400;
                LOG(jrNetWork::Logger::WARNING, "Request head invalid");
                stop = true;
                break;
            }
        }
    }

    void HTTPServer::parser_request_body(jrNetWork::TCP::Socket *client, int content_length, std::string &request_body) {
        while(content_length--) {
            auto recv = client->recv(1);
            if(!recv.second)
                break;
            request_body += recv.first[0];
        }
    }
}
