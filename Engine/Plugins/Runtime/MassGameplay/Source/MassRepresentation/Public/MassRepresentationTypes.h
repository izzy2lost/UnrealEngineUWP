// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassLODTypes.h"
#include "Engine/DataTable.h"
#include "Misc/MTAccessDetector.h"
#include "InstanceDataTypes.h"
#include "Experimental/Containers/RobinHoodHashTable.h"

#include "MassRepresentationTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;
struct FMassLODSignificanceRange;
class UMassVisualizationComponent;
class UInstancedStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogMassRepresentation, Log, All);

namespace UE::Mass::ProcessorGroupNames
{
	const FName Representation = FName(TEXT("Representation"));
}

UENUM()
enum class EMassRepresentationType : uint8
{
	HighResSpawnedActor,
	LowResSpawnedActor,
	StaticMeshInstance,
	None,
};

enum class EMassActorEnabledType : uint8
{
	Disabled,
	LowRes,
	HighRes,
};

USTRUCT()
struct MASSREPRESENTATION_API FMassStaticMeshInstanceVisualizationMeshDesc
{
	GENERATED_BODY()

	FMassStaticMeshInstanceVisualizationMeshDesc();
	
	/** The static mesh visual representation */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/**
	 * Material overrides for the static mesh visual representation. 
	 * 
	 * Array indices correspond to material slot indices on the static mesh.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** The minimum inclusive LOD significance to start using this static mesh */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	float MinLODSignificance = float(EMassLOD::High);

	/** The maximum exclusive LOD significance to stop using this static mesh */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	float MaxLODSignificance = float(EMassLOD::Max);

	/** Controls whether the ISM can cast shadow or not */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	bool bCastShadows = false;
	
	/** Controls the mobility of the ISM */
	EComponentMobility::Type Mobility = EComponentMobility::Movable;

	/** InstancedStaticMeshComponent class to use to manage instances described by this struct instance */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TSubclassOf<UInstancedStaticMeshComponent> ISMComponentClass;

	bool operator==(const FMassStaticMeshInstanceVisualizationMeshDesc& Other) const
	{
		return Mesh == Other.Mesh && 
			MaterialOverrides == Other.MaterialOverrides && 
			FMath::IsNearlyEqual(MinLODSignificance, Other.MinLODSignificance, KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(MaxLODSignificance, Other.MaxLODSignificance, KINDA_SMALL_NUMBER) &&
			bCastShadows == Other.bCastShadows && 
			Mobility == Other.Mobility;
	}
	
	friend FORCEINLINE uint32 GetTypeHash(const FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc)
	{
		uint32 Hash = 0x0;
		Hash = PointerHash(MeshDesc.Mesh, Hash);
		Hash = HashCombine(GetTypeHash(MeshDesc.bCastShadows), Hash);
		Hash = HashCombine(GetTypeHash(MeshDesc.Mobility), Hash);
		for (UMaterialInterface* MaterialOverride : MeshDesc.MaterialOverrides)
		{
			if (MaterialOverride)
			{
				Hash = PointerHash(MaterialOverride, Hash);
			}
		}
		return Hash;
	}

	// convenience function for setting MinLODSinificance and MaxLODSinificance based on EMassLOD values
	void SetSignificanceRange(const EMassLOD::Type MinLOD, const EMassLOD::Type MaxLOD)
	{
		checkSlow(MinLOD <= MaxLOD);
		MinLODSignificance = float(MinLOD);
		MaxLODSignificance = float(MaxLOD);
	}
};

USTRUCT()
struct FStaticMeshInstanceVisualizationDesc : public FTableRowBase
{
	GENERATED_BODY()

	/** 
	 * Mesh descriptions. These will be instanced together using the same transform for each, to 
	 * visualize the given agent.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TArray<FMassStaticMeshInstanceVisualizationMeshDesc> Meshes;

	/** Boolean to enable code to transform the static meshes if not align the mass agent transform */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	bool bUseTransformOffset = false;

	/** Transform to offset the static meshes if not align the mass agent transform */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual", meta=(EditCondition="bUseTransformOffset"))
	FTransform TransformOffset;

	bool operator==(const FStaticMeshInstanceVisualizationDesc& Other) const
	{
		return Meshes == Other.Meshes;
	}

