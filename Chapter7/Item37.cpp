// Item 37: Make std::threads unjoinable on all paths
//
// Key Points:
//  1. Every std::thread is either joinable or unjoinable.
//  2. Destroying a joinable std::thread calls std::terminate() — program crash.
//  3. join-on-destruction can cause hard-to-debug performance anomalies.
//  4. detach-on-destruction can cause undefined behavior (dangling references).
//  5. Use a RAII wrapper (ThreadRAII) to guarantee unjoinability on all paths,
//     including exceptions and early returns.
//  6. Inside the RAII wrapper, declare the DtorAction member before the
//     std::thread member so the thread is initialized last (and thus available
//     when the user-provided action is read in the destructor body).

#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <vector>

// ============================================================
// Helper
// ============================================================
static void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// ============================================================
// Point 1: Joinable vs. Unjoinable thread states
//
// A std::thread is joinable if it corresponds to an underlying
// thread of execution that is running or could be run.
// A std::thread is unjoinable when it is:
//   - default-constructed          (no task)
//   - moved-from                   (ownership transferred)
//   - joined                       (thread finished, result collected)
//   - detached                     (connection to thread severed)
// ============================================================
void point1_joinable_vs_unjoinable()
{
    std::cout << "=== Point 1: Joinable vs. Unjoinable ===\n";

    // Default-constructed: unjoinable (no associated thread)
    std::thread t1;
    std::cout << "  default-constructed joinable? " << t1.joinable() << "\n"; // 0

    // Running thread: joinable
    std::thread t2([] { sleep_ms(10); });
    std::cout << "  running thread   joinable? " << t2.joinable() << "\n";    // 1

    // After join: unjoinable
    t2.join();
    std::cout << "  after join       joinable? " << t2.joinable() << "\n";    // 0

    // Moved-from: unjoinable
    std::thread t3([] { sleep_ms(10); });
    std::thread t4 = std::move(t3);
    std::cout << "  moved-from       joinable? " << t3.joinable() << "\n";    // 0
    t4.join();

    // After detach: unjoinable
    std::thread t5([] { sleep_ms(10); });
    t5.detach();
    std::cout << "  after detach     joinable? " << t5.joinable() << "\n";    // 0

    std::cout << "\n";
}

// ============================================================
// Point 2: Destroying a joinable thread calls std::terminate()
//
// The C++ Standard requires that the std::thread destructor
// call std::terminate() when the thread is still joinable.
// This is a deliberate design choice: both implicit join and
// implicit detach have serious problems (see Points 3 & 4),
// so the Standard chose the loudest possible failure instead.
// ============================================================
void point2_terminate_on_joinable_destroy()
{
    std::cout << "=== Point 2: Destroying a joinable thread = std::terminate() ===\n";
    std::cout << "  (Shown as description only — executing would crash the program)\n\n";

    std::cout << R"(  void bad() {
      std::thread t([] { /* work */ });
      if (someCondition) return;  // BUG: destructor of t calls std::terminate()!
      t.join();
  }
)" << "\n";
}

// ============================================================
// Point 3: Why join-on-destruction is problematic
//
// If a wrapper naively calls join() in its destructor, an early
// return or exception forces the caller to wait for the thread
// even though it no longer needs its result — a hidden
// performance anomaly that is very hard to diagnose.
// ============================================================

// Simulates an expensive filter check that sometimes fails early
static bool conditionMet() { return false; }

// Naive RAII that always joins — illustrates the problem
class NaiveJoinOnDestruction
{
public:
    explicit NaiveJoinOnDestruction(std::thread t) : t_(std::move(t)) {}
    ~NaiveJoinOnDestruction() { if (t_.joinable()) t_.join(); }

    NaiveJoinOnDestruction(const NaiveJoinOnDestruction&) = delete;
    NaiveJoinOnDestruction& operator=(const NaiveJoinOnDestruction&) = delete;

private:
    std::thread t_;
};

void point3_join_on_destruction_anomaly()
{
    std::cout << "=== Point 3: join-on-destruction performance anomaly ===\n";

    auto longTask = []
    {
        sleep_ms(80);
        std::cout << "  [thread] long task done.\n";
    };

    auto start = std::chrono::steady_clock::now();

    {
        // Note: NaiveJoinOnDestruction guard(std::thread(longTask)) would be the
        // "most vexing parse" — the compiler treats it as a function declaration.
        // Use an explicit variable or braced-init to avoid it.
        std::thread thread(longTask);
        NaiveJoinOnDestruction guard(std::move(thread));

        if (!conditionMet())
        {
            std::cout << "  Condition not met — early return.\n";
            std::cout << "  But join-on-destruction BLOCKS here until the thread finishes!\n";
            // Destructor called here: forced to wait ~80 ms even though we don't need it
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "  Waited ~" << elapsed << " ms despite early return.\n\n";
}

// ============================================================
// Point 4: Why detach-on-destruction is problematic
//
// If a wrapper calls detach() in its destructor, the thread
// continues to run after the function returns. If the thread
// captures locals by reference (a very common pattern), those
// variables are destroyed when the function exits — the thread
// then accesses dangling memory, causing undefined behavior.
// ============================================================
void point4_detach_on_destruction_ub()
{
    std::cout << "=== Point 4: detach-on-destruction undefined behavior ===\n";
    std::cout << "  (Shown as description only — executing would be UB)\n\n";

    std::cout << R"(  void badDetach() {
      int localVar = 42;
      std::thread t([&localVar] {
          sleep_ms(500);
          localVar = 100;   // UB: localVar is destroyed when badDetach() returns!
      });
      t.detach();           // thread keeps running after function exits
  }                         // localVar destroyed here -> thread accesses garbage
)" << "\n";
}

