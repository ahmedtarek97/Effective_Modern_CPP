#include <iostream>
#include <memory>


int main() {
    //weak pointers can't be dereferenced and can't be tested for nullness
    std::weak_ptr<int> weakPtr;
    // std::cout << *weakPtr; // Error: cannot dereference weak_ptr
    // std::cout << (weakPtr == nullptr); // Error: cannot compare weak_ptr to nullptr

    std::shared_ptr<int> sharedPtr = std::make_shared<int>(42); // RC = 1
    std::weak_ptr<int> weakPtr2 = sharedPtr; // RC = 1
    sharedPtr = nullptr; // RC = 0, object is destroyed, weakPtr2 dangles

    // std::shared_ptr<int> sharedPtr2 = weakPtr2; // Error: cannot construct shared_ptr from weak_ptr directly, must use lock() or constructor that takes weak_ptr

    // weak pointers that dangle are said to be expired
    if (weakPtr2.expired()) {
        std::cout << "weakPtr2 is expired" << std::endl;
    }
    // weakPtr2 can be locked to create a shared_ptr
    if (auto sharedPtr3 = weakPtr2.lock()) {
        std::cout << "weakPtr2 is valid" << std::endl;
    } else {
        // weakPtr2 is expired sharedPtr3 is nullptr
        std::cout << "weakPtr2 is expired" << std::endl;
    }

    // other way a shared_ptr can be created from a weak_ptr
    // std::shared_ptr<int> sharedPtr4(weakPtr2); // if weakPtr2 is expired, bad_weak_ptr is thrown

    // =========================================================================
    // Example: shared_ptr cycle with a doubly linked list (PROBLEM)
    // =========================================================================
    // If both next and prev are shared_ptrs, a cycle is formed:
    //   NodeA->next --> NodeB
    //   NodeB->prev --> NodeA
    // When NodeA and NodeB go out of scope, their ref counts never reach 0
    // because they keep each other alive => MEMORY LEAK.

    struct BadNode {
        int value;
        std::shared_ptr<BadNode> next;
        std::shared_ptr<BadNode> prev; // causes cycle!
        BadNode(int v) : value(v) { std::cout << "BadNode(" << value << ")" << std::endl; }
        ~BadNode() { std::cout << "~BadNode(" << value << ")" << std::endl; }
    };

    {
        auto nodeA = std::make_shared<BadNode>(1);  // constructs in-place, no temporary
        auto nodeB = std::make_shared<BadNode>(2);

        nodeA->next = nodeB; // nodeB RC = 2
        nodeB->prev = nodeA; // nodeA RC = 2

        // When this scope ends, nodeA RC goes 2->1, nodeB RC goes 2->1
        // Neither reaches 0 => destructors are NEVER called => memory leak!
        std::cout << "--- Leaving BadNode scope (expect NO destructors) ---" << std::endl;
    }

    // =========================================================================
    // Solution: use weak_ptr for the back-pointer (prev) to break the cycle
    // =========================================================================

    struct GoodNode {
        int value;
        std::shared_ptr<GoodNode> next;    // owning pointer (forward)
        std::weak_ptr<GoodNode>   prev;    // non-owning pointer (backward) — breaks cycle!
        GoodNode(int v) : value(v) { std::cout << "GoodNode(" << value << ")" << std::endl; }
        ~GoodNode() { std::cout << "~GoodNode(" << value << ")" << std::endl; }
    };

    {
        auto nodeA = std::make_shared<GoodNode>(1);  // constructs in-place, no temporary
        auto nodeB = std::make_shared<GoodNode>(2);

        nodeA->next = nodeB;  // nodeB RC = 2 (nodeA->next + nodeB)
        nodeB->prev = nodeA;  // nodeA RC stays 1 (weak_ptr doesn't increase RC)

        // Traverse forward
        if (auto nx = nodeA->next) {
            std::cout << "nodeA->next->value = " << nx->value << std::endl;
        }

        // Traverse backward (lock the weak_ptr first)
        if (auto pv = nodeB->prev.lock()) {
            std::cout << "nodeB->prev->value = " << pv->value << std::endl;
        }

        // When this scope ends:
        //   nodeB RC: 2->1 (nodeB out of scope), then nodeA is destroyed
        //   which destroys nodeA->next, so nodeB RC: 1->0 => nodeB destroyed
        //   nodeB's weak_ptr prev expires harmlessly.
        std::cout << "--- Leaving GoodNode scope (expect BOTH destructors) ---" << std::endl;
    }

    return 0;
}