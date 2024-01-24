// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackStatelessEmitterSimulateGroup.h"

#include "IDetailTreeNode.h"
#include "NiagaraEditorStyle.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "ViewModels/Stack/NiagaraStackItemPropertyHeaderValueShared.h"
#include "ViewModels/Stack/NiagaraStackObject.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterStatelessSimulateGroup"

void UNiagaraStackStatelessEmitterSimulateGroup::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter)
{
	Super::Initialize(
		InRequiredEntryData, 
		LOCTEXT("EmitterStatelessSimulateGroupDisplayName", "Simulate"),
		LOCTEXT("EmitterStatelessSimulateGroupToolTip", "Data related to the simulation of the particles"),
		nullptr);
	StatelessEmitterWeak = InStatelessEmitter;
}

const FSlateBrush* UNiagaraStackStatelessEmitterSimulateGroup::GetIconBrush() const
{
	return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stateless.UpdateIcon");
}

void UNiagaraStackStatelessEmitterSimulateGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		for (UNiagaraStatelessModule* StatelessModule : StatelessEmitter->GetModules())
		{
			UNiagaraStackStatelessModuleItem* ModuleItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackStatelessModuleItem>(CurrentChildren,
				[StatelessModule](const UNiagaraStackStatelessModuleItem* CurrentChild) { return CurrentChild->GetStatelessModule() == StatelessModule; });
			if (ModuleItem == nullptr)
			{
				ModuleItem = NewObject<UNiagaraStackStatelessModuleItem>(this);
				ModuleItem->Initialize(CreateDefaultChildRequiredData(), StatelessModule);
			}
			NewChildren.Add(ModuleItem);
		}
	}
}

void UNiagaraStackStatelessModuleItem::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessModule* InStatelessModule)
{
	Super::Initialize(InRequiredEntryData, FString::Printf(TEXT("StatelessModuleItem-%s"), *InStatelessModule->GetName()));
	StatelessModuleWeak = InStatelessModule;
	DisplayName = InStatelessModule->GetClass()->GetDisplayNameText();
}

bool UNiagaraStackStatelessModuleItem::SupportsChangeEnabled() const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	return StatelessModule != nullptr && StatelessModule->CanDisableModule();
}

bool UNiagaraStackStatelessModuleItem::GetIsEnabled() const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	return StatelessModule != nullptr && StatelessModule->IsModuleEnabled();
}

void UNiagaraStackStatelessModuleItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		UNiagaraStackObject* ModuleObject = ModuleObjectWeak.Get();
		if (ModuleObject == nullptr || ModuleObject->GetObject() != StatelessModule)
		{
			bool bIsInTopLevelObject = true;
			bool bHideTopLevelCategories = true;
			ModuleObject = NewObject<UNiagaraStackObject>(this);
			ModuleObject->Initialize(CreateDefaultChildRequiredData(), StatelessModule, bIsInTopLevelObject, bHideTopLevelCategories, GetStackEditorDataKey());
			ModuleObject->SetOnFilterDetailNodes(FNiagaraStackObjectShared::FOnFilterDetailNodes::CreateStatic(&UNiagaraStackStatelessModuleItem::FilterDetailNodes), UNiagaraStackObject::EDetailNodeFilterMode::FilterAllNodes);
			ModuleObjectWeak = ModuleObject;
		}
		NewChildren.Add(ModuleObject);

		if (bGeneratedHeaderValueHandlers == false)
		{
			bGeneratedHeaderValueHandlers = true;
			FNiagaraStackItemPropertyHeaderValueShared::GenerateHeaderValueHandlers(*StatelessModule, nullptr, *StatelessModule->GetClass(), FSimpleDelegate::CreateUObject(this, &UNiagaraStackStatelessModuleItem::OnHeaderValueChanged), HeaderValueHandlers);
		}
		else
		{
			for (TSharedRef<FNiagaraStackItemPropertyHeaderValue> HeaderValueHandler : HeaderValueHandlers)
			{
				HeaderValueHandler->Refresh();
			}
		}
	}
	else
	{
		ModuleObjectWeak.Reset();
		HeaderValueHandlers.Empty();
	}
}

void UNiagaraStackStatelessModuleItem::SetIsEnabledInternal(bool bInIsEnabled)
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr && StatelessModule->CanDisableModule() && StatelessModule->IsModuleEnabled() != bInIsEnabled)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("ChangeStatelessModuleEnabledTransaction", "Change module enabled"));
		StatelessModule->Modify();
		StatelessModule->SetIsModuleEnabled(bInIsEnabled);
		StatelessModule->PostEditChange();
		TArray<UObject*> ChangedObjects = { StatelessModule };
		OnDataObjectModified().Broadcast(ChangedObjects, ENiagaraDataObjectChange::Changed);
		RefreshChildren();
	}
}

void UNiagaraStackStatelessModuleItem::GetHeaderValueHandlers(TArray<TSharedRef<INiagaraStackItemHeaderValueHandler>>& OutHeaderValueHandlers) const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		OutHeaderValueHandlers.Append(HeaderValueHandlers);
	}
}

void UNiagaraStackStatelessModuleItem::FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes)
{
	for (const TSharedRef<IDetailTreeNode>& SourceNode : InSourceNodes)
	{
		bool bIncludeNode = true;
		if (SourceNode->GetNodeType() == EDetailNodeType::Item)
		{
			TSharedPtr<IPropertyHandle> NodePropertyHandle = SourceNode->CreatePropertyHandle();
			if (NodePropertyHandle.IsValid() && (NodePropertyHandle->HasMetaData("HideInStack") || NodePropertyHandle->HasMetaData("ShowInStackItemHeader")))
			{
				bIncludeNode = false;
			}
		}
		if (bIncludeNode)
		{
			OutFilteredNodes.Add(SourceNode);
		}
	}
}

void UNiagaraStackStatelessModuleItem::OnHeaderValueChanged()
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		TArray<UObject*> ChangedObjects;
		ChangedObjects.Add(StatelessModule);
		OnDataObjectModified().Broadcast(ChangedObjects, ENiagaraDataObjectChange::Changed);
	}
}

#undef LOCTEXT_NAMESPACE
