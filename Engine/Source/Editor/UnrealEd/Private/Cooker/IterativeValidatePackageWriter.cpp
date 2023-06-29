// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/IterativeValidatePackageWriter.h"

FIterativeValidatePackageWriter::FIterativeValidatePackageWriter(TUniquePtr<ICookedPackageWriter>&& InInner)
	: FDiffPackageWriter(MoveTemp(InInner))
{
}

void FIterativeValidatePackageWriter::UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
	bool& bInOutShouldIterativelySkip)
{
	bool bModified = !bIterativelyUnmodified;
	bInOutShouldIterativelySkip = bModified;

	bool bInnerIterativelyUnmodified = bModified;
	bool bInnerInOutShouldIterativelySkip = bModified;
	Inner->UpdatePackageModificationStatus(PackageName, bInnerIterativelyUnmodified, bInnerInOutShouldIterativelySkip);
	checkf(bInnerInOutShouldIterativelySkip == bInnerIterativelyUnmodified,
		TEXT("IterativeValidatePackageWriter is not supported with an Inner that modifies bInOutShouldIterativelySkip."));
}
