#ifndef BUFFER_H
#define BUFFER_H

#include <string>

namespace jrNetWork {
    class Buffer {
    private:
        using uint = unsigned int;
        /* Buffer of send */
        std::string send_buffer;
        /* Buffer of recv */
        std::string recv_buffer;

    public:
        Buffer();
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
        send_buffer.append(start, end);
    }

    template<typename Iterator>
    void Buffer::append_recv(Iterator start, Iterator end) {
        recv_buffer.append(start, end);
    }

    template<typename Iterator>
    void Buffer::push_front_send(Iterator start, Iterator end) {
        send_buffer.insert(send_buffer.begin(), start, end);
    }
}

#endif
