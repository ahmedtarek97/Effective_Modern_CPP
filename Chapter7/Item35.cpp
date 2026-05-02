// Item 35: Prefer task-based programming to thread-based
//
// Key Points:
//  1. Thread-based programming requires manual thread lifecycle management
//     (join/detach before destruction) and offers no built-in way to retrieve
//     a return value or propagate exceptions to the caller.
//  2. Task-based programming (std::async) returns a std::future that gives
//     automatic access to the return value and propagates exceptions naturally.
//  3. Three levels of "threads": hardware threads (CPU cores), software threads
//     (OS-managed), and std::thread objects. Software threads are a limited
//     resource; exhaustion throws std::system_error even if the program is
//     logically correct.
//  4. Oversubscription — creating more std::threads than the hardware can run
//     simultaneously — causes expensive context-switching and degrades cache
//     locality. std::async sidesteps this by delegating scheduling to the runtime.
//  5. std::async (with std::launch::async) can use thread pools maintained by
//     the runtime, naturally achieving load balancing and avoiding oversubscription.
//  6. When to still prefer std::thread: (a) access to low-level platform thread
//     APIs (priority, affinity…); (b) threading technology the C++ concurrency
//     API does not model (e.g., custom thread pools); (c) a known, fixed execution
//     environment where you can hand-tune thread counts.

#include <iostream>
#include <future>        // std::async, std::future, std::launch
#include <thread>        // std::thread, std::this_thread
#include <stdexcept>     // std::runtime_error
#include <numeric>       // std::accumulate
#include <vector>
#include <chrono>
#include <cassert>
#include <mutex>

using namespace std::chrono_literals;

// ============================================================================
// Point 1 — Thread-based: manual lifecycle + no easy return value
// ============================================================================

// A pure computation we want to run concurrently.
int expensiveComputation(int n)
{
    // Simulate work.
    int result = 0;
    for (int i = 1; i <= n; ++i) result += i;
    return result;
}

void demo_thread_based()
{
    std::cout << "\n--- Point 1: Thread-based (manual lifecycle, no return value) ---\n";

    int result = 0;  // must share state explicitly — e.g. via a captured reference

    // std::thread: we have to (a) capture the output via shared state,
    //              (b) remember to join before the thread object is destroyed.
    std::thread t([&result]() {
        result = expensiveComputation(100);
    });

    // If we forget t.join() here, the std::thread destructor calls std::terminate().
    t.join();  // <-- mandatory; easy to forget or get wrong in the face of exceptions

    std::cout << "  thread result (via shared int): " << result << '\n';

    // Exception handling is also painful with std::thread:
    // An exception thrown inside the thread's function and not caught there
    // will call std::terminate(). There is no mechanism to propagate it to
    // the spawning thread automatically.
    std::cout << "  (exceptions thrown inside std::thread cannot propagate to caller)\n";
}

// ============================================================================
// Point 2 — Task-based: automatic lifecycle, return value, and exception
//            propagation via std::future
// ============================================================================

// A function that may throw.
int riskyComputation(int n)
{
    if (n < 0)
        throw std::runtime_error("negative input is not allowed");
    int result = 0;
    for (int i = 1; i <= n; ++i) result += i;
    return result;
}

void demo_task_based()
{
    std::cout << "\n--- Point 2: Task-based (std::async + std::future) ---\n";

    // std::async launches the task (possibly on a new thread or a thread pool).
    // It returns a std::future<int> — no explicit join, no shared state boilerplate.
    auto fut = std::async(expensiveComputation, 100);

    // Do other work here while the computation runs concurrently...
    std::cout << "  doing other work while computation runs...\n";

    // Retrieve the result (blocks until ready).
    int result = fut.get();
    std::cout << "  async result: " << result << '\n';

    // Exception propagation: if the task throws, fut.get() re-throws at the
    // call site, giving the caller full control.
    auto faultFut = std::async(std::launch::async, riskyComputation, -5);
    try {
        faultFut.get();  // re-throws std::runtime_error
    } catch (const std::exception& e) {
        std::cout << "  caught exception from async task: " << e.what() << '\n';
    }
}

