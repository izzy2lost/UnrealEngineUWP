// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshAssetFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "StaticMeshResources.h"
#include "UDynamicMesh.h"

#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "RenderingThread.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "SkeletalMeshLODRenderDataToDynamicMesh.h"

#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "AssetUtils/StaticMeshMaterialUtil.h"
#include "ConversionUtils/SceneComponentToDynamicMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshAssetFunctions)


#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshAssetFunctions"


static void ConvertGeometryScriptReadLOD(const FGeometryScriptMeshReadLOD& ReadLOD, UE::Conversion::EMeshLODType& OutLODType, int32& OutLODIndex)
{
	using namespace UE::Conversion;
	OutLODType = [](const EGeometryScriptLODType& LODType)
		{
			switch (LODType)
			{
			case EGeometryScriptLODType::MaxAvailable:
				return EMeshLODType::MaxAvailable;
			case EGeometryScriptLODType::HiResSourceModel:
				return EMeshLODType::HiResSourceModel;
			case EGeometryScriptLODType::SourceModel:
				return EMeshLODType::SourceModel;
			case EGeometryScriptLODType::RenderData:
				return EMeshLODType::RenderData;
			default:
				checkNoEntry();
				return EMeshLODType::RenderData;
			}
		}(ReadLOD.LODType);
	OutLODIndex = ReadLOD.LODIndex;
}

static void ConvertGeometryScriptWriteLOD(const FGeometryScriptMeshWriteLOD& WriteLOD, UE::Conversion::EMeshLODType& OutLODType, int32& OutLODIndex)
{
	using namespace UE::Conversion;
	OutLODType = WriteLOD.bWriteHiResSource ? EMeshLODType::HiResSourceModel : EMeshLODType::SourceModel;
	OutLODIndex = WriteLOD.LODIndex;
}


UDynamicMesh*  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMeshV2(
	UStaticMesh* FromStaticMeshAsset, 
	UDynamicMesh* ToDynamicMesh, 
	FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
	FGeometryScriptMeshReadLOD RequestedLOD,
	EGeometryScriptOutcomePins& Outcome,
	bool bUseSectionMaterials,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromAsset_InvalidInput1", "CopyMeshFromStaticMesh: FromStaticMeshAsset is Null"));
		return ToDynamicMesh;
	}
	if (ToDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromAsset_InvalidInput2", "CopyMeshFromStaticMesh: ToDynamicMesh is Null"));
		return ToDynamicMesh;
	}

	using namespace UE::Conversion;
	EMeshLODType LODType;
	int32 LODIndex;
	ConvertGeometryScriptReadLOD(RequestedLOD, LODType, LODIndex);
	FStaticMeshConversionOptions ConversionOptions;
	ConversionOptions.bApplyBuildSettings = AssetOptions.bApplyBuildSettings;
	ConversionOptions.bRequestTangents = AssetOptions.bRequestTangents;
	ConversionOptions.bIgnoreRemoveDegenerates = AssetOptions.bIgnoreRemoveDegenerates;
	ConversionOptions.bUseBuildScale = AssetOptions.bUseBuildScale;
	ConversionOptions.bUseSectionMaterialIndices = bUseSectionMaterials;

	FText ErrorMessage;

	FDynamicMesh3 NewMesh;
	bool bSuccess = StaticMeshToDynamicMesh(FromStaticMeshAsset, NewMesh, ErrorMessage, ConversionOptions, LODType, LODIndex);
	if (!bSuccess)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, ErrorMessage);
	}
	else
	{
		ToDynamicMesh->SetMesh(MoveTemp(NewMesh));
		Outcome = EGeometryScriptOutcomePins::Success;
	}
	return ToDynamicMesh;
}




UDynamicMesh*  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
	UDynamicMesh* FromDynamicMesh,
	UStaticMesh* ToStaticMeshAsset,
	FGeometryScriptCopyMeshToAssetOptions Options,
	FGeometryScriptMeshWriteLOD TargetLOD,
	EGeometryScriptOutcomePins& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_InvalidInput1", "CopyMeshToStaticMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (ToStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_InvalidInput2", "CopyMeshToStaticMesh: ToStaticMeshAsset is Null"));
		return FromDynamicMesh;
	}

