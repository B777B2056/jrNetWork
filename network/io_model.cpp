#include "io_model.hpp"

namespace jrNetWork {
    MultiplexerPoll::MultiplexerPoll(uint max_task_n) : max_task_n(max_task_n), current_index(0), event_list(max_task_n) {

    }

    bool MultiplexerPoll::regist_event(Event<IO_Model_POLL> event) {
        if(current_index == max_task_n)
            return false;
        event_list.events[current_index].fd = event.event;
        event_list.events[current_index].events = event.type;
        ++current_index;
        return true;
    }

    bool MultiplexerPoll::unregist_event(Event<IO_Model_POLL> event) {
        for(int i = 1; i < max_task_n; ++i) {
            if(event.event == event_list.events[i].fd) {
                event_list.events[i].fd = -1;
                return true;
            }
        }
        return false;
    }

    int MultiplexerPoll::wait() {
        return ::poll(event_list.events, max_task_n, -1);
    }

    MultiplexerEpoll::MultiplexerEpoll(uint max_task_n)
        : max_task_n(max_task_n), event_list(max_task_n) {
        epollfd = ::epoll_create(max_task_n);
        if(epollfd == -1) {
            throw strerror(errno);
        }
    }

    MultiplexerEpoll::~MultiplexerEpoll() {
        ::close(epollfd);
    }

    bool MultiplexerEpoll::regist_event(Event<IO_Model_EPOLL> event) {
        ee.events = event.type;
        ee.data.fd = event.event;
        return epoll_ctl(epollfd, EPOLL_CTL_ADD, event.event, &ee) != -1;
    }

    bool MultiplexerEpoll::unregist_event(Event<IO_Model_EPOLL> event) {
        ee.events = event.type;
        ee.data.fd = event.event;
        return epoll_ctl(epollfd, EPOLL_CTL_DEL, event.event, &ee) != -1;
    }

    int MultiplexerEpoll::wait() {
        return ::epoll_wait(epollfd, event_list.events, max_task_n, -1);
    }
}
