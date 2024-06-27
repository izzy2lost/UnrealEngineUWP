// Copyright Epic Games, Inc. All Rights Reserved.

#include "Usd/InterchangeUsdTranslator.h"

#include "UnrealUSDWrapper.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdGeomXformable.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdRelationship.h"
#include "UsdWrappers/UsdSkelBlendShape.h"
#include "UsdWrappers/UsdSkelBlendShapeQuery.h"
#include "UsdWrappers/UsdSkelCache.h"
#include "UsdWrappers/UsdSkelInbetweenShape.h"
#include "UsdWrappers/UsdSkelSkeletonQuery.h"
#include "UsdWrappers/UsdSkelSkinningQuery.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdTyped.h"

#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDLightConversion.h"
#include "USDLog.h"
#include "USDMaterialUtils.h"
#include "USDPrimConversion.h"
#include "USDShadeConversion.h"
#include "USDSkeletalDataConversion.h"
#include "USDStageOptions.h"
#include "USDTypesConversion.h"

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
#include "MovieSceneSection.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UDIMUtilities.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdLux/tokens.h"
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

static bool GInterchangeEnableUSDLevelImport = false;
static FAutoConsoleVariableRef CVarInterchangeEnableUSDLevelImport(
	TEXT("Interchange.FeatureFlags.Import.USD.ToLevel"),
	GInterchangeEnableUSDLevelImport,
	TEXT("Whether support for USD level import is enabled.")
);

namespace UE::InterchangeUsdTranslator::Private
{
	const static FString AnimationPrefix = TEXT("\\Animation\\");
	const static FString AnimationTrackPrefix = TEXT("\\AnimationTrack\\");
	const static FString CameraPrefix = TEXT("\\Camera\\");
	const static FString LightPrefix = TEXT("\\Light\\");
	const static FString MaterialPrefix = TEXT("\\Material\\");
	const static FString MeshPrefix = TEXT("\\Mesh\\");
	const static FString MorphTargetPrefix = TEXT("\\MorphTarget\\");

	// Information intended to be passed down from parent to children (by value) as we traverse the stage
	struct FTraversalInfo
	{
		UInterchangeBaseNode* ParentNode = nullptr;

		TSharedPtr<UE::FUsdSkelCache> FurthestSkelCache;
		UE::FUsdPrim ClosestParentSkelRoot;

		UE::FUsdSkelSkeletonQuery ActiveSkelQuery;
		TSharedPtr<TArray<FString>> SkelJointNames;	   // Needed for skel mesh payloads
	};

	// clang-format off
	const static TMap<FName, EInterchangePropertyTracks> PropertyNameToTrackType = {
		// Common properties
		{UnrealIdentifiers::HiddenInGamePropertyName, 			EInterchangePropertyTracks::Visibility}, // Binding visibility to the actor works better for cameras

		// Camera properties
		{UnrealIdentifiers::CurrentFocalLengthPropertyName, 	EInterchangePropertyTracks::CameraCurrentFocalLength},
		{UnrealIdentifiers::ManualFocusDistancePropertyName, 	EInterchangePropertyTracks::CameraFocusSettingsManualFocusDistance},
		{UnrealIdentifiers::CurrentAperturePropertyName, 		EInterchangePropertyTracks::CameraCurrentAperture},
		{UnrealIdentifiers::SensorWidthPropertyName, 			EInterchangePropertyTracks::CameraFilmbackSensorWidth},
		{UnrealIdentifiers::SensorHeightPropertyName, 			EInterchangePropertyTracks::CameraFilmbackSensorHeight},

		// Light properties
		{UnrealIdentifiers::LightColorPropertyName, 			EInterchangePropertyTracks::LightColor},
		{UnrealIdentifiers::TemperaturePropertyName, 			EInterchangePropertyTracks::LightTemperature},
		{UnrealIdentifiers::UseTemperaturePropertyName, 		EInterchangePropertyTracks::LightUseTemperature},
		{UnrealIdentifiers::SourceHeightPropertyName, 			EInterchangePropertyTracks::LightSourceHeight},
		{UnrealIdentifiers::SourceWidthPropertyName, 			EInterchangePropertyTracks::LightSourceWidth},
		{UnrealIdentifiers::SourceRadiusPropertyName, 			EInterchangePropertyTracks::LightSourceRadius},
		{UnrealIdentifiers::OuterConeAnglePropertyName, 		EInterchangePropertyTracks::LightOuterConeAngle},
		{UnrealIdentifiers::InnerConeAnglePropertyName, 		EInterchangePropertyTracks::LightInnerConeAngle},
		{UnrealIdentifiers::LightSourceAnglePropertyName, 		EInterchangePropertyTracks::LightSourceAngle},
		{UnrealIdentifiers::IntensityPropertyName, 				EInterchangePropertyTracks::LightIntensity},
	};
	// clang-format on

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

		// When traversing we'll generate FTraversalInfo objects. If we need to (e.g. for skinned meshes),
		// we'll store the info for that translated node here, so we don't have to recompute it when returning
		// the payload data.
		// Note: We only do this when needed: This shouldn't have data for every prim in the stage.
		TMap<FString, FTraversalInfo> NodeUidToCachedTraversalInfo;
		mutable FRWLock CachedTraversalInfoLock;

		// This node eventually becomes a LevelSequence, and all track nodes are connected to it.
		// For now we only generate a single LevelSequence per stage though, so we'll keep track of this
		// here for easy access when parsing the tracks
		UInterchangeAnimationTrackSetNode* CurrentTrackSet = nullptr;
	};
}	 // namespace UE::InterchangeUsdTranslator::Private

UInterchangeUsdTranslatorSettings::UInterchangeUsdTranslatorSettings()
	: GeometryPurpose((int32)(EUsdPurpose::Default | EUsdPurpose::Proxy | EUsdPurpose::Render | EUsdPurpose::Guide))
	, RenderContext(TEXT("unreal"))	   // The proper definition of this is on the USDSchemas module, which we can't depend on
	, MaterialPurpose(*UnrealIdentifiers::MaterialPreviewPurpose)
	, InterpolationType(EUsdInterpolationType::Linear)
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
	return GInterchangeEnableUSDLevelImport ? EInterchangeTranslatorType::Scenes : EInterchangeTranslatorType::Assets;
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

#if USE_USD_SDK
namespace UE::InterchangeUsdTranslator::Private
{
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
		UInterchangeUSDTranslatorImpl& TranslatorImpl,
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
		FString RenderContext = TranslatorImpl.TranslatorSettings ? TranslatorImpl.TranslatorSettings->RenderContext.ToString() : FString();
		const bool bSuccess = UsdToUnreal::ConvertMaterial(Prim, MaterialData, TranslatorImpl.TranslatorSettings ? *RenderContext : nullptr);

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
#if USE_USD_SDK

		FString NodeUid = LightPrefix + Prim.GetPrimPath().GetString();
		FString NodeName(Prim.GetName().ToString());

