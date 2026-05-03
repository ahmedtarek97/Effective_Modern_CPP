// Item 36: Specify std::launch::async if asynchronicity is essential
//
// Key Points:
//  1. std::async has two explicit launch policies:
//       • std::launch::async   — task MUST run asynchronously on a new thread.
//       • std::launch::deferred — task is deferred until get() / wait() is called
//         on the returned future; it runs SYNCHRONOUSLY on that calling thread.
//  2. The DEFAULT policy is (std::launch::async | std::launch::deferred), giving
//     the implementation the freedom to choose either.  This means you cannot
//     rely on the task running concurrently, or at all if get()/wait() is never
//     called.
//  3. Thread-local storage (TLS) problem: with the default policy you cannot
//     predict which thread's TLS is used by the async task.
//  4. Timeout-loop hazard: a loop that calls wait_for(0s) to poll completion can
//     spin FOREVER if the implementation chose deferred, because the future
//     status is always std::future_status::deferred — never "ready".
//  5. Fix: specify std::launch::async explicitly when asynchrony is required.
//  6. Utility idiom: write a variadic reallyAsync() that always uses
//     std::launch::async, providing the cleaner default many codebases need.

#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <type_traits>   // std::result_of / std::invoke_result
#include <utility>       // std::forward

using namespace std::chrono_literals;

// ============================================================================
// Point 1 — The two explicit launch policies
// ============================================================================

void demo_explicit_policies()
{
    std::cout << "\n--- Point 1: Explicit launch policies ---\n";

    // std::launch::async: guaranteed to run on a separate thread.
    auto fut_async = std::async(std::launch::async, [] {
        std::cout << "  [async]    running on thread "
                  << std::this_thread::get_id() << '\n';
        return 42;
    });

    // std::launch::deferred: runs synchronously when get() is called below.
    auto fut_deferred = std::async(std::launch::deferred, [] {
        std::cout << "  [deferred] running on thread "
                  << std::this_thread::get_id() << '\n';
        return 99;
    });

    std::cout << "  Caller thread: " << std::this_thread::get_id() << '\n';

    // The async task may already be running here.
    // The deferred task hasn't started yet — it only starts on get().
    std::cout << "  Calling get() on deferred future now:\n";
    int d = fut_deferred.get();   // deferred task runs right here, same thread
    int a = fut_async.get();

    std::cout << "  async result   = " << a << '\n';
    std::cout << "  deferred result= " << d << '\n';

    // Key observation: with deferred, the lambda ran on the CALLER's thread,
    // not on a new thread.  With async, it ran on a different thread.
}

// ============================================================================
// Point 2 — Default policy: implementation may choose either
// ============================================================================

void demo_default_policy_ambiguity()
{
    std::cout << "\n--- Point 2: Default policy ambiguity ---\n";

    // No policy argument — implementation picks async OR deferred.
    auto fut = std::async([] {
        std::this_thread::sleep_for(50ms);
        return std::string("done");
    });

    // We cannot know whether this task is ALREADY running.
    // We cannot know whether it runs on another thread.
    // We cannot predict when (or if) it will complete before get().

    // The only safe way to retrieve the result is to just call get():
    std::string result = fut.get();
    std::cout << "  result = " << result << '\n';
    std::cout << "  (Whether it ran concurrently is implementation-defined)\n";
}

// ============================================================================
// Point 3 — TLS (thread_local) hazard with default policy
// ============================================================================

// Each software thread has its own "threadName" via thread_local storage.
thread_local std::string threadName = "main";

void demo_tls_hazard()
{
    std::cout << "\n--- Point 3: TLS hazard with default policy ---\n";

    threadName = "main-thread";

    // With std::launch::async the task gets its OWN TLS copy (a new thread).
    auto fut_a = std::async(std::launch::async, [] {
        // This is a DIFFERENT thread_local — independent of main's copy.
        std::cout << "  [async]    threadName TLS = '" << threadName
                  << "' (fresh, not 'main-thread')\n";
    });
    fut_a.get();

    // With std::launch::deferred the task inherits the SAME thread's TLS as
    // wherever get() is called — here that is "main-thread".
    auto fut_d = std::async(std::launch::deferred, [] {
        std::cout << "  [deferred] threadName TLS = '" << threadName
                  << "' (same thread as caller, so it sees 'main-thread')\n";
    });
    fut_d.get();

    // With the default policy you cannot know which behaviour you get.
    // If your task reads/writes TLS, use an explicit policy.
}

// ============================================================================
// Point 4 — Timeout-loop hazard with default policy
// ============================================================================

