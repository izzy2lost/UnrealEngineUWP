// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStack.h"
#include "Param/ParamHelpers.h"
#include "PropertyBag.h"
#include "EngineLogs.h"
#include "Param/ParamAdapter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Param/ParamStackLayer.h"
#include "UObject/ObjectKey.h"

#define LOCTEXT_NAMESPACE "AnimNextParamStack"

namespace UE::AnimNext
{

// Stack layer that can own its own data as a UObject or reference an externally owned object
struct FUObjectLayer : FParamStackLayer
{
	FUObjectLayer() = delete;

	explicit FUObjectLayer(UObject* InObject, bool bInMutable)
	{
		Object = InObject;

		const UClass* Class = InObject->GetClass();

		TArray<FParamId, TInlineAllocator<128>> CachedIds;
		TArray<FProperty*, TInlineAllocator<128>> CachedProperties;
		TArray<FParamTypeHandle, TInlineAllocator<128>> CachedParamTypes;

		// Determine param ID range for this layer
		MinParamId = MAX_uint32;
		uint32 MaxParamId = 0;

		for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FParamTypeHandle ParamTypeHandle = FParamTypeHandle::FromProperty(*PropIt);
			if(ParamTypeHandle.IsValid())
			{
				CachedParamTypes.Add(ParamTypeHandle);
				CachedProperties.Add(*PropIt);
				const FParamId& ParamId = CachedIds.Add_GetRef(FParamId(PropIt->GetFName()));
				MinParamId = FMath::Min(ParamId.ToInt(), MinParamId);
				MaxParamId = FMath::Max(ParamId.ToInt(), MaxParamId);
			}
		}

		if (MinParamId <= MaxParamId)
		{
			const uint32 ParamRangeSize = (MaxParamId - MinParamId) + 1;
			Params.SetNumZeroed(ParamRangeSize);
			for (uint32 PropertyIndex = 0; PropertyIndex < static_cast<uint32>(CachedProperties.Num()); ++PropertyIndex)
			{
				const FProperty* Property = CachedProperties[PropertyIndex];
				const FParamId& ParamId = CachedIds[PropertyIndex];
				uint8* DataPtr = Property->ContainerPtrToValuePtr<uint8>(InObject);
				const uint32 LocalParamIndex = ParamId.ToInt() - MinParamId;
				Params[LocalParamIndex] = Private::FParamEntry(CachedParamTypes[PropertyIndex], TArrayView<uint8>(DataPtr, Property->GetSize()), true, bInMutable);
			}
		}
	}

	// FParamStackLayer interface
	virtual UObject* AsUObject() override
	{
		return Object.Get();
	}

	TWeakObjectPtr<UObject> Object;
};

// Stack layer that can own its own data or reference an external FInstancedPropertyBag
struct FInstancedPropertyBagLayer : FParamStackLayer, FGCObject
{
	FInstancedPropertyBagLayer() = delete;

	explicit FInstancedPropertyBagLayer(FInstancedPropertyBag& InPropertyBag, bool bInMutable)
	{
		if (const UPropertyBag* PropertyBagStruct = InPropertyBag.GetPropertyBagStruct())
		{
			TConstArrayView<FPropertyBagPropertyDesc> Descs = PropertyBagStruct->GetPropertyDescs();

			// Determine param ID range for this layer
			TArray<FParamId, TInlineAllocator<128>> CachedIds;
			CachedIds.SetNumUninitialized(Descs.Num());
			MinParamId = MAX_uint32;
			uint32 MaxParamId = 0;
			for (uint32 DescIndex = 0; DescIndex < static_cast<uint32>(Descs.Num()); ++DescIndex)
			{
				const FPropertyBagPropertyDesc& Desc = Descs[DescIndex];
				const FParamId& ParamId = CachedIds[DescIndex] = FParamId(Desc.Name);
				MinParamId = FMath::Min(ParamId.ToInt(), MinParamId);
				MaxParamId = FMath::Max(ParamId.ToInt(), MaxParamId);
			}

			if (MinParamId <= MaxParamId)
			{
				const uint32 ParamRangeSize = (MaxParamId - MinParamId) + 1;
				Params.SetNumZeroed(ParamRangeSize);
				FStructView StructView = InPropertyBag.GetMutableValue();
				for (uint32 DescIndex = 0; DescIndex < static_cast<uint32>(Descs.Num()); ++DescIndex)
				{
					const FPropertyBagPropertyDesc& Desc = Descs[DescIndex];
					const FParamId& ParamId = CachedIds[DescIndex];
					uint8* DataPtr = StructView.GetMemory() + Desc.CachedProperty->GetOffset_ForInternal();
					const uint32 LocalParamIndex = ParamId.ToInt() - MinParamId;
					Params[LocalParamIndex] = Private::FParamEntry(FParamTypeHandle::FromPropertyBagPropertyDesc(Desc), TArrayView<uint8>(DataPtr, Desc.CachedProperty->GetSize()), true, true);
				}
			}
		}
	}

