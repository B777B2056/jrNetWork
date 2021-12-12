#include "timer.hpp"

namespace jrNetWork {
    Timer::Timer(TCPSocket* client, uint timeout, const std::function<void(TCPSocket*)>& th) : client(client), timeout_handler(th) {
        /* Set time */
         running_time = std::chrono::steady_clock::now()
                  +  std::chrono::duration<unsigned int, std::ratio<1>>(timeout);
    }

    bool Timer::operator<(const Timer& t) const {
        return running_time < t.running_time;
    }

    bool Timer::operator>(const Timer& t) const {
        return running_time > t.running_time;
    }

    bool Timer::operator==(const Timer& t) const {
        return running_time == t.running_time;
    }

    bool Timer::operator!=(const Timer& t) const {
        return !operator==(t);
    }

    void TimerContainer::add_timer(TCPSocket* client, uint timeout, const std::function<void(TCPSocket*)>& th) {
        container.emplace(client, timeout, th);
    }

    void TimerContainer::del_timer(const Timer& t) {
        container.erase(container.find(t));
    }

    Timer TimerContainer::get_min() const {
        return *(container.begin());
    }

    bool TimerContainer::is_empty() const {
        return container.empty();
    }

    void TimerContainer::tick() {
        while(!this->is_empty()) {
            Timer t = this->get_min();
            /* Min element NOT timeout */
            if(std::chrono::steady_clock::now() < t.running_time)
                break;
            /* Timeout */
            this->del_timer(t);
            t.timeout_handler(t.client);
        }
    }
}

