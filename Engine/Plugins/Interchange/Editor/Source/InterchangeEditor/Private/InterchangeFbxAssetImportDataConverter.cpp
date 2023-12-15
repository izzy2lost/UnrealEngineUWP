// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeFbxAssetImportDataConverter.h"

#include "Animation/AnimSequence.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxAssetImportData.h"
#include "Factories/FbxMeshImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "InterchangeAnimSequenceFactoryNode.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

namespace UE::Interchange::Private
{
	void TransferSourceFileInformation(const UAssetImportData* SourceData, UAssetImportData* DestinationData)
	{
		TArray<FAssetImportInfo::FSourceFile> SourceFiles = SourceData->GetSourceData().SourceFiles;
		DestinationData->SetSourceFiles(MoveTemp(SourceFiles));
	}

	void FillFbxAssetImportData(const UInterchangeGenericAssetsPipeline* GenericAssetPipeline, UFbxAssetImportData* AssetImportData)
	{
		AssetImportData->bConvertScene = true;
		AssetImportData->bConvertSceneUnit = true;
		AssetImportData->bForceFrontXAxis = false;
		AssetImportData->bImportAsScene = false;
		AssetImportData->ImportRotation = GenericAssetPipeline->ImportOffsetRotation;
		AssetImportData->ImportTranslation = GenericAssetPipeline->ImportOffsetTranslation;
		AssetImportData->ImportUniformScale = GenericAssetPipeline->ImportOffsetUniformScale;
	}

	void FillFbxMeshImportData(const UInterchangeGenericAssetsPipeline* GenericAssetPipeline, UFbxMeshImportData* MeshImportData)
	{
		MeshImportData->bBakePivotInVertex = false;
		MeshImportData->bComputeWeightedNormals = GenericAssetPipeline->CommonMeshesProperties->bComputeWeightedNormals;
		MeshImportData->bImportMeshLODs = GenericAssetPipeline->CommonMeshesProperties->bImportLods;
		MeshImportData->bReorderMaterialToFbxOrder = true;
		MeshImportData->bTransformVertexToAbsolute = GenericAssetPipeline->CommonMeshesProperties->bBakeMeshes;

		if (GenericAssetPipeline->CommonMeshesProperties->bUseMikkTSpace)
		{
			MeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
		}
		else
		{
			MeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::BuiltIn;
		}

		if (GenericAssetPipeline->CommonMeshesProperties->bRecomputeNormals)
		{
			MeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ComputeNormals;
		}
		else
		{
			if (GenericAssetPipeline->CommonMeshesProperties->bRecomputeTangents)
			{
				MeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ImportNormals;
			}
			else
			{
				MeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ImportNormalsAndTangents;
			}
		}
	}

	void FillInterchangeGenericAssetsPipelineFromFbxMeshImportData(UInterchangeGenericAssetsPipeline* GenericAssetPipeline, const UFbxMeshImportData* LegacyMeshImportData)
	{
		GenericAssetPipeline->CommonMeshesProperties->bComputeWeightedNormals = LegacyMeshImportData->bComputeWeightedNormals;
		GenericAssetPipeline->CommonMeshesProperties->bImportLods = LegacyMeshImportData->bImportMeshLODs;
		GenericAssetPipeline->CommonMeshesProperties->bBakeMeshes = LegacyMeshImportData->bTransformVertexToAbsolute;

		if (LegacyMeshImportData->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace)
		{
			GenericAssetPipeline->CommonMeshesProperties->bUseMikkTSpace = true;
		}
		else
		{
			GenericAssetPipeline->CommonMeshesProperties->bUseMikkTSpace = false;
		}

		if (LegacyMeshImportData->NormalImportMethod == EFBXNormalImportMethod::FBXNIM_ComputeNormals)
		{
			GenericAssetPipeline->CommonMeshesProperties->bRecomputeNormals = true;
		}
		else
		{
			if (LegacyMeshImportData->NormalImportMethod == EFBXNormalImportMethod::FBXNIM_ImportNormals)
			{
				GenericAssetPipeline->CommonMeshesProperties->bRecomputeTangents = true;
			}
			else
			{
				GenericAssetPipeline->CommonMeshesProperties->bRecomputeTangents = false;
			}
		}
	}

