#ifndef TIMER_HPP
#define TIMER_HPP

#include <set>
#include <chrono>
#include <iostream>
#include <functional>

namespace jrRPC {
    using time_type = std::chrono::time_point<std::chrono::steady_clock>;

    /* Timer */
    struct timer {
        int clientfd;    // client fd
        int epfd;    // epoll fd
        time_type running_time;    // program run time(absolute time)
        std::function<void(int,int)> timeout_handler;    // handler

        timer(int f, int e, uint timeout, const std::function<void(int,int)>& th);
        bool operator<(const timer& t) const;
        bool operator>(const timer& t) const;
        bool operator==(const timer& t) const;
        bool operator!=(const timer& t) const;
    };

    /* Timer container */
    class timer_container {
    private:
        /* Base container(RB-Tree) */
        std::set<timer> _cont;
        /* Get the min timer */
        timer _get_min() const;
        /* Check have or have not timer */
        bool _is_empty() const;


    public:
        /* Add a timer into container */
        void add_timer(const timer&);
        /* Delte processed timer from container */
        void del_timer(const timer&);
        /* Check timers in heap, exec timeout timer's handler */
        void tick();
    };
}

#endif

