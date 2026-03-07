#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace orb {
    class ThreadPool {
    private:
        friend struct WorkerThread;

    public:
        using ThreadJob = std::move_only_function<void()>;

    public:
        struct PoolStatistics {
            size_t jobs_completed{ 0 };
            size_t job_count{};
            uint32_t thread_count{};
        };

    public:
        ThreadPool();
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;

        static void set_thread_count(uint32_t count) noexcept {
            ThreadPool::thread_count = count;
        }

        static ThreadPool& get_pool() noexcept;

        ~ThreadPool();

        void schedule(ThreadJob&& job);

        [[nodiscard]] PoolStatistics get_statistics() const noexcept;

        void full_wait() const;

        [[nodiscard]] bool is_finished() const;

        [[nodiscard]] bool current_thread_on_pool() const noexcept;

    private:
        class WorkerThread {
        public:
            using JobType = ThreadJob;
            using ListGrabber = std::move_only_function<std::vector<WorkerThread>&()>;

            [[nodiscard]] uint32_t get_job_count() const noexcept {
                return this->job_count.load(std::memory_order_seq_cst);
            }

            [[nodiscard]] uint32_t get_completed() const noexcept {
                return this->executed_jobs.load(std::memory_order_seq_cst);
            }

            void schedule_job(JobType&& function);

            void stop() {
                this->executing = false;

                // This is a slightly dirty hack.
                // It exists to help threads waiting for more work get out of that loop
                // So they hit the execution check and bail.
                this->schedule_job([] {});
            }

            WorkerThread(ListGrabber&& grabber);
            ~WorkerThread();
            WorkerThread(const WorkerThread&) = delete;
            WorkerThread(WorkerThread&& other) noexcept {
                this->worker_thread = std::move(other.worker_thread);
                this->resume_variable = std::move(other.resume_variable);
                this->job_list = std::move(other.job_list);
                this->job_mutex = std::move(other.job_mutex);
                this->job_count = other.job_count.load(std::memory_order_relaxed);
                this->executed_jobs =
                    other.executed_jobs.load(std::memory_order_relaxed);
                this->executing = other.executing.load(std::memory_order_relaxed);
                this->is_being_stolen_from =
                    other.is_being_stolen_from.load(std::memory_order_relaxed);
                this->list_grabber = std::move(other.list_grabber);
            }

            bool operator==(const WorkerThread& other) const noexcept {
                return this == &other;
            }

            void start_spinning();

        private:
            void execution_task();

            JobType get_function();

        private:
            std::thread worker_thread;
            std::unique_ptr<std::condition_variable> resume_variable;
            std::deque<JobType> job_list{};
            std::unique_ptr<std::mutex> job_mutex{};
            std::atomic_uint32_t job_count{ 0 };
            std::atomic_uint32_t executed_jobs{};
            std::atomic_bool executing{ true };
            std::atomic_bool started{ false };
            std::atomic_bool is_being_stolen_from{ false };
            ListGrabber list_grabber{};

            friend class ThreadPool;
        };

    private:

    private:
        static uint32_t thread_count;
        std::vector<WorkerThread> threads;
        std::vector<std::thread::id> thread_ids{};
        PoolStatistics statistics{ .jobs_completed = 0,
                                   .job_count = 0,
                                   .thread_count = ThreadPool::thread_count };
    };
} // namespace orb