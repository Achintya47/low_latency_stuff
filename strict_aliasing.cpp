#include <iostream>

int main() {

    int x = 0;

    int* ip = &x;
    float* fp = (float*)&x;

    *ip = 5;
    *fp = 0.0f;

    std::cout << *ip;

    return 0;
}