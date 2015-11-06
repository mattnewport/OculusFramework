#pragma once

#include <functional>
#include <limits>

namespace frp {

using TimeS = double;

template <typename T>
class Behaviour {
public:
    Behaviour() = default;
    Behaviour(std::function<T(TimeS)> f_) : f(f_) {}
    T operator()(TimeS t) {
        if (!f) return T{};
        if (t != lastTime) {
            lastValue = f(t);
            lastTime = t;
        }
        return lastValue;
    }

private:
    TimeS lastTime = std::numeric_limits<TimeS>::min();
    T lastValue;
    std::function<T(TimeS)> f;
};

template <typename F>
auto makeBehaviour(F f) {
    return Behaviour<decltype(f(TimeS{}))>{f};
}

template <typename T, typename F>
auto map(Behaviour<T> b, F f) {
    return makeBehaviour([b, f](TimeS t) mutable { return f(b(t)); });
}
}  // namespace frp
