#pragma once

#include <functional>
#include <limits>
#include <utility>

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

template <typename T>
auto always(T x) {
    return Behaviour<T>{[x](TimeS) { return x; }};
}

template <typename T, typename F>
auto map(Behaviour<T>& b, F f) {
    return makeBehaviour([&b, f](TimeS t) mutable { return f(b(t)); });
}

template <typename T, typename F>
auto map(Behaviour<T>&& b, F f) {
    return makeBehaviour([b = std::move(b), f](TimeS t) mutable { return f(b(t)); });
}

template <typename T, typename U, typename F>
auto map(Behaviour<T>& b0, Behaviour<U>& b1, F f) {
    return makeBehaviour([&b0, &b1, f](TimeS t) mutable { return f(b0(t), b1(t)); });
}

template <typename T, typename U, typename F>
auto map(Behaviour<T>&& b0, Behaviour<U>& b1, F f) {
    return makeBehaviour(
        [b0 = std::move(b0), &b1, f](TimeS t) mutable { return f(b0(t), b1(t)); });
}

template <typename T, typename U, typename F>
auto map(Behaviour<T>& b0, Behaviour<U>&& b1, F f) {
    return makeBehaviour(
        [&b0, b1 = std::move(b1), f](TimeS t) mutable { return f(b0(t), b1(t)); });
}

template <typename T, typename U, typename F>
auto map(Behaviour<T>&& b0, Behaviour<U>&& b1, F f) {
    return makeBehaviour(
        [b0 = std::move(b0), b1 = std::move(b1), f](TimeS t) mutable { return f(b0(t), b1(t)); });
}

template <typename T>
auto choice(Behaviour<bool>& b, T whenTrue, T whenFalse) {
    return map(b, [whenTrue, whenFalse](bool x) { return x ? whenTrue : whenFalse; });
}

template <typename T, typename U>
auto operator*(Behaviour<T>& x, Behaviour<U>& y) {
    return makeBehaviour([&x, &y](TimeS t) mutable { return x(t) * y(t); });
}

template <typename T, typename U>
auto operator+(Behaviour<T>& x, Behaviour<U>& y) {
    return makeBehaviour([&x, &y](TimeS t) mutable { return x(t) + y(t); });
}

template <typename T, typename U>
auto operator-(Behaviour<T>& x, Behaviour<U>& y) {
    return makeBehaviour([&x, &y](TimeS t) mutable { return x(t) - y(t); });
}

inline auto makeTimeDeltaBehaviour(TimeS startTime) {
    return makeBehaviour([lastTime = startTime](TimeS t) mutable {
        const auto delta = t - lastTime;
        lastTime = t;
        return static_cast<float>(delta);
    });
}

template <typename T>
auto eulerIntegrate(T init, Behaviour<T>& rate, TimeS start) {
    return map(map(rate, makeTimeDeltaBehaviour(start), std::multiplies<>{}),
               [value = init](auto delta) mutable { return value += delta; });
}

}  // namespace frp