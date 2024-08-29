// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyPoseAdapter.h"
#include "Rigs/RigHierarchy.h"

URigHierarchy* FRigHierarchyPoseAdapter::GetHierarchy() const
{
	if(WeakHierarchy.IsValid())
	{
		return WeakHierarchy.Get();
	}
	return nullptr;
}

void FRigHierarchyPoseAdapter::PostLinked(URigHierarchy* InHierarchy)
{
	WeakHierarchy = InHierarchy; 
}

void FRigHierarchyPoseAdapter::PreUnlinked(URigHierarchy* InHierarchy)
{
}

void FRigHierarchyPoseAdapter::PostUnlinked(URigHierarchy* InHierarchy)
{
	WeakHierarchy.Reset();
}

bool FRigHierarchyPoseAdapter::RelinkTransformStorage(const FRigElementKeyAndIndex& InKeyAndIndex, ERigTransformType::Type InTransformType,
	ERigTransformStorageType::Type InStorageType, FTransform* InTransformStorage, bool* InDirtyFlagStorage)
{
	TArray<TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type, FTransform*, bool*>> Data =
		{{InKeyAndIndex, InTransformType, InStorageType, InTransformStorage, InDirtyFlagStorage}};
	return RelinkTransformStorage(Data);
}

bool FRigHierarchyPoseAdapter::RestoreTransformStorage(const FRigElementKeyAndIndex& InKeyAndIndex, ERigTransformType::Type InTransformType,
	ERigTransformStorageType::Type InStorageType, bool bUpdateElementStorage)
{
	TArray<TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type>> Data =
		{{InKeyAndIndex, InTransformType, InStorageType}};
	return RestoreTransformStorage(Data, bUpdateElementStorage);
}

bool FRigHierarchyPoseAdapter::RelinkTransformStorage(
	const TArrayView<TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type, FTransform*, bool*>>& InData)
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		TArray<int32> TransformIndicesToDeallocate;
		TArray<int32> DirtyStateIndicesToDeallocate;
		TransformIndicesToDeallocate.Reserve(InData.Num());
		DirtyStateIndicesToDeallocate.Reserve(InData.Num());
		
		bool bPerformedChange = false;
		for(const TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type, FTransform*, bool*>& Tuple : InData)
		{
			auto CurrentStorage = Hierarchy->GetElementTransformStorage(Tuple.Get<0>(), Tuple.Get<1>(), Tuple.Get<2>());

			if(FTransform* NewTransformStorage = Tuple.Get<3>())
			{
				if(Hierarchy->ElementTransforms.Contains(CurrentStorage.Get<0>()))
				{
					TransformIndicesToDeallocate.Add(CurrentStorage.Get<0>()->GetStorageIndex());
				}
				CurrentStorage.Get<0>()->StorageIndex = INDEX_NONE;
				CurrentStorage.Get<0>()->Storage = NewTransformStorage;
				bPerformedChange = true;
			}
			if(bool* NewDirtyStateStorage = Tuple.Get<4>())
			{
				if(Hierarchy->ElementDirtyStates.Contains(CurrentStorage.Get<1>()))
				{
					DirtyStateIndicesToDeallocate.Add(CurrentStorage.Get<1>()->GetStorageIndex());
				}
				CurrentStorage.Get<1>()->StorageIndex = INDEX_NONE;
				CurrentStorage.Get<1>()->Storage = NewDirtyStateStorage;
				bPerformedChange = true;
			}
		}

		Hierarchy->ElementTransforms.Deallocate(TransformIndicesToDeallocate);
		Hierarchy->ElementDirtyStates.Deallocate(DirtyStateIndicesToDeallocate);
		return bPerformedChange;
	}
	return false;
}

