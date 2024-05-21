// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "Misc/HashBuilder.h"

#if WITH_EDITOR
struct FCellCoord
{
	FCellCoord(int64 InX, int64 InY, int64 InZ, int32 InLevel)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, Level(InLevel)
	{}

	static FCellCoord Invalid;

	inline FString ToString2D() const
	{
		check(!Z);
		return FString::Printf(TEXT("L%d_X%" INT64_FMT "_Y%" INT64_FMT), Level, X, Y);
	}

	inline FString ToString3D() const
	{
		return FString::Printf(TEXT("L%d_X%" INT64_FMT "_Y%" INT64_FMT "_Z%" INT64_FMT), Level, X, Y, Z);
	}

	inline bool operator==(const FCellCoord& Other) const
	{
		return (X == Other.X) && (Y == Other.Y) && (Z == Other.Z) && (Level == Other.Level);
	}

	static inline int32 GetLevelForBox(const FBox& InBox, int32 InCellSize)
	{
		const FVector Extent = InBox.GetSize();
		const FVector::FReal MaxLength = Extent.GetMax();
		return FMath::CeilToInt32(FMath::Max<FVector::FReal>(FMath::Log2(MaxLength / InCellSize), 0));
	}

	static inline FCellCoord GetCellCoords(const FVector& InPos, int32 InCellSize, int32 InLevel, const FVector& InOrigin)
	{
		check(InLevel >= 0);
		const int64 CellSizeForLevel = (int64)InCellSize * (1LL << InLevel);
		return FCellCoord(
			FMath::FloorToInt((InPos.X - InOrigin.X) / CellSizeForLevel),
			FMath::FloorToInt((InPos.Y - InOrigin.Y) / CellSizeForLevel),
			FMath::FloorToInt((InPos.Z - InOrigin.Z) / CellSizeForLevel),
			InLevel
		);
	}

	static inline FBox GetCellBounds(const FCellCoord& InCellCoord, int32 InCellSize, const FVector& InOrigin)
	{
		check(InCellCoord.Level >= 0);
		const int64 CellSizeForLevel = (int64)InCellSize * (1LL << InCellCoord.Level);
		const FVector Min = InOrigin + FVector(
			static_cast<FVector::FReal>(InCellCoord.X * CellSizeForLevel), 
			static_cast<FVector::FReal>(InCellCoord.Y * CellSizeForLevel), 
			static_cast<FVector::FReal>(InCellCoord.Z * CellSizeForLevel)
		);
		const FVector Max = Min + FVector(static_cast<double>(CellSizeForLevel));
		return FBox(Min, Max);
	}

	friend uint32 GetTypeHash(const FCellCoord& CellCoord)
	{
		FHashBuilder HashBuilder;
		HashBuilder << CellCoord.X << CellCoord.Y << CellCoord.Z << CellCoord.Level;
		return HashBuilder.GetHash();
	}

	int64 X;
	int64 Y;
	int64 Z;
	int32 Level;
};

FCellCoord FCellCoord::Invalid(0, 0, 0, -1);

bool URuntimePartitionLHGrid::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FString PropertyName = InProperty->GetName();
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(URuntimePartitionLHGrid, bIs2D))
		{
			return HLODIndex == INDEX_NONE;
		}
	}

	return Super::CanEditChange(InProperty);
}

static bool GPackageWasDirty = false;
void URuntimePartitionLHGrid::PreEditChange(FProperty* InPropertyAboutToChange)
{
	GPackageWasDirty = GetPackage()->IsDirty();
	Super::PreEditChange(InPropertyAboutToChange);
}

