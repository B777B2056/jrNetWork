#include "buffer.hpp"

namespace jrNetWork {
    Buffer::Buffer() : send_buffer(),recv_buffer() {

    }

    uint Buffer::send_buffer_size() const {
        return send_buffer.size();
    }

    uint Buffer::recv_buffer_size() const {
        return recv_buffer.size();
    }

    std::string Buffer::get_send() {
        std::string ret = send_buffer;
        send_buffer.clear();
        return ret;
    }

    std::string Buffer::get_recv(uint length) {
        std::string ret;
        if(recv_buffer_size() <= length) {
            ret = recv_buffer;
            recv_buffer.clear();
        } else {
            ret.append(recv_buffer.begin(), recv_buffer.begin()+length);
            recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin()+length);
        }
        return ret;
    }
}
