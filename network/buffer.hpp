#ifndef BUFFER_H
#define BUFFER_H

#include <string>

namespace jrNetWork {
    /* Buffer */
    class Buffer {
    private:
        using uint = unsigned int;
        std::string buffer;

    public:
        /* Get buffer current size */
        uint size() const;
        /* Check buffer is or not empty */
        bool empty() const;
        /* Get all readable or writable data from buffer */
        std::string get_data();
        /* Get readable or writable data from buffer by length */
        std::string get_data(uint length);
        /* Append data to buffer's tail */
        template<typename Iterator>
        void append(Iterator start, Iterator end);
    };

    template<typename Iterator>
    void Buffer::append(Iterator start, Iterator end) {
        buffer.append(start, end);
    }
}

#endif
