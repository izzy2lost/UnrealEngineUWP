// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTree.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeInstanceData)

namespace UE::StateTree
{

	/**
	 * Duplicates object, and tries to covert old BP classes (REINST_*) to their newer version.
	 */
	UObject* DuplicateNodeInstance(const UObject& Instance, UObject& InOwner)
	{
		const UClass* InstanceClass = Instance.GetClass();
		if (InstanceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			const UClass* AuthoritativeClass = InstanceClass->GetAuthoritativeClass();
			UObject* NewInstance = NewObject<UObject>(&InOwner, AuthoritativeClass);

			// Try to copy the values over using serialization
			// FObjectAndNameAsStringProxyArchive is used to store and restore names and objects as memory writer does not support UObject references at all.
			TArray<uint8> Data;
			FMemoryWriter Writer(Data);
			FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
			UObject& NonConstInstance = const_cast<UObject&>(Instance);
			NonConstInstance.Serialize(WriterProxy);

			FMemoryReader Reader(Data);
			FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
			NewInstance->Serialize(ReaderProxy);

			const UStateTree* OuterStateTree = Instance.GetTypedOuter<UStateTree>();

			UE_LOG(LogStateTree, Display, TEXT("FStateTreeInstanceData: Duplicating '%s' with old class '%s' as '%s', potential data loss. Please resave State Tree asset %s."),
				*GetFullNameSafe(&Instance), *GetNameSafe(InstanceClass), *GetNameSafe(AuthoritativeClass), *GetFullNameSafe(OuterStateTree));

			return NewInstance;
		}

		return DuplicateObject(&Instance, &InOwner);
	}

} // UE::StateTree


//----------------------------------------------------------------//
// FStateTreeInstanceStorage
//----------------------------------------------------------------//

void FStateTreeInstanceStorage::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	constexpr int32 MaxPendingTransitionRequests = 32;
	
	if (TransitionRequests.Num() >= MaxPendingTransitionRequests)
	{
		UE_VLOG_UELOG(Owner, LogStateTree, Error, TEXT("%s: Too many transition requests sent to '%s' (%d pending). Dropping request."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), TransitionRequests.Num());
		return;
	}

	TransitionRequests.Add(Request);
}

void FStateTreeInstanceStorage::ResetTransitionRequests()
{
	TransitionRequests.Reset();
}

bool FStateTreeInstanceStorage::AreAllInstancesValid() const
{
	for (FConstStructView Instance : InstanceStructs)
	{
		if (!Instance.IsValid())
		{
			return false;
		}
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (!Wrapper->InstanceObject)
			{
				return false;
			}
		}
	}
	return true;
}


//----------------------------------------------------------------//
// FStateTreeInstanceData
//----------------------------------------------------------------//

FStateTreeInstanceData::FStateTreeInstanceData()
{
	InstanceStorage.InitializeAs<FStateTreeInstanceStorage>();
}

FStateTreeInstanceData::~FStateTreeInstanceData()
{
	Reset();
}

const FStateTreeInstanceStorage& FStateTreeInstanceData::GetStorage() const
{
	check(InstanceStorage.GetMemory() != nullptr);
	return *reinterpret_cast<const FStateTreeInstanceStorage*>(InstanceStorage.GetMemory());
}

FStateTreeInstanceStorage& FStateTreeInstanceData::GetMutableStorage()
{
	check(InstanceStorage.GetMemory() != nullptr);
	return *reinterpret_cast<FStateTreeInstanceStorage*>(InstanceStorage.GetMutableMemory());
}

FStateTreeEventQueue& FStateTreeInstanceData::GetMutableEventQueue()
{
	return GetMutableStorage().EventQueue;	
}

const FStateTreeEventQueue& FStateTreeInstanceData::GetEventQueue() const
{
	return GetStorage().EventQueue;
}

void FStateTreeInstanceData::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	GetMutableStorage().AddTransitionRequest(Owner, Request);
}

