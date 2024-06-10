// Copyright Epic Games, Inc. All Rights Reserved.

#include "Usd/InterchangeUsdTranslator.h"

#include "UnrealUSDWrapper.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdGeomXformable.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdTyped.h"
#include "UsdWrappers/VtValue.h"

#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDLog.h"
#include "USDMaterialUtils.h"
#include "USDPrimConversion.h"
#include "USDShadeConversion.h"
#include "USDStageOptions.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"

#include "Async/Async.h"
#include "HAL/IConsoleManager.h"
#include "InterchangeCameraNode.h"
#include "InterchangeImportLog.h"
#include "InterchangeLightNode.h"
#include "InterchangeMaterialInstanceNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTranslatorHelper.h"
#include "Internationalization/Regex.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UDIMUtilities.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdPhysics/tokens.h"
#include "pxr/usd/usdShade/tokens.h"
#include "USDIncludesEnd.h"

#endif	  // USE_USD_SDK

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeUsdTranslator)

#define LOCTEXT_NAMESPACE "InterchangeUSDTranslator"

static bool GInterchangeEnableUSDImport = false;
static FAutoConsoleVariableRef CVarInterchangeEnableUSDImport(
	TEXT("Interchange.FeatureFlags.Import.USD"),
	GInterchangeEnableUSDImport,
	TEXT("Whether USD support is enabled.")
);

namespace UE::InterchangeUsdTranslator::Private
{
	const static FString CameraPrefix = TEXT("\\Camera\\");
	const static FString LightPrefix = TEXT("\\Light\\");
	const static FString MaterialPrefix = TEXT("\\Material\\");
	const static FString MeshPrefix = TEXT("\\Mesh\\");

	// Small container that we can use Pimpl with so we don't have to include too many USD includes on the header file.
	//
	// It also skirts around a small complication where UInterchangeUSDTranslator::Translate is const, and yet we must
	// keep and modify some members (like UsdStage) for when the payload functions get called later... The other translators
	// use mutable or const casts, but with the Impl we don't need to!
	class UInterchangeUSDTranslatorImpl
	{
	public:
		// We have to keep a stage reference so that we can parse the payloads after Translate() complete.
		// ReleaseSource() clears this member, once translation is complete.
		UE::FUsdStage UsdStage;

		TObjectPtr<UInterchangeUsdTranslatorSettings> TranslatorSettings = nullptr;

#if USE_USD_SDK
		// On UInterchangeUSDTranslator::Translate we set this up based on our TranslatorSettings, and then
		// we can reuse it (otherwise we have to keep converting the FNames into Tokens all the time)
		UsdToUnreal::FUsdMeshConversionOptions CachedMeshConversionOptions;
#endif
	};
}

UInterchangeUsdTranslatorSettings::UInterchangeUsdTranslatorSettings()
	: GeometryPurpose((int32)(EUsdPurpose::Default | EUsdPurpose::Proxy | EUsdPurpose::Render | EUsdPurpose::Guide))
	, RenderContext(TEXT("unreal"))	   // The proper definition of this is on the USDSchemas module, which we can't depend on
	, MaterialPurpose(*UnrealIdentifiers::MaterialPreviewPurpose)
	, bOverrideStageOptions(false)
	, StageOptions{
		  0.01f,			   // MetersPerUnit
		  EUsdUpAxis::ZAxis	   // UpAxis
	  }
{
}

UInterchangeUSDTranslator::UInterchangeUSDTranslator()
	: Impl(MakeUnique<UE::InterchangeUsdTranslator::Private::UInterchangeUSDTranslatorImpl>())
{
}

EInterchangeTranslatorType UInterchangeUSDTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Scenes;
}

EInterchangeTranslatorAssetType UInterchangeUSDTranslator::GetSupportedAssetTypes() const
{
	return EInterchangeTranslatorAssetType::Materials | EInterchangeTranslatorAssetType::Meshes | EInterchangeTranslatorAssetType::Animations;
}

TArray<FString> UInterchangeUSDTranslator::GetSupportedFormats() const
{
	TArray<FString> Extensions;
	if (GInterchangeEnableUSDImport)
	{
		UnrealUSDWrapper::AddUsdImportFileFormatDescriptions(Extensions);
	}
	return Extensions;
}

namespace UE::InterchangeUsdTranslator::Private
{
#if USE_USD_SDK
	FString EncodeTexturePayloadKey(const UsdToUnreal::FTextureParameterValue& Value)
	{
		// Encode the compression settings onto the payload key as we need to move that into the
		// payload data within UInterchangeUSDTranslator::GetTexturePayloadData.
		//
		// This should be a temporary thing, and in the future we'll be able to store compression
		// settings directly on the texture translated node
		return Value.TextureFilePath + TEXT("\\") + LexToString((int32)Value.Group);
	}

