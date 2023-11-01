// Copyright Epic Games, Inc. All Rights Reserved. 

#include "InterchangeGenericAssetsPipeline.h"

#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeAnimSequenceFactoryNode.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangePipelineMeshesUtilities.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSourceData.h"
#include "InterchangeStaticMeshFactoryNode.h"
#include "InterchangeStaticMeshLodDataNode.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/MetaData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericAssetsPipeline)

UInterchangeGenericAssetsPipeline::UInterchangeGenericAssetsPipeline()
{
	MaterialPipeline = CreateDefaultSubobject<UInterchangeGenericMaterialPipeline>("MaterialPipeline");
	CommonMeshesProperties = CreateDefaultSubobject<UInterchangeGenericCommonMeshesProperties>("CommonMeshesProperties");
	CommonSkeletalMeshesAndAnimationsProperties = CreateDefaultSubobject<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties>("CommonSkeletalMeshesAndAnimationsProperties");
	MeshPipeline = CreateDefaultSubobject<UInterchangeGenericMeshPipeline>("MeshPipeline");
	MeshPipeline->CommonMeshesProperties = CommonMeshesProperties;
	MeshPipeline->CommonSkeletalMeshesAndAnimationsProperties = CommonSkeletalMeshesAndAnimationsProperties;
	AnimationPipeline = CreateDefaultSubobject<UInterchangeGenericAnimationPipeline>("AnimationPipeline");
	AnimationPipeline->CommonSkeletalMeshesAndAnimationsProperties = CommonSkeletalMeshesAndAnimationsProperties;
	AnimationPipeline->CommonMeshesProperties = CommonMeshesProperties;
}

void UInterchangeGenericAssetsPipeline::PreDialogCleanup(const FName PipelineStackName)
{
	check(CommonSkeletalMeshesAndAnimationsProperties)
	//We always clean the pipeline skeleton when showing the dialog
	CommonSkeletalMeshesAndAnimationsProperties->Skeleton = nullptr;

	if (MaterialPipeline)
	{
		MaterialPipeline->PreDialogCleanup(PipelineStackName);
	}
	
	if (MeshPipeline)
	{
		MeshPipeline->PreDialogCleanup(PipelineStackName);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->PreDialogCleanup(PipelineStackName);
	}
	
	SaveSettings(PipelineStackName);
}

