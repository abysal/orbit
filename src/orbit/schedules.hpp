#pragma once
#include <array>
namespace orb {
    enum Schedule {
        FixedUpdate,
        Startup,


        COUNT_DO_NOT_USE_PLEASE
    };

    template<typename T>
    using SchedulesStorage = std::array<T, COUNT_DO_NOT_USE_PLEASE>;
}