#include <stdio.h>

int main(){
    int x = 16;
    int n = 4;
    printf("%d", (x / n) + (x - n * (x / n)));
    return 0;
}