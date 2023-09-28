// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMExecuteContext)

void FRigVMExecuteContext::SetOwningComponent(const USceneComponent* InOwningComponent)
{
	OwningComponent = InOwningComponent;
	OwningActor = nullptr;
	World = nullptr;
	ToWorldSpaceTransform = FTransform::Identity;
	
	if(OwningComponent)
	{
		ToWorldSpaceTransform = OwningComponent->GetComponentToWorld();
		SetOwningActor(OwningComponent->GetOwner());
	}
}

void FRigVMExecuteContext::SetOwningActor(const AActor* InActor)
{
	OwningActor = InActor;
	World = nullptr;
	if(OwningActor)
	{
		World = OwningActor->GetWorld();
	}
}

void FRigVMExecuteContext::SetWorld(const UWorld* InWorld)
{
	World = InWorld;
}

bool FRigVMExecuteContext::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	static const FName ControlRigExecuteContextName("ControlRigExecuteContext");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == ControlRigExecuteContextName)
	{
		static const FString CRExecuteContextPath = TEXT("/Script/ControlRig.ControlRigExecuteContext");
		UScriptStruct* OldStruct = FindFirstObject<UScriptStruct>(*CRExecuteContextPath, EFindFirstObjectOptions::NativeFirst | EFindFirstObjectOptions::EnsureIfAmbiguous);
		checkf(OldStruct, TEXT("FControlRigExecuteContext was not found."));

		TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(OldStruct));
		OldStruct->SerializeItem(Slot, StructOnScope->GetStructMemory(), nullptr);		
		return true;
	}

	return false;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FRigVMExtendedExecuteContext::~FRigVMExtendedExecuteContext()
{
	Reset();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// --- FRigVMExtendedExecuteContext ---

void FRigVMExtendedExecuteContext::Reset()
{
	VMHash = 0;

	ResetExecutionState();

	WorkMemoryStorageObject = nullptr;
	DebugMemoryStorageObject = nullptr;

	WorkMemoryStorage = FRigVMMemoryStorageStruct();
	DebugMemoryStorage = FRigVMMemoryStorageStruct();

	CurrentVMMemory = TArrayView<FRigVMMemoryStorageStruct*>();
	ExecutionReachedExit().Clear();

	CachedMemoryHandles.Reset();

	LazyBranchInstanceData.Reset();
	ExternalVariableRuntimeData.Reset();

	if(PublicDataScope.IsValid())
	{
		GetPublicData<>().NumExecutions = 0;
	}

	ExecutingThreadId = INDEX_NONE;

	EntriesBeingExecuted.Reset();

	CurrentExecuteResult = ERigVMExecuteResult::Failed;
	CurrentEntryName = NAME_None;
	bCurrentlyRunningRootEntry = false;

#if WITH_EDITOR
	if (DebugInfo != nullptr)
	{
		DebugInfo->Reset();
		DebugInfo = nullptr;
	}
#endif // WITH_EDITOR
}

/** Resets VM execution state */
void FRigVMExtendedExecuteContext::ResetExecutionState()
{
	if (FRigVMExecuteContext* ExecuteContext = reinterpret_cast<FRigVMExecuteContext*>(PublicDataScope.GetStructMemory()))
	{
		ExecuteContext->Reset();
		ExecuteContext->ExtendedExecuteContext = this;
	}
	
	VM = nullptr;
	Slices.Reset();
	Slices.Add(FRigVMSlice());
	SliceOffsets.Reset();
	Factory = nullptr;
}

void FRigVMExtendedExecuteContext::CopyMemoryStorage(const FRigVMExtendedExecuteContext& Other, UObject* Outer)
{
	WorkMemoryStorage = Other.WorkMemoryStorage;
	DebugMemoryStorage = Other.DebugMemoryStorage;
}

void FRigVMExtendedExecuteContext::CopyMemoryStorage(TObjectPtr<URigVMMemoryStorage>& TargetMemory, const TObjectPtr <URigVMMemoryStorage>& SourceMemory, UObject* Outer)
{
	if(SourceMemory != nullptr)
	{
		if(TargetMemory == nullptr)
		{
			TargetMemory = NewObject<URigVMMemoryStorage>(Outer, SourceMemory->GetClass());
		}
		else if(TargetMemory->GetClass() != SourceMemory->GetClass()
			|| TargetMemory == SourceMemory) // when a instance comes with CDO data automatically copied during instantiation
		{
			TargetMemory = NewObject<URigVMMemoryStorage>(Outer, SourceMemory->GetClass());
		}

		TargetMemory->CopyFrom(SourceMemory);
	}
	else if(TargetMemory != nullptr)
	{
		TargetMemory = nullptr;
	}
}

FRigVMExtendedExecuteContext& FRigVMExtendedExecuteContext::operator =(const FRigVMExtendedExecuteContext& Other)
{
	VMHash = Other.VMHash;

	const UScriptStruct* OtherPublicDataStruct = Cast<UScriptStruct>(Other.PublicDataScope.GetStruct());
	check(OtherPublicDataStruct);
	if(PublicDataScope.GetStruct() != OtherPublicDataStruct)
	{
		PublicDataScope = FStructOnScope(OtherPublicDataStruct);
	}

	FRigVMExecuteContext* ThisPublicContext = (FRigVMExecuteContext*)PublicDataScope.GetStructMemory();
	const FRigVMExecuteContext* OtherPublicContext = (const FRigVMExecuteContext*)Other.PublicDataScope.GetStructMemory();
	ThisPublicContext->Copy(OtherPublicContext);
	ThisPublicContext->ExtendedExecuteContext = this;

	if(OtherPublicContext->GetNameCache() == &Other.NameCache)
	{
		SetDefaultNameCache();
	}

	VM = Other.VM;
	Slices = Other.Slices;
	SliceOffsets = Other.SliceOffsets;

	CachedMemoryHandles = Other.CachedMemoryHandles;

	LazyBranchInstanceData = Other.LazyBranchInstanceData;
	ExternalVariableRuntimeData = Other.ExternalVariableRuntimeData;

	return *this;
}

void FRigVMExtendedExecuteContext::Initialize(const UScriptStruct* InScriptStruct)
{
	check(InScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
	PublicDataScope = FStructOnScope(InScriptStruct);
	((FRigVMExecuteContext*)PublicDataScope.GetStructMemory())->ExtendedExecuteContext = this;
	SetDefaultNameCache();
}
