#include "thread_pool.hpp"

namespace jrThreadPool {
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

    bool ThreadPool::add_task(TaskType task) {
        std::lock_guard<std::mutex> lock(locker);
        if(task_queue.size() >= max_task_num)
            return false;
        task_queue.emplace(task);
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
