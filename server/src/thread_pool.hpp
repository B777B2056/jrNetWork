#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

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
        std::queue<std::unique_ptr<tinyRPC::task_base>> _task_queue;
        /* Task queue mutex */
        std::mutex _locker;

    private:
        /* Run task */
        void _run();

    public:
        thread_pool(uint, uint);
        ~thread_pool();
        bool add_task(std::unique_ptr<tinyRPC::task_base>&&);

    public:
        /* Not allowed Operation */
        thread_pool(const thread_pool&) = delete;
        thread_pool(thread_pool&&) = delete;
        thread_pool& operator=(const thread_pool&) = delete;
        thread_pool& operator=(thread_pool&&) = delete;
    };
}

#endif