	bool DecodeTexturePayloadKey(const FString& PayloadKey, FString& OutTextureFilePath, TextureGroup& OutTextureGroup)
	{
		// Use split from end here so that we ignore any backslashes within the file path itself
		FString FilePath;
		FString TextureGroupStr;
		bool bSplit = PayloadKey.Split(TEXT("\\"), &FilePath, &TextureGroupStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (!bSplit)
		{
			return false;
		}

		OutTextureFilePath = FilePath;

		int32 TempInt;
		if (LexTryParseString<int32>(TempInt, *TextureGroupStr))
		{
			OutTextureGroup = (TextureGroup)TempInt;
		}

		return true;
	}

	void AddTextureNode(
		const UE::FUsdPrim& Prim,
		const FString& NodeUid,
		const UsdToUnreal::FTextureParameterValue& Value,
		UInterchangeBaseNodeContainer& NodeContainer
	)
	{
		FString PrimPath = Prim.GetPrimPath().GetString();
		FString NodeName{FPaths::GetCleanFilename(Value.TextureFilePath)};

		// Check if Node already exist with this ID
		if (const UInterchangeTexture2DNode* Node = Cast<UInterchangeTexture2DNode>(NodeContainer.GetNode(NodeUid)))
		{
			return;
		}

		UInterchangeTexture2DNode* Node = NewObject<UInterchangeTexture2DNode>(&NodeContainer);
		Node->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		Node->SetPayLoadKey(EncodeTexturePayloadKey(Value));

		static_assert((int)TextureAddress::TA_Wrap == (int)EInterchangeTextureWrapMode::Wrap);
		static_assert((int)TextureAddress::TA_Clamp == (int)EInterchangeTextureWrapMode::Clamp);
		static_assert((int)TextureAddress::TA_Mirror == (int)EInterchangeTextureWrapMode::Mirror);
		Node->SetCustomWrapU((EInterchangeTextureWrapMode)Value.AddressX);
		Node->SetCustomWrapV((EInterchangeTextureWrapMode)Value.AddressY);

		Node->SetCustomSRGB(Value.GetSRGBValue());

		// Provide the other UDIM tiles
		//
		// Note: There is an bImportUDIM option on UInterchangeGenericTexturePipeline that is exclusively used within
		// UInterchangeGenericTexturePipeline::HandleCreationOfTextureFactoryNode in order to essentially do the exact same
		// thing as we do here. In theory, we shouldn't need to do this then, and in fact it is a bit bad to do so because
		// we will always parse these UDIMs whether the option is enabled or disabled. The issue however is that (as of the
		// time of this writing) UInterchangeGenericTexturePipeline::HandleCreationOfTextureFactoryNode is hard-coded to expect
		// the texture payload key to be just the texture file path. We can't do that, because we need to also encode
		// the texture compression settings onto payload key...
		//
		// All of that is to say that everything will actually work fine, but if you uncheck "bImportUDIM" on the import options
		// you will still get UDIMs (for now).
		if (Value.bIsUDIM)
		{
			TMap<int32, FString> TileIndexToPath = UE::TextureUtilitiesCommon::GetUDIMBlocksFromSourceFile(
				Value.TextureFilePath,
				UE::TextureUtilitiesCommon::DefaultUdimRegexPattern
			);
			Node->SetSourceBlocks(MoveTemp(TileIndexToPath));
		}

		NodeContainer.AddNode(Node);
	}

	// We use this visitor to set UsdToUnreal::FParameterValue TVariant values onto UInterchangeMaterialInstanceNodes
	struct FParameterValueVisitor
	{
		FParameterValueVisitor(
			const UE::FUsdPrim& InPrim,
			UInterchangeBaseNodeContainer& InNodeContainer,
			UInterchangeMaterialInstanceNode& InMaterialNode,
			const TMap<FString, int32>& InPrimvarToUVIndex
		)
			: Prim(InPrim)
			, NodeContainer(InNodeContainer)
			, MaterialNode(InMaterialNode)
			, PrimvarToUVIndex(InPrimvarToUVIndex)
		{
		}

		void operator()(const float Value) const
		{
			MaterialNode.AddScalarParameterValue(*ParameterName, Value);

			// Disable the texture input since we have a direct value
			MaterialNode.AddScalarParameterValue(*FString::Printf(TEXT("Use%sTexture"), **ParameterName), 0.0f);
		}

		void operator()(const FVector& Value) const
		{
			MaterialNode.AddVectorParameterValue(*ParameterName, FLinearColor{Value});

			// Disable the texture input since we have a direct value
			MaterialNode.AddScalarParameterValue(*FString::Printf(TEXT("Use%sTexture"), **ParameterName), 0.0f);
		}

		void operator()(const UsdToUnreal::FTextureParameterValue& Value) const
		{
			// Emit texture node itself (this is the main place where this happens)
			// Note that the node name isn't just the texture path, as we may have multiple material users of this texture
			// with different settings, and so we need separate translated nodes for each material and parameter
			FString TextureUid = FString::Printf(TEXT("Texture:%s:%s"), *Prim.GetPrimPath().GetString(), **ParameterName);
			AddTextureNode(Prim, TextureUid, Value, NodeContainer);

			// Actual texture assignment
			MaterialNode.AddTextureParameterValue(*FString::Printf(TEXT("%sTexture"), **ParameterName), TextureUid);
			MaterialNode.AddScalarParameterValue(*FString::Printf(TEXT("Use%sTexture"), **ParameterName), 1.0f);

			// UV transform
			FLinearColor ScaleAndTranslation = FLinearColor{
				Value.UVScale.GetVector()[0],
				Value.UVScale.GetVector()[1],
				Value.UVTranslation[0],
				Value.UVTranslation[1]};
			MaterialNode.AddVectorParameterValue(*FString::Printf(TEXT("%sScaleTranslation"), **ParameterName), ScaleAndTranslation);
			MaterialNode.AddScalarParameterValue(*FString::Printf(TEXT("%sRotation"), **ParameterName), Value.UVRotation);

			// UV index
			if (const int32* FoundIndex = PrimvarToUVIndex.Find(Value.Primvar))
			{
				MaterialNode.AddScalarParameterValue(*FString::Printf(TEXT("%sUVIndex"), **ParameterName), *FoundIndex);
			}
			else
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to find primvar '%s' when setting material parameter '%s' on material '%s'. Available primvars and UV "
						 "indices: %s.%s"),
					*Value.Primvar,
					**ParameterName,
					*Prim.GetPrimPath().GetString(),
					*UsdUtils::StringifyMap(PrimvarToUVIndex),
					Value.Primvar.IsEmpty() ? TEXT(
						" Is your UsdUVTexture Shader missing the 'inputs:st' attribute? (It specifies which UV set to sample the texture with)"
					)
											: TEXT("")
				);
			}

