// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGLevelToAsset.h"

#include "PCGEditorModule.h"

#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

void UPCGLevelToAsset::CreateOrUpdatePCGAssets(const TArray<FAssetData>& WorldAssets, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	TArray<UPackage*> PackagesToSave;

	for (const FAssetData& WorldAsset : WorldAssets)
	{
		if (UPackage* Package = CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld>(WorldAsset.GetSoftObjectPath()), ExporterSubclass))
		{
			PackagesToSave.Add(Package);
			// TODO: consider if we should garbage collect?
		}
	}

	// Save the file(s)
	// TODO: check if we should just dirty and not save (could be an option here)
	if (!PackagesToSave.IsEmpty())
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
	}
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(TSoftObjectPtr<UWorld> WorldPath, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	return CreateOrUpdatePCGAsset(WorldPath.LoadSynchronous(), ExporterSubclass);
}

UPackage* UPCGLevelToAsset::CreateOrUpdatePCGAsset(UWorld* Level, TSubclassOf<UPCGLevelToAsset> ExporterSubclass)
{
	if (!Level)
	{
		return nullptr;
	}

	UPCGLevelToAsset* Exporter = nullptr;
	if (ExporterSubclass)
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage(), ExporterSubclass);
	}
	else
	{
		Exporter = NewObject<UPCGLevelToAsset>(GetTransientPackage());
	}

	if (!Exporter)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to create Level to Settings exporter."));
		return nullptr;
	}

	const FString AssetName = Level->GetName() + TEXT("_PCG");
	// TODO: since we store the level SOP in the PCG data assets, we can technically search if there is already an asset matching this level and select this one instead.
	const FString PackageName = FPaths::Combine(FPackageName::GetLongPackagePath(Level->GetPackage()->GetName()), AssetName);

	// Implementation note: this will cause a warning if the file can't be loaded (if the file doesn't exist, for example). It doesn't seem possible to quiet this.
	UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);

	UPCGDataAsset* Asset = nullptr;
	bool NewAssetCreated = false;

	if (Package)
	{
		UObject* Object = FindObjectFast<UObject>(Package, *AssetName);
		if (Object && Object->GetClass() != Exporter->GetAssetType())
		{
			Object->SetFlags(RF_Transient);
			Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			NewAssetCreated = true;
		}
		else
		{
			Asset = Cast<UPCGDataAsset>(Object);
		}
	}
	else
	{
		Package = CreatePackage(*PackageName);
		NewAssetCreated = true;
	}

	if (!Asset)
	{
		const EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transactional;
		Asset = NewObject<UPCGDataAsset>(Package, Exporter->GetAssetType(), FName(*AssetName), Flags);
	}

	if (Asset)
	{
		if (NewAssetCreated)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Asset);
		}

		Exporter->ExportLevel(Level, PackageName, Asset);

		// Mark the package dirty...
		Package->MarkPackageDirty();

		// Make sure everybody knows we changed these settings.
		if (!NewAssetCreated)
		{
			FCoreUObjectDelegates::BroadcastOnObjectModified(Asset);
		}
	}

	return Package;
}

TSubclassOf<UPCGDataAsset> UPCGLevelToAsset::BP_GetAssetType_Implementation() const
{
	return UPCGDataAsset::StaticClass();
}

TSubclassOf<UPCGDataAsset> UPCGLevelToAsset::GetAssetType() const
{
	return BP_GetAssetType();
}

bool UPCGLevelToAsset::ExportLevel(UWorld* Level, const FString& PackageName, UPCGDataAsset* Asset)
{
	return BP_ExportLevel(Level, PackageName, Asset);
}

