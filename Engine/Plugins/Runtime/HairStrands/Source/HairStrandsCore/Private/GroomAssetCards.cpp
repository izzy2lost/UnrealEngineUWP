// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomAssetCards.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "GroomAsset.h" // for EHairAtlasTextureType

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomAssetCards)

FHairGroupsCardsSourceDescription::FHairGroupsCardsSourceDescription()
{
	MaterialSlotName = NAME_None;
	SourceType_DEPRECATED = EHairCardsSourceType::Imported;
	GroupIndex = 0;
	LODIndex = -1;
}

void FHairGroupCardsTextures::SetTexture(EHairAtlasTextureType SlotID, UTexture2D* Texture)
{
	switch (SlotID)
	{
	case EHairAtlasTextureType::Depth:
		DepthTexture = Texture;
		break;

	case EHairAtlasTextureType::Coverage:
		CoverageTexture = Texture;
		break;

	case EHairAtlasTextureType::Tangent:
		TangentTexture = Texture;
		break;

	case EHairAtlasTextureType::Attribute:
		AttributeTexture = Texture;
		break;

	case EHairAtlasTextureType::AuxilaryData:
		AuxilaryDataTexture = Texture;
		break;
	};
}

bool FHairGroupsCardsSourceDescription::operator==(const FHairGroupsCardsSourceDescription& A) const
{
	return
		MaterialSlotName == A.MaterialSlotName &&
		GroupIndex == A.GroupIndex &&
		LODIndex == A.LODIndex &&
		ImportedMesh == A.ImportedMesh;
}

FString FHairGroupsCardsSourceDescription::GetMeshKey() const
{
#if WITH_EDITORONLY_DATA
	if (UStaticMesh* Mesh = GetMesh())
	{
		Mesh->ConditionalPostLoad();
		FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
		if (SourceModel.GetMeshDescriptionBulkData())
		{
			return SourceModel.GetMeshDescriptionBulkData()->GetIdString();
		}
	}
#endif
	return TEXT("INVALID_MESH");
}

bool FHairGroupsCardsSourceDescription::HasMeshChanged() const
{
#if WITH_EDITORONLY_DATA
	if (ImportedMesh)
	{
		return ImportedMeshKey == GetMeshKey();
	}
#endif
	return false;
}

void FHairGroupsCardsSourceDescription::UpdateMeshKey()
{
#if WITH_EDITORONLY_DATA
	if (ImportedMesh)
	{
		ImportedMeshKey = GetMeshKey();
	}
#endif
}

UStaticMesh* FHairGroupsCardsSourceDescription::GetMesh() const
{
#if WITH_EDITORONLY_DATA
	return ImportedMesh;
#else
	return nullptr;
#endif
}


