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

template <typename T>
auto always(T x) {
    return Behaviour<T>{[x](TimeS) { return x; }};
}

template <typename T, typename F>
auto map(Behaviour<T> b, F f) {
    return makeBehaviour([b, f](TimeS t) mutable { return f(b(t)); });
}

template <typename T, typename U, typename F>
auto map(Behaviour<T> b0, Behaviour<U> b1, F f) {
    return makeBehaviour([b0, b1, f](TimeS t) mutable { return f(b0(t), b1(t)); });
}

template<typename T, typename U>
auto operator*(Behaviour<T> x, Behaviour<U> y) {
    return map(x, y, std::multiplies<>{});
}

struct TimeDelta {
    TimeDelta(TimeS startTime) : lastTime{startTime} {}
    float operator()(TimeS t) {
        const auto delta = t - lastTime;
        lastTime = t;
        return static_cast<float>(delta);
    }
    TimeS lastTime = std::numeric_limits<TimeS>::min();
};

inline auto makeTimeDeltaBehaviour(TimeS startTime) {
    return Behaviour<float>{TimeDelta{startTime}};
}

template <typename T>
struct EulerIntegrator {
    EulerIntegrator(T init) : value{init} {}
    __declspec(noinline) T operator()(T delta) { 
        return value += delta; 
    }
    T value;
};

template <typename T>
auto eulerIntegrate(T init, Behaviour<T> rate, TimeS start) {
    auto deltas = map(rate, makeTimeDeltaBehaviour(start), std::multiplies<>{});
    return map(deltas, EulerIntegrator<T>{init});
}

}  // namespace frp
