#include <typeindex>
#include <type_traits>
#include <chrono>
#include <tuple>
#include <scoped_allocator>
#include <cstdint>
#include <cinttypes>
#include <system_error>
#include <array>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <ratio>
#include <cfenv>
#include <codecvt>
#include <regex>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <ctgmath>
#include <cstdbool>

#include <iostream>

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
