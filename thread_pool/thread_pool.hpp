#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <noncopyable.hpp>
#include <worker.hpp>
#include <atomic>
#include <stdexcept>
#include <memory>
#include <vector>

/**
 * @brief The ThreadPoolOptions struct provides construction options for ThreadPool.
 */
struct ThreadPoolOptions {
    enum {AUTODETECT = 0};
    size_t threads_count = AUTODETECT;
    size_t worker_queue_size = 1024;
};

/**
 * @brief The ThreadPool class implements thread pool pattern.
 * It is highly scalable and fast.
 * It is header only.
 * It implements both work-stealing and work-distribution balancing startegies.
 * It implements cooperative scheduling strategy for tasks.
 */
class ThreadPool : NonCopyable {
public:
    /**
     * @brief ThreadPool Construct and start new thread pool.
     * @param options Creation options.
     */
    explicit ThreadPool(const ThreadPoolOptions &options = ThreadPoolOptions());

    /**
     * @brief ~ThreadPool Stop all workers and descroy thread pool.
     */
    ~ThreadPool();

    /**
     * @brief post Post piece of job to thread pool.
     * @param handler Handler to be called from thread pool worker. It has to be callable as 'handler()'.
     * @throws std::overflow_error if worker's queue is full.
     */
    template <typename Handler>
    void post(Handler &&handler);

private:
    /**
     * @brief get_worker Helper function to select next executing worker.
     * @return Reference to worker.
     */
    Worker & getWorker();

    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<size_t> m_next_worker;
};


/// Implementation

inline ThreadPool::ThreadPool(const ThreadPoolOptions &options)
    : m_next_worker(0)
{
    size_t workers_count = options.threads_count;

    if (ThreadPoolOptions::AUTODETECT == options.threads_count) {
        workers_count = std::thread::hardware_concurrency();
    }

    if (0 == workers_count) {
        workers_count = 1;
    }

    m_workers.resize(workers_count);
    for (auto &worker_ptr : m_workers) {
        worker_ptr.reset(new Worker(options.worker_queue_size));
    }

    for (size_t i = 0; i < m_workers.size(); ++i) {
        Worker *steal_donor = m_workers[(i + 1) % m_workers.size()].get();
        m_workers[i]->start(i, steal_donor);
    }
}

inline ThreadPool::~ThreadPool()
{
    for (auto &worker_ptr : m_workers) {
        worker_ptr->stop();
    }
}

template <typename Handler>
inline void ThreadPool::post(Handler &&handler)
{
    if (!getWorker().post(std::forward<Handler>(handler)))
    {
        throw std::overflow_error("worker queue is full");
    }
}

inline Worker & ThreadPool::getWorker()
{
    size_t id = Worker::getWorkerIdForCurrentThread();

    if (id > m_workers.size()) {
        id = m_next_worker.fetch_add(1, std::memory_order_relaxed) % m_workers.size();
    }

    return *m_workers[id];
}

#endif

