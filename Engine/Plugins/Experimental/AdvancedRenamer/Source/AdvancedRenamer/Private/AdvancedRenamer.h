// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAdvancedRenamer.h"
#include "Providers/IAdvancedRenamerProvider.h"
#include "Templates/SharedPointer.h"

struct FAdvancedRenamerPreview;

class FAdvancedRenamer : public IAdvancedRenamer
{
public:
	FAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InProvider);

	//~ Begin IAdvancedRenamer
	virtual const TSharedRef<IAdvancedRenamerProvider>& GetProvider() const override;
	virtual const TArray<TSharedPtr<FAdvancedRenamerPreview>>& GetPreviews() override;
	virtual TSharedPtr<FAdvancedRenamerPreview> GetPreview(int32 InIndex) const override;
	virtual void AddSection(FAdvancedRenamerExecuteSection InSection) override;
	virtual bool HasRenames() const override;
	virtual bool IsDirty() const override;
	virtual void MarkDirty() override;
	virtual void MarkClean() override;
	virtual FString ApplyRename(const FString& InName) override;
	virtual bool UpdatePreviews() override;
	virtual bool Execute() override;
	//~ End IAdvancedRenamer

	//~ Begin IAdvancedRenamerProvider
	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 InIndex) const override;
	virtual uint32 GetHash(int32 InIndex) const override;
	virtual FString GetOriginalName(int32 InIndex) const override;
	virtual bool RemoveIndex(int32 InIndex) override;
	virtual bool CanRename(int32 InIndex) const override;
	virtual bool ExecuteRename(int32 InIndex, const FString& InNewName) override;
	//~ End IAdvancedRenamerProvider

private:
	/** Called before the whole Rename logic start */
	void BeforeOperationsStartExecute();
	
	/** Called after the whole Rename logic end */
	void AfterOperationsEndExecute();

private:
	/** Provider for this Renamer */
	TSharedRef<IAdvancedRenamerProvider> Provider;

	/** Previews Name list */
	TArray<TSharedPtr<FAdvancedRenamerPreview>> Previews;
	
	/** Renamer sections list */
	TArray<FAdvancedRenamerExecuteSection> Sections;

	/** Whether or not at least 1 preview has a Rename */
	bool bHasRenames;
	
	/** if true the Rename logic will be executed */
	bool bDirty;
};
