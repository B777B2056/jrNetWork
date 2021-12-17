#ifndef TIMER_HPP
#define TIMER_HPP

#include <set>
#include <memory>
#include <chrono>
#include <iostream>
#include <functional>

namespace jrNetWork {
    using time_type = std::chrono::time_point<std::chrono::steady_clock>;

    namespace TCP {
        class Socket;
    }

    /* Timer */
    struct Timer {
        std::shared_ptr<TCP::Socket> client;
        time_type running_time;    // program run time(absolute time)
        std::function<void(std::shared_ptr<TCP::Socket>)> timeout_handler;    // handler

        Timer(std::shared_ptr<TCP::Socket> client, uint timeout, const std::function<void(std::shared_ptr<TCP::Socket>)>& th);
        bool operator<(const Timer& t) const;
        bool operator>(const Timer& t) const;
        bool operator==(const Timer& t) const;
        bool operator!=(const Timer& t) const;
    };

    /* Timer container */
    class TimerContainer {
    private:
        /* Base container(RB-Tree) */
        std::set<Timer> container;
        /* Get the min timer */
        Timer get_min() const;
        /* Check have or have not timer */
        bool is_empty() const;


    public:
        /* Add a timer into container */
        void add_timer(std::shared_ptr<TCP::Socket> client, uint timeout,
                       const std::function<void(std::shared_ptr<TCP::Socket>)>& th);
        /* Delte processed timer from container */
        void del_timer(const Timer&);
        /* Check timers in heap, exec timeout timer's handler */
        void tick();
    };
}

#endif

