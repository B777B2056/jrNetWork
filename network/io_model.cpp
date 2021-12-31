#include "io_model.hpp"

namespace jrNetWork {
    MultiplexerPoll::MultiplexerPoll(uint max_task_n) : max_task_n(max_task_n), current_index(0), event_list(max_task_n) {

    }

    bool MultiplexerPoll::set_listened(Event<IO_Model_POLL> listen) {
        return regist_event(listen);
    }

    IncomingTask MultiplexerPoll::get_incomming_task(int index) const {
        if(event_list[index].event < 0) {
            return ERROR;
        } else if((index == 0) && (event_list[index].type & POLLIN)) {
            return ACCEPTION;
        } else if(event_list[index].type & POLLIN) {
            return READABLE;
        } else if(event_list[index].type & POLLOUT) {
            return WRITEABLE;
        } else {
            return ERROR;
        }
    }

    bool MultiplexerPoll::regist_event(Event<IO_Model_POLL> event) {
        if(current_index == max_task_n+1)
            return false;
        event_list.events[current_index].fd = event.event;
        event_list.events[current_index].events = event.type;
        ++current_index;
        return true;
    }

    bool MultiplexerPoll::unregist_event(Event<IO_Model_POLL> event) {
        for(int i = 1; i <= max_task_n; ++i) {
            if(event.event == event_list.events[i].fd) {
                event_list.events[i].fd = -1;
                return true;
            }
        }
        return false;
    }

    int MultiplexerPoll::wait() {
        int flag = ::poll(event_list.events, current_index+1, -1);
        return flag==-1 ? -1 : current_index+1;
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

    bool MultiplexerEpoll::set_listened(Event<IO_Model_EPOLL> listen) {
        listenfd = listen.event;
        return regist_event(listen);
    }

    IncomingTask MultiplexerEpoll::get_incomming_task(int index) const {
        if(event_list[index].event < 0) {
            return ERROR;
        } else if(event_list[index].event == listenfd) {
            return ACCEPTION;
        } else if(event_list[index].type & EPOLLIN) {
            return READABLE;
        } else if(event_list[index].type & EPOLLOUT) {
            return WRITEABLE;
        } else {
            return ERROR;
        }
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
