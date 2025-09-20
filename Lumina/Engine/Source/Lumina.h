#pragma once

#include "Containers/Array.h"
#include "Containers/Name.h"
#include "Containers/String.h"
#include "Core/Assertions/Assert.h"
#include "Core/Math/Math.h"
#include "Log/Log.h"
#include "Memory/Memory.h"
#include "Platform/Platform.h"


#define LUMINA_VERSION "0.01.0"
#define LUMINA_VERSION_MAJOR 0
#define LUMINA_VERSION_MINOR 01
#define LUMINA_VERSION_PATCH 0
#define LUMINA_VERSION_NUM 0010

#define WITH_DEVELOPMENT_TOOLS 1


#ifdef  _MSC_VER
#pragma warning (push)
#endif

// Pick an unsigned integer type big enough to hold N bytes
template <size_t N>
struct TBytesToUnsigned;

template <> struct TBytesToUnsigned<1> { using Type = uint8;  };
template <> struct TBytesToUnsigned<2> { using Type = uint16; };
template <> struct TBytesToUnsigned<4> { using Type = uint32; };
template <> struct TBytesToUnsigned<8> { using Type = uint64; };

#define LUMINA_STATIC_HELPER(InType)                                          \
static TBytesToUnsigned<sizeof(InType)>::Type StaticRawValue = 0;             \
InType& StaticValue = *reinterpret_cast<InType*>(&StaticRawValue);            \
if (!StaticRawValue)

// Invalid Index
constexpr int64 INDEX_NONE = -1;

