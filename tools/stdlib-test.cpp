#include <utility>

template <typename V> struct Node {
  V value;

  template <typename... Args>
  Node(Args &&... args)
      : value(std::forward<Args>(args)...) {}
};

void foo(std::pair<int const, int> const &p) {
  Node<std::pair<int const, int>> node(p);
}

int main() { return 0; }

