#ifndef BUFFER_H
#define BUFFER_H

#include <string>

namespace jrNetWork {
    class Buffer {
    private:
        using uint = unsigned int;
        /* Buffer of send */
        uint send_start, send_end;
        std::string send_buffer;
        /* Buffer of recv */
        uint recv_start, recv_end;
        std::string recv_buffer;

    public:
        Buffer(uint buffer_len = 32768);
        /* Get buffer current size */
        uint send_buffer_size() const;
        uint recv_buffer_size() const;
        /* Get writable or readable data from buffer */
        std::string get_send();
        std::string get_recv(uint length);
        /* Append data to buffer's tail */
        template<typename Iterator>
        void append_send(Iterator start, Iterator end);
        template<typename Iterator>
        void append_recv(Iterator start, Iterator end);
        /* Push non-sent data(it's from the send buffer) to send buffer's head */
        template<typename Iterator>
        void push_front_send(Iterator start, Iterator end);
    };

    template<typename Iterator>
    void Buffer::append_send(Iterator start, Iterator end) {
        if(send_end == send_buffer.size())
            return ;
        send_buffer.insert(send_buffer.begin()+send_end, start, end);
        send_end += std::distance(start, end);
    }

    template<typename Iterator>
    void Buffer::append_recv(Iterator start, Iterator end) {
        if(recv_end == recv_buffer.size())
            return ;
        recv_buffer.insert(recv_buffer.begin()+recv_end, start, end);
        recv_end += std::distance(start, end);
    }

    template<typename Iterator>
    void Buffer::push_front_send(Iterator start, Iterator end) {
        send_buffer.insert(send_buffer.begin()+send_start, start, end);
        send_start -= std::distance(start, end);
    }
}

#endif
