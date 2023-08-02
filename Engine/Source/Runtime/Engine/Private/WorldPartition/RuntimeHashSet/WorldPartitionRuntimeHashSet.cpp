// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleBase.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionDraw2DContext.h"
#include "WorldPartition/WorldPartitionDebugHelper.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Math/TransformCalculus2D.h"
#include "Misc/ArchiveMD5.h"

FAutoConsoleCommand WorldPartitionRuntimeHashSetEnable(
	TEXT("wp.Editor.WorldPartitionRuntimeHashSet.Enable"),
	TEXT("Enable experimental runtime hash set class."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWorldPartitionRuntimeHashSet::StaticClass()->ClassFlags &= ~CLASS_HideDropDown;
	})
);

static int32 GShowRuntimeHashSetDebugDisplayLevel = 0;
static FAutoConsoleVariableRef CVarShowRuntimeHashSetDebugDisplayLevel(
	TEXT("wp.Runtime.HashSet.ShowDebugDisplayLevel"),
	GShowRuntimeHashSetDebugDisplayLevel,
	TEXT("Used to choose which level to display when showing runtime partitions."));

static int32 GShowRuntimeHashSetDebugDisplayLevelCount = 1;
static FAutoConsoleVariableRef CVarShowRuntimeHashSetDebugDisplayLevelCount(
	TEXT("wp.Runtime.HashSet.ShowDebugDisplayLevelCount"),
	GShowRuntimeHashSetDebugDisplayLevelCount,
	TEXT("Used to choose how many levels to display when showing runtime partitions."));

static int32 GShowRuntimeHashSetDebugDisplayMode = 0;
static FAutoConsoleVariableRef CVarShowRuntimeHashSetDebugDisplayMode(
	TEXT("wp.Runtime.HashSet.ShowDebugDisplayMode"),
	GShowRuntimeHashSetDebugDisplayMode,
	TEXT("Used to choose what mode to display when showing runtime partitions (0=Level Streaming State, 1=Data Layers, 2=Content Bundles)."));

#if WITH_EDITOR
void FRuntimePartitionDesc::UpdateHLODPartitionLayers()
{
	if (!Class || !MainLayer || !HLODLayer)
	{
		HLODSetups.Empty();
	}
	else
	{
		TSet<const UHLODLayer*> VisitedHLODLayers;

		const UHLODLayer* CurHLODLayer = HLODLayer;
		while (CurHLODLayer)
		{
			const int32 HLODSetupIndex = VisitedHLODLayers.Num();

			bool bHLODLayerWasAlreadyInSet;
			VisitedHLODLayers.Add(CurHLODLayer, &bHLODLayerWasAlreadyInSet);
			if (bHLODLayerWasAlreadyInSet)
			{
				break;
			}

			if (!HLODSetups.IsValidIndex(HLODSetupIndex))
			{
				HLODSetups.AddDefaulted();
			}

			FRuntimePartitionHLODSetup& HLODSetup = HLODSetups[HLODSetupIndex];

			const bool bHLODLayerMatches = HLODSetup.HLODLayer == CurHLODLayer;
			const UClass* ExpectedHLODPartitionClass = CurHLODLayer->IsSpatiallyLoaded() ? MainLayer->GetClass() : URuntimePartitionPersistent::StaticClass();
			const bool bHasValidPartitionLayer = HLODSetup.PartitionLayer && (HLODSetup.PartitionLayer->GetClass() == ExpectedHLODPartitionClass);
			
			if (!bHLODLayerMatches || !bHasValidPartitionLayer)
			{
				HLODSetup.HLODLayer = CurHLODLayer;
				HLODSetup.PartitionLayer = CurHLODLayer->IsSpatiallyLoaded() ? DuplicateObject<URuntimePartition>(MainLayer, MainLayer->GetOuter()) : NewObject<URuntimePartition>(MainLayer->GetOuter(), ExpectedHLODPartitionClass);
				HLODSetup.PartitionLayer->Name = CurHLODLayer->GetFName();
				HLODSetup.PartitionLayer->bIsHLODSetup = true;
			}

			CurHLODLayer = CurHLODLayer->GetParentLayer();	
		}

		HLODSetups.SetNum(VisitedHLODLayers.Num());
	}
}
#endif

void FRuntimePartitionStreamingData::CreatePartitionsSpatialIndex() const
{
	if (!SpatialIndex)
	{
		SpatialIndex = MakeUnique<FStaticSpatialIndexType>();

		TArray<TPair<FBox, TObjectPtr<UWorldPartitionRuntimeCell>>> PartitionsElements;
		Algo::Transform(StreamingCells, PartitionsElements, [](UWorldPartitionRuntimeCell* Cell)
		{
			return TPair<FBox, TObjectPtr<UWorldPartitionRuntimeCell>>(Cell->GetContentBounds(), Cell);
		});
		SpatialIndex->Init(PartitionsElements);
	}
}

void FRuntimePartitionStreamingData::DestroyPartitionsSpatialIndex() const
{
	SpatialIndex.Reset();
}

void URuntimeHashSetExternalStreamingObject::CreatePartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		StreamingData.CreatePartitionsSpatialIndex();
	}
}

void URuntimeHashSetExternalStreamingObject::DestroyPartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		StreamingData.DestroyPartitionsSpatialIndex();
	}
}

void URuntimeHashSetExternalStreamingObject::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	URuntimeHashSetExternalStreamingObject* This = CastChecked<URuntimeHashSetExternalStreamingObject>(InThis);
	for (const FRuntimePartitionStreamingData& StreamingData : This->RuntimeStreamingData)
	{
		if (StreamingData.SpatialIndex.IsValid())
		{
			StreamingData.SpatialIndex->AddReferencedObjects(Collector);
		}
	}
#endif

	Super::AddReferencedObjects(InThis, Collector);
}

