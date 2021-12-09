#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <queue>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include <condition_variable>

namespace jrThreadPool {
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
        std::queue<std::function<void()>> _task_queue;
        /* Task queue mutex */
        std::mutex _locker;

    private:
        /* Run task */
        void _run();

    public:
        thread_pool(uint, uint);
        ~thread_pool();

        template<typename F, typename... Args>
        bool add_task(F&&, Args&&...);

    public:
        /* Not allowed Operation */
        thread_pool(const thread_pool&) = delete;
        thread_pool(thread_pool&&) = delete;
        thread_pool& operator=(const thread_pool&) = delete;
        thread_pool& operator=(thread_pool&&) = delete;
    };

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

    template<typename F, typename... Args>
    bool thread_pool::add_task(F&& f, Args&&... args) {
        std::unique_lock<std::mutex> lock(_locker);
        if(_task_queue.size() >= _max_task_num) {
            return false;
        }
        _task_queue.push(std::function<void()>(std::bind(std::forward<F>(f), std::forward<Args>(args)...)));
        // Wake a sleep thread up
        _condition.notify_one();
        return true;
    }

    void thread_pool::_run() {
        while(true) {
            std::function<void()> task;
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
            task();
        }
    }
}

#endif

