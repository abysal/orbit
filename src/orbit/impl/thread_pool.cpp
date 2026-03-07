#include "thread_pool.hpp"
#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <ranges>
#include <thread>
#include <utility>

namespace orb {

    uint32_t ThreadPool::thread_count = (std::thread::hardware_concurrency() != 1)
                                            ? std::thread::hardware_concurrency() - 1
                                            : 1;

    ThreadPool& ThreadPool::get_pool() noexcept {
        static ThreadPool pool{};

        return pool;
    }

    void ThreadPool::schedule(ThreadPool::ThreadJob&& job) {
        assert(!this->threads.empty());
        size_t index = 0;
        size_t min = std::numeric_limits<size_t>::max();

        for (const auto& [t_index, thread] : this->threads | std::views::enumerate) {
            const auto count = thread.get_job_count();
            if (count < min) {
                min = count;
                index = static_cast<size_t>(t_index);
            }
        }

        auto& thread = this->threads[index];

        thread.schedule_job(std::forward<ThreadPool::ThreadJob>(job));
    }

    ThreadPool::~ThreadPool() {
        for (auto& thread : this->threads) {
            thread.stop();
        }
    }

    ThreadPool::ThreadPool()
        : threads([this] {
              std::vector<WorkerThread> workers;
              for (uint32_t x = 0; x < ThreadPool::thread_count; x++)
                  threads.emplace_back([this]() -> std::vector<WorkerThread>& {
                      return this->threads;
                  });
              return workers;
          }()) {
        for (auto [index, thread] : this->threads | std::views::enumerate) {
            thread.start_spinning();
            this->thread_ids.push_back(thread.worker_thread.get_id());
        }
    }

    void
    ThreadPool::WorkerThread::schedule_job(ThreadPool::WorkerThread::JobType&& job) {
        std::unique_lock<std::mutex> lock(*this->job_mutex);
        this->job_list.push_back(std::forward<JobType>(job));
        this->job_count++;
        lock.unlock();
        this->resume_variable->notify_all();
    }

    bool ThreadPool::current_thread_on_pool() const noexcept {
        const auto id = std::this_thread::get_id();

        return std::ranges::contains(this->thread_ids, id);
    }

    ThreadPool::PoolStatistics ThreadPool::get_statistics() const noexcept {
        const auto executed_jobs = std::accumulate(
            this->threads.begin(),
            this->threads.end(),
            0,
            [](size_t current, const WorkerThread& two) {
                return current + two.get_completed();
            }
        );

        const auto waiting_jobs = std::accumulate(
            this->threads.begin(),
            this->threads.end(),
            0,
            [](int current, const WorkerThread& other) {
                return static_cast<int>(current)
                       + static_cast<int>(other.get_job_count());
            }
        );

        return PoolStatistics{ .jobs_completed = static_cast<size_t>(executed_jobs),
                               .job_count = static_cast<size_t>(waiting_jobs),
                               .thread_count = ThreadPool::thread_count };
    }

    ThreadPool::WorkerThread::WorkerThread(
        ThreadPool::WorkerThread::ListGrabber&& grabber
    )
        : resume_variable(std::make_unique<std::condition_variable>()),
          job_mutex(std::make_unique<std::mutex>()),
          list_grabber(std::forward<ListGrabber>(grabber)) {
    }

    ThreadPool::WorkerThread::~WorkerThread() {
        if (!this->started) {
            return;
        }

        while (this->executing || !this->worker_thread.joinable()) {
        }
        this->worker_thread.join();
    }

    void ThreadPool::WorkerThread::start_spinning() {
        this->worker_thread =
            std::thread{ &ThreadPool::WorkerThread::execution_task, this };
        this->started = true;
    }

    void ThreadPool::WorkerThread::execution_task() {
        while (this->executing) {
            this->get_function()();
            this->job_count--;
            this->executed_jobs++;
        }
    }

    void ThreadPool::full_wait() const {

        auto waiting_jobs = 1;

        while (waiting_jobs != 0) {
            waiting_jobs = 0;
            for (const auto& x : this->threads) {
                waiting_jobs += x.get_job_count();
            }
        }
    }

    bool ThreadPool::is_finished() const {
        auto waiting_jobs = 0;
        for (const auto& x : this->threads) {
            waiting_jobs += x.get_job_count();
        }

        return waiting_jobs == 0;
    }

    ThreadPool::WorkerThread::JobType ThreadPool::WorkerThread::get_function() {
        {
            std::unique_lock lock(*this->job_mutex);
            if (!this->job_list.empty()) {
                auto job = std::move(this->job_list.front());
                this->job_list.pop_front();
                return job;
            }
        }

        auto& other_threads = this->list_grabber();

        while (true) {
            size_t largest_index = SIZE_MAX;
            size_t largest_jobs = 0;

            // Find thread with most jobs
            for (const auto& [index, thread] : other_threads | std::views::enumerate) {
                if (&thread == this) continue;

                std::unique_lock try_lock(*thread.job_mutex, std::try_to_lock);
                if (!try_lock.owns_lock()) continue;

                size_t other_job_count = thread.job_list.size();
                if (other_job_count > largest_jobs) {
                    largest_jobs = other_job_count;
                    largest_index = static_cast<size_t>(index);
                }
            }

            // No jobs found to steal
            if (largest_index == SIZE_MAX || largest_jobs == 0) {
                // wait until this thread gets work again
                std::unique_lock lock(*this->job_mutex);
                this->resume_variable->wait(lock, [&]() {
                    return !this->job_list.empty();
                });

                auto job = std::move(this->job_list.front());
                this->job_list.pop_front();
                return job;
            }

            // Attempt to steal from the thread
            auto& thread = other_threads[largest_index];
            std::unique_lock steal_lock(*thread.job_mutex);

            // If this fails, that means another thread must have gotten in our way
            if (!thread.job_list.empty()) {
                auto job = std::move(thread.job_list.front());
                thread.job_list.pop_front();

                ++this->job_count;
                --thread.job_count;

                return job;
            }
        }
    }

} // namespace orb