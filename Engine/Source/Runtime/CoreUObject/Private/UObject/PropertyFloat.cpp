// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Math/PreciseFP.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

/*-----------------------------------------------------------------------------
	FFloatProperty.
-----------------------------------------------------------------------------*/

bool FFloatProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	return UE::PreciseFPEqual(*static_cast<const float*>(A), B ? *static_cast<const float*>(B) : 0.0f);
}

uint32 FFloatProperty::GetValueTypeHashInternal(const void* Src) const
{
	return UE::PreciseFPHash(*static_cast<const float*>(Src));
}
