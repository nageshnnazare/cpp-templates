// ch05 — serialization via class-template specialization (full + partial)
// build: clang++ -std=c++20 -Wall -Wextra ch05_serialize.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>
#include <vector>

template <class T>
struct Serializer {
    static std::string to_str(const T& v) { return std::to_string(v); }
};

template <>
struct Serializer<std::string> {
    static std::string to_str(const std::string& v) { return '"' + v + '"'; }
};

template <class T>
struct Serializer<std::vector<T>> {
    static std::string to_str(const std::vector<T>& v) {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) {
            out += Serializer<T>::to_str(v[i]);
            if (i + 1 < v.size()) out += ", ";
        }
        return out + "]";
    }
};

template <class T>
std::string serialize(const T& v) { return Serializer<T>::to_str(v); }

int main() {
    std::cout << serialize(42) << '\n';                       // 42
    std::cout << serialize(std::string("hi")) << '\n';        // "hi"
    std::cout << serialize(std::vector<int>{1, 2, 3}) << '\n';// [1, 2, 3]
    std::vector<std::vector<int>> nested{{1, 2}, {3}};
    std::cout << serialize(nested) << '\n';                   // [[1, 2], [3]]
}
