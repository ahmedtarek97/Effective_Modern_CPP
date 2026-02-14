// first usage for mutable is to allow caching of expensive calculations
// second usage is to allow non-const access to member variables
// You want the method to appear const to the user (because it's just "getting" a value), but internally you want to calculate the value once and store it (cache it) for future calls.
// Because storing the cache requires writing to a variable inside a const function, that variable must be mutable.
class MathObject {
private:
    int number;
    // 'mutable' allows these to change inside const methods
    mutable bool isCached = false;
    mutable int cachedValue = 0; 

public:
    MathObject(int n) : number(n) {}

    // This method is conceptually const (it just returns the square),
    // but it needs to modify internal state to work efficiently.
    int getSquare() const {
        if (!isCached) {
            // Expensive calculation simulation
            cachedValue = number * number; 
            isCached = true;
        }
        return cachedValue;
    }
};



// second usage for mutable 
// If you have a class that is accessed by multiple threads, you need a mutex to synchronize access. Reading data (a const operation) still requires locking the mutex.

// However, locking a mutex changes its internal state (from "unlocked" to "locked"). If the mutex isn't mutable, you cannot lock it inside a const getter method.

#include <mutex>

class ThreadSafeCounter {
private:
    int value = 0;
    mutable std::mutex mtx; // Mutex must change state to lock/unlock

public:
    int getValue() const {
        // Locking modifies 'mtx', so it must be mutable
        std::lock_guard<std::mutex> lock(mtx); 
        return value;
    }
};

int main() {
    const MathObject obj(10);
    // Compiles fine even though obj is const and getSquare modifies variables!
    int val = obj.getSquare(); 
}