#if WITH_EDITOR

	int32 UseLODIndex = FMath::Clamp(TargetLOD.LODIndex, 0, MAX_STATIC_MESH_LODS);

	// currently material updates are only applied when writing LODs
	if (Options.bReplaceMaterials && TargetLOD.bWriteHiResSource)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToStaticMesh_InvalidOptions1", "CopyMeshToStaticMesh: Can only Replace Materials when updating LODs"));
		return FromDynamicMesh;
	}

	// Don't allow built-in engine assets to be modified. However we do allow assets in /Engine/Transient/ to be updated because
	// this is a location that temporary assets in the Transient package will be created in, and in some cases we want to use
	// script functions on such asset (Datasmith does this for example)
	if ( ToStaticMeshAsset->GetPathName().StartsWith(TEXT("/Engine/")) && ToStaticMeshAsset->GetPathName().StartsWith(TEXT("/Engine/Transient")) == false )
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_EngineAsset", "CopyMeshToStaticMesh: Cannot modify built-in Engine asset"));
		return FromDynamicMesh;
	}

	// flush any pending rendering commands, which might want to touch this StaticMesh while we are rebuilding it
	FlushRenderingCommands();

	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateStaticMesh", "Update Static Mesh"));
	}

	// make sure transactional flag is on for the Asset
	ToStaticMeshAsset->SetFlags(RF_Transactional);
	// mark as modified
	ToStaticMeshAsset->Modify();

	auto ConfigureBuildSettingsFromOptions = [](FStaticMeshSourceModel& SourceModel, FGeometryScriptCopyMeshToAssetOptions& Options) -> FVector
												{
													FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;
													BuildSettings.bRecomputeNormals  = Options.bEnableRecomputeNormals;
													BuildSettings.bRecomputeTangents = Options.bEnableRecomputeTangents;
													BuildSettings.bRemoveDegenerates = Options.bEnableRemoveDegenerates;
													if (!Options.bUseBuildScale) // if we're not using build scale, set asset BuildScale to 1,1,1
													{
														BuildSettings.BuildScale3D = FVector::OneVector;
													}
													return BuildSettings.BuildScale3D;
												};
	
	auto ApplyInverseBuildScale = [](FMeshDescription& MeshDescription, FVector BuildScale)
	{
		if (BuildScale.Equals(FVector::OneVector))
		{
			return;
		}
		FTransform InverseBuildScaleTransform = FTransform::Identity;
		FVector InverseBuildScale;
		// Safely invert BuildScale
		for (int32 Idx = 0; Idx < 3; ++Idx)
		{
			InverseBuildScale[Idx] = FMath::IsNearlyZero(BuildScale[Idx], FMathd::Epsilon) ? 1.0 : 1.0 / BuildScale[Idx];
		}
		InverseBuildScaleTransform.SetScale3D(InverseBuildScale);
		FStaticMeshOperations::ApplyTransform(MeshDescription, InverseBuildScaleTransform, true /*use correct normal transforms*/);
	};

	if (TargetLOD.bWriteHiResSource)
	{
		// update model build settings
		FVector BuildScale = ConfigureBuildSettingsFromOptions(ToStaticMeshAsset->GetHiResSourceModel(), Options);

		ToStaticMeshAsset->ModifyHiResMeshDescription();
		FMeshDescription* NewHiResMD = ToStaticMeshAsset->CreateHiResMeshDescription();

		if (!ensure(NewHiResMD != nullptr))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_NullHiResMeshDescription", "CopyMeshToAsset: MeshDescription for HiRes is null?"));
			return FromDynamicMesh;
		}

		FConversionToMeshDescriptionOptions ConversionOptions;
		FDynamicMeshToMeshDescription Converter(ConversionOptions);
		FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				Converter.Convert(&ReadMesh, *NewHiResMD, !Options.bEnableRecomputeTangents);
			});

		ApplyInverseBuildScale(*NewHiResMD, BuildScale);

		ToStaticMeshAsset->CommitHiResMeshDescription();
	}
	else
	{ 

		if (ToStaticMeshAsset->GetNumSourceModels() < UseLODIndex+1)
		{
			ToStaticMeshAsset->SetNumSourceModels(UseLODIndex+1);
		}

		// update model build settings
		FVector BuildScale = ConfigureBuildSettingsFromOptions(ToStaticMeshAsset->GetSourceModel(UseLODIndex), Options);

		FMeshDescription* MeshDescription = ToStaticMeshAsset->GetMeshDescription(UseLODIndex);
		if (MeshDescription == nullptr)
		{
			MeshDescription = ToStaticMeshAsset->CreateMeshDescription(UseLODIndex);
		}

		// mark mesh description for modify
		ToStaticMeshAsset->ModifyMeshDescription(UseLODIndex);

		if (!ensure(MeshDescription != nullptr))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs,
				FText::Format(LOCTEXT("CopyMeshToAsset_NullMeshDescription", "CopyMeshToAsset: MeshDescription for LOD {0} is null?"), FText::AsNumber(UseLODIndex)));
			return FromDynamicMesh;
		}

		FConversionToMeshDescriptionOptions ConversionOptions;
		FDynamicMeshToMeshDescription Converter(ConversionOptions);
		if (!Options.bReplaceMaterials)
		{
			UE::Conversion::EMeshLODType LODType;
			int32 LODIndex;
			ConvertGeometryScriptWriteLOD(TargetLOD, LODType, LODIndex);
			TArray<int32> MaterialIDMap = UE::Conversion::GetPolygonGroupToMaterialIndexMap(ToStaticMeshAsset, LODType, LODIndex);
			Converter.SetMaterialIDMapFromInverseMap(MaterialIDMap);
		}
		FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			Converter.Convert(&ReadMesh, *MeshDescription, !Options.bEnableRecomputeTangents);
		});

		ApplyInverseBuildScale(*MeshDescription, BuildScale);

		// Setting to prevent the standard static mesh reduction from running and replacing the render LOD.
		FStaticMeshSourceModel& ThisSourceModel = ToStaticMeshAsset->GetSourceModel(UseLODIndex);
		ThisSourceModel.ResetReductionSetting();

		if (Options.bApplyNaniteSettings)
		{
			ToStaticMeshAsset->NaniteSettings = Options.NewNaniteSettings;
		}

		if (Options.bReplaceMaterials)
		{
			bool bHaveSlotNames = (Options.NewMaterialSlotNames.Num() == Options.NewMaterials.Num());

			TArray<FStaticMaterial> NewMaterials;
			for (int32 k = 0; k < Options.NewMaterials.Num(); ++k)
			{
				FStaticMaterial NewMaterial;
				NewMaterial.MaterialInterface = Options.NewMaterials[k];
				FName UseSlotName = (bHaveSlotNames && Options.NewMaterialSlotNames[k] != NAME_None) ? Options.NewMaterialSlotNames[k] :
					UE::AssetUtils::GenerateNewMaterialSlotName(NewMaterials, NewMaterial.MaterialInterface, k);

				NewMaterial.MaterialSlotName = UseSlotName;
				NewMaterial.ImportedMaterialSlotName = UseSlotName;
				NewMaterial.UVChannelData = FMeshUVChannelInfo(1.f);		// this avoids an ensure in  UStaticMesh::GetUVChannelData
				NewMaterials.Add(NewMaterial);
			}

			ToStaticMeshAsset->SetStaticMaterials(NewMaterials);

			// Set material slot names on the mesh description
			FStaticMeshAttributes Attributes(*MeshDescription);
			TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
			for (int32 SlotIdx = 0; SlotIdx < NewMaterials.Num(); ++SlotIdx)
			{
				if (SlotIdx < PolygonGroupImportedMaterialSlotNames.GetNumElements())
				{
					PolygonGroupImportedMaterialSlotNames.Set(SlotIdx, NewMaterials[SlotIdx].ImportedMaterialSlotName);
				}
			}

			// Reset the section info map
			ToStaticMeshAsset->GetSectionInfoMap().Clear();
			ToStaticMeshAsset->GetOriginalSectionInfoMap().Clear();

			// Repopulate section info map
			FMeshSectionInfoMap SectionInfoMap;
			for (int32 LODIndex = 0, NumLODs = ToStaticMeshAsset->GetNumSourceModels(); LODIndex < NumLODs; ++LODIndex)
			{
				if (const FMeshDescription* Mesh = (LODIndex == UseLODIndex) ? MeshDescription : ToStaticMeshAsset->GetMeshDescription(LODIndex))
				{
					FStaticMeshConstAttributes MeshDescriptionAttributes(*Mesh);
					TPolygonGroupAttributesConstRef<FName> MaterialSlotNames = MeshDescriptionAttributes.GetPolygonGroupMaterialSlotNames();
					int32 SectionIndex = 0;
					for (FPolygonGroupID PolygonGroupID : Mesh->PolygonGroups().GetElementIDs())
					{
						// Material index is either from the matching material slot name or the section index if that name is not found
						int32 MaterialIndex = ToStaticMeshAsset->GetStaticMaterials().IndexOfByPredicate(
							[&MaterialSlotName = MaterialSlotNames[PolygonGroupID]](const FStaticMaterial& StaticMaterial) { return StaticMaterial.MaterialSlotName == MaterialSlotName; }
						);
						if (MaterialIndex == INDEX_NONE)
						{
							MaterialIndex = SectionIndex;
						}
						SectionInfoMap.Set(LODIndex, SectionIndex, FMeshSectionInfo(MaterialIndex));
						SectionIndex++;
					}
				}
			}
			ToStaticMeshAsset->GetSectionInfoMap().CopyFrom(SectionInfoMap);
			ToStaticMeshAsset->GetOriginalSectionInfoMap().CopyFrom(SectionInfoMap);
		}

		ToStaticMeshAsset->CommitMeshDescription(UseLODIndex);
	}

	if (Options.bDeferMeshPostEditChange == false)
	{
		ToStaticMeshAsset->PostEditChange();
	}

	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}

	Outcome = EGeometryScriptOutcomePins::Success;