UWorldPartitionRuntimeHashSet::UWorldPartitionRuntimeHashSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

void UWorldPartitionRuntimeHashSet::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!GetTypedOuter<UWorld>()->IsGameWorld())
	{
		UpdateHLODPartitionLayers();
	}
	else
#endif
	{
		ForEachStreamingData([](const FRuntimePartitionStreamingData& StreamingData)
		{
			StreamingData.CreatePartitionsSpatialIndex();
		});
	}
}

#if WITH_EDITOR
static EWorldPartitionRuntimeCellVisualizeMode GetStreamingCellVisualizeMode()
{
	const EWorldPartitionRuntimeCellVisualizeMode VisualizeMode = FWorldPartitionDebugHelper::IsRuntimeSpatialHashCellStreamingPriorityShown() ? EWorldPartitionRuntimeCellVisualizeMode::StreamingPriority : EWorldPartitionRuntimeCellVisualizeMode::StreamingStatus;
	return VisualizeMode;
}

static TMap<FName, FColor> GetDataLayerDebugColors(const UWorldPartition* InWorldPartition)
{
	TMap<FName, FColor> DebugColors;
	if (const UDataLayerManager* DataLayerManager = InWorldPartition->GetDataLayerManager())
	{
		DataLayerManager->ForEachDataLayerInstance([&DebugColors](UDataLayerInstance* DataLayerInstance)
		{
			DebugColors.Add(DataLayerInstance->GetDataLayerFName(), DataLayerInstance->GetDebugColor());
			return true;
		});
	}
	return DebugColors;
}