// ============================================================================
// Point 3 — Three levels of threads: hardware, software (OS), std::thread
//
// Hardware threads are the executing units (CPU cores / hyperthreads).
// Software threads are OS-managed contexts scheduled onto hardware threads.
// std::thread is the C++ handle to a software thread.
//
// Software threads are a finite OS resource. If the system is saturated,
// std::thread construction throws std::system_error.
// ============================================================================

void demo_thread_levels()
{
    std::cout << "\n--- Point 3: Hardware vs software vs std::thread ---\n";

    unsigned hwThreads = std::thread::hardware_concurrency();
    std::cout << "  hardware_concurrency() hint: " << hwThreads << " hardware threads\n";

    // If we naively tried to spawn thousands of std::threads we could exhaust
    // OS resources. Demonstrate the concept safely by showing what would happen:
    std::cout << "  Creating more software threads than hardware threads causes\n"
              << "  context-switching overhead and can throw std::system_error\n"
              << "  if the OS runs out of thread handles.\n";

    // Safe illustration: attempt to track how many threads we could spawn.
    // (We keep this small to avoid actually exhausting resources.)
    std::vector<std::thread> threads;
    constexpr int kSafeCount = 8;
    std::mutex mtx;
    int sum = 0;

    for (int i = 0; i < kSafeCount; ++i) {
        threads.emplace_back([&, i]() {
            std::lock_guard<std::mutex> lock(mtx);
            sum += i;
        });
    }
    for (auto& t : threads) t.join();
    std::cout << "  sum via " << kSafeCount << " std::threads: " << sum << '\n';
    std::cout << "  (each thread is a software thread mapped to hardware threads)\n";
}

// ============================================================================
// Point 4 — Oversubscription: more std::threads than hardware → context-switch
//            overhead and degraded cache performance
// ============================================================================

void demo_oversubscription()
{
    std::cout << "\n--- Point 4: Oversubscription with std::thread ---\n";

    unsigned hw = std::thread::hardware_concurrency();
    // Launching many more threads than hardware threads is "oversubscription".
    // The OS must time-slice them, causing overhead and evicting cache lines.
    unsigned oversubscribed = hw * 4;  // 4× the hardware capacity

    std::cout << "  hardware threads: " << hw << '\n';
    std::cout << "  oversubscribed count: " << oversubscribed << '\n';
    std::cout << "  Spawning " << oversubscribed << " threads forces the OS scheduler\n"
              << "  to context-switch excessively, degrading cache locality and\n"
              << "  increasing scheduling overhead compared to std::async which can\n"
              << "  use a thread pool sized for the hardware.\n";

    // Time comparison (conceptual): using std::async avoids this by reusing
    // a pool of worker threads sized to hardware concurrency.
    auto start_async = std::chrono::steady_clock::now();

    std::vector<std::future<int>> futures;
    futures.reserve(oversubscribed);
    for (unsigned i = 0; i < oversubscribed; ++i) {
        futures.push_back(std::async(std::launch::async,
                                     [i]() { return static_cast<int>(i * i); }));
    }
    int async_sum = 0;
    for (auto& f : futures) async_sum += f.get();

    auto end_async = std::chrono::steady_clock::now();
    auto ms_async  = std::chrono::duration_cast<std::chrono::microseconds>(
                         end_async - start_async).count();

    std::cout << "  std::async sum (" << oversubscribed << " tasks): "
              << async_sum << "  time: " << ms_async << " µs\n";
    std::cout << "  (The runtime may schedule these on a thread pool,\n"
              << "   avoiding creation of " << oversubscribed << " OS threads.)\n";
}

// ============================================================================
// Point 5 — std::async with std::launch::async policy: runtime-managed
//            scheduling, natural load balancing, thread-pool friendly
// ============================================================================

int parallelPartialSum(const std::vector<int>& data, std::size_t begin, std::size_t end)
{
    return std::accumulate(data.begin() + begin, data.begin() + end, 0);
}