#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_EditorOnly", "CopyMeshToStaticMesh: Not currently supported at Runtime"));
#endif

	return FromDynamicMesh;
}




bool UGeometryScriptLibrary_StaticMeshFunctions::CheckStaticMeshHasAvailableLOD(
	UStaticMesh* FromStaticMeshAsset,
	FGeometryScriptMeshReadLOD RequestedLOD,
	EGeometryScriptSearchOutcomePins& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CheckStaticMeshHasAvailableLOD_InvalidInput1", "CheckStaticMeshHasAvailableLOD: FromStaticMeshAsset is Null"));
		return false;
	}

	if (RequestedLOD.LODType == EGeometryScriptLODType::RenderData)
	{
		Outcome = (RequestedLOD.LODIndex >= 0 && RequestedLOD.LODIndex < FromStaticMeshAsset->GetNumLODs()) ?
			EGeometryScriptSearchOutcomePins::Found : EGeometryScriptSearchOutcomePins::NotFound;

#if !WITH_EDITOR
		if (FromStaticMeshAsset->bAllowCPUAccess == false)
		{
			Outcome = EGeometryScriptSearchOutcomePins::NotFound;
		}
#endif

		return (Outcome == EGeometryScriptSearchOutcomePins::Found);
	}

#if WITH_EDITOR
	bool bResult = false;
	if (RequestedLOD.LODType == EGeometryScriptLODType::HiResSourceModel)
	{
		bResult = FromStaticMeshAsset->IsHiResMeshDescriptionValid();
	}
	else if (RequestedLOD.LODType == EGeometryScriptLODType::SourceModel)
	{
		bResult = RequestedLOD.LODIndex >= 0
			&& RequestedLOD.LODIndex < FromStaticMeshAsset->GetNumSourceModels()
			&& FromStaticMeshAsset->IsSourceModelValid(RequestedLOD.LODIndex);
	}
	else if (RequestedLOD.LODType == EGeometryScriptLODType::MaxAvailable)
	{
		bResult = (FromStaticMeshAsset->GetNumSourceModels() > 0);
	}
	Outcome = (bResult) ? EGeometryScriptSearchOutcomePins::Found : EGeometryScriptSearchOutcomePins::NotFound;
	return bResult;

