// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAdvancedRenamer.h"
#include "Providers/IAdvancedRenamerProvider.h"
#include "Templates/SharedPointer.h"

class FRegexPattern;
struct FAdvancedRenamerOptions;
struct FAdvancedRenamerPreview;

class FAdvancedRenamer : public IAdvancedRenamer
{
public:
	FAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InProvider, FAdvancedRenamerOptions* InInitialOptions = nullptr);

	//~ Begin IAdvancedRenamer
	virtual const TSharedRef<IAdvancedRenamerProvider>& GetProvider() const override;
	virtual const FAdvancedRenamerOptions& GetOptions() const override;
	virtual FAdvancedRenamerOptions& GetOptions() override;
	virtual void SetOption(const FAdvancedRenamerOptions& InOptions) override;
	virtual const TArray<TSharedPtr<FAdvancedRenamerPreview>>& GetPreviews() override;
	virtual TSharedPtr<FAdvancedRenamerPreview> GetPreview(int32 InIndex) const override;
	virtual bool HasRenames() const override;
	virtual bool IsDirty() const override;
	virtual void MarkDirty() override;
	virtual void MarkClean() override;
	virtual FString ApplyRename(const FString& InName, int32 InIndex) const override;
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

protected:
	TSharedRef<IAdvancedRenamerProvider> Provider;
	TArray<TSharedPtr<FAdvancedRenamerPreview>> Previews;
	FAdvancedRenamerOptions Options;
	bool bHasRenames;
	bool bDirty;

	FString ApplyBaseName(const FString& InOriginalName) const;
	FString ApplyPrefix(const FString& InOriginalName) const;
	FString ApplySuffix(const FString& InOriginalName, int32 InIndex) const;
	FString ApplySearchPlainText(const FString& InOriginalName) const;
	FString ApplySearchReplaceRegex(const FString& InOriginalName) const;

	FString RegexReplace(const FString& InOriginalString, const FRegexPattern& InPattern, const FString& InReplaceString) const;
};
