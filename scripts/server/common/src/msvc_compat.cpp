// MSVC CRT compatibility stub
// The pre-built absl/protobuf libraries were compiled with a newer MSVC toolset
// that has __std_rotate. This provides a minimal implementation.

#include <algorithm>
#include <cstring>

extern "C" {

// __std_rotate is an internal MSVC CRT function used by std::rotate
// Provide a fallback implementation
char* __cdecl __std_rotate(char* first, char* middle, char* last) {
    if (first == middle) return last;
    if (middle == last) return first;

    // Simple implementation: use std::rotate from <algorithm>
    std::rotate(first, middle, last);
    return first + (last - middle);
}

}  // extern "C"