#else
	Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	return false;
#endif
}



int UGeometryScriptLibrary_StaticMeshFunctions::GetNumStaticMeshLODsOfType(
	UStaticMesh* FromStaticMeshAsset,
	EGeometryScriptLODType LODType)
{
	if (FromStaticMeshAsset == nullptr) return 0;

#if WITH_EDITOR
	if (LODType == EGeometryScriptLODType::RenderData)
	{
		return FromStaticMeshAsset->GetNumLODs();
	}
	if (LODType == EGeometryScriptLODType::HiResSourceModel)
	{
		return FromStaticMeshAsset->IsHiResMeshDescriptionValid() ? 1 : 0;
	}
	if (LODType == EGeometryScriptLODType::SourceModel || LODType == EGeometryScriptLODType::MaxAvailable)
	{
		return FromStaticMeshAsset->GetNumSourceModels();
	}
#else
	if (LODType == EGeometryScriptLODType::RenderData && FromStaticMeshAsset->bAllowCPUAccess)
	{
		return FromStaticMeshAsset->GetNumLODs();
	}
#endif

	return 0;
}



void UGeometryScriptLibrary_StaticMeshFunctions::GetMaterialListFromStaticMesh(
	UStaticMesh* FromStaticMeshAsset,
	TArray<UMaterialInterface*>& MaterialList,
	UGeometryScriptDebug* Debug)
{
	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetMaterialListFromStaticMesh_InvalidInput1", "GetMaterialListFromStaticMesh: FromStaticMeshAsset is Null"));
		return;
	}

	const TArray<FStaticMaterial>& AssetMaterials = FromStaticMeshAsset->GetStaticMaterials();
	for (int32 k = 0; k < AssetMaterials.Num(); ++k)
	{
		MaterialList.Add(AssetMaterials[k].MaterialInterface);
	}
}