// ============================================================
// Point 5 & 6: ThreadRAII — the correct RAII solution
//
// The wrapper receives the desired destructor action (join or
// detach) as a constructor argument, applies it safely in the
// destructor, and provides access to the underlying thread.
//
// Member declaration order note (Point 6):
//   action_ is declared BEFORE t_ so that in the constructor's
//   member-initialization list, action_ is initialized first.
//   This guarantees action_ is ready when used in the destructor
//   body, even though in practice both are valid throughout the
//   destructor body. It's also the natural "data before resource"
//   convention.
// ============================================================
class ThreadRAII
{
public:
    enum class DtorAction { join, detach };

    ThreadRAII(std::thread&& t, DtorAction a)
        : action_(a), t_(std::move(t)) {}   // action_ initialized first (see Point 6)

    ~ThreadRAII()
    {
        if (t_.joinable())
        {
            if (action_ == DtorAction::join)
                t_.join();
            else
                t_.detach();
        }
    }

    // Provide access to the underlying std::thread (e.g., to call native_handle)
    std::thread& get() { return t_; }

    // Move-only (threads are move-only)
    ThreadRAII(ThreadRAII&&)            = default;
    ThreadRAII& operator=(ThreadRAII&&) = default;

    ThreadRAII(const ThreadRAII&)            = delete;
    ThreadRAII& operator=(const ThreadRAII&) = delete;

private:
    DtorAction action_;   // declared BEFORE t_ (Point 6)
    std::thread t_;
};

// ---- 5a: join action keeps us safe when thread accesses local state ----
void point5a_threadraii_join()
{
    std::cout << "=== Point 5a: ThreadRAII with DtorAction::join ===\n";

    int result = 0;

    {
        ThreadRAII t(
            std::thread([&result]
            {
                sleep_ms(30);
                result = 42;
            }),
            ThreadRAII::DtorAction::join   // wait for thread before locals are destroyed
        );
        // 'result' is still alive; destructor will join before it goes out of scope
    }

    std::cout << "  result = " << result << " (thread safely joined)\n\n";
}

// ---- 5b: detach action safe when thread does NOT touch local state ----
void point5b_threadraii_detach()
{
    std::cout << "=== Point 5b: ThreadRAII with DtorAction::detach ===\n";

    {
        ThreadRAII t(
            std::thread([]
            {
                // Uses only its own stack — no references to the caller's locals
                sleep_ms(5);
            }),
            ThreadRAII::DtorAction::detach   // safe: no shared mutable state
        );
        std::cout << "  Scope ending — thread detached, runs independently.\n";
    }

    std::cout << "\n";
}

// ---- 5c: ThreadRAII handles ALL exit paths, including exceptions ----
static bool shouldThrow() { return true; }

void point5c_all_exit_paths()
{
    std::cout << "=== Point 5c: ThreadRAII handles all exit paths (including exceptions) ===\n";

    auto work = []
    {
        sleep_ms(20);
        std::cout << "  [thread] work done inside exception path.\n";
    };

    try
    {
        ThreadRAII t(std::thread(work), ThreadRAII::DtorAction::join);

        if (shouldThrow())
            throw std::runtime_error("something went wrong");

        // normal exit
    }
    catch (const std::exception& e)
    {
        // ThreadRAII destructor already ran (and joined) during stack unwinding
        std::cout << "  Caught: \"" << e.what() << "\"\n";
        std::cout << "  ThreadRAII ensured the thread was joined before unwinding.\n";
    }

    std::cout << "\n";
}

// ---- Point 6: Illustrate member declaration order explicitly ----
void point6_member_declaration_order()
{
    std::cout << "=== Point 6: Member declaration order in ThreadRAII ===\n\n";

    std::cout << R"(  class ThreadRAII {
      DtorAction action_;   // declared first -> initialized first in ctor init-list
      std::thread t_;       // declared second

      // In the destructor body, BOTH members are still fully constructed,
      // so reading action_ and calling t_.joinable() are both safe regardless
      // of order. But the convention 'data before resource' and the rule that
      // members are initialized in declaration order make it important to
      // declare action_ before t_.  If t_'s initialization ever depended on
      // action_ (e.g., passing a callback), correct ordering is critical.
  };
)" << "\n";
}

// ============================================================
// main
// ============================================================
int main()
{
    point1_joinable_vs_unjoinable();
    point2_terminate_on_joinable_destroy();
    point3_join_on_destruction_anomaly();
    point4_detach_on_destruction_ub();
    point5a_threadraii_join();
    point5b_threadraii_detach();
    point5c_all_exit_paths();
    point6_member_declaration_order();

    std::cout << "All Item 37 demonstrations completed.\n";
    return 0;
}
