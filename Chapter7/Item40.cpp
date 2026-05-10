// Item 40: Use std::atomic for concurrency, std::volatile for special memory.
//
// Key Points:
//  1. std::atomic provides atomic read-modify-write (RMW) operations that are
//     indivisible from the perspective of all threads — no data race.
//  2. std::atomic imposes memory-ordering constraints: operations on atomics
//     appear sequentially consistent by default (seq_cst), preventing
//     compiler/hardware reordering that would break inter-thread logic.
//  3. volatile tells the compiler that a variable's value may change outside
//     normal program flow (hardware, signal handler, DMA), so reads/writes
//     must not be optimized away or reordered.
//  4. volatile does NOT make operations atomic and does NOT prevent data races.
//     It is unsuitable for inter-thread communication.
//  5. std::atomic DOES allow the compiler to eliminate redundant operations
//     (consecutive loads or stores with no other atomic operation in between).
//     volatile NEVER considers any read or write redundant.
//  6. std::atomic and volatile are orthogonal and address different problems.
//     Neither replaces the other; occasionally both are needed together
//     (e.g., memory-mapped hardware counter shared between threads).

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 1: std::atomic makes individual operations indivisible (atomic).
//
// A plain int counter incremented from multiple threads yields data races and
// undefined behaviour.  Replacing it with std::atomic<int> gives well-defined,
// race-free results because each fetch_add() is an indivisible RMW.
// ─────────────────────────────────────────────────────────────────────────────

void point1_atomic_counter()
{
    std::cout << "=== Point 1: std::atomic — atomic RMW operation ===\n";

    constexpr int kThreads    = 8;
    constexpr int kIterations = 100'000;

    // ---------- safe: std::atomic ----------
    std::atomic<int> atomicCounter{0};
    {
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i)
            workers.emplace_back([&]
            {
                for (int j = 0; j < kIterations; ++j)
                    atomicCounter.fetch_add(1, std::memory_order_relaxed);
                    // memory_order_relaxed is sufficient when only the final
                    // count matters and we synchronise via thread::join().
            });
        for (auto& t : workers) t.join();
    }
    std::cout << "  atomic counter (expected "
              << kThreads * kIterations << "): "
              << atomicCounter.load() << '\n';
    assert(atomicCounter.load() == kThreads * kIterations);

    // ---------- unsafe: plain int (demonstration — do NOT do this) ----------
    // We intentionally mention it conceptually; running it would be UB.
    std::cout << "  (A plain 'int' counter would be a data race — UB.)\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 2: std::atomic enforces memory ordering — the default is seq_cst.
//
// With two plain flags and no synchronisation, the compiler/CPU could reorder
// stores, causing another thread to see y==true before x==true.
// std::atomic<bool> with the default seq_cst ordering prevents this.
//
// Example: classic "store x, store y → load y, load x" pattern.
// If load(y)==true then load(x) MUST also be true under seq_cst.
// ─────────────────────────────────────────────────────────────────────────────