bool UPCGLevelToAsset::BP_ExportLevel_Implementation(UWorld* Level, const FString& PackageName, UPCGDataAsset* Asset)
{
	check(Level && Asset);
	Asset->LevelPath = FSoftObjectPath(Level);
	Asset->Description = FText::Format(NSLOCTEXT("PCGLevelToAsset", "DefaultDescriptionOnExportedLevel", "Generated from level: {0}"), FText::FromString(Level->GetName()));

	FPCGDataCollection& DataCollection = Asset->Data;

	// Create Root Data
	UPCGPointData* RootPointData = NewObject<UPCGPointData>(Asset);
	UPCGMetadata* RootMetadata = RootPointData->MutableMetadata();
	TArray<FPCGPoint>& Roots = RootPointData->GetMutablePoints();
	RootMetadata->CreateAttribute<FString>(TEXT("Name"), Level->GetName(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	RootMetadata->CreateAttribute<FSoftObjectPath>(TEXT("Source"), PackageName, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	// Add to data collection
	{
		FPCGTaggedData& RootsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		RootsTaggedData.Data = RootPointData;
		RootsTaggedData.Pin = TEXT("Root");
	}

	// Create PCGData
	UPCGPointData* PointData = NewObject<UPCGPointData>(Asset);
	UPCGMetadata* PointMetadata = PointData->MutableMetadata();
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	// Add to data collection
	{
		FPCGTaggedData& PointsTaggedData = DataCollection.TaggedData.Emplace_GetRef();
		PointsTaggedData.Data = PointData;
		PointsTaggedData.Pin = TEXT("Points");
	}

	// Common data shared across steps
	FBox AllActorBounds(EForceInit::ForceInit);

	// Attribute setup on the points
	FPCGMetadataAttribute<FSoftObjectPath>* MaterialAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(TEXT("Material"), FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = PointMetadata->CreateAttribute<FSoftObjectPath>(TEXT("Mesh"), FSoftObjectPath(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* HierarchyDepthAttribute = PointMetadata->CreateAttribute<int64>(TEXT("HierarchyDepth"), 0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ActorIndexAttribute = PointMetadata->CreateAttribute<int64>(TEXT("ActorIndex"), -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<int64>* ParentIndexAttribute = PointMetadata->CreateAttribute<int64>(TEXT("ParentIndex"), -1, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);
	FPCGMetadataAttribute<FTransform>* RelativeTransformAttribute = PointMetadata->CreateAttribute<FTransform>(TEXT("RelativeTransform"), FTransform::Identity, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

	TMap<FName, FPCGMetadataAttribute<int64>*> TagToAttributeMap;

	// Hierarchy root point
	{
		FPCGPoint& RootPoint = Points.Emplace_GetRef(FTransform::Identity, /*Density=*/1.0f, /*Seed=*/0);
		RootPoint.Steepness = 1.0f;
		RootPoint.BoundsMin = RootPoint.BoundsMax = FVector::Zero();

		PointMetadata->InitializeOnSet(RootPoint.MetadataEntry);
		ActorIndexAttribute->SetValue(RootPoint.MetadataEntry, 0);
	}

	// Build actor-index map
	TMap<AActor*, int> ActorIndexMap;
	int LastActorIndex = 1; // Since the root is the "first" point we'll have, we'll have the map start from 1.
	UPCGActorHelpers::ForEachActorInWorld(Level, AActor::StaticClass(), [&ActorIndexMap, &LastActorIndex](AActor* Actor)
	{
		ActorIndexMap.Add(Actor, LastActorIndex++);
		return true;
	});

	// Create points
	UPCGActorHelpers::ForEachActorInWorld(Level, AActor::StaticClass(), [&](AActor* Actor)
	{
		// TODO Actor-level decisions if any; if the actor is "consumed" at this step, make sure to update AllActorBounds as well.

		// Otherwise, parse "known" actor components
		TArray<UStaticMeshComponent*> SMCs;
		Actor->GetComponents(SMCs);

		if (SMCs.IsEmpty()) // early out
		{
			return true;
		}

		const FBox ActorBounds = PCGHelpers::GetActorBounds(Actor, /*bIgnorePCGCreatedComponents=*/true);
		AllActorBounds += ActorBounds;

		for (FName ActorTag : Actor->Tags)
		{
			if (!TagToAttributeMap.Contains(ActorTag))
			{
				TagToAttributeMap.Add(ActorTag, PointMetadata->CreateAttribute<int64>(ActorTag, 0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true));
			}
		}

		// Prepare actor-level data that's propagated to all points
		const FTransform& ActorTransform = Actor->GetTransform();
		const int64 ActorIndex = ActorIndexMap[Actor];
		AActor* ParentActor = Actor->GetAttachParentActor();
		const int64 ParentActorIndex = ParentActor ? ActorIndexMap[ParentActor] : 0;
		const FTransform RelativeTransform = ParentActor ? ActorTransform.GetRelativeTransform(ParentActor->GetTransform()) : ActorTransform;

		// Hierarchy depth, starts at 1 if the actor doesn't have a parent
		int HierarchyDepth = 1;
		while (ParentActor)
		{
			++HierarchyDepth;
			ParentActor = ParentActor->GetAttachParentActor();
		}

		auto MakePoint = [&](const FTransform& Transform, const FSoftObjectPath& MeshPath, const FBox& MeshBounds, const TArray<UMaterialInterface*>& MeshMaterials)
		{
			FPCGPoint& Point = Points.Emplace_GetRef(Transform, /*Density=*/1.0f, PCGHelpers::ComputeSeedFromPosition(Transform.GetLocation()));
			Point.BoundsMin = MeshBounds.Min;
			Point.BoundsMax = MeshBounds.Max;
			Point.Steepness = 1.0f;

			PointMetadata->InitializeOnSet(Point.MetadataEntry);
			MeshAttribute->SetValue(Point.MetadataEntry, MeshPath);

			if (!MeshMaterials.IsEmpty())
			{
				MaterialAttribute->SetValue(Point.MetadataEntry, FSoftObjectPath(MeshMaterials[0]));
			}

			ActorIndexAttribute->SetValue(Point.MetadataEntry, ActorIndex);
			ParentIndexAttribute->SetValue(Point.MetadataEntry, ParentActorIndex);
			RelativeTransformAttribute->SetValue(Point.MetadataEntry, RelativeTransform);
			HierarchyDepthAttribute->SetValue(Point.MetadataEntry, HierarchyDepth);

			// For all tags, set attribute value to 1.
			for (FName ActorTag : Actor->Tags)
			{
				TagToAttributeMap[ActorTag]->SetValue(Point.MetadataEntry, 1);
			}
		};

		for (UStaticMeshComponent* SMC : SMCs)
		{
			TObjectPtr<UStaticMesh> StaticMesh = SMC->GetStaticMesh();

			if (!StaticMesh)
			{
				continue;
			}

			const FSoftObjectPath MeshPath(StaticMesh);
			const FBox MeshBounds = StaticMesh->GetBoundingBox();
			TArray<UMaterialInterface*> Materials = SMC->GetMaterials();

			// For all instances (or a single instance if this is a static mesh and not an ISM)
			// if a static mesh -> use actor transform (which might be wrong?)
			// if ISM -> get instance transform in world space
			if (UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC))
			{
				const int InstanceCount = ISMC->GetNumInstances();
				for (int I = 0; I < InstanceCount; ++I)
				{
					FTransform InstanceTransform;
					ISMC->GetInstanceTransform(I, InstanceTransform, /*bWorldSpace=*/true);

					MakePoint(InstanceTransform, MeshPath, MeshBounds, Materials);
				}
			}
			else
			{
				MakePoint(ActorTransform, MeshPath, MeshBounds, Materials);
			}
		}

		return true;
	});

	// Finally, create root point in the root data
	{
		FPCGPoint& RootPoint = Roots.Emplace_GetRef(FTransform::Identity, 1.0f, 0);
		RootPoint.BoundsMin = AllActorBounds.Min;
		RootPoint.BoundsMax = AllActorBounds.Max;
	}

	return true;
}