	// FGCObject interface
	virtual FString GetReferencerName() const override
	{
		return TEXT("AnimNext Instanced Property Bag Parameter Layer");
	}
};

// Stack layer that owns its own data as a FInstancedPropertyBag
struct FInstancedPropertyBagValueLayer : FInstancedPropertyBagLayer
{
	FInstancedPropertyBagValueLayer() = delete;

	explicit FInstancedPropertyBagValueLayer(FInstancedPropertyBag&& InPropertyBag, bool bInMutable)
		: FInstancedPropertyBagLayer(InPropertyBag, bInMutable)
		, PropertyBag(MoveTemp(InPropertyBag))
	{}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		PropertyBag.AddStructReferencedObjects(Collector);
	}

	// FParamStackLayer interface
	virtual FInstancedPropertyBag* AsInstancedPropertyBag() override
	{
		return &PropertyBag;
	}

	FInstancedPropertyBag PropertyBag;
};

// Stack layer that references an external FInstancedPropertyBag
struct FInstancedPropertyBagReferenceLayer : FInstancedPropertyBagLayer
{
	FInstancedPropertyBagReferenceLayer() = delete;

	explicit FInstancedPropertyBagReferenceLayer(FInstancedPropertyBag& InPropertyBag, bool bInMutable)
		: FInstancedPropertyBagLayer(InPropertyBag, bInMutable)
		, PropertyBag(InPropertyBag)
	{}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) { /* We do not own the references held here, assume they are accounted for elsewhere */ }

	// FParamStackLayer interface
	virtual FInstancedPropertyBag* AsInstancedPropertyBag() override
	{
		return &PropertyBag;
	}

	FInstancedPropertyBag& PropertyBag;
};

FParamStack::FPushedLayer::FPushedLayer(FParamStackLayer& InLayer, FParamStack& InStack)
	: Layer(InLayer)
{
	SerialNumber = InStack.MakeSerialNumber();

	if (Layer.Params.Num())
	{
		InStack.ResizeLayerIndices();

		for (uint32 LocalParamIndex = 0; LocalParamIndex < (uint32)InLayer.Params.Num(); ++LocalParamIndex)
		{
			const uint32 GlobalParamIndex = Layer.MinParamId + LocalParamIndex;
			InStack.LayerIndices[GlobalParamIndex] = InLayer.Params[LocalParamIndex].IsValid() ? 0 : MAX_uint16;
		}
	}
}

FParamStack::FPushedLayer::FPushedLayer(const FPushedLayer& InPreviousLayer, FParamStackLayer& InLayer, FParamStack& InStack)
	: Layer(InLayer)
{
	SerialNumber = InStack.MakeSerialNumber();

	if (Layer.Params.Num())
	{
		InStack.ResizeLayerIndices();

		const uint32 NumParams = Layer.Params.Num();
		const uint32 BaseLayerIndex = InStack.PreviousLayerIndices.Num();
		InStack.PreviousLayerIndices.SetNum(BaseLayerIndex + NumParams);
		PreviousLayerIndexStart = BaseLayerIndex;
		
		// Update layer indices and build previous layer indices
		for (uint32 LocalParamIndex = 0; LocalParamIndex < NumParams; ++LocalParamIndex)
		{
			const uint32 GlobalParamIndex = Layer.MinParamId + LocalParamIndex;
			InStack.PreviousLayerIndices[BaseLayerIndex + LocalParamIndex] = InStack.LayerIndices[GlobalParamIndex];
			if(InLayer.Params[LocalParamIndex].IsValid())
			{
				InStack.LayerIndices[GlobalParamIndex] = InStack.Layers.Num() - 1;
			}
		}
	}
}

// Current stack assigned to this thread
static thread_local TWeakPtr<FParamStack> GWeakStack;

// Stacks that are asscociated with objects, pending execution of an object's tick function
static TMap<TObjectKey<UObject>, TWeakPtr<FParamStack>> GPendingObjects;
static FRWLock GPendingObjectsLock;

FParamStack::FParamStack()
{
	Layers.Reserve(8);
	LayerIndices.Reserve(FParamId::GetMaxParamId().ToInt());
	PreviousLayerIndices.Reserve(FParamId::GetMaxParamId().ToInt());
}

