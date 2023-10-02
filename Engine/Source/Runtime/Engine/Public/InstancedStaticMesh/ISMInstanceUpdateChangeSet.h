// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstanceDataSceneProxy.h"
#include "Containers/StridedView.h"

class HHitProxy;
class FOpaqueHitProxyContainer;

/**
 * Utility to gather and scatter a set of delta-updating values.
 * If there is no delta, and in other words there is a bulk update, the "scatter" to destination can use a more efficient move operation.
 */
class FArrayIndexDelta
{
public:
	FArrayIndexDelta(int32 InNumElements = -1) : NumElements(InNumElements) {}

	/**
	 * Set this delta to represent a full (dense) delta.
	 */
	void SetToFull(int32 InNumElements)
	{
		NumElements = InNumElements;
		DeltaIndices.Empty();
	}

	bool IsDelta() const { return NumElements < 0; }
	int32 Num() const { return IsDelta() ? DeltaIndices.Num() : NumElements; }
	bool IsEmpty() const { return Num() == 0; }

	void AddIndex(int32 Index)
	{
		NumElements = -1;
		DeltaIndices.Add(Index);
	}

	int32 operator[](int32 Index) const
	{
		if (IsDelta())
		{
			return DeltaIndices[Index];
		}
		return Index;
	}


	struct FConstIterator
	{
		FConstIterator(const FArrayIndexDelta *InArrayIndexDelta, int32 InIndex) : ArrayIndexDelta(InArrayIndexDelta), Index(InIndex) {}

		void operator++() 
		{ 
			++Index;
		}

		int32 operator*() const
		{
			return (*ArrayIndexDelta)[Index];
		}

		int32 GetIndex() const { return Index; }

		bool operator != (const FConstIterator& It ) const { return Index != It.Index || ArrayIndexDelta != It.ArrayIndexDelta; }
	private:
		const FArrayIndexDelta *ArrayIndexDelta;
		int32 Index;
	};

	FConstIterator begin() const { return FConstIterator(this, 0);}
	FConstIterator end() const { return FConstIterator(this, Num());}

	/**
	 * Gather the needed values from InSource to OutDest, according to the delta.
	 * If there is no delta, it will perform a bulk copy.
	 */
	template <typename ValueType>
	void Gather(TArray<ValueType> &OutDest, const TArrayView<const ValueType> &InSource, int32 ElementStride) const
	{
		if (IsDelta())
		{
			OutDest.Reset(Num() * ElementStride);
			for (int32 Index : *this)
			{
				OutDest.Append(&InSource[Index * ElementStride], ElementStride);
			}
		}
		else
		{
			OutDest = InSource;
		}
	}

	/**
	 * Gather the needed values from InSource to OutDest, according to the delta, calling TransformLambda on each element.
	 * Never performs bulk copy.
	 */
	template <typename OutValueType, typename InValueArray, typename LambdaType>
	void GatherTransform(TArray<OutValueType> &OutDest, const InValueArray &InSource, LambdaType TransformLambda) const
	{
		for (int32 Index : *this)
		{
			OutDest.Add(TransformLambda(InSource[Index]));
		}
	}

	/**
	 * Write InSource that was previously gathered using the above methods to the final destination array OutDest
	 * using the same delta information. 
	 * If there is no delta, it performs a move of the source data to the final array, saving a copy.
	 */
	template <typename ValueType>
	void Scatter(TArray<ValueType> &OutDest, int32 DestNumElements, TArray<ValueType> &&InSource, int32 ElementStride = 1) const
	{
		check(InSource.Num() == Num() * ElementStride);
		if (IsDelta())
		{
			OutDest.SetNumUninitialized(DestNumElements * ElementStride);
			for (int32 ItemIndex = 0; ItemIndex < DeltaIndices.Num(); ++ItemIndex)
			{
				int32 DestIndex = DeltaIndices[ItemIndex];
				FMemory::Memcpy(&OutDest[DestIndex * ElementStride], &InSource[ItemIndex * ElementStride], ElementStride * sizeof(ValueType));
			}
		}
		else
		{
			check(InSource.Num() == DestNumElements * ElementStride);
			OutDest = MoveTemp(InSource);
		}
	}
	/**
	 * Write InSource that was previously gathered using the above methods to the final destination array OutDest
	 * using the same delta information. 
	 * Never takes move shortcut as there is an index remap.
	 */
	template <typename ValueType, typename DestIndexRemapType>
	void Scatter(TArray<ValueType> &OutDest, int32 DestNumElements, const TArray<ValueType> &InSource, const DestIndexRemapType &DestIndexRemap, int32 ElementStride = 1) const
	{
		check(InSource.Num() == Num() * ElementStride);
		OutDest.SetNumUninitialized(DestNumElements * ElementStride);
		if (IsDelta())
		{
			for (int32 ItemIndex = 0; ItemIndex < DeltaIndices.Num(); ++ItemIndex)
			{
				int32 DestIndex = DestIndexRemap[DeltaIndices[ItemIndex]];
				if (DestIndex != INDEX_NONE)
				{
					FMemory::Memcpy(&OutDest[DestIndex * ElementStride], &InSource[ItemIndex * ElementStride], ElementStride * sizeof(ValueType));
				}
			}
		}
		else
		{
			for (int32 ItemIndex = 0; ItemIndex < NumElements; ++ItemIndex)
			{
				int32 DestIndex = DestIndexRemap[ItemIndex];
				if (DestIndex != INDEX_NONE)
				{
					FMemory::Memcpy(&OutDest[DestIndex * ElementStride], &InSource[ItemIndex * ElementStride], ElementStride * sizeof(ValueType));
				}
			}
		}
	}
private:
	int32 NumElements = -1; // Number of elements if there is no delta (AKA identity mapping) otherwise -1 and the number is the size of DeltaIndices
	TArray<int32> DeltaIndices;
};


