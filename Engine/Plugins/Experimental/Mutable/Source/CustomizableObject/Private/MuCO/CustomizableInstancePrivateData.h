// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"

#include "GameplayTagContainer.h"

#include "UObject/Package.h"
#include "CustomizableInstancePrivateData.generated.h"

namespace mu 
{
	class PhysicsBody;
	typedef uint64 FResourceID;
}

struct FMutableModelImageProperties;
struct FMutableRefSkeletalMeshData;
struct FMutableImageCacheKey;
struct FStreamableHandle;

/** CustomizableObject Instance flags for internal use  */
enum ECOInstanceFlags
{
	ECONone							= 0,  // Should not use the name None here.. it collides with other enum in global namespace

	// Update process
	CreatingSkeletalMesh			= 1 << 1,	//
	Generated						= 1 << 2,	//
	ReuseTextures					= 1 << 3, 	// 
	ReplacePhysicsAssets			= 1 << 4,	// Merge active PhysicsAssets and replace the base physics asset

	// Update priorities
	UsedByComponent					= 1 << 5,	// If any components are using this instance, they will set flag every frame
	UsedByComponentInPlay			= 1 << 6,	// If any components are using this instance in play, they will set flag every frame
	UsedByPlayerOrNearIt			= 1 << 7,	// The instance is used by the player or is near the player, used to give more priority to its updates
	DiscardedByNumInstancesLimit	= 1 << 8,	// The instance is descarded because we exceeded the limit of instances generated 

	// Types of updates
	PendingLODsUpdate				= 1 << 9,	// Used to queue an update due to a change in LODs required by the instance
	PendingLODsDowngrade			= 1 << 10,	// Used to queue a downgrade update to reduce the number of LODs. LOD update goes from a high res level to a low res one, ex: 0 to 1 or 1 to 2
	
	// Generation
	ForceGenerateMipTail			= 1 << 13,	// If set, SkipGenerateResidentMips will be ignored and the mip tail will be generated
};

ENUM_CLASS_FLAGS(ECOInstanceFlags);


USTRUCT()
struct FGeneratedTexture
{
	GENERATED_USTRUCT_BODY();

	FMutableImageCacheKey Key;

	UPROPERTY(Category = CustomizableObjectInstance, VisibleAnywhere)
	FString Name;

	UPROPERTY(Category = CustomizableObjectInstance, VisibleAnywhere)
	TObjectPtr<UTexture> Texture = nullptr;
};


USTRUCT()
struct FGeneratedMaterial
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> MaterialInterface;

	UPROPERTY(Category = CustomizedMaterial, VisibleAnywhere)
	TArray< FGeneratedTexture > Textures;

	// Surface or SharedSurface Id
	uint32 SurfaceId = 0;

	// Index of the material to instantiate (UCustomizableObject::ReferencedMaterials)
	uint32 MaterialIndex = 0;

	bool operator==(const FGeneratedMaterial& Other) const { return SurfaceId == Other.SurfaceId && MaterialIndex == Other.MaterialIndex; };
};


USTRUCT()
struct FReferencedPhysicsAssets
{
	GENERATED_USTRUCT_BODY();
	
	TArray<FString> PhysicsAssetToLoad;
	
	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > PhysicsAssetsToMerge;

	TArray<int32> AdditionalPhysicsAssetsToLoad;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPhysicsAsset>> AdditionalPhysicsAssets;
};


USTRUCT()
struct FReferencedSkeletons
{
	GENERATED_USTRUCT_BODY();

	// Merged skeleton if found in the cache
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	UPROPERTY()
	TArray<uint16> SkeletonIds;

	UPROPERTY()
	TArray< TObjectPtr<USkeleton> > SkeletonsToMerge;
};


USTRUCT()
struct FCustomizableInstanceComponentData
{
	GENERATED_USTRUCT_BODY();

	// AnimBP data gathered for a component from its constituent meshes
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TMap<FName, TSoftClassPtr<UAnimInstance>> AnimSlotToBP;

	// AssetUserData gathered for a component from its constituent meshes
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TSet<TObjectPtr<UAssetUserData>> AssetUserDataArray;

