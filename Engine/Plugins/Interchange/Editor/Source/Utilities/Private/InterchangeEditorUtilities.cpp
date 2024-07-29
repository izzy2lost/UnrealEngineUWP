// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeEditorUtilities.h"

#include "FileHelpers.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeEditorUtilities)

bool UInterchangeEditorUtilities::SaveAsset(UObject* Asset)
{
	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(Asset->GetPackage());
	FEditorFileUtils::EPromptReturnCode ReturnCode = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false /*bCheckDirty*/, false /*bPromptToSave*/);
	return (ReturnCode == FEditorFileUtils::PR_Success);
}
