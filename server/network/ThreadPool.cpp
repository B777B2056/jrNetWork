#include "ThreadPool.h"

namespace jrNetWork {
    ThreadPool::ThreadPool(std::uint16_t maxPoolSize) 
        : _stop(false)
        , _candidateThreads(new std::thread[maxPoolSize], [](std::thread* t) { if (t->joinable()) { t->join(); } })
    {
        for (std::size_t i = 0; i < maxPoolSize; ++i)
        {
            _candidateThreads[i] = std::move(std::thread(&ThreadPool::run, this));
        }
    }

    ThreadPool::~ThreadPool() 
    {
        _stop = true;    
        _condition.notify_all();
    }

    void ThreadPool::addTask(TaskType task) 
    {
        std::lock_guard<std::mutex> lock(_mutexLock);
        _taskQueue.emplace(task);
        // Wake a sleep thread up
        _condition.notify_one();
    }

    void ThreadPool::run() 
    {
        for(;;)
        {
            std::unique_lock<std::mutex> waitLock(_mutexLock);
            if(_stop) break;
            // Blocking thread when task queue is empty
            while (_taskQueue.empty())
            {
                _condition.wait(waitLock);
            }
            _taskQueue.front()();  // Run task
            _taskQueue.pop();
        }
    }
}
