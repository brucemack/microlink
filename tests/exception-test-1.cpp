#include <iostream>

int main(int, const char**) {
    try {
        throw std::domain_error("test");
    } catch (const std::domain_error& ex) {
        std::cout << "Catch " << ex.what() << std::endl;
    }
}