bool UWorldPartitionRuntimeHashSet::Draw2D(FWorldPartitionDraw2DContext& DrawContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionRuntimeHashSet::Draw2D);

	const UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	const TArray<FWorldPartitionStreamingSource>& Sources = WorldPartition->GetStreamingSources();
	
	if (!Sources.Num())
	{
		return false;
	}

	TMap<FName, TArray<const FRuntimePartitionStreamingData*>> FilteredStreamingObjects;
	ForEachStreamingData([&FilteredStreamingObjects](const FRuntimePartitionStreamingData& StreamingData)
	{
		if (StreamingData.StreamingCells.Num())
		{
			if (FWorldPartitionDebugHelper::IsDebugRuntimeHashGridShown(StreamingData.Name))
			{
				FilteredStreamingObjects.FindOrAdd(StreamingData.Name).Add(&StreamingData);
			}
		}
	});

	if (!FilteredStreamingObjects.Num())
	{
		return false;
	}

	const FTransform WorldPartitionTransform = WorldPartition->GetInstanceTransform();
	const FTransform2D LocalWPToGlobal(FQuat2D(FMath::DegreesToRadians(WorldPartitionTransform.Rotator().Yaw)), FVector2D(WorldPartitionTransform.GetLocation()));
	const FTransform2D GlobalToLocalWP = LocalWPToGlobal.Inverse();
	const FBox2D& WorldRegion = DrawContext.GetWorldRegion();
	const FVector2D WorldRegionSize = WorldRegion.GetSize();
	TArray<FVector2D> Points;
	Points.Add(GlobalToLocalWP.TransformPoint(WorldRegion.Min));
	Points.Add(GlobalToLocalWP.TransformPoint(WorldRegion.Min + FVector2D(WorldRegionSize.X, 0)));
	Points.Add(GlobalToLocalWP.TransformPoint(WorldRegion.Min + WorldRegionSize));
	Points.Add(GlobalToLocalWP.TransformPoint(WorldRegion.Min + FVector2D(0, WorldRegionSize.Y)));
	const FBox2D LocalRegion(Points);

	const FBox2D& CanvasRegion = DrawContext.GetCanvasRegion();
	const int32 GridScreenWidthDivider = DrawContext.IsDetailedMode() ? FilteredStreamingObjects.Num() : 1;
	const float GridScreenWidthShrinkSize = GridScreenWidthDivider > 1 ? 20.f : 0.f;
	const float CanvasMaxScreenWidth = CanvasRegion.GetSize().X;
	const float GridMaxScreenWidth = CanvasMaxScreenWidth / GridScreenWidthDivider;
	const float GridEffectiveScreenWidth = FMath::Min(GridMaxScreenWidth, CanvasRegion.GetSize().Y) - GridScreenWidthShrinkSize;
	const FVector2D PartitionCanvasSize = FVector2D(CanvasRegion.GetSize().GetMin());
	const FVector2D GridScreenExtent = FVector2D(GridEffectiveScreenWidth, GridEffectiveScreenWidth);
	const FVector2D GridScreenHalfExtent = 0.5f * GridScreenExtent;
	const FVector2D GridScreenInitialOffset = CanvasRegion.Min;

	auto DrawStreamingData = [this](const FRuntimePartitionStreamingData* StreamingData, const FBox2D& Region2D, const FBox2D& GridScreenBounds, TFunctionRef<FVector2D(const FVector2D&, bool)> InWorldToScreen, FWorldPartitionDraw2DContext& DrawContext)
	{
		const UWorldPartition* WorldPartition = GetOuterUWorldPartition();
		const TArray<FWorldPartitionStreamingSource>& Sources = WorldPartition->GetStreamingSources();

		const FBox Region(FVector(Region2D.Min.X, Region2D.Min.Y, 0), FVector(Region2D.Max.X, Region2D.Max.Y, 0));
		const UWorld* OwningWorld = WorldPartition->GetWorld();
		const EWorldPartitionRuntimeCellVisualizeMode VisualizeMode = GetStreamingCellVisualizeMode();
		const UContentBundleManager* ContentBundleManager = OwningWorld->ContentBundleManager;
		TMap<FName, FColor> DataLayerDebugColors = GetDataLayerDebugColors(WorldPartition);

		auto WorldToScreen = [&](const FVector2D& Pos, bool bIsLocal = true)
		{
			return InWorldToScreen(Pos, bIsLocal);
		};

		TArray<const UWorldPartitionRuntimeCell*> FilteredCells;
		const FBox Region3D(FVector(Region.Min.X, Region.Min.Y, -HALF_WORLD_MAX), FVector(Region.Max.X, Region.Max.Y, HALF_WORLD_MAX));		
		StreamingData->SpatialIndex->ForEachIntersectingElement(Region3D, [&FilteredCells](UWorldPartitionRuntimeCell* Cell)
		{
			UWorldPartitionRuntimeCellDataSpatialHashSet* RuntimeCellDataHashSet = CastChecked<UWorldPartitionRuntimeCellDataSpatialHashSet>(Cell->RuntimeCellData);

			if ((RuntimeCellDataHashSet->Level >= GShowRuntimeHashSetDebugDisplayLevel) && (RuntimeCellDataHashSet->Level < (GShowRuntimeHashSetDebugDisplayLevel + GShowRuntimeHashSetDebugDisplayLevelCount)))
			{
				FilteredCells.Add(Cell);
			}
		});

		if (FilteredCells.Num())
		{
			for (const UWorldPartitionRuntimeCell* Cell : FilteredCells)
			{
				const FVector2D CellBoundsSize = FVector2D(Cell->GetCellBounds().GetSize());
				const FVector2D CellBoundsMin = FVector2D(Cell->GetCellBounds().Min);

				float CellOpacity = 0.0f;;
				TArray<FLinearColor> CellColors;

				switch (GShowRuntimeHashSetDebugDisplayMode)
				{
				case 0:
					CellColors.Add(Cell->GetDebugColor(VisualizeMode));
					CellOpacity = 0.25f / FMath::Max<float>(GShowRuntimeHashSetDebugDisplayLevelCount, 1);
					break;
				case 1:
					if (DataLayerDebugColors.Num() && Cell->GetDataLayers().Num())
					{
						for (const FName& DataLayer : Cell->GetDataLayers())
						{
							CellColors.Add(DataLayerDebugColors[DataLayer]);
						}
						CellOpacity = 0.67f;
					}
					break;
				case 2:
					if (ContentBundleManager && Cell->GetContentBundleID().IsValid())
					{
						if (const FContentBundleBase* ContentBundle = ContentBundleManager->GetContentBundle(OwningWorld, Cell->GetContentBundleID()))
						{
							check(ContentBundle->GetDescriptor());
							CellColors.Add(ContentBundle->GetDescriptor()->GetDebugColor());
							CellOpacity = 0.67f;
						}
					}
					break;
				}

				if (CellColors.IsEmpty())
				{
					CellColors.Add(FLinearColor::White);
					CellOpacity = 0.1f;
				}

				FVector2D::FReal BoundsOffsetX = 0;
				const FVector2D::FReal BoundsOffsetStepX = CellBoundsSize.X / CellColors.Num();
				for (const FLinearColor& CellColor : CellColors)
				{
					const FVector2D EffectiveCellBoundsMin(CellBoundsMin.X + BoundsOffsetX, CellBoundsMin.Y);
					const FVector2D EffectiveCellBoundsSize(BoundsOffsetStepX, CellBoundsSize.Y);
					DrawContext.LocalDrawTile(GridScreenBounds, EffectiveCellBoundsMin, EffectiveCellBoundsSize, CellColor.CopyWithNewOpacity(CellOpacity), WorldToScreen);
					BoundsOffsetX += BoundsOffsetStepX;
				}
				
				DrawContext.LocalDrawBox(GridScreenBounds, CellBoundsMin, CellBoundsSize, FLinearColor::Black, 1, WorldToScreen);
			}
		}

		// Draw X/Y Axis
		if (DrawContext.GetDrawGridAxis())
		{
			DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(FVector2D(-1638400.f, 0.f), false), WorldToScreen(FVector2D(1638400.f, 0.f), false), FLinearColor::Red, 3);
			DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(FVector2D(0.f, -1638400.f), false), WorldToScreen(FVector2D(0.f, 1638400.f), false), FLinearColor::Green, 3);
		}

		// Draw Grid Bounds
		if (DrawContext.GetDrawGridBounds())
		{
			const FBox2D Bounds = GridScreenBounds.ExpandBy(FVector2D(10));
			const FVector2D Size = GridScreenBounds.GetSize();
			DrawContext.PushDrawBox(Bounds, GridScreenBounds.Min, GridScreenBounds.Min + FVector2D(Size.X, 0), GridScreenBounds.Max, GridScreenBounds.Min + FVector2D(0, Size.Y), FLinearColor::White, 1);
		}

		// Draw Streaming Sources
		for (const FWorldPartitionStreamingSource& Source : Sources)
		{
			const FColor Color = Source.GetDebugColor();
			// @todo_jfd
			const FSoftObjectPath HLODLayer;
			Source.ForEachShape(StreamingData->LoadingRange, StreamingData->Name, HLODLayer, true, [&Color, &WorldToScreen, &GridScreenBounds, &DrawContext, this](const FSphericalSector& Shape)
			{
				check(!Shape.IsNearlyZero())

				// Spherical Sector
				const FVector2D Center2D(FVector2D(Shape.GetCenter()));
				const FSphericalSector::FReal Angle = Shape.GetAngle();
				const int32 MaxSegments = FMath::Max(4, FMath::CeilToInt(64 * Angle / 360.f));
				const float AngleIncrement = Angle / MaxSegments;
				const FVector2D Axis = FVector2D(Shape.GetAxis());
				const FVector Startup = FRotator(0, -0.5f * Angle, 0).RotateVector(Shape.GetScaledAxis());

				FVector2D LineStart = FVector2D(Startup);
				if (!Shape.IsSphere())
				{
					// Draw sector start axis
					DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(Center2D), WorldToScreen(Center2D + LineStart), Color, 2);
				}
				// Draw sector Arc
				for (int32 i = 1; i <= MaxSegments; i++)
				{
					FVector2D LineEnd = FVector2D(FRotator(0, AngleIncrement * i, 0).RotateVector(Startup));
					DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(Center2D + LineStart), WorldToScreen(Center2D + LineEnd), Color, 2);
					LineStart = LineEnd;
				}
				// If sphere, close circle, else draw sector end axis
				DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(Center2D + LineStart), WorldToScreen(Center2D + (Shape.IsSphere() ? FVector2D(Startup) : FVector2D::ZeroVector)), Color, 2);

				// Draw direction vector
				DrawContext.PushDrawSegment(GridScreenBounds, WorldToScreen(Center2D), WorldToScreen(Center2D + Axis * Shape.GetRadius()), Color, 2);
			});
		}
	};

	FBox GridsShapeBounds(ForceInit);
	FBox2D GridsBounds(ForceInit);
	int32 GridIndex = 0;
	for (auto& [Name, StreamingDataList] : FilteredStreamingObjects)
	{
		for (const FWorldPartitionStreamingSource& Source : Sources)
		{
			// @todo_jfd
			const FSoftObjectPath HLODLayer;
			Source.ForEachShape(StreamingDataList[0]->LoadingRange, Name, HLODLayer, true, [&GridsShapeBounds](const FSphericalSector& Shape) { GridsShapeBounds += Shape.CalcBounds(); });
		}

		const FVector2D GridReferenceWorldPos = FVector2D(WorldRegion.GetCenter());
		const FVector2D WorldRegionExtent = FVector2D(WorldRegion.GetExtent().GetMax());
		const FVector2D GridScreenOffset = GridScreenInitialOffset + ((float)GridIndex * FVector2D(GridMaxScreenWidth, 0.f)) + GridScreenHalfExtent + FVector2D(GridScreenWidthShrinkSize * 0.5f);
		const FVector2D WorldToScreenScale = GridScreenHalfExtent / WorldRegionExtent;
		const FBox2D GridScreenBounds(GridScreenOffset - GridScreenHalfExtent, GridScreenOffset + GridScreenHalfExtent);

		auto WorldToScreen = [&](const FVector2D& LocalWorldPos, bool bIsLocalWorldPos = true)
		{
			FVector2D GlocalPos = bIsLocalWorldPos ? LocalWPToGlobal.TransformPoint(LocalWorldPos) : LocalWorldPos;
			return (WorldToScreenScale * (GlocalPos - GridReferenceWorldPos)) + GridScreenOffset;
		};

		for (const FRuntimePartitionStreamingData* StreamingData : StreamingDataList)
		{
			DrawStreamingData(StreamingData, LocalRegion, GridScreenBounds, WorldToScreen, DrawContext);
		}

		GridsBounds += GridScreenBounds;

		if (DrawContext.IsDetailedMode())
		{
			FVector2D GridInfoPos = GridScreenOffset - GridScreenHalfExtent;
			FWorldPartitionCanvasMultiLineText MultiLineText;
			MultiLineText.Emplace(UWorld::RemovePIEPrefix(FPaths::GetBaseFilename(WorldPartition->GetPackage()->GetName())), FLinearColor::White);
			FString GridInfoText = FString::Printf(TEXT("%s | %d m"), *Name.ToString(), int32(StreamingDataList[0]->LoadingRange * 0.01f));
			MultiLineText.Emplace(GridInfoText, FLinearColor::Yellow);
			FWorldPartitionCanvasMultiLineTextItem Item(GridInfoPos, MultiLineText);
			DrawContext.PushDrawText(Item);
			++GridIndex;
		}
	}

	FBox2D DesiredWorldBounds(ForceInit);
	if (GridsShapeBounds.IsValid)
	{
		// Convert to 2D
		FBox2D GridsShapeBounds2D = FBox2D(FVector2D(GridsShapeBounds.Min.X, GridsShapeBounds.Min.Y), FVector2D(GridsShapeBounds.Max.X, GridsShapeBounds.Max.Y));
		FVector2D CenterGlobalPos = LocalWPToGlobal.TransformPoint(GridsShapeBounds2D.GetCenter());
		// Use max extent of X/Y
		FVector2D Extent = FVector2D(GridsShapeBounds2D.GetExtent().GetMax());
		// Transform to global space
		GridsShapeBounds2D = FBox2D(CenterGlobalPos - Extent, CenterGlobalPos + Extent);
		// Expand by 10% computed bounds
		DesiredWorldBounds = GridsShapeBounds2D.ExpandBy(GridsShapeBounds2D.GetExtent() * 0.1f);
	}
	DrawContext.SetDesiredWorldBounds(DesiredWorldBounds);
	DrawContext.SetUsedCanvasBounds(GridsBounds);

	return true;
}

