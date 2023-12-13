// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LogBenchmarkUtil.h"
#include "Containers/Queue.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "Containers/Ticker.h"

#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Mesh.h"
#include "MuR/Parameters.h"
#include "MuR/System.h"
#include "MuR/Image.h"
#include "UObject/GCObject.h"
#include "WorldCollision.h"
#include "MuCO/FMutableTaskGraph.h"

// This define could come from MuR/System.h
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	#include "Tasks/Task.h"
#else
	#include "Async/TaskGraphInterfaces.h"
#endif

#include "CustomizableObjectSystemPrivate.generated.h"

#if WITH_EDITORONLY_DATA
class UEditorImageProvider;
#endif

class UCustomizableObjectSystem;
namespace LowLevelTasks { enum class ETaskPriority : int8; }
struct FTexturePlatformData;


struct FMutablePendingInstanceUpdate
{
	TSharedRef<FUpdateContextPrivate> Context;

	FMutablePendingInstanceUpdate(const TSharedRef<FUpdateContextPrivate>& InContext);

	bool operator==(const FMutablePendingInstanceUpdate& Other) const;

	bool operator<(const FMutablePendingInstanceUpdate& Other) const;
};


inline uint32 GetTypeHash(const FMutablePendingInstanceUpdate& Update);


struct FPendingInstanceUpdateKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<const UCustomizableObjectInstance>>
{
	FORCEINLINE static TWeakObjectPtr<const UCustomizableObjectInstance> GetSetKey(const FMutablePendingInstanceUpdate& PendingUpdate);

	FORCEINLINE static bool Matches(const TWeakObjectPtr<const UCustomizableObjectInstance>& A, const TWeakObjectPtr<const UCustomizableObjectInstance>& B);

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<const UCustomizableObjectInstance>& Identifier);
};


struct FMutablePendingInstanceDiscard
{
	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	FMutablePendingInstanceDiscard(UCustomizableObjectInstance* InCustomizableObjectInstance)
	{
		CustomizableObjectInstance = InCustomizableObjectInstance;
	}

	friend bool operator ==(const FMutablePendingInstanceDiscard& A, const FMutablePendingInstanceDiscard& B)
	{
		return A.CustomizableObjectInstance.HasSameIndexAndSerialNumber(B.CustomizableObjectInstance);
	}
};


inline uint32 GetTypeHash(const FMutablePendingInstanceDiscard& Discard)
{
	return GetTypeHash(Discard.CustomizableObjectInstance.GetWeakPtrTypeHash());
}


struct FPendingInstanceDiscardKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<UCustomizableObjectInstance>>
{
	FORCEINLINE static const TWeakObjectPtr<UCustomizableObjectInstance>& GetSetKey(const FMutablePendingInstanceDiscard& PendingDiscard)
	{
		return PendingDiscard.CustomizableObjectInstance;
	}

	FORCEINLINE static bool Matches(const TWeakObjectPtr<UCustomizableObjectInstance>& A, const TWeakObjectPtr<UCustomizableObjectInstance>& B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<UCustomizableObjectInstance>& Identifier)
	{
		return GetTypeHash(Identifier.GetWeakPtrTypeHash());
	}
};


/** Instance updates queue.
 *
 * The queues will only contain a single operation per UCustomizableObjectInstance.
 * If there is already an operation it will be replaced. */
class FMutablePendingInstanceWork
{
	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs> PendingInstanceUpdates;

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs> PendingInstanceDiscards;

	TSet<mu::Instance::ID> PendingIDsToRelease;

	int32 NumLODUpdatesLastTick = 0;

public:
	// Returns the number of pending instance updates, LOD Updates, discards and releases last tick.
	int32 Num() const;

	void SetLODUpdatesLastTick(int32 NumLODUpdates);