bool UInterchangeGenericAssetsPipeline::IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const
{
	if (MaterialPipeline && !MaterialPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (CommonMeshesProperties && !CommonMeshesProperties->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (CommonSkeletalMeshesAndAnimationsProperties && !CommonSkeletalMeshesAndAnimationsProperties->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (MeshPipeline && !MeshPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	if (AnimationPipeline && !AnimationPipeline->IsSettingsAreValid(OutInvalidReason))
	{
		return false;
	}

	return Super::IsSettingsAreValid(OutInvalidReason);
}

void UInterchangeGenericAssetsPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

	if (MaterialPipeline)
	{
		MaterialPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}

	if (MeshPipeline)
	{
		MeshPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->AdjustSettingsForContext(ImportType, ReimportAsset);
	}
}

#if WITH_EDITOR

void UInterchangeGenericAssetsPipeline::FilterPropertiesFromTranslatedData(UInterchangeBaseNodeContainer* InBaseNodeContainer)
{
	Super::FilterPropertiesFromTranslatedData(InBaseNodeContainer);

	if (MaterialPipeline)
	{
		MaterialPipeline->FilterPropertiesFromTranslatedData(InBaseNodeContainer);
	}

	if (CommonMeshesProperties && CommonSkeletalMeshesAndAnimationsProperties && MeshPipeline && AnimationPipeline)
	{
		CommonMeshesProperties->FilterPropertiesFromTranslatedData(InBaseNodeContainer);
		CommonSkeletalMeshesAndAnimationsProperties->FilterPropertiesFromTranslatedData(InBaseNodeContainer);
		MeshPipeline->FilterPropertiesFromTranslatedData(InBaseNodeContainer);
		AnimationPipeline->FilterPropertiesFromTranslatedData(InBaseNodeContainer);

		UInterchangePipelineMeshesUtilities* PipelineMeshesUtilities = UInterchangeGenericMeshPipeline::CreateMeshPipelineUtilities(InBaseNodeContainer, MeshPipeline, CommonMeshesProperties->bAutoDetectMeshType);

		TArray<FString> SkeletalMeshes;
		PipelineMeshesUtilities->GetAllSkinnedMeshInstance(SkeletalMeshes);
		if(SkeletalMeshes.Num() == 0)
		{
			PipelineMeshesUtilities->GetAllSkinnedMeshGeometry(SkeletalMeshes);
		}
		TArray<FString> StaticMeshes;
		PipelineMeshesUtilities->GetAllStaticMeshInstance(StaticMeshes);
		if(StaticMeshes.Num() == 0)
		{
			PipelineMeshesUtilities->GetAllStaticMeshGeometry(StaticMeshes);
		}

		int32 RawStaticMesh = 0;
		int32 RawSkeletalMesh = 0;
		int32 RawMorphTargetShape = 0;
		InBaseNodeContainer->IterateNodesOfType<UInterchangeMeshNode>([&RawStaticMesh, &RawSkeletalMesh, &RawMorphTargetShape](const FString& NodeUid, UInterchangeMeshNode* MeshNode)
			{
				if (MeshNode->IsMorphTarget())
				{
					RawMorphTargetShape++;
				}
				else
				{
					MeshNode->IsSkinnedMesh() ? RawSkeletalMesh++ : RawStaticMesh++;
				}
			});

		int32 RawAnimationNode = 0;
		InBaseNodeContainer->IterateNodesOfType<UInterchangeAnimationTrackBaseNode>([&RawAnimationNode](const FString& NodeUid, UInterchangeAnimationTrackBaseNode* AnimationNode)
			{
				RawAnimationNode++;
			});
		InBaseNodeContainer->IterateNodesOfType<UInterchangeAnimationTrackSetNode>([&RawAnimationNode](const FString& NodeUid, UInterchangeAnimationTrackSetNode* AnimationNode)
			{
				RawAnimationNode++;
			});

		UInterchangePipelineBase* OuterMostPipeline = GetMostPipelineOuter();
		auto HideFullCategory = [this, &OuterMostPipeline](const FString& Category, UInterchangePipelineBase* Pipeline)
		{
			TArray<FString> HideCategories;
			//Filter out all Textures properties
			HideCategories.Add(Category);
			if (OuterMostPipeline)
			{
				for (const FString& HideCategoryName : HideCategories)
				{
					HidePropertiesOfCategory(OuterMostPipeline, Pipeline, HideCategoryName);
				}
			}
		};

		auto LocalHideProperty = [this, &OuterMostPipeline](UInterchangePipelineBase* Pipeline, FName PropertyName)
		{
			if (OuterMostPipeline)
			{
				HideProperty(OuterMostPipeline, Pipeline, PropertyName);
			}
		};

		//Found which categories to hide
		bool bHideStaticMeshes = false;
		bool bHideSkeletalMeshes = false;
		bool bHideCommonMeshes = false;
		bool bHideCommonSkeletalMeshesAndAnimations = false;
		bool bHideCommonSkeletalMeshesAndAnimations_StaticMesh = false;
		bool bHideAnimations = false;

		if (RawStaticMesh == 0 || RawMorphTargetShape == 0)
		{
			bHideCommonSkeletalMeshesAndAnimations_StaticMesh = true;
		}

		if (SkeletalMeshes.Num() == 0 && StaticMeshes.Num() == 0)
		{
			bHideStaticMeshes = true;
			bHideSkeletalMeshes = true;
			bHideCommonMeshes = true;
		}
		else if (StaticMeshes.Num() > 0 && SkeletalMeshes.Num() == 0)
		{
			bHideSkeletalMeshes = true;
			bHideCommonSkeletalMeshesAndAnimations = true;
			bHideAnimations = true;
		}
		else if (SkeletalMeshes.Num() > 0 && StaticMeshes.Num() == 0)
		{
			bHideStaticMeshes = true;
		}

		if (SkeletalMeshes.Num() > 0)
		{
			if (MeshPipeline->SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::SkinningWeights)
			{
				LocalHideProperty(CommonMeshesProperties, GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonMeshesProperties, VertexOverrideColor));
				LocalHideProperty(CommonMeshesProperties, GET_MEMBER_NAME_CHECKED(UInterchangeGenericCommonMeshesProperties, VertexColorImportOption));
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, bImportMorphTargets));
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, ThresholdPosition));
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, ThresholdTangentNormal));
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, ThresholdUV));
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, MorphThresholdPosition));
			}
			else if (MeshPipeline->SkeletalMeshImportContentType == EInterchangeSkeletalMeshContentType::Geometry)
			{
				LocalHideProperty(MeshPipeline, GET_MEMBER_NAME_CHECKED(UInterchangeGenericMeshPipeline, bUpdateSkeletonReferencePose));
			}
		}

		if (RawAnimationNode == 0)
		{
			bHideAnimations = true;
		}

		if (bHideAnimations && bHideSkeletalMeshes)
		{
			bHideCommonSkeletalMeshesAndAnimations = true;
		}

		//Hide the categories
		if (bHideStaticMeshes)
		{
			HideFullCategory(TEXT("Static Meshes"), MeshPipeline);
		}
		if (bHideSkeletalMeshes)
		{
			HideFullCategory(TEXT("Skeletal Meshes"), MeshPipeline);
		}
		if(bHideCommonMeshes)
		{
			HideFullCategory(TEXT("Common Meshes"), CommonMeshesProperties);
		}
		if (bHideCommonSkeletalMeshesAndAnimations)
		{
			HideFullCategory(TEXT("Common Skeletal Meshes and Animations"), CommonSkeletalMeshesAndAnimationsProperties);
		}
		if (bHideCommonSkeletalMeshesAndAnimations_StaticMesh)
		{
			HideFullCategory(TEXT("Static Meshes"), CommonSkeletalMeshesAndAnimationsProperties);
		}
		if (bHideAnimations)
		{
			HideFullCategory(TEXT("Animations"), AnimationPipeline);
		}
	}
}