	void Reset()
	{
		new(this)FStaticMeshInstanceVisualizationDesc();
	}
};

class UInstancedStaticMeshComponent;


struct MASSREPRESENTATION_API FMassISMCSharedData
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMassISMCSharedData() 
		: bRequiresExternalInstanceIDTracking(false)
	{		
	}

	explicit FMassISMCSharedData(UInstancedStaticMeshComponent* InISMC, bool bInRequiresExternalInstanceIDTracking = false)
		: ISMC(InISMC), bRequiresExternalInstanceIDTracking(bInRequiresExternalInstanceIDTracking)
	{
	}

	FMassISMCSharedData(const FMassISMCSharedData& Other) = default;
	FMassISMCSharedData& operator=(const FMassISMCSharedData& Other) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void SetISMComponent(UInstancedStaticMeshComponent& InISMC)
	{
		check(ISMC == nullptr && ISMComponentReferencesCount == 0);
		ISMC = &InISMC;
	}

	UInstancedStaticMeshComponent* GetMutableISMComponent() { return ISMC; }
	const UInstancedStaticMeshComponent* GetISMComponent() const { return ISMC; }
	int32 OnISMComponentReferenceStored() { return ++ISMComponentReferencesCount; }
	int32 OnISMComponentReferenceReleased() { ensure(ISMComponentReferencesCount >= 0); return --ISMComponentReferencesCount; }

	void ResetAccumulatedData()
	{
		UpdateInstanceIds.Reset();
		StaticMeshInstanceCustomFloats.Reset();
		StaticMeshInstanceTransforms.Reset();
		StaticMeshInstancePrevTransforms.Reset();
		RemoveInstanceIds.Reset();
		WriteIterator = 0;
	}

	void RemoveUpdatedInstanceIdsAtSwap(const int32 InstanceIDIndex)
	{
		UpdateInstanceIds.RemoveAtSwap(InstanceIDIndex, 1, /*bAllowShrinking=*/false);
		StaticMeshInstanceTransforms.RemoveAtSwap(InstanceIDIndex, 1, /*bAllowShrinking=*/false);
		StaticMeshInstancePrevTransforms.RemoveAtSwap(InstanceIDIndex, 1, /*bAllowShrinking=*/false);
		if (StaticMeshInstanceCustomFloats.Num())
		{
			StaticMeshInstanceCustomFloats.RemoveAtSwap(InstanceIDIndex, 1, /*bAllowShrinking=*/false);
		}
	}

	bool HasUpdatesToApply() const { return UpdateInstanceIds.Num() || RemoveInstanceIds.Num(); }
	TConstArrayView<int32> GetUpdateInstanceIds() const { return UpdateInstanceIds; }
	TConstArrayView<FTransform> GetStaticMeshInstanceTransforms() const { return StaticMeshInstanceTransforms; }
	/** 
	 * this function is a flavor we need to interact with older engine API that's using TArray references. 
	 * Use GetStaticMeshInstanceTransforms instead whenever possible. 
	 */
	const TArray<FTransform>& GetStaticMeshInstanceTransformsArray() const { return StaticMeshInstanceTransforms; }
	TConstArrayView<FTransform> GetStaticMeshInstancePrevTransforms() const { return StaticMeshInstancePrevTransforms; }
	TConstArrayView<int32> GetRemoveInstanceIds() const { return RemoveInstanceIds; }
	TConstArrayView<float> GetStaticMeshInstanceCustomFloats() const { return StaticMeshInstanceCustomFloats; }
	
	bool RequiresExternalInstanceIDTracking() const { return bRequiresExternalInstanceIDTracking; }

	void Reset() 
	{
		*this = FMassISMCSharedData();
	}

	using FIdMap = Experimental::TRobinHoodHashMap<int32, FPrimitiveInstanceId>;

	FIdMap& GetMutableIdMap() { return MassInstanceIdToComponentInstanceIdMap; }
	const FIdMap& GetIdMap() const { return MassInstanceIdToComponentInstanceIdMap; }

