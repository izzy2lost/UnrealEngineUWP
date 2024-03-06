// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/EnableIf.h"
#include <type_traits>

/*-----------------------------------------------------------------------------
	Readability macro for a constraint in template definitions, future-proofed
	for C++ 20 concepts. Usage:

	template <
		typename T,
		typename U  // note - no trailing comma before the constraint
		UE_REQUIRES(std::is_integral_v<T> && sizeof(U) <= 4)
	>
	void IntegralUpTo32Bit(T Lhs, U Rhs) {}
 -----------------------------------------------------------------------------*/
#if __cplusplus < 202000
	#define UE_REQUIRES(...) , std::enable_if_t<(__VA_ARGS__), int> = 0
#else
	namespace UE::Core::Private
	{
		// Only needed for the UE_REQUIRES macro to work, to allow for a trailing > token after the macro
		template <bool B>
		concept BoolIdentityConcept = B;
	}

	#define UE_REQUIRES(...) > requires (!!(__VA_ARGS__)) && UE::Core::Private::BoolIdentityConcept<true
#endif

// This should be regarded as deprecated - please use UE_REQUIRES instead.
#define TEMPLATE_REQUIRES(...) typename TEnableIf<__VA_ARGS__, int>::type = 0