bool UInterchangeGenericAssetsPipeline::IsPropertyChangeNeedRefresh(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if ((CommonMeshesProperties && CommonMeshesProperties->IsPropertyChangeNeedRefresh(PropertyChangedEvent))
		|| (CommonSkeletalMeshesAndAnimationsProperties && CommonSkeletalMeshesAndAnimationsProperties->IsPropertyChangeNeedRefresh(PropertyChangedEvent))
		|| (MeshPipeline && MeshPipeline->IsPropertyChangeNeedRefresh(PropertyChangedEvent))
		|| (MaterialPipeline && MaterialPipeline->IsPropertyChangeNeedRefresh(PropertyChangedEvent))
		|| (AnimationPipeline && AnimationPipeline->IsPropertyChangeNeedRefresh(PropertyChangedEvent)))
	{
		return true;
	}
	return Super::IsPropertyChangeNeedRefresh(PropertyChangedEvent);
}

bool UInterchangeGenericAssetsPipeline::GetPropertyPossibleValues(const FName PropertyPath, TArray<FString>& PossibleValues)
{
	const FString PropertyPathString = PropertyPath.ToString();
	if (MaterialPipeline && PropertyPathString.StartsWith(UInterchangeGenericMaterialPipeline::StaticClass()->GetPathName()))
	{
		if (MaterialPipeline->GetPropertyPossibleValues(PropertyPath, PossibleValues))
		{
			return true;
		}
	}
	
	if (MeshPipeline && PropertyPathString.StartsWith(UInterchangeGenericMeshPipeline::StaticClass()->GetPathName()))
	{
		if (MeshPipeline->GetPropertyPossibleValues(PropertyPath, PossibleValues))
		{
			return true;
		}
	}
	
	if (AnimationPipeline && PropertyPathString.StartsWith(UInterchangeGenericAnimationPipeline::StaticClass()->GetPathName()))
	{
		if (AnimationPipeline->GetPropertyPossibleValues(PropertyPath, PossibleValues))
		{
			return true;
		}
	}

	//If we did not find any property call the super implementation
	return Super::GetPropertyPossibleValues(PropertyPath, PossibleValues);
}