			// Component mask (which channel of the texture to use)
			FLinearColor ComponentMask = FLinearColor::Black;
			switch (Value.OutputIndex)
			{
				case 0:	   // RGB
					ComponentMask = FLinearColor{1.f, 1.f, 1.f, 0.f};
					break;
				case 1:	   // R
					ComponentMask = FLinearColor{1.f, 0.f, 0.f, 0.f};
					break;
				case 2:	   // G
					ComponentMask = FLinearColor{0.f, 1.f, 0.f, 0.f};
					break;
				case 3:	   // B
					ComponentMask = FLinearColor{0.f, 0.f, 1.f, 0.f};
					break;
				case 4:	   // A
					ComponentMask = FLinearColor{0.f, 0.f, 0.f, 1.f};
					break;
			}
			MaterialNode.AddVectorParameterValue(*FString::Printf(TEXT("%sTextureComponent"), **ParameterName), ComponentMask);
		}

		void operator()(const UsdToUnreal::FPrimvarReaderParameterValue& Value) const
		{
			MaterialNode.AddVectorParameterValue(*ParameterName, FLinearColor{Value.FallbackValue});

			if (Value.PrimvarName == TEXT("displayColor"))
			{
				MaterialNode.AddScalarParameterValue(TEXT("UseVertexColorForBaseColor"), 1.0f);
			}
		}

		void operator()(const bool Value) const
		{
			MaterialNode.AddScalarParameterValue(*ParameterName, static_cast<float>(Value));
		}

	public:
		const UE::FUsdPrim& Prim;
		UInterchangeBaseNodeContainer& NodeContainer;
		UInterchangeMaterialInstanceNode& MaterialNode;
		const TMap<FString, int32>& PrimvarToUVIndex;
		const FString* ParameterName = nullptr;
	};

	void AddMaterialInstanceNode(
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl* TranslatorImpl,
		UInterchangeBaseNodeContainer& NodeContainer
	)
	{
		FString PrimPath = Prim.GetPrimPath().GetString();
		FString NodeUid = MaterialPrefix + PrimPath;
		FString NodeName(Prim.GetName().ToString());

		// Check if Node already exist with this ID
		if (const UInterchangeMaterialInstanceNode* Node = Cast<UInterchangeMaterialInstanceNode>(NodeContainer.GetNode(NodeUid)))
		{
			return;
		}

		UInterchangeMaterialInstanceNode* MaterialNode = NewObject<UInterchangeMaterialInstanceNode>(&NodeContainer);
		MaterialNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		MaterialNode->SetAssetName(NodeName);
		NodeContainer.AddNode(MaterialNode);

		UsdToUnreal::FUsdPreviewSurfaceMaterialData MaterialData;
		FString RenderContext = TranslatorImpl->TranslatorSettings ? TranslatorImpl->TranslatorSettings->RenderContext.ToString() : FString();
		const bool bSuccess = UsdToUnreal::ConvertMaterial(Prim, MaterialData, TranslatorImpl->TranslatorSettings ? *RenderContext : nullptr);

		// Set all the parameter values to the interchange node
		bool bHasUDIMTexture = false;
		FParameterValueVisitor Visitor{Prim, NodeContainer, *MaterialNode, MaterialData.PrimvarToUVIndex};
		for (TPair<FString, UsdToUnreal::FParameterValue>& Pair : MaterialData.Parameters)
		{
			Visitor.ParameterName = &Pair.Key;
			Visit(Visitor, Pair.Value);

			// Also simultaneously check if any of these parameters wants to be an UDIM texture so that we can use the VT reference material later
			if (!bHasUDIMTexture)
			{
				if (UsdToUnreal::FTextureParameterValue* TextureParameter = Pair.Value.TryGet<UsdToUnreal::FTextureParameterValue>())
				{
					if (TextureParameter->bIsUDIM)
					{
						bHasUDIMTexture = true;
					}
				}
			}
		}

		// Find and set the right reference material
		//
		// TODO: Proper VT texture support (we'd need to know the texture resolution at this point, and we haven't parsed them yet...).
		// The way it currently works on Interchange is that the factory will create a VT or nonVT version of the texture to match the
		// material parameter slot. Since we'll currently never set the VT reference material, it essentially means it will always
		// downgrade our VT textures to non-VT.
		// The only exception is how we upgrade the reference material to VT in case we have any UDIM textures, as those are trivial to
		// check for (we don't have to actually load the textures to do it)
		EUsdReferenceMaterialProperties Properties = EUsdReferenceMaterialProperties::None;
		if (UsdUtils::IsMaterialTranslucent(MaterialData))
		{
			Properties |= EUsdReferenceMaterialProperties::Translucent;
		}
		if (bHasUDIMTexture)
		{
			Properties |= EUsdReferenceMaterialProperties::VT;
		}

		FSoftObjectPath ReferenceMaterial = UsdUnreal::MaterialUtils::GetReferencePreviewSurfaceMaterial(Properties);
		MaterialNode->SetCustomParent(ReferenceMaterial.ToString());
	}

	void AddDisplayColorMaterialInstanceNodeIfNeeded(UInterchangeBaseNodeContainer& NodeContainer, const FString& DisplayColorDesc)
	{
		using namespace UsdUnreal::MaterialUtils;

		FString NodeUid = MaterialPrefix + DisplayColorDesc;

		// We'll treat the DisplayColorDesc (something like "!DisplayColor_1_0") as the material instance UID here
		const UInterchangeMaterialInstanceNode* Node = Cast<UInterchangeMaterialInstanceNode>(NodeContainer.GetNode(DisplayColorDesc));
		if (Node)
		{
			return;
		}

		// Need to create a new instance
		TOptional<FDisplayColorMaterial> ParsedMat = FDisplayColorMaterial::FromString(DisplayColorDesc);
		if (!ParsedMat)
		{
			return;
		}
		FString NodeName = ParsedMat->ToPrettyString();

		const FSoftObjectPath* ReferenceMaterialPath = GetReferenceMaterialPath(ParsedMat.GetValue());
		if (!ReferenceMaterialPath)
		{
			return;
		}

		// Not needed
		const FString ParentNodeUid;
		UInterchangeMaterialInstanceNode* NewNode = UInterchangeMaterialInstanceNode::Create(&NodeContainer, DisplayColorDesc, ParentNodeUid);
		NewNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);

		NewNode->SetCustomParent(ReferenceMaterialPath->GetAssetPathString());
	}

	void AddLightNode(const UE::FUsdPrim& Prim, UInterchangeBaseNodeContainer& NodeContainer)
	{
		FString NodeUid = LightPrefix + Prim.GetPrimPath().GetString();
		FString NodeName(Prim.GetName().ToString());

		// #ueent_todo: Retrieve light attributes and set them below

		if (Prim.IsA(TEXT("DistantLight")))
		{
			UInterchangeDirectionalLightNode* LightNode = NewObject<UInterchangeDirectionalLightNode>(&NodeContainer);
			LightNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
			LightNode->SetAssetName(NodeName);

			// LightNode->SetCustomLightColor(Color);
			// LightNode->SetCustomIntensity(Intensity);

			NodeContainer.AddNode(LightNode);
		}
		else if (Prim.IsA(TEXT("SphereLight")))
		{
			if (Prim.HasAPI(TEXT("ShapingAPI")))
			{
				UInterchangeSpotLightNode* LightNode = NewObject<UInterchangeSpotLightNode>(&NodeContainer);
				LightNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
				LightNode->SetAssetName(NodeName);

				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Lumens);
				// LightNode->SetCustomLightColor(Color));
				// LightNode->SetCustomIntensity(Intensity);

				// LightNode->SetCustomInnerConeAngle(FMath::RadiansToDegrees(InnerConeAngle));
				// LightNode->SetCustomOuterConeAngle(FMath::RadiansToDegrees(OuterConeAngle));

				NodeContainer.AddNode(LightNode);
			}
			else
			{
				UInterchangePointLightNode* LightNode = NewObject<UInterchangePointLightNode>(&NodeContainer);
				LightNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
				LightNode->SetAssetName(NodeName);

				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Lumens);
				// LightNode->SetCustomLightColor(FLinearColor(Color));
				// LightNode->SetCustomIntensity(Intensity);

				// LightNode->SetCustomAttenuationRadius(AttenuationRadius);

				NodeContainer.AddNode(LightNode);
			}
		}
		else if (Prim.IsA(TEXT("RectLight")) || Prim.IsA(TEXT("DiskLight")))
		{
			UInterchangeRectLightNode* LightNode = NewObject<UInterchangeRectLightNode>(&NodeContainer);
			LightNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
			LightNode->SetAssetName(NodeName);
			NodeContainer.AddNode(LightNode);
		}
		// #ueent_todo:
		// DomeLight -> SkyLight
	}

	void AddCameraNode(const UE::FUsdPrim& Prim, UInterchangeBaseNodeContainer& NodeContainer)
	{
		// FUsdCamera Camera;
		// if (!UsdToUnreal::ConvertGeomCamera(Prim, Camera, UsdUtils::GetEarliestTimeCode()))
		//{
		//	return;
		// }

		FString NodeUid = CameraPrefix + Prim.GetPrimPath().GetString();
		FString NodeName(Prim.GetName().ToString());

		UInterchangePhysicalCameraNode* CameraNode = NewObject<UInterchangePhysicalCameraNode>(&NodeContainer);
		CameraNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		NodeContainer.AddNode(CameraNode);

		// CameraNode->SetCustomFocalLength(Camera.FocalLength);
		// CameraNode->SetCustomSensorHeight(Camera.SensorHeight);
		// CameraNode->SetCustomSensorWidth(Camera.SensorWidth);
	}

	void AddMeshNode(const UE::FUsdPrim& Prim, UInterchangeUSDTranslatorImpl* TranslatorImpl, UInterchangeBaseNodeContainer& NodeContainer)
	{
		FString PrimPath = Prim.GetPrimPath().GetString();
		FString NodeUid = MeshPrefix + PrimPath;
		FString NodeName(Prim.GetName().ToString());

		// Check if Node already exist with this ID
		if (const UInterchangeMeshNode* Node = Cast<UInterchangeMeshNode>(NodeContainer.GetNode(NodeUid)))
		{
			return;
		}

		UInterchangeMeshNode* MeshNode = NewObject<UInterchangeMeshNode>(&NodeContainer);
		MeshNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		MeshNode->SetPayLoadKey(PrimPath, EInterchangeMeshPayLoadType::STATIC);
		MeshNode->SetAssetName(NodeName);

		// Material assignments
		{
			const double TimeCode = UsdUtils::GetDefaultTimeCode();
			const bool bProvideMaterialIndices = false;
			UsdUtils::FUsdPrimMaterialAssignmentInfo Assignments = UsdUtils::GetPrimMaterialAssignments(
				Prim,
				TimeCode,
				bProvideMaterialIndices,
				TranslatorImpl->CachedMeshConversionOptions.RenderContext,
				TranslatorImpl->CachedMeshConversionOptions.MaterialPurpose
			);

			for (const UsdUtils::FUsdPrimMaterialSlot& Slot : Assignments.Slots)
			{
				const FString& SlotName = Slot.SlotName;

				// Get the Uid of the material instance that we'll end up assigning to this slot
				FString MaterialInstanceUid;
				switch (Slot.AssignmentType)
				{
					case UsdUtils::EPrimAssignmentType::DisplayColor:
					{
						AddDisplayColorMaterialInstanceNodeIfNeeded(NodeContainer, Slot.MaterialSource);
						MaterialInstanceUid = MaterialPrefix + Slot.MaterialSource;	   // This is e.g. "!DisplayColor_0_1"
						break;
					}
					case UsdUtils::EPrimAssignmentType::MaterialPrim:
					{
						MaterialInstanceUid = MaterialPrefix + Slot.MaterialSource;	   // This is the prim path
						break;
					}
					case UsdUtils::EPrimAssignmentType::UnrealMaterial:
					{
						// TODO: We can't support these yet without a custom pipeline unfortunately
						// We could spawn a material instance of the referenced material... That's probably not what you'd expect though
						break;
					}
					default:
					{
						ensure(false);
						break;
					}
				}

				MeshNode->SetSlotMaterialDependencyUid(SlotName, MaterialInstanceUid);
			}
		}

		NodeContainer.AddNode(MeshNode);
	}

	void Traverse(
		const UE::FUsdStage& UsdStage,
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl* TranslatorImpl,
		UInterchangeBaseNodeContainer& NodeContainer,
		UInterchangeBaseNode* ParentNode
	)
	{
		// Ignore prim subtrees from disabled purposes
		// TODO: Move this to the pipeline and filter only the factory nodes
		EUsdPurpose PrimPurpose = IUsdPrim::GetPurpose(Prim);
		if (!EnumHasAllFlags(TranslatorImpl->CachedMeshConversionOptions.PurposesToLoad, PrimPurpose))
		{
			return;
		}

		FString NodeUid = Prim.GetPrimPath().GetString();
		FString DisplayLabel(Prim.GetName().ToString());

		// Generate asset node if applicable
		const FString* Prefix = nullptr;
		if (Prim.IsA(TEXT("Material")))
		{
			Prefix = &MaterialPrefix;
			AddMaterialInstanceNode(Prim, TranslatorImpl, NodeContainer);
		}
		else if (Prim.IsA(TEXT("Mesh")))
		{
			Prefix = &MeshPrefix;
			AddMeshNode(Prim, TranslatorImpl, NodeContainer);
		}
		else if (Prim.IsA(TEXT("Camera")))
		{
			Prefix = &CameraPrefix;
			AddCameraNode(Prim, NodeContainer);
		}
		else if (Prim.HasAPI(TEXT("LightAPI")))
		{
			Prefix = &LightPrefix;
			AddLightNode(Prim, NodeContainer);
		}

		// Generate scene node if we're an Xformable
		UInterchangeSceneNode* SceneNode = nullptr;
		FTransform Transform = FTransform::Identity;
		if (UsdToUnreal::ConvertXformable(UsdStage, UE::FUsdTyped(Prim), Transform, UsdUtils::GetDefaultTimeCode(), nullptr))
		{
			SceneNode = NewObject<UInterchangeSceneNode>(&NodeContainer);
			SceneNode->InitializeNode(NodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedScene);
			SceneNode->SetCustomLocalTransform(&NodeContainer, Transform);
			NodeContainer.AddNode(SceneNode);

			// Connect scene node and asset node
			if (Prefix)
			{
				const FString AssetNodeUid = *Prefix + NodeUid;
				SceneNode->SetCustomAssetInstanceUid(AssetNodeUid);
			}

			// Connect parent and child scene nodes
			if (ParentNode)
			{
				NodeContainer.SetNodeParentUid(SceneNode->GetUniqueID(), ParentNode->GetUniqueID());
			}
		}

		// Recurse into child prims
		for (const FUsdPrim& ChildPrim : Prim.GetChildren())
		{
			Traverse(UsdStage, ChildPrim, TranslatorImpl, NodeContainer, SceneNode);
		}
	}

	bool GetStaticMeshPayloadDataForPayLoadKey(
		const UE::FUsdStage& UsdStage,
		const FString& PrimPath,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		FMeshDescription& OutMeshDescription
	)
	{
		UE::FUsdPrim Prim = UsdStage.GetPrimAtPath(UE::FSdfPath{*PrimPath});
		if (!Prim)
		{
			return false;
		}

		// TODO: We can't do much with these yet: There will be used to generate primvar-compatible
		// versions of the materials that are assigned to this mesh, whenever we get a pipeline
		UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;
		return UsdToUnreal::ConvertGeomMesh(Prim, OutMeshDescription, TempMaterialInfo, Options);
	}

