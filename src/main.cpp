#include <ostream>
#include <iostream>
#include <string>

#include <GC.hpp>

struct S
{
    std::string name;
    GC::Ptr<S> next;

    S(std::string name) : name(std::move(name)) 
    {
        std::cout << "Constructed S(" + this->name + ").\n";
    }

    ~S()
    {
        std::cout << "Destructed S(" + name + ").\n";
    }
};

template <typename T, typename U>
auto operator<<(std::basic_ostream<T, U> &os, S const &s) -> std::basic_ostream<T, U> &
{
    os << "S(" + s.name + ")";
    return os;
}

template <>
struct GC::PtrTrait<S>
{
    template <GC::PtrVisitor V>
    static auto visit(S *self, V &&visitor) -> void
    {
        visitor(self->next);
    }
};

auto main() -> int
{
    auto s = GC::Ptr<S>::make(std::string("A"));
    s->next = GC::Ptr<S>::make("B");
    s->next->next = s;

    std::cout << "trying to collect. nothing should happen...\n";
    GC::collect();
    s = nullptr;
    std::cout << "collecting again... now everything should be destroyed.\n";
    GC::collect();

    return 0;
}
