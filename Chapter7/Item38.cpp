// Item 38: Be aware of varying thread handle destructor behavior
//
// Key Points:
//  1. Both std::thread and std::future are "thread handles" — but their
//     destructor behaviors differ fundamentally.
//  2. std::thread: destructor calls std::terminate() if the thread is joinable.
//  3. std::future: the result lives in a "shared state" on the heap, separate
//     from both the caller (future) and the callee (promise/packaged_task/async).
//  4. A future's destructor normally does NOT block — it merely destroys the
//     future object (detach-like behavior).
//  5. EXCEPTION — a future's destructor BLOCKS (join-like) if ALL three
//     conditions hold:
//       a) The shared state was created by std::async.
//       b) The task's launch policy was (or could have been) std::launch::async.
//       c) The future is the LAST future referring to that shared state.
//  6. Futures from std::packaged_task do NOT exhibit the blocking destructor.
//  7. Futures from std::promise do NOT exhibit the blocking destructor.
//  8. A std::shared_future also participates: only the very last shared_future
//     (or an std::future that is the lone owner) triggers the block.

#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

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
        std::chrono::steady_clock::now() - start).count();
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 1 & 2: std::thread vs std::future — different "thread handle" rules
//
// std::thread's destructor calls std::terminate() when the thread is joinable,
// which makes it an explicit ownership model: you MUST join or detach.
//
// std::future's destructor, by contrast, normally does nothing dramatic —
// the underlying async work is either still running (and will continue), or
// has already finished.  The future is merely a window into the shared state,
// not an owner of the running thread.
// ─────────────────────────────────────────────────────────────────────────────
void point1_2_thread_vs_future_handles()
{
    std::cout << "=== Points 1 & 2: std::thread vs std::future as thread handles ===\n";

    // --- std::thread ---
    // Must join or detach before the destructor is called.
    {
        std::thread t([] { sleep_ms(20); });
        t.join(); // explicit ownership discharge
    }
    std::cout << "  std::thread: joined before destructor — OK.\n";

    // --- std::future from std::promise ---
    // The future is destroyed here without calling .get().
    // The shared state still holds the (already-set) value, but no blocking
    // and no std::terminate() occurs.
    {
        std::promise<int> p;
        std::future<int> f = p.get_future();
        p.set_value(42);
        // f goes out of scope without .get() — perfectly fine, no crash, no block.
    }
    std::cout << "  std::future (promise): destroyed without .get() — no crash, no block.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 3: The shared state lives on the heap between caller and callee
//
// When you call std::async (or use std::promise / std::packaged_task), a
// "shared state" object is allocated on the heap.  The future holds one
// reference to it; the callee (the running thread) holds another.  Neither
// side owns the thread directly through the future — the thread is managed
// by the C++ runtime and keeps the shared state alive as long as needed.
//
//   ┌───────────┐      ┌──────────────────────┐      ┌────────────────┐
//   │  caller   │─────▶│    shared state      │◀─────│  callee thread │
//   │ (future)  │      │  (result, exception, │      │ (sets result)  │
//   └───────────┘      │   ref-count, …)      │      └────────────────┘
//                      └──────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
void point3_shared_state_model()
{
    std::cout << "=== Point 3: shared state model ===\n";

    // The shared state is kept alive as long as either the future
    // or the underlying async provider is alive.
    std::future<std::string> f = std::async(std::launch::async, []
    {
        sleep_ms(30);
        return std::string("result from callee");
    });

    // The future is a lightweight handle — the actual result lives in the
    // heap-allocated shared state, not inside the future itself.
    std::cout << "  shared state result: " << f.get() << "\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 4: Normal future destructor behavior — NON-blocking (detach-like)
//
// A future created from std::promise or std::packaged_task does NOT block in
// its destructor, even if the underlying work has not finished.  The work
// runs to completion on its own; the future simply releases its reference to
// the shared state.
// ─────────────────────────────────────────────────────────────────────────────
void point4_normal_future_destructor_nonblocking()
{
    std::cout << "=== Point 4: Normal future destructor — non-blocking ===\n";

    // --- Using std::packaged_task ---
    {
        std::packaged_task<int()> task([]
        {
            sleep_ms(80);
            return 99;
        });

        std::future<int> f = task.get_future();

        // Launch the task on a separate thread.
        std::thread t(std::move(task));
        t.detach(); // we intentionally don't join — the thread manages itself

        auto start = std::chrono::steady_clock::now();
        // Destroy f immediately — does NOT block, even though the thread is
        // still running for ~80 ms.
        f = std::future<int>{}; // explicit reset (same as going out of scope)

        std::cout << "  packaged_task future destroyed in ~" << elapsed_ms(start)
                  << " ms (expected ~0 ms, no blocking).\n";
        // Let the detached thread finish before we leave (just for clean output).
        sleep_ms(100);
    }

    // --- Using std::promise ---
    {
        std::promise<int> prom;
        std::future<int> f = prom.get_future();

        std::thread t([p = std::move(prom)]() mutable
        {
            sleep_ms(80);
            p.set_value(7);
        });
        t.detach();

        auto start = std::chrono::steady_clock::now();
        f = std::future<int>{}; // destroy immediately

        std::cout << "  promise future destroyed in ~" << elapsed_ms(start)
                  << " ms (expected ~0 ms, no blocking).\n";
        sleep_ms(100);
    }

    std::cout << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 5: The EXCEPTIONAL case — std::async future destructor BLOCKS
//
// The Standard mandates blocking when ALL three conditions hold:
//   a) Shared state was created by std::async.
//   b) Launch policy is (or could have been) std::launch::async.
//   c) The future is the LAST future referring to that shared state.
//
// In practice this means that a local std::future returned by std::async with
// (or defaulting to) the async policy will BLOCK in its destructor until the
// associated thread finishes — behaving like an implicit join.
// ─────────────────────────────────────────────────────────────────────────────
void point5_async_future_destructor_blocks()
{
    std::cout << "=== Point 5: std::async (async policy) future destructor BLOCKS ===\n";

    auto start = std::chrono::steady_clock::now();

    {
        // Explicit std::launch::async — condition (b) is clearly satisfied.
        std::future<void> f = std::async(std::launch::async, []
        {
            sleep_ms(120);
            std::cout << "  [async task] finished.\n";
        });

        // f is the only (last) future for this shared state — condition (c).
        // Shared state was created by std::async — condition (a).
        // All three conditions satisfied → destructor of f will BLOCK ~120 ms.
        std::cout << "  Exiting inner scope (f's destructor will block) ...\n";
    } // <--- blocks here until the async task finishes

    std::cout << "  Scope exited after ~" << elapsed_ms(start)
              << " ms (blocked ~120 ms as expected).\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 5b: Default launch policy may also trigger blocking
//
// std::async(f) without an explicit policy uses std::launch::async |
// std::launch::deferred.  The implementation is free to choose async.
// If it chooses async, condition (b) is satisfied and the destructor blocks.
// If it chooses deferred, the task hasn't run yet; the destructor runs the
// task synchronously and then returns, still appearing to "block" the caller.
// Either way, the default is NOT "fire and forget".
// ─────────────────────────────────────────────────────────────────────────────
void point5b_default_policy_also_blocks()
{
    std::cout << "=== Point 5b: Default launch policy — also effectively blocking ===\n";

    auto start = std::chrono::steady_clock::now();

    {
        // No explicit policy — implementation picks async or deferred.
        std::future<void> f = std::async([]
        {
            sleep_ms(80);
            std::cout << "  [default-policy task] finished.\n";
        });
        // Either path (async or deferred) causes the destructor to wait.
    }

    std::cout << "  Exited after ~" << elapsed_ms(start) << " ms.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 6: std::packaged_task future destructor does NOT block
//
// std::packaged_task creates a shared state, but NOT through std::async.
// Condition (a) fails → the future destructor is always non-blocking,
// regardless of whether the underlying thread is still running.
// ─────────────────────────────────────────────────────────────────────────────
void point6_packaged_task_no_block()
{
    std::cout << "=== Point 6: std::packaged_task future destructor — no block ===\n";

    std::packaged_task<int()> task([]
    {
        sleep_ms(120);
        return 42;
    });

    std::future<int> f = task.get_future();
    std::thread t(std::move(task));
    t.detach(); // thread keeps running independently

    auto start = std::chrono::steady_clock::now();

    // Dropping f does NOT block even though the thread sleeps 120 ms.
    // Condition (a) is NOT met (shared state not from std::async).
    f = std::future<int>{};

    std::cout << "  packaged_task future dropped in ~" << elapsed_ms(start)
              << " ms (expected ~0, task still running in background).\n\n";

    sleep_ms(150); // let background thread finish before process exits
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 7: std::promise future destructor does NOT block
//
// Same reasoning: shared state was created by std::promise, not std::async.
// Condition (a) fails → non-blocking destructor.
// ─────────────────────────────────────────────────────────────────────────────
void point7_promise_no_block()
{
    std::cout << "=== Point 7: std::promise future destructor — no block ===\n";

    std::promise<int> prom;
    std::future<int> f = prom.get_future();

    std::thread t([p = std::move(prom)]() mutable
    {
        sleep_ms(120);
        p.set_value(100);
    });
    t.detach();

    auto start = std::chrono::steady_clock::now();
    f = std::future<int>{}; // non-blocking drop

    std::cout << "  promise future dropped in ~" << elapsed_ms(start)
              << " ms (expected ~0, task still running in background).\n\n";

    sleep_ms(150);
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 8: shared_future and the "last future" condition
//
// Condition (c): blocking only occurs for the LAST future referring to the
// shared state.  If you copy an std::future into an std::shared_future (or
// create multiple shared_futures from the same source), the blocking destructor
// is deferred to the very last shared_future that is destroyed.
// ─────────────────────────────────────────────────────────────────────────────
void point8_last_future_condition()
{
    std::cout << "=== Point 8: 'last future' condition with shared_future ===\n";

    // Create an async shared state and immediately share ownership.
    std::shared_future<int> sf1 = std::async(std::launch::async, []
    {
        sleep_ms(100);
        std::cout << "  [shared async task] finished.\n";
        return 7;
    }).share(); // .share() converts std::future -> std::shared_future

    // Make a second owner of the same shared state.
    std::shared_future<int> sf2 = sf1;

    auto start = std::chrono::steady_clock::now();

    {
        // Destroy sf1.  It is NOT the last owner (sf2 still exists).
        // → No blocking.
        std::shared_future<int> temp = sf1;
        // temp goes out of scope here: still not last (sf1, sf2 remain).
    }
    std::cout << "  sf1 copy destroyed in ~" << elapsed_ms(start)
              << " ms (not the last — no block).\n";

    // Destroy sf1 (one of the two remaining owners — sf2 still alive).
    // Resetting sf1 first:
    start = std::chrono::steady_clock::now();
    sf1 = std::shared_future<int>{}; // sf2 is now the sole owner
    std::cout << "  sf1 destroyed in ~" << elapsed_ms(start)
              << " ms (sf2 still alive — no block).\n";

    // Now destroy sf2 — it IS the last owner AND conditions (a) and (b) hold.
    start = std::chrono::steady_clock::now();
    sf2 = std::shared_future<int>{}; // blocks until the async task finishes
    std::cout << "  sf2 (last owner) destroyed in ~" << elapsed_ms(start)
              << " ms (blocked ~100 ms as expected).\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Practical summary: "fire and forget" via std::async is NOT truly fire and
// forget when you store the returned future.
//
// If you genuinely want fire-and-forget semantics you must ensure the future
// is not destroyed before the task completes — OR use a void future that you
// intentionally let block (which is actually often desirable for clean shutdown).
//
// Two common patterns:
//   a) Store futures in a container and wait for all of them explicitly.
//   b) Use deferred tasks when you want lazy evaluation.
// ─────────────────────────────────────────────────────────────────────────────
void practical_fire_and_forget_pattern()
{
    std::cout << "=== Practical: Collecting async futures to avoid surprise blocks ===\n";

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 4; ++i)
    {
        futures.push_back(std::async(std::launch::async, [i]
        {
            sleep_ms(30 * (i + 1));
            return i * i;
        }));
    }

    // All tasks run in parallel.  Collecting results (or just letting the
    // vector go out of scope) provides well-defined join semantics.
    int sum = 0;
    for (auto& f : futures)
        sum += f.get();

    std::cout << "  Sum of squares 0..3 = " << sum << " (expected 14).\n\n";
}

void practical_deferred_lazy_evaluation()
{
    std::cout << "=== Practical: Deferred (lazy) evaluation — no blocking destructor ===\n";

    std::future<int> f = std::async(std::launch::deferred, []
    {
        std::cout << "  [deferred task] running now (on .get() call).\n";
        return 55;
    });

    std::cout << "  Task not yet executed.\n";
    // The task only runs when we call .get() or .wait().
    // The destructor of a deferred future does NOT block (task never ran
    // asynchronously, so no thread to join).
    int v = f.get();
    std::cout << "  Result: " << v << "\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    point1_2_thread_vs_future_handles();
    point3_shared_state_model();
    point4_normal_future_destructor_nonblocking();
    point5_async_future_destructor_blocks();
    point5b_default_policy_also_blocks();
    point6_packaged_task_no_block();
    point7_promise_no_block();
    point8_last_future_condition();
    practical_fire_and_forget_pattern();
    practical_deferred_lazy_evaluation();

    return 0;
}
