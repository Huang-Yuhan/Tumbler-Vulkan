#pragma once

// Shared between C++ and Slang — included by both.
//
//   #ifdef __cplusplus    → C++  compilation
//   #else                 → Slang compilation (Slang does NOT define __cplusplus)
//
// Use only types that both languages understand: uint32_t, float, etc.
// NO std:: types, templates, or C++ standard library headers.

// ===================================================================
// Platform detection
// ===================================================================
#ifdef __cplusplus
	#define NANITE_CPP  1
	#define NANITE_CONST constexpr
	#include <cstdint>
	// uint32_t, int32_t from <cstdint>
#else
	#define NANITE_SLANG 1
	#define NANITE_CONST static const
	// Slang/HLSL native types → stdint-compatible aliases
	typedef uint    uint32_t;   // HLSL 'uint' is guaranteed 32-bit
	typedef int     int32_t;
#endif

// ===================================================================
// Namespace (C++ only; Slang uses global scope or modules)
// ===================================================================
#if NANITE_CPP
namespace Tumbler::Nanite {
#endif

// ===================================================================
// Constants
// ===================================================================
NANITE_CONST uint32_t kClusterTriangleCount = 128;
NANITE_CONST uint32_t kClustersPerPage      = 32;

// ===================================================================
// Cleanup
// ===================================================================
#if NANITE_CPP
} // namespace Tumbler::Nanite
#endif

#undef NANITE_CONST
#undef NANITE_CPP
#undef NANITE_SLANG
