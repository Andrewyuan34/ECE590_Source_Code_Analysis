#include <iostream>

int foo() {
    int x = 10;  // Dead store, as 'x' is assigned but never used
    return x;
}

int main() {
    int a = 42;  // Dead store, as 'a' is assigned but never used
    a = 10;      // Another dead store, overwrites previous value without use

    int b = foo();       // Uninitialized variable
    std::cout << "Hello, World!" << std::endl;

    return 0;
}
