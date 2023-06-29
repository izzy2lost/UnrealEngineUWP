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
	FIterativeValidatePackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner);

	// ICookedPackageWriter
	virtual void UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
		bool& bInOutShouldIterativelySkip) override;
};
