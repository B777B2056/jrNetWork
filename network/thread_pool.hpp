#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

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
        std::queue<std::function<void()>> task_queue;
        /* Task queue mutex */
        mutable std::mutex locker;

    private:
        /* Run task */
        void run();

    public:
        ThreadPool(uint max_task_num, uint max_pool_size);
        ~ThreadPool();

        template<typename F, typename... Args>
        bool add_task(F&& f, Args&&... args);

    public:
        /* Not allowed Operation */
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    };

    ThreadPool::ThreadPool(uint max_task_num, uint max_pool_size) : max_task_num(max_task_num), stop(false) {
        for(uint i = 0; i < max_pool_size; ++i)
            candidate_threads.emplace_back(&ThreadPool::run, this);
    }

    ThreadPool::~ThreadPool() {
        stop = true;    // atomic variable do not need mutex
        condition.notify_all();
        for(auto& t : candidate_threads) {
            if(t.joinable())
                t.join();
        }
        candidate_threads.clear();
    }

    template<typename F, typename... Args>
    bool ThreadPool::add_task(F&& f, Args&&... args) {
        std::lock_guard<std::mutex> lock(locker);
        if(task_queue.size() >= max_task_num)
            return false;
        task_queue.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        // Wake a sleep thread up
        condition.notify_one();
        return true;
    }

    void ThreadPool::run() {
        while(true) {
            std::unique_lock<std::mutex> wait_locker(locker);
            if(stop)
                break;
            // Blocking thread when task queue is empty
            while(task_queue.empty())
                condition.wait(wait_locker);
            task_queue.front()();  // Run task
            task_queue.pop();

        }
    }
}

#endif

