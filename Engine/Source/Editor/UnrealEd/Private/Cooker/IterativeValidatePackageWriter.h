// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiffPackageWriter.h"

/**
 * A CookedPackageWriter that diffs the cook results of iteratively-unmodified packages between their last cook
 * results and the current cook.
 */
class FIterativeValidatePackageWriter : public FDiffPackageWriter
{
public:
	using Super = FDiffPackageWriter;
	enum class EPhase
	{
		FirstCook,
		FinalCook,
	};
	FIterativeValidatePackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner, EPhase InPhase,
		const FString& ResolvedMetadataPath);

	// IPackageWriter
	virtual void BeginPackage(const FBeginPackageInfo& Info) override;
	virtual void CommitPackage(FCommitPackageInfo&& Info) override;
	virtual void WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive,
		const TArray<FFileRegion>& FileRegions) override;
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex) override;
	virtual TUniquePtr<FLargeMemoryWriter> CreateLinkerExportsArchive(FName PackageName, UObject* Asset, uint16 MultiOutputIndex) override;
	virtual bool IsPreSaveCompleted() const override;

	// ICookedPackageWriter
	virtual void Initialize(const FCookInfo& CookInfo) override;
	virtual void UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
		bool& bInOutShouldIterativelySkip) override;
	virtual void BeginCook(const FCookInfo& Info) override;
	virtual void EndCook(const FCookInfo& Info) override;
	virtual void UpdateSaveArguments(FSavePackageArgs& SaveArgs) override;
	virtual bool IsAnotherSaveNeeded(FSavePackageResultStruct& PreviousResult, FSavePackageArgs& SaveArgs) override;

protected:
	virtual void OnDiffWriterMessage(ELogVerbosity::Type Verbosity, FStringView Message) override;
	void LogIterativeDifferences();
	void Save();
	void Load();
	void Serialize(FArchive& Ar);
	FString GetIterativeValidatePath() const;

	struct FMessage
	{
		FString Text;
		ELogVerbosity::Type Verbosity;
	};
	friend FArchive& operator<<(FArchive& Ar, FMessage& Message);

	TSet<FName> IterativeValidated;
	TMap<FName, TArray<FMessage>> IterativeFailed;
	TSet<FName> IndeterminismFailed;
	TSet<FName> IterativeFalseNegative;

	FString MetadataPath;
	int32 ModifiedCount = 0;
	EPhase Phase = EPhase::FirstCook;
	bool bPackageSaveToDiskPass = false;
	bool bPackageFirstPass = false;
};