#endif //WITH_EDITOR

void UInterchangeGenericAssetsPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas)
{
	check(CommonSkeletalMeshesAndAnimationsProperties);

	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAssetsPipeline: Cannot execute pre-import pipeline because InBaseNodeContrainer is null"));
		return;
	}

	//Set the result container to allow error message
	//The parent Results container should be set at this point
	ensure(Results);
	{
		if (MaterialPipeline)
		{
			MaterialPipeline->SetResultsContainer(Results);
		}
		if (MeshPipeline)
		{
			MeshPipeline->SetResultsContainer(Results);
		}
		if (AnimationPipeline)
		{
			AnimationPipeline->SetResultsContainer(Results);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//Make sure all options go together
	
	//When we import only animation we need to prevent material and physic asset to be created
	if (CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations)
	{
		MaterialPipeline->bImportMaterials = false;
		MeshPipeline->bImportStaticMeshes = false;
		MeshPipeline->bCreatePhysicsAsset = false;
		MeshPipeline->PhysicsAsset = nullptr;
		MaterialPipeline->TexturePipeline->bImportTextures = false;
	}

	//////////////////////////////////////////////////////////////////////////


	BaseNodeContainer = InBaseNodeContainer;

	//Setup the Global import offset
	{
		//Make sure the scale value is greater than zero, warn the user in this case and set the scale to the default value 1.0f
		if (ImportOffsetUniformScale < UE_SMALL_NUMBER)
		{
			FNumberFormattingOptions FormatingOptions;
			FormatingOptions.SetMaximumFractionalDigits(6);
			FormatingOptions.SetMinimumFractionalDigits(1);
			float DefaultScaleValue = 1.0f;
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericAssetsPipeline", "BadImportOffsetUniformScale", "Value [{0}] for ImportOffsetUniformScale setting is too small, we will use the default value [{1}]."),
				FText::AsNumber(ImportOffsetUniformScale, &FormatingOptions),
				FText::AsNumber(DefaultScaleValue, &FormatingOptions));
			ImportOffsetUniformScale = DefaultScaleValue;
		}

		FTransform ImportOffsetTransform;
		ImportOffsetTransform.SetTranslation(ImportOffsetTranslation);
		ImportOffsetTransform.SetRotation(FQuat(ImportOffsetRotation));
		ImportOffsetTransform.SetScale3D(FVector(ImportOffsetUniformScale));

		UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::FindOrCreateUniqueInstance(BaseNodeContainer);
		CommonPipelineDataFactoryNode->SetCustomGlobalOffsetTransform(BaseNodeContainer, ImportOffsetTransform);

		// In case all mesh types are forced to Static/Skeletal we bake the scene instance hierarchy transforms
		CommonPipelineDataFactoryNode->SetBakeMeshes(BaseNodeContainer, CommonMeshesProperties->ForceAllMeshAsType != EInterchangeForceMeshType::IFMT_None || CommonMeshesProperties->bBakeMeshes);
	}

	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}

	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePipeline(InBaseNodeContainer, InSourceDatas);
	}
	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePipeline(InBaseNodeContainer, InSourceDatas);
	}
	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePipeline(InBaseNodeContainer, InSourceDatas);
	}

	ImplementUseSourceNameForAssetOption();
	//Make sure all factory nodes have the specified strategy
	BaseNodeContainer->IterateNodesOfType<UInterchangeFactoryBaseNode>([ReimportStrategyClosure = ReimportStrategy](const FString& NodeUid, UInterchangeFactoryBaseNode* FactoryNode)
		{
			FactoryNode->SetReimportStrategyFlags(ReimportStrategyClosure);
		});
}

