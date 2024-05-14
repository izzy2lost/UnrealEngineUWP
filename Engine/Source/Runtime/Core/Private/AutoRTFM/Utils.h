// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM/AutoRTFM.h"
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAutoRTFM, Display, All)

namespace AutoRTFM
{

[[noreturn]] void PrettyAbort(const char* const File, const unsigned Line, const char* const Function, const char* const Expression);

[[noreturn]] inline void Unreachable()
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("Unreachable encountered!"));

#if defined(_MSC_VER) && !defined(__clang__)
    __assume(false);
#else
    __builtin_unreachable();
#endif // PLATFORM_WINDOWS
}

FString GetFunctionDescription(void* FunctionPtr);

template<typename TReturnType, typename... TParameterTypes>
FString GetFunctionDescription(TReturnType (*FunctionPtr)(TParameterTypes...))
{
    return GetFunctionDescription(reinterpret_cast<void*>(FunctionPtr));
}

template<size_t A, size_t B> struct PrettyStaticAssert final
{
  static_assert(A == B, "Not equal");
  static constexpr bool _cResult = (A == B);
};

} // namespace AutoRTFM

#if defined(_MSC_VER) && !defined(__clang__)
	#define ASSERT(exp) do { if (UNLIKELY(!(exp))) { UE_DEBUG_BREAK(); PrettyAbort(__FILE__, __LINE__, __FUNCSIG__, #exp); } } while (false)
#else
	#define ASSERT(exp) do { if (UNLIKELY(!(exp))) { UE_DEBUG_BREAK(); PrettyAbort(__FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); } } while (false)
#endif

#if defined(__has_feature)
	#if __has_feature(address_sanitizer)
		#define AUTORTFM_NO_ASAN [[clang::no_sanitize("address")]]
	#endif
#endif

#if !defined(AUTORTFM_NO_ASAN)
	#define AUTORTFM_NO_ASAN
#endif