void UWorldPartitionRuntimeHashSet::SetDefaultValues()
{
}

bool UWorldPartitionRuntimeHashSet::SupportsHLODs() const
{
	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		if (RuntimePartitionDesc.MainLayer)
		{
			if (RuntimePartitionDesc.MainLayer->SupportsHLODs())
			{
				return true;
			}
		}
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::GenerateStreaming(UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate)
{
	verify(Super::GenerateStreaming(StreamingPolicy, StreamingGenerationContext, OutPackagesToGenerate));

	UWorldPartition* WorldPartition = GetOuterUWorldPartition();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	check(!PersistentPartitionDesc.Class);
	PersistentPartitionDesc.Class = URuntimePartitionPersistent::StaticClass();
	PersistentPartitionDesc.Name = NAME_PersistentLevel;
	PersistentPartitionDesc.MainLayer = NewObject<URuntimePartition>(this, URuntimePartitionPersistent::StaticClass(), NAME_None);
	PersistentPartitionDesc.MainLayer->Name = NAME_PersistentLevel;

	//
	// Split actor sets into their corresponding runtime partition implementation
	//
	TMap<FName, const FRuntimePartitionDesc*> NameToRuntimePartitionDescMap;

	NameToRuntimePartitionDescMap.Add(NAME_None, &RuntimePartitions[0]);				// Actors with RuntimeGrid=None will be assigned to the default partition
	NameToRuntimePartitionDescMap.Add(NAME_PersistentLevel, &PersistentPartitionDesc);	// Non-spatially loaded actors will be assigned to the persistent partition

	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		NameToRuntimePartitionDescMap.Add(RuntimePartitionDesc.Name, &RuntimePartitionDesc);
	}

	TMap<URuntimePartition*, TArray<const IStreamingGenerationContext::FActorSetInstance*>> RuntimePartitionsToActorSetMap;
	StreamingGenerationContext->ForEachActorSetInstance([this, &NameToRuntimePartitionDescMap, &RuntimePartitionsToActorSetMap](const IStreamingGenerationContext::FActorSetInstance& ActorSetInstance)
	{
		const TArray<FName> ActorSetRuntimeGrid = ActorSetInstance.bIsSpatiallyLoaded ? ParseGridName(ActorSetInstance.RuntimeGrid) : TArray<FName>({ NAME_PersistentLevel });

		if (const FRuntimePartitionDesc** RuntimePartitionDesc = NameToRuntimePartitionDescMap.Find(ActorSetRuntimeGrid[0]))
		{
			RuntimePartitionsToActorSetMap.FindOrAdd((*RuntimePartitionDesc)->MainLayer).Add(&ActorSetInstance);
		}
	});

	struct FCellDescInstance : public URuntimePartition::FCellDesc
	{
		FCellDescInstance(const URuntimePartition::FCellDesc& InCellDesc, URuntimePartition* InSourcePartition, const TArray<const UDataLayerInstance*>& InDataLayers)
			: URuntimePartition::FCellDesc(InCellDesc)
			, SourcePartition(InSourcePartition)
			, DataLayers(InDataLayers)
		{}

		URuntimePartition* SourcePartition;
		TArray<const UDataLayerInstance*> DataLayers;
	};

	//
	// Generate runtime partitions streaming data
	//
	TArray<FCellDescInstance> RuntimeCellDescsInstances;
	{
		TSet<FName> CellDescsNames;
		for (auto [RuntimePartition, ActorSetInstances] : RuntimePartitionsToActorSetMap)
		{
			// Gather runtime partition cell descs
			TArray<URuntimePartition::FCellDesc> RuntimeCellDescs;
			if (!RuntimePartition->GenerateStreaming(ActorSetInstances, RuntimeCellDescs))
			{
				return false;
			}

			// Split cell descs into data layers
			for (const URuntimePartition::FCellDesc& RuntimeCellDesc : RuntimeCellDescs)
			{
				TMap<FDataLayersID, FCellDescInstance> RuntimeCellDescsInstancesSet;

				bool bCellNameExists;
				CellDescsNames.Add(RuntimeCellDesc.Name, &bCellNameExists);
				check(!bCellNameExists);

				for (const IStreamingGenerationContext::FActorInstance& ActorInstance : RuntimeCellDesc.ActorInstances)
				{
					const FDataLayersID DataLayersID(ActorInstance.ActorSetInstance->DataLayers);
					FCellDescInstance* CellDescInstance = RuntimeCellDescsInstancesSet.Find(DataLayersID);

					if (!CellDescInstance)
					{
						CellDescInstance = &RuntimeCellDescsInstancesSet.Emplace(DataLayersID, FCellDescInstance(RuntimeCellDesc, RuntimePartition, ActorInstance.ActorSetInstance->DataLayers));
						CellDescInstance->ActorInstances.Empty();
					}

					CellDescInstance->ActorInstances.Add(ActorInstance);
				}

				Algo::Transform(RuntimeCellDescsInstancesSet, RuntimeCellDescsInstances, [](const auto& Value) { return Value.Value; });
			}
		}
	}

	//
	// Create and populate streaming object
	//
	check(RuntimeStreamingData.IsEmpty());

	// Generate runtime cells
	auto CreateRuntimeCellFromCellDesc = [this](const URuntimePartition::FCellDesc& CellDesc, const TArray<const UDataLayerInstance*>& DataLayers, TSubclassOf<UWorldPartitionRuntimeCell> CellClass, TSubclassOf<UWorldPartitionRuntimeCellData> CellDataClass)
	{
		FString CellObjectName;
		FGuid CellGuid;
		{
			UWorld* OuterWorld = GetTypedOuter<UWorld>();
			check(OuterWorld);

			FString WorldName = FPackageName::GetShortName(OuterWorld->GetPackage());

			CellObjectName = FString::Printf(TEXT("%s_%s"), *WorldName, *CellDesc.Name.ToString());

			const FDataLayersID DataLayersID(DataLayers);
			if (DataLayersID.GetHash())
			{
				CellObjectName += FString::Printf(TEXT("_d%X"), DataLayersID.GetHash());
			}

			if (CellDesc.ContentBundleID.IsValid())
			{
				CellObjectName += FString::Printf(TEXT("_c%s"), *UContentBundleDescriptor::GetContentBundleCompactString(CellDesc.ContentBundleID));
			}

			if (!IsRunningCookCommandlet() && OuterWorld->IsGameWorld())
			{
				FString SourceWorldPath;
				FString InstancedWorldPath;
				if (OuterWorld->GetSoftObjectPathMapping(SourceWorldPath, InstancedWorldPath))
				{
					const FTopLevelAssetPath SourceAssetPath(SourceWorldPath);
					WorldName = FPackageName::GetShortName(SourceAssetPath.GetPackageName());
						
					InstancedWorldPath = UWorld::RemovePIEPrefix(InstancedWorldPath);

					const FString SourcePackageName = SourceAssetPath.GetPackageName().ToString();
					const FTopLevelAssetPath InstanceAssetPath(InstancedWorldPath);
					const FString InstancePackageName = InstanceAssetPath.GetPackageName().ToString();

					if (int32 Index = InstancePackageName.Find(SourcePackageName); Index != INDEX_NONE)
					{
						CellObjectName += FString::Printf(TEXT("_i%s"), *InstancePackageName.Mid(Index + SourcePackageName.Len()));
					}
				}
			}

			FArchiveMD5 ArMD5;
			ArMD5 << CellObjectName;
			CellGuid = ArMD5.GetGuidFromHash();
			check(CellGuid.IsValid());
		}

		UWorldPartitionRuntimeCell* RuntimeCell = Super::CreateRuntimeCell(CellClass, CellDataClass, CellObjectName, TEXT(""));

		RuntimeCell->SetIsAlwaysLoaded(!CellDesc.bIsSpatiallyLoaded);
		RuntimeCell->SetDataLayers(DataLayers);
		RuntimeCell->SetContentBundleUID(CellDesc.ContentBundleID);
		RuntimeCell->SetPriority(CellDesc.Priority);
		RuntimeCell->SetClientOnlyVisible(CellDesc.bClientOnlyVisible);
		RuntimeCell->SetBlockOnSlowLoading(CellDesc.bBlockOnSlowStreaming);
		RuntimeCell->SetIsHLOD(false);
		RuntimeCell->SetGuid(CellGuid);

		UWorldPartitionRuntimeCellDataSpatialHashSet* RuntimeCellDataHashSet = CastChecked<UWorldPartitionRuntimeCellDataSpatialHashSet>(RuntimeCell->RuntimeCellData);
		RuntimeCellDataHashSet->DebugName = CellObjectName;
		RuntimeCellDataHashSet->Level = CellDesc.Level;

		return RuntimeCell;
	};

	TArray<UWorldPartitionRuntimeCell*> RuntimeCells;
	TMap<URuntimePartition*, FRuntimePartitionStreamingData> RuntimePartitionsStreamingData;
	for (const FCellDescInstance& CellDescInstance : RuntimeCellDescsInstances)
	{
		UWorldPartitionRuntimeCell* RuntimeCell = RuntimeCells.Emplace_GetRef(CreateRuntimeCellFromCellDesc(CellDescInstance, CellDescInstance.DataLayers, StreamingPolicy->GetRuntimeCellClass(), UWorldPartitionRuntimeCellDataSpatialHashSet::StaticClass()));
		PopulateRuntimeCell(RuntimeCell, CellDescInstance.ActorInstances, nullptr);

		// Override the cell bounds if the runtime partition provided one
		if (CellDescInstance.Bounds.IsValid)
		{
			RuntimeCell->RuntimeCellData->ContentBounds = CellDescInstance.Bounds;
		}

		// Create partition streaming data
		FRuntimePartitionStreamingData& StreamingData = RuntimePartitionsStreamingData.FindOrAdd(CellDescInstance.SourcePartition);

		StreamingData.Name = CellDescInstance.SourcePartition->Name;
		StreamingData.LoadingRange = CellDescInstance.SourcePartition->LoadingRange;

		if (RuntimeCell->IsAlwaysLoaded())
		{
			StreamingData.NonStreamingCells.Add(RuntimeCell);
		}
		else
		{
			StreamingData.StreamingCells.Add(RuntimeCell);
		}
	}

	//
	// Finalize streaming object
	//
	for (auto& [Partition, StreamingData] : RuntimePartitionsStreamingData)
	{
		RuntimeStreamingData.Emplace(MoveTemp(StreamingData));
	}

	//
	// Output generated packages
	//
	if (OutPackagesToGenerate)
	{
		for (UWorldPartitionRuntimeCell* RuntimeCell : RuntimeCells)
		{
			// Always loaded cell actors are transfered to World's Persistent Level (see UWorldPartitionRuntimeSpatialHash::PopulateGeneratorPackageForCook)
			if (RuntimeCell->GetActorCount() && !RuntimeCell->IsAlwaysLoaded())
			{
				const FString PackageRelativePath = RuntimeCell->GetPackageNameToCreate();
				check(!PackageRelativePath.IsEmpty());

				OutPackagesToGenerate->Add(PackageRelativePath);

				// Map relative package to StreamingCell for PopulateGeneratedPackageForCook/PopulateGeneratorPackageForCook/GetCellForPackage
				PackagesToGenerateForCook.Add(PackageRelativePath, RuntimeCell);
			}
		}
	}

	return true;
}

