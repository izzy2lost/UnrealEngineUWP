// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstantMovementEffect.h"

FInstantMovementEffect* FInstantMovementEffect::Clone() const
{
	// If child classes don't override this, saved moves will not work
	checkf(false, TEXT("FInstantMovementEffect::Clone() being called erroneously. This should always be overridden in child classes!"));
	return nullptr;
}

void FInstantMovementEffect::NetSerialize(FArchive& Ar)
{
	
}

UScriptStruct* FInstantMovementEffect::GetScriptStruct() const
{
	return FInstantMovementEffect::StaticStruct();
}

FString FInstantMovementEffect::ToSimpleString() const
{
	return GetScriptStruct()->GetName();
}
