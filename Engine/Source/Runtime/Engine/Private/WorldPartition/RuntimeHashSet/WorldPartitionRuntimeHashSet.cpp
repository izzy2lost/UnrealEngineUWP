// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/HLOD/HLODLayer.h"

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
	if (!Class || !MainLayer)
	{
		HLODSetups.Empty();
		return;
	}

	for (FRuntimePartitionHLODSetup& HLODSetup : HLODSetups)
	{
		TSet<const UHLODLayer*> VisitedHLODLayers;

		const UHLODLayer* CurHLODLayer = HLODSetup.HLODLayer;
		while (CurHLODLayer)
		{
			const int32 HLODSetupIndex = VisitedHLODLayers.Num();

			bool bHLODLayerWasAlreadyInSet;
			VisitedHLODLayers.Add(CurHLODLayer, &bHLODLayerWasAlreadyInSet);
			if (bHLODLayerWasAlreadyInSet)
			{
				// Circular reference
				break;
			}

			if (!HLODSetup.PartitionLayers.IsValidIndex(HLODSetupIndex))
			{
				HLODSetup.PartitionLayers.AddDefaulted();
			}

			FRuntimePartitionHLODSetupLayer& HLODSetupLayer = HLODSetup.PartitionLayers[HLODSetupIndex];

			const bool bHLODLayerMatches = HLODSetup.HLODLayer == CurHLODLayer;
			const UClass* ExpectedHLODPartitionClass = CurHLODLayer->IsSpatiallyLoaded() ? MainLayer->GetClass() : URuntimePartitionPersistent::StaticClass();
			const bool bHasValidPartitionLayer = HLODSetupLayer.PartitionLayer && (HLODSetupLayer.PartitionLayer->GetClass() == ExpectedHLODPartitionClass);
			
			if (!bHLODLayerMatches || !bHasValidPartitionLayer)
			{
				HLODSetupLayer.HLODLayer = CurHLODLayer;
				HLODSetupLayer.PartitionLayer = CurHLODLayer->IsSpatiallyLoaded() ? DuplicateObject<URuntimePartition>(MainLayer, MainLayer->GetOuter()) : NewObject<URuntimePartition>(MainLayer->GetOuter(), ExpectedHLODPartitionClass);
				HLODSetupLayer.PartitionLayer->Name = CurHLODLayer->GetFName();
				HLODSetupLayer.PartitionLayer->bIsHLODSetup = true;
			}

			CurHLODLayer = CurHLODLayer->GetParentLayer();	
		}

		HLODSetup.PartitionLayers.SetNum(VisitedHLODLayers.Num());
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
void UWorldPartitionRuntimeHashSet::SetDefaultValues()
{}

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
	if (!RuntimePartitions.Num())
	{
		return false;
	}

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
			return RuntimePartitionDesc.MainLayer && RuntimePartitionDesc.MainLayer->IsValidGrid(GridName);
		}
	}

	return false;
}

bool UWorldPartitionRuntimeHashSet::IsValidHLODLayer(FName GridName, const FSoftObjectPath& HLODLayerPath) const
{
	if (!RuntimePartitions.Num())
	{
		return false;
	}

	if (const UHLODLayer* HLODLayer = Cast<UHLODLayer>(HLODLayerPath.ResolveObject()))
	{
		// The None grid name will always map to the first runtime partition in the list
		int32 RuntimePartitionIndex = GridName.IsNone() ? 0 : INDEX_NONE;
		
		if (RuntimePartitionIndex == INDEX_NONE)
		{
			// Parse the potentially dot separated grid name to identiy the associated runtime partition
			const TArray<FName> GridNameList = ParseGridName(GridName);
			for (const FRuntimePartitionDesc& RuntimePartitionDesc : RuntimePartitions)
			{
				if (RuntimePartitionDesc.Name == GridNameList[0])
				{
					break;
				}
				RuntimePartitionIndex++;
			}
		}

		if (RuntimePartitionIndex == INDEX_NONE)
		{
			return false;
		}

		for (const FRuntimePartitionHLODSetup& HLODSetup : RuntimePartitions[RuntimePartitionIndex].HLODSetups)
		{
			if (HLODSetup.HLODLayer == HLODLayer)
			{
				return true;
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
							switch (Cell->GetCellEffectiveWantedState())
							{
							case EDataLayerRuntimeState::Loaded:
								LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
								break;
							case EDataLayerRuntimeState::Activated:
								switch (Source.TargetState)
								{
								case EStreamingSourceTargetState::Loaded:
									LoadStreamingSourceCells.AddCell(Cell, Source, Shape);
									break;
								case EStreamingSourceTargetState::Activated:
									ActivateStreamingSourceCells.AddCell(Cell, Source, Shape);
									break;
								default:
									checkNoEntry();
								}
								break;
							case EDataLayerRuntimeState::Unloaded:
								break;
							default:
								checkNoEntry();
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
				switch (Cell->GetCellEffectiveWantedState())
				{
				case EDataLayerRuntimeState::Loaded:
					LoadStreamingSourceCells.GetCells().Add(Cell);
					break;
				case EDataLayerRuntimeState::Activated:
					ActivateStreamingSourceCells.GetCells().Add(Cell);
					break;
				case EDataLayerRuntimeState::Unloaded:
					break;
				default:
					checkNoEntry();
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