void UWorldPartitionRuntimeHashSet::FlushStreaming()
{
	Super::FlushStreaming();
	
	check(PersistentPartitionDesc.Class);
	PersistentPartitionDesc.Class = nullptr;
	PersistentPartitionDesc.Name = NAME_None;
	PersistentPartitionDesc.MainLayer = nullptr;

	RuntimeStreamingData.Empty();
}

bool UWorldPartitionRuntimeHashSet::IsValidGrid(FName GridName) const
{
	// The None grid name will always map to the first runtime partition in the list
	if (GridName.IsNone())
	{
		return true;
	}

	// Parse the potentially dot separated grid name to identiy the associated runtime partition
	const TArray<FName> GridNameList = ParseGridName(GridName);
	for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		if (RuntimePartitionDesc.Name == GridNameList[0])
		{
			if (RuntimePartitionDesc.MainLayer)
			{
				return RuntimePartitionDesc.MainLayer->IsValidGrid(GridName);
			}
		}
	}

	return false;
}

TArray<UWorldPartitionRuntimeCell*> UWorldPartitionRuntimeHashSet::GetAlwaysLoadedCells() const
{
	TArray<UWorldPartitionRuntimeCell*> AlwaysLoadedCells;
	ForEachStreamingData([&AlwaysLoadedCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		AlwaysLoadedCells.Append(StreamingData.NonStreamingCells);
	});
	return AlwaysLoadedCells;
}

