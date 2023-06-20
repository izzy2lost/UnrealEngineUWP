// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Misc/ArchiveMD5.h"

FAutoConsoleCommand WorldPartitionRuntimeHashSetEnable(
	TEXT("wp.Editor.WorldPartitionRuntimeHashSet.Enable"),
	TEXT("Enable experimental runtime hash set class."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWorldPartitionRuntimeHashSet::StaticClass()->ClassFlags &= ~CLASS_HideDropDown;
	})
);

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

void URuntimeHashSetExternalStreamingObject::CreatePartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		if (!StreamingData.SpatialIndex)
		{
			StreamingData.SpatialIndex = MakeUnique<FStaticSpatialIndexType>();

			TArray<TPair<FBox, UWorldPartitionRuntimeCell*>> PartitionsElements;
			Algo::Transform(StreamingData.RuntimeCells, PartitionsElements, [](UWorldPartitionRuntimeCell* RuntimeCell)
			{
				return TPair<FBox, UWorldPartitionRuntimeCell*>(RuntimeCell->GetContentBounds(), RuntimeCell);
			});
			StreamingData.SpatialIndex->Init(PartitionsElements);
		}
	}
}

void URuntimeHashSetExternalStreamingObject::DestroyPartitionsSpatialIndex() const
{
	for (const FRuntimePartitionStreamingData& StreamingData : RuntimeStreamingData)
	{
		StreamingData.SpatialIndex.Reset();
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
		ForEachStreamingObject([](const URuntimeHashSetExternalStreamingObject* CurStreamingObject)
		{
			CurStreamingObject->CreatePartitionsSpatialIndex();
		});
	}
}

