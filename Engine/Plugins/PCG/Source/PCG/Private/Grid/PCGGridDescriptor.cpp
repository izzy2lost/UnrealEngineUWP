// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGGridDescriptor.h"
#include "Serialization/ArchiveCrc32.h"

bool FPCGGridDescriptor::operator==(const FPCGGridDescriptor& Other) const
{
	return GridSize == Other.GridSize && bIs2DGrid == Other.bIs2DGrid && bIsRuntime == Other.bIsRuntime;
}

uint32 FPCGGridDescriptor::ComputeHash() const
{
	FArchiveCrc32 Ar;
	FPCGGridDescriptor* NonConstThis = const_cast<FPCGGridDescriptor*>(this);

	Ar << NonConstThis->GridSize << NonConstThis->bIs2DGrid << NonConstThis->bIsRuntime;

	return Ar.GetCrc();
}

bool FPCGGridCellDescriptor::operator==(const FPCGGridCellDescriptor& InOther) const
{
	return Descriptor == InOther.Descriptor && GridCoords == InOther.GridCoords;
}

uint32 GetTypeHash(const FPCGGridCellDescriptor& In)
{
	return HashCombine(GetTypeHash(In.Descriptor), GetTypeHash(In.GridCoords));
}