#endif
}	 // namespace UE::InterchangeUsdTranslator::Private

bool UInterchangeUSDTranslator::Translate(UInterchangeBaseNodeContainer& NodeContainer) const
{
#if USE_USD_SDK
	using namespace UE;
	using namespace UE::InterchangeUsdTranslator::Private;
	using namespace UsdToUnreal;

	UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return false;
	}

	UInterchangeUsdTranslatorSettings* Settings = Cast<UInterchangeUsdTranslatorSettings>(GetSettings());
	if (!Settings)
	{
		return false;
	}

	FString FilePath = GetSourceData()->GetFilename();
	if (!FPaths::FileExists(FilePath))
	{
		return false;
	}

	// Import should always feel like it's directly from disk, so we ignore already loaded layers and stage cache
	const bool bUseStageCache = false;
	const bool bForceReloadLayersFromDisk = true;
	ImplPtr->UsdStage = UnrealUSDWrapper::OpenStage(*FilePath, EUsdInitialLoadSet::LoadAll, bUseStageCache, bForceReloadLayersFromDisk);
	if (!ImplPtr->UsdStage)
	{
		return false;
	}

	// Apply coordinate system conversion to the stage if we have one
	if (Settings && Settings->bOverrideStageOptions)
	{
		UsdUtils::SetUsdStageMetersPerUnit(ImplPtr->UsdStage, Settings->StageOptions.MetersPerUnit);
		UsdUtils::SetUsdStageUpAxis(ImplPtr->UsdStage, Settings->StageOptions.UpAxis);
	}

	// Cache these so we don't have to keep converting these tokens over and over during translation
	FUsdMeshConversionOptions& MeshOptions = ImplPtr->CachedMeshConversionOptions;
	MeshOptions.PurposesToLoad = (EUsdPurpose)Settings->GeometryPurpose;
	MeshOptions.RenderContext = Settings->RenderContext.IsNone() ? pxr::UsdShadeTokens->universalRenderContext
																 : UnrealToUsd::ConvertToken(*Settings->RenderContext.ToString()).Get();
	MeshOptions.MaterialPurpose = Settings->MaterialPurpose.IsNone() ? pxr::UsdShadeTokens->allPurpose
																	 : UnrealToUsd::ConvertToken(*Settings->MaterialPurpose.ToString()).Get();

	// Traverse stage and emit translated nodes
	FUsdPrim RootPrim = ImplPtr->UsdStage.GetPseudoRoot();
	for (const FUsdPrim& Prim : RootPrim.GetChildren())
	{
		UInterchangeBaseNode* ParentNode = nullptr;
		Traverse(ImplPtr->UsdStage, Prim, ImplPtr, NodeContainer, ParentNode);
	}

	return true;
