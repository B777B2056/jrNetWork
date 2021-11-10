#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <iostream>
#include <queue>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace tinyRPC {
    /* Task interface */
    struct task_base {
        virtual void start() = 0;
    };

    /* Thread pool */
    class thread_pool {
    private:
        /* Max thread num */
        const uint _max_pool_size;
        /* Max task num */
        const uint _max_task_num;
        /* Storage threads */
        std::vector<std::thread> _candidate_threads;
        /* Flag of thread pool stop */
        std::atomic_bool _stop;
        /* Condition of worker queue */
        std::condition_variable _condition;
        /* Task Queue */
        std::queue<std::shared_ptr<tinyRPC::task_base>> _task_queue;
        /* Task queue mutex */
        std::mutex _locker;

    public:
        thread_pool(uint max_pool_size, uint max_task_num)
            : _max_pool_size(max_pool_size),
              _max_task_num(max_task_num),
              _candidate_threads(_max_pool_size),
              _stop(false) {
            for(uint i = 0; i < _max_pool_size; ++i) {
                _candidate_threads[i] = std::thread(&thread_pool::_run, this);
            }
        }

        ~thread_pool() {
            _stop = true;    // atomic variable do not need mutex
            _condition.notify_all();
            for(auto& t : _candidate_threads) {
                if(t.joinable())
                    t.join();
            }
            _candidate_threads.clear();
        }

        bool add_task(std::shared_ptr<tinyRPC::task_base> task) {
            std::unique_lock<std::mutex> lock(_locker);
            if(_task_queue.size() >= _max_task_num) {
                return false;
            }
            std::cout << "Queue size: " << _task_queue.size() << std::endl;
            _task_queue.push(task);
            // Wake a sleep thread up
            _condition.notify_one();
            return true;
        }

    private:
        /* Run task */
        void _run() {
            while(true) {
                std::shared_ptr<task_base> task;
                {
                    std::unique_lock<std::mutex> waitl(_locker);
                    // Blocking thread when task queue is empty
                    _condition.wait(waitl, [this]()->bool {
                                            return  this->_stop || !this->_task_queue.empty();
                                          }
                                  );
                    task = _task_queue.front();
                    _task_queue.pop();
                }
                // Run task
                task->start();
            }
        }
    };
}

#endif

