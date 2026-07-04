// ch10 — type erasure: runtime polymorphism without inheritance
// build: clang++ -std=c++20 -Wall -Wextra ch10_type_erasure.cpp -o /tmp/d && /tmp/d
#include <memory>
#include <vector>
#include <iostream>

class Drawable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void do_draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    template <class T>
    struct Model : Concept {
        T obj;
        explicit Model(T o) : obj(std::move(o)) {}
        void do_draw() const override { draw(obj); }        // found via ADL
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };
    std::unique_ptr<Concept> self_;
public:
    template <class T>
    Drawable(T x) : self_(std::make_unique<Model<T>>(std::move(x))) {}
    Drawable(const Drawable& o) : self_(o.self_->clone()) {}
    Drawable& operator=(Drawable o) { self_ = std::move(o.self_); return *this; }

    friend void draw(const Drawable& d) { d.self_->do_draw(); }
};

struct Circle {};
struct Square {};
void draw(const Circle&) { std::cout << "circle\n"; }
void draw(const Square&) { std::cout << "square\n"; }

int main() {
    std::vector<Drawable> shapes;
    shapes.emplace_back(Circle{});
    shapes.emplace_back(Square{});
    shapes.emplace_back(Circle{});
    for (const auto& s : shapes) draw(s);   // circle / square / circle
}