	// Adds a new instance update
	void AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd);

	// Removes an instance update
	void RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance);

	const FMutablePendingInstanceUpdate* GetUpdate(const TWeakObjectPtr<const UCustomizableObjectInstance>& Instance) const;

	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs>::TIterator GetUpdateIterator()
	{
		return PendingInstanceUpdates.CreateIterator();
	}

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs>::TIterator GetDiscardIterator()
	{
		return PendingInstanceDiscards.CreateIterator();
	}

	TSet<mu::Instance::ID>::TIterator GetIDsToReleaseIterator()
	{
		return PendingIDsToRelease.CreateIterator();
	}

	void AddDiscard(const FMutablePendingInstanceDiscard& TaskToEnqueue);
	void AddIDRelease(mu::Instance::ID IDToRelease);

	void RemoveAllUpdatesAndDiscardsAndReleases();
};


struct FMutableImageCacheKey
{
	mu::FResourceID Resource = 0;
	int32 SkippedMips = 0;

	FMutableImageCacheKey() {};

	FMutableImageCacheKey(mu::FResourceID InResource, int32 InSkippedMips)
		: Resource(InResource), SkippedMips(InSkippedMips) {}

	inline bool operator==(const FMutableImageCacheKey& Other) const
	{
		return Resource == Other.Resource && SkippedMips == Other.SkippedMips;
	}
};


inline uint32 GetTypeHash(const FMutableImageCacheKey& Key)
{
	return HashCombine(GetTypeHash(Key.Resource), GetTypeHash(Key.SkippedMips));
}


// Cache of weak references to generated resources for one single model
struct FMutableResourceCache
{
	TWeakObjectPtr<const UCustomizableObject> Object;
	TMap<mu::FResourceID, TWeakObjectPtr<USkeletalMesh> > Meshes;
	TMap<FMutableImageCacheKey, TWeakObjectPtr<UTexture2D> > Images;

	void Clear()
	{
		Meshes.Reset();
		Images.Reset();
	}
};


USTRUCT()
struct FGeneratedTexture
{
	GENERATED_USTRUCT_BODY();

	FMutableImageCacheKey Key;

	UPROPERTY(Category = CustomizableObjectInstance, VisibleAnywhere)
	FString Name;

	UPROPERTY(Category = CustomizableObjectInstance, VisibleAnywhere)
	TObjectPtr<UTexture> Texture = nullptr;

	bool operator==(const FGeneratedTexture& Other) const = default;
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


DECLARE_DELEGATE(FMutableTaskDelegate);
struct FMutableTask
{
	/** Actual function to perform in this task. */
	FMutableTaskDelegate Function;

	/** We can have 2 types of depencies:
		From the new task system: */
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::FTask Dependency;
#endif

	/** From the traditional event graph system, still used to wait for async loads. */
	FGraphEventRef GraphDependency0;
	
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::FTask GraphDependency1;
#else
	FGraphEventRef GraphDependency1;
#endif

	/** Check if all the dependencies of this task have been completed. */
	inline bool AreDependenciesComplete() const
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		return (!Dependency.IsValid() || Dependency.IsCompleted())
			&&
			(!GraphDependency0 || GraphDependency0->IsComplete())
			&&
			(GraphDependency1.IsCompleted());
#else
		return 
			(!GraphDependency0 || GraphDependency0->IsComplete())
			&&
			(!GraphDependency1 || GraphDependency1->IsComplete());
#endif
	}

	/** Free the handles for any dependency of this task. */
	inline void ClearDependencies()
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		Dependency = {};
#endif
		GraphDependency0 = nullptr;

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		GraphDependency1 = {};
#else
		GraphDependency1 = nullptr;
#endif
	}
};


// Mutable data generated during the update steps.
// We keep it from begin to end update, and it is used in several steps.
struct FInstanceUpdateData
{
	struct FImage
	{
		FName Name;
		mu::FResourceID ImageID;
		
		// LOD of the ImageId. If the texture is shared between LOD, first LOD where this image can be found. 
		int32 BaseLOD;
		int32 BaseMip;
		