	// Temp structure to store the AssetUserData that will be async loaded during an update
	TSet<TSoftObjectPtr<UAssetUserData>> AssetUserDataToStream;

#if WITH_EDITORONLY_DATA
	// Just used for mutable.EnableMutableAnimInfoDebugging command
	TArray<FString> MeshPartPaths;
#endif

	/** Skeletons required by the current generated instance. Skeletons to be loaded and merged.*/
	UPROPERTY(Transient)
	FReferencedSkeletons Skeletons;
	
	/** PhysicsAssets required by the current generated instance. PhysicsAssets to be loaded and merged.*/
	UPROPERTY(Transient)
	FReferencedPhysicsAssets PhysicsAssets;

	/** Clothing PhysicsAssets required by the current generated instance. PhysicsAssets to be loaded and merged.*/
	TArray<TPair<int32, FString>> ClothingPhysicsAssetsToStream;

	/** Array of generated MeshIds per each LOD, used to decide if the mesh should be updated or not.
	 *  Size == NumLODsAvailable
	 *  LODs without mesh will be set to the maximum value of FResourceID (Max_uint64). */
	TArray<mu::FResourceID> LastMeshIdPerLOD;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

USTRUCT()
struct FAnimInstanceOverridePhysicsAsset
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	int32 PropertyIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsAsset> PhysicsAsset;
};

USTRUCT()
struct FAnimBpGeneratedPhysicsAssets
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<FAnimInstanceOverridePhysicsAsset> AnimInstancePropertyIndexAndPhysicsAssets;
};

UCLASS()
class UCustomizableInstancePrivateData : public UObject
{
public:
	GENERATED_BODY()

	UCustomizableInstancePrivateData();

	UPROPERTY( Transient )
	TArray<FGeneratedMaterial> GeneratedMaterials;

	UPROPERTY( Transient )
	TArray<FGeneratedTexture> GeneratedTextures;

	// Indices of the parameters that are relevant for the given parameter values.
	// This only gets updated if parameter decorations are generated.
	TArray<int> RelevantParameters;

	// If Texture reuse is enabled, stores which texture is being used in a particular <LODIndex, ComponentIndex, MeshSurfaceIndex, image>
	// \TODO: Create a key based on a struct instead of generating strings dynamically.
	UPROPERTY(Transient)
	TMap<FString, TWeakObjectPtr<UTexture2D>> TextureReuseCache;

	// Handle used to store a streaming request operation if one is ongoing.
	TSharedPtr<FStreamableHandle> StreamingHandle;

	// Only used in LiveUpdateMode to reuse core instances between updates and their temp data to speed up updates, but spend way more memory
	mu::Instance::ID LiveUpdateModeInstanceID = 0;

	/** Cached Texture Parameters from the last update.
	 * This cache is required since the Instance can have a LOD Update at any time.
	 * So we need to make sure that the initially provided Texture Parameters by the user will be available until the user decides to change them. */
	TArray<FName> UpdateTextureParameters;

#if WITH_EDITOR

	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	void OnPostCompile();

#endif

	/** Invalidates the previously generated data and retrieves information from the CObject after specific actions.
	 *  It'll be called in the PostLoad, after Compiling the CO, and after changing the CO of the Instance. */
	void InitCustomizableObjectData(const UCustomizableObject* InCustomizableObject);
	
	FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex);
	const FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex) const;

	ECOInstanceFlags GetCOInstanceFlags() const { return InstanceFlagsPrivate; }
	void SetCOInstanceFlags(ECOInstanceFlags FlagsToSet) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate | FlagsToSet); }
	void ClearCOInstanceFlags(ECOInstanceFlags FlagsToClear) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate & ~FlagsToClear); }
	bool HasCOInstanceFlags(ECOInstanceFlags FlagsToCheck) const { return (InstanceFlagsPrivate & FlagsToCheck) != 0; }

	void BuildMaterials(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObjectInstance* Public);

	void ReuseTexture(UTexture2D* Texture, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData);

	// Return an event that will be fired when the assets  have been loaded. It returns null if no asset needs loading.
	FGraphEventRef LoadAdditionalAssetsAsync(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObjectInstance* Public, struct FStreamableManager &StreamableManager);
	void AdditionalAssetsAsyncLoaded(UCustomizableObjectInstance* Public);

	void TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Publics, FMutableInstanceUpdateMap& InOutRequestedUpdates);
	void UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates);

	// Returns true if success (?)
	bool UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData);

	static void ReleaseMutableTexture(const FMutableImageCacheKey& MutableTextureKey, UTexture2D* Texture, struct FMutableResourceCache& Cache);

	// Prepare the data for the unreal textures, but don't create them because
	// it runs in the mutable thread
	// \TODO: CustomizableObject shouldn't be here
	static void ProcessTextureCoverageQueries(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObject* CustomizableObject, const FString& ImageKeyName, FTexturePlatformData *PlatformData, UMaterialInterface* Material);

	// Copy data generated in the mutable thread over to the instance and initializes additional data required during the update
	void PrepareForUpdate(const TSharedRef<FUpdateContextPrivate>& OperationData);

	int32 GetNumLODsAvailable() const { return NumLODsAvailable; }
	
	// The following method is basically copied from PostEditChangeProperty and/or SkeletalMesh.cpp to be able to replicate PostEditChangeProperty without the editor
	void PostEditChangePropertyWithoutEditor(USkeletalMesh* SkeletalMesh);
	
	void DiscardResourcesAndSetReferenceSkeletalMesh(UCustomizableObjectInstance* Instance);

	// Releases all the mutable resources this instance holds, should only be called when it is not going to be used any more.
	void ReleaseMutableResources(bool bCalledFromBeginDestroy, const UCustomizableObjectInstance& Instance);

	const TArray<FAnimInstanceOverridePhysicsAsset>* GetGeneratedPhysicsAssetsForAnimInstance(TSubclassOf<UAnimInstance> AnimInstance) const;

	/** 
	* \param OnlyLOD: If not 0, extract and convert only one single LOD from the source image.
	* \param ExtractChannel: If different than -1, extract a single-channel image with the specified source channel data.
	*/
	CUSTOMIZABLEOBJECT_API static void ConvertImage(class UTexture2D* Texture, mu::ImagePtrConst MutableImage, const FMutableModelImageProperties& Props, int32 OnlyLOD=-1, int32 ExtractChannel=-1);

	/** Set OnlyLOD to -1 to generate all mips */
	CUSTOMIZABLEOBJECT_API static FTexturePlatformData* MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);

	/** */
#if WITH_EDITORONLY_DATA
	static void RegenerateImportedModel(USkeletalMesh* SkeletalMesh);
#endif

private:

	void InitSkeletalMeshData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, const UCustomizableObject& CustomizableObject, int32 ComponentIndex);

	bool BuildSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh& SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, UCustomizableObject& CustomizableObject, int32 ComponentIndex);
	void BuildMeshSockets(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, UCustomizableObjectInstance* CustomizableObjectInstance, mu::MeshPtrConst MutableMesh);
	void BuildOrCopyElementData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyMorphTargetsData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	bool BuildOrCopyRenderData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyClothingData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	
	//
	USkeleton* MergeSkeletons(UCustomizableObject& CustomizableObject, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, int32 ComponentIndex);

	//
	UPhysicsAsset* GetOrBuildMainPhysicsAsset(TObjectPtr<class UPhysicsAsset> TamplateAsset, const mu::PhysicsBody* PhysicsBody, const UCustomizableObject& CustomizableObject, int32 ComponentIndex, bool bDisableCollisionBetweenAssets);
	
	// Create a transient texture and add it to the TextureTrackerArray
	UTexture2D* CreateTexture();

	void InvalidateGeneratedData();

	bool DoComponentsNeedUpdate(UCustomizableObjectInstance* CustomizableObjectInstance, const TSharedRef<FUpdateContextPrivate>& OperationData, TArray<bool>& OutComponentNeedsUpdate, bool& bOutEmptyMesh);

	mu::FResourceID GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const;
	void SetLastMeshId(int32 ComponentIndex, int32 LODIndex, mu::FResourceID MeshId);