void UGeometryScriptLibrary_StaticMeshFunctions::GetSectionMaterialListFromStaticMesh(
	UStaticMesh* FromStaticMeshAsset, 
	FGeometryScriptMeshReadLOD RequestedLOD,
	TArray<UMaterialInterface*>& MaterialList,
	TArray<int32>& MaterialIndex,
	TArray<FName>& MaterialSlotNames,
	EGeometryScriptOutcomePins& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_InvalidInput1", "GetSectionMaterialListFromStaticMesh: FromStaticMeshAsset is Null"));
		return;
	}

	// RenderData mesh sections directly reference a Material Index, which is set as the MaterialID in CopyMeshFromStaticMesh_RenderData
	if (RequestedLOD.LODType == EGeometryScriptLODType::RenderData)
	{
		MaterialList.Reset();
		MaterialIndex.Reset();
		MaterialSlotNames.Reset();
		const TArray<FStaticMaterial>& AssetMaterials = FromStaticMeshAsset->GetStaticMaterials();
		for (int32 k = 0; k < AssetMaterials.Num(); ++k)
		{
			MaterialList.Add(AssetMaterials[k].MaterialInterface);
			MaterialIndex.Add(k);
			MaterialSlotNames.Add(AssetMaterials[k].MaterialSlotName);
		}

		Outcome = EGeometryScriptOutcomePins::Success;
		return;
	}

#if WITH_EDITOR

	if (RequestedLOD.LODType != EGeometryScriptLODType::MaxAvailable && RequestedLOD.LODType != EGeometryScriptLODType::SourceModel)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_LODNotAvailable", "GetSectionMaterialListFromStaticMesh: Requested LOD is not available"));
		return;
	}

	int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromStaticMeshAsset->GetNumSourceModels() - 1);

	MaterialList.Reset();
	MaterialIndex.Reset();
	MaterialSlotNames.Reset();
	if (UE::AssetUtils::GetStaticMeshLODMaterialListBySection(FromStaticMeshAsset, UseLODIndex, MaterialList, MaterialIndex, MaterialSlotNames) == false)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_QueryFailed", "GetSectionMaterialListFromStaticMesh: Could not fetch Material Set from Asset"));
		return;
	}

	Outcome = EGeometryScriptOutcomePins::Success;

#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_EditorOnly", "GetSectionMaterialListFromStaticMesh: Source Models are not available at Runtime"));
#endif
}


namespace UELocal
{
	bool CopyMeshFromSkeletalMesh_RenderData(USkeletalMesh* FromSkeletalMeshAsset, FGeometryScriptCopyMeshFromAssetOptions AssetOptions, int32 LODIndex, UDynamicMesh* ToDynamicMesh, UGeometryScriptDebug* Debug)
	{
		

		if (FSkeletalMeshRenderData* RenderData = FromSkeletalMeshAsset->GetResourceForRendering())
		{
			int32 NumLODs = RenderData->LODRenderData.Num();
			if (NumLODs -1 < LODIndex )
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_RenderDataLDONotAvailable", "CopyMeshFromSkeletalMesh: Renderdata for specified LOD is not available"));
				return false;
			}

			FSkeletalMeshLODRenderData* SkeletalMeshLODRenderData = &(RenderData->LODRenderData[LODIndex]);

			UE::Geometry::FDynamicMesh3 NewMesh;
			
			UE::Geometry::FSkeletalMeshLODRenderDataToDynamicMesh::ConversionOptions  ConversionOptions;
			ConversionOptions.bWantTangents = AssetOptions.bRequestTangents;

			UE::Geometry::FSkeletalMeshLODRenderDataToDynamicMesh::Convert(SkeletalMeshLODRenderData, FromSkeletalMeshAsset->GetRefSkeleton(), ConversionOptions, NewMesh);
			ToDynamicMesh->SetMesh(MoveTemp(NewMesh));

			return true;
		}
	
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_RenderDataNotAvailable", "CopyMeshFromSkeletalMesh: Renderdata is not available"));
		return false;
		
	}
	
};