void UWorldPartitionRuntimeHashSet::DumpStateLog(FHierarchicalLogArchive& Ar) const
{
	Super::DumpStateLog(Ar);

	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));
	Ar.Printf(TEXT("%s - Runtime Hash Set"), *GetWorld()->GetName());
	Ar.Printf(TEXT("----------------------------------------------------------------------------------------------------------------"));

	TArray<const UWorldPartitionRuntimeCell*> StreamingCells;
	ForEachStreamingCells([&StreamingCells](const UWorldPartitionRuntimeCell* StreamingCell) { StreamingCells.Add(StreamingCell); return true; });
				
	StreamingCells.Sort([this](const UWorldPartitionRuntimeCell& A, const UWorldPartitionRuntimeCell& B) { return A.GetFName().LexicalLess(B.GetFName()); });

	for (const UWorldPartitionRuntimeCell* StreamingCell : StreamingCells)
	{
		FHierarchicalLogArchive::FIndentScope CellIndentScope = Ar.PrintfIndent(TEXT("Content of Cell %s (%s)"), *StreamingCell->GetDebugName(), *StreamingCell->GetName());
		StreamingCell->DumpStateLog(Ar);
	}

	Ar.Printf(TEXT(""));
}

TArray<FName> UWorldPartitionRuntimeHashSet::ParseGridName(FName GridName)
{
	TArray<FString> GridNameList;
	const FString GridNameStr = GridName.ToString();
	if (GridNameStr.ParseIntoArray(GridNameList, TEXT(".")))
	{
		TArray<FName> Result;
		Algo::Transform(GridNameList, Result, [](const FString& GridName) { return *GridName; });
		return MoveTemp(Result);
	}
	return { GridName };
}