public:
	CUSTOMIZABLEOBJECT_API ESkeletalMeshStatus GetSkeletalMeshStatus() const;

	void SetSkeletalMeshStatus(ESkeletalMeshStatus Status);
	
	// If any components are using this instance, they will store the min of their distances to the player here every frame for LOD purposes
	float MinSquareDistFromComponentToPlayer;
	float LastMinSquareDistFromComponentToPlayer; // The same as the previous dist for last frame
												
	// This is the LODs that the Customizable Object has
	int32 NumLODsAvailable;

	// First SkeletalMesh LOD we can generate on the running platform
	uint8 FirstLODAvailable;

	// Maximum number of SkeletalMesh LODs to stream
	uint8 NumMaxLODsToStream;
	
	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries;

	UPROPERTY(Transient)
	TArray<FCustomizableInstanceComponentData> ComponentsData;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UMaterialInterface> > ReferencedMaterials;

	// Converts a ReferencedMaterials index from the CustomizableObject to an index in the ReferencedMaterials in the Instance
	TMap<uint32, uint32> ObjectToInstanceIndexMap;

	TArray<FGeneratedTexture> TexturesToRelease;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > ClothingPhysicsAssets;

	// To keep loaded AnimBPs referenced and prevent GC
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TArray<TSubclassOf<UAnimInstance>> GatheredAnimBPs;

	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	FGameplayTagContainer AnimBPGameplayTags;

	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TMap<TSubclassOf<UAnimInstance>, FAnimBpGeneratedPhysicsAssets> AnimBpPhysicsAssets;

	// The pass-through textures that will be loaded during an update
	TArray<TSoftObjectPtr<UTexture>> PassThroughTexturesToLoad;

	// Used during an update to prevent the pass-through textures loaded by LoadAdditionalAssetsAsync() from being unloaded by GC
	// between AdditionalAssetsAsyncLoaded() and their setting into the generated materials in BuildMaterials()
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture>> LoadedPassThroughTexturesPendingSetMaterial;

private:
	
	ECOInstanceFlags InstanceFlagsPrivate = ECOInstanceFlags::ECONone;

public:
	/** Hash of the UCustomizableObjectInstance::Descriptor on the last update request. */
	FDescriptorRuntimeHash UpdateDescriptorRuntimeHash;
	
	/** Hash of the UCustomizableObjectInstance::Descriptor on the last successful update. */
	FDescriptorRuntimeHash DescriptorRuntimeHash;

private:
	/** Status of the generated Skeletal Mesh. Not to be confused with the Update Result. */
	ESkeletalMeshStatus SkeletalMeshStatus = ESkeletalMeshStatus::NotGenerated;
	
	// Struct used by BuildMaterials() to identify common materials between LODs
	struct FMutableMaterialPlaceholder
	{
		enum class EPlaceHolderParamType { Vector, Scalar, Texture };

		struct FMutableMaterialPlaceHolderParam
		{
			FName ParamName;
			int32 LayerIndex; // Set to -1 for non-multilayer params
			FLinearColor Vector;
			float Scalar;
			FGeneratedTexture Texture;
			EPlaceHolderParamType Type;

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FLinearColor& InVector)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Vector(InVector), Type(EPlaceHolderParamType::Vector) {}

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const float InScalar)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Scalar(InScalar), Type(EPlaceHolderParamType::Scalar) {}

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FGeneratedTexture& InTexture)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Texture(InTexture), Type(EPlaceHolderParamType::Texture) {}

			bool operator<(const FMutableMaterialPlaceHolderParam& Other) const
			{
				return Type < Other.Type || ParamName.CompareIndexes(Other.ParamName);
			}
		};

		UMaterialInterface* ParentMaterial;
		TArray<FMutableMaterialPlaceHolderParam> Params;
		int32 MatIndex = -1;

		void AddParam(const FMutableMaterialPlaceHolderParam& NewParam) { Params.Add(NewParam); }

		// Return a hash of the material and its parameters
		uint32 GetHash();
	};
};

