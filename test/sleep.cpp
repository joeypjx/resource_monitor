#include <iostream>
#include <thread>

int main() {
    std::cout << "Sleeping for 10 seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "Done" << std::endl;
    return 0;
}