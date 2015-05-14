#include "d3dhelper.h"

void ThrowOnFailure(HRESULT hr) {
    if (FAILED(hr)) {
        _com_error err{ hr };
        OutputDebugString(err.ErrorMessage());
        throw std::runtime_error{ "Failed HRESULT" };
    }
}
