// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDParticleDataWrapper.h"
#include "Math/BoxSphereBounds.h"

#include "ChaosVDQueryDataWrappers.generated.h"

UENUM()
enum class EChaosVDSceneQueryType
{
	Invalid,
	Sweep,
	Overlap,
	RayCast
};

UENUM()
enum class EChaosVDSceneQueryMode
{
	Invalid,
	Single,
	Multi,
	Test
};

USTRUCT()
struct FChaosVDCollisionResponseParams : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		FlagsPerChannel = TArray(Other.CollisionResponse.EnumArray, UE_ARRAY_COUNT(Other.CollisionResponse.EnumArray));
		bHasValidData = true;
	}
	
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	TArray<uint8> FlagsPerChannel;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDCollisionResponseParams& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDCollisionResponseParams> : public TStructOpsTypeTraitsBase2<FChaosVDCollisionResponseParams>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT()
struct FChaosVDCollisionObjectQueryParams : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		ObjectTypesToQuery = Other.ObjectTypesToQuery;
		IgnoreMask = Other.IgnoreMask;
		bHasValidData = true;
	}
	
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	uint8 ObjectTypesToQuery = 0;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	uint8 IgnoreMask = 0;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDCollisionObjectQueryParams& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDCollisionObjectQueryParams> : public TStructOpsTypeTraitsBase2<FChaosVDCollisionObjectQueryParams>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT()
struct FChaosVDCollisionQueryParams : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		TraceTag = Other.TraceTag;
		OwnerTag = Other.OwnerTag;
		bTraceComplex = Other.bTraceComplex;
		bFindInitialOverlaps = Other.bFindInitialOverlaps;
		bReturnFaceIndex = Other.bReturnFaceIndex;
		bReturnPhysicalMaterial = Other.bReturnPhysicalMaterial;
		bIgnoreBlocks = Other.bIgnoreBlocks;
		bIgnoreTouches = Other.bIgnoreTouches;
		bSkipNarrowPhase = Other.bSkipNarrowPhase;
		bTraceIntoSubComponents = Other.bTraceIntoSubComponents;
		bReplaceHitWithSubComponents = Other.bReplaceHitWithSubComponents;
		IgnoreMask = Other.IgnoreMask;

		IgnoredComponentsIDs = Other.GetIgnoredComponents();
		IgnoredActorsIDs = Other.GetIgnoredActors();

		bHasValidData = true;
	}
	
	/** Tag used to provide extra information or filtering for debugging of the trace (e.g. Collision Analyzer) */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	FName TraceTag;

	/** Tag used to indicate an owner for this trace */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	FName OwnerTag;

	/** Whether we should trace against complex collision */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bTraceComplex = false;

	/** Whether we want to find out initial overlap or not. If true, it will return if this was initial overlap. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bFindInitialOverlaps = false;

	/** Whether we want to return the triangle face index for complex static mesh traces */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bReturnFaceIndex = false;

	/** Whether we want to include the physical material in the results. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bReturnPhysicalMaterial = false;

	/** Whether to ignore blocking results. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bIgnoreBlocks = false;

	/** Whether to ignore touch/overlap results. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bIgnoreTouches = false;

	/** Whether to skip narrow phase checks (only for overlaps). */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bSkipNarrowPhase = false;

	/** Whether to ignore traces to the cluster union and trace against its children instead. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bTraceIntoSubComponents = false;

	/** If bTraceIntoSubComponents is true, whether to replace the hit of the cluster union with its children instead. */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	bool bReplaceHitWithSubComponents = false;

	/** Extra filtering done on the query. See declaration for filtering logic */
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	uint8 IgnoreMask = 0;

	TArray<uint32> IgnoredComponentsIDs;

	TArray<uint32> IgnoredActorsIDs;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	TArray<FName> IgnoredActorsNames;
	
	UPROPERTY(VisibleAnywhere, Category=QueryData)
	TArray<FName> IgnoredComponentsNames;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDCollisionQueryParams& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDCollisionQueryParams> : public TStructOpsTypeTraitsBase2<FChaosVDCollisionQueryParams>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT()
