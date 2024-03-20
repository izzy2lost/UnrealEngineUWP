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

	UE_REQUIRES_EXPR() wraps the effects of a requires expression, used to test the
	compilability of an expression based on a deduced template parameter.  Usage:

	template <typename Type>
	struct TSmartPtr
	{
		// Only enable this constructor if the incoming pointer type is a UObject
		// convertible to the smart pointer type:
		template <
			typename OtherType
			UE_REQUIRES(
				std::is_base_of_v<UObject, OtherType> &&
				UE_REQUIRES_EXPR(ImplicitConv<Type*>((OtherType*)nullptr)))
			)
		>
		explicit TSmartPtr(OtherType* OtherPtr)
			: Ptr(Cast<Type>(OtherPtr))
		{
		}

	private:
		BaseType* Ptr;
	};

	Unlike a C++20 requires expression, UE_REQUIRES_EXPR() can only be used in
	the body of a UE_REQUIRES() macro - standalone concept checks must still be
	done via something like a TModels-type concept.
 -----------------------------------------------------------------------------*/
#if __cplusplus < 202000
	#define UE_REQUIRES(...) , std::enable_if_t<(__VA_ARGS__), int> = 0
	#define UE_REQUIRES_EXPR(...) (!std::is_same_v<decltype(__VA_ARGS__), long long************>) // This is highly unlikely to be the type of any expression
#else
	namespace UE::Core::Private
	{
		// Only needed for the UE_REQUIRES macro to work, to allow for a trailing > token after the macro
		template <bool B>
		concept BoolIdentityConcept = B;
	}

	#define UE_REQUIRES(...) > requires (!!(__VA_ARGS__)) && UE::Core::Private::BoolIdentityConcept<true
    #define UE_REQUIRES_EXPR(...) requires { (__VA_ARGS__); }
#endif

// This should be regarded as deprecated - please use UE_REQUIRES instead.
#define TEMPLATE_REQUIRES(...) typename TEnableIf<__VA_ARGS__, int>::type = 0