#else
	return false;
#endif	  // USE_USD_SDK
}

void UInterchangeUSDTranslator::ReleaseSource()
{
	UE::InterchangeUsdTranslator::Private::UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	ImplPtr->UsdStage = UE::FUsdStage{};

	if (ImplPtr->TranslatorSettings)
	{
		ImplPtr->TranslatorSettings->ClearFlags(RF_Standalone);
		ImplPtr->TranslatorSettings = nullptr;
	}
}

UInterchangeTranslatorSettings* UInterchangeUSDTranslator::GetSettings() const
{
	using namespace UE::InterchangeUsdTranslator::Private;

	UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return nullptr;
	}

	TObjectPtr<UInterchangeUsdTranslatorSettings>& Settings = ImplPtr->TranslatorSettings;
	if (!Settings)
	{
		Settings = DuplicateObject<UInterchangeUsdTranslatorSettings>(
			UInterchangeUsdTranslatorSettings::StaticClass()->GetDefaultObject<UInterchangeUsdTranslatorSettings>(),
			GetTransientPackage()
		);
		Settings->LoadSettings();
		Settings->SetFlags(RF_Standalone);
		Settings->ClearInternalFlags(EInternalObjectFlags::Async);
	}
	return Settings;
}

void UInterchangeUSDTranslator::SetSettings(const UInterchangeTranslatorSettings* InterchangeTranslatorSettings)
{
	using namespace UE::InterchangeUsdTranslator::Private;

	UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	TObjectPtr<UInterchangeUsdTranslatorSettings>& Settings = ImplPtr->TranslatorSettings;

	if (Settings)
	{
		Settings->ClearFlags(RF_Standalone);
		Settings->ClearInternalFlags(EInternalObjectFlags::Async);
		Settings = nullptr;
	}
	if (const UInterchangeUsdTranslatorSettings* USDTranslatorSettings = Cast<UInterchangeUsdTranslatorSettings>(InterchangeTranslatorSettings))
	{
		Settings = DuplicateObject<UInterchangeUsdTranslatorSettings>(USDTranslatorSettings, GetTransientPackage());
		Settings->ClearInternalFlags(EInternalObjectFlags::Async);
		Settings->SetFlags(RF_Standalone);
	}
}

