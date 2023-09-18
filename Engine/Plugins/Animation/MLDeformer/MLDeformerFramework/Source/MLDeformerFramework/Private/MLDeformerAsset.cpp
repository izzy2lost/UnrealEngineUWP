// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerAsset.h"
#include "MLDeformerObjectVersion.h"
#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "Engine/SkeletalMesh.h"

void UMLDeformerAsset::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerAsset::Serialize)

	Archive.UsingCustomVersion(UE::MLDeformer::FMLDeformerObjectVersion::GUID);
	Super::Serialize(Archive);
}

void UMLDeformerAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	if (Model)
	{
		Model->GetAssetRegistryTags(OutTags);
	}
}
