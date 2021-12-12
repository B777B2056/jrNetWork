#ifndef TIMER_HPP
#define TIMER_HPP

#include <set>
#include <chrono>
#include <iostream>
#include <functional>

namespace jrNetWork {
    using time_type = std::chrono::time_point<std::chrono::steady_clock>;

    class TCPSocket;

    /* Timer */
    struct Timer {
        TCPSocket* client;
        time_type running_time;    // program run time(absolute time)
        std::function<void(TCPSocket*)> timeout_handler;    // handler

        Timer(TCPSocket* client, uint timeout, const std::function<void(TCPSocket*)>& th);
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
        void add_timer(TCPSocket* client, uint timeout, const std::function<void(TCPSocket*)>& th);
        /* Delte processed timer from container */
        void del_timer(const Timer&);
        /* Check timers in heap, exec timeout timer's handler */
        void tick();
    };
}

#endif

