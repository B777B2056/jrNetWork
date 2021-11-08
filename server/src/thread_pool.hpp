#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace tinyRPC {
    /* Task interface */
    class task_base {
    public:
        virtual void start() = 0;
    };

    /* Thread pool */
    class thread_pool {
    private:
        /* Max thread num */
        const unsigned int _max_pool_size;
        /* Max task num */
        const unsigned int _max_task_num;
        /* Storage threads */
        std::vector<std::thread> _candidate_threads;
        /* Flag of thread pool stop */
        std::atomic_bool _stop;
        /* Condition of worker queue */
        std::condition_variable _queue_cv;
        /* Work Queue */
        std::queue<std::shared_ptr<tinyRPC::task_base>> _work_queue;
        /* Work queue mutex */
        std::mutex _locker;

    public:
        thread_pool(unsigned int max_pool_size = 16, unsigned int max_task_num = 100)
            : _max_pool_size(max_pool_size),
              _max_task_num(max_task_num),
              _candidate_threads(_max_pool_size),
              _stop(false) {
            for(unsigned int i = 0; i < _max_pool_size; ++i) {
                _candidate_threads[i] = std::thread(&thread_pool::_run, this);
            }
        }

        ~thread_pool() {
            _stop = true;    // atomic variable do not need mutex
            _queue_cv.notify_all();
            for(auto& t : _candidate_threads) {
                if(t.joinable())
                    t.join();
            }
        }

        bool add_task(const std::shared_ptr<tinyRPC::task_base>& task) {
            std::lock_guard<std::mutex> lock(_locker);
            if(_work_queue.size() >= _max_task_num) {
                return false;
            }
            _work_queue.push(task);
            // Wake a sleep thread up
            _queue_cv.notify_one();
            return true;
        }

        bool add_task(std::shared_ptr<tinyRPC::task_base>&& task) {
            std::lock_guard<std::mutex> lock(_locker);
            if(_work_queue.size() >= _max_task_num) {
                return false;
            }
            _work_queue.push(task);
            // Wake a sleep thread up
            _queue_cv.notify_one();
            return true;
        }

    private:
        /* Run task */
        void _run() {
            while(!_stop) {
                std::unique_lock<std::mutex> waitl(_locker);
                // Blocking thread when worker queue is empty
                _queue_cv.wait(waitl, 
                               [this]()->bool { 
                                        return !this->_work_queue.empty(); 
                                    }
                              );
                auto task = _work_queue.front();
                _work_queue.pop();
                // Run task
                task->start();
            }
        }
    };
}

#endif

