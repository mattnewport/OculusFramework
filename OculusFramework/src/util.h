#pragma once

#pragma warning(push)
#pragma warning(disable: 4245)
#include <string_view.h> // GSL
#pragma warning(pop)

#include <string>

namespace util {
std::string narrow(gsl::cwstring_view<> x);
std::wstring widen(gsl::cstring_view<> x);
}