TFuture<TOptional<UE::Interchange::FMeshPayloadData>> UInterchangeUSDTranslator::GetMeshPayloadData(
	const FInterchangeMeshPayLoadKey& PayloadKey,
	const FTransform& MeshGlobalTransform
) const
{
	using namespace UE::InterchangeUsdTranslator::Private;

	return Async(
		EAsyncExecution::TaskGraph,
		[this, PayloadKey, MeshGlobalTransform]
		{
			TOptional<UE::Interchange::FMeshPayloadData> Result;

#if USE_USD_SDK
			UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
			if (!ImplPtr)
			{
				return Result;
			}

			UsdToUnreal::FUsdMeshConversionOptions OptionsCopy = ImplPtr->CachedMeshConversionOptions;
			OptionsCopy.AdditionalTransform = MeshGlobalTransform;

			bool bSuccess = false;
			UE::Interchange::FMeshPayloadData MeshPayloadData;
			switch (PayloadKey.Type)
			{
				case EInterchangeMeshPayLoadType::STATIC:
				{
					bSuccess = UE::InterchangeUsdTranslator::Private::GetStaticMeshPayloadDataForPayLoadKey(
						ImplPtr->UsdStage,
						PayloadKey.UniqueId,
						OptionsCopy,
						MeshPayloadData.MeshDescription
					);
					break;
				}
				case EInterchangeMeshPayLoadType::SKELETAL:		  // Fallthrough
				case EInterchangeMeshPayLoadType::MORPHTARGET:	  // Fallthrough
				case EInterchangeMeshPayLoadType::NONE:			  // Fallthrough
				default:
					break;
			}

			if (bSuccess)
			{
				Result.Emplace(MeshPayloadData);
			}
#endif	  // USE_USD_SDK

			return Result;
		}
	);
}