FParamStack::~FParamStack()
{
}

void FParamStack::SetParent(TWeakPtr<const FParamStack> InParent)
{
	WeakParentStack = InParent;
}

FParamStack& FParamStack::Get()
{
	return *GWeakStack.Pin().Get();
}

TWeakPtr<FParamStack> FParamStack::GetForCurrentThread()
{
	return GWeakStack;
}

void FParamStack::AddForPendingObject(const UObject* InObject, TWeakPtr<FParamStack> InStack)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_Write);
	if(!GPendingObjects.Contains(InObject))
	{
		GPendingObjects.Add(InObject, InStack);
	}
}

void FParamStack::RemoveForPendingObject(const UObject* InObject)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_Write);
	GPendingObjects.Remove(InObject);
}

bool FParamStack::AttachToCurrentThreadForPendingObject(const UObject* InObject)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_ReadOnly);
	if (TWeakPtr<FParamStack>* PendingStack = GPendingObjects.Find(InObject))
	{
		GWeakStack = *PendingStack;
		return true;
	}

	return false;
}

bool FParamStack::DetachFromCurrentThreadForPendingObject(const UObject* InObject)
{
	FRWScopeLock ScopeLock(GPendingObjectsLock, SLT_ReadOnly);
	if (TWeakPtr<FParamStack>* PendingStack = GPendingObjects.Find(InObject))
	{
		GWeakStack.Reset();
		return true;
	}

	return false;
}

void FParamStack::AttachToCurrentThread(TWeakPtr<FParamStack> InStack)
{
	GWeakStack = InStack;
}

TWeakPtr<FParamStack> FParamStack::DetachFromCurrentThread()
{
	TWeakPtr<FParamStack> Stack = GWeakStack;
	GWeakStack.Reset();
	return Stack;
}

FParamStack::FPushedLayerHandle FParamStack::PushLayer(const FParamStackLayerHandle& InLayerHandle)
{
	return PushLayerInternal(*InLayerHandle.Layer.Get());
}

FParamStack::FPushedLayerHandle FParamStack::PushLayer(TConstArrayView<TPair<FParamId, Private::FParamEntry>> InParams)
{
	if(Layers.Num() < MAX_uint16)
	{
		FParamStackLayer& OwnedLayer = OwnedStackLayers.Add_GetRef(FParamStackLayer(InParams));
		OwnedLayer.OwnedStorageOffset = AllocAndCopyOwnedParamStorage(OwnedLayer.Params);
		return PushLayerInternal(OwnedLayer);
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("FParamStack: Could not push a layer: Maximum 65535 stack layers."));
		return FPushedLayerHandle();
	}
}

FParamStack::FPushedLayerHandle FParamStack::PushLayerInternal(FParamStackLayer& InLayer)
{
	if(InLayer.Params.Num() > 0)
	{
		if (Layers.Num() < MAX_uint16)
		{
			if (Layers.Num())
			{
				FPushedLayer& NewPushedLayer = Layers.Emplace_GetRef(Layers.Top(), InLayer, *this);
				return FPushedLayerHandle(Layers.Num() - 1, NewPushedLayer.SerialNumber);
			}
			else
			{
				FPushedLayer& NewPushedLayer = Layers.Emplace_GetRef(InLayer, *this);
				return FPushedLayerHandle(Layers.Num() - 1, NewPushedLayer.SerialNumber);
			}
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("FParamStack: Could not push a layer: Maximum 65535 stack layers."));
		}
	}

	return FPushedLayerHandle();
}

