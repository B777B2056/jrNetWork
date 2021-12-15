#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <queue>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <condition_variable>

namespace jrThreadPool {
    /* Thread pool */
    class ThreadPool {
    public:
        using TaskType = std::function<void()>;

    private:
        /* Max task num */
        const uint max_task_num;
        /* Storage threads */
        std::vector<std::thread> candidate_threads;
        /* Flag of thread pool stop */
        std::atomic_bool stop;
        /* Condition of worker queue */
        std::condition_variable condition;
        /* Task Queue */
        std::queue<TaskType> task_queue;
        /* Task queue mutex */
        mutable std::mutex locker;

    private:
        /* Run task */
        void run();

    public:
        ThreadPool(uint max_task_num, uint max_pool_size);
        ~ThreadPool();

        bool add_task(TaskType task);

    public:
        /* Not allowed Operation */
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    };
}

#endif

