#pragma once

#include <set>
#include <memory>
#include <chrono>
#include <functional>
#include <unistd.h>

namespace jrNetWork {


    /* Timer container */
    template<class SocketType>
    class TimerContainer 
    {
    private:
        using CltPtrType = std::shared_ptr<SocketType>;
        using TimeoutCallbackType = std::function<void(CltPtrType)>;
        using TimePonitType = std::chrono::time_point<std::chrono::steady_clock>;
        struct _TimerInfo
        {
            CltPtrType cltPtr;
            TimePonitType time;

            _TimerInfo(CltPtrType c, std::uint16_t timeoutMs) 
                : cltPtr(c)
                , time(std::chrono::steady_clock::now()
                + std::chrono::duration<std::uint16_t, std::ratio<1>>(timeoutMs))
            {}

            bool operator<(const _TimerInfo& rhs) const { return time < rhs.time; }
            bool operator<=(const _TimerInfo& rhs) const { return time <= rhs.time; }
            bool operator>(const _TimerInfo& rhs) const { return time > rhs.time; }
            bool operator>=(const _TimerInfo& rhs) const { return time >= rhs.time; }
            bool operator==(const _TimerInfo& rhs) const { return time == rhs.time; }
            bool operator!=(const _TimerInfo& rhs) const { return time != rhs.time; }
        };

    private:
        std::uint16_t _timeoutMs = 0;
        /* Base container */
        std::set<_TimerInfo> _container;
        /* Get the min timer */
        const _TimerInfo& getMin() const { return *(_container.begin()); }
        /* Check have or have not timer */
        bool isEmpty() const { return _container.empty(); }

    public:
        TimerContainer() = default;

        void startCount(std::uint16_t timeoutMs)
        {
            _timeoutMs = timeoutMs;
            ::alarm(timeoutMs);
        }

        /* Add a timer into container */
        void addTask(CltPtrType cltPtr)
        {
            _container.emplace(cltPtr, _timeoutMs);
        }

        /* Delte processed timer from container top */
        void delTask()
        {
            _container.erase(_container.begin());
        }

        /* Check timers in heap, exec timeout timer's handler */
        void tick(TimeoutCallbackType callback)
        {
            while (!isEmpty())
            {
                /* Min element NOT timeout */
                if (std::chrono::steady_clock::now() < getMin().time)
                {
                    break;
                }
                callback(getMin().cltPtr);
                /* Timeout */
                delTask();
            }
            startCount(_timeoutMs); // Reset alarm
        }
    };
}
