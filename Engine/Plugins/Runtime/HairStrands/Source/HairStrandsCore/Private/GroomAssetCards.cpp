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
	GuideType = EHairCardsGuideType::Generated;
	GroupIndex = 0;
	LODIndex = -1;
}

void FHairGroupCardsTextures::SetTexture(EHairAtlasTextureType SlotID, UTexture2D* Texture)
{
	Layout = EHairTextureLayout::Layout0; // Default hair card layout for now
	Textures.SetNum(6);
	switch (SlotID)
	{
	case EHairAtlasTextureType::Depth: 			Textures[0] = Texture; break;
	case EHairAtlasTextureType::Coverage:		Textures[1] = Texture; break;
	case EHairAtlasTextureType::Tangent:		Textures[2] = Texture; break;
	case EHairAtlasTextureType::Attribute:		Textures[3] = Texture; break;
	case EHairAtlasTextureType::AuxilaryData:	Textures[4] = Texture; break;
	case EHairAtlasTextureType::Material:		Textures[5] = Texture; break;
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

uint32 GetHairTextureLayoutTextureCount(EHairTextureLayout In)
{
	switch (In)
	{
		case EHairTextureLayout::Layout0: return 6u;
		case EHairTextureLayout::Layout1: return 6u;
		case EHairTextureLayout::Layout2: return 4u;
		case EHairTextureLayout::Layout3: return 4u;
	}
	return 0;
}
