// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Math/PreciseFP.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

/*-----------------------------------------------------------------------------
	FDoubleProperty.
-----------------------------------------------------------------------------*/

bool FDoubleProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return UE::PreciseFPEqual(*static_cast<const double*>(A), B ? *static_cast<const double*>(B) : 0.0);
}

uint32 FDoubleProperty::GetValueTypeHashInternal(const void* Src) const
{
	return UE::PreciseFPHash(*static_cast<const double*>(Src));
}