void UInterchangeGenericAssetsPipeline::ExecutePostFactoryPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePostFactoryPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePostFactoryPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePostFactoryPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
}

void UInterchangeGenericAssetsPipeline::ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport)
{
	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (MeshPipeline)
	{
		MeshPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}
	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedExecutePostImportPipeline(InBaseNodeContainer, NodeKey, CreatedAsset, bIsAReimport);
	}

#if WITH_EDITORONLY_DATA
	AddPackageMetaData(CreatedAsset, InBaseNodeContainer->GetNode(NodeKey));
#endif
}

#if WITH_EDITORONLY_DATA
void UInterchangeGenericAssetsPipeline::AddPackageMetaData(UObject* CreatedAsset, const UInterchangeBaseNode* Node)
{
	if (!CreatedAsset || !Node)
	{
		return;
	}

	const FString InterchangeMetaDataPrefix = TEXT("INTERCHANGE.");

	//Add UObject package meta data
	if (UMetaData* MetaData = CreatedAsset->GetOutermost()->GetMetaData())
	{
		//Cleanup existing INTERCHANGE_ prefix metadata name for this object (in case we re-import)
		{
			TArray<FName> InterchangeMetaDataKeys;
			if(TMap<FName, FString>* MetaDataMapPtr = MetaData->GetMapForObject(CreatedAsset))
			{
				for (const TPair<FName, FString>& ObjectMetadata : *MetaDataMapPtr)
				{
					if (ObjectMetadata.Key.ToString().StartsWith(InterchangeMetaDataPrefix))
					{
						InterchangeMetaDataKeys.Add(ObjectMetadata.Key);
					}
				}
				for (const FName& MetaDataKey : InterchangeMetaDataKeys)
				{
					MetaData->RemoveValue(CreatedAsset, MetaDataKey);
				}
			}
		}
		TArray<FInterchangeUserDefinedAttributeInfo> UserAttributeInfos;
		UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(Node, UserAttributeInfos);
		//We must convert all different type to String since meta data only support string
		for (const FInterchangeUserDefinedAttributeInfo& UserAttributeInfo : UserAttributeInfos)
		{
			if (UserAttributeInfo.PayloadKey.IsSet())
			{
				//Skip animated attributes
				continue;
			}
			TOptional<FString> MetaDataValue;
			TOptional<FString> PayloadKey;
			switch (UserAttributeInfo.Type)
			{
				case UE::Interchange::EAttributeTypes::Bool:
				{
					bool Value = false;
					if(UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int8:
				{
					int8 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int16:
				{
					int16 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int32:
				{
					int32 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Int64:
				{
					int64 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt8:
				{
					uint8 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt16:
				{
					uint16 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt32:
				{
					uint32 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::UInt64:
				{
					uint64 Value = 0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Float:
				{
					float Value = 0.0f;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Float16:
				{
					FFloat16 Value = 0.0f;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector2f:
				{
					FVector2f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector3f:
				{
					FVector3f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector4f:
				{
					FVector4f Value(0.0f);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Double:
				{
					double Value = 0.0;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector2d:
				{
					FVector2D Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector3d:
				{
					FVector3d Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::Vector4d:
				{
					FVector4d Value(0.0);
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
				case UE::Interchange::EAttributeTypes::String:
				{
					FString Value;
					if (UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(Node, UserAttributeInfo.Name, Value, PayloadKey))
					{
						MetaDataValue = UE::Interchange::AttributeValueToString(Value);
					}
				}
				break;
			}
			if (MetaDataValue.IsSet())
			{
				const FString& MetaDataStringValue = MetaDataValue.GetValue();
				const FName& MetaDataKey = FName(InterchangeMetaDataPrefix + UserAttributeInfo.Name);
				//SetValue either add the key or set the new value
				MetaData->SetValue(CreatedAsset, MetaDataKey, *MetaDataStringValue);
			}
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void UInterchangeGenericAssetsPipeline::SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex)
{
	if (MaterialPipeline)
	{
		MaterialPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}

	if (MeshPipeline)
	{
		MeshPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}

	if (AnimationPipeline)
	{
		AnimationPipeline->ScriptedSetReimportSourceIndex(ReimportObjectClass, SourceFileIndex);
	}
}

void UInterchangeGenericAssetsPipeline::ImplementUseSourceNameForAssetOption()
{
	const UClass* SkeletalMeshFactoryNodeClass = UInterchangeSkeletalMeshFactoryNode::StaticClass();
	TArray<FString> SkeletalMeshNodeUids;
	BaseNodeContainer->GetNodes(SkeletalMeshFactoryNodeClass, SkeletalMeshNodeUids);

	const UClass* StaticMeshFactoryNodeClass = UInterchangeStaticMeshFactoryNode::StaticClass();
	TArray<FString> StaticMeshNodeUids;
	BaseNodeContainer->GetNodes(StaticMeshFactoryNodeClass, StaticMeshNodeUids);

	const UClass* AnimSequenceFactoryNodeClass = UInterchangeAnimSequenceFactoryNode::StaticClass();
	TArray<FString> AnimSequenceNodeUids;
	BaseNodeContainer->GetNodes(AnimSequenceFactoryNodeClass, AnimSequenceNodeUids);

	//If we import only one mesh, we want to rename the mesh using the file name.
	const int32 MeshesImportedNodeCount = SkeletalMeshNodeUids.Num() + StaticMeshNodeUids.Num();

	FString OverrideAssetName = IsStandAlonePipeline() ? DestinationName : FString();
	if(OverrideAssetName.IsEmpty() && IsStandAlonePipeline())
	{
		OverrideAssetName = AssetName;
	}

	//SkeletalMesh it must always be run even if there is no rename option, skeleton and physics asset will be rename properly
	MeshPipeline->ImplementUseSourceNameForAssetOptionSkeletalMesh(MeshesImportedNodeCount, bUseSourceNameForAsset, OverrideAssetName);

	if (bUseSourceNameForAsset || !OverrideAssetName.IsEmpty())
	{
		//StaticMesh
		if (MeshesImportedNodeCount == 1 && StaticMeshNodeUids.Num() > 0)
		{
			UInterchangeStaticMeshFactoryNode* StaticMeshNode = Cast<UInterchangeStaticMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(StaticMeshNodeUids[0]));
			const FString DisplayLabelName = OverrideAssetName.IsEmpty() ? FPaths::GetBaseFilename(SourceDatas[0]->GetFilename()) : OverrideAssetName;
			StaticMeshNode->SetDisplayLabel(DisplayLabelName);
		}

		//Animation, simply look if we import only 1 animation before applying the option to animation
		if (AnimSequenceNodeUids.Num() == 1)
		{
			UInterchangeAnimSequenceFactoryNode* AnimSequenceNode = Cast<UInterchangeAnimSequenceFactoryNode>(BaseNodeContainer->GetFactoryNode(AnimSequenceNodeUids[0]));
			const FString DisplayLabelName = (OverrideAssetName.IsEmpty() ? FPaths::GetBaseFilename(SourceDatas[0]->GetFilename()) : OverrideAssetName) + TEXT("_Anim");
			AnimSequenceNode->SetDisplayLabel(DisplayLabelName);
		}
	}
}

