// Item 39: Consider void futures for one-shot event communication
//
// Key Points:
//  1. The classic condition-variable (condvar) approach for task communication
//     has subtle but real problems: it requires a mutex even when not logically
//     needed, is susceptible to spurious wakeups, and can suffer from a "lost
//     signal" if the detecting task fires before the reacting task waits.
//  2. A shared atomic/flag (polling) approach avoids some condvar issues but
//     wastes CPU cycles (busy-wait) or introduces latency (sleep loop).
//  3. Combining a condvar with a flag fixes spurious wakeups and lost signals,
//     but still burdens the design with an unnecessary mutex.
//  4. A std::promise<void>/std::future<void> pair provides clean one-shot
//     event communication: no mutex, no spurious wakeups, no lost signal,
//     genuinely blocking.
//  5. The void-future channel is one-shot only — the promise cannot be set
//     again after set_value().  For repeated signaling use a condvar.
//  6. std::shared_future lets multiple reacting tasks each block on the same
//     one-shot event without any additional synchronization.

#include <iostream>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <vector>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static long long elapsed_ms(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 1a: Condition-variable approach — "lost signal" problem
//
// If the detecting task calls notify_one() BEFORE the reacting task has
// entered wait(), the notification is lost and the reactor blocks forever
// (or until a spurious wakeup occurs).
//
// Demonstration: we artificially guarantee the detects fires first.
// ─────────────────────────────────────────────────────────────────────────────
void point1a_condvar_lost_signal_demo()
{
    std::cout << "=== Point 1a: condvar — lost-signal problem ===\n";

    std::mutex mtx;
    std::condition_variable cv;
    bool notified = false; // we add a flag to rescue the demo; without it the
                           // reactor would block forever — see point 3 for why

    // Detecting task: fires immediately and notifies BEFORE the reactor waits.
    std::thread detector([&]
    {
        sleep_ms(0); // no delay — fires before reactor is ready
        {
            std::lock_guard<std::mutex> lk(mtx);
            notified = true;
        }
        cv.notify_one();
        std::cout << "  [detector] event fired (notified).\n";
    });

    // Reacting task: waits a bit, then tries to wait on condvar.
    // Without the boolean predicate this would deadlock in a real scenario.
    std::thread reactor([&]
    {
        sleep_ms(40); // detector has already notified by now
        std::unique_lock<std::mutex> lk(mtx);
        // With a predicate the wait returns immediately because notified==true.
        // WITHOUT the predicate (plain cv.wait(lk)) the wait would block
        // indefinitely since the notification was already consumed.
        cv.wait(lk, [&]{ return notified; });
        std::cout << "  [reactor] unblocked (flag saved us from lost signal).\n";
    });

    detector.join();
    reactor.join();

    std::cout << "  NOTE: a bare cv.wait() with no predicate would have\n"
              << "        deadlocked because the signal arrived before wait().\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 1b: Condition-variable approach — spurious wakeup problem
//
// POSIX allows condition variables to wake up spontaneously without any
// notify() call.  Without a predicate the reactor acts on spurious wakeups.
//
// The item notes that callers MUST supply a predicate to guard against this,
// which in turn mandates a mutex — even when the mutex is not otherwise needed.
// ─────────────────────────────────────────────────────────────────────────────
void point1b_condvar_spurious_wakeup_explained()
{
    std::cout << "=== Point 1b: condvar — spurious wakeup (explanation) ===\n";

    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;

    // Correct usage — predicate guards against spurious wakeups:
    //
    //   cv.wait(lk, [&]{ return ready; });
    //
    // This is equivalent to:
    //
    //   while (!ready) cv.wait(lk);
    //
    // Without the predicate, a spurious wakeup could make the reactor proceed
    // even though the detecting task has not set 'ready'.

    std::thread detector([&]
    {
        sleep_ms(30);
        {
            std::lock_guard<std::mutex> lk(mtx);
            ready = true;
        }
        cv.notify_one();
        std::cout << "  [detector] signalled (ready = true).\n";
    });

    std::thread reactor([&]
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&]{ return ready; }); // predicate prevents acting on spurious wakeup
        std::cout << "  [reactor] unblocked — ready is genuinely true.\n";
    });

    detector.join();
    reactor.join();
    std::cout << "  Predicate-guarded wait is correct, but forces a mutex on the design.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 2: Shared-flag (polling) approach — works but wastes CPU
