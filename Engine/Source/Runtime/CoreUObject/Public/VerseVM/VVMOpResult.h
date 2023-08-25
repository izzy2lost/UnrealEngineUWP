// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !WITH_VERSE_VM
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "CoreTypes.h"
#include "VVMValue.h"

namespace Verse
{

// Represents the result of a single VM operation
struct FOpResult
{
	enum EKind
	{
		Normal,        // All went well, Value is the result
		Failed,        // Something went wrong, Value is undefined
		ShouldSuspend, // A placeholder was encountered among the arguments and Value is this placeholder
		RuntimeError   // A runtime error occurred, and Value holds a VUTF8String with an error message
	};

	FOpResult(EKind Kind, VValue Value = VValue())
		: Kind(Kind)
		, Value(Value)
	{
	}

	EKind Kind;
	VValue Value;
};

} // namespace Verse
