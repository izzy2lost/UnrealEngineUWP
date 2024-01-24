// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/StateMachine.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "MuR/Types.h"

#if WITH_EDITOR
#include "Misc/Guid.h"
#endif

#include "CustomizableObjectPrivate.generated.h"

namespace mu { class Model; }
class UCustomizableObject;
class USkeletalMesh;
class USkeleton;
class UPhysicsAsset;
class UMaterialInterface;
class UTexture;
class UAnimInstance;
class UAssetUserData;

class FMeshCache
{
public:
	USkeletalMesh* Get(const TArray<mu::FResourceID>& Key);

	void Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value);

private:
	TMap<TArray<mu::FResourceID>, TWeakObjectPtr<USkeletalMesh>> GeneratedMeshes;
};


class FSkeletonCache
{
public:
	USkeleton* Get(const TArray<uint16>& Key);

	void Add(const TArray<uint16>& Key, USkeleton* Value);

private:
	TMap<TArray<uint16>, TWeakObjectPtr<USkeleton>> MergedSkeletons;
};


struct FCustomizableObjectStatusTypes
{
	enum class EState : uint8
	{
		Loading = 0, // Waiting for PostLoad and Asset Registry to finish.
		ModelLoaded, // Model loaded correctly.
		NoModel, // No model (due to no model not found and automatic compilations disabled).
		// Compiling, // Compiling the CO.

		Count,
	};
	
	static constexpr EState StartState = EState::Loading;

	static constexpr bool ValidTransitions[3][3] =
	{
		// TO
		// Loading, ModelLoaded, NoModel // FROM
		{false,   true,        true},  // Loading
		{false,   true,        true},  // ModelLoaded
		{false,   true,        true},  // NoModel
	};
};

using FCustomizableObjectStatus = FStateMachine<FCustomizableObjectStatusTypes>;


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableModelImageProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelImageProperties()
		: Filter(TF_Default)
		, SRGB(0)
		, FlipGreenChannel(0)
		, IsPassThrough(0)
		, LODBias(0)
		, LODGroup(TEXTUREGROUP_World)
		, AddressX(TA_Clamp)
		, AddressY(TA_Clamp)
	{}

	FMutableModelImageProperties(const FString& InTextureParameterName, TextureFilter InFilter, uint32 InSRGB,
		uint32 InFlipGreenChannel, uint32 bInIsPassThrough, int32 InLODBias, TEnumAsByte<enum TextureGroup> InLODGroup,
		TEnumAsByte<enum TextureAddress> InAddressX, TEnumAsByte<enum TextureAddress> InAddressY)
		: TextureParameterName(InTextureParameterName)
		, Filter(InFilter)
		, SRGB(InSRGB)
		, FlipGreenChannel(InFlipGreenChannel)
		, IsPassThrough(bInIsPassThrough)
		, LODBias(InLODBias)
		, LODGroup(InLODGroup)
		, AddressX(InAddressX)
		, AddressY(InAddressY)
	{}

	// Name in the material.
	UPROPERTY()
	FString TextureParameterName;

	UPROPERTY()
	TEnumAsByte<enum TextureFilter> Filter;

	UPROPERTY()
	uint32 SRGB : 1;

	UPROPERTY()
	uint32 FlipGreenChannel : 1;

	UPROPERTY()
	uint32 IsPassThrough : 1;

	UPROPERTY()
	int32 LODBias;

	UPROPERTY()
	TEnumAsByte<enum TextureGroup> LODGroup;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressY;

	bool operator!=(const FMutableModelImageProperties& rhs) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableModelImageProperties& ImageProps);
#endif
};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableRefSocket
{
	GENERATED_BODY()

	UPROPERTY()
	FName SocketName;
	UPROPERTY()
	FName BoneName;

	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;
	UPROPERTY()
	FVector RelativeScale = FVector::ZeroVector;

	UPROPERTY()
	bool bForceAlwaysAnimated = false;

	// When two sockets have the same name, the one with higher priority will be picked and the other discarded
	UPROPERTY()
	int32 Priority = -1;

	bool operator ==(const FMutableRefSocket& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSocket& Data);
#endif
};


