// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDDataSerializationMacros.h"
#include "ChaosVDParticleDataWrapper.h"

#include "ChaosVDDebugShapeDataWrapper.generated.h"

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDDebugDrawShapeBase : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SolverID = INDEX_NONE;
	
	UPROPERTY()
	FName Tag = NAME_None;

	UPROPERTY()
	FColor Color = FColor::Blue;

protected:
	void SerializeBase_Internal(FArchive& Ar);
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDDebugDrawBoxDataWrapper : public FChaosVDDebugDrawShapeBase
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDDDebugDrawBoxDataWrapper");

	UPROPERTY()
	FBox Box = FBox(ForceInitToZero);

	bool Serialize(FArchive& Ar);
};

CVD_IMPLEMENT_SERIALIZER(FChaosVDDebugDrawBoxDataWrapper)

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDDebugDrawSphereDataWrapper : public FChaosVDDebugDrawShapeBase
{
	GENERATED_BODY()
	
	inline static FStringView WrapperTypeName = TEXT("FChaosVDDebugDrawSphereDataWrapper");

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;
	
	UPROPERTY()
	float Radius = 0.0f;

	bool Serialize(FArchive& Ar);
};

CVD_IMPLEMENT_SERIALIZER(FChaosVDDebugDrawSphereDataWrapper)

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDDebugDrawLineDataWrapper : public FChaosVDDebugDrawShapeBase
{
	GENERATED_BODY()
	
	inline static FStringView WrapperTypeName = TEXT("FChaosVDDebugDrawLineDataWrapper");

	UPROPERTY()
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY()
	bool bIsArrow = false;

	bool Serialize(FArchive& Ar);
};

CVD_IMPLEMENT_SERIALIZER(FChaosVDDebugDrawLineDataWrapper)

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDDebugDrawImplicitObjectDataWrapper : public FChaosVDDebugDrawShapeBase
{
	GENERATED_BODY()
	
	inline static FStringView WrapperTypeName = TEXT("FChaosVDDebugDrawImplicitObjectDataWrapper");

	uint32 ImplicitObjectHash = 0;

	FTransform ParentTransform = FTransform();

	bool Serialize(FArchive& Ar);
};