bool FRigHierarchyPoseAdapter::RestoreTransformStorage(
	const TArrayView<TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type>>& InData, bool bUpdateElementStorage)
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		TArray<TTuple<FRigComputedTransform*, FRigTransformDirtyState*>> StoragePerElement;
		StoragePerElement.Reserve(InData.Num());
		for(const TTuple<FRigElementKeyAndIndex, ERigTransformType::Type, ERigTransformStorageType::Type>& Tuple : InData)
		{
			const TTuple<FRigComputedTransform*, FRigTransformDirtyState*> CurrentStorage =
				Hierarchy->GetElementTransformStorage(Tuple.Get<0>(), Tuple.Get<1>(), Tuple.Get<2>());

			if(CurrentStorage.Get<0>() == nullptr || CurrentStorage.Get<1>() == nullptr)
			{
				continue;
			}
			if(Hierarchy->ElementTransforms.Contains(CurrentStorage.Get<0>()) ||
				Hierarchy->ElementDirtyStates.Contains(CurrentStorage.Get<1>()))
			{
				continue;
			}
			StoragePerElement.Add(CurrentStorage);
		}

		if(StoragePerElement.IsEmpty())
		{
			return false;
		}

		const TArray<int32, TInlineAllocator<4>> NewTransformIndices = Hierarchy->ElementTransforms.Allocate(StoragePerElement.Num(), FTransform::Identity);
		const TArray<int32, TInlineAllocator<4>> NewDirtyStateIndices = Hierarchy->ElementDirtyStates.Allocate(StoragePerElement.Num(), false);
		check(StoragePerElement.Num() == NewTransformIndices.Num());
		check(StoragePerElement.Num() == NewDirtyStateIndices.Num());
		for(int32 Index = 0; Index < StoragePerElement.Num(); Index++)
		{
			StoragePerElement[Index].Get<0>()->StorageIndex = NewTransformIndices[Index];
			StoragePerElement[Index].Get<1>()->StorageIndex = NewDirtyStateIndices[Index];
		}
		
		if(bUpdateElementStorage)
		{
			(void)UpdateElementStorage();
			(void)SortStorage();
		}
		return true;
	}
	return false;
}

bool FRigHierarchyPoseAdapter::RelinkCurveStorage(const FRigElementKeyAndIndex& InKeyAndIndex, float* InCurveStorage)
{
	TArray<TTuple<FRigElementKeyAndIndex, float*>> Data = {{InKeyAndIndex, InCurveStorage}};
	return RelinkCurveStorage(Data);
}

bool FRigHierarchyPoseAdapter::RestoreCurveStorage(const FRigElementKeyAndIndex& InKeyAndIndex, bool bUpdateElementStorage)
{
	TArray<FRigElementKeyAndIndex> Data = {InKeyAndIndex};
	return RestoreCurveStorage(Data, bUpdateElementStorage);
}

bool FRigHierarchyPoseAdapter::RelinkCurveStorage(const TArrayView<TTuple<FRigElementKeyAndIndex, float*>>& InData)
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		TArray<int32> CurveIndicesToDeallocate;
		CurveIndicesToDeallocate.Reserve(InData.Num());
		
		bool bPerformedChange = false;
		for(const TTuple<FRigElementKeyAndIndex, float*>& Tuple : InData)
		{
			FRigCurveElement* CurveElement = Hierarchy->Get<FRigCurveElement>(Tuple.Get<0>());

			if(float* NewCurveStorage = Tuple.Get<1>())
			{
				if(Hierarchy->ElementCurves.Contains(CurveElement))
				{
					CurveIndicesToDeallocate.Add(CurveElement->GetStorageIndex());
				}
				CurveElement->StorageIndex = INDEX_NONE;
				CurveElement->Storage = NewCurveStorage;
				bPerformedChange = true;
			}
		}

		Hierarchy->ElementCurves.Deallocate(CurveIndicesToDeallocate);
		return bPerformedChange;
	}
	return false;
}

bool FRigHierarchyPoseAdapter::RestoreCurveStorage(const TArrayView<FRigElementKeyAndIndex>& InData, bool bUpdateElementStorage)
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		TArray<FRigCurveElement*> Curves;
		Curves.Reserve(InData.Num());
		for(const FRigElementKeyAndIndex& KeyAndIndex : InData)
		{
			if(FRigCurveElement* CurveElement = Hierarchy->Get<FRigCurveElement>(KeyAndIndex))
			{
				if(!Hierarchy->ElementCurves.Contains(CurveElement))
				{
					Curves.Add(CurveElement);
				}
			}
		}

		if(Curves.IsEmpty())
		{
			return false;
		}

		const TArray<int32, TInlineAllocator<4>> NewCurveIndices = Hierarchy->ElementCurves.Allocate(Curves.Num(), 0.f);
		check(Curves.Num() == NewCurveIndices.Num());
		for(int32 Index = 0; Index < Curves.Num(); Index++)
		{
			Curves[Index]->StorageIndex = NewCurveIndices[Index];
		}

		if(bUpdateElementStorage)
		{
			(void)UpdateElementStorage();
		}
		return true;
	}
	return false;
}

bool FRigHierarchyPoseAdapter::SortStorage()
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		return Hierarchy->SortElementStorage();
	}
	return false;
}

bool FRigHierarchyPoseAdapter::ShrinkStorage()
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		return Hierarchy->ShrinkElementStorage();
	}
	return false;
}

bool FRigHierarchyPoseAdapter::UpdateElementStorage()
{
	if(URigHierarchy* Hierarchy = GetHierarchy())
	{
		Hierarchy->UpdateElementStorage();
		return true;
	}
	return false;
}
