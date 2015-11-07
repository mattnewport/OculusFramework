#pragma once

#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace frp {

using TimeS = double;

template <typename T>
class Behaviour {
public:
    Behaviour() = default;
    Behaviour(std::function<T(TimeS)> f) : state{std::make_shared<State>(f)} {}
    T operator()(TimeS t) { return (*state)(t); }

private:
    struct State {
        State(std::function<T(TimeS)> f_) : f{f_} {}
        T operator()(TimeS t) {
            if (!f) return T{};
            if (t != lastTime) {
                lastValue = f(t);
                lastTime = t;
            }
            return lastValue;
        }
        std::function<T(TimeS)> f;
        TimeS lastTime = std::numeric_limits<TimeS>::min();
        T lastValue = {};
    };
    std::shared_ptr<State> state;
};

template <typename F>
auto makeBehaviour(F f) {
    return Behaviour<decltype(f(TimeS{}))>{f};
}

template <typename T>
auto always(T x) {
    return Behaviour<T>{[x](TimeS) { return x; }};
}

template <typename F, typename... Ts>
auto map(F f, Behaviour<Ts>... bs) {
    return makeBehaviour([f, bs...](TimeS t) mutable { return f(bs(t)...); });
}

template <typename T>
auto choice(Behaviour<bool> b, T whenTrue, T whenFalse) {
    return map([whenTrue, whenFalse](bool x) { return x ? whenTrue : whenFalse; }, b);
}

template <typename T, typename U>
auto operator*(Behaviour<T> x, Behaviour<U> y) {
    return makeBehaviour([x, y](TimeS t) mutable { return x(t) * y(t); });
}

template <typename T, typename U>
auto operator+(Behaviour<T> x, Behaviour<U> y) {
    return makeBehaviour([x, y](TimeS t) mutable { return x(t) + y(t); });
}

template <typename T, typename U>
auto operator-(Behaviour<T> x, Behaviour<U> y) {
    return makeBehaviour([x, y](TimeS t) mutable { return x(t) - y(t); });
}

inline auto makeTimeDeltaBehaviour(TimeS startTime) {
    return makeBehaviour([lastTime = startTime](TimeS t) mutable {
        const auto delta = t - lastTime;
        lastTime = t;
        return static_cast<float>(delta);
    });
}

template <typename T>
auto eulerIntegrate(T init, Behaviour<T> rate, TimeS start) {
    return map([value = init](auto delta) mutable { return value += delta; },
               map(std::multiplies<>{}, rate, makeTimeDeltaBehaviour(start)));
}

}  // namespace frp