void URuntimePartitionLHGrid::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(URuntimePartitionLHGrid, CellSize))
	{
		CellSize = FMath::Max<int32>(CellSize, 1600);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(URuntimePartitionLHGrid, bShowGridPreview))
	{
		if (bShowGridPreview)
		{
			check(!WorldGridPreviewer);
			WorldGridPreviewer = MakeUnique<FWorldGridPreviewer>(GetTypedOuter<UWorld>(), false);
		}
		else
		{
			check(WorldGridPreviewer);
			WorldGridPreviewer.Reset();
		}

		if (!GPackageWasDirty)
		{
			GetPackage()->ClearDirtyFlag();
		}
	}

	if (WorldGridPreviewer)
	{
		WorldGridPreviewer->CellSize = CellSize;
		WorldGridPreviewer->GridColor = DebugColor;
		WorldGridPreviewer->GridOffset = Origin;
		WorldGridPreviewer->LoadingRange = LoadingRange;
		WorldGridPreviewer->Update();
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

void URuntimePartitionLHGrid::InitHLODRuntimePartitionFrom(const URuntimePartition* InRuntimePartition, int32 InHLODIndex)
{
	Super::InitHLODRuntimePartitionFrom(InRuntimePartition, InHLODIndex);
	const URuntimePartitionLHGrid* RuntimePartitionLHGrid = CastChecked<const URuntimePartitionLHGrid>(InRuntimePartition);
	CellSize = RuntimePartitionLHGrid->CellSize * 2;
	bIs2D = RuntimePartitionLHGrid->bIs2D;
}

void URuntimePartitionLHGrid::UpdateHLODRuntimePartitionFrom(const URuntimePartition* InRuntimePartition)
{
	Super::UpdateHLODRuntimePartitionFrom(InRuntimePartition);
	const URuntimePartitionLHGrid* RuntimePartitionLHGrid = CastChecked<const URuntimePartitionLHGrid>(InRuntimePartition);
	bIs2D = RuntimePartitionLHGrid->bIs2D;
}

void URuntimePartitionLHGrid::SetDefaultValues()
{
	Super::SetDefaultValues();
	CellSize = LoadingRange / 2;
}
#endif

bool URuntimePartitionLHGrid::IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const
{
	return InPartitionTokens.Num() == 1;
}

#if WITH_EDITOR
bool URuntimePartitionLHGrid::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);
	const FVector SpaceMask = FVector(1, 1, bIs2D ? 0 : 1);

	TMap<FCellCoord, TArray<const IStreamingGenerationContext::FActorSetInstance*>> CellsActorSetInstances;
	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : *InParams.ActorSetInstances)
	{
		if (ActorSetInstance->bIsSpatiallyLoaded)
		{
			const FBox ActorSetInstanceBounds = FBox(ActorSetInstance->Bounds.Min * SpaceMask, ActorSetInstance->Bounds.Max * SpaceMask);
			const int32 GridLevel = FCellCoord::GetLevelForBox(ActorSetInstanceBounds, CellSize);
			const FCellCoord CellCoord = FCellCoord::GetCellCoords(ActorSetInstanceBounds.GetCenter(), CellSize, GridLevel, Origin * SpaceMask);
			CellsActorSetInstances.FindOrAdd(CellCoord).Add(ActorSetInstance);
		}
		else
		{
			CellsActorSetInstances.FindOrAdd(FCellCoord::Invalid).Add(ActorSetInstance);
		}
	}

	for (auto& [CellCoord, CellActorSetInstances] : CellsActorSetInstances)
	{
		const bool bIsSpatiallyLoaded = CellCoord != FCellCoord::Invalid;

		URuntimePartition::FCellDesc& CellDesc = OutResult.RuntimeCellDescs.Emplace_GetRef(CreateCellDesc(bIs2D ? CellCoord.ToString2D() : CellCoord.ToString3D(), bIsSpatiallyLoaded, CellCoord.Level, CellActorSetInstances));

		if (bIsSpatiallyLoaded)
		{
			CellDesc.CellBounds = FCellCoord::GetCellBounds(CellCoord, CellSize, Origin * SpaceMask);
			CellDesc.bIs2D = bIs2D;
		}
	}

	return true;
}

FArchive& URuntimePartitionLHGrid::AppendCellGuid(FArchive& InAr)
{
	return Super::AppendCellGuid(InAr) << CellSize;
}
#endif