UDynamicMesh* UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
		USkeletalMesh* FromSkeletalMeshAsset, 
		UDynamicMesh* ToDynamicMesh,
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromSkeletalMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_InvalidInput1", "CopyMeshFromSkeletalMesh: FromSkeletalMeshAsset is Null"));
		return ToDynamicMesh;
	}
	if (ToDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_InvalidInput2", "CopyMeshFromSkeletalMesh: ToDynamicMesh is Null"));
		return ToDynamicMesh;
	}

	// TODO: Consolidate this code with SkeletalMeshToolTarget::GetMeshDescription(..) 
	const int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromSkeletalMeshAsset->GetLODNum() - 1);;
	
	if (RequestedLOD.LODType == EGeometryScriptLODType::MaxAvailable || RequestedLOD.LODType == EGeometryScriptLODType::SourceModel)
	{
#if WITH_EDITOR
		const FMeshDescription* SourceMesh = nullptr;

		// Check first if we have bulk data available and non-empty.
		if (FromSkeletalMeshAsset->HasMeshDescription(UseLODIndex))
		{
			SourceMesh = FromSkeletalMeshAsset->GetMeshDescription(UseLODIndex); 
		}
		if (SourceMesh == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_LODNotAvailable", "CopyMeshFromSkeletalMesh: Requested LOD source mesh is not available"));
			return ToDynamicMesh;
		}

		FDynamicMesh3 NewMesh;
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(SourceMesh, NewMesh, AssetOptions.bRequestTangents);
	
		ToDynamicMesh->SetMesh(MoveTemp(NewMesh));
	
		Outcome = EGeometryScriptOutcomePins::Success;
#else 
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_SourceMesh_EditorOnly", "CopyMeshFromSkeletalMesh: Source Meshes are not available at Runtime"));
	
#endif	
	}
	else if (RequestedLOD.LODType == EGeometryScriptLODType::RenderData)
	{
		if (UELocal::CopyMeshFromSkeletalMesh_RenderData(FromSkeletalMeshAsset, AssetOptions, RequestedLOD.LODIndex, ToDynamicMesh, Debug))
		{
			Outcome = EGeometryScriptOutcomePins::Success;
		}
		
	}

// todo remove this
#if !WITH_EDITOR

	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_EditorOnly", "CopyMeshFromSkeletalMesh: Not currently supported at Runtime"));
#endif
	
	return ToDynamicMesh;
}

#if WITH_EDITOR
namespace UELocal 
{

// this is identical to UE::AssetUtils::GenerateNewMaterialSlotName except it takes a TArray<FSkeletalMaterial>
// instead of a TArray<FStaticMaterial>. It seems likely that we will need SkeletalMeshMaterialUtil.h soon,
// at that point this function can be moved there
static FName GenerateNewMaterialSlotName(
	const TArray<FSkeletalMaterial>& ExistingMaterials,
	UMaterialInterface* SlotMaterial,
	int32 NewSlotIndex)
{
	FString MaterialName = (SlotMaterial) ? SlotMaterial->GetName() : TEXT("Material");
	FName BaseName(MaterialName);

	bool bFound = false;
	for (const FSkeletalMaterial& Mat : ExistingMaterials)
	{
		if (Mat.MaterialSlotName == BaseName || Mat.ImportedMaterialSlotName == BaseName)
		{
			bFound = true;
			break;
		}
	}
	if (bFound == false && SlotMaterial != nullptr)
	{
		return BaseName;
	}

	bFound = true;
	while (bFound)
	{
		bFound = false;

		BaseName = FName(FString::Printf(TEXT("%s_%d"), *MaterialName, NewSlotIndex++));
		for (const FSkeletalMaterial& Mat : ExistingMaterials)
		{
			if (Mat.MaterialSlotName == BaseName || Mat.ImportedMaterialSlotName == BaseName)
			{
				bFound = true;
				break;
			}
		}
	}

	return BaseName;
}
}
#endif


UDynamicMesh* UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
		UDynamicMesh* FromDynamicMesh, 
		USkeletalMesh* ToSkeletalMeshAsset,
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_InvalidInput1", "CopyMeshToSkeletalMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (ToSkeletalMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_InvalidInput2", "CopyMeshToSkeletalMesh: ToSkeletalMeshAsset is Null"));
		return FromDynamicMesh;
	}
	if (TargetLOD.bWriteHiResSource)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_Unsupported", "CopyMeshToSkeletalMesh: Writing HiResSource LOD is not yet supported"));
		return FromDynamicMesh;
	}

	// TODO: Consolidate this code with SkeletalMeshToolTarget::CommitMeshDescription
