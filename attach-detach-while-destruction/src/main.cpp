#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <ranges>
#include <thread>

#include <graal_isolate.h>

graal_isolate_t *graalIsolateHandle{};
graal_isolatethread_t *graalIsolateThreadHandle{};

thread_local std::mutex graalCurrentThreadHandleMutex{};
thread_local graal_isolatethread_t *graalCurrentThreadHandle{};

auto createIsolateResult = graal_create_isolate(nullptr, &graalIsolateHandle, &graalIsolateThreadHandle);

std::mutex ioMutex{};

template <typename F>
auto tryRunIsolated(F &&f /* (graal_isolatethread_t * currentThreadHandle) -> auto */)
    -> std::invoke_result_t<F &&, graal_isolatethread_t *> {
    if (createIsolateResult != 0) {
        if constexpr (std::is_same_v<std::invoke_result_t<F &&, graal_isolatethread_t *>, void>) {
            return;
        } else {
            return {};
        }
    }

    // Perhaps the code is already running within the GraalVM thread (for example, we are in a listener)
    if (graal_isolatethread_t *currentThreadHandle = graal_get_current_thread(graalIsolateHandle);
        currentThreadHandle != nullptr) {

        {
            std::lock_guard l{ioMutex};
            std::cerr << "tryRunIsolated(): graal_get_current_thread() = " << currentThreadHandle << std::endl;
        }

        return std::invoke(std::forward<F>(f), currentThreadHandle);
    }

    // Already attached
    if (graalCurrentThreadHandle) {
        {
            std::lock_guard l{ioMutex};
            std::cerr << "tryRunIsolated(): graalCurrentThreadHandle = " << graalCurrentThreadHandle << std::endl;
        }

        return std::invoke(std::forward<F>(f), graalCurrentThreadHandle);
    }

    graal_isolatethread_t *currentThreadHandle{};
    auto attachResult = graal_attach_thread(graalIsolateHandle, &currentThreadHandle);

    {
        std::lock_guard l{ioMutex};
        std::cerr << "tryRunIsolated(): graal_attach_thread = " << currentThreadHandle << std::endl;
    }

    if (attachResult != 0) {
        if constexpr (std::is_same_v<std::invoke_result_t<F &&, graal_isolatethread_t *>, void>) {
            return;
        } else {
            return {};
        }
    }

    {
        std::lock_guard l{graalCurrentThreadHandleMutex};
        graalCurrentThreadHandle = currentThreadHandle;
    }

    return std::invoke(std::forward<F>(f), graalCurrentThreadHandle);
}

void detachThead() {
    {
        std::lock_guard l{ioMutex};
        std::cerr << "detachThread()" << std::endl;
        std::cerr << "detachThread(): graalCurrentThreadHandle = " << graalCurrentThreadHandle
                  << ", graalIsolateThreadHandle = " << graalIsolateThreadHandle << std::endl;
    }

    {
        std::lock_guard l{graalCurrentThreadHandleMutex};
        if (graalCurrentThreadHandle == nullptr || graalCurrentThreadHandle == graalIsolateThreadHandle) {
            return;
        }
    }

    auto result = graal_detach_thread(graalCurrentThreadHandle);

    {
        std::lock_guard l{ioMutex};
        std::cerr << "detachThread(): result = " << result << std::endl;
    }

    if (result == 0) {
        {
            std::lock_guard l{graalCurrentThreadHandleMutex};

            // Comment this line to break the graal
            graalCurrentThreadHandle = nullptr;
        }
    }
}

class Test {
  public:
    Test() {
        {
            std::lock_guard l{ioMutex};
            std::cerr << "Test() " << std::this_thread::get_id() << std::endl;
        }

        tryRunIsolated([](auto graalThreadHandle) {
            {
                std::lock_guard l{ioMutex};
                std::cerr << "Test() 'isolated' " << std::this_thread::get_id() << " " << graalThreadHandle
                          << std::endl;
            }
        });
    }

    ~Test() {
        {
            std::lock_guard l{ioMutex};
            std::cerr << "~Test() " << std::this_thread::get_id() << std::endl;
        }

        tryRunIsolated([](auto graalThreadHandle) {
            {
                std::lock_guard l{ioMutex};
                std::cerr << "Test() 'isolated' " << std::this_thread::get_id() << " " << graalThreadHandle
                          << std::endl;
            }
        });

        detachThead();
    }

    void print() {
        {
            std::lock_guard l{ioMutex};
            std::cerr << "print() " << std::this_thread::get_id() << std::endl;
        }

        tryRunIsolated([](auto graalThreadHandle) {
            {
                std::lock_guard l{ioMutex};
                std::cerr << "print() 'isolated' " << std::this_thread::get_id() << " " << graalThreadHandle
                          << std::endl;
            }
        });
    }
};

struct Detacher {
    ~Detacher() {
        {
            std::lock_guard l{ioMutex};
            std::cerr << "~Detacher() " << std::this_thread::get_id() << std::endl;
        }

        detachThead();
    }
};

thread_local Test test{};
thread_local Detacher detacher{};

using namespace std::literals;

int main() {
    if (createIsolateResult != 0) {
        return createIsolateResult;
    }

    std::vector<std::unique_ptr<std::thread>> threads{};

    threads.reserve(16);

    for (auto i = 0; i < threads.capacity(); i++) {
        threads.emplace_back(new std::thread{[] {
            test.print();
        }});
    }

    for (auto &thread : std::ranges::reverse_view(threads)) {
        thread->join();
    }

    std::this_thread::sleep_for(30s);
}