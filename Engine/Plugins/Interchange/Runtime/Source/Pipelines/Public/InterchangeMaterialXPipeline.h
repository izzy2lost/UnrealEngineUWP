// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include "Engine/DeveloperSettings.h"
#include "Templates/Tuple.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "InterchangeMaterialXPipeline.generated.h"

class UInterchangeDatasmithPbrMaterialNode;
class UInterchangeFactoryBaseNode;
class UInterchangeMaterialFactoryNode;
class UInterchangeMaterialInstanceFactoryNode;
class UInterchangeShaderGraphNode;
class UInterchangeMaterialInstanceNode;

class UMaterialFunction;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EInterchangeMaterialXShaders : uint8
{
	/** Default settings for Autodesk's Standard Surface shader	*/
	StandardSurface,

	/** Standard Surface shader used for translucency */
	StandardSurfaceTransmission,

	/** Shader used for unlit surface*/
	SurfaceUnlit,

	/** Default settings for USD's Surface shader */
	UsdPreviewSurface,

	/** Construct a surface shader from scattering and emission distribution functions.*/
	Surface,

	MaxShaderCount UMETA(hidden)
};

#define INTERCHANGE_MATERIALX_EXPERIEMENTAL

UENUM(BlueprintType)
/** Data type representing a Bidirectional Scattering Distribution Function. */
enum class EInterchangeMaterialXBSDF : uint8
{
	/** A BSDF node for diffuse reflections. */
	OrenNayarDiffuse,

	/** A BSDF node for Burley diffuse reflections. */
	BurleyDiffuse,

	/** A BSDF node for pure diffuse transmission. */
	Translucent,

	/** A reflection/transmission BSDF node based on a microfacet model and a Fresnel curve for dielectrics. */
	Dielectric,

	/** A reflection BSDF node based on a microfacet model and a Fresnel curve for conductors/metals. */
	Conductor,

	/** A reflection/transmission BSDF node based on a microfacet model and a generalized Schlick Fresnel curve. */
	GeneralizedSchlick,

	/** A subsurface scattering BSDF for true subsurface scattering. */
	Subsurface,

	/** A microfacet BSDF for the back-scattering properties of cloth-like materials. */
	Sheen,

	/** Adds an iridescent thin film layer over a microfacet base BSDF. */
	ThinFilm,

	MaxBSDFCount UMETA(hidden)
};

UENUM(BlueprintType)
/** Data type representing an Emission Distribution Function. */
enum class EInterchangeMaterialXEDF : uint8
{
	/** An EDF node for uniform emission. */
	Uniform,

	/** Constructs an EDF emitting light inside a cone around the normal direction. */
	Conical,

	/** Constructs an EDF emitting light according to a measured IES light profile. */
	Measured,

	MaxEDFCount UMETA(hidden)
};

UENUM(BlueprintType)
/** Data type representing a Volume Distribution Function. */
enum class EInterchangeMaterialXVDF : uint8
{
	/** Constructs a VDF for pure light absorption. */
	Absorption,

	/** Constructs a VDF scattering light for a participating medium, based on the Henyey-Greenstein phase function. */
	Anisotropic,

	MaxVDFCount UMETA(hidden)
};

using EMaterialXSettings = TVariant<EInterchangeMaterialXShaders, EInterchangeMaterialXBSDF, EInterchangeMaterialXEDF, EInterchangeMaterialXVDF>;

uint32 INTERCHANGEPIPELINES_API GetTypeHash(EMaterialXSettings Key);

bool INTERCHANGEPIPELINES_API operator==(EMaterialXSettings Lhs, EMaterialXSettings Rhs);

UCLASS(config = Interchange, meta = (DisplayName = "Interchange MaterialX"))
class INTERCHANGEPIPELINES_API UMaterialXPipelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMaterialXPipelineSettings();

	bool AreRequiredPackagesLoaded();

	FString GetAssetPathString(EMaterialXSettings EnumType) const;

	template<typename EnumT>
	FString GetAssetPathString(EnumT EnumValue) const
	{
		static_assert(std::is_same_v<EnumT, EInterchangeMaterialXShaders> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXBSDF> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXEDF> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXVDF>,
					  "Enum type not supported");

		return GetAssetPathString(EMaterialXSettings{ TInPlaceType<EnumT>{}, EnumValue });
	}

	UPROPERTY(EditAnywhere, config, Category = "MaterialXPredefined | Surface Shaders", meta = (DisplayName = "MaterialX Predefined Surface Shaders"))
	TMap<EInterchangeMaterialXShaders, FSoftObjectPath> PredefinedSurfaceShaders;

	UPROPERTY(EditAnywhere, config, Category = "MaterialXPredefined | BSDF", meta = (DisplayName = "MaterialX Predefined BSDF"))
	TMap<EInterchangeMaterialXBSDF, FSoftObjectPath> PredefinedBSDF;

	UPROPERTY(EditAnywhere, config, Category = "MaterialXPredefined | EDF", meta = (DisplayName = "MaterialX Predefined EDF"))
	TMap<EInterchangeMaterialXEDF, FSoftObjectPath> PredefinedEDF;

	UPROPERTY(EditAnywhere, config, Category = "MaterialXPredefined | VDF", meta = (DisplayName = "MaterialX Predefined VDF"))
	TMap<EInterchangeMaterialXVDF, FSoftObjectPath> PredefinedVDF;

#if WITH_EDITOR
	/** Init the Predefined with Substrate assets, since the default value is set in BaseInterchange.ini and we have no way in the config file to conditionally init a property*/
	void InitPredefinedAssets();

private:
	friend class FInterchangeMaterialXPipelineSettingsCustomization;
	friend class UInterchangeMaterialXPipeline;

	using FMaterialXSettings = TMap<EMaterialXSettings, TPair<TSet<FName>, TSet<FName>>>;

	static bool ShouldFilterAssets(UMaterialFunction* Asset, const TSet<FName>& Inputs, const TSet<FName>& Outputs);

	static EMaterialXSettings ToEnumKey(uint8 EnumType, uint8 EnumValue);

	template<typename EnumT>
	static EMaterialXSettings ToEnumKey(EnumT EnumValue)
	{
		static_assert(std::is_same_v<EnumT, EInterchangeMaterialXShaders> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXBSDF> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXEDF> ||
					  std::is_same_v<EnumT, EInterchangeMaterialXVDF>,
					  "Enum type not supported");

		return EMaterialXSettings{ TInPlaceType<EnumT>{}, EnumValue };
	}

	/** The key is a combination of the index in the variant + the corresponding enum */
	static FMaterialXSettings SettingsInputsOutputs;

	static constexpr uint8 IndexSurfaceShaders = 0;
	static constexpr uint8 IndexBSDF = 1;
	static constexpr uint8 IndexEDF = 2;
	static constexpr uint8 IndexVDF = 3;

	bool bIsSubstrateEnabled{ false };
#endif // WITH_EDITOR
};

UCLASS(BlueprintType)
class INTERCHANGEPIPELINES_API UInterchangeMaterialXPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

	UInterchangeMaterialXPipeline();

public:
	TObjectPtr<UMaterialXPipelineSettings> MaterialXSettings;

protected:
	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas, const FString& ContentBasePath) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		// This pipeline creates UObjects and assets. Not safe to execute outside of main thread.
		return true;
	}

private:

	static TMap<FString, EMaterialXSettings> PathToEnumMapping;
};