		uint16 FullImageSizeX, FullImageSizeY;
		mu::Ptr<const mu::Image> Image;
		TWeakObjectPtr<UTexture2D> Cached;

		bool bIsPassThrough = false;
	};

	struct FVector
	{
		FName Name;
		FLinearColor Vector;
	};

	struct FScalar
	{
		FName Name;
		float Scalar;
	};

	struct FSurface
	{
		/** Range in the Images array */
		uint16 FirstImage = 0;
		uint16 ImageCount = 0;

		/** Range in the Vectors array */
		uint16 FirstVector = 0;
		uint16 VectorCount = 0;

		/** Range in the Scalar array */
		uint16 FirstScalar = 0;
		uint16 ScalarCount = 0;

		uint32 MaterialIndex = 0;

		/** Id of the surface in the mutable core instance. */
		uint32 SurfaceId = 0;
	};

	struct FComponent
	{
		uint16 Id = 0;
		
		// True if the Mesh is valid
		bool bGenerated = false;

		mu::FResourceID MeshID;
		mu::MeshPtrConst Mesh;

		/** Range in the Surfaces array */
		uint16 FirstSurface = 0;
		uint16 SurfaceCount = 0;

		// \TODO: Flatten
		TArray<uint16> ActiveBones;
		/** Range in the external Bones array */
		//uint32 FirstActiveBone;
		//uint32 ActiveBoneCount;

		/** Range in the external Bones array */
		uint32 FirstBoneMap = 0;
		uint32 BoneMapCount = 0;
	};

	struct FLOD
	{
		/** Range in the Components array */
		uint16 FirstComponent = 0;
		uint16 ComponentCount = 0;
	};

	TArray<FLOD> LODs;
	TArray<FComponent> Components;
	TArray<FSurface> Surfaces;
	TArray<FImage> Images;
	TArray<FVector> Vectors;
	TArray<FScalar> Scalars;

	TArray<uint16> BoneMaps;

	struct FSkeletonData
	{
		int16 ComponentIndex = INDEX_NONE;

		TArray<uint16> SkeletonIds;

		TArray<uint16> BoneIds;
		TArray<FMatrix44f> BoneMatricesWithScale;
	};

	TArray<FSkeletonData> Skeletons;

	struct FNamedExtensionData
	{
		mu::ExtensionDataPtrConst Data;
		FName Name;
	};
	TArray<FNamedExtensionData> ExtendedInputPins;

	/** */
	void Clear()
	{
		LODs.Empty();
		Components.Empty();
		Surfaces.Empty();
		Images.Empty();
		Scalars.Empty();
		Vectors.Empty();
		Skeletons.Empty();
		ExtendedInputPins.Empty();
	}
};


/** Update Context.
 *
 * Alive from the start to the end of the update (both API and LOD update). */
class FUpdateContextPrivate : public FGCObject
{
public:
	FUpdateContextPrivate(UCustomizableObjectInstance& InInstance);
	virtual ~FUpdateContextPrivate() override;

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	EQueuePriorityType PriorityType = EQueuePriorityType::Low;
	
	FInstanceUpdateDelegate UpdateCallback;

	/** Weak reference to the instance we are operating on.
	 *It is weak because we don't want to lock it in case it becomes irrelevant in the game while operations are pending and it needs to be destroyed. */
	TWeakObjectPtr<UCustomizableObjectInstance> Instance;

	/** Hash of the UCustomizableObjectInstance::Descriptor at the time of the update request. */
	FDescriptorRuntimeHash InstanceDescriptorRuntimeHash;
			
	/** Instance parameters at the time of the operation request. */
	mu::ParametersPtr Parameters; 
	
	TArray<FName> TextureParameters;

	bool bBuildParameterRelevancy = false;

	/** Instance state. */
	int32 State = 0;

	bool bOnlyUpdateIfNotGenerated = false;
	bool bIgnoreCloseDist = false;
	bool bForceHighPriority = false;
	
