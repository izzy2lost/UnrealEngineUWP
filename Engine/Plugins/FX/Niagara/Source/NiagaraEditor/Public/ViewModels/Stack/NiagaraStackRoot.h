// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackCommentCollection.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackRoot.generated.h"

class FNiagaraEmitterViewModel;
class UNiagaraStackSystemPropertiesGroup;
class UNiagaraStackSystemUserParametersGroup;
class UNiagaraStackEmitterPropertiesGroup;
class UNiagaraStackScriptItemGroup;
class UNiagaraStackRenderItemGroup;
class UNiagaraStackEmitterSummaryGroup;
class UNiagaraStackSummaryViewCollapseButton;

UCLASS(MinimalAPI)
class UNiagaraStackRoot : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	NIAGARAEDITOR_API UNiagaraStackRoot();
	
	void Initialize(FRequiredEntryData InRequiredEntryData, bool bInIncludeSystemInformation, bool bInIncludeEmitterInformation);
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;

	NIAGARAEDITOR_API virtual bool GetCanExpand() const override;
	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;
	UNiagaraStackRenderItemGroup* GetRenderGroup() const
	{
		return RenderGroup;
	}

	UNiagaraStackCommentCollection* GetCommentCollection() const
	{
		return CommentCollection;
	}

	UNiagaraStackEmitterSummaryGroup* GetEmitterSummaryGroup() const
	{
		return EmitterSummaryGroup;
	}
	
protected:
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	NIAGARAEDITOR_API void EmitterArraysChanged();
	NIAGARAEDITOR_API void OnSummaryViewStateChanged();

	void AddDefaultViewFilter();
	void RemoveDefaultViewFilter();
	
	void AddSummaryViewFilter();
	void RemoveSummaryViewFilter();

	NIAGARAEDITOR_API bool FilterForDefaultView(const UNiagaraStackEntry& NiagaraStackEntry) const;
	NIAGARAEDITOR_API bool FilterForSummaryView(const UNiagaraStackEntry& NiagaraStackEntry) const;

private:
	UPROPERTY()
	TObjectPtr<UNiagaraStackSystemPropertiesGroup> SystemPropertiesGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> SystemSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> SystemUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterPropertiesGroup> EmitterPropertiesGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackEmitterSummaryGroup> EmitterSummaryGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> EmitterSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> EmitterUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> ParticleSpawnGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackScriptItemGroup> ParticleUpdateGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackRenderItemGroup> RenderGroup;

	UPROPERTY()
	TObjectPtr<UNiagaraStackCommentCollection> CommentCollection;

	bool bIncludeSystemInformation;
	bool bIncludeEmitterInformation;

	FDelegateHandle DefaultViewFilterHandle;
	FDelegateHandle SummaryViewFilterHandle;
};
