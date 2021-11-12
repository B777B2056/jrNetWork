#include "timer.hpp"

namespace jrRPC {
    void timer_container::add_timer(const timer& t) {
        _cont.insert(t);
    }

    void timer_container::del_timer(const timer& t) {
        _cont.erase(_cont.find(t));
    }

    timer timer_container::_get_min() const {
        return *(_cont.begin());
    }

    bool timer_container::_is_empty() const {
        return _cont.empty();
    }

    void timer_container::tick() {
        while(!this->_is_empty()) {
            timer t = this->_get_min();
            /* Min element NOT timeout */
            if(std::chrono::steady_clock::now() < t.run_time)
                break;
            /* Timeout */
            this->del_timer(t);
            t.timeout_handler(t.epfd, t.fd);
        }
    }
}