#if WITH_EDITOR
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
	check(!StreamingObject);
	StreamingObject = CreateExternalStreamingObject<URuntimeHashSetExternalStreamingObject>(this, MakeUniqueObjectName(this, URuntimeHashExternalStreamingObjectBase::StaticClass()));

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

		UWorldPartitionRuntimeCell* RuntimeCell = Super::CreateRuntimeCell(CellClass, CellDataClass, CellObjectName, TEXT(""), StreamingObject);

		RuntimeCell->SetIsAlwaysLoaded(!CellDesc.bIsSpatiallyLoaded);
		RuntimeCell->SetDataLayers(DataLayers);
		RuntimeCell->SetContentBundleUID(CellDesc.ContentBundleID);
		RuntimeCell->SetPriority(CellDesc.Priority);
		RuntimeCell->SetClientOnlyVisible(CellDesc.bClientOnlyVisible);
		RuntimeCell->SetBlockOnSlowLoading(CellDesc.bBlockOnSlowStreaming);
		RuntimeCell->SetIsHLOD(false);
		RuntimeCell->SetGuid(CellGuid);
		RuntimeCell->RuntimeCellData->DebugName = CellObjectName;

		return RuntimeCell;
	};

	TArray<UWorldPartitionRuntimeCell*> RuntimeCells;
	TMap<URuntimePartition*, FRuntimePartitionStreamingData> RuntimePartitionsStreamingData;
	for (const FCellDescInstance& CellDescInstance : RuntimeCellDescsInstances)
	{
		UWorldPartitionRuntimeCell* RuntimeCell = RuntimeCells.Emplace_GetRef(CreateRuntimeCellFromCellDesc(CellDescInstance, CellDescInstance.DataLayers, StreamingPolicy->GetRuntimeCellClass(), UWorldPartitionRuntimeCellData::StaticClass()));
		PopulateRuntimeCell(RuntimeCell, CellDescInstance.ActorInstances, nullptr);

		// Override the cell bounds if the runtime partition provided one
		if (CellDescInstance.Bounds.IsValid)
		{
			RuntimeCell->RuntimeCellData->ContentBounds = CellDescInstance.Bounds;
		}

		if (RuntimeCell->IsAlwaysLoaded())
		{
			StreamingObject->NonSpatiallyLoadedRuntimeCells.Add(RuntimeCell);
		}
		else
		{
			FRuntimePartitionStreamingData& StreamingData = RuntimePartitionsStreamingData.FindOrAdd(CellDescInstance.SourcePartition);

			StreamingData.Name = CellDescInstance.SourcePartition->Name;
			StreamingData.LoadingRange = CellDescInstance.SourcePartition->LoadingRange;
			StreamingData.RuntimeCells.Add(RuntimeCell);
		}
	}

	//
	// Finalize streaming object
	//
	for (auto& [Partition, StreamingData] : RuntimePartitionsStreamingData)
	{
		StreamingObject->RuntimeStreamingData.Emplace(MoveTemp(StreamingData));
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

	StreamingObject = nullptr;
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
	TArray<UWorldPartitionRuntimeCell*> NonSpatiallyLoadedRuntimeCells;
	ForEachStreamingObject([&NonSpatiallyLoadedRuntimeCells](const URuntimeHashSetExternalStreamingObject* CurStreamingObject)
	{
		NonSpatiallyLoadedRuntimeCells.Append(CurStreamingObject->NonSpatiallyLoadedRuntimeCells);
	});
	return NonSpatiallyLoadedRuntimeCells;
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
	check(StreamingObject);
	StreamingObject->Rename(*StreamingObjectName.ToString(), StreamingObjectOuter, REN_DoNotDirty | REN_ForceNoResetLoaders);
	URuntimeHashExternalStreamingObjectBase* Result = StreamingObject;
	StreamingObject = nullptr;
	return Result;
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
	auto ForEachStreamingCells = [this, &Func](FStaticSpatialIndexType* InSpatialIndex)
	{
		if (InSpatialIndex)
		{
			InSpatialIndex->ForEachElement([this, &Func](const UWorldPartitionRuntimeCell* RuntimeCell)
			{
				if (IsCellRelevantFor(RuntimeCell->GetClientOnlyVisible()))
				{
					Func(RuntimeCell);
				}
			});
		}
	};

	auto ForEachNonStreamingCells = [this, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
		{
			if (IsCellRelevantFor(Cell->GetClientOnlyVisible()))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingObject([&ForEachStreamingCells, &ForEachNonStreamingCells](const URuntimeHashSetExternalStreamingObject* CurStreamingObject)
	{
		for (const FRuntimePartitionStreamingData& StreamingData : CurStreamingObject->RuntimeStreamingData)
		{
			ForEachStreamingCells(StreamingData.SpatialIndex.Get());
		}

		ForEachNonStreamingCells(CurStreamingObject->NonSpatiallyLoadedRuntimeCells);
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

	auto ForEachNonStreamingCells = [&ShouldAddCell, &QuerySource, &Func](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
		{
			if (ShouldAddCell(Cell, QuerySource))
			{
				Func(Cell);
			}
		}
	};

	ForEachStreamingObject([&ForEachStreamingCells, &ForEachNonStreamingCells](const URuntimeHashSetExternalStreamingObject* CurStreamingObject)
	{
		for (const FRuntimePartitionStreamingData& StreamingData : CurStreamingObject->RuntimeStreamingData)
		{
			ForEachStreamingCells(StreamingData.SpatialIndex.Get());
		}

		ForEachNonStreamingCells(CurStreamingObject->NonSpatiallyLoadedRuntimeCells);
	});
}

void UWorldPartitionRuntimeHashSet::ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const
{
	UWorldPartitionRuntimeHash::FStreamingSourceCells ActivateStreamingSourceCells;
	UWorldPartitionRuntimeHash::FStreamingSourceCells LoadStreamingSourceCells;

	auto ForEachStreamingCells = [this, &Sources, &Func, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](FStaticSpatialIndexType* InSpatialIndex, float InLoadingRange, FName InGridName)
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

	auto ForEachNonStreamingCells = [this, &ActivateStreamingSourceCells, &LoadStreamingSourceCells](TArray<TObjectPtr<UWorldPartitionRuntimeCell>> InNonSpatiallyLoadedRuntimeCells)
	{
		for (UWorldPartitionRuntimeCell* Cell : InNonSpatiallyLoadedRuntimeCells)
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

	ForEachStreamingObject([&ForEachStreamingCells, &ForEachNonStreamingCells](const URuntimeHashSetExternalStreamingObject* CurStreamingObject)
	{
		for (const FRuntimePartitionStreamingData& StreamingData : CurStreamingObject->RuntimeStreamingData)
		{
			ForEachStreamingCells(StreamingData.SpatialIndex.Get(), StreamingData.LoadingRange, StreamingData.Name);
		}

		ForEachNonStreamingCells(CurStreamingObject->NonSpatiallyLoadedRuntimeCells);
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
			RuntimePartitionDesc.MainLayer->Name = RuntimePartitionDesc.Name;

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

void UWorldPartitionRuntimeHashSet::ForEachStreamingObject(TFunctionRef<void(const URuntimeHashSetExternalStreamingObject*)> Func) const
{
	if (StreamingObject)
	{
		Func(StreamingObject.Get());
	}

	for (const TWeakObjectPtr<URuntimeHashExternalStreamingObjectBase>& InjectedExternalStreamingObject : InjectedExternalStreamingObjects)
	{
		if (InjectedExternalStreamingObject.IsValid())
		{
			Func(CastChecked<URuntimeHashSetExternalStreamingObject>(InjectedExternalStreamingObject.Get()));
		}
	}
}