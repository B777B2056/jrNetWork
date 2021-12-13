#ifndef TIMER_HPP
#define TIMER_HPP

#include <set>
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
        TCP::Socket* client;
        time_type running_time;    // program run time(absolute time)
        std::function<void(TCP::Socket*)> timeout_handler;    // handler

        Timer(TCP::Socket* client, uint timeout, const std::function<void(TCP::Socket*)>& th);
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
        void add_timer(TCP::Socket* client, uint timeout, const std::function<void(TCP::Socket*)>& th);
        /* Delte processed timer from container */
        void del_timer(const Timer&);
        /* Check timers in heap, exec timeout timer's handler */
        void tick();
    };
}

#endif