	void FillInterchangeGenericAssetsPipelineFromFbxStaticMeshImportData(UInterchangeGenericAssetsPipeline* GenericAssetPipeline
		, const UFbxStaticMeshImportData* StaticMeshImportData
		, UInterchangeBaseNodeContainer* DestinationContainer
		, const FString& NodeUniqueId
		, const FString& NodeDisplayLabel)
	{
		UInterchangeStaticMeshFactoryNode* MeshNode = NewObject<UInterchangeStaticMeshFactoryNode>(DestinationContainer);
		MeshNode->InitializeStaticMeshNode(NodeUniqueId, NodeDisplayLabel, UStaticMesh::StaticClass()->GetName());
		DestinationContainer->AddNode(MeshNode);

		FillInterchangeGenericAssetsPipelineFromFbxMeshImportData(GenericAssetPipeline, StaticMeshImportData);

		GenericAssetPipeline->MeshPipeline->bImportCollision = StaticMeshImportData->bAutoGenerateCollision;
		GenericAssetPipeline->MeshPipeline->bBuildNanite = StaticMeshImportData->bBuildNanite;
		GenericAssetPipeline->MeshPipeline->bBuildReversedIndexBuffer = StaticMeshImportData->bBuildReversedIndexBuffer;
		GenericAssetPipeline->MeshPipeline->bCombineStaticMeshes = StaticMeshImportData->bCombineMeshes;
		GenericAssetPipeline->MeshPipeline->bGenerateLightmapUVs = StaticMeshImportData->bGenerateLightmapUVs;
		GenericAssetPipeline->MeshPipeline->bOneConvexHullPerUCX = StaticMeshImportData->bOneConvexHullPerUCX;
		GenericAssetPipeline->CommonMeshesProperties->bRemoveDegenerates = StaticMeshImportData->bRemoveDegenerates;
		GenericAssetPipeline->MeshPipeline->DistanceFieldResolutionScale = StaticMeshImportData->DistanceFieldResolutionScale;
		GenericAssetPipeline->MeshPipeline->LodGroup = StaticMeshImportData->StaticMeshLODGroup;
		if (StaticMeshImportData->VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Ignore;
		}
		else if (StaticMeshImportData->VertexColorImportOption == EVertexColorImportOption::Override)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Override;
		}
		else if (StaticMeshImportData->VertexColorImportOption == EVertexColorImportOption::Replace)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Replace;
		}

		GenericAssetPipeline->CommonMeshesProperties->VertexOverrideColor = StaticMeshImportData->VertexOverrideColor;
	}

	void FillInterchangeGenericAssetsPipelineFromFbxSkeletalMeshImportData(UInterchangeGenericAssetsPipeline* GenericAssetPipeline
		, const UFbxSkeletalMeshImportData* SkeletalMeshImportData
		, UInterchangeBaseNodeContainer* DestinationContainer
		, const FString& NodeUniqueId
		, const FString& NodeDisplayLabel)
	{
		UInterchangeSkeletalMeshFactoryNode* MeshNode = NewObject<UInterchangeSkeletalMeshFactoryNode>(DestinationContainer);
		MeshNode->InitializeSkeletalMeshNode(NodeUniqueId, NodeDisplayLabel, USkeletalMesh::StaticClass()->GetName());
		DestinationContainer->AddNode(MeshNode);

		FillInterchangeGenericAssetsPipelineFromFbxMeshImportData(GenericAssetPipeline, SkeletalMeshImportData);

		GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy = SkeletalMeshImportData->bImportMeshesInBoneHierarchy;
		GenericAssetPipeline->CommonMeshesProperties->bKeepSectionsSeparate = SkeletalMeshImportData->bKeepSectionsSeparate;
		GenericAssetPipeline->MeshPipeline->bImportMorphTargets = SkeletalMeshImportData->bImportMorphTargets;
		GenericAssetPipeline->MeshPipeline->bImportVertexAttributes = SkeletalMeshImportData->bImportVertexAttributes;
		GenericAssetPipeline->MeshPipeline->bUpdateSkeletonReferencePose = SkeletalMeshImportData->bUpdateSkeletonReferencePose;
		GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bUseT0AsRefPose = SkeletalMeshImportData->bUseT0AsRefPose;
		if (SkeletalMeshImportData->ImportContentType == EFBXImportContentType::FBXICT_All)
		{
			GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::All;
		}
		else if (SkeletalMeshImportData->ImportContentType == EFBXImportContentType::FBXICT_Geometry)
		{
			GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::Geometry;
		}
		else if (SkeletalMeshImportData->ImportContentType == EFBXImportContentType::FBXICT_SkinningWeights)
		{
			GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::SkinningWeights;
		}

		if (SkeletalMeshImportData->LastImportContentType == EFBXImportContentType::FBXICT_All)
		{
			GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::All;
		}
		else if (SkeletalMeshImportData->LastImportContentType == EFBXImportContentType::FBXICT_Geometry)
		{
			GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::Geometry;
		}
		else if (SkeletalMeshImportData->LastImportContentType == EFBXImportContentType::FBXICT_SkinningWeights)
		{
			GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType = EInterchangeSkeletalMeshContentType::SkinningWeights;
		}

		GenericAssetPipeline->MeshPipeline->MorphThresholdPosition = SkeletalMeshImportData->MorphThresholdPosition;
		GenericAssetPipeline->MeshPipeline->ThresholdPosition = SkeletalMeshImportData->ThresholdPosition;
		GenericAssetPipeline->MeshPipeline->ThresholdTangentNormal = SkeletalMeshImportData->ThresholdTangentNormal;
		GenericAssetPipeline->MeshPipeline->ThresholdUV = SkeletalMeshImportData->ThresholdUV;

		if (SkeletalMeshImportData->VertexColorImportOption == EVertexColorImportOption::Ignore)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Ignore;
		}
		else if (SkeletalMeshImportData->VertexColorImportOption == EVertexColorImportOption::Override)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Override;
		}
		else if (SkeletalMeshImportData->VertexColorImportOption == EVertexColorImportOption::Replace)
		{
			GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption = EInterchangeVertexColorImportOption::IVCIO_Replace;
		}
		GenericAssetPipeline->CommonMeshesProperties->VertexOverrideColor = SkeletalMeshImportData->VertexOverrideColor;
	}

	void FillInterchangeGenericAssetsPipelineFromFbxAnimSequenceImportData(UInterchangeGenericAssetsPipeline* GenericAssetPipeline
		, const UFbxAnimSequenceImportData* AnimSequenceImportData
		, UInterchangeBaseNodeContainer* DestinationContainer
		, const FString& NodeUniqueId
		, const FString& NodeDisplayLabel)
	{
		UInterchangeAnimSequenceFactoryNode* AnimationNode = NewObject<UInterchangeAnimSequenceFactoryNode>(DestinationContainer);
		AnimationNode->InitializeAnimSequenceNode(NodeUniqueId, NodeDisplayLabel);
		DestinationContainer->AddNode(AnimationNode);
		switch (AnimSequenceImportData->AnimationLength)
		{
		case EFBXAnimationLengthImportType::FBXALIT_ExportedTime:
		{
			GenericAssetPipeline->AnimationPipeline->AnimationRange = EInterchangeAnimationRange::Timeline;
			break;
		}
		case EFBXAnimationLengthImportType::FBXALIT_AnimatedKey:
		{
			GenericAssetPipeline->AnimationPipeline->AnimationRange = EInterchangeAnimationRange::Animated;
			break;
		}
		case EFBXAnimationLengthImportType::FBXALIT_SetRange:
		{
			GenericAssetPipeline->AnimationPipeline->AnimationRange = EInterchangeAnimationRange::SetRange;
			break;
		}
		}
		GenericAssetPipeline->AnimationPipeline->bAddCurveMetadataToSkeleton = AnimSequenceImportData->bAddCurveMetadataToSkeleton;
		GenericAssetPipeline->AnimationPipeline->bDeleteExistingCustomAttributeCurves = AnimSequenceImportData->bDeleteExistingCustomAttributeCurves;
		GenericAssetPipeline->AnimationPipeline->bDeleteExistingMorphTargetCurves = AnimSequenceImportData->bDeleteExistingMorphTargetCurves;
		GenericAssetPipeline->AnimationPipeline->bDeleteExistingNonCurveCustomAttributes = AnimSequenceImportData->bDeleteExistingNonCurveCustomAttributes;
		GenericAssetPipeline->AnimationPipeline->bDoNotImportCurveWithZero = AnimSequenceImportData->bDoNotImportCurveWithZero;
		GenericAssetPipeline->AnimationPipeline->bImportBoneTracks = AnimSequenceImportData->bImportBoneTracks;
		GenericAssetPipeline->AnimationPipeline->bImportCustomAttribute = AnimSequenceImportData->bImportCustomAttribute;
		GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy = AnimSequenceImportData->bImportMeshesInBoneHierarchy;
		GenericAssetPipeline->AnimationPipeline->bRemoveCurveRedundantKeys = AnimSequenceImportData->bRemoveRedundantKeys;
		GenericAssetPipeline->AnimationPipeline->bSetMaterialDriveParameterOnCustomAttribute = AnimSequenceImportData->bSetMaterialDriveParameterOnCustomAttribute;
		GenericAssetPipeline->AnimationPipeline->bSnapToClosestFrameBoundary = AnimSequenceImportData->bSnapToClosestFrameBoundary;
		GenericAssetPipeline->AnimationPipeline->bUse30HzToBakeBoneAnimation = AnimSequenceImportData->bUseDefaultSampleRate;
		GenericAssetPipeline->AnimationPipeline->CustomBoneAnimationSampleRate = AnimSequenceImportData->CustomSampleRate;
		GenericAssetPipeline->AnimationPipeline->FrameImportRange = AnimSequenceImportData->FrameImportRange;
		GenericAssetPipeline->AnimationPipeline->MaterialCurveSuffixes = AnimSequenceImportData->MaterialCurveSuffixes;
		GenericAssetPipeline->AnimationPipeline->SourceAnimationName = AnimSequenceImportData->SourceAnimationName;
	}

	UAssetImportData* ConvertToLegacyFbx(UStaticMesh* StaticMesh, const UInterchangeAssetImportData* InterchangeSourceData)
	{
		if (!InterchangeSourceData || !StaticMesh)
		{
			return nullptr;
		}

		//Create a fbx asset import data and fill the options
		UFbxStaticMeshImportData* DestinationStaticMeshImportData = NewObject<UFbxStaticMeshImportData>(StaticMesh);

		if (!DestinationStaticMeshImportData)
		{
			return nullptr;
		}

		//Transfer the Source file information
		TransferSourceFileInformation(InterchangeSourceData, DestinationStaticMeshImportData);

		//Now find the generic asset pipeline
		for (UObject* Pipeline : InterchangeSourceData->GetPipelines())
		{
			if (const UInterchangeGenericAssetsPipeline* GenericAssetPipeline = Cast<UInterchangeGenericAssetsPipeline>(Pipeline))
			{

				FillFbxAssetImportData(GenericAssetPipeline, DestinationStaticMeshImportData);
				FillFbxMeshImportData(GenericAssetPipeline, DestinationStaticMeshImportData);

				DestinationStaticMeshImportData->bAutoGenerateCollision = GenericAssetPipeline->MeshPipeline->bImportCollision;
				DestinationStaticMeshImportData->bBuildNanite = GenericAssetPipeline->MeshPipeline->bBuildNanite;
				DestinationStaticMeshImportData->bBuildReversedIndexBuffer = GenericAssetPipeline->MeshPipeline->bBuildReversedIndexBuffer;
				DestinationStaticMeshImportData->bCombineMeshes = GenericAssetPipeline->MeshPipeline->bCombineStaticMeshes;
				DestinationStaticMeshImportData->bGenerateLightmapUVs = GenericAssetPipeline->MeshPipeline->bGenerateLightmapUVs;
				DestinationStaticMeshImportData->bOneConvexHullPerUCX = GenericAssetPipeline->MeshPipeline->bOneConvexHullPerUCX;
				DestinationStaticMeshImportData->bRemoveDegenerates = GenericAssetPipeline->CommonMeshesProperties->bRemoveDegenerates;
				DestinationStaticMeshImportData->DistanceFieldResolutionScale = GenericAssetPipeline->MeshPipeline->DistanceFieldResolutionScale;
				DestinationStaticMeshImportData->StaticMeshLODGroup = GenericAssetPipeline->MeshPipeline->LodGroup;
				if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Ignore)
				{
					DestinationStaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Ignore;
				}
				else if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Override)
				{
					DestinationStaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Override;
				}
				else if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Replace)
				{
					DestinationStaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
				}
				DestinationStaticMeshImportData->VertexOverrideColor = GenericAssetPipeline->CommonMeshesProperties->VertexOverrideColor;

				//Fill the reimport material match data and section data
				FImportMeshLodSectionsData SectionData;
				for (const FStaticMaterial& Material : StaticMesh->GetStaticMaterials())
				{
					DestinationStaticMeshImportData->ImportMaterialOriginalNameData.Add(Material.ImportedMaterialSlotName);
					SectionData.SectionOriginalMaterialName.Add(Material.ImportedMaterialSlotName);
				}
				DestinationStaticMeshImportData->ImportMeshLodData.Add(SectionData);
			}
		}
		return DestinationStaticMeshImportData;
	}

	UAssetImportData* ConvertToLegacyFbx(USkeletalMesh* SkeletalMesh, const UInterchangeAssetImportData* InterchangeSourceData)
	{
		if (!InterchangeSourceData || !SkeletalMesh)
		{
			return nullptr;
		}

		//Create a fbx asset import data and fill the options
		UFbxSkeletalMeshImportData* DestinationSkeletalMeshImportData = NewObject<UFbxSkeletalMeshImportData>(SkeletalMesh);

		if (!DestinationSkeletalMeshImportData)
		{
			return nullptr;
		}

		//Transfer the Source file information
		TransferSourceFileInformation(InterchangeSourceData, DestinationSkeletalMeshImportData);

		//Now find the generic asset pipeline
		for (UObject* Pipeline : InterchangeSourceData->GetPipelines())
		{
			if (const UInterchangeGenericAssetsPipeline* GenericAssetPipeline = Cast<UInterchangeGenericAssetsPipeline>(Pipeline))
			{
				FillFbxAssetImportData(GenericAssetPipeline, DestinationSkeletalMeshImportData);
				FillFbxMeshImportData(GenericAssetPipeline, DestinationSkeletalMeshImportData);
				DestinationSkeletalMeshImportData->bImportMeshesInBoneHierarchy = GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy;
				DestinationSkeletalMeshImportData->bImportMorphTargets = GenericAssetPipeline->MeshPipeline->bImportMorphTargets;
				DestinationSkeletalMeshImportData->bImportVertexAttributes = GenericAssetPipeline->MeshPipeline->bImportVertexAttributes;
				DestinationSkeletalMeshImportData->bKeepSectionsSeparate = GenericAssetPipeline->CommonMeshesProperties->bKeepSectionsSeparate;
				DestinationSkeletalMeshImportData->bPreserveSmoothingGroups = true;
				DestinationSkeletalMeshImportData->bUpdateSkeletonReferencePose = GenericAssetPipeline->MeshPipeline->bUpdateSkeletonReferencePose;
				DestinationSkeletalMeshImportData->bUseT0AsRefPose = GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bUseT0AsRefPose;
				if (GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::All)
				{
					DestinationSkeletalMeshImportData->ImportContentType = EFBXImportContentType::FBXICT_All;
				}
				else if (GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::Geometry)
				{
					DestinationSkeletalMeshImportData->ImportContentType = EFBXImportContentType::FBXICT_Geometry;
				}
				else if (GenericAssetPipeline->MeshPipeline->SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::SkinningWeights)
				{
					DestinationSkeletalMeshImportData->ImportContentType = EFBXImportContentType::FBXICT_SkinningWeights;
				}

				if (GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::All)
				{
					DestinationSkeletalMeshImportData->LastImportContentType = EFBXImportContentType::FBXICT_All;
				}
				else if (GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::Geometry)
				{
					DestinationSkeletalMeshImportData->LastImportContentType = EFBXImportContentType::FBXICT_Geometry;
				}
				else if (GenericAssetPipeline->MeshPipeline->LastSkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::SkinningWeights)
				{
					DestinationSkeletalMeshImportData->LastImportContentType = EFBXImportContentType::FBXICT_SkinningWeights;
				}

				DestinationSkeletalMeshImportData->MorphThresholdPosition = GenericAssetPipeline->MeshPipeline->MorphThresholdPosition;
				DestinationSkeletalMeshImportData->ThresholdPosition = GenericAssetPipeline->MeshPipeline->ThresholdPosition;
				DestinationSkeletalMeshImportData->ThresholdTangentNormal = GenericAssetPipeline->MeshPipeline->ThresholdTangentNormal;
				DestinationSkeletalMeshImportData->ThresholdUV = GenericAssetPipeline->MeshPipeline->ThresholdUV;

				if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Ignore)
				{
					DestinationSkeletalMeshImportData->VertexColorImportOption = EVertexColorImportOption::Ignore;
				}
				else if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Override)
				{
					DestinationSkeletalMeshImportData->VertexColorImportOption = EVertexColorImportOption::Override;
				}
				else if (GenericAssetPipeline->CommonMeshesProperties->VertexColorImportOption == EInterchangeVertexColorImportOption::IVCIO_Replace)
				{
					DestinationSkeletalMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
				}
				DestinationSkeletalMeshImportData->VertexOverrideColor = GenericAssetPipeline->CommonMeshesProperties->VertexOverrideColor;

				//Fill the reimport material match data and section data
				FImportMeshLodSectionsData SectionData;
				for (const FSkeletalMaterial& Material : SkeletalMesh->GetMaterials())
				{
					DestinationSkeletalMeshImportData->ImportMaterialOriginalNameData.Add(Material.ImportedMaterialSlotName);
					SectionData.SectionOriginalMaterialName.Add(Material.ImportedMaterialSlotName);
				}
				DestinationSkeletalMeshImportData->ImportMeshLodData.Add(SectionData);
			}
		}
		return DestinationSkeletalMeshImportData;
	}

	UAssetImportData* ConvertToLegacyFbx(UAnimSequence* AnimSequence, const UInterchangeAssetImportData* InterchangeSourceData)
	{
		if (!InterchangeSourceData || !AnimSequence)
		{
			return nullptr;
		}

		//Create a fbx asset import data and fill the options
		UFbxAnimSequenceImportData* DestinationAnimSequenceImportData = NewObject<UFbxAnimSequenceImportData>(AnimSequence);

		if (!DestinationAnimSequenceImportData)
		{
			return nullptr;
		}

		//Transfer the Source file information
		TransferSourceFileInformation(InterchangeSourceData, DestinationAnimSequenceImportData);

		//Now find the generic asset pipeline
		for (UObject* Pipeline : InterchangeSourceData->GetPipelines())
		{
			if (const UInterchangeGenericAssetsPipeline* GenericAssetPipeline = Cast<UInterchangeGenericAssetsPipeline>(Pipeline))
			{
				FillFbxAssetImportData(GenericAssetPipeline, DestinationAnimSequenceImportData);

				switch (GenericAssetPipeline->AnimationPipeline->AnimationRange)
				{
				case EInterchangeAnimationRange::Timeline:
				{
					DestinationAnimSequenceImportData->AnimationLength = EFBXAnimationLengthImportType::FBXALIT_ExportedTime;
					break;
				}
				case EInterchangeAnimationRange::Animated:
				{
					DestinationAnimSequenceImportData->AnimationLength = EFBXAnimationLengthImportType::FBXALIT_AnimatedKey;
					break;
				}
				case EInterchangeAnimationRange::SetRange:
				{
					DestinationAnimSequenceImportData->AnimationLength = EFBXAnimationLengthImportType::FBXALIT_SetRange;
					break;
				}
				}
				DestinationAnimSequenceImportData->bAddCurveMetadataToSkeleton = GenericAssetPipeline->AnimationPipeline->bAddCurveMetadataToSkeleton;
				DestinationAnimSequenceImportData->bDeleteExistingCustomAttributeCurves = GenericAssetPipeline->AnimationPipeline->bDeleteExistingCustomAttributeCurves;
				DestinationAnimSequenceImportData->bDeleteExistingMorphTargetCurves = GenericAssetPipeline->AnimationPipeline->bDeleteExistingMorphTargetCurves;
				DestinationAnimSequenceImportData->bDeleteExistingNonCurveCustomAttributes = GenericAssetPipeline->AnimationPipeline->bDeleteExistingNonCurveCustomAttributes;
				DestinationAnimSequenceImportData->bDoNotImportCurveWithZero = GenericAssetPipeline->AnimationPipeline->bDoNotImportCurveWithZero;
				DestinationAnimSequenceImportData->bImportBoneTracks = GenericAssetPipeline->AnimationPipeline->bImportBoneTracks;
				DestinationAnimSequenceImportData->bImportCustomAttribute = GenericAssetPipeline->AnimationPipeline->bImportCustomAttribute;
				DestinationAnimSequenceImportData->bImportMeshesInBoneHierarchy = GenericAssetPipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportMeshesInBoneHierarchy;
				DestinationAnimSequenceImportData->bPreserveLocalTransform = false;
				DestinationAnimSequenceImportData->bRemoveRedundantKeys = GenericAssetPipeline->AnimationPipeline->bRemoveCurveRedundantKeys;
				DestinationAnimSequenceImportData->bSetMaterialDriveParameterOnCustomAttribute = GenericAssetPipeline->AnimationPipeline->bSetMaterialDriveParameterOnCustomAttribute;
				DestinationAnimSequenceImportData->bSnapToClosestFrameBoundary = GenericAssetPipeline->AnimationPipeline->bSnapToClosestFrameBoundary;
				DestinationAnimSequenceImportData->bUseDefaultSampleRate = GenericAssetPipeline->AnimationPipeline->bUse30HzToBakeBoneAnimation;
				DestinationAnimSequenceImportData->CustomSampleRate = GenericAssetPipeline->AnimationPipeline->CustomBoneAnimationSampleRate;
				DestinationAnimSequenceImportData->FrameImportRange = GenericAssetPipeline->AnimationPipeline->FrameImportRange;
				DestinationAnimSequenceImportData->MaterialCurveSuffixes = GenericAssetPipeline->AnimationPipeline->MaterialCurveSuffixes;
				DestinationAnimSequenceImportData->SourceAnimationName = GenericAssetPipeline->AnimationPipeline->SourceAnimationName;
			}
		}
		return DestinationAnimSequenceImportData;
	}

	UAssetImportData* ConvertToInterchange(UObject* Obj, const UFbxAssetImportData* FbxAssetImportData)
	{
		if (!FbxAssetImportData || !Obj)
		{
			return nullptr;
		}
		//Create a fbx asset import data and fill the options
		UInterchangeAssetImportData* DestinationData = NewObject<UInterchangeAssetImportData>(Obj);
		//Transfer the Source file information
		TransferSourceFileInformation(FbxAssetImportData, DestinationData);

		//Create a container
		UInterchangeBaseNodeContainer* DestinationContainer = NewObject<UInterchangeBaseNodeContainer>(DestinationData);
		const FString BasePathToRemove = FPaths::GetBaseFilename(FbxAssetImportData->GetFirstFilename()) + TEXT("_");
		FString NodeDisplayLabel = Obj->GetName();
		if (NodeDisplayLabel.StartsWith(BasePathToRemove))
		{
			NodeDisplayLabel.RightInline(NodeDisplayLabel.Len() - BasePathToRemove.Len());
		}
		const FString NodeUniqueId = FGuid::NewGuid().ToString(EGuidFormats::Base36Encoded) + TEXT("_") + NodeDisplayLabel;

		TArray<UObject*> Pipelines;
		UInterchangeGenericAssetsPipeline* GenericAssetPipeline = NewObject<UInterchangeGenericAssetsPipeline>(DestinationData);
		Pipelines.Add(GenericAssetPipeline);
		DestinationData->SetPipelines(Pipelines);

		GenericAssetPipeline->ReimportStrategy = EReimportStrategyFlags::ApplyNoProperties;
		GenericAssetPipeline->ImportOffsetRotation = FbxAssetImportData->ImportRotation;
		GenericAssetPipeline->ImportOffsetTranslation = FbxAssetImportData->ImportTranslation;
		GenericAssetPipeline->ImportOffsetUniformScale = FbxAssetImportData->ImportUniformScale;

		if (const UFbxStaticMeshImportData* LegacyStaticMeshImportData = Cast<UFbxStaticMeshImportData>(FbxAssetImportData))
		{
			check(Obj->IsA<UStaticMesh>());
			FillInterchangeGenericAssetsPipelineFromFbxStaticMeshImportData(GenericAssetPipeline
				, LegacyStaticMeshImportData
				, DestinationContainer
				, NodeUniqueId
				, NodeDisplayLabel);
		}
		else if (const UFbxSkeletalMeshImportData* LegacySkeletalMeshImportData = Cast<UFbxSkeletalMeshImportData>(FbxAssetImportData))
		{
			check(Obj->IsA<USkeletalMesh>());
			FillInterchangeGenericAssetsPipelineFromFbxSkeletalMeshImportData(GenericAssetPipeline
				, LegacySkeletalMeshImportData
				, DestinationContainer
				, NodeUniqueId
				, NodeDisplayLabel);
		}
		else if (const UFbxAnimSequenceImportData* LegacyAnimSequenceImportData = Cast<UFbxAnimSequenceImportData>(FbxAssetImportData))
		{
			check(Obj->IsA<UAnimSequence>());
			FillInterchangeGenericAssetsPipelineFromFbxAnimSequenceImportData(GenericAssetPipeline
				, LegacyAnimSequenceImportData
				, DestinationContainer
				, NodeUniqueId
				, NodeDisplayLabel);
		}

		if (UInterchangeFactoryBaseNode* DestinationFactoryNode = DestinationContainer->GetFactoryNode(NodeUniqueId))
		{
			DestinationFactoryNode->SetReimportStrategyFlags(EReimportStrategyFlags::ApplyNoProperties);
			DestinationFactoryNode->SetCustomReferenceObject(Obj);
			DestinationData->SetNodeContainer(DestinationContainer);
			DestinationData->NodeUniqueID = NodeUniqueId;
		}
		return DestinationData;
	}

	UAssetImportData* ConvertData(UObject* Obj, UAssetImportData* SourceData, const bool bInterchangeSupportTargetExtension)
	{
		if (const UInterchangeAssetImportData* InterchangeSourceData = Cast<UInterchangeAssetImportData>(SourceData))
		{
			if (bInterchangeSupportTargetExtension)
			{
				//This converter do not convert Interchange to Interchange
				return nullptr;
			}

			//Do not convert scene data
			if (InterchangeSourceData->SceneImportAsset.IsValid())
			{
				return nullptr;
			}

			//Convert Interchange import data to Legacy Fbx Import data
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj))
			{
				return ConvertToLegacyFbx(StaticMesh, InterchangeSourceData);
			}
			else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj))
			{
				return ConvertToLegacyFbx(SkeletalMesh, InterchangeSourceData);
			}
			else if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Obj))
			{
				return ConvertToLegacyFbx(AnimSequence, InterchangeSourceData);
			}
		}
		else if (const UFbxAssetImportData* LegacyFbxSourceData = Cast<UFbxAssetImportData>(SourceData))
		{
			if (!bInterchangeSupportTargetExtension)
			{
				//This converter do not convert Legacy Fbx to other format then Interchange.
				//This is probably a conversion from Legacy Fbx to Legacy Fbx which we do not need to do
				return nullptr;
			}

			//Do not convert scene data
			if (LegacyFbxSourceData->bImportAsScene)
			{
				return nullptr;
			}

			//Convert Legacy Fbx import data to Interchange Import data
			return ConvertToInterchange(Obj, LegacyFbxSourceData);
		}
		return nullptr;
	}
} //ns: UE::Interchange::Private