URuntimeHashExternalStreamingObjectBase* UWorldPartitionRuntimeHashSet::StoreToExternalStreamingObject(UObject* StreamingObjectOuter, FName StreamingObjectName)
{
	check(!RuntimeStreamingData.IsEmpty());

	URuntimeHashSetExternalStreamingObject* NewStreamingObject = CreateExternalStreamingObject<URuntimeHashSetExternalStreamingObject>(this, MakeUniqueObjectName(this, URuntimeHashExternalStreamingObjectBase::StaticClass()));
	NewStreamingObject->RuntimeStreamingData = MoveTemp(RuntimeStreamingData);

	for (FRuntimePartitionStreamingData& StreamingData : NewStreamingObject->RuntimeStreamingData)
	{
		for (UWorldPartitionRuntimeCell* Cell : StreamingData.StreamingCells)
		{
			Cell->Rename(nullptr, NewStreamingObject,  REN_DoNotDirty | REN_ForceNoResetLoaders);
		}

		for (UWorldPartitionRuntimeCell* Cell : StreamingData.NonStreamingCells)
		{
			Cell->Rename(nullptr, NewStreamingObject,  REN_DoNotDirty | REN_ForceNoResetLoaders);
		}
	}

	return NewStreamingObject;
}
#endif

bool UWorldPartitionRuntimeHashSet::InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	if (Super::InjectExternalStreamingObject(ExternalStreamingObject))
	{
		URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);
		HashSetExternalStreamingObject->CreatePartitionsSpatialIndex();
		return true;
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject)
{
	if (Super::RemoveExternalStreamingObject(ExternalStreamingObject))
	{
		URuntimeHashSetExternalStreamingObject* HashSetExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(ExternalStreamingObject);
		HashSetExternalStreamingObject->DestroyPartitionsSpatialIndex();
		return true;
	}

	return false;
}