		// Ref. UsdToUnreal::ConvertLight
		static const FString IntensityToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsIntensity);
		static const FString ExposureToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsExposure);
		static const FString ColorToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsColor);

		float Intensity = UsdUtils::GetAttributeValue<float>(Prim, IntensityToken);
		float Exposure = UsdUtils::GetAttributeValue<float>(Prim, ExposureToken);
		FLinearColor Color = UsdUtils::GetAttributeValue<FLinearColor>(Prim, ColorToken);

		const bool bSRGB = true;
		Color.ToFColor(bSRGB);

		static const FString TemperatureToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsColorTemperature);
		static const FString UseTemperatureToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsEnableColorTemperature);

		float Temperature = UsdUtils::GetAttributeValue<float>(Prim, TemperatureToken);
		bool UseTemperature = UsdUtils::GetAttributeValue<bool>(Prim, UseTemperatureToken);

		// "Shadow enabled" currently not supported

		auto SetBaseLightProperties = [&NodeUid, &NodeName, Color, Temperature, UseTemperature](UInterchangeBaseLightNode* LightNode)
		{
			LightNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
			LightNode->SetAssetName(NodeName);

			LightNode->SetCustomLightColor(Color);
			LightNode->SetCustomTemperature(Temperature);
			LightNode->SetCustomUseTemperature(UseTemperature);
		};

		static const FString RadiusToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsRadius);

		if (Prim.IsA(TEXT("DistantLight")))
		{
			UInterchangeDirectionalLightNode* LightNode = NewObject<UInterchangeDirectionalLightNode>(&NodeContainer);
			SetBaseLightProperties(LightNode);

			Intensity = UsdToUnreal::ConvertLightIntensityAttr(Intensity, Exposure);
			LightNode->SetCustomIntensity(Intensity);

			// LightSourceAngle currently not supported by UInterchangeDirectionalLightNode
			// float Angle = UsdUtils::GetAttributeValue<float>(Prim, TEXTVIEW("inputs:angle"));

			NodeContainer.AddNode(LightNode);
		}
		else if (Prim.IsA(TEXT("SphereLight")))
		{
			const FUsdStageInfo StageInfo(Prim.GetStage());

			const float Radius = UsdUtils::GetAttributeValue<float>(Prim, RadiusToken);
			const float SourceRadius = UsdToUnreal::ConvertDistance(StageInfo, Radius);	   // currently not supported

			if (Prim.HasAPI(TEXT("ShapingAPI")))
			{
				UInterchangeSpotLightNode* LightNode = NewObject<UInterchangeSpotLightNode>(&NodeContainer);
				SetBaseLightProperties(LightNode);

				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Lumens);

				static const FString ConeAngleToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsShapingConeAngle);
				static const FString ConeSoftnessToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsShapingConeSoftness);

				float ConeAngle = UsdUtils::GetAttributeValue<float>(Prim, ConeAngleToken);
				float ConeSoftness = UsdUtils::GetAttributeValue<float>(Prim, ConeSoftnessToken);

				float InnerConeAngle = 0.0f;
				const float OuterConeAngle = UsdToUnreal::ConvertConeAngleSoftnessAttr(ConeAngle, ConeSoftness, InnerConeAngle);

				Intensity = UsdToUnreal::ConvertLuxShapingAPIIntensityAttr(Intensity, Exposure, Radius, ConeAngle, ConeSoftness, StageInfo);
				LightNode->SetCustomIntensity(Intensity);

				LightNode->SetCustomInnerConeAngle(InnerConeAngle);
				LightNode->SetCustomOuterConeAngle(OuterConeAngle);

				NodeContainer.AddNode(LightNode);
			}
			else
			{
				UInterchangePointLightNode* LightNode = NewObject<UInterchangePointLightNode>(&NodeContainer);
				SetBaseLightProperties(LightNode);

				LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Lumens);

				Intensity = UsdToUnreal::ConvertSphereLightIntensityAttr(Intensity, Exposure, Radius, StageInfo);
				LightNode->SetCustomIntensity(Intensity);

				NodeContainer.AddNode(LightNode);
			}
		}
		else if (Prim.IsA(TEXT("RectLight")) || Prim.IsA(TEXT("DiskLight")))
		{
			UInterchangeRectLightNode* LightNode = NewObject<UInterchangeRectLightNode>(&NodeContainer);
			SetBaseLightProperties(LightNode);

			LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Lumens);

			static const FString WidthToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsWidth);
			static const FString HeightToken = UsdToUnreal::ConvertToken(pxr::UsdLuxTokens->inputsHeight);

			float Width = UsdUtils::GetAttributeValue<float>(Prim, WidthToken);
			float Height = UsdUtils::GetAttributeValue<float>(Prim, HeightToken);

			const FUsdStageInfo StageInfo(Prim.GetStage());

			if (Prim.IsA(TEXT("RectLight")))
			{
				Width = UsdToUnreal::ConvertDistance(StageInfo, Width);
				Height = UsdToUnreal::ConvertDistance(StageInfo, Height);
				Intensity = UsdToUnreal::ConvertRectLightIntensityAttr(Intensity, Exposure, Width, Height, StageInfo);
			}
			else
			{
				float Radius = UsdUtils::GetAttributeValue<float>(Prim, RadiusToken);
				Width = UsdToUnreal::ConvertDistance(StageInfo, Radius) * 2.f;
				Height = Width;

				Intensity = UsdToUnreal::ConvertDiskLightIntensityAttr(Intensity, Exposure, Radius, StageInfo);
			}
			LightNode->SetCustomIntensity(Intensity);
			LightNode->SetCustomSourceWidth(Width);
			LightNode->SetCustomSourceHeight(Height);

			NodeContainer.AddNode(LightNode);
		}
		// #ueent_todo:
		// DomeLight -> SkyLight
