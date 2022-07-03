#pragma once

#include <string>
#include <cstdint>

namespace jrNetWork {
    /* Buffer */
    class Buffer {
    private:
        std::string _buffer;

    public:
        /* Get buffer current size */
        std::size_t size() const;
        /* Check buffer is or not empty */
        bool empty() const;
        /* Get all readable or writable data from buffer */
        std::string getData();
        /* Get readable or writable data from buffer by length */
        std::string getData(std::uint32_t length);
        /* Append data to buffer's tail */
        template<typename Iterator>
        void append(Iterator start, Iterator end)
        {
            _buffer.append(start, end);
        }
    };
}
