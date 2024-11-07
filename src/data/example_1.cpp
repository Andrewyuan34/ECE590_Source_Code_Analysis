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
    y = 10;
}

int doubleIt() {
    int x = 10;
    x = 20;
    x = 30;
    return x * 2;
    x  = 40;  // Dead store, as 'x' is assigned but never used
}
// Situation: No loop & no condition control flow in the function
// Can I solve this situation by first examining the liveness info of the func. If the variable is not live at the end of the block, then it is a dead store.


int main() {
    int a = 42;  // Dead store, as 'a' is assigned but never used
    a = 10;      // Another dead store, overwrites previous value without use
    int b;

    int c = 30;
/*
    while(a < 10) {
        a = 20;  // Dead store, overwrites previous value without use
        b = 30;
        c = 40;
    }*/

    //b = bar(a); 
    //std::cout << "Hello, World!" << std::endl;
    b = 30;
    c = b;
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

switch stmt will be seperate into blocks according to the number of case stmts. The default block will be the last block in the CFG.
and the condition probably will always set to be one block before the first case block.
and also the liveness info will not displayed through the switch CFG, so basically you can ignore the switch CFG.
*/

/*
Now try to solve this part by defining situations where dead store can be detected.
1. when a stmt dont have any condition control flow, which means the CFG will only generate "one" block body for the whole function. Basically,
I need to make sure that the stmt has only three blocks in the CFG, the entry block, the body block, and the exit block.

2. 
*/