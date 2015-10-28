#include "util.h"

#include <cassert>
#include <vector>

#include <Windows.h>

using namespace std;

namespace util {

wstring widen(gsl::cstring_view<> x) {
    const auto outCharsCount = MultiByteToWideChar(CP_UTF8, 0, x.data(), x.bytes(), nullptr, 0);
    vector<wchar_t> buf(outCharsCount);
    const auto res = MultiByteToWideChar(CP_UTF8, 0, x.data(), x.bytes(), buf.data(), buf.size());
    assert(res == outCharsCount);
    return wstring{buf.data(), buf.size()};
}

string narrow(gsl::cwstring_view<> x) {
    const auto outCharsCount =
        WideCharToMultiByte(CP_UTF8, 0, x.data(), x.size(), nullptr, 0, nullptr, nullptr);
    vector<char> buf(outCharsCount);
    const auto res = WideCharToMultiByte(CP_UTF8, 0, x.data(), x.size(), buf.data(), buf.size(),
                                         nullptr, nullptr);
    assert(res == outCharsCount);
    return string{buf.data(), buf.size()};
}
}
