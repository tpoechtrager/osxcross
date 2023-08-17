/*

#!/usr/bin/env bash

libcxx=$PWD/target/SDK/$(ls target/SDK/|grep MacOSX|sort -u|head -n1)/usr/include/c++/v1

headers=$(ls $libcxx | grep -v __ | grep -v "\.h" | grep -v modulemap)
exp=$(ls $libcxx/experimental | grep -v __ | grep -v "\.h" | grep -v modulemap)

for e in $exp; do
  headers+=$'\n'"experimental/$e"
done

for header in $headers; do
 echo "#if __has_include(<$header>)";
 echo "#  include <$header>"
 echo "#endif"
done

*/

#if __has_include(<algorithm>)
#  include <algorithm>
#endif
#if __has_include(<any>)
#  include <any>
#endif
#if __has_include(<array>)
#  include <array>
#endif
#if __has_include(<atomic>)
#  include <atomic>
#endif
#if __has_include(<barrier>)
#  include <barrier>
#endif
#if __has_include(<bit>)
#  include <bit>
#endif
#if __has_include(<bitset>)
#  include <bitset>
#endif
#if __has_include(<cassert>)
#  include <cassert>
#endif
#if __has_include(<ccomplex>)
#  include <ccomplex>
#endif
#if __has_include(<cctype>)
#  include <cctype>
#endif
#if __has_include(<cerrno>)
#  include <cerrno>
#endif
#if __has_include(<cfenv>)
#  include <cfenv>
#endif
#if __has_include(<cfloat>)
#  include <cfloat>
#endif
#if __has_include(<charconv>)
#  include <charconv>
#endif
#if __has_include(<chrono>)
#  include <chrono>
#endif
#if __has_include(<cinttypes>)
#  include <cinttypes>
#endif
#if __has_include(<ciso646>)
#  include <ciso646>
#endif
#if __has_include(<climits>)
#  include <climits>
#endif
#if __has_include(<clocale>)
#  include <clocale>
#endif
#if __has_include(<cmath>)
#  include <cmath>
#endif
#if __has_include(<codecvt>)
#  include <codecvt>
#endif
#if __has_include(<compare>)
#  include <compare>
#endif
#if __has_include(<complex>)
#  include <complex>
#endif
#if __has_include(<concepts>)
#  include <concepts>
#endif
#if __has_include(<condition_variable>)
#  include <condition_variable>
#endif
#if __has_include(<coroutine>)
#  include <coroutine>
#endif
#if __has_include(<csetjmp>)
#  include <csetjmp>
#endif
#if __has_include(<csignal>)
#  include <csignal>
#endif
#if __has_include(<cstdarg>)
#  include <cstdarg>
#endif
#if __has_include(<cstdbool>)
#  include <cstdbool>
#endif
#if __has_include(<cstddef>)
#  include <cstddef>
#endif
#if __has_include(<cstdint>)
#  include <cstdint>
#endif
#if __has_include(<cstdio>)
#  include <cstdio>
#endif
#if __has_include(<cstdlib>)
#  include <cstdlib>
#endif
#if __has_include(<cstring>)
#  include <cstring>
#endif
#if __has_include(<ctgmath>)
#  include <ctgmath>
#endif
#if __has_include(<ctime>)
#  include <ctime>
#endif
#if __has_include(<cuchar>)
#  include <cuchar>
#endif
#if __has_include(<cwchar>)
#  include <cwchar>
#endif
#if __has_include(<cwctype>)
#  include <cwctype>
#endif
#if __has_include(<deque>)
#  include <deque>
#endif
#if __has_include(<exception>)
#  include <exception>
#endif
#if __has_include(<execution>)
#  include <execution>
#endif
#if __has_include(<expected>)
#  include <expected>
#endif
#if __has_include(<experimental>)
#  include <experimental>
#endif
#if __has_include(<ext>)
#  include <ext>
#endif
#if __has_include(<filesystem>)
#  include <filesystem>
#endif
#if __has_include(<format>)
#  include <format>
#endif
#if __has_include(<forward_list>)
#  include <forward_list>
#endif
#if __has_include(<fstream>)
#  include <fstream>
#endif
#if __has_include(<functional>)
#  include <functional>
#endif
#if __has_include(<future>)
#  include <future>
#endif
#if __has_include(<initializer_list>)
#  include <initializer_list>
#endif
#if __has_include(<iomanip>)
#  include <iomanip>
#endif
#if __has_include(<ios>)
#  include <ios>
#endif
#if __has_include(<iosfwd>)
#  include <iosfwd>
#endif
#if __has_include(<iostream>)
#  include <iostream>
#endif
#if __has_include(<istream>)
#  include <istream>
#endif
#if __has_include(<iterator>)
#  include <iterator>
#endif
#if __has_include(<latch>)
#  include <latch>
#endif
#if __has_include(<libcxx.imp>)
#  include <libcxx.imp>
#endif
#if __has_include(<limits>)
#  include <limits>
#endif
#if __has_include(<list>)
#  include <list>
#endif
#if __has_include(<locale>)
#  include <locale>
#endif
#if __has_include(<map>)
#  include <map>
#endif
#if __has_include(<memory>)
#  include <memory>
#endif
#if __has_include(<memory_resource>)
#  include <memory_resource>
#endif
#if __has_include(<mutex>)
#  include <mutex>
#endif
#if __has_include(<new>)
#  include <new>
#endif
#if __has_include(<numbers>)
#  include <numbers>
#endif
#if __has_include(<numeric>)
#  include <numeric>
#endif
#if __has_include(<optional>)
#  include <optional>
#endif
#if __has_include(<ostream>)
#  include <ostream>
#endif
#if __has_include(<queue>)
#  include <queue>
#endif
#if __has_include(<random>)
#  include <random>
#endif
#if __has_include(<ranges>)
#  include <ranges>
#endif
#if __has_include(<ratio>)
#  include <ratio>
#endif
#if __has_include(<regex>)
#  include <regex>
#endif
#if __has_include(<scoped_allocator>)
#  include <scoped_allocator>
#endif
#if __has_include(<semaphore>)
#  include <semaphore>
#endif
#if __has_include(<set>)
#  include <set>
#endif
#if __has_include(<shared_mutex>)
#  include <shared_mutex>
#endif
#if __has_include(<source_location>)
#  include <source_location>
#endif
#if __has_include(<span>)
#  include <span>
#endif
#if __has_include(<sstream>)
#  include <sstream>
#endif
#if __has_include(<stack>)
#  include <stack>
#endif
#if __has_include(<stdexcept>)
#  include <stdexcept>
#endif
#if __has_include(<streambuf>)
#  include <streambuf>
#endif
#if __has_include(<string>)
#  include <string>
#endif
#if __has_include(<string_view>)
#  include <string_view>
#endif
#if __has_include(<strstream>)
#  include <strstream>
#endif
#if __has_include(<system_error>)
#  include <system_error>
#endif
#if __has_include(<thread>)
#  include <thread>
#endif
#if __has_include(<tuple>)
#  include <tuple>
#endif
#if __has_include(<typeindex>)
#  include <typeindex>
#endif
#if __has_include(<typeinfo>)
#  include <typeinfo>
#endif
#if __has_include(<type_traits>)
#  include <type_traits>
#endif
#if __has_include(<unordered_map>)
#  include <unordered_map>
#endif
#if __has_include(<unordered_set>)
#  include <unordered_set>
#endif
#if __has_include(<utility>)
#  include <utility>
#endif
#if __has_include(<valarray>)
#  include <valarray>
#endif
#if __has_include(<variant>)
#  include <variant>
#endif
#if __has_include(<vector>)
#  include <vector>
#endif
#if __has_include(<version>)
#  include <version>
#endif
#if __has_include(<experimental/algorithm>)
#  include <experimental/algorithm>
#endif
#if __has_include(<experimental/coroutine>)
#  include <experimental/coroutine>
#endif
#if __has_include(<experimental/deque>)
#  include <experimental/deque>
#endif
#if __has_include(<experimental/forward_list>)
#  include <experimental/forward_list>
#endif
#if __has_include(<experimental/functional>)
#  include <experimental/functional>
#endif
#if __has_include(<experimental/iterator>)
#  include <experimental/iterator>
#endif
#if __has_include(<experimental/list>)
#  include <experimental/list>
#endif
#if __has_include(<experimental/map>)
#  include <experimental/map>
#endif
#if __has_include(<experimental/memory_resource>)
#  include <experimental/memory_resource>
#endif
#if __has_include(<experimental/propagate_const>)
#  include <experimental/propagate_const>
#endif
#if __has_include(<experimental/regex>)
#  include <experimental/regex>
#endif
#if __has_include(<experimental/set>)
#  include <experimental/set>
#endif
#if __has_include(<experimental/simd>)
#  include <experimental/simd>
#endif
#if __has_include(<experimental/string>)
#  include <experimental/string>
#endif
#if __has_include(<experimental/type_traits>)
#  include <experimental/type_traits>
#endif
#if __has_include(<experimental/unordered_map>)
#  include <experimental/unordered_map>
#endif
#if __has_include(<experimental/unordered_set>)
#  include <experimental/unordered_set>
#endif
#if __has_include(<experimental/utility>)
#  include <experimental/utility>
#endif
#if __has_include(<experimental/vector>)
#  include <experimental/vector>
#endif