TOptional<UE::Interchange::FImportImage> UInterchangeUSDTranslator::GetTexturePayloadData(
	const FString& PayloadKey,
	TOptional<FString>& AlternateTexturePath
) const
{
	using namespace UE::InterchangeUsdTranslator::Private;

	TOptional<UE::Interchange::FImportImage> TexturePayloadData;

#if USE_USD_SDK
	FString FilePath;
	TextureGroup TextureGroup;
	bool bDecoded = DecodeTexturePayloadKey(PayloadKey, FilePath, TextureGroup);
	if (!bDecoded)
	{
		return {};
	}

	// Defer back to another translator to actually parse the texture raw data
	UE::Interchange::Private::FScopedTranslator ScopedTranslator(FilePath, Results);
	const IInterchangeTexturePayloadInterface* TextureTranslator = ScopedTranslator.GetPayLoadInterface<IInterchangeTexturePayloadInterface>();
	if (!ensure(TextureTranslator))
	{
		return {};
	}

	AlternateTexturePath = FilePath;

	// The texture translators don't use the payload key, and read the texture directly from the SourceData's file path
	const FString UnusedPayloadKey = {};
	TexturePayloadData = TextureTranslator->GetTexturePayloadData(UnusedPayloadKey, AlternateTexturePath);

	// Move compression settings onto the payload data.
	// Note: We don't author anything else on the texture payload data here (like the sRGB flag), because those
	// settings were already on our translated node, and presumably already made their way to the factory node.
	// The factory should use them to override whatever it finds in this payload data, with the exception of the
	// compression settings (which can't be stored on the translated node)
	TexturePayloadData->CompressionSettings = TextureGroup == TEXTUREGROUP_WorldNormalMap ? TC_Normalmap : TC_Default;
#endif	  // USE_USD_SDK

	return TexturePayloadData;
}

