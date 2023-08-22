// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/WorldPartitionStreamingPolicy.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "Misc/ArchiveMD5.h"

#if WITH_EDITOR
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
			URuntimePartition::FGenerateStreamingParams GenerateStreamingParams;
			GenerateStreamingParams.ActorSetInstances = &ActorSetInstances;

			URuntimePartition::FGenerateStreamingResult GenerateStreamingResult;

			if (!RuntimePartition->GenerateStreaming(GenerateStreamingParams, GenerateStreamingResult))
			{
				return false;
			}

			// Split cell descs into data layers
			for (const URuntimePartition::FCellDesc& RuntimeCellDesc : GenerateStreamingResult.RuntimeCellDescs)
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
#endif