	FInstanceUpdateData InstanceUpdateData;
	TArray<int32> RelevantParametersInProgress;

	TArray<FString> LowPriorityTextures;

	/** This option comes from the operation request */
	bool bNeverStream = false;
	
	/** When this option is enabled it will reuse the Mutable core instance and its temp data between updates.  */
	bool bLiveUpdateMode = false;
	bool bReuseInstanceTextures = false;
	bool bUseMeshCache = false;
	
	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;

	mu::Instance::ID InstanceID = 0; // Redundant
	const mu::Instance* MutableInstance = nullptr;

	int32 NumComponents = 0;
	int32 NumLODsAvailable = 0;

	int32 CurrentMinLOD = 0;
	int32 CurrentMaxLOD = 0;

	TArray<uint16> RequestedLODs;

	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;

	EUpdateResult UpdateResult = EUpdateResult::Success;

	mu::FImageOperator::FImagePixelFormatFunc PixelFormatOverride;

	/** Mutable Meshes required for each component. Outermost index is the component, inner index is the LOD. */
	TArray<TArray<mu::FResourceID>> MeshDescriptors;
	
	bool UpdateStarted = false;

	// Update stats
	double StartQueueTime = 0.0;
	double QueueTime = 0.0;
	
	double StartUpdateTime = 0.0;
	double UpdateTime = 0.0;

	double TaskGetMeshTime = 0.0;
	double TaskLockCacheTime = 0.0;
	double TaskGetImagesTime = 0.0;
	double TaskConvertResourcesTime = 0.0f;
	double TaskCallbacksTime = 0.0;

	// Update Memory stats
	int64 UpdateStartBytes = 0;
	int64 UpdateEndPeakBytes = 0;
	int64 UpdateEndRealPeakBytes = 0;
	
#if WITH_EDITOR
	/** Used for profiling in the editor. */
	uint32 MutableRuntimeCycles = 0;
#endif

	/** Hard references to objects. Avoids GC to collect them. */
	TArray<TObjectPtr<const UObject>> Objects;
};


/** Runtime data used during a mutable instance update */
struct FMutableReleasePlatformOperationData
{
	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;
};


class FCustomizableObjectSystemPrivate : public FGCObject
{
public:
	// Singleton for the unreal mutable system.
	static UCustomizableObjectSystem* SSystem;

	// Pointer to the lower level mutable system that actually does the work.
	mu::Ptr<mu::System> MutableSystem;

	/** Store the last streaming memory size in bytes, to change it when it is safe. */
	uint64 LastWorkingMemoryBytes = 0;
	uint32 LastGeneratedResourceCacheSize = 0;

	// This object is responsible for streaming data to the MutableSystem.
	TSharedPtr<class FUnrealMutableModelBulkReader> Streamer;

	TSharedPtr<class FUnrealExtensionDataStreamer> ExtensionDataStreamer;

	// This object is responsible for providing custom images to mutable models (for image parameters)
	// This object is called from the mutable thread, and it should only access data already safely submitted from
	// the game thread and stored in FUnrealMutableImageProvider::GlobalExternalImages.
	TSharedPtr<class FUnrealMutableImageProvider> ImageProvider;

	// Cache of weak references to generated resources to see if they can be reused.
	TArray<FMutableResourceCache> ModelResourcesCache;

	// List of textures currently cached and valid for the current object that we are operating on.
	// This array gets generated when the object cached resources are protected in SetResourceCacheProtected
	// from the game thread, and it is read from the Mutable thread only while updating the instance.
	TArray<mu::FResourceID> ProtectedObjectCachedImages;

	// The pending instance updates, discards or releases
	FMutablePendingInstanceWork MutablePendingInstanceWork;

	// Queue of game-thread tasks that need to be executed for the current operation. TQueue is thread safe
	TQueue<FMutableTask> PendingTasks;

