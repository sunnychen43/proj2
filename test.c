#include <stdio.h>

void foo(int* x){
    int a = 7;
    *x = a;
}

int main(int argc, char** argv){
    
    int x;
    foo(&x);
    printf("%d\n", x);
    return 0;
}