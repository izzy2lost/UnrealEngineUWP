// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMRuntimeDataRegistry.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Graph/RigUnit_AnimNextBeginExecution.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_ParamBlock_UpdateLayer);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextParameterBlock)

UAnimNextParameterBlock::UAnimNextParameterBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRigVMExtendedExecuteContext(&BaseRigVMContext);
}

void UAnimNextParameterBlock::UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamBlock_UpdateLayer);
	
	if (VM)
	{
		UE::AnimNext::FRigVMRuntimeData* RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::FindRuntimeData(VM);
		if (RuntimeData == nullptr || RuntimeData->Context.VMHash != VM->GetVMHash())
		{
			RuntimeData = UE::AnimNext::FRigVMRuntimeDataRegistry::AddRuntimeData(VM, GetRigVMExtendedExecuteContext());
			VM->InitializeInstance(RuntimeData->Context, false); // TODO zzz : Temp until VM supports WorkData cached handles using offsets (UE-197067)
			RuntimeData->Context.VMHash = VM->GetVMHash();
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

void UAnimNextParameterBlock::PostLoad()
{
	Super::PostLoad();

	VM = RigVM;

	// In packaged builds, initialize the VM
	// In editor, the VM will be recompiled and initialized at UAnimNextParameterBlock_EditorData::HandlePackageDone::RecompileVM
#if !WITH_EDITOR
	if(VM != nullptr)
	{
		VM->Initialize(BaseRigVMContext);
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