// ChatGPT generated

// Define a concept to ensure we only accept integral types
template <typename T>
concept Integral = std::is_integral_v<T>;

// Define a simple user structure
struct User {
    int id;
    std::string name;
    std::optional<std::string> email; // not all users may have an email
};

// A simulated database of users
std::vector<User> usersDatabase = {
    {1, "Alice", "alice@email.com"},
    {2, "Bob", std::nullopt},
    {3, "Charlie", "charlie@email.com"},
};

std::optional<User> fetchUserByID(int id) {
    for (const auto& user : usersDatabase) {
        if (user.id == id) return user;
    }
    return std::nullopt;
}

// Function that accepts a lambda with a templated parameter
template <Integral T>
void processWithLambda(auto lambda) {
    lambda(static_cast<T>(5));
}

// A simple function to print a variant which contains either int or std::string
void print_variant(const std::variant<int, std::string>& var) {
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) {
            std::cout << "It's an int: " << arg << "\n";
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "It's a string: " << arg << "\n";
        }
    }, var);
}

// Using inline variable (C++17)
inline static const std::string appName = "C++ Feature Tester";

// Using if constexpr in a template
template <typename T>
void print_type_info(const T& val) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "Integral value: " << val << "\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "Floating point value: " << val << "\n";
    } else {
        std::cout << "Other type value: " << val << "\n";
    }
}