TOptional<UE::Interchange::FImportBlockedImage> UInterchangeUSDTranslator::GetBlockedTexturePayloadData(
	const FString& PayloadKey,
	TOptional<FString>& AlternateTexturePath
) const
{
	using namespace UE::InterchangeUsdTranslator::Private;

	UE::Interchange::FImportBlockedImage BlockData;

#if USE_USD_SDK
	FString FilePath;
	TextureGroup TextureGroup;
	bool bDecoded = DecodeTexturePayloadKey(PayloadKey, FilePath, TextureGroup);
	if (!bDecoded)
	{
		return {};
	}

	AlternateTexturePath = FilePath;

	// Collect all the UDIM tile filepaths similar to this current tile. If we've been asked to translate
	// a blocked texture then we must have some
	TMap<int32, FString> TileIndexToPath = UE::TextureUtilitiesCommon::GetUDIMBlocksFromSourceFile(
		FilePath,
		UE::TextureUtilitiesCommon::DefaultUdimRegexPattern
	);
	if (!ensure(TileIndexToPath.Num() > 0))
	{
		return {};
	}

	bool bInitializedBlockData = false;

	TArray<UE::Interchange::FImportImage> TileImages;
	TileImages.Reserve(TileIndexToPath.Num());

	for (const TPair<int32, FString>& TileIndexAndPath : TileIndexToPath)
	{
		int32 UdimTile = TileIndexAndPath.Key;
		const FString& TileFilePath = TileIndexAndPath.Value;

		int32 BlockX = INDEX_NONE;
		int32 BlockY = INDEX_NONE;
		UE::TextureUtilitiesCommon::ExtractUDIMCoordinates(UdimTile, BlockX, BlockY);
		if (BlockX == INDEX_NONE || BlockY == INDEX_NONE)
		{
			continue;
		}

		// Find another translator that actually supports that filetype to handle the texture
		UE::Interchange::Private::FScopedTranslator ScopedTranslator(TileFilePath, Results);
		const IInterchangeTexturePayloadInterface* TextureTranslator = ScopedTranslator.GetPayLoadInterface<IInterchangeTexturePayloadInterface>();
		if (!ensure(TextureTranslator))
		{
			continue;
		}

		// Invoke the translator to actually load the texture and parse it
		const FString UnusedPayloadKey = {};
		TOptional<UE::Interchange::FImportImage> TexturePayloadData;
		TexturePayloadData = TextureTranslator->GetTexturePayloadData(UnusedPayloadKey, AlternateTexturePath);
		if (!TexturePayloadData)
		{
			continue;
		}
		const UE::Interchange::FImportImage& Image = TileImages.Emplace_GetRef(MoveTemp(TexturePayloadData.GetValue()));
		TexturePayloadData.Reset();

		// Initialize the settings on the BlockData itself based on the first image we parse
		if (!bInitializedBlockData)
		{
			bInitializedBlockData = true;

			BlockData.Format = Image.Format;
			BlockData.CompressionSettings = TextureGroup == TEXTUREGROUP_WorldNormalMap ? TC_Normalmap : TC_Default;
			BlockData.bSRGB = Image.bSRGB;
			BlockData.MipGenSettings = Image.MipGenSettings;
		}

		// Prepare the BlockData to receive this image data (later)
		BlockData.InitBlockFromImage(BlockX, BlockY, Image);
	}

	// Move all of the FImportImage buffers into the BlockData itself
	BlockData.MigrateDataFromImagesToRawData(TileImages);
#endif	  // USE_USD_SDK

	return BlockData;
}

TArray<UE::Interchange::FAnimationPayloadData> UInterchangeUSDTranslator::GetAnimationPayloadData(
	const TArray<UE::Interchange::FAnimationPayloadQuery>& PayloadQuery
) const
{
	return {};
}

#undef LOCTEXT_NAMESPACE