	static int32 EnableMutableProgressiveMipStreaming;
	static int32 EnableMutableLiveUpdate;
	static int32 EnableReuseInstanceTextures;
	static int32 EnableMutableAnimInfoDebugging;
	static int32 EnableSkipGenerateResidentMips;
	static int32 EnableOnlyGenerateRequestedLODs;
	static int32 MaxTextureSizeToGenerate;
	static int32 SkeletalMeshMinLodQualityLevel;

#if WITH_EDITOR
	mu::FImageOperator::FImagePixelFormatFunc ImageFormatOverrideFunc;
#endif

	void AddGameThreadTask(const FMutableTask& Task);

	/** FSerializableObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	// Remove references to cached objects that have been deleted in the unreal
	// side, and cannot be cached anyway.
	// This should only happen in the game thread
	void CleanupCache();

	// This should only happen in the game thread
	FMutableResourceCache& GetObjectCache(const UCustomizableObject* Object);

	void AddTextureReference(const FMutableImageCacheKey& TextureId);

	// Returns true if the texture's references become zero
	bool RemoveTextureReference(const FMutableImageCacheKey& TextureId);

	bool TextureHasReferences(const FMutableImageCacheKey& TextureId) const;

	EUpdateRequired IsUpdateRequired(const UCustomizableObjectInstance& Instance, bool bOnlyUpdateIfNotGenerated, bool bOnlyUpdateIfLOD, bool bIgnoreCloseDist) const;

	EQueuePriorityType GetUpdatePriority(const UCustomizableObjectInstance& Instance, bool bForceHighPriority) const;

	void EnqueueUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);
		
	// Init an async and safe release of the UE and Mutable resources used by the instance without actually destroying the instance, for example if it's very far away
	void InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance);

	// Init the async release of a Mutable Core Instance ID and all the temp resources associated with it
	void InitInstanceIDRelease(mu::Instance::ID IDToRelease);

	void GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const;
	
	bool IsReplaceDiscardedWithReferenceMeshEnabled() const;
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled);

	/** Updated at the beginning of each tick. */
	int32 GetNumSkeletalMeshes() const;

	bool bReplaceDiscardedWithReferenceMesh = false;
	bool bReleaseTexturesImmediately = false;

	bool bSupport16BitBoneIndex = false;

	static FCustomizableObjectCompilerBase* (*NewCompilerFunc)();

	TMap<FMutableImageCacheKey, uint32> TextureReferenceCount; // Keeps a count of texture usage to decide if they have to be blocked from GC during an update

	// This is protected from GC by AddReferencedObjects
	TObjectPtr<UCustomizableObjectInstance> CurrentInstanceBeingUpdated = nullptr;

	TSharedPtr<FUpdateContextPrivate> CurrentMutableOperation = nullptr;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;
	FTickerDelegate TickDelegate;

	/** Update the last set amount of internal memory Mutable can use to build objects. */
	void UpdateMemoryLimit();

	bool IsMutableAnimInfoDebuggingEnabled() const;

	FUnrealMutableImageProvider* GetImageProviderChecked() const;

	/** Start the actual work of Update Skeletal Mesh process (Update Skeletal Mesh without the queue). */
	void StartUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);

	/** See UCustomizableObjectInstance::IsUpdating. */
	bool IsUpdating(const UCustomizableObjectInstance& Instance) const;

	/** Update stats at each tick.
	 * Used for stats that are costly to update. */
	void UpdateStats();
	
	/** Mutable TaskGraph system (Mutable Thread). */
	FMutableTaskGraph MutableTaskGraph;
	
#if WITH_EDITORONLY_DATA
	/** Mutable default image provider. Used by the COIEditor and Instance/Descriptor APIs. */
	TObjectPtr<UEditorImageProvider> EditorImageProvider = nullptr;
#endif

	FLogBenchmarkUtil LogBenchmarkUtil;

	int32 NumSkeletalMeshes = 0;
};


/** Set OnlyLOD to -1 to generate all mips */
CUSTOMIZABLEOBJECT_API FTexturePlatformData* MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);
