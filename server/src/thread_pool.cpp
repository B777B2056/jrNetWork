#include "thread_pool.hpp"

namespace tinyRPC {
    thread_pool::thread_pool(uint max_pool_size, uint max_task_num)
        : _max_pool_size(max_pool_size),
          _max_task_num(max_task_num),
          _candidate_threads(_max_pool_size),
          _stop(false) {
        for(uint i = 0; i < _max_pool_size; ++i) {
            _candidate_threads[i] = std::thread(&thread_pool::_run, this);
        }
    }

    thread_pool::~thread_pool() {
        _stop = true;    // atomic variable do not need mutex
        _condition.notify_all();
        for(auto& t : _candidate_threads) {
            if(t.joinable())
                t.join();
        }
        _candidate_threads.clear();
    }

    bool thread_pool::add_task(std::unique_ptr<tinyRPC::task_base>&& task) {
        std::unique_lock<std::mutex> lock(_locker);
        if(_task_queue.size() >= _max_task_num) {
            return false;
        }
        _task_queue.push(std::move(task));
        // Wake a sleep thread up
        _condition.notify_one();
        return true;
    }

    void thread_pool::_run() {
        while(true) {
            std::unique_ptr<task_base> task;
            {
                std::unique_lock<std::mutex> waitl(_locker);
                // Blocking thread when task queue is empty
                _condition.wait(waitl, [this]()->bool {
                                        return  this->_stop || !this->_task_queue.empty();
                                      }
                              );
                task = std::move(_task_queue.front());
                _task_queue.pop();
            }
            // Run task
            task->start();
        }
    }
}
