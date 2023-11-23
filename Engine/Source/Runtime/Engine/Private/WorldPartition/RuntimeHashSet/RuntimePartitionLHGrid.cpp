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

	int64 X;
	int64 Y;
	int64 Z;
	int32 Level;

	inline FString ToString() const
	{
		return FString::Printf(TEXT("%d_%d_%d_%d"), X, Y, Z, Level);
	}

	inline bool operator==(const FCellCoord& Other) const
	{
		return (X == Other.X) && (Y == Other.Y) && (Z == Other.Z) && (Level == Other.Level);
	}

	static inline int32 GetLevelForBox(const FBox& InBox, int32 InCellSize)
	{
		const FVector Extent = InBox.GetExtent();
		const FVector::FReal MaxLength = Extent.GetMax() * 2.0;
		return FMath::CeilToInt32(FMath::Max<FVector::FReal>(FMath::Log2(MaxLength / InCellSize), 0));
	}

	static inline FCellCoord GetCellCoords(const FVector& InPos, int32 InCellSize, int32 InLevel)
	{
		check(InLevel >= 0);
		const int64 CellSizeForLevel = (int64)InCellSize * (1LL << InLevel);
		return FCellCoord(
			FMath::FloorToInt(InPos.X / CellSizeForLevel),
			FMath::FloorToInt(InPos.Y / CellSizeForLevel),
			FMath::FloorToInt(InPos.Z / CellSizeForLevel),
			InLevel
		);
	}

	friend uint32 GetTypeHash(const FCellCoord& CellCoord)
	{
		FHashBuilder HashBuilder;
		HashBuilder << CellCoord.X << CellCoord.Y << CellCoord.Z << CellCoord.Level;
		return HashBuilder.GetHash();
	}
};

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
		WorldGridPreviewer->GridOffset = FVector::ZeroVector;
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
}

void URuntimePartitionLHGrid::SetDefaultValues()
{
	Super::SetDefaultValues();
	CellSize = LoadingRange / 2;
}

bool URuntimePartitionLHGrid::IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const
{
	return InPartitionTokens.Num() == 1;
}

bool URuntimePartitionLHGrid::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	TMap<FCellCoord, TArray<const IStreamingGenerationContext::FActorSetInstance*>> CellsActorSetInstances;
	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : *InParams.ActorSetInstances)
	{
		const int32 GridLevel = FCellCoord::GetLevelForBox(ActorSetInstance->Bounds, CellSize);
		const FCellCoord CellCoord = FCellCoord::GetCellCoords(ActorSetInstance->Bounds.GetCenter(), CellSize, GridLevel);
		CellsActorSetInstances.FindOrAdd(CellCoord).Add(ActorSetInstance);
	}

	for (auto& [CellCoord, CellActorSetInstances] : CellsActorSetInstances)
	{
		OutResult.RuntimeCellDescs.Emplace(CreateCellDesc(CellCoord.ToString(), true, CellCoord.Level, CellActorSetInstances));
	}

	return true;
}
#endif