// Using fold expressions to compute sum of variadic arguments
template<typename... Args>
auto sum(Args... args) {
    return (... + args);  // fold expression
}

// A simple class to demonstrate spaceship operator
class Point {
public:
    int x, y;

    auto operator<=>(const Point&) const = default; // Compiler generates the memberwise comparisons for us
};

// Function returning tuple, to be used with structured bindings
std::tuple<int, std::string, double> get_data() {
    return {42, "Answer", 3.14};
}

// Concept to check if a type is an arithmetic type (int, float, etc.)
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <Arithmetic T>
T half(T value) {
    return value / 2;
}

// Function using std::ranges to filter and transform a container
auto filter_and_double(const std::vector<int>& numbers, int threshold) {
    return numbers | std::views::filter([threshold](int n) { return n > threshold; })
                   | std::views::transform([](int n) { return n * 2; });
}

// Inline variable definition
inline constexpr int globalValue = 100;

// Function marked as [[nodiscard]]
[[nodiscard]] int computeValue(int x) {
    return x * x + globalValue;
}

void useValue([[maybe_unused]] int val) {
    // Intentionally do nothing
}

// Using consteval
consteval int computeCompileTimeValue(int x) {
    return x * x + 10;
}

int main() {
    // Use structured bindings with range-based for loop
    for (const auto& [id, name, email] : usersDatabase) {
        std::cout << "ID: " << id << ", Name: " << name;
        if (email) std::cout << ", Email: " << *email;
        std::cout << std::endl;
    }

    // Use ranges to filter and transform data
    auto names = usersDatabase | std::views::transform([](const User& u) { return u.name; });

    std::cout << "\nUser names using ranges:\n";
    for (const auto& name : names) {
        std::cout << name << std::endl;
    }

    if (auto user = fetchUserByID(2); user) {
        std::cout << "\nFetched user by ID: " << user->name << std::endl;
    } else {
        std::cout << "\nUser not found." << std::endl;
    }

    // Use lambda with templated parameter
    processWithLambda<int>([](auto val) {
        std::cout << "\nProcessed value inside lambda: " << val << std::endl;
    });

    // Using std::any
    std::any data;
    data = 5;
    if (data.has_value() && data.type() == typeid(int)) {
        std::cout << "\nData contains int: " << std::any_cast<int>(data) << std::endl;
    }

    data = std::string("Hello, std::any!");
    if (data.has_value() && data.type() == typeid(std::string)) {
        std::cout << "Data contains string: " << std::any_cast<std::string>(data) << std::endl;
    }

    // Using std::variant
    std::variant<int, std::string> var = 10;
    print_variant(var);

    var = "Hello, std::variant!";
    print_variant(var);

    // Using std::filesystem to print the current path
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::cout << "\nCurrent working directory: " << currentPath.string() << std::endl;

    std::cout << "\nApp Name: " << appName << std::endl;

    std::string_view sv = "Hello, string_view!";
    std::cout << "\nString view: " << sv << std::endl;

    print_type_info(10);
    print_type_info(10.5);

    int total = sum(1, 2, 3, 4, 5);
    std::cout << "\nSum of numbers using fold expressions: " << total << std::endl;

    // Designated initializers
    Point p1{.x = 5, .y = 10};
    Point p2{.x = 5, .y = 20};

    if (p1 < p2) {
        std::cout << "p1 is less than p2\n";
    }

    // Using structured bindings
    auto [value, text, number] = get_data();
    std::cout << "\nStructured bindings:\n";
    std::cout << "Value: " << value << "\n";
    std::cout << "Text: " << text << "\n";
    std::cout << "Number: " << number << "\n";

    // std::optional improvements
    if (auto userOpt = fetchUserByID(3); userOpt.has_value()) {
        std::cout << "\nFound user: " << userOpt->name << std::endl;
        std::cout << "Using value(): " << userOpt.value().name << std::endl;
    }

    // Using std::from_chars and std::to_chars for fast conversion
    char buffer[10];
    int value_to_convert = 12345;
    std::to_chars(buffer, buffer + sizeof(buffer), value_to_convert);

    int parsed_value = 0;
    std::from_chars(buffer, buffer + sizeof(buffer), parsed_value);
    std::cout << "\nConverted back and forth: " << parsed_value << std::endl;

    // Using std::span
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    std::span<int> arr_span(arr);
    for (int val : arr_span) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // Range-based for loop with initializer
    for (std::size_t i = 0; auto& val : arr_span) {
        std::cout << "Element " << i++ << ": " << val << "\n";
    }

    // Using std::byte
    std::byte b{0x3F}; 
    std::cout << "\nByte value: " << static_cast<int>(b) << std::endl;

    // Using concepts
    std::cout << "\nHalf of 10: " << half(10) << std::endl;

    // Using std::ranges
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::cout << "\nFiltered and doubled numbers:\n";
    for (int n : filter_and_double(numbers, 5)) {
        std::cout << n << " ";
    }
    std::cout << std::endl;

    // Using inline variable
    std::cout << "\nGlobal inline value: " << globalValue << std::endl;

    // Using [[nodiscard]] attribute
    int result = computeValue(5); // OK
    // computeValue(5);  // This will produce a warning because the return value is discarded.

    // Using [[maybe_unused]]
    useValue(result);

    // Lambdas with constexpr
    auto constexprLambda = []() constexpr {
        return 2 + 3;
    };
    static_assert(constexprLambda() == 5, "Math is broken!");

    // Lambdas using template syntax
    auto genericLambda = []<typename T>(T x, T y) {
        return x + y;
    };
    std::cout << "\nResult of generic lambda: " << genericLambda(3.5, 4.5) << std::endl;

    constexpr int compileTimeResult = computeCompileTimeValue(5);
    std::cout << "\nCompile-time result: " << compileTimeResult << std::endl;

    return 0;
}
