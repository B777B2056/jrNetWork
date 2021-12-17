#include "buffer.hpp"

namespace jrNetWork {
    uint Buffer::size() const {
        return buffer.size();
    }

    bool Buffer::empty() const {
        return size() == 0;
    }

    std::string Buffer::get_data() {
        return get_data(buffer.size());
    }

    std::string Buffer::get_data(uint length) {
        std::string ret;
        if(size() <= length) {
            ret = buffer;
            buffer.clear();
        } else {
            ret.append(buffer.begin(), buffer.begin()+length);
            buffer.erase(buffer.begin(), buffer.begin()+length);
        }
        return ret;
    }
}
