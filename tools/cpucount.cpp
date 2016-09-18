#include <iostream>
#include <thread>

/** Print number of (enabled) CPU cores.
 *
 * Requires C++11 or better.
 */
int main()
{
    std::cout << std::thread::hardware_concurrency() << std::endl;
}