#endif	  // USE_USD_SDK
	}

	void AddCameraNode(const UE::FUsdPrim& Prim, UInterchangeBaseNodeContainer& NodeContainer)
	{
#if USE_USD_SDK
		FString NodeUid = CameraPrefix + Prim.GetPrimPath().GetString();
		FString NodeName(Prim.GetName().ToString());

		UInterchangePhysicalCameraNode* CameraNode = NewObject<UInterchangePhysicalCameraNode>(&NodeContainer);
		CameraNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		NodeContainer.AddNode(CameraNode);

		// ref. UsdToUnreal::ConvertGeomCamera
		UE::FUsdStage Stage = Prim.GetStage();
		FUsdStageInfo StageInfo(Stage);

		static const FString FocalLengthToken = UsdToUnreal::ConvertToken(pxr::UsdGeomTokens->focalLength);
		static const FString HorizontalApertureToken = UsdToUnreal::ConvertToken(pxr::UsdGeomTokens->horizontalAperture);
		static const FString VerticalApertureToken = UsdToUnreal::ConvertToken(pxr::UsdGeomTokens->verticalAperture);

		float FocalLength = UsdUtils::GetAttributeValue<float>(Prim, FocalLengthToken);
		FocalLength = UsdToUnreal::ConvertDistance(StageInfo, FocalLength);
		CameraNode->SetCustomFocalLength(FocalLength);

		float SensorWidth = UsdUtils::GetAttributeValue<float>(Prim, HorizontalApertureToken);
		SensorWidth = UsdToUnreal::ConvertDistance(StageInfo, SensorWidth);
		CameraNode->SetCustomSensorWidth(SensorWidth);

		float SensorHeight = UsdUtils::GetAttributeValue<float>(Prim, VerticalApertureToken);
		SensorHeight = UsdToUnreal::ConvertDistance(StageInfo, SensorHeight);
		CameraNode->SetCustomSensorHeight(SensorHeight);

		// Focus distance and FStop not currently supported
#endif	  // USE_USD_SDK
	}

	void AddMorphTargetNodes(
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl& TranslatorImpl,
		UInterchangeMeshNode& MeshNode,
		UInterchangeBaseNodeContainer& NodeContainer
	)
	{
		UE::FUsdSkelBlendShapeQuery Query{Prim};
		if (!Query)
		{
			return;
		}

		const FString MeshPrimPath = Prim.GetPrimPath().GetString();

		TFunction<void(const FString&, int32, const FString&)> AddMorphTargetNode =
			[&MeshNode, &MeshPrimPath, &NodeContainer](const FString& MorphTargetName, int32 BlendShapeIndex, const FString& InbetweenName)
		{
			// Note: We identify a blend shape by its Mesh prim path and the blend shape index, even though
			// the blend shape itself is a full standalone prim. This is for two reasons:
			//  - We need to also read the Mesh prim's mesh data when emitting the payload, so having the Mesh path on the payload key is handy;
			//  - It could be possible for different meshes to share the same BlendShape (possibly?), so we really want a separate version of
			//    a blend shape for each mesh that uses it.
			//
			// Despite of that though, we'll just use the blendshape (+inbetween) name for MorphTargetName (so not anything's full path),
			// so that users can get different blendshapes across the model to combine into a single morph target. Interchange has
			// an import option to let you control whether they become separate morph targets or not anyway
			// ("Merge Morph Targets with Same Name")
			FString IDString = FString::Printf(TEXT("%s\\%d\\%s"), *MeshPrimPath, BlendShapeIndex, *InbetweenName);
			FString MorphTargetUid = MorphTargetPrefix + IDString;

			UInterchangeMeshNode* MorphTargetMeshNode = NewObject<UInterchangeMeshNode>(&NodeContainer);
			MorphTargetMeshNode->InitializeNode(MorphTargetUid, MorphTargetName, EInterchangeNodeContainerType::TranslatedAsset);
			MorphTargetMeshNode->SetPayLoadKey(IDString, EInterchangeMeshPayLoadType::MORPHTARGET);
			MorphTargetMeshNode->SetMorphTarget(true);
			MorphTargetMeshNode->SetMorphTargetName(MorphTargetName);
			NodeContainer.AddNode(MorphTargetMeshNode);
			MeshNode.SetMorphTargetDependencyUid(MorphTargetUid);
		};

		for (size_t Index = 0; Index < Query.GetNumBlendShapes(); ++Index)
		{
			UE::FUsdSkelBlendShape BlendShape = Query.GetBlendShape(Index);
			if (!BlendShape)
			{
				continue;
			}

			UE::FUsdPrim BlendShapePrim = BlendShape.GetPrim();
			FString MorphTargetName = BlendShapePrim.GetName().ToString();
			AddMorphTargetNode(MorphTargetName, Index, FString{});

			TArray<UE::FUsdSkelInbetweenShape> Inbetweens = BlendShape.GetInbetweens();
			for (const UE::FUsdSkelInbetweenShape& Inbetween : Inbetweens)
			{
				FString InbetweenName = Inbetween.GetAttr().GetName().ToString();
				FString InnerMorphTargetName = MorphTargetName + TEXT("_") + InbetweenName;
				AddMorphTargetNode(InnerMorphTargetName, Index, InbetweenName);
			}
		}
	}

	void AddMeshNode(
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl& TranslatorImpl,
		UInterchangeBaseNodeContainer& NodeContainer,
		const FTraversalInfo& Info
	)
	{
		FString PrimPath = Prim.GetPrimPath().GetString();
		FString NodeUid = MeshPrefix + PrimPath;
		FString NodeName(Prim.GetName().ToString());

		// Check if Node already exist with this ID
		if (const UInterchangeMeshNode* Node = Cast<UInterchangeMeshNode>(NodeContainer.GetNode(NodeUid)))
		{
			return;
		}

		// Fill in the MeshNode itself
		UInterchangeMeshNode* MeshNode = NewObject<UInterchangeMeshNode>(&NodeContainer);
		MeshNode->InitializeNode(NodeUid, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		MeshNode->SetAssetName(NodeName);
		const bool bIsSkinned = static_cast<bool>(Info.ClosestParentSkelRoot) && Prim.HasAPI(TEXT("SkelBindingAPI"));
		if (bIsSkinned)
		{
			MeshNode->SetSkinnedMesh(true);
			MeshNode->SetPayLoadKey(PrimPath, EInterchangeMeshPayLoadType::SKELETAL);
			if (Info.ActiveSkelQuery)
			{
				MeshNode->SetSkeletonDependencyUid(Info.ActiveSkelQuery.GetSkeleton().GetPrimPath().GetString());
			}

			AddMorphTargetNodes(Prim, TranslatorImpl, *MeshNode, NodeContainer);

			// When returning the payload data later, we'll need at the very least our SkeletonQuery, so
			// here we store the Info object into the Impl
			{
				FWriteScopeLock ScopedInfoWriteLock{TranslatorImpl.CachedTraversalInfoLock};
				TranslatorImpl.NodeUidToCachedTraversalInfo.Add(NodeUid, Info);
			}
		}
		else
		{
			MeshNode->SetPayLoadKey(PrimPath, EInterchangeMeshPayLoadType::STATIC);
		}

		// Material assignments
		{
			const double TimeCode = UsdUtils::GetDefaultTimeCode();
			const bool bProvideMaterialIndices = false;
			UsdUtils::FUsdPrimMaterialAssignmentInfo Assignments = UsdUtils::GetPrimMaterialAssignments(
				Prim,
				TimeCode,
				bProvideMaterialIndices,
				TranslatorImpl.CachedMeshConversionOptions.RenderContext,
				TranslatorImpl.CachedMeshConversionOptions.MaterialPurpose
			);

			for (const UsdUtils::FUsdPrimMaterialSlot& Slot : Assignments.Slots)
			{
				// Use the material prim path/display color desc as the material slot name, because Interchange
				// already has a mechanism to merge material slots with the same name. Using the material name itself
				// as the slot name has Interchange combine slots with identical materials, which works fine. If we
				// were to use GeomSubset names or prim names in here though, it's possible that two similarly named
				// slots in different skeletal mesh chunks (but with different materials!) could get merged together,
				// which is not what we want
				const FString& SlotName = Slot.MaterialSource;

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

	void AddSkeletonNodes(
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl& TranslatorImpl,
		UInterchangeSceneNode& SkeletonPrimNode,
		UInterchangeBaseNodeContainer& NodeContainer,
		FTraversalInfo Info
	)
	{
		// If we're not inside of a SkelRoot, the skeleton shouldn't really do anything
		if (!Info.FurthestSkelCache.IsValid())
		{
			return;
		}

		// By the time we get here we've already emitted a scene node for the skeleton prim itself, so we just
		// need to emit a node hierarchy that mirrors the joints.

		// Make the prim node into an Interchange joint/bone itself. By doing this we solve three issues:
		//  - It becomes easy to identify our SkeletonDependencyUid when parsing Mesh nodes: It's just the skeleton prim path
		//    (as opposed to having to target the translated node of the first root joint of the skeleton);
		//  - We automatically handle USD skeletons with multiple root bones: We'll only ever have one "true"
		//    root bone anyway: The SkeletonPrimNode itself;
		//  - If a skeleton has no bones at all somehow, we'll still make one "bone" for it (this node).
		SkeletonPrimNode.AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
		SkeletonPrimNode.SetCustomBindPoseLocalTransform(&NodeContainer, FTransform::Identity);
		SkeletonPrimNode.SetCustomTimeZeroLocalTransform(&NodeContainer, FTransform::Identity);

		// Convert the skeleton bones/joints into ConvertedData
		UE::FUsdSkelSkeletonQuery SkelQuery = Info.FurthestSkelCache->GetSkelQuery(Prim);
		const bool bEnsureAtLeastOneBone = false;
		const bool bEnsureSingleRootBone = false;
		UsdToUnreal::FUsdSkeletonData ConvertedData;
		const bool bSuccess = UsdToUnreal::ConvertSkeleton(SkelQuery, ConvertedData, bEnsureAtLeastOneBone, bEnsureSingleRootBone);
		if (!bSuccess)
		{
			return;
		}

		// Recursively traverse ConvertedData spawning the joint translated nodes
		TFunction<void(const UsdToUnreal::FUsdSkeletonData::FBone&, UInterchangeSceneNode&, const FString&)> RecursiveTraverseBones = nullptr;
		RecursiveTraverseBones = [&RecursiveTraverseBones,
								  &ConvertedData,
								  &NodeContainer	//
		](const UsdToUnreal::FUsdSkeletonData::FBone& Bone, UInterchangeSceneNode& ParentNode, const FString& BonePath)
		{
			// Concatenate a full "bone path" here for uniqueness, because Bone.Name is just the name of this
			// single bone/joint itself (e.g. "Elbow")
			const FString BoneNodeUid = BonePath + TEXT("\\") + Bone.Name;

			UInterchangeSceneNode* BoneNode = NewObject<UInterchangeSceneNode>(&NodeContainer);
			BoneNode->InitializeNode(BoneNodeUid, Bone.Name, EInterchangeNodeContainerType::TranslatedScene);
			BoneNode->SetCustomLocalTransform(&NodeContainer, Bone.LocalRestTransform);

			BoneNode->AddSpecializedType(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString());
			BoneNode->SetCustomBindPoseLocalTransform(&NodeContainer, Bone.LocalBindTransform);
			BoneNode->SetCustomTimeZeroLocalTransform(&NodeContainer, Bone.LocalRestTransform);

			NodeContainer.AddNode(BoneNode);
			NodeContainer.SetNodeParentUid(BoneNodeUid, ParentNode.GetUniqueID());

			for (int32 ChildIndex : Bone.ChildIndices)
			{
				RecursiveTraverseBones(ConvertedData.Bones[ChildIndex], *BoneNode, BoneNodeUid);
			}
		};

		// Start traversing from the root bones (we may have more than one)
		const FString SkeletonPrimNodeUid = SkeletonPrimNode.GetUniqueID();
		for (const UsdToUnreal::FUsdSkeletonData::FBone& Bone : ConvertedData.Bones)
		{
			if (Bone.ParentIndex == INDEX_NONE)
			{
				RecursiveTraverseBones(Bone, SkeletonPrimNode, SkeletonPrimNodeUid);
			}
		}

		// Cache our joint names in order, as this is needed when generating skeletal mesh payloads
		Info.SkelJointNames = MakeShared<TArray<FString>>();
		Info.SkelJointNames->Reserve(ConvertedData.Bones.Num());
		for (const UsdToUnreal::FUsdSkeletonData::FBone& Bone : ConvertedData.Bones)
		{
			Info.SkelJointNames->Add(Bone.Name);
		}
		{
			FWriteScopeLock ScopedInfoWriteLock{TranslatorImpl.CachedTraversalInfoLock};
			TranslatorImpl.NodeUidToCachedTraversalInfo.Add(SkeletonPrimNodeUid, Info);
		}
	}

	void UpdateTraversalInfo(FTraversalInfo& Info, const UE::FUsdPrim& CurrentPrim)
	{
		if (CurrentPrim.IsA(TEXT("SkelRoot")))
		{
			if (!Info.ClosestParentSkelRoot)
			{
				// The root-most skel cache should handle any nested UsdSkel prims as well
				Info.FurthestSkelCache = MakeShared<UE::FUsdSkelCache>();

				const bool bTraverseInstanceProxies = true;
				Info.FurthestSkelCache->Populate(CurrentPrim, bTraverseInstanceProxies);
			}

			Info.ClosestParentSkelRoot = CurrentPrim;
		}

		if (Info.ClosestParentSkelRoot && CurrentPrim.HasAPI(TEXT("SkelBindingAPI")))
		{
			UE::FUsdStage Stage = CurrentPrim.GetStage();

			if (UE::FUsdRelationship SkelRel = CurrentPrim.GetRelationship(TEXT("skel:skeleton")))
			{
				TArray<UE::FSdfPath> Targets;
				if (SkelRel.GetTargets(Targets) && Targets.Num() > 0)
				{
					UE::FUsdPrim TargetSkeleton = Stage.GetPrimAtPath(Targets[0]);
					if (TargetSkeleton && TargetSkeleton.IsA(TEXT("Skeleton")))
					{
						Info.ActiveSkelQuery = Info.FurthestSkelCache->GetSkelQuery(TargetSkeleton);
					}
				}
			}
		}
	}

	void AddTrackSetNode(UInterchangeUSDTranslatorImpl& Impl, UInterchangeBaseNodeContainer& NodeContainer)
	{
		// For now we only want a single track set (i.e. LevelSequence) per stage.
		// TODO: One track set per layer, and add the tracks to the tracksets that correspond to layers where the opinions came from
		// (similar to LevelSequenceHelper). Then we can use UInterchangeAnimationTrackSetInstanceNode to create "subsequences"
		if (Impl.CurrentTrackSet)
		{
			return;
		}

		UE::FSdfLayer Layer = Impl.UsdStage.GetRootLayer();
		const FString AnimTrackSetNodeUid = AnimationPrefix + Layer.GetIdentifier();
		const FString AnimTrackSetNodeDisplayName = FPaths::GetBaseFilename(Layer.GetDisplayName());	// Strip extension

		// We should only have one track set node per scene for now
		const UInterchangeAnimationTrackSetNode* ExistingNode = Cast<UInterchangeAnimationTrackSetNode>(NodeContainer.GetNode(AnimTrackSetNodeUid));
		if (!ensure(ExistingNode == nullptr))
		{
			return;
		};

		UInterchangeAnimationTrackSetNode* TrackSetNode = NewObject<UInterchangeAnimationTrackSetNode>(&NodeContainer);
		TrackSetNode->InitializeNode(AnimTrackSetNodeUid, AnimTrackSetNodeDisplayName, EInterchangeNodeContainerType::TranslatedAsset);
		TrackSetNode->SetCustomFrameRate(Layer.GetFramesPerSecond());	 // Key values in Interchange seem to be in seconds, so timeCodesPerSecond is
																		 // not relevant here

		NodeContainer.AddNode(TrackSetNode);
		Impl.CurrentTrackSet = TrackSetNode;
	}

	void AddTransformAnimationNode(const UE::FUsdPrim& Prim, UInterchangeUSDTranslatorImpl& Impl, UInterchangeBaseNodeContainer& NodeContainer)
	{
		const FString PrimPath = Prim.GetPrimPath().GetString();
		const FString UniquePath = PrimPath + TEXT("\\") + UnrealIdentifiers::TransformPropertyName.ToString();
		const FString AnimTrackNodeUid = AnimationTrackPrefix + UniquePath;

		const UInterchangeTransformAnimationTrackNode* ExistingNode = Cast<UInterchangeTransformAnimationTrackNode>(
			NodeContainer.GetNode(AnimTrackNodeUid)
		);
		if (ExistingNode)
		{
			return;
		}

		UInterchangeTransformAnimationTrackNode* TransformAnimTrackNode = NewObject<UInterchangeTransformAnimationTrackNode>(&NodeContainer);
		TransformAnimTrackNode->InitializeNode(AnimTrackNodeUid, UniquePath, EInterchangeNodeContainerType::TranslatedAsset);
		TransformAnimTrackNode->SetCustomActorDependencyUid(*PrimPath);
		TransformAnimTrackNode->SetCustomAnimationPayloadKey(UniquePath, EInterchangeAnimationPayLoadType::CURVE);
		TransformAnimTrackNode->SetCustomUsedChannels((int32)EMovieSceneTransformChannel::AllTransform);

		NodeContainer.AddNode(TransformAnimTrackNode);

		AddTrackSetNode(Impl, NodeContainer);
		Impl.CurrentTrackSet->AddCustomAnimationTrackUid(AnimTrackNodeUid);
	}

	void AddPropertyAnimationNodes(const UE::FUsdPrim& Prim, UInterchangeUSDTranslatorImpl& Impl, UInterchangeBaseNodeContainer& NodeContainer)
	{
		using namespace UE::InterchangeUsdTranslator::Private;

		if (!Prim)
		{
			return;
		}
		const FString PrimPath = Prim.GetPrimPath().GetString();

		for (UE::FUsdAttribute Attr : Prim.GetAttributes())
		{
			if (!Attr || !Attr.ValueMightBeTimeVarying() || Attr.GetNumTimeSamples() == 0)
			{
				continue;
			}

			// Emit a STEPCURVE in case of a bool track: CURVE is only for floats/doubles (c.f. FLevelSequenceHelper::PopulateAnimationTrack).
			// For now we're lucky in that all possible results from GetPropertiesForAttribute() are either all not bool, or either all bool,
			// so we can reuse this for all the different UEAttrNames we get from the same attribute
			const FName AttrTypeName = Attr.GetTypeName();
			const bool bIsBoolTrack = AttrTypeName == TEXT("bool") || AttrTypeName == TEXT("token");	// Visibility is a token track

			TArray<FName> UEAttrNames = UsdUtils::GetPropertiesForAttribute(Prim, Attr.GetName().ToString());
			for (const FName& UEAttrName : UEAttrNames)
			{
				const EInterchangePropertyTracks* FoundTrackType = PropertyNameToTrackType.Find(UEAttrName);
				if (!FoundTrackType)
				{
					continue;
				}

				// We don't use the USD attribute path here because we want one unique node per UE track name,
				// so that if e.g. both "intensity" and "exposure" are animated we make a single track for
				// the Intensity UE property
				const FString UniquePath = PrimPath + TEXT("\\") + UEAttrName.ToString();
				const FString AnimTrackNodeUid = AnimationTrackPrefix + UniquePath;

				const UInterchangeAnimationTrackNode* ExistingNode = Cast<UInterchangeAnimationTrackNode>(NodeContainer.GetNode(AnimTrackNodeUid));
				if (ExistingNode)
				{
					continue;
				}

				UInterchangeAnimationTrackNode* AnimTrackNode = NewObject<UInterchangeAnimationTrackNode>(&NodeContainer);
				AnimTrackNode->InitializeNode(AnimTrackNodeUid, UniquePath, EInterchangeNodeContainerType::TranslatedAsset);
				AnimTrackNode->SetCustomActorDependencyUid(*PrimPath);
				AnimTrackNode->SetCustomPropertyTrack(*FoundTrackType);
				AnimTrackNode->SetCustomAnimationPayloadKey(
					UniquePath,
					bIsBoolTrack ? EInterchangeAnimationPayLoadType::STEPCURVE : EInterchangeAnimationPayLoadType::CURVE
				);

				NodeContainer.AddNode(AnimTrackNode);

				AddTrackSetNode(Impl, NodeContainer);
				Impl.CurrentTrackSet->AddCustomAnimationTrackUid(AnimTrackNodeUid);
			}
		}
	}

	void Traverse(
		const UE::FUsdPrim& Prim,
		UInterchangeUSDTranslatorImpl& TranslatorImpl,
		UInterchangeBaseNodeContainer& NodeContainer,
		FTraversalInfo Info
	)
	{
		// Ignore prim subtrees from disabled purposes
		// TODO: Move this to the pipeline and filter only the factory nodes
		EUsdPurpose PrimPurpose = IUsdPrim::GetPurpose(Prim);
		if (!EnumHasAllFlags(TranslatorImpl.CachedMeshConversionOptions.PurposesToLoad, PrimPurpose))
		{
			return;
		}

		FString SceneNodeUid = Prim.GetPrimPath().GetString();
		FString DisplayLabel(Prim.GetName().ToString());

		// Do this before generating other nodes as they may need the updated info
		UpdateTraversalInfo(Info, Prim);

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
			AddMeshNode(Prim, TranslatorImpl, NodeContainer, Info);
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

		// Only prims that require rendering (and have a renderable parent) get a scene node.
		// This includes Xforms but also Scopes, which are not Xformable
		UInterchangeSceneNode* SceneNode = nullptr;
		if (Prim.IsA(TEXT("Imageable")) && (Info.ParentNode || Prim.GetParent().IsPseudoRoot()))
		{
			SceneNode = NewObject<UInterchangeSceneNode>(&NodeContainer);
			SceneNode->InitializeNode(SceneNodeUid, DisplayLabel, EInterchangeNodeContainerType::TranslatedScene);
			NodeContainer.AddNode(SceneNode);

			// If we're an Xformable, get our transform
			FTransform Transform = FTransform::Identity;
			bool bResetTransformStack = false;
			if (UsdToUnreal::ConvertXformable(
					Prim.GetStage(),
					UE::FUsdTyped(Prim),
					Transform,
					UsdUtils::GetEarliestTimeCode(),
					&bResetTransformStack
				))
			{
				SceneNode->SetCustomLocalTransform(&NodeContainer, Transform);
			}

			// Skeleton joints are separate scene nodes in Interchange, so we need to emit that node hierarchy now
			if (Prim.IsA(TEXT("Skeleton")))
			{
				AddSkeletonNodes(Prim, TranslatorImpl, *SceneNode, NodeContainer, Info);
			}

			// Connect scene node and asset node
			if (Prefix)
			{
				const FString AssetNodeUid = *Prefix + SceneNodeUid;
				SceneNode->SetCustomAssetInstanceUid(AssetNodeUid);
			}

			// Connect parent and child scene nodes
			if (Info.ParentNode)
			{
				NodeContainer.SetNodeParentUid(SceneNode->GetUniqueID(), Info.ParentNode->GetUniqueID());
			}

			// Add animation tracks
			AddPropertyAnimationNodes(Prim, TranslatorImpl, NodeContainer);
			if (UsdUtils::HasAnimatedTransform(Prim))
			{
				AddTransformAnimationNode(Prim, TranslatorImpl, NodeContainer);
			}
		}

		// Note: This has the effect of effectively shutting down the generation of scene nodes
		// below any prim that is not a least an Imageable, as we check for a valid parent before
		// generating one
		Info.ParentNode = SceneNode;

		// Recurse into child prims
		for (const FUsdPrim& ChildPrim : Prim.GetChildren())
		{
			Traverse(ChildPrim, TranslatorImpl, NodeContainer, Info);
		}
	}

	void FixSkeletalMeshDescriptionColors(FMeshDescription& MeshDescription)
	{
		// FSkeletalMeshImportData::GetMeshDescription() will reinterpret our Wedge FColors as linear, and put those
		// sRGB values disguised as linear into the mesh description. This also seems to disagree with the patch on
		// cl 32791826, so here we have to fix that up and get our mesh description colors to be actually linear...
		//
		// This will hopefully go away once we have our own skinned mesh to FMeshDescription conversion function.
		//
		// Note: Weirdly enough skeletal meshes seem to put linear colors on VertexColor output, while static meshes
		// put sRGB colors? Maybe this is why the comment above the change on 32791826 mentions to remove the ToFColor on
		// StaticMeshBuilder? This is overall very confusing
		FStaticMeshAttributes Attributes(MeshDescription);
		TVertexInstanceAttributesRef<FVector4f> VertexColor = Attributes.GetVertexInstanceColors();
		for (FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
		{
			const FColor ActualSRGB = FLinearColor(VertexColor[VertexInstanceID]).ToFColor(false);
			VertexColor[VertexInstanceID] = FLinearColor{ActualSRGB};
		}
	}

	void FixMaterialSlotNames(FMeshDescription& MeshDescription, const TArray<UsdUtils::FUsdPrimMaterialSlot>& MeshAssingmentSlots)
	{
		// Fixup material slot names to match the material that is assigned. For Interchange it is better to have the material
		// slot names match what is assigned into them, as it will use those names to "merge identical slots" depending on the
		// import options.
		//
		// Note: These names must also match what is set via MeshNode->SetSlotMaterialDependencyUid(SlotName, MaterialUid)
		FStaticMeshAttributes StaticMeshAttributes(MeshDescription);
		for (int32 MaterialSlotIndex = 0; MaterialSlotIndex < StaticMeshAttributes.GetPolygonGroupMaterialSlotNames().GetNumElements();
			 ++MaterialSlotIndex)
		{
			int32 MaterialIndex = 0;
			LexFromString(MaterialIndex, *StaticMeshAttributes.GetPolygonGroupMaterialSlotNames()[MaterialSlotIndex].ToString());

			if (MeshAssingmentSlots.IsValidIndex(MaterialIndex))
			{
				const FString Source = MeshAssingmentSlots[MaterialIndex].MaterialSource;
				StaticMeshAttributes.GetPolygonGroupMaterialSlotNames()[MaterialSlotIndex] = *Source;
			}
		}
	}

	bool GetStaticMeshPayloadData(
		const FString& PayloadKey,
		const UInterchangeUSDTranslatorImpl& Impl,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		FMeshDescription& OutMeshDescription
	)
	{
		const FString& PrimPath = PayloadKey;
		UE::FUsdPrim Prim = Impl.UsdStage.GetPrimAtPath(UE::FSdfPath{*PrimPath});
		if (!Prim)
		{
			return false;
		}

		// TODO: We can't do much with these yet: There will be used to generate primvar-compatible
		// versions of the materials that are assigned to this mesh, whenever we get a pipeline
		UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;
		bool bSuccess = UsdToUnreal::ConvertGeomMesh(Prim, OutMeshDescription, TempMaterialInfo, Options);
		if (!bSuccess)
		{
			return false;
		}

		FixMaterialSlotNames(OutMeshDescription, TempMaterialInfo.Slots);

		return true;
	}

	bool GetSkeletalMeshPayloadData(
		const FString& PayloadKey,
		const UInterchangeUSDTranslatorImpl& Impl,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		FMeshDescription& OutMeshDescription,
		TArray<FString>& OutJointNames
	)
	{
		const FString& PrimPath = PayloadKey;
		UE::FUsdPrim Prim = Impl.UsdStage.GetPrimAtPath(UE::FSdfPath{*PrimPath});
		if (!Prim)
		{
			return false;
		}

		const FString& MeshNodeUid = MeshPrefix + Prim.GetPrimPath().GetString();

		// Read these variables from the data we cached during traversal for translation
		TSharedPtr<TArray<FString>> JointNames = nullptr;
		UE::FUsdSkelSkeletonQuery SkelQuery;
		{
			FReadScopeLock ReadLock{Impl.CachedTraversalInfoLock};

			const FTraversalInfo* MeshInfo = Impl.NodeUidToCachedTraversalInfo.Find(MeshNodeUid);
			if (!MeshInfo)
			{
				return false;
			}
			SkelQuery = MeshInfo->ActiveSkelQuery;
			if (!SkelQuery)
			{
				return false;
			}

			// The above fields are associated to the mesh *asset* node Uid (hence the prefix),
			// while the joint names are associated to the skeleton *scene* node Uid, so no prefix
			const FString SkeletonNodeUid = SkelQuery.GetSkeleton().GetPrimPath().GetString();
			const FTraversalInfo* SkeletonInfo = Impl.NodeUidToCachedTraversalInfo.Find(SkeletonNodeUid);
			if (!SkeletonInfo)
			{
				return false;
			}
			JointNames = SkeletonInfo->SkelJointNames;
			if (!JointNames)
			{
				return false;
			}
		}

		UE::FUsdSkelSkinningQuery SkinningQuery = UsdUtils::CreateSkinningQuery(Prim, SkelQuery);
		if (!SkinningQuery)
		{
			return false;
		}

		FSkeletalMeshImportData SkelMeshImportData;
		UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;
		bool bSuccess = UsdToUnreal::ConvertSkinnedMesh(SkinningQuery, SkelQuery, SkelMeshImportData, TempMaterialInfo, Options);
		if (!bSuccess)
		{
			return false;
		}

		// TODO: Swap this code path with some function to directly convert a skinned USD mesh to MeshDescription.
		// We need that on the other USD workflows as well, not only here...
		//
		// Note: This is also doubly bad because it internally recomputes tangents and normals, which will also
		// be done by Interchange later..
		const USkeletalMesh* UnusedSkelMesh = nullptr;
		FSkeletalMeshBuildSettings* UnusedBuildSettings = nullptr;
		bSuccess = SkelMeshImportData.GetMeshDescription(UnusedSkelMesh, UnusedBuildSettings, OutMeshDescription);
		if (!bSuccess)
		{
			return false;
		}

		FixSkeletalMeshDescriptionColors(OutMeshDescription);

		FixMaterialSlotNames(OutMeshDescription, TempMaterialInfo.Slots);

		OutJointNames = *JointNames;

		return true;
	}

	bool GetMorphTargetPayloadData(
		const FString& PayloadKey,
		const UInterchangeUSDTranslatorImpl& Impl,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		FMeshDescription& OutMeshDescription,
		FString& OutMorphTargetName
	)
	{
		// Our payloadkey should be something like "/MySkelRoot/MyMeshPrim\2\BlendShapeName_inbetweens:someName"
		const bool bCullEmpty = false;
		TArray<FString> PayloadKeyTokens;
		PayloadKey.ParseIntoArray(PayloadKeyTokens, TEXT("\\"), bCullEmpty);
		if (PayloadKeyTokens.Num() != 3)
		{
			return false;
		}

		const FString& MeshPrimPath = PayloadKeyTokens[0];
		const FString& BlendShapeIndexStr = PayloadKeyTokens[1];
		const FString& InbetweenName = PayloadKeyTokens[2];

		int32 BlendShapeIndex = INDEX_NONE;
		bool bLexed = LexTryParseString(BlendShapeIndex, *BlendShapeIndexStr);
		if (!bLexed)
		{
			return false;
		}

		UE::FUsdPrim MeshPrim = Impl.UsdStage.GetPrimAtPath(FSdfPath{*MeshPrimPath});
		UE::FUsdSkelBlendShapeQuery Query{MeshPrim};
		if (!Query)
		{
			return false;
		}

		UE::FUsdSkelBlendShape BlendShape = Query.GetBlendShape(BlendShapeIndex);
		if (!BlendShape)
		{
			return false;
		}

		// TODO: This is extremely slow, as it will reimport the mesh for every single morph target!
		// It seems to be what the other translators do, however. We need some form of FMeshDescription caching here
		TArray<FString> UnusedJointNames;
		bool bConverted = GetSkeletalMeshPayloadData(MeshPrimPath, Impl, Options, OutMeshDescription, UnusedJointNames);
		if (!bConverted || OutMeshDescription.IsEmpty())
		{
			return false;
		}

		OutMorphTargetName = BlendShape.GetPrim().GetName().ToString();
		if (!InbetweenName.IsEmpty())
		{
			OutMorphTargetName += TEXT("_") + InbetweenName;
		}

		const float Weight = 1.0f;
		return UsdUtils::ApplyBlendShape(OutMeshDescription, BlendShape.GetPrim(), Weight, InbetweenName);
	}

	bool ReadBools(
		const UE::FUsdStage& UsdStage,
		const TArray<double>& UsdTimeSamples,
		const TFunction<bool(double)>& ReaderFunc,
		Interchange::FAnimationPayloadData& OutPayloadData
	)
	{
		OutPayloadData.StepCurves.SetNum(1);
		FInterchangeStepCurve& Curve = OutPayloadData.StepCurves[0];
		TArray<float>& KeyTimes = Curve.KeyTimes;
		TArray<bool>& BooleanKeyValues = Curve.BooleanKeyValues.Emplace();

		KeyTimes.Reserve(UsdTimeSamples.Num());
		BooleanKeyValues.Reserve(UsdTimeSamples.Num());

		const FFrameRate StageFrameRate{static_cast<uint32>(UsdStage.GetTimeCodesPerSecond()), 1};

		double LastTimeSample = TNumericLimits<double>::Lowest();
		for (const double UsdTimeSample : UsdTimeSamples)
		{
			// We never want to evaluate the same time twice
			if (FMath::IsNearlyEqual(UsdTimeSample, LastTimeSample))
			{
				continue;
			}
			LastTimeSample = UsdTimeSample;

			int32 FrameNumber = FMath::FloorToInt(UsdTimeSample);
			float SubFrameNumber = UsdTimeSample - FrameNumber;

			FFrameTime FrameTime{FrameNumber, SubFrameNumber};
			double FrameTimeSeconds = static_cast<float>(StageFrameRate.AsSeconds(FrameTime));

			bool UEValue = ReaderFunc(UsdTimeSample);

			KeyTimes.Add(FrameTimeSeconds);
			BooleanKeyValues.Add(UEValue);
		}

		return true;
	}

	bool ReadFloats(
		const UE::FUsdStage& UsdStage,
		const TArray<double>& UsdTimeSamples,
		const TFunction<float(double)>& ReaderFunc,
		Interchange::FAnimationPayloadData& OutPayloadData
	)
	{
		OutPayloadData.Curves.SetNum(1);
		FRichCurve& Curve = OutPayloadData.Curves[0];

		const FFrameRate StageFrameRate{static_cast<uint32>(UsdStage.GetTimeCodesPerSecond()), 1};
		const ERichCurveInterpMode InterpMode = (UsdStage.GetInterpolationType() == EUsdInterpolationType::Linear)
													? ERichCurveInterpMode::RCIM_Linear
													: ERichCurveInterpMode::RCIM_Constant;

		double LastTimeSample = TNumericLimits<double>::Lowest();
		for (const double UsdTimeSample : UsdTimeSamples)
		{
			// We never want to evaluate the same time twice
			if (FMath::IsNearlyEqual(UsdTimeSample, LastTimeSample))
			{
				continue;
			}
			LastTimeSample = UsdTimeSample;

			int32 FrameNumber = FMath::FloorToInt(UsdTimeSample);
			float SubFrameNumber = UsdTimeSample - FrameNumber;

			FFrameTime FrameTime{FrameNumber, SubFrameNumber};
			double FrameTimeSeconds = static_cast<float>(StageFrameRate.AsSeconds(FrameTime));

			float UEValue = ReaderFunc(UsdTimeSample);

			FKeyHandle Handle = Curve.AddKey(FrameTimeSeconds, UEValue);
			Curve.SetKeyInterpMode(Handle, InterpMode);
		}

		return true;
	}

	bool ReadColors(
		const UE::FUsdStage& UsdStage,
		const TArray<double>& UsdTimeSamples,
		const TFunction<FLinearColor(double)>& ReaderFunc,
		Interchange::FAnimationPayloadData& OutPayloadData
	)
	{
		OutPayloadData.Curves.SetNum(4);
		FRichCurve& RCurve = OutPayloadData.Curves[0];
		FRichCurve& GCurve = OutPayloadData.Curves[1];
		FRichCurve& BCurve = OutPayloadData.Curves[2];
		FRichCurve& ACurve = OutPayloadData.Curves[3];

		const FFrameRate StageFrameRate{static_cast<uint32>(UsdStage.GetTimeCodesPerSecond()), 1};
		const ERichCurveInterpMode InterpMode = (UsdStage.GetInterpolationType() == EUsdInterpolationType::Linear)
													? ERichCurveInterpMode::RCIM_Linear
													: ERichCurveInterpMode::RCIM_Constant;

		double LastTimeSample = TNumericLimits<double>::Lowest();
		for (const double UsdTimeSample : UsdTimeSamples)
		{
			// We never want to evaluate the same time twice
			if (FMath::IsNearlyEqual(UsdTimeSample, LastTimeSample))
			{
				continue;
			}
			LastTimeSample = UsdTimeSample;

			int32 FrameNumber = FMath::FloorToInt(UsdTimeSample);
			float SubFrameNumber = UsdTimeSample - FrameNumber;

			FFrameTime FrameTime{FrameNumber, SubFrameNumber};
			double FrameTimeSeconds = static_cast<float>(StageFrameRate.AsSeconds(FrameTime));

			FLinearColor UEValue = ReaderFunc(UsdTimeSample);

			FKeyHandle RHandle = RCurve.AddKey(FrameTimeSeconds, UEValue.R);
			FKeyHandle GHandle = GCurve.AddKey(FrameTimeSeconds, UEValue.G);
			FKeyHandle BHandle = BCurve.AddKey(FrameTimeSeconds, UEValue.B);
			FKeyHandle AHandle = ACurve.AddKey(FrameTimeSeconds, UEValue.A);

			RCurve.SetKeyInterpMode(RHandle, InterpMode);
			GCurve.SetKeyInterpMode(GHandle, InterpMode);
			BCurve.SetKeyInterpMode(BHandle, InterpMode);
			ACurve.SetKeyInterpMode(AHandle, InterpMode);
		}

		return true;
	}

	bool ReadTransforms(
		const UE::FUsdStage& UsdStage,
		const TArray<double>& UsdTimeSamples,
		const TFunction<FTransform(double)>& ReaderFunc,
		Interchange::FAnimationPayloadData& OutPayloadData
	)
	{
		OutPayloadData.Curves.SetNum(9);
		FRichCurve& TransXCurve = OutPayloadData.Curves[0];
		FRichCurve& TransYCurve = OutPayloadData.Curves[1];
		FRichCurve& TransZCurve = OutPayloadData.Curves[2];
		FRichCurve& RotXCurve = OutPayloadData.Curves[3];
		FRichCurve& RotYCurve = OutPayloadData.Curves[4];
		FRichCurve& RotZCurve = OutPayloadData.Curves[5];
		FRichCurve& ScaleXCurve = OutPayloadData.Curves[6];
		FRichCurve& ScaleYCurve = OutPayloadData.Curves[7];
		FRichCurve& ScaleZCurve = OutPayloadData.Curves[8];

		const FFrameRate StageFrameRate{static_cast<uint32>(UsdStage.GetTimeCodesPerSecond()), 1};
		const ERichCurveInterpMode InterpMode = (UsdStage.GetInterpolationType() == EUsdInterpolationType::Linear)
													? ERichCurveInterpMode::RCIM_Linear
													: ERichCurveInterpMode::RCIM_Constant;

		double LastTimeSample = TNumericLimits<double>::Lowest();
		for (const double UsdTimeSample : UsdTimeSamples)
		{
			// We never want to evaluate the same time twice
			if (FMath::IsNearlyEqual(UsdTimeSample, LastTimeSample))
			{
				continue;
			}
			LastTimeSample = UsdTimeSample;

			int32 FrameNumber = FMath::FloorToInt(UsdTimeSample);
			float SubFrameNumber = UsdTimeSample - FrameNumber;

			FFrameTime FrameTime{FrameNumber, SubFrameNumber};
			double FrameTimeSeconds = static_cast<float>(StageFrameRate.AsSeconds(FrameTime));

			FTransform UEValue = ReaderFunc(UsdTimeSample);
			FVector Location = UEValue.GetLocation();
			FRotator Rotator = UEValue.Rotator();
			FVector Scale = UEValue.GetScale3D();

			FKeyHandle HandleTransX = TransXCurve.AddKey(FrameTimeSeconds, Location.X);
			FKeyHandle HandleTransY = TransYCurve.AddKey(FrameTimeSeconds, Location.Y);
			FKeyHandle HandleTransZ = TransZCurve.AddKey(FrameTimeSeconds, Location.Z);
			FKeyHandle HandleRotX = RotXCurve.AddKey(FrameTimeSeconds, Rotator.Roll);
			FKeyHandle HandleRotY = RotYCurve.AddKey(FrameTimeSeconds, Rotator.Pitch);
			FKeyHandle HandleRotZ = RotZCurve.AddKey(FrameTimeSeconds, Rotator.Yaw);
			FKeyHandle HandleScaleX = ScaleXCurve.AddKey(FrameTimeSeconds, Scale.X);
			FKeyHandle HandleScaleY = ScaleYCurve.AddKey(FrameTimeSeconds, Scale.Y);
			FKeyHandle HandleScaleZ = ScaleZCurve.AddKey(FrameTimeSeconds, Scale.Z);

			TransXCurve.SetKeyInterpMode(HandleTransX, InterpMode);
			TransYCurve.SetKeyInterpMode(HandleTransY, InterpMode);
			TransZCurve.SetKeyInterpMode(HandleTransZ, InterpMode);
			RotXCurve.SetKeyInterpMode(HandleRotX, InterpMode);
			RotYCurve.SetKeyInterpMode(HandleRotY, InterpMode);
			RotZCurve.SetKeyInterpMode(HandleRotZ, InterpMode);
			ScaleXCurve.SetKeyInterpMode(HandleScaleX, InterpMode);
			ScaleYCurve.SetKeyInterpMode(HandleScaleY, InterpMode);
			ScaleZCurve.SetKeyInterpMode(HandleScaleZ, InterpMode);
		}

		return true;
	}

	bool GetAnimationCurvePayloadData(const UE::FUsdStage& UsdStage, const FString& PayloadKey, Interchange::FAnimationPayloadData& OutPayloadData)
	{
		FString PrimPath;
		FString UEPropertyNameStr;
		bool bSplit = PayloadKey.Split(TEXT("\\"), &PrimPath, &UEPropertyNameStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (!bSplit)
		{
			return false;
		}

		UE::FUsdPrim Prim = UsdStage.GetPrimAtPath(UE::FSdfPath{*PrimPath});
		FName UEPropertyName = *UEPropertyNameStr;
		if (!Prim || UEPropertyName == NAME_None)
		{
			return false;
		}

		TArray<double> TimeSampleUnion;
		TArray<UE::FUsdAttribute> Attrs = UsdUtils::GetAttributesForProperty(Prim, UEPropertyName);
		bool bSuccess = UE::FUsdAttribute::GetUnionedTimeSamples(Attrs, TimeSampleUnion);
		if (!bSuccess)
		{
			return false;
		}

		const bool bIgnorePrimLocalTransform = false;
		UsdToUnreal::FPropertyTrackReader Reader = UsdToUnreal::CreatePropertyTrackReader(Prim, UEPropertyName, bIgnorePrimLocalTransform);
		if (Reader.BoolReader)
		{
			return ReadBools(UsdStage, TimeSampleUnion, Reader.BoolReader, OutPayloadData);
		}
		else if (Reader.ColorReader)
		{
			return ReadColors(UsdStage, TimeSampleUnion, Reader.ColorReader, OutPayloadData);
		}
		else if (Reader.FloatReader)
		{
			return ReadFloats(UsdStage, TimeSampleUnion, Reader.FloatReader, OutPayloadData);
		}
		else if (Reader.TransformReader)
		{
			return ReadTransforms(UsdStage, TimeSampleUnion, Reader.TransformReader, OutPayloadData);
		}

		return false;
	}

}	 // namespace UE::InterchangeUsdTranslator::Private
#endif	  // USE_USD_SDK

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
	ImplPtr->CurrentTrackSet = nullptr;

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

	// Apply stage settings
	if (Settings)
	{
		// Apply coordinate system conversion to the stage if we have one
		if (Settings->bOverrideStageOptions)
		{
			UsdUtils::SetUsdStageMetersPerUnit(ImplPtr->UsdStage, Settings->StageOptions.MetersPerUnit);
			UsdUtils::SetUsdStageUpAxis(ImplPtr->UsdStage, Settings->StageOptions.UpAxis);
		}

		ImplPtr->UsdStage.SetInterpolationType(Settings->InterpolationType);
	}

	// Cache these so we don't have to keep converting these tokens over and over during translation
	FUsdMeshConversionOptions& MeshOptions = ImplPtr->CachedMeshConversionOptions;
	MeshOptions.PurposesToLoad = (EUsdPurpose)Settings->GeometryPurpose;
	MeshOptions.RenderContext = Settings->RenderContext.IsNone() ? pxr::UsdShadeTokens->universalRenderContext
																 : UnrealToUsd::ConvertToken(*Settings->RenderContext.ToString()).Get();
	MeshOptions.MaterialPurpose = Settings->MaterialPurpose.IsNone() ? pxr::UsdShadeTokens->allPurpose
																	 : UnrealToUsd::ConvertToken(*Settings->MaterialPurpose.ToString()).Get();

	// Traverse stage and emit translated nodes
	FTraversalInfo Info;
	for (const FUsdPrim& Prim : ImplPtr->UsdStage.GetPseudoRoot().GetChildren())
	{
		Traverse(Prim, *ImplPtr, NodeContainer, Info);
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
	ImplPtr->CurrentTrackSet = nullptr;

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
					bSuccess = UE::InterchangeUsdTranslator::Private::GetStaticMeshPayloadData(
						PayloadKey.UniqueId,
						*ImplPtr,
						OptionsCopy,
						MeshPayloadData.MeshDescription
					);
					break;
				}
				case EInterchangeMeshPayLoadType::SKELETAL:
				{
					bSuccess = UE::InterchangeUsdTranslator::Private::GetSkeletalMeshPayloadData(
						PayloadKey.UniqueId,
						*ImplPtr,
						OptionsCopy,
						MeshPayloadData.MeshDescription,
						MeshPayloadData.JointNames
					);
					break;
				}
				case EInterchangeMeshPayLoadType::MORPHTARGET:
				{
					bSuccess = UE::InterchangeUsdTranslator::Private::GetMorphTargetPayloadData(
						PayloadKey.UniqueId,
						*ImplPtr,
						OptionsCopy,
						MeshPayloadData.MeshDescription,
						MeshPayloadData.MorphTargetName
					);
					break;
				}
				case EInterchangeMeshPayLoadType::NONE:	   // Fallthrough
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

TFuture<TOptional<UE::Interchange::FAnimationPayloadData>> UInterchangeUSDTranslator::ResolveAnimationPayloadQuery(
	const UE::Interchange::FAnimationPayloadQuery& PayloadQuery
) const
{
	using namespace UE::Interchange;
	using namespace UE::InterchangeUsdTranslator::Private;

	return Async(
		EAsyncExecution::TaskGraph,
		[this, PayloadQuery]
		{
			TOptional<FAnimationPayloadData> Result;

#if USE_USD_SDK
			FAnimationPayloadData AnimationPayLoadData{PayloadQuery.SceneNodeUniqueID, PayloadQuery.PayloadKey};

			UInterchangeUSDTranslatorImpl* ImplPtr = Impl.Get();
			if (!ImplPtr)
			{
				return Result;
			}

			switch (PayloadQuery.PayloadKey.Type)
			{
				case EInterchangeAnimationPayLoadType::CURVE:	 // Fallthrough
				case EInterchangeAnimationPayLoadType::STEPCURVE:
				{
					if (GetAnimationCurvePayloadData(ImplPtr->UsdStage, PayloadQuery.PayloadKey.UniqueId, AnimationPayLoadData))
					{
						Result.Emplace(AnimationPayLoadData);
					}
					break;
				}
				case EInterchangeAnimationPayLoadType::MORPHTARGETCURVE:
				case EInterchangeAnimationPayLoadType::BAKED:
				case EInterchangeAnimationPayLoadType::NONE:
				default:
				{
					break;
				}
			}
#endif	  // USE_USD_SDK

			return Result;
		}
	);
}

TArray<UE::Interchange::FAnimationPayloadData> UInterchangeUSDTranslator::GetAnimationPayloadData(
	const TArray<UE::Interchange::FAnimationPayloadQuery>& PayloadQueries
) const
{
	// Note: This TFuture/Async approach is a bit overkill when it comes to the property track case, as each
	// individual PayloadQueries has a single item in it (check FLevelSequenceHelper::PopulateAnimationTrack).
	// This will actually help a lot with AnimSequences though, where RetrieveAnimationPayloads shows how all
	// the skeletal joint tracks are queried in a single call (and the analogous for the morph target curves).

	TArray<TFuture<TOptional<UE::Interchange::FAnimationPayloadData>>> AnimationPayloadFutures;
	for (const UE::Interchange::FAnimationPayloadQuery& PayloadQuery : PayloadQueries)
	{
		AnimationPayloadFutures.Add(ResolveAnimationPayloadQuery(PayloadQuery));
	}

	TArray<UE::Interchange::FAnimationPayloadData> AnimationPayloads;
	for (TFuture<TOptional<UE::Interchange::FAnimationPayloadData>>& AnimationPayloadFuture : AnimationPayloadFutures)
	{
		TOptional<UE::Interchange::FAnimationPayloadData> OptionalPayloadData = AnimationPayloadFuture.Get();
		if (!OptionalPayloadData.IsSet())
		{
			continue;
		}
		AnimationPayloads.Add(OptionalPayloadData.GetValue());
	}

	return AnimationPayloads;
}

#undef LOCTEXT_NAMESPACE