	int16 GetComponentInstanceIdTouchCounter() const { return ComponentInstanceIdTouchCounter; }

protected:
	friend FMassLODSignificanceRange;
	friend UMassVisualizationComponent;
	/** Buffer holding current frame transforms for the static mesh instances, used to batch update the transforms */
	TArray<int32> UpdateInstanceIds;
	TArray<FTransform> StaticMeshInstanceTransforms;
	TArray<FTransform> StaticMeshInstancePrevTransforms;
	TArray<int32> RemoveInstanceIds;

	/** Buffer holding current frame custom floats for the static mesh instances, used to batch update the ISMs custom data */
	TArray<float> StaticMeshInstanceCustomFloats;

	// When initially adding to StaticMeshInstanceCustomFloats, can use the size as the write iterator, but on subsequent processors, we need to know where to start writing
	int32 WriteIterator = 0;

	UInstancedStaticMeshComponent* ISMC = nullptr;
	int32 ISMComponentReferencesCount = 0;

	/** 
	 * When set to true will result in MassVisualizationComponent manually perform Instance ID-related operations 
	 * instead of relying on ISMComponent's internal ID operations. 
	 * @note this mechanism has been added in preparation of changes to ISM component to change access to its internal 
	 *	instance ID logic. WIP as of Jun 17th 2023 
	 */
	uint8 bRequiresExternalInstanceIDTracking : 1;
	
private:
	/** Indicates that mutating changes, that can affect MassInstanceIdToComponentInstanceIdMap, have been performed.
	 *	Can be used to validate whether cached data stored in other placed needs to be re-cached. */
	uint16 ComponentInstanceIdTouchCounter = 0;

protected:
	FIdMap MassInstanceIdToComponentInstanceIdMap;

	UE_DEPRECATED(5.4, "RefCount is deprecated, use ISMComponentReferencesCount instead")
	int32 RefCount = 0;

public:
	UE_DEPRECATED(5.4, "StoreReference is deprecated, use OnISMComponentReferenceStored instead")
	int32 StoreReference() { return OnISMComponentReferenceStored(); }
	UE_DEPRECATED(5.4, "ReleaseReference is deprecated, use OnISMComponentReferenceReleased instead")
	int32 ReleaseReference() { return OnISMComponentReferenceReleased(); }
};


/** 
 * The container type hosting FMassISMCSharedData instances and supplying functionality of marking entries that require 
 * instance-related operations (adding, removing). 
 * 
 * To get a FMassISMCSharedData instance to add operations to it call GetAndMarkDirty.
 * 
 * Use FDirtyIterator to iterate over just the data that needs processing. 
 * 
 * @see UMassVisualizationComponent::EndVisualChanges for iteration
 * @see FMassLODSignificanceRange methods for performing dirtying operations
 */
struct FMassISMCSharedDataMap 
{
	struct FDirtyIterator
	{
		friend FMassISMCSharedDataMap;
		explicit FDirtyIterator(FMassISMCSharedDataMap& InContainer)
			: Container(InContainer), It(InContainer.GetDirtyArray())
		{
			if (It && It.GetValue() != bValueToCheck)
			{
				// will result in either setting IT to the first bInValue, or making bool(It) == false
				++(*this);
			}
		}
	public:
		operator bool() const { return bool(It); }

		FDirtyIterator& operator++()
		{
			while (++It)
			{
				if (It.GetValue() == bValueToCheck)
				{
					break;
				}
			}
			return *this;
		}

		FMassISMCSharedData& operator*() const
		{
			return Container.GetAtIndex(It.GetIndex());
		}

		void ClearDirtyFlag()
		{
			It.GetValue() = false;
		}

	private:
		FMassISMCSharedDataMap& Container;
		TBitArray<>::FIterator It;
		static constexpr bool bValueToCheck = true;
	};

	FMassISMCSharedData& GetAndMarkDirtyChecked(const uint32 Hash)
	{
		const int32 DataIndex = Map[Hash];
		DirtyData[DataIndex] = true;
		return Data[DataIndex];
	}

	FMassISMCSharedData* GetAndMarkDirty(const uint32 Hash)
	{
		if (const int32* DataIndex = Map.Find(Hash))
		{
			DirtyData[*DataIndex] = true;
			return &Data[*DataIndex];
		}
		return nullptr;
	}
	
