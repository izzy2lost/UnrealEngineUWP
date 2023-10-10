// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMRuntimeDataRegistry.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Param/AnimNextParameterExecuteContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextParameterBlock)

namespace UE::AnimNext::Private
{

static TArray<UClass*> GetClassObjectsInPackage(UPackage* InPackage)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(InPackage, Objects, false);

	TArray<UClass*> ClassObjects;
	for (UObject* Object : Objects)
	{
		if (UClass* Class = Cast<UClass>(Object))
		{
			ClassObjects.Add(Class);
		}
	}

	return ClassObjects;
}

}

UAnimNextParameterBlock::UAnimNextParameterBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRigVMExtendedExecuteContext(&BaseRigVMContext);
}

void UAnimNextParameterBlock::UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const
{
	if (VM)
	{
		UE::AnimNext::FRigVMRuntimeData* RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::FindRuntimeData(VM);
		if (RuntimeData == nullptr || RuntimeData->Context.VMHash != VM->GetVMHash())
		{
			RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::AddRuntimeData(VM, GetRigVMExtendedExecuteContext());
			VM->InitializeInstance(RuntimeData->Context, false); // TODO zzz : Temp until VM supports WorkData cached handles using offsets (UE-197067)
		}

		FRigVMExtendedExecuteContext& Context = RuntimeData->Context;
		FAnimNextParameterExecuteContext& AnimNextParameterContext = Context.GetPublicDataSafe<FAnimNextParameterExecuteContext>();
		AnimNextParameterContext.SetParamContextData(InHandle);
		VM->ExecuteVM(Context, FRigUnit_AnimNextBeginExecution::EventName);
	}
}

UE::AnimNext::FParamStackLayerHandle UAnimNextParameterBlock::CacheLayer() const
{
	return UE::AnimNext::FParamStack::MakeValueLayer(PropertyBag);
}

bool UAnimNextParameterBlock::ShouldCacheLayer(const UE::AnimNext::FParamStackLayerHandle& InHandle) const
{
	if(!InHandle.IsValid())
	{
		return true;
	}

#if WITH_EDITOR	// Layout should only be changing in editor
	const FInstancedPropertyBag* HandlePropertyBag = InHandle.As<FInstancedPropertyBag>();
	if(HandlePropertyBag == nullptr || HandlePropertyBag->GetPropertyBagStruct() != PropertyBag.GetPropertyBagStruct())
	{
		return true;
	}
#endif

	return false;
}

void UAnimNextParameterBlock::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// Whenever the asset is renamed/moved, generated classes parented to the old package
	// are not moved to the new package automatically (see FAssetRenameManager), so we
	// have to manually perform the move/rename, to avoid invalid reference to the old package

	// Note: while asset duplication doesn't duplicate the classes either, it is not a problem there
	// because we always recompile in post duplicate.
	TArray<UClass*> ClassObjects = UE::AnimNext::Private::GetClassObjectsInPackage(OldOuter->GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			MemoryClass->Rename(nullptr, GetPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}
}

void UAnimNextParameterBlock::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	TArray<UClass*> ClassObjects = UE::AnimNext::Private::GetClassObjectsInPackage(GetPackage());

	for (UClass* ClassObject : ClassObjects)
	{
		if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(ClassObject))
		{
			OutDeps.Add(MemoryClass);
		}
	}
}

void UAnimNextParameterBlock::PostLoad()
{
	Super::PostLoad();

	// In packaged builds, initialize the VM
	// In editor, the VM will be recompiled and initialized at UAnimNextParameterBlock_EditorData::HandlePackageDone::RecompileVM
#if !WITH_EDITOR
	if(VM != nullptr)
	{
		InitializeVM(FRigUnit_AnimNextBeginExecution::EventName);
	}
#endif
}

void UAnimNextParameterBlock::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	
#if WITH_EDITORONLY_DATA
	if(EditorData)
	{
		EditorData->GetAssetRegistryTags(OutTags);
	}
#endif
}