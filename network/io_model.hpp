#ifndef IO_MODEL_H
#define IO_MODEL_H

#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/epoll.h>

namespace jrNetWork {
    class IO_Model {};
    class IO_Model_POLL : public IO_Model {};
    class IO_Model_EPOLL : public IO_Model {};

    /* ============ Event ============ */
    template<typename model>
    struct EventType {};

    template<>
    struct EventType<IO_Model_POLL> {
        enum Type {READ_IN = POLLIN, WRITE_OUT = POLLOUT};
    };

    template<>
    struct EventType<IO_Model_EPOLL> {
        enum Type {READ_IN = EPOLLIN|EPOLLET, WRITE_OUT = EPOLLOUT|EPOLLET};
    };

    template<typename model>
    struct Event {
        int event;
        typename EventType<model>::Type type;
    };

    template<typename model>
    Event<model> make_event(int event, typename EventType<model>::Type type) {
        return Event<model>{event, type};
    }

    /* ============ Event List ============ */

    template<typename model>
    class EventList {};

    template<>
    class EventList<IO_Model_POLL> {
    friend class MultiplexerPoll;
    private:
        pollfd* events;

    public:
        EventList(uint max_task_n) : events(new pollfd[max_task_n+1]) {
            for(uint i=0; i<max_task_n; ++i)
                events[i].fd = -1;
            events[0].events = POLLIN;
        }

        ~EventList() { delete[] events; }
        Event<IO_Model_POLL> operator[](std::size_t index) const {
            return Event<IO_Model_POLL>{events[index].fd,
                                        EventType<IO_Model_POLL>::Type(events[index].revents)};
        }
    };

    template<>
    class EventList<IO_Model_EPOLL> {
    friend class MultiplexerEpoll;
    private:
        epoll_event* events;

    public:
        EventList(uint max_task_n) : events(new epoll_event[max_task_n]) {}
        ~EventList() { delete[] events; }
        Event<IO_Model_EPOLL> operator[](std::size_t index) const {
            return Event<IO_Model_EPOLL>{events[index].data.fd,
                                         EventType<IO_Model_EPOLL>::Type(events[index].events)};
        }
    };

    /* ============ IO multiplexing ============ */
    enum IncomingTask {ACCEPTION, READABLE, WRITEABLE, ERROR};

    template<typename model>
    struct MultiplexerBase {
        virtual ~MultiplexerBase() {}
        virtual bool set_listened(Event<model> listen) = 0;
        virtual IncomingTask get_incomming_task(int index) const = 0;
        virtual bool regist_event(Event<model> event) = 0;
        virtual bool unregist_event(Event<model> event) = 0;
        virtual int wait() = 0;
        virtual const EventList<model>& get_event_list() const = 0;
    };

    /* Poll */
    class MultiplexerPoll : public MultiplexerBase<IO_Model_POLL> {
    private:
        int max_task_n;
        int current_index;
        EventList<IO_Model_POLL> event_list;

    public:
        MultiplexerPoll(uint max_task_n);
        virtual ~MultiplexerPoll() {}
        virtual bool set_listened(Event<IO_Model_POLL> listen) override;
        virtual IncomingTask get_incomming_task(int index) const override;
        virtual bool regist_event(Event<IO_Model_POLL> event) override;    // Register event
        virtual bool unregist_event(Event<IO_Model_POLL> event) override;  // Unregister event
        virtual int wait() override;
        virtual const EventList<IO_Model_POLL>& get_event_list() const override { return event_list; }
    };

    /* Epoll */
    class MultiplexerEpoll : public MultiplexerBase<IO_Model_EPOLL> {
    private:
        int max_task_n;
        int listenfd;
        int epollfd;    // epoll file descriptor
        epoll_event ee; // epoll event object
        EventList<IO_Model_EPOLL> event_list;

    public:
        MultiplexerEpoll(uint max_task_n);   // epoll init, create epoll file descriptor
        virtual ~MultiplexerEpoll();
        virtual bool set_listened(Event<IO_Model_EPOLL> listen) override;  // Set server listen socket
        virtual IncomingTask get_incomming_task(int index) const override;
        virtual bool regist_event(Event<IO_Model_EPOLL> event) override;    // Register event into epoll wait
        virtual bool unregist_event(Event<IO_Model_EPOLL> event) override;  // Unregister event from epoll wait
        virtual int wait() override;
        virtual const EventList<IO_Model_EPOLL>& get_event_list() const override { return this->event_list; }
    };
}

#endif
