#pragma once
#include "impl/platform.hpp"
#include "impl/type_hash.hpp"
#include "schedules.hpp"

#include <memory_resource>
#include <set>
#include <vector>

namespace orb {

    template <typename T>
        requires (!std::is_const_v<T>)
    class EventReader;

    template <typename T>
        requires (!std::is_const_v<T>)
    class EventWriter;

    namespace impl {
        struct BaseEventObject {
            bool canceled{ false };
        };

        template <typename T>
        struct WrapperEvent : BaseEventObject {
            const T storage;

            explicit(false) WrapperEvent(T&& value) : storage(std::move(value)) {
            }
        };

        template <typename T>
        struct EventWiper;

        // Intentionally left private
        template <typename T>
        class EventProcesser {
            EventProcesser() = default;

            std::vector<WrapperEvent<T>> event_objects{};
            friend class EventReader<T>;
            friend class EventWriter<T>;
            friend struct EventWiper<T>;

            static EventProcesser& instance() {
                static EventProcesser instance{};
                return instance;
            }

        public:
            EventProcesser(const EventProcesser&) = delete;
            EventProcesser& operator=(const EventProcesser&) = delete;
            EventProcesser(EventProcesser&&) = delete;
            EventProcesser& operator=(EventProcesser&&) = delete;
        };

        template <typename T>
        struct EventWiper {
            static void wipe_events() {
                using proc = EventProcesser<T>;
                proc::instance().event_objects.clear();
            }
        };
    } // namespace impl

    using EventClearer = void (*)();

    class EventManager {
    public:
        template <typename T>
        void register_event(const Schedule sched) {
            auto& registered_clearers = this->m_registered_clearers[sched];
            auto& clearers = this->m_clear[sched];

            constexpr static auto hash = type_hash<T>();
            if (registered_clearers.contains(hash)) {
                return;
            }
            registered_clearers.insert(hash);
            clearers.emplace_back(&impl::EventWiper<T>::wipe_events);
        }

        void clear_schedule(Schedule sched) const;

    private:
        SchedulesStorage<std::set<TypeHash>> m_registered_clearers{};
        SchedulesStorage<std::vector<EventClearer>> m_clear{};
    };

    template <typename T>
        requires (!std::is_const_v<T>)
    class EventReader {
    public:
        using event_type = T;
        EventReader() : m_processer(&impl::EventProcesser<T>::instance()) {
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_processer->event_objects.empty();
        }

        class Iterator {
        public:
            using it_ret = std::tuple<bool&, const T&>;
            using iterator_category = std::forward_iterator_tag;
            using value_type = it_ret;
            using difference_type = std::ptrdiff_t;
            using pointer = it_ret*;
            using reference = it_ret&;

            Iterator(impl::WrapperEvent<T>* curr, impl::WrapperEvent<T>* end)
                : m_current(curr), m_end(end) {
            }

            it_ret operator*() const {
                ORB_ASSERT(m_current != m_end);
                return std::forward_as_tuple(
                    this->m_current->canceled, this->m_current->storage
                );
            }

            Iterator& operator++() {
                do {
                    ++m_current;
                } while (m_current != m_end && m_current->canceled);

                return *this;
            }

            Iterator operator++(int) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const = default;
            bool operator!=(const Iterator& other) const = default;

        private:
            friend class EventReader;
            impl::WrapperEvent<T>* m_current{};
            impl::WrapperEvent<T>* m_end{};
        };

        Iterator begin() {
            auto* data = m_processer->event_objects.data();
            auto size = m_processer->event_objects.size();

            Iterator it{ data, data + size };

            if (it.m_current != it.m_end && it.m_current->canceled) ++it;

            return it;
        }

        Iterator end() {
            return Iterator{
                m_processer->event_objects.data() + m_processer->event_objects.size(),
                m_processer->event_objects.data() + m_processer->event_objects.size()
            };
        }

    private:
        impl::EventProcesser<T>* m_processer{};
    };

    template <typename T>
        requires(!std::is_const_v<T>)
    class EventWriter {
    public:
        using event_type = T;

        EventWriter() : m_processer(&impl::EventProcesser<T>::instance()) {
        }

        template <typename... Args>
        void queue_event(Args&&... args) {
            this->m_processer->event_objects.emplace_back(
                T{ std::forward<Args>(args)... }
            );
        }

    private:
        impl::EventProcesser<T>* m_processer{};
    };

    template <typename>
    struct is_event_handler : std::false_type {};

    template <typename T>
    struct is_event_handler<EventReader<T>> : std::true_type {};

    template <typename T>
    struct is_event_handler<EventWriter<T>> : std::true_type {};

    template <typename T>
    constexpr static bool is_event_handler_v = is_event_handler<T>::value;

} // namespace orb