	template<typename... TArgs>
	FMassISMCSharedData& FindOrAdd(const uint32 Hash, TArgs&&... InNewInstanceArgs)
	{
		const int32* DataIndex = Map.Find(Hash);
		if (DataIndex == nullptr)
		{
			return Add(Hash, Forward<TArgs>(InNewInstanceArgs)...);
		}
		check(Data.IsValidIndex(*DataIndex));
		return Data[*DataIndex];
	}

	FMassISMCSharedData* Find(const uint32 Hash)
	{
		int32* DataIndex = Map.Find(Hash);
		return (DataIndex == nullptr || *DataIndex == INDEX_NONE) ? (FMassISMCSharedData*)nullptr : &Data[*DataIndex];
	}

	template<typename... TArgs>
	FMassISMCSharedData& Add(const uint32 Hash, TArgs&&... InNewInstanceArgs)
	{
		const int32 DataIndex = FreeIndices.Num() ? FreeIndices.Pop() : Data.Num();
		Map.Add(Hash, DataIndex);

		if (DataIndex == Data.Num())
		{
			DirtyData.Add(false, DataIndex - DirtyData.Num() + 1);
			DirtyData[DataIndex] = true;
			return Data.Add_GetRef(FMassISMCSharedData(Forward<TArgs>(InNewInstanceArgs)...));
		}
		else
		{
			DirtyData[DataIndex] = true;
			Data[DataIndex] = FMassISMCSharedData(Forward<TArgs>(InNewInstanceArgs)...);
			return Data[DataIndex];
		}
	}

	void Remove(const uint32 Hash)
	{
		int32 DataIndex = INDEX_NONE;
		if (ensure(Map.RemoveAndCopyValue(Hash, DataIndex)))
		{
			DirtyData[DataIndex] = false;
			Data[DataIndex].Reset();
			FreeIndices.Add(DataIndex);
		}
	}

	FMassISMCSharedData& GetAtIndex(const int32 DataIndex)
	{
		return Data[DataIndex];
	}
	
	TBitArray<>& GetDirtyArray()
	{ 
		return DirtyData;
	}

	/** @return total number of entries in Data array. Note that some or all entries could be empty (i.e. already freed) */
	int32 Num() const
	{
		return Data.Num();
	}

	/** @return number of non-empty entries in Data. */
	int32 NumValid() const
	{
		return Data.Num() - FreeIndices.Num();
	}

	bool IsDirty(const int32 DataIndex) const
	{
		return DirtyData[DataIndex];
	}

	void Reset()
	{
		*this = FMassISMCSharedDataMap();
	}

	const FMassISMCSharedData* GetDataForIndex(const int32 Index) const
	{
		return Data.IsValidIndex(Index) ? &Data[Index] : nullptr;
	}

protected:
	TArray<FMassISMCSharedData> Data;
	/** Mapping from Hash of data represented by FMassISMCSharedData to an index to Data */
	TMap<uint32, int32> Map;
	/** Indicates whether corresponding Data entry has any instance work assigned to it (instance addition or removal) */
	TBitArray<> DirtyData;
	/** Indices to Data that are available for reuse */
	TArray<int32> FreeIndices;
};


USTRUCT()
struct MASSREPRESENTATION_API FMassLODSignificanceRange
{
	GENERATED_BODY()
public:

	void AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const TArray<uint32>& ExcludeStaticMeshRefs);

	// Adds the specified struct reinterpreted as custom floats to our custom data. Individual members of the specified struct should always fit into a float.
	// When adding any custom data, the custom data must be added for every instance.
	template<typename InCustomDataType>
	void AddBatchedCustomData(InCustomDataType InCustomData, const TArray<uint32>& ExcludeStaticMeshRefs, int32 NumFloatsToPad = 0)
	{
		check(ISMCSharedDataPtr);
		static_assert((sizeof(InCustomDataType) % sizeof(float)) == 0, "AddBatchedCustomData: InCustomDataType should have a total size multiple of sizeof(float), and have members that fit in a float's boundaries");
		const size_t StructSize = sizeof(InCustomDataType);
		const size_t StructSizeInFloats = StructSize / sizeof(float);
		for (int i = 0; i < StaticMeshRefs.Num(); i++)
		{
			if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
			{
				continue;
			}

			FMassISMCSharedData& SharedData = (*ISMCSharedDataPtr).GetAndMarkDirtyChecked(StaticMeshRefs[i]);
			const int32 StartIndex = SharedData.StaticMeshInstanceCustomFloats.AddDefaulted(StructSizeInFloats + NumFloatsToPad);
			InCustomDataType* CustomData = reinterpret_cast<InCustomDataType*>(&SharedData.StaticMeshInstanceCustomFloats[StartIndex]);
			*CustomData = InCustomData;
		}
	}

	void AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const TArray<uint32>& ExcludeStaticMeshRefs);

	/** Single-instance version of AddBatchedCustomData when called to add entities (as opposed to modify existing ones).*/
	void AddInstance(const int32 InstanceId, const FTransform& Transform);

	void RemoveInstance(const int32 InstanceId);

	void WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const int32 FloatsPerInstance, const int32 StartIndex, const TArray<uint32>& ExcludeStaticMeshRefs);

	/** LOD Significance range */
	float MinSignificance;
	float MaxSignificance;

	/** The component handling these instances */
	UPROPERTY(VisibleAnywhere, Category = "Mass/Debug")
	TArray<uint32> StaticMeshRefs;

	FMassISMCSharedDataMap* ISMCSharedDataPtr = nullptr;
};

USTRUCT()
struct MASSREPRESENTATION_API FMassInstancedStaticMeshInfo
{
	GENERATED_BODY()
public:

	FMassInstancedStaticMeshInfo() = default;

	explicit FMassInstancedStaticMeshInfo(const FStaticMeshInstanceVisualizationDesc& InDesc)
		: Desc(InDesc)
	{
	}

	/** Clears out contents so that a given FMassInstancedStaticMeshInfo instance can be reused */
	void Reset();

	const FStaticMeshInstanceVisualizationDesc& GetDesc() const
	{
		return Desc;
	}

	/** Whether or not to transform the static meshes if not align the mass agent transform */
	bool ShouldUseTransformOffset() const { return Desc.bUseTransformOffset; }
	FTransform GetTransformOffset() const { return Desc.TransformOffset; }

	FORCEINLINE FMassLODSignificanceRange* GetLODSignificanceRange(float LODSignificance)
	{
		for (FMassLODSignificanceRange& Range : LODSignificanceRanges)
		{
			if (LODSignificance >= Range.MinSignificance && LODSignificance < Range.MaxSignificance)
			{
				return &Range;
			}
		}
		return nullptr;
	}

	FORCEINLINE void AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const float LODSignificance, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedTransform(InstanceId, Transform, PrevTransform, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (ensureMsgf(PrevRange, TEXT("Couldn't find a valid LODSignificanceRange for PrevLODSignificance %f"), PrevLODSignificance)
					&& PrevRange != Range)
				{
					PrevRange->AddBatchedTransform(InstanceId, Transform, PrevTransform, Range->StaticMeshRefs);
				}
			}
		}
	}

	FORCEINLINE void RemoveInstance(const int32 InstanceId, const float LODSignificance)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->RemoveInstance(InstanceId);
		}
	}

	// Adds the specified struct reinterpreted as custom floats to our custom data. Individual members of the specified struct should always fit into a float.
	// When adding any custom data, the custom data must be added for every instance.
	template<typename InCustomDataType>
	void AddBatchedCustomData(InCustomDataType InCustomData, const float LODSignificance, const float PrevLODSignificance = -1.0f, int32 NumFloatsToPad = 0)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedCustomData(InCustomData, TArray<uint32>(), NumFloatsToPad);
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (ensureMsgf(PrevRange, TEXT("Couldn't find a valid LODSignificanceRange for PrevLODSignificance %f"), PrevLODSignificance)
					&& PrevRange != Range)
				{
					PrevRange->AddBatchedCustomData(InCustomData, Range->StaticMeshRefs, NumFloatsToPad);
				}
			}
		}
	}

	FORCEINLINE void AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const float LODSignificance, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedCustomDataFloats(CustomFloats, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (ensureMsgf(PrevRange, TEXT("Couldn't find a valid LODSignificanceRange for PrevLODSignificance %f"), PrevLODSignificance)
					&& PrevRange != Range)
				{
					PrevRange->AddBatchedCustomDataFloats(CustomFloats, Range->StaticMeshRefs);
				}
			}
		}
	}

	FORCEINLINE void WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const float LODSignificance, const int32 FloatsPerInstance, const int32 FloatStartIndex, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->WriteCustomDataFloatsAtStartIndex(StaticMeshIndex, CustomFloats, FloatsPerInstance, FloatStartIndex, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (ensureMsgf(PrevRange, TEXT("Couldn't find a valid LODSignificanceRange for PrevLODSignificance %f"), PrevLODSignificance)
					&& PrevRange != Range)
				{
					PrevRange->WriteCustomDataFloatsAtStartIndex(StaticMeshIndex, CustomFloats, FloatsPerInstance, FloatStartIndex, Range->StaticMeshRefs);
				}
			}
		}
	}

	void AddISMComponent(FMassISMCSharedData& SharedData)
	{
		if (ensure(SharedData.GetISMComponent()))
		{
			InstancedStaticMeshComponents.Add(SharedData.GetMutableISMComponent());
			SharedData.OnISMComponentReferenceStored();
		}
	}

	int32 GetLODSignificanceRangesNum() const { return LODSignificanceRanges.Num(); }

	bool IsValid() const
	{
		return Desc.Meshes.Num() && InstancedStaticMeshComponents.Num() && LODSignificanceRanges.Num();
	}