void demo_timeout_loop_hazard()
{
    std::cout << "\n--- Point 4: Timeout-loop hazard ---\n";

    // ----------------------------------------------------------------
    // BROKEN pattern (commented out to avoid infinite loop in the demo):
    // If the implementation chose deferred, fut.wait_for(0s) always returns
    // std::future_status::deferred — NEVER std::future_status::ready —
    // so the loop below would spin forever.
    //
    //   auto fut = std::async([] { doWork(); });   // default policy
    //   while (fut.wait_for(0s) != std::future_status::ready) {
    //       doOtherStuff();    // <-- infinite loop if deferred!
    //   }
    // ----------------------------------------------------------------

    // CORRECT: detect deferred status and handle it explicitly.
    auto fut = std::async([] {          // default policy (could be deferred)
        std::this_thread::sleep_for(30ms);
        return 7;
    });

    if (fut.wait_for(0s) == std::future_status::deferred) {
        // The task hasn't started.  Force it to complete synchronously.
        std::cout << "  Task was deferred — calling get() synchronously.\n";
        std::cout << "  result = " << fut.get() << '\n';
    } else {
        // Task is running (or already finished) — safe to poll.
        while (fut.wait_for(5ms) != std::future_status::ready) {
            std::cout << "  Waiting for async task…\n";
        }
        std::cout << "  result = " << fut.get() << '\n';
    }

    // ----------------------------------------------------------------
    // BEST FIX: force async so wait_for is always meaningful:
    // ----------------------------------------------------------------
    auto fut2 = std::async(std::launch::async, [] {
        std::this_thread::sleep_for(30ms);
        return 7;
    });

    int polls = 0;
    // Safe: with launch::async status is never "deferred".
    while (fut2.wait_for(5ms) != std::future_status::ready) {
        ++polls;
    }
    std::cout << "  [async] result = " << fut2.get()
              << "  (polled " << polls << " time(s))\n";
}

// ============================================================================
// Point 5 — Specify std::launch::async explicitly when you need asynchrony
// ============================================================================

void demo_explicit_async()
{
    std::cout << "\n--- Point 5: Explicit std::launch::async ---\n";

    // Guarantees:
    //  • The lambda runs on a DIFFERENT thread — true concurrency.
    //  • TLS is independent (fresh thread).
    //  • wait_for / wait_until / wait are meaningful (never "deferred").
    //  • Task WILL run even if we never call get() —
    //    the future's destructor blocks until it finishes (for async futures).

    auto fut = std::async(std::launch::async, [] {
        std::cout << "  Running concurrently on thread "
                  << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(20ms);
        return std::string("concurrent result");
    });

    std::cout << "  Caller: " << std::this_thread::get_id()
              << " (doing other work while task runs)\n";

    std::cout << "  Result: " << fut.get() << '\n';
}

// ============================================================================
// Point 6 — reallyAsync: a utility wrapper that always uses launch::async
// ============================================================================

// C++14 variadic template — mirrors std::async but always launches asynchronously.
template<typename F, typename... Ts>
inline auto reallyAsync(F&& f, Ts&&... params)
{
    return std::async(std::launch::async,
                      std::forward<F>(f),
                      std::forward<Ts>(params)...);
}

int add(int a, int b) { return a + b; }

void demo_really_async()
{
    std::cout << "\n--- Point 6: reallyAsync() utility wrapper ---\n";

    // Free function
    auto f1 = reallyAsync(add, 10, 32);
    std::cout << "  reallyAsync(add, 10, 32) = " << f1.get() << '\n';

    // Lambda
    auto f2 = reallyAsync([] { return std::string("hello from reallyAsync"); });
    std::cout << "  reallyAsync(lambda)      = " << f2.get() << '\n';

    // reallyAsync always uses launch::async so:
    //  • no ambiguity about when/where the task runs
    //  • safe to use in timeout loops without checking for deferred status
    //  • predictable TLS behaviour (fresh thread, independent TLS)

    auto f3 = reallyAsync([] {
        std::this_thread::sleep_for(10ms);
        return 99;
    });

    // Safe to poll — will never be "deferred"
    int polls = 0;
    while (f3.wait_for(1ms) != std::future_status::ready) ++polls;
    std::cout << "  reallyAsync poll result  = " << f3.get()
              << "  (polls: " << polls << ")\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "=== Item 36: Specify std::launch::async if asynchronicity is essential ===\n";

    demo_explicit_policies();
    demo_default_policy_ambiguity();
    demo_tls_hazard();
    demo_timeout_loop_hazard();
    demo_explicit_async();
    demo_really_async();

    std::cout << "\nDone.\n";
    return 0;
}