bool UInterchangeFbxAssetImportDataConverter::ConvertImportData(UObject* Obj, const FString& TargetExtension) const
{
	bool bResult = false;
	const FString TargetExtensionLower = TargetExtension.ToLower();
	bool bUseInterchangeFramework = UInterchangeManager::IsInterchangeImportEnabled();;
	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
	
	UAssetImportData* OldAssetData = nullptr;
	TArray<FString> InterchangeSupportedExtensions;
	if (Obj->IsA(UStaticMesh::StaticClass()) || Obj->IsA(USkeletalMesh::StaticClass()))
	{
		InterchangeSupportedExtensions = InterchangeManager.GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Meshes);
	}
	else if (Obj->IsA(UAnimSequence::StaticClass()))
	{
		InterchangeSupportedExtensions = InterchangeManager.GetSupportedAssetTypeFormats(EInterchangeTranslatorAssetType::Animations);
	}
	//Remove the detail of the extensions
	for (FString& Extension : InterchangeSupportedExtensions)
	{
		int32 FindIndex = INDEX_NONE;
		Extension.FindChar(';', FindIndex);
		if (FindIndex != INDEX_NONE && FindIndex < Extension.Len() && FindIndex > 0)
		{
			Extension.LeftInline(FindIndex);
		}
	}
	const bool bInterchangeSupportTargetExtension = bUseInterchangeFramework && InterchangeSupportedExtensions.Contains(TargetExtensionLower);
	
	if (TargetExtensionLower.Equals(TEXT("fbx")) || bInterchangeSupportTargetExtension)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj))
		{
			if (UAssetImportData* ConvertedAssetData = UE::Interchange::Private::ConvertData(StaticMesh, StaticMesh->GetAssetImportData(), bInterchangeSupportTargetExtension))
			{
				OldAssetData = StaticMesh->GetAssetImportData();
				StaticMesh->SetAssetImportData(ConvertedAssetData);
				bResult = true;
			}
		}
		else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj))
		{
			if (UAssetImportData* ConvertedAssetData = UE::Interchange::Private::ConvertData(SkeletalMesh, SkeletalMesh->GetAssetImportData(), bInterchangeSupportTargetExtension))
			{
				OldAssetData = SkeletalMesh->GetAssetImportData();
				SkeletalMesh->SetAssetImportData(ConvertedAssetData);
				bResult = true;
			}
		}
		else if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(Obj))
		{
			if (UAssetImportData* ConvertedAssetData = UE::Interchange::Private::ConvertData(AnimSequence, AnimSequence->AssetImportData, bInterchangeSupportTargetExtension))
			{
				OldAssetData = AnimSequence->AssetImportData;
				AnimSequence->AssetImportData = ConvertedAssetData;
				bResult = true;
			}
		}
	}
	
	//Make sure old import asset data will be deleted by the next garbage collect
	if (bResult && OldAssetData)
	{
		OldAssetData->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		OldAssetData->ClearFlags(RF_Public | RF_Standalone);
	}

	return bResult;
}


