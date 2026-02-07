#include <iostream>


int main() {
    enum Color { WHITE, BLACK, YELLO }; // unscoped enum RED, GREEN, BLUE will have the scope as Color so we can't define another variable with the same name in the same scope
    // auto RED = Color::RED; // compilation error RED is already defined in the scope

    enum class Color1 { RED, GREEN, BLUE }; // scoped enum 
    auto RED = false; // fine no other RED in scope
    // Color c = GREEN; // compilation error no enamuration GREEN in this scope we should use Color::GREEN
    Color1 c = Color1::GREEN;

    // one problem with unscoped enums is that they can implicitly convert to int which can lead to unexpected behavior
    int colorValue = Color::WHITE; // fine
    // int colorValue1 = Color1::RED; // compilation error scoped enums do not implicitly convert to int, which is a good thing because it prevents accidental misuse of enum values as integers. if you want to convert a scoped enum to an integer you have to do it explicitly like this:
    int colorValue1 = static_cast<int>(Color1::RED); // fine
}