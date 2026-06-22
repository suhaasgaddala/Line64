#include <exception>
#include <iostream>

void run_fixed_message_tests();
void run_blocking_queue_tests();
void run_spsc_queue_tests();
void run_spmc_multicast_queue_tests();

int main() {
    try {
        run_fixed_message_tests();
        run_blocking_queue_tests();
        run_spsc_queue_tests();
        run_spmc_multicast_queue_tests();
        std::cout << "All OrbitQueue tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "OrbitQueue test failure: " << error.what() << '\n';
        return 1;
    }
}
