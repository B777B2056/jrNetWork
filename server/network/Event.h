#pragma once

#include <deque>
#include <cstdint>
#include <sys/epoll.h>

namespace jrNetWork
{
    enum class EventType : std::uint8_t
    {
        LISTEN = 0X01,
        READ,
        WRITE,
        ConnClosed,
        SIGNAL,
        Timeout
    };

    using _NativeEvent = epoll_event;

    struct Event
    {
        int id;
        EventType type;

        Event() = default;
        Event(const _NativeEvent& ne);
        Event(_NativeEvent&& ne);

        Event& operator=(const _NativeEvent& ne);
        Event& operator=(_NativeEvent&& ne);

        bool operator==(const Event& rhs);
        bool operator!=(const Event& rhs);
    };
}
