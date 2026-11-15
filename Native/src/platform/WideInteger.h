#pragma once

namespace capture::platform {
// Checked media arithmetic deliberately uses a wide intermediate. Keep the
// compiler choice here, not scattered through timeline/queue algorithms.
// A future MSVC adapter must supply equivalent exact arithmetic and run the
// overflow/counter-wrap suite; never replace this with floating-point math.
#if defined(__SIZEOF_INT128__)
using WideInteger = __int128;
#else
#error "Capture needs an exact signed 128-bit arithmetic adapter for this compiler"
#endif
}