class FISMInstanceUpdateChangeSet
{
public:
	FISMInstanceUpdateChangeSet(bool bInNeedFullUpdate) : bNeedFullUpdate(bInNeedFullUpdate) {}
#if WITH_EDITOR
	// Set editor data 
	void SetEditorData(const TArray<TRefCountPtr<HHitProxy>>& HitProxies, const TBitArray<> &SelectedInstances);//, bool bWasHitProxiesReallocated);
#endif

	TFunction<void(TArray<float> &InstanceRandomIDs)> GeneratePerInstanceRandomIds;

	/**
	 * Add a value, must be done in the order represented in the InstanceLightShadowUVBiasDelta.
	 */
	inline void AddInstanceLightShadowUVBias(const FVector4f &Value)
	{
		InstanceLightShadowUVBias.Emplace(Value); 
	}

	inline void SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms, const FVector Offset)
	{
		TransformsDelta.GatherTransform(Transforms, InInstanceTransforms, [Offset](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M.ConcatTranslation(Offset)); });
	}

	inline void SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms)
	{
		TransformsDelta.GatherTransform(Transforms, InInstanceTransforms, [](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M); });
	}

	ENGINE_API void SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms, const FVector &Offset);
	ENGINE_API void SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms);

	ENGINE_API void SetCustomData(const TArrayView<const float> &InPerInstanceCustomData, int32 InNumCustomDataFloats);
	
	ENGINE_API void SetInstanceLocalBounds(const FRenderBounds &Bounds);

	bool IsFullUpdate() const
	{
		return bNeedFullUpdate;
	}

	bool bNeedFullUpdate;
	FChangeMask ChangeMask;

	FInstanceIdIndexMap InstanceIdIndexMap;
	int32 NumCustomDataFloats;

	// TODO: Make it possible to share the deltas such that we don't have to produce an unique index array for each attribute
	//       e.g., if the only thing that happened was somethis was added and we therefore need all attributes for one instance.
	FArrayIndexDelta TransformsDelta;
	TArray<FRenderTransform> Transforms;
	TArray<FRenderTransform> PrevTransforms;

	FArrayIndexDelta CustomDataDelta;
	TArray<float> PerInstanceCustomData;

	FArrayIndexDelta InstanceLightShadowUVBiasDelta;
	TArray<FVector4f> InstanceLightShadowUVBias;
	FInstanceDataFlags Flags;
#if WITH_EDITOR
	FArrayIndexDelta InstanceEditorDataDelta;
	TArray<uint32> InstanceEditorData;
	TBitArray<> SelectedInstances;
	TPimplPtr<FOpaqueHitProxyContainer> HitProxyContainer;
#endif
	TArray<FRenderBounds, TInlineAllocator<1>> InstanceLocalBounds;
	TArray<int32> LegacyInstanceReorderTable;
	FRenderTransform PrimitiveToRelativeWorld;
	FVector PrimitiveWorldSpaceOffset;
	TOptional<FRenderTransform> PreviousPrimitiveToRelativeWorld;
	float AbsMaxDisplacement = 0.0f;
	int32 PostUpdateNumInstances = 0;
};


#if WITH_EDITOR

ENGINE_API TPimplPtr<FOpaqueHitProxyContainer> MakeOpaqueHitProxyContainer(const TArray<TRefCountPtr<HHitProxy>>& InHitProxies);


#endif
