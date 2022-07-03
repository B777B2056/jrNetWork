#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <cstdint>
#include <functional>
#include <condition_variable>

namespace jrNetWork {
    /* Thread pool */
    class ThreadPool {
    public:
        using TaskType = std::function<void()>;

    private:
        /* Storage threads */
        std::unique_ptr<std::thread[], std::function<void(std::thread*)> > _candidateThreads;
        /* Flag of thread pool stop */
        std::atomic_bool _stop;
        /* Condition of worker queue */
        std::condition_variable _condition;
        /* Task Queue */
        std::queue<TaskType> _taskQueue;
        /* Task queue mutex */
        mutable std::mutex _mutexLock;

    private:
        /* Run task */
        void run();

    public:
        ThreadPool(std::uint16_t maxPoolSize);
        ~ThreadPool();

        void addTask(TaskType task);

    public:
        /* Not allowed Operation */
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    };
}