TConstArrayView<FStateTreeTransitionRequest> FStateTreeInstanceData::GetTransitionRequests() const
{
	return GetStorage().GetTransitionRequests();
}

void FStateTreeInstanceData::ResetTransitionRequests()
{
	GetMutableStorage().ResetTransitionRequests();
}

bool FStateTreeInstanceData::AreAllInstancesValid() const
{
	return GetStorage().AreAllInstancesValid();
}

int32 FStateTreeInstanceData::GetEstimatedMemoryUsage() const
{
	const FStateTreeInstanceStorage& Storage = GetStorage();
	int32 Size = sizeof(FStateTreeInstanceData);

	Size += Storage.InstanceStructs.GetAllocatedMemory();

	for (FConstStructView Instance : Storage.InstanceStructs)
	{
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Size += Wrapper->InstanceObject->GetClass()->GetStructureSize();
			}
		}
	}

	return Size;
}

bool FStateTreeInstanceData::Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const
{
	if (Other == nullptr)
	{
		return false;
	}

	// Identical if both are uninitialized.
	if (!IsValid() && !Other->IsValid())
	{
		return true;
	}

	// Not identical if one is valid and other is not.
	if (IsValid() != Other->IsValid())
	{
		return false;
	}

	const FStateTreeInstanceStorage& Storage = GetStorage();
	const FStateTreeInstanceStorage& OtherStorage = Other->GetStorage();

	// Not identical if structs are different.
	if (Storage.InstanceStructs.Identical(&OtherStorage.InstanceStructs, PortFlags) == false)
	{
		return false;
	}
	
	// Check that the instance object contents are identical.
	// Copied from object property.
	auto AreObjectsIdentical = [](UObject* A, UObject* B, uint32 PortFlags) -> bool
	{
		if ((PortFlags & PPF_DuplicateForPIE) != 0)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		// Resolve the object handles and run the deep comparison logic 
		if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
		{
			return FObjectPropertyBase::StaticIdentical(A, B, PortFlags);
		}

		return true;
	};

	bool bResult = true;

	for (int32 Index = 0; Index < Storage.InstanceStructs.Num(); Index++)
	{
		const FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();
		const FStateTreeInstanceObjectWrapper* OtherWrapper = OtherStorage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();

		if (Wrapper)
		{
			if (!OtherWrapper)
			{
				bResult = false;
				break;
			}
			if (Wrapper->InstanceObject && OtherWrapper->InstanceObject)
			{
				if (!AreObjectsIdentical(Wrapper->InstanceObject, OtherWrapper->InstanceObject, PortFlags))
				{
					bResult = false;
					break;
				}
			}
		}
	}
	
	return bResult;
}

void FStateTreeInstanceData::CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther)
{
	if (&InOther == this)
	{
		return;
	}

	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	const FStateTreeInstanceStorage& OtherStorage = InOther.GetStorage();

	// Copy structs
	Storage.InstanceStructs = OtherStorage.InstanceStructs;

	// Copy instance objects.
	for (FStructView Instance : Storage.InstanceStructs)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs)
{
	Reset();
	Append(InOwner, InStructs);
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs)
{
	Reset();
	Append(InOwner, InStructs);
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);
	
	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Wrapper->InstanceObject = UE::StateTree::DuplicateNodeInstance(*Wrapper->InstanceObject, InOwner);
			}
		}
	}
}

void FStateTreeInstanceData::ShrinkTo(const int32 NumStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	check(NumStructs <= Storage.InstanceStructs.Num());  
	Storage.InstanceStructs.SetNum(NumStructs);
}

bool FStateTreeInstanceData::IsValid() const
{
	return InstanceStorage.IsValid();
}

void FStateTreeInstanceData::Reset()
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	Storage.InstanceStructs.Reset();
	Storage.EventQueue.Reset();
	ExecutionState.Reset();
}
