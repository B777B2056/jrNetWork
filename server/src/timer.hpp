#ifndef TIMER_HPP
#define TIMER_HPP

#include <set>
#include <chrono>
#include <iostream>
#include <functional>

namespace jrRPC {
    typedef std::chrono::time_point<std::chrono::steady_clock> time_type;
    
    /* Set time */
    static time_type set_timeout(unsigned int timeout) {
        return std::chrono::steady_clock::now()
            +  std::chrono::duration<unsigned int, std::ratio<1>>(timeout);
    }

    /* Timer */
    struct timer {
        int fd;    // client fd
        int epfd;    // epoll fd
        time_type run_time;    // program run time(absolute time)
        void (*timeout_handler)(int, int);    // handler
        
        bool operator<(const timer& t) const {
            return run_time < t.run_time;
        }

        bool operator>(const timer& t) const {
            return run_time > t.run_time;
        }

        bool operator==(const timer& t) const {
            return run_time == t.run_time;
        }

        bool operator!=(const timer& t) const {
            return !operator==(t);
        }
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