#if WITH_EDITOR
	if (ToSkeletalMeshAsset->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		const FText Error = FText::Format(LOCTEXT("CopyMeshToSkeletalMesh_BuiltInAsset", "CopyMeshToSkeletalMesh: Cannot modify built-in engine asset: {0}"), FText::FromString(*ToSkeletalMeshAsset->GetPathName()));
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, Error);
		return FromDynamicMesh;
	}

	// flush any pending rendering commands, which might touch a component while we are rebuilding it's mesh
	FlushRenderingCommands();

	if (Options.bEmitTransaction)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateSkeletalMesh", "Update Skeletal Mesh"));
	}

	// make sure transactional flag is on for this asset
	ToSkeletalMeshAsset->SetFlags(RF_Transactional);

	verify(ToSkeletalMeshAsset->Modify());

	// Ensure we have enough LODInfos to cover up to the requested LOD.
	for (int32 LODIndex = ToSkeletalMeshAsset->GetLODNum(); LODIndex <= TargetLOD.LODIndex; LODIndex++)
	{
		FSkeletalMeshLODInfo& LODInfo = ToSkeletalMeshAsset->AddLODInfo();
		
		ToSkeletalMeshAsset->GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel);
		LODInfo.ReductionSettings.BaseLOD = 0;
	}

	FMeshDescription* MeshDescription = ToSkeletalMeshAsset->CreateMeshDescription(TargetLOD.LODIndex);

	if (MeshDescription == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_TargetMeshDescription", "CopyMeshToSkeletalMesh: Failed to generate the mesh data for the Target LOD Index"));
		return FromDynamicMesh;
	}
	
	FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
	MeshAttributes.Register();

	ToSkeletalMeshAsset->ModifyMeshDescription(TargetLOD.LODIndex);
	
	FConversionToMeshDescriptionOptions ConversionOptions;
	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Converter.Convert(&ReadMesh, *MeshDescription, !Options.bEnableRecomputeTangents);
	});

	FSkeletalMeshLODInfo* SkeletalLODInfo = ToSkeletalMeshAsset->GetLODInfo(TargetLOD.LODIndex);
	SkeletalLODInfo->BuildSettings.bRecomputeNormals = Options.bEnableRecomputeNormals;
	SkeletalLODInfo->BuildSettings.bRecomputeTangents = Options.bEnableRecomputeTangents;
	
	// Prevent decimation of this LOD.
	SkeletalLODInfo->ReductionSettings.NumOfTrianglesPercentage = 1.0;
	SkeletalLODInfo->ReductionSettings.NumOfVertPercentage = 1.0;
	SkeletalLODInfo->ReductionSettings.MaxNumOfTriangles = MAX_int32;
	SkeletalLODInfo->ReductionSettings.MaxNumOfVerts = MAX_int32; 
	SkeletalLODInfo->ReductionSettings.BaseLOD = TargetLOD.LODIndex;

	// update materials on the Asset
	if (Options.bReplaceMaterials)
	{
		bool bHaveSlotNames = (Options.NewMaterialSlotNames.Num() == Options.NewMaterials.Num());

		TArray<FSkeletalMaterial> NewMaterials;
		for (int32 k = 0; k < Options.NewMaterials.Num(); ++k)
		{
			FSkeletalMaterial NewMaterial;
			NewMaterial.MaterialInterface = Options.NewMaterials[k];
			FName UseSlotName = (bHaveSlotNames && Options.NewMaterialSlotNames[k] != NAME_None) ? Options.NewMaterialSlotNames[k] :
				UELocal::GenerateNewMaterialSlotName(NewMaterials, NewMaterial.MaterialInterface, k);

			NewMaterial.MaterialSlotName = UseSlotName;
			NewMaterial.ImportedMaterialSlotName = UseSlotName;
			NewMaterial.UVChannelData = FMeshUVChannelInfo(1.f);		// this avoids an ensure in  UStaticMesh::GetUVChannelData
			NewMaterials.Add(NewMaterial);
		}
		
		ToSkeletalMeshAsset->SetMaterials(NewMaterials);
	}

	ToSkeletalMeshAsset->CommitMeshDescription(TargetLOD.LODIndex);

	bool bHasVertexColors = false;
	TVertexInstanceAttributesConstRef<FVector4f> VertexColors = MeshAttributes.GetVertexInstanceColors();
	for (const FVertexInstanceID VertexInstanceID: MeshDescription->VertexInstances().GetElementIDs())
	{
		if (!VertexColors.Get(VertexInstanceID).Equals(FVector4f::One()))
		{
			bHasVertexColors = true;
			break;
		}
	}
		
	// configure vertex color setup in the Asset
	ToSkeletalMeshAsset->SetHasVertexColors(bHasVertexColors);
#if WITH_EDITORONLY_DATA
	ToSkeletalMeshAsset->SetVertexColorGuid(bHasVertexColors ? FGuid::NewGuid() : FGuid() );
#endif
	
	if (Options.bDeferMeshPostEditChange == false)
	{
		ToSkeletalMeshAsset->PostEditChange();
	}

	if (Options.bEmitTransaction)
	{
		GEditor->EndTransaction();
	}
	
	Outcome = EGeometryScriptOutcomePins::Success;
#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_EditorOnly", "CopyMeshToSkeletalMesh: Not currently supported at Runtime"));
#endif
	
	return FromDynamicMesh;
}


