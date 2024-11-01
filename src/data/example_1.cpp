//#include <iostream>

int foo() {
    int x = 10;  // Dead store, as 'x' is assigned but never used
    while(x--) {
        x = 20;  // Another dead store, overwrites previous value without use
        int a = 0;
    }

    return x;
}

int bar(int f) {
    int y = f;  // Uninitialized variable
    return y;
}



int main() {
    int a = 42;  // Dead store, as 'a' is assigned but never used
    a = 10;      // Another dead store, overwrites previous value without use
    int b = 20;
    int c = 30;

    if(b) {
        b = 40;  // Another dead store, overwrites previous value without use
    }else if(c) {
        b = 90;  // Another dead store, overwrites previous value without use
    }else {
        b = 100; // Another dead store, overwrites previous value without use
    }

    b = bar(a); 
    //std::cout << "Hello, World!" << std::endl;

    return 0;
}

/*
So the essence of checking dead store is: In a funcion, you can only detect them when they have condition control flow otherwise the CFG will only generate 
one block for the whole function.

while loop will be sperate into three blocks in CFG, the first block is the condition block, the second block is the body block, the third block is the block that is
used to jump back to the condition block. and if there is nested while loop exists, the CFG would do the same thing(dividing the while loop into three blocks).

the for loop will be sperate into three blocks in CFG as well, the first block is the condition block, the second block is the body block, 
the third block is the block that is only used to jump back to the condition block. Interestingly, the initialization of index will be placed one block before the condition block.

the do while loop will be sperate into three blocks in CFG as well, the first block is the jump block(which doesnt mean anything), the second block is the loop body,
the third block is the condition block.

if else stmt will be sperate into two blocks in CFG, the first block is the if block body, the second block is the else block body.
That's werid. It seems that they deal with else if stmt by subtituting the else block for its place.
But whatever it is. Using if stmt definitely lead to extra block in CFG, which is what I wanna to see in this case.
*/
