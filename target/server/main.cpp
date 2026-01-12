#include <iostream>
#include "../common/dummy.h"

int main() {
    std::cout << "server start\n";
    std::cout << common::build_info() << "\n";
    return 0;
}