UDynamicMesh* UGeometryScriptLibrary_StaticMeshFunctions::CopyMorphTargetToSkeletalMesh(
		UDynamicMesh* FromDynamicMesh, 
		USkeletalMesh* ToSkeletalMeshAsset,
		FName MorphTargetName,
		FGeometryScriptCopyMorphTargetToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (ToSkeletalMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_InvalidInput1", "CopyMorphTargetToSkeletalMesh: ToSkeletalMeshAsset is Null"));
		return FromDynamicMesh;
	}
	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_InvalidInput2", "CopyMorphTargetToSkeletalMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}

#if WITH_EDITOR
	if (ToSkeletalMeshAsset->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		const FText Error = FText::Format(LOCTEXT("CopyMorphTargetToSkeletalMesh_BuiltInAsset", "CopyMorphTargetToSkeletalMesh: Cannot modify built-in engine asset: {0}"), FText::FromString(*ToSkeletalMeshAsset->GetPathName()));
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, Error);
		return FromDynamicMesh;
	}

	// flush any pending rendering commands, which might touch a component while we are rebuilding it's mesh
	FlushRenderingCommands();

	if (Options.bEmitTransaction)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateSkeletalMesh", "Update Skeletal Mesh"));
	}
	
	// make sure transactional flag is on for this asset
	ToSkeletalMeshAsset->SetFlags(RF_Transactional);

	(void)ToSkeletalMeshAsset->Modify();
	
	// Ensure we have enough LODInfos to cover up to the requested LOD.
	for (int32 LODIndex = ToSkeletalMeshAsset->GetLODNum(); LODIndex <= TargetLOD.LODIndex; LODIndex++)
	{
		FSkeletalMeshLODInfo& LODInfo = ToSkeletalMeshAsset->AddLODInfo();
		
		ToSkeletalMeshAsset->GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel);
		LODInfo.ReductionSettings.BaseLOD = 0;
	}

	FMeshDescription* MeshDescription = ToSkeletalMeshAsset->GetMeshDescription(TargetLOD.LODIndex);

	if (MeshDescription == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_TargetMeshDescription", "CopyMorphTargetToSkeletalMesh: Failed to generate the mesh data for the Target LOD Index"));
		return FromDynamicMesh;
	}

	// Morph targets must be compact and have the same number of vertices as the skeletal asset mesh description
	if (FromDynamicMesh->GetMeshRef().MaxVertexID() != FromDynamicMesh->GetMeshRef().VertexCount() ||
		FromDynamicMesh->GetMeshRef().MaxVertexID() != MeshDescription->Vertices().Num())
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_InvalidMorphTargetGeometry", "CopyMorphTargetToSkeletalMesh: Morph target mesh doesnt have the same number of vertices as the skeletal mesh."));
		return FromDynamicMesh;
	}

	FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
	MeshAttributes.Register();

	ToSkeletalMeshAsset->ModifyMeshDescription(TargetLOD.LODIndex);

	if (MeshAttributes.GetMorphTargetNames().Contains(MorphTargetName))
	{
		if (!Options.bOverwriteExistingTarget) // only throw error if we dont want to overwrite the existing target 
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_InvalidMorphTargetName1", "CopyMorphTargetToSkeletalMesh: Morph target name already exists"));
			return FromDynamicMesh;
		}
	}
	else
	{
		if (!MeshAttributes.RegisterMorphTargetAttribute(MorphTargetName, false))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_InvalidMorphTargetName2", "CopyMorphTargetToSkeletalMesh: Morph target name is invalid."));
			return FromDynamicMesh;
		}
	}

	TVertexAttributesRef<FVector3f> PositionDelta = MeshAttributes.GetVertexMorphPositionDelta(MorphTargetName);
	TVertexAttributesRef<FVector3f> VertexPositions = MeshAttributes.GetVertexPositions();

	for (int32 VID = 0; VID < FromDynamicMesh->GetMeshRef().MaxVertexID(); ++VID)
	{
		const FVector3d V0 = FromDynamicMesh->GetMeshRef().GetVertex(VID);
		const FVector3f V1 = VertexPositions[VID];

		PositionDelta.Set(VID, FVector3f(float(V0[0]) - V1[0], 
										 float(V0[1]) - V1[1], 
										 float(V0[2]) - V1[2]));
	}

	ToSkeletalMeshAsset->CommitMeshDescription(TargetLOD.LODIndex);

	if (Options.bDeferMeshPostEditChange == false)
	{
		ToSkeletalMeshAsset->PostEditChange();
	}

	if (Options.bEmitTransaction)
	{
		GEditor->EndTransaction();
	}

	Outcome = EGeometryScriptOutcomePins::Success;
#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMorphTargetToSkeletalMesh_EditorOnly", "CopyMorphTargetToSkeletalMesh: Not currently supported at Runtime"));
#endif
	
	return FromDynamicMesh;
}

#undef LOCTEXT_NAMESPACE