//
// A std::atomic<bool> is simpler: no mutex, no spurious wakeups, no lost
// signal.  However, the reactor must spin (poll) or sleep-and-poll, which
// either burns CPU or adds latency.  Neither is ideal.
// ─────────────────────────────────────────────────────────────────────────────
void point2_shared_flag_polling()
{
    std::cout << "=== Point 2: shared atomic flag — polling approach ===\n";

    std::atomic<bool> eventOccurred{false};

    std::thread detector([&]
    {
        sleep_ms(50);
        eventOccurred.store(true, std::memory_order_release);
        std::cout << "  [detector] flag set.\n";
    });

    // Reactor: busy-wait (or sleep-poll).
    // Busy-wait version shown; a sleep inside the loop would reduce CPU usage
    // but add latency proportional to the sleep interval.
    std::thread reactor([&]
    {
        int spins = 0;
        while (!eventOccurred.load(std::memory_order_acquire))
        {
            ++spins;
            // In a real "sleep-poll" variant:  sleep_ms(1);
        }
        std::cout << "  [reactor] flag observed (spun " << spins
                  << " times — CPU wasted).\n";
    });

    detector.join();
    reactor.join();
    std::cout << "  Polling works but is wasteful; a busy-wait burns a core.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 3: Combined condvar + flag — fixes lost signal & spurious wakeup
//
// Adding a boolean flag to the condvar design solves both pathologies:
//   • Lost signal: reactor checks the flag under the lock; if already set,
//     wait() returns immediately.
//   • Spurious wakeup: predicate [&]{ return flag; } re-checks after wakeup.
//
// This is the correct condvar idiom, but it still requires a mutex even
// when the logical design doesn't need one — a minor conceptual overhead.
// ─────────────────────────────────────────────────────────────────────────────
void point3_combined_condvar_flag()
{
    std::cout << "=== Point 3: condvar + flag — correct but mutex still required ===\n";

    std::mutex mtx;
    std::condition_variable cv;
    bool flag = false;

    // Run 3 trials: (a) normal order, (b) detector fires first (lost-signal
    // scenario), (c) simulate by letting detector go a second time.

    auto make_reactor = [&]
    {
        return std::thread([&]
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [&]{ return flag; }); // correct: handles both pathologies
            std::cout << "  [reactor] unblocked.\n";
        });
    };

    auto make_detector = [&]
    {
        return std::thread([&]
        {
            sleep_ms(20);
            {
                std::lock_guard<std::mutex> lk(mtx);
                flag = true;
            }
            cv.notify_one();
            std::cout << "  [detector] signalled.\n";
        });
    };

    // Trial: normal order
    {
        std::lock_guard<std::mutex> lk(mtx); // reset flag for this trial
        flag = false;
    }
    auto r = make_reactor();
    auto d = make_detector();
    d.join(); r.join();

    std::cout << "  Works correctly, but the mutex is a conceptual overhead\n"
              << "  when the protected state is just the event's occurrence.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 4: The void-future approach — clean one-shot event communication