// Streaming interface
void UWorldPartitionRuntimeHashSet::ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const
{
	auto ForEachCells = [this, &Func](const TArray<TObjectPtr<UWorldPartitionRuntimeCell>>& InCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingData([&ForEachCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		ForEachCells(StreamingData.StreamingCells);
		ForEachCells(StreamingData.NonStreamingCells);
	});
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsQuery(const FWorldPartitionStreamingQuerySource& QuerySource, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func, FWorldPartitionQueryCache* QueryCache) const
{
	auto ShouldAddCell = [this](const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingQuerySource& QuerySource)
	{
		if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
		{
			if (Cell->HasDataLayers())
			{
				if (Cell->GetDataLayers().FindByPredicate([&](const FName& DataLayerName) { return QuerySource.DataLayers.Contains(DataLayerName); }))
				{
					return true;
				}
			}
			else if (!QuerySource.bDataLayersOnly)
			{
				return true;
			}
		}

		return false;
	};

	auto ForEachStreamingCells = [&ShouldAddCell, &QuerySource, &Func](FStaticSpatialIndexType* InSpatialIndex)
	{
		if (InSpatialIndex)
		{
			InSpatialIndex->ForEachElement([&ShouldAddCell, &QuerySource, &Func](UWorldPartitionRuntimeCell* RuntimeCell)
			{
				if (ShouldAddCell(RuntimeCell, QuerySource))
				{
					Func(RuntimeCell);
				}
			});
		}
	};

	auto ForEachNonStreamingCells = [&ShouldAddCell, &QuerySource, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonStreamingCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonStreamingCells)
		{
			if (ShouldAddCell(Cell, QuerySource))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingData([&ForEachStreamingCells, &ForEachNonStreamingCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		ForEachStreamingCells(StreamingData.SpatialIndex.Get());
		ForEachNonStreamingCells(StreamingData.NonStreamingCells);
	});
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const
{
	UWorldPartitionRuntimeHash::FStreamingSourceCells ActivateStreamingSourceCells;
	UWorldPartitionRuntimeHash::FStreamingSourceCells LoadStreamingSourceCells;

	auto ForEachStreamingCells = [this, &Sources, &Func, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](FStaticSpatialIndexType* InSpatialIndex, int32 InLoadingRange, FName InGridName)
	{
		if (InSpatialIndex)
		{
			for (const FWorldPartitionStreamingSource& Source : Sources)
			{
				// @todo_jfd
				const FSoftObjectPath HLODLayer;
				Source.ForEachShape(InLoadingRange, InGridName, HLODLayer, false, [this, &Source, InSpatialIndex, &Func, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](const FSphericalSector& Shape)
				{
					const FSphere ShapeSphere(Shape.GetCenter(), Shape.GetRadius());

					InSpatialIndex->ForEachIntersectingElement(ShapeSphere, [this, &Source, &Shape, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](UWorldPartitionRuntimeCell* Cell)
					{
						if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
						{
							if (!Cell->HasDataLayers() || Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Activated))
							{
								if (Source.TargetState == EStreamingSourceTargetState::Loaded)
								{
									LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
								}
								else
								{
									ActivateStreamingSourceCells.AddCell(Cell, Source, Shape);
								}
							}
							else if (Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Loaded))
							{
								LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
							}
						}
					});
				});
			}
		}
	};

	auto ForEachNonStreamingCells = [this, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonStreamingCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonStreamingCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				if (!Cell->HasDataLayers() || Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Activated))
				{
					ActivateStreamingSourceCells.GetCells().Add(Cell);
				}
				else if (Cell->HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState::Loaded))
				{
					LoadStreamingSourceCells.GetCells().Add(Cell);
				}
			}
		}
	};

	ForEachStreamingData([&ForEachStreamingCells, &ForEachNonStreamingCells](const FRuntimePartitionStreamingData& StreamingData)
	{
		ForEachStreamingCells(StreamingData.SpatialIndex.Get(), StreamingData.LoadingRange, StreamingData.Name);
		ForEachNonStreamingCells(StreamingData.NonStreamingCells);
	});

	auto ExecuteFuncOnCells = [Func](const TSet<const UWorldPartitionRuntimeCell*>& Cells, EStreamingSourceTargetState TargetState)
	{
		for (const UWorldPartitionRuntimeCell* Cell : Cells)
		{
			Func(Cell, TargetState);
		}
	};

	ExecuteFuncOnCells(ActivateStreamingSourceCells.GetCells(), EStreamingSourceTargetState::Activated);
	ExecuteFuncOnCells(LoadStreamingSourceCells.GetCells(), EStreamingSourceTargetState::Loaded);
}

#if WITH_EDITOR
void UWorldPartitionRuntimeHashSet::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	static FName NAME_RuntimePartitions(TEXT("RuntimePartitions"));
	static FName NAME_HLODSetups_Key(TEXT("HLODSetups_Key"));
	static FName NAME_HLODLayer(TEXT("HLODLayer"));

	FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Class))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];
		
		RuntimePartitionDesc.MainLayer = nullptr;

		if (RuntimePartitionDesc.Class)
		{
			RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
			RuntimePartitionDesc.MainLayer = NewObject<URuntimePartition>(this, RuntimePartitionDesc.Class, NAME_None);
			RuntimePartitionDesc.MainLayer->SetDefaultValues();

			RuntimePartitionDesc.UpdateHLODPartitionLayers();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FRuntimePartitionDesc, Name))
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		FRuntimePartitionDesc& RuntimePartitionDesc = RuntimePartitions[RuntimePartitionIndex];

		if (RuntimePartitionDesc.Name == NAME_PersistentLevel)
		{
			RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
		}
		else
		{
			for (int32 CurRuntimePartitionIndex = 0; CurRuntimePartitionIndex < RuntimePartitions.Num(); CurRuntimePartitionIndex++)
			{
				if (CurRuntimePartitionIndex != RuntimePartitionIndex)
				{
					if (RuntimePartitionDesc.Name == RuntimePartitions[CurRuntimePartitionIndex].Name)
					{
						RuntimePartitionDesc.Name = RuntimePartitionDesc.Class->GetFName();
						break;
					}
				}
			}
		}

		RuntimePartitionDesc.MainLayer->Name = RuntimePartitionDesc.Name;
	}
	else if (PropertyName == NAME_HLODLayer)
	{
		int32 RuntimePartitionIndex = PropertyChangedEvent.GetArrayIndex(NAME_RuntimePartitions.ToString());
		check(RuntimePartitions.IsValidIndex(RuntimePartitionIndex));

		RuntimePartitions[RuntimePartitionIndex].UpdateHLODPartitionLayers();
	}
}

void UWorldPartitionRuntimeHashSet::UpdateHLODPartitionLayers()
{
	for (FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
	{
		RuntimePartitionDesc.UpdateHLODPartitionLayers();
	}
}
#endif

void UWorldPartitionRuntimeHashSet::ForEachStreamingData(TFunctionRef<void(const FRuntimePartitionStreamingData&)> Func) const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		Func(StreamingData);
	}

	for (const TWeakObjectPtr<URuntimeHashExternalStreamingObjectBase>& InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		if (InjectedExternalStreamingObject.IsValid())
		{
			URuntimeHashSetExternalStreamingObject* ExternalStreamingObject = CastChecked<URuntimeHashSetExternalStreamingObject>(InjectedExternalStreamingObject.Get());
			
			for (const FRuntimePartitionStreamingData& StreamingData : ExternalStreamingObject->RuntimeStreamingData)
			{
				Func(StreamingData);
			}
		}
	}
}