struct FChaosVDQueryFastData : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	FVector Dir = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	FVector InvDir = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	float CurrentLength = 0.0f;

	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	float InvCurrentLength = 0.0f;

	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	bool bParallel0 = false;

	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	bool bParallel1 = false;

	UPROPERTY(VisibleAnywhere, Category=QueryFastData)
	bool bParallel2 = false;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDQueryFastData& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDQueryFastData> : public TStructOpsTypeTraitsBase2<FChaosVDQueryFastData>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT()
struct FChaosVDQueryHitData : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	float Distance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	int32 FaceIdx = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	uint16 Flags = 0;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	FVector WorldPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	FVector WorldNormal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	FVector FaceNormal = FVector::ZeroVector;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDQueryHitData& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDQueryHitData> : public TStructOpsTypeTraitsBase2<FChaosVDQueryHitData>
{
	enum
	{
		WithSerializer = true,
	};
};

UENUM()
enum class EChaosVDCollisionQueryHitType
{
	None = 0,
	Touch = 1,
	Block = 2
};

UENUM()
enum class EChaosVDSceneQueryVisitorType
{
	Invalid,
	BroadPhase,
	NarrowPhase
};

USTRUCT()
struct FChaosVDQueryVisitStep : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()
	
	inline static FStringView WrapperTypeName = TEXT("FChaosVDQueryVisitStep");

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	int32 OwningQueryID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	EChaosVDSceneQueryVisitorType Type = EChaosVDSceneQueryVisitorType::Invalid;

	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	uint32 ShapeIndex = 0;
	
	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	int32 ParticleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	FTransform ParticleTransform;

	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	FChaosVDQueryFastData QueryFastData;
	
	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	EChaosVDCollisionQueryHitType HitType = EChaosVDCollisionQueryHitType::None;

	UPROPERTY(VisibleAnywhere, Category="SQ Visit Data")
	FChaosVDQueryHitData HitData;

	// Editor only properties

	bool bIsSelectedInEditor = false;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDQueryVisitStep& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDQueryVisitStep> : public TStructOpsTypeTraitsBase2<FChaosVDQueryVisitStep>
{
	enum
	{
		WithSerializer = true,
	};
};

USTRUCT()
struct FChaosVDQueryDataWrapper
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDQueryDataWrapper");

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	UPROPERTY(VisibleAnywhere, Category="CVD QueryData")
	int32 ID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="CVD QueryData")
	int32 ParentQueryID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="CVD QueryData")
	int32 WorldSolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	bool bIsRetryQuery = false;

	uint32 InputGeometryKey = 0;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FQuat GeometryOrientation;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	EChaosVDSceneQueryType Type = EChaosVDSceneQueryType::Invalid;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	EChaosVDSceneQueryMode Mode = EChaosVDSceneQueryMode::Invalid;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	int32 CollisionChannel = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FChaosVDCollisionQueryParams CollisionQueryParams;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FChaosVDCollisionResponseParams CollisionResponseParams;

	UPROPERTY(VisibleAnywhere, Category=RecordedQueryData)
	FChaosVDCollisionObjectQueryParams CollisionObjectQueryParams;

	TArray<FChaosVDQueryVisitStep> SQVisitData;

	UPROPERTY(VisibleAnywhere, Category=QueryData)
	TArray<FChaosVDQueryVisitStep> Hits;

	TArray<int32> SubQueriesIDs;

	bool bIsSelectedInEditor = false;

	int32 CurrentVisitIndex = 0;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDQueryDataWrapper& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDQueryDataWrapper> : public TStructOpsTypeTraitsBase2<FChaosVDQueryDataWrapper>
{
	enum
	{
		WithSerializer = true,
	};
};