void demo_async_scheduling()
{
    std::cout << "\n--- Point 5: std::async scheduling and load balancing ---\n";

    // Build a data set and split work into tasks.
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);  // 1 … 1000

    std::size_t half = data.size() / 2;

    // std::async delegates scheduling to the runtime.
    // The runtime may reuse existing pool threads, avoiding thread-creation cost.
    auto f1 = std::async(std::launch::async, parallelPartialSum,
                         std::ref(data), 0, half);
    auto f2 = std::async(std::launch::async, parallelPartialSum,
                         std::ref(data), half, data.size());

    int total = f1.get() + f2.get();
    std::cout << "  sum of 1..1000 via two async tasks: " << total << '\n';
    assert(total == 500500);

    // Contrast: with std::thread we'd need to join both threads and share
    // output through extra synchronization — more code, same overhead.
    std::cout << "  (expected 500500) — correct result obtained without manual\n"
              << "  thread management or shared-state synchronization.\n";
}

// ============================================================================
// Point 6 — When std::thread is still the right tool
//
//  (a) Accessing the native thread handle for platform-specific APIs
//      (e.g., setting real-time priority or CPU affinity via pthreads).
//  (b) Implementing your own thread pool or scheduling mechanism.
//  (c) Working in an environment where you have complete knowledge of the
//      machine and can hand-tune concurrency for maximum performance.
// ============================================================================

void demo_when_to_use_thread()
{
    std::cout << "\n--- Point 6: When std::thread is still appropriate ---\n";

    // (a) Native handle for platform-specific tuning.
    //     std::future/std::async give no access to the underlying thread;
    //     only std::thread exposes native_handle().
    std::thread t([]() {
        // In real code you might call pthread_setschedparam() here using
        // t.native_handle() to set real-time scheduling policy.
        std::this_thread::sleep_for(1ms);
    });

    // native_handle() returns the implementation-defined handle
    // (pthread_t on POSIX, HANDLE on Windows).
    auto handle = t.native_handle();
    (void)handle;  // suppress unused-variable warning; would be passed to pthreads API

    std::cout << "  std::thread::native_handle() = " << handle << '\n';
    std::cout << "  (can be passed to platform APIs for priority/affinity tuning)\n";
    t.join();

    // (b) Thread-pool or custom scheduler: you need direct control over which
    //     work items map to which OS threads → std::thread is appropriate.
    std::cout << "\n  (b) Custom thread pools require std::thread for direct OS-thread control.\n";

    // (c) Known fixed environment: if you know exactly how many cores are
    //     available and want to pin exactly N threads (e.g., NUMA-aware code),
    //     managing std::threads directly gives you that control.
    unsigned hw = std::thread::hardware_concurrency();
    std::cout << "  (c) On this machine, hardware_concurrency() = " << hw
              << ". A hand-tuned pool would spawn exactly " << hw
              << " std::threads.\n";
}

// ============================================================================
// Bonus — Deferred execution: std::launch::deferred
//
// std::async with std::launch::deferred runs the task lazily in the calling
// thread when .get() or .wait() is called — no new thread is ever created.
// This is not available with raw std::thread at all.
// ============================================================================

void demo_deferred_launch()
{
    std::cout << "\n--- Bonus: std::launch::deferred (lazy evaluation) ---\n";

    bool ran = false;

    auto fut = std::async(std::launch::deferred, [&ran]() -> int {
        ran = true;
        return expensiveComputation(50);
    });

    std::cout << "  task has run before get()? " << std::boolalpha << ran << '\n';

    int result = fut.get();  // task runs HERE, in the calling thread

    std::cout << "  task has run after  get()? " << std::boolalpha << ran << '\n';
    std::cout << "  deferred result: " << result << '\n';
    std::cout << "  (No new thread was created — impossible to express with std::thread)\n";
}

// ============================================================================
// main
// ============================================================================

int main()
{
    demo_thread_based();
    demo_task_based();
    demo_thread_levels();
    demo_oversubscription();
    demo_async_scheduling();
    demo_when_to_use_thread();
    demo_deferred_launch();
    return 0;
}
