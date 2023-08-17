#include <type_traits>
#include <thread>
#include <mutex>
#include <iostream>
#include <unordered_map>

int main()
{
    auto test = []() -> int
    {
        return 0;
    };

    std::mutex m;
    std::thread t(test);
    t.join();

    std::cout << "Hello World!" << std::endl;

    return 0;
}
