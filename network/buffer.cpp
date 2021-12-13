#include "buffer.hpp"

namespace jrNetWork {
    Buffer::Buffer(uint buffer_len) : send_start(0), send_end(0), send_buffer(buffer_len, '0'),
                                      recv_start(0), recv_end(0), recv_buffer(buffer_len, '0') {

    }

    uint Buffer::send_buffer_size() const {
        return send_end - send_start;
    }

    uint Buffer::recv_buffer_size() const {
        return recv_end - recv_start;
    }

    std::string Buffer::get_send() {
        std::string ret;
        uint i;
        for(i = 0; i+send_start < send_end; ++i) {
            ret += send_buffer[i];
        }
        send_buffer.erase(send_buffer.begin()+send_start, send_buffer.begin()+i);
        send_end = send_end-send_start;
        send_start = 0;
        return ret;
    }

    std::string Buffer::get_recv(uint length) {
        std::string ret;
        uint i;
        for(i = 0; i<length && i+recv_start<recv_end; ++i) {
            ret += recv_buffer[i];
        }
        recv_buffer.erase(recv_buffer.begin()+recv_start, recv_buffer.begin()+i);
        recv_end = recv_end-recv_start;
        recv_start = 0;
        return ret;
    }
}