//
// std::promise<void> + std::future<void> gives:
//   • No mutex needed.
//   • No spurious wakeups (set_value() is idempotent from the reactor's POV).
//   • No lost signal: the shared state records that set_value() was called;
//     future::wait() returns immediately if the value has already been set.
//   • Genuinely blocks (no busy-wait).
//
// The detecting task calls p.set_value(); the reacting task calls f.wait()
// (or f.get() — same effect for void).
// ─────────────────────────────────────────────────────────────────────────────
void point4_void_future_clean_approach()
{
    std::cout << "=== Point 4: void promise/future — clean one-shot communication ===\n";

    // ── 4a: Normal order (react waits before detect fires) ──
    {
        std::cout << "  [4a] reactor waits, then detector fires:\n";
        std::promise<void> p;
        std::future<void> f = p.get_future();

        std::thread detector([&p]
        {
            sleep_ms(60);
            p.set_value(); // signal the event
            std::cout << "    [detector] set_value() called.\n";
        });

        auto start = std::chrono::steady_clock::now();
        f.wait(); // blocks until set_value() is called
        std::cout << "    [reactor] unblocked after ~" << elapsed_ms(start) << " ms.\n";

        detector.join();
    }

    // ── 4b: Detector fires first (would be "lost signal" with bare condvar) ──
    {
        std::cout << "  [4b] detector fires BEFORE reactor waits:\n";
        std::promise<void> p;
        std::future<void> f = p.get_future();

        // Set value immediately — before the reactor ever calls wait().
        p.set_value();
        std::cout << "    [detector] set_value() called immediately.\n";

        auto start = std::chrono::steady_clock::now();
        f.wait(); // returns immediately — shared state already has the value
        std::cout << "    [reactor] returned from wait() in ~" << elapsed_ms(start)
                  << " ms (no lost-signal, no deadlock).\n";
    }

    // ── 4c: No mutex required ──
    std::cout << "  [4c] no mutex was needed anywhere in the above — by design.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 5: One-shot limitation of the void-future channel
//
// std::promise::set_value() may be called at most once.  A second call throws
// std::future_error with std::future_errc::promise_already_satisfied.
// For repeated event signaling a condvar (or a semaphore, or a channel) is
// more appropriate.
// ─────────────────────────────────────────────────────────────────────────────
void point5_one_shot_limitation()
{
    std::cout << "=== Point 5: void-future is one-shot only ===\n";

    std::promise<void> p;
    std::future<void> f = p.get_future();

    p.set_value(); // first signal — OK
    std::cout << "  First set_value() succeeded.\n";

    // A second set_value() would throw std::future_error.
    try
    {
        p.set_value(); // illegal: promise already satisfied
        std::cout << "  Second set_value() — should NOT reach here.\n";
    }
    catch (const std::future_error& e)
    {
        std::cout << "  Second set_value() threw std::future_error: "
                  << e.what() << "\n";
        assert(e.code() == std::future_errc::promise_already_satisfied);
    }

    std::cout << "  For repeated events, prefer std::condition_variable.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 6a: std::shared_future — broadcasting to multiple reactors
//
// std::future is move-only and can only be waited on by one consumer.
// std::shared_future is copyable: distribute one copy to each reacting task.
// All copies refer to the same shared state; all unblock when set_value() fires.
// ─────────────────────────────────────────────────────────────────────────────
void point6_shared_future_multiple_reactors()
{
    std::cout << "=== Point 6: shared_future — multiple reactors, one event ===\n";

    std::promise<void> p;
    // Convert the unique future to a shared_future once so we can copy it.
    std::shared_future<void> sf = p.get_future().share();

    const int N = 4;
    std::mutex print_mtx;
    std::vector<std::thread> reactors;

    for (int i = 0; i < N; ++i)
    {
        // Each reactor gets its own copy of the shared_future.
        reactors.emplace_back([sf, i, &print_mtx]  // sf copied by value
        {
            sf.wait(); // all N threads block here until set_value()
            std::lock_guard<std::mutex> lk(print_mtx);
            std::cout << "  [reactor " << i << "] unblocked.\n";
        });
    }

    std::cout << "  All " << N << " reactors are waiting...\n";
    sleep_ms(60);
    p.set_value(); // broadcasts to ALL reactors simultaneously
    std::cout << "  [detector] set_value() fired — all reactors released:\n";

    for (auto& t : reactors) t.join();
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 6b: Suspend-then-run pattern using void future
//
// A common idiom is to suspend a freshly created thread at a known point and
// then release it (or all of them) precisely when the caller is ready — like a
// starting gun.  The void-future approach is ideal here.
// ─────────────────────────────────────────────────────────────────────────────
void point6b_suspend_then_run_starting_gun()
{
    std::cout << "=== Point 6b: suspend-then-run (starting-gun) pattern ===\n";

    std::promise<void> ready_promise;
    std::shared_future<void> ready_sf = ready_promise.get_future().share();

    const int N = 3;
    std::vector<std::future<void>> results;
    std::mutex print_mtx;

    for (int i = 0; i < N; ++i)
    {
        results.push_back(std::async(std::launch::async,
            [ready_sf, i, &print_mtx]
            {
                ready_sf.wait(); // suspend here until the "gun fires"
                {
                    std::lock_guard<std::mutex> lk(print_mtx);
                    std::cout << "  [worker " << i << "] started simultaneously.\n";
                }
            }));
    }

    // Do preparatory work here while all workers are suspended...
    sleep_ms(50);
    std::cout << "  [main] preparation done — firing starting gun.\n";
    ready_promise.set_value(); // release all workers at the same time

    for (auto& f : results) f.wait();
    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Summary comparison table (printed to stdout)
// ─────────────────────────────────────────────────────────────────────────────
void print_summary()
{
    std::cout << "=== Summary: comparing one-shot communication approaches ===\n";
    std::cout << R"(
  Approach               | Mutex needed | Spurious wakeup | Lost signal | Busy-wait
  ─────────────────────────────────────────────────────────────────────────────────
  condvar (no flag)      |     YES      |      YES        |     YES     |    No
  condvar + flag         |     YES      |      No         |     No      |    No
  atomic flag (poll)     |     No       |      No         |     No      |   YES
  void promise/future    |     No       |      No         |     No      |    No
  ─────────────────────────────────────────────────────────────────────────────────
  void promise/future is the cleanest for one-shot events.
  Use condvar for repeatable signals; shared_future for broadcast.
)" << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    point1a_condvar_lost_signal_demo();
    point1b_condvar_spurious_wakeup_explained();
    point2_shared_flag_polling();
    point3_combined_condvar_flag();
    point4_void_future_clean_approach();
    point5_one_shot_limitation();
    point6_shared_future_multiple_reactors();
    point6b_suspend_then_run_starting_gun();
    print_summary();

    return 0;
}