void FParamStack::PopLayer(FPushedLayerHandle InHandle)
{
	if(InHandle.IsValid() && Layers.Num() > 0)
	{
		const FPushedLayer& TopLayer = Layers.Top();

		checkf(TopLayer.SerialNumber == InHandle.SerialNumber && (uint32)Layers.Num() - 1 == InHandle.Index, 
			TEXT("UE::AnimNext::FParamStack::PopLayer: Invalid layer handle supplied (Have: %u, %u, Expected: %u, %u)"), 
			InHandle.Index, InHandle.SerialNumber, (uint32)Layers.Num() - 1, TopLayer.SerialNumber);

		// Fixup layer indices to previous, if any
		const uint32 NumParams = TopLayer.Layer.Params.Num();
		if(NumParams > 0 && PreviousLayerIndices.Num() > 0)
		{
			for (uint32 LocalParamIndex = 0; LocalParamIndex < (uint32)NumParams; ++LocalParamIndex)
			{
				const uint16 PreviousLayerIndex = PreviousLayerIndices[TopLayer.PreviousLayerIndexStart + LocalParamIndex];
				if (PreviousLayerIndex != MAX_uint16)
				{
					const uint32 GlobalParamIndex = TopLayer.Layer.MinParamId + LocalParamIndex;
					LayerIndices[GlobalParamIndex] = PreviousLayerIndex;
				}
			}

			PreviousLayerIndices.SetNum(PreviousLayerIndices.Num() - NumParams);
		}
		else
		{
			// Clear layer indices as this is the last layer to be popped
			FMemory::Memset(&LayerIndices[0], 0xff, LayerIndices.Num() * LayerIndices.GetTypeSize());
		}

		// Dont shrink allocs to avoid thrashing
		constexpr bool bAllowShrinking = false;

		// If we own the layer, pop the owned stack
		if(TopLayer.Layer.OwnedStorageOffset != MAX_uint32)
		{
			check(OwnedStackLayers.Num() && &TopLayer.Layer == &OwnedStackLayers[OwnedStackLayers.Num() - 1]);
			OwnedStackLayers.Pop(bAllowShrinking);

			// Free any owned storage
			FreeOwnedParamStorage(TopLayer.Layer.OwnedStorageOffset);
		}

		// Pop the layer itself
		Layers.Pop(bAllowShrinking);
	}
}