protected:

	/** Destroy the visual instance */
	void ClearVisualInstance(FMassISMCSharedDataMap& ISMCSharedData);

	/** Information about this static mesh which will represent all instances */
	UPROPERTY(VisibleAnywhere, Category = "Mass/Debug")
	FStaticMeshInstanceVisualizationDesc Desc;

	/** The component handling these instances */
	UPROPERTY(VisibleAnywhere, Category = "Mass/Debug")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstancedStaticMeshComponents;

	UPROPERTY(VisibleAnywhere, Category = "Mass/Debug")
	TArray<FMassLODSignificanceRange> LODSignificanceRanges;

	friend class UMassVisualizationComponent;
};

#if ENABLE_MT_DETECTOR

#define MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(ArrayView, AccessDetector) FMassInstancedStaticMeshInfoArrayView(ArrayView, AccessDetector)
struct FMassInstancedStaticMeshInfoArrayViewAccessDetector
{
	FMassInstancedStaticMeshInfoArrayViewAccessDetector(TArrayView<FMassInstancedStaticMeshInfo> InInstancedStaticMeshInfos, const FRWRecursiveAccessDetector& InAccessDetector)
		: InstancedStaticMeshInfos(InInstancedStaticMeshInfos)
		, AccessDetector(&InAccessDetector)
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(*AccessDetector);
	}

	FMassInstancedStaticMeshInfoArrayViewAccessDetector(FMassInstancedStaticMeshInfoArrayViewAccessDetector&& Other)
		: InstancedStaticMeshInfos(Other.InstancedStaticMeshInfos)
		, AccessDetector(Other.AccessDetector)
	{
		Other.AccessDetector = nullptr;
	}
	FMassInstancedStaticMeshInfoArrayViewAccessDetector(const FMassInstancedStaticMeshInfoArrayViewAccessDetector& Other) = delete;
	void operator=(const FMassInstancedStaticMeshInfoArrayViewAccessDetector& Other) = delete;

	~FMassInstancedStaticMeshInfoArrayViewAccessDetector()
	{
		if(AccessDetector)
		{
			UE_MT_RELEASE_WRITE_ACCESS(*AccessDetector);
		}
	}

	FORCEINLINE FMassInstancedStaticMeshInfo& operator[](int32 Index) const
	{
		return InstancedStaticMeshInfos[Index];
	}

	bool IsValidIndex(const int32 Index) const
	{
		return InstancedStaticMeshInfos.IsValidIndex(Index);
	}

private:
	TArrayView<FMassInstancedStaticMeshInfo> InstancedStaticMeshInfos;
	const FRWRecursiveAccessDetector* AccessDetector;
};

typedef FMassInstancedStaticMeshInfoArrayViewAccessDetector FMassInstancedStaticMeshInfoArrayView;

#else // ENABLE_MT_DETECTOR

#define MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(ArrayView, AccessDetector) ArrayView

typedef TArrayView<FMassInstancedStaticMeshInfo> FMassInstancedStaticMeshInfoArrayView;

#endif // ENABLE_MT_DETECTOR
