#include "Buffer.h"

namespace jrNetWork {
    std::size_t Buffer::size() const 
    {
        return _buffer.size();
    }

    bool Buffer::empty() const 
    {
        return size() == 0;
    }

    std::string Buffer::getData() 
    {
        return getData(_buffer.size());
    }

    std::string Buffer::getData(std::uint32_t length)
    {
        std::string ret;
        if(size() <= length) 
        {
            ret = _buffer;
            _buffer.clear();
        } 
        else 
        {
            ret.append(_buffer.begin(), _buffer.begin()+length);
            _buffer.erase(_buffer.begin(), _buffer.begin()+length);
        }
        return ret;
    }
}