FParamStackLayerHandle FParamStack::MakeValueLayer(const UClass* InClass)
{
	check(InClass != nullptr);

	UObject* OwnedObject = NewObject<UObject>(GetTransientPackage(), InClass);
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FUObjectLayer>(OwnedObject, true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeReferenceLayer(UObject* InObject)
{
	check(InObject != nullptr);

	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FUObjectLayer>(InObject, true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeValueLayer(const FInstancedPropertyBag& InPropertyBag)
{
	FInstancedPropertyBag OwnedPropertyBag = InPropertyBag;
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FInstancedPropertyBagValueLayer>(MoveTemp(OwnedPropertyBag), true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeReferenceLayer(FInstancedPropertyBag& InPropertyBag)
{
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FInstancedPropertyBagReferenceLayer>(InPropertyBag, true);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamStackLayerHandle FParamStack::MakeLayer(TConstArrayView<TPair<FParamId, Private::FParamEntry>> InParams)
{
	TUniquePtr<FParamStackLayer> Layer = MakeUnique<FParamStackLayer>(InParams);
	return FParamStackLayerHandle(MoveTemp(Layer));
}

FParamResult FParamStack::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility) const
{
	FParamTypeHandle ParamTypeHandle;
	return GetParamData(InId, InTypeHandle, OutParamData, ParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStack::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	const FParamResult Result = GetParamDataInternal(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if (Result.IsInScope())
	{
		return Result;
	}
	else if (TSharedPtr<const FParamStack> ParentStack = WeakParentStack.Pin())
	{
		return ParentStack->GetParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	}
	// Check if we have a built in adapter to fall back on
	else if(const FParamAdapter* Adapter = InId.GetAdapter(InId))
	{
		return Adapter->GetParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	}

	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	if (Layers.Num() == 0 || InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return EParamResult::NotInScope;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const FParamResult Result = Layer.Layer.GetParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if(Result.IsInScope())
	{
		return Result;
	}



	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility)
{
	FParamTypeHandle ParamTypeHandle;
	return GetMutableParamData(InId, InTypeHandle, OutParamData, ParamTypeHandle, InRequiredCompatibility);
}

FParamResult FParamStack::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	const FParamResult Result = GetMutableParamDataInternal(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if (Result.IsInScope())
	{
		return Result;
	}
	else if (TSharedPtr<const FParamStack> ParentStack = WeakParentStack.Pin())
	{
		// we use a dummy here because if the data is present in a parent, it must be immutable anyways and we will early out
		TConstArrayView<uint8> ParamData;
		FParamResult ParentResult = ParentStack->GetParamData(InId, InTypeHandle, ParamData, OutParamTypeHandle, InRequiredCompatibility);
		if (ParentResult.IsInScope())
		{
			// Parent data is immutable
			return ParentResult.Result & EParamResult::MutabilityError;
		}
	}
	// Check if we have a built in adapter to fall back on
	else if(const FParamAdapter* Adapter = InId.GetAdapter(InId))
	{
		return Adapter->GetMutableParamData(InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	}

	return EParamResult::NotInScope;
}

FParamResult FParamStack::GetMutableParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	if (Layers.Num() == 0 || InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return EParamResult::NotInScope;
	}

	FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const FParamResult Result =  Layer.Layer.GetMutableParamData(InId, InTypeHandle, OutParamData, OutParamTypeHandle, InRequiredCompatibility);
	if(Result.IsInScope())
	{
		return Result;
	}

	return EParamResult::NotInScope;
}

bool FParamStack::IsMutableParam(FParamId InId) const
{
	if (Layers.Num() == 0 || InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return false;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	const Private::FParamEntry& Param = Layer.Layer.Params[LocalParamIndex];

	return Param.IsMutable();
}

bool FParamStack::IsReferenceParam(FParamId InId) const
{
	if (Layers.Num() == 0 || InId.ToInt() >= (uint32)LayerIndices.Num() || LayerIndices[InId.ToInt()] == MAX_uint16)
	{
		return false;
	}

	const FPushedLayer& Layer = Layers[LayerIndices[InId.ToInt()]];
	const uint32 LocalParamIndex = InId.ToInt() - Layer.Layer.MinParamId;

	const Private::FParamEntry& Param = Layer.Layer.Params[LocalParamIndex];

	return Param.IsReference();
}

bool FParamStack::LayerContainsParam(const FParamStackLayerHandle& InHandle, FName InKey)
{
	const FParamId ParamIdToFind(InKey);
	const uint32 MinParamId = InHandle.Layer->MinParamId;
	const uint32 NumParams = InHandle.Layer->Params.Num();
	if (ParamIdToFind.ToInt() >= MinParamId && ParamIdToFind.ToInt() < MinParamId + NumParams)
	{
		const uint32 LocalParamIndex = ParamIdToFind.ToInt() - MinParamId;
		return InHandle.Layer->Params[LocalParamIndex].IsValid();
	}
	return false;
}

uint32 FParamStack::AllocAndCopyOwnedParamStorage(TArrayView<Private::FParamEntry> InParams)
{
	const uint32 CurrentOffset = OwnedLayerParamStorage.Num();
	const uint32 PageSize = OwnedLayerParamStorage.MaxPerPage();

	for (Private::FParamEntry& Param : InParams)
	{
		if(Param.IsValid())
		{
			FParamTypeHandle TypeHandle = Param.GetTypeHandle();
			const uint32 Size = TypeHandle.GetSize();

			// Dont copy references or embdedded values to internal storage
			// - References should refer back to the originally-passed in data
			// - Embedded do not need their own storage 
			if (!Param.IsReference() && !Param.IsEmbedded())
			{
				const uint32 Alignment = TypeHandle.GetAlignment();
				uint32 AllocSize = Align(Size, Alignment);
				const uint32 NumPages = OwnedLayerParamStorage.NumPages();
				check(AllocSize < PageSize);

				// Do we need a new page? If so extend allocated size to the next page boundary
				const uint32 PageMax = (NumPages + 1) * PageSize;
				uint32 BaseOffset = Align(CurrentOffset, Alignment);
				if (BaseOffset + AllocSize > PageMax)
				{
					OwnedLayerParamStorage.SetNum((NumPages + 1) * PageSize);
				}

				// Extend to encompass requested size
				BaseOffset = Align(OwnedLayerParamStorage.Num(), Alignment);
				OwnedLayerParamStorage.SetNum(BaseOffset + AllocSize);
		
				// Copy param data
				TArrayView<uint8> TargetMemory(&OwnedLayerParamStorage[BaseOffset], Size);
				FParamHelpers::Copy(TypeHandle, TypeHandle, Param.GetData(), TargetMemory);

				// Update param to reference new storage
				Param.Data = TargetMemory.GetData();
			}
		}
	}

	return CurrentOffset;
}

void FParamStack::FreeOwnedParamStorage(uint32 InOffset)
{
	check(InOffset <= (uint32)OwnedLayerParamStorage.Num());

	OwnedLayerParamStorage.SetNum(InOffset, false);
}

void FParamStack::ResizeLayerIndices()
{
	const uint32 NumParams = FParamId::GetMaxParamId().ToInt();
	const uint32 NumLayerIndices = LayerIndices.Num();

	if (NumParams > NumLayerIndices)
	{
		LayerIndices.SetNum(NumParams);
		FMemory::Memset(&LayerIndices[NumLayerIndices], 0xff, (NumParams - NumLayerIndices) * sizeof(uint16));
	}
}

uint32 FParamStack::MakeSerialNumber()
{
	++SerialNumber;
	if (SerialNumber == 0)
	{
		++SerialNumber;
	}
	return SerialNumber;
}

}

#undef LOCTEXT_NAMESPACE