USTRUCT()
struct FMutableRefLODInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float ScreenSize = 0.f;

	UPROPERTY()
	float LODHysteresis = 0.f;

	UPROPERTY()
	bool bSupportUniformlyDistributedSampling = false;

	UPROPERTY()
	bool bAllowCPUAccess = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODInfo& Data);
#endif
};


USTRUCT()
struct FMutableRefLODRenderData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsLODOptional = false;

	UPROPERTY()
	bool bStreamedDataInlined = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODRenderData& Data);
#endif
};


USTRUCT()
struct FMutableRefLODData
{
	GENERATED_BODY()

	UPROPERTY()
	FMutableRefLODInfo LODInfo;

	UPROPERTY()
	FMutableRefLODRenderData RenderData;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODData& Data);
#endif
};


USTRUCT()
struct FMutableRefSkeletalMeshSettings
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnablePerPolyCollision = false;

	UPROPERTY()
	float DefaultUVChannelDensity = 0.f;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshSettings& Data);
#endif
};


USTRUCT()
struct FMutableRefAssetUserData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAssetUserData> AssetUserData;

#if WITH_EDITORONLY_DATA
	FString ClassPath;
	TArray<uint8> Bytes;

	friend FArchive& operator<<(FArchive& Ar, FMutableRefAssetUserData& Data);

	void InitResources(UCustomizableObject* InOuter);
#endif

};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableRefSkeletalMeshData
{
	GENERATED_BODY()

	// Reference Skeletal Mesh
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// Path to load the ReferenceSkeletalMesh
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> SoftSkeletalMesh;

	// LOD info
	UPROPERTY()
	TArray<FMutableRefLODData> LODData;

	// Sockets
	UPROPERTY()
	TArray<FMutableRefSocket> Sockets;

	// Bounding Box
	UPROPERTY()
	FBoxSphereBounds Bounds = FBoxSphereBounds(ForceInitToZero);

	// Settings
	UPROPERTY()
	FMutableRefSkeletalMeshSettings Settings;

	// Skeleton
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	// PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	// Post Processing AnimBP
	UPROPERTY()
	TSoftClassPtr<UAnimInstance> PostProcessAnimInst;

	// Shadow PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> ShadowPhysicsAsset;

	// Asset user data
	UPROPERTY()
	TArray<FMutableRefAssetUserData> AssetUserData;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshData& Data);

	void InitResources(UCustomizableObject* InOuter, const ITargetPlatform* InTargetPlatform);
#endif

};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FAnimBpOverridePhysicsAssetsInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<UAnimInstance> AnimInstanceClass;

	UPROPERTY()
	TSoftObjectPtr<UPhysicsAsset> SourceAsset;

	UPROPERTY()
	int32 PropertyIndex = -1;

	bool operator==(const FAnimBpOverridePhysicsAssetsInfo& Rhs) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FAnimBpOverridePhysicsAssetsInfo& Info);
#endif
};


USTRUCT()
struct CUSTOMIZABLEOBJECT_API FMutableSkinWeightProfileInfo
{
	GENERATED_USTRUCT_BODY()

	FMutableSkinWeightProfileInfo() {};

	FMutableSkinWeightProfileInfo(FName InName, bool InDefaultProfile, int8 InDefaultProfileFromLODIndex) : Name(InName),
	DefaultProfile(InDefaultProfile), DefaultProfileFromLODIndex(InDefaultProfileFromLODIndex) {};

	UPROPERTY()
	FName Name;

	UPROPERTY()
	bool DefaultProfile = false;

	UPROPERTY(meta = (ClampMin = 0))
	int8 DefaultProfileFromLODIndex = 0;

	bool operator==(const FMutableSkinWeightProfileInfo& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableSkinWeightProfileInfo& Info);
#endif
};


// Referenced materials, skeletons, passthrough textures...
USTRUCT()
struct FModelResources
{
	GENERATED_BODY()

	/** All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	 * properties for everything that Mutable doesn't create or modify. This struct stores the information used from
	 * the Reference Skeletal Meshes to avoid having them loaded at all times. This includes data like LOD distances,
	 * LOD render data settings, Mesh sockets, Bounding volumes, etc.
	 */
	UPROPERTY()
	TArray<FMutableRefSkeletalMeshData> ReferenceSkeletalMeshesData;

	/** Skeletons used by the compiled mu::Model. */
	UPROPERTY()
	TArray<TSoftObjectPtr<USkeleton>> Skeletons;

	/** Materials used by the compiled mu::Model */
	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/** PassThrough Textures used by the mu::Model. */
	UPROPERTY()
	TArray<TSoftObjectPtr<UTexture>> PassThroughTextures;

	/** Physics assets gathered from the SkeletalMeshes, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftObjectPtr<UPhysicsAsset>> PhysicsAssets;

	/** UAnimBlueprint assets gathered from the SkeletalMesh, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftClassPtr<UAnimInstance>> AnimBPs;

	/** */
	UPROPERTY()
	TArray<FAnimBpOverridePhysicsAssetsInfo> AnimBpOverridePhysiscAssetsInfo;

	/** Material slot names for the materials referenced by the surfaces. */
	UPROPERTY()
	TArray<FName> MaterialSlotNames;

	/** Bone names of all the bones that can possibly use the generated meshes */
	UPROPERTY()
	TArray<FName> BoneNames;

	/** Mesh sockets provided by the part skeletal meshes, to be merged in the generated meshes */
	UPROPERTY()
	TArray<FMutableRefSocket> SocketArray;

	UPROPERTY()
	TArray<FMutableSkinWeightProfileInfo> SkinWeightProfilesInfo;

	UPROPERTY()
	TArray<FMutableModelImageProperties> ImageProperties;

	/** Parameter UI metadata information for all the dependencies of this Customizable Object. */
	UPROPERTY()
	TMap<FString, FParameterUIData> ParameterUIDataMap;

	/** State UI metadata information for all the dependencies of this Customizable Object */
	UPROPERTY()
	TMap<FString, FParameterUIData> StateUIDataMap;
};


UCLASS()
class UCustomizableObjectPrivate : public UObject
{
	GENERATED_BODY()

private:

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> MutableModel;

	/** Stores resources to be used by MutableModel In-Game. Cooked resources. */
	UPROPERTY()
	FModelResources ModelResources;

#if WITH_EDITORONLY_DATA
	/** Stores resources to be used by MutableModel in the Editor. Editor resources.
	 * Editor-Only to avoid packaging assets referenced by editor compilations. */
	UPROPERTY(Transient)
	FModelResources ModelResourcesEditor;
#endif

public:
	void SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& GetModel();
	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> GetModel() const;

	const FModelResources& GetModelResources() const;

#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API FModelResources& GetModelResources(bool bIsCooking);
#endif

	/** Cache of generated SkeletalMeshes */
	FMeshCache MeshCache;

	/** Cache of merged Skeletons */
	FSkeletonCache SkeletonCache;

	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITORONLY_DATA

	/** Unique Identifier - Deterministic. Used to locate Model and Streamable data on disk. Should not be modified. */
	FGuid Identifier;

	/** Cooked platform names. */
	TArray<FString> CachedPlatformNames;

	/** List of external packages that if changed, a compilation is required.
	 * Key is the package name. Value is the the UPackage::Guid, which is regenerated each time the packages is saved.
	 *
	 * Updated each time the CO is compiled and saved in the Derived Data. */
	TMap<FName, FGuid> ParticipatingObjects;
#endif

	FCustomizableObjectStatus Status;
};