void point2_memory_ordering()
{
    std::cout << "=== Point 2: std::atomic — sequential-consistency (memory ordering) ===\n";

    // Run many trials; under seq_cst the result is always consistent.
    constexpr int kTrials = 100'000;
    int violations        = 0; // should remain 0

    for (int trial = 0; trial < kTrials; ++trial)
    {
        std::atomic<bool> x{false}, y{false};
        int observedX = 0;

        std::thread writer([&]
        {
            x.store(true);  // (A) seq_cst store
            y.store(true);  // (B) seq_cst store
        });

        std::thread reader([&]
        {
            while (!y.load()) {} // spin until (B) is visible
            // Under seq_cst, once we see (B), we are guaranteed to also see
            // the effect of (A) because (A) is sequenced before (B).
            observedX = x.load() ? 1 : 0;
        });

        writer.join();
        reader.join();

        if (observedX == 0) ++violations;
    }

    std::cout << "  seq_cst violations (must be 0): " << violations << '\n';
    assert(violations == 0);
    std::cout << "  Once y is observed true, x is guaranteed true.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 3: volatile does NOT make operations atomic.
//
// A volatile bool flag used to stop a loop is sometimes seen in older code.
// This works incidentally on most modern hardware for simple bool writes —
// but it provides no atomicity guarantees and constitutes a data race (UB).
// std::atomic is the correct tool.
//
// We can't safely demonstrate the UB, so we contrast the interfaces:
// ─────────────────────────────────────────────────────────────────────────────

void point3_volatile_not_atomic()
{
    std::cout << "=== Point 3: volatile does NOT make operations atomic ===\n";

    // --- Correct: use std::atomic<bool> as a stop flag ---
    std::atomic<bool> stopFlag{false};
    int workDone = 0;

    std::thread worker([&]
    {
        while (!stopFlag.load(std::memory_order_acquire))
        {
            ++workDone;
            // simulate work
        }
        std::cout << "  [worker] stopped after " << workDone << " iterations.\n";
    });

    sleep_ms(10);
    stopFlag.store(true, std::memory_order_release); // atomic, visible to worker
    worker.join();

    // --- Wrong (conceptual only — do not use in production) ---
    // volatile bool stopFlag = false;       // NOT atomic!
    // stopFlag = true;                      // data race if another thread reads
    // The write may not be visible atomically; the compiler may have cached it.

    std::cout << "  NOTE: 'volatile bool' would be a data race here.\n"
              << "        std::atomic<bool> provides the necessary guarantee.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 4: volatile prevents optimisation of reads/writes to special memory.
//
// The compiler is allowed to eliminate "redundant" loads and stores to normal
// variables.  For hardware registers or memory-mapped I/O the programmer knows
// that each access has a real side-effect (e.g., triggering a peripheral or
// reading a hardware sensor).  volatile prevents the elimination.
//
// We simulate a hardware register with a global volatile variable.
// Without volatile the compiler may collapse several reads into one or skip
// stores entirely; with volatile every access is preserved.
// ─────────────────────────────────────────────────────────────────────────────

// Simulated hardware register (in real embedded code this would be at a fixed
// memory-mapped address).
volatile std::uint32_t hardwareStatusRegister = 0;

// For comparison: a plain (non-volatile) variable the compiler CAN optimise.
std::uint32_t normalVariable = 0;

void point4_volatile_special_memory()
{
    std::cout << "=== Point 4: volatile — prevents optimisation of special memory ===\n";

    // Each of these reads of hardwareStatusRegister MUST be emitted by the
    // compiler; it cannot assume the value hasn't changed between reads even
    // though no C++ code modifies it here (the hardware might!).
    std::uint32_t val1 = hardwareStatusRegister;
    std::uint32_t val2 = hardwareStatusRegister; // compiler CANNOT cache val1
    std::uint32_t val3 = hardwareStatusRegister; // every read is preserved

    // With a plain variable the compiler may (and with optimisation will)
    // collapse these three reads into one, since it can prove the value hasn't
    // changed:
    //   uint32_t n1 = normalVariable;
    //   uint32_t n2 = normalVariable; // may be optimised to: n2 = n1;
    //   uint32_t n3 = normalVariable; // may be optimised to: n3 = n1;

    // Similarly, these writes must ALL be emitted (e.g., to set hardware bits):
    hardwareStatusRegister = 0x01; // enable bit 0
    hardwareStatusRegister = 0x03; // enable bit 1 — NOT dead code
    hardwareStatusRegister = 0x07; // enable bit 2 — NOT dead code

    // With a normal variable the first two stores would be dead stores and
    // the compiler would eliminate them:
    //   normalVariable = 1; // dead store — optimised away
    //   normalVariable = 2; // dead store — optimised away
    normalVariable = 3;      // the only one the compiler would keep

    std::cout << "  volatile reads: " << val1 << ", " << val2 << ", " << val3
              << " (all three reads are emitted even if values are the same)\n";
    std::cout << "  volatile writes: each store to hardwareStatusRegister is\n"
              << "  preserved — none is treated as a dead store.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 5: std::atomic CAN eliminate redundant operations.
//
// The standard explicitly allows the compiler to combine redundant atomic
// stores or loads (consecutive stores/loads with no other atomic operation
// between them where the intermediate value is not observed by any thread).
//
// volatile NEVER treats any read or write as redundant.
//
// This matters: if you need every single write to hardware to be visible
// (e.g., to a peripheral), volatile is the right tool, NOT std::atomic.
// ─────────────────────────────────────────────────────────────────────────────

void point5_atomic_allows_redundant_elimination()
{
    std::cout << "=== Point 5: std::atomic may eliminate redundant operations ===\n";

    // The C++ standard allows the implementation to turn the sequence:
    //
    //   std::atomic<int> x{0};
    //   x.store(1);   // intermediate — no thread observes this value
    //   x.store(2);   // intermediate — no thread observes this value
    //   x.store(3);   // final value
    //
    // into just:  x.store(3);
    //
    // In contrast:
    //
    //   volatile int v = 0;
    //   v = 1;   // cannot be eliminated
    //   v = 2;   // cannot be eliminated
    //   v = 3;   // cannot be eliminated
    //
    // All three stores are mandatory for volatile.

    std::atomic<int> ai{0};
    ai.store(1);
    ai.store(2);
    ai.store(3); // compiler may only emit this one

    volatile int vi = 0;
    vi = 1; // must be emitted
    vi = 2; // must be emitted
    vi = 3; // must be emitted

    std::cout << "  ai = " << ai.load() << " (compiler may have kept only the final store)\n";
    std::cout << "  vi = " << vi        << " (all three stores are definitely emitted)\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 6: std::atomic and volatile are orthogonal — occasionally both needed.
//
// A hardware counter incremented by a DMA engine AND read by multiple threads
// requires both guarantees:
//   • volatile  → every read/write is emitted (no dead-store/dead-load elim.)
//   • std::atomic → reads and writes are atomic relative to other threads
//
// std::atomic<T> with volatile is not directly expressible in standard C++, so
// the typical solution is to use volatile for the hardware address and
// std::atomic for the software-shared copy, or to use platform-specific types
// (e.g., sig_atomic_t for signal handlers, or vendor atomic intrinsics).
//
// We demonstrate the concept with a simulation.
// ─────────────────────────────────────────────────────────────────────────────

// Simulated DMA-written hardware counter at a fixed address.
volatile std::uint32_t dmaCounter = 0; // updated "externally" (by hardware)

void point6_volatile_and_atomic_orthogonal()
{
    std::cout << "=== Point 6: volatile and std::atomic are orthogonal ===\n";

    // Thread-safe software counter (for inter-thread communication).
    std::atomic<std::uint32_t> softwareCounter{0};

    // In real code a driver thread might periodically snapshot the hardware
    // register into the atomic, making the value safely accessible to other
    // threads without a data race.
    std::thread driver([&]
    {
        for (int i = 0; i < 5; ++i)
        {
            // Read the volatile hardware register (every read is preserved).
            std::uint32_t hwValue = dmaCounter;
            // Publish to other threads via the atomic.
            softwareCounter.store(hwValue, std::memory_order_release);
            sleep_ms(5);
        }
    });

    std::thread consumer([&]
    {
        sleep_ms(25);
        // Safe to read from any thread — atomic load, no data race.
        std::cout << "  consumer sees softwareCounter = "
                  << softwareCounter.load(std::memory_order_acquire) << '\n';
    });

    driver.join();
    consumer.join();

    std::cout << "  volatile  → hardware register reads/writes are never optimised away.\n"
              << "  atomic    → software counter reads/writes are thread-safe.\n"
              << "  One does not replace the other.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 7: std::atomic compare_exchange — a classic lock-free pattern.
//
// compare_exchange_strong (CAS) is the foundation of lock-free algorithms.
// It atomically checks whether the variable holds an expected value and, only
// if so, replaces it — all as a single indivisible operation.
// ─────────────────────────────────────────────────────────────────────────────

void point7_compare_exchange()
{
    std::cout << "=== Point 7: std::atomic compare_exchange (CAS) ===\n";

    std::atomic<int> value{0};

    // Only one of many threads will succeed in setting the value from 0 to 42.
    constexpr int kThreads = 8;
    std::atomic<int> winnerThread{-1};

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int id = 0; id < kThreads; ++id)
        threads.emplace_back([&, id]
        {
            int expected = 0;
            // CAS: if value==0, set it to 42 and record this thread as winner.
            if (value.compare_exchange_strong(expected, 42))
                winnerThread.store(id);
        });

    for (auto& t : threads) t.join();

    std::cout << "  value after CAS from " << kThreads << " competing threads: "
              << value.load() << '\n';
    std::cout << "  winning thread id: " << winnerThread.load() << '\n';
    assert(value.load() == 42);
    std::cout << "  Exactly one thread won the CAS race.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 8: Summary — when to use which tool.
// ─────────────────────────────────────────────────────────────────────────────

void point8_summary()
{
    std::cout << "=== Point 8: Summary — std::atomic vs volatile ===\n\n";

    std::cout
        << "  Use std::atomic when:\n"
        << "    • You need an operation to be atomic (indivisible) w.r.t. threads.\n"
        << "    • You need memory ordering guarantees between threads.\n"
        << "    • You are implementing lock-free data structures or flags.\n"
        << "    • Example: shared counter, stop flag, published pointer.\n\n"

        << "  Use volatile when:\n"
        << "    • A variable's value may change outside normal program control\n"
        << "      (e.g., memory-mapped hardware register, signal handler global).\n"
        << "    • You must prevent the compiler from eliminating reads or writes.\n"
        << "    • Example: hardware status register, DMA buffer, sig_atomic_t.\n\n"

        << "  NEVER use volatile as a substitute for std::atomic in multi-threaded\n"
        << "  code — it does not prevent data races.\n\n"

        << "  NEVER use std::atomic as a substitute for volatile for hardware\n"
        << "  access — atomics may still eliminate 'redundant' accesses that are\n"
        << "  in fact required by the hardware.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    point1_atomic_counter();
    point2_memory_ordering();
    point3_volatile_not_atomic();
    point4_volatile_special_memory();
    point5_atomic_allows_redundant_elimination();
    point6_volatile_and_atomic_orthogonal();
    point7_compare_exchange();
    point8_summary();

    std::cout << "All assertions passed.\n";
    return 0;
}
