// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Misc/CString.h"
#include "Providers/IAdvancedRenamerProvider.h"
#include "Templates/SharedPointerFwd.h"

enum class EAdvancedRenamerSeachAndReplaceType : uint8
{
	None,
	PlainText,
	RegularExpression
};

/**
 * Options for the rename engine.
 *
 * - RemovePrefixSeparator and RemovePrefixCharacterCount are mutually exclusive.
 * - RemoveSuffixSeparator and RemoveSuffixCharacterCount are mutually exclusive.
 * - bAddSuffixNumber implies bRemoveSuffixNumer.
 */
struct FAdvancedRenamerOptions
{
	/** Name => OtherName */
	FString BaseName = ""; 

	/** Name => Other_Name */
	FString AddPrefix = ""; 

	/** Other_Name, _ => Name */
	FString RemovePrefixSeparator = "";

	/** OtherName, 5 => Name */
	int32 RemovePrefixCharacterCount = 0; 

	/** Name => Name_Other */
	FString AddSuffix = ""; 

	/** Name => Name1 */
	bool bAddSuffixNumber = false; 

	/** Name, 5 => Name5 */
	int32 AddSuffixNumberStart = 1; 

	/** Name, Moo, 1, 5 => Name1, Moo6 */
	int32 AddSuffixNumberStep = 1;

	/** Name_Other, _ => Name */
	FString RemoveSuffixSeparator = ""; 

	/** NameOther, 5 => Name */
	int32 RemoveSuffixCharacterCount = 0; 

	/** Name5 => Name */
	bool bRemoveSuffixNumber = false; 

	EAdvancedRenamerSeachAndReplaceType SearchAndReplaceType = EAdvancedRenamerSeachAndReplaceType::None;
	ESearchCase::Type SearchAndReplaceCase = ESearchCase::CaseSensitive;

	/** Name, am, ot => Note */
	FString SearchAndReplaceFromText = "";

	/** Name, am, ot => Note */
	FString SearchAndReplaceToText = "";

	bool HasBaseName() const
	{
		return !BaseName.IsEmpty();
	}

	bool HasPrefix() const
	{
		return !AddPrefix.IsEmpty()
			|| !RemovePrefixSeparator.IsEmpty()
			|| RemovePrefixCharacterCount > 0;
	}

	bool HasSuffix() const
	{
		return !AddSuffix.IsEmpty()
			|| (bAddSuffixNumber && AddSuffixNumberStep > 0)
			|| !RemoveSuffixSeparator.IsEmpty()
			|| RemoveSuffixCharacterCount > 0
			|| bRemoveSuffixNumber;			
	}

	bool HasSearchAndReplace() const
	{
		return SearchAndReplaceType != EAdvancedRenamerSeachAndReplaceType::None
			&& !SearchAndReplaceFromText.IsEmpty();
	}
};

struct FAdvancedRenamerPreview
{
	FAdvancedRenamerPreview(int32 InHash, const FString InOriginalName)
		: Hash(InHash)
		, OriginalName(InOriginalName)
		, NewName(FString(""))
	{
	}

	int32 Hash;
	const FString OriginalName;
	FString NewName;
};

/**
* Implements its own provider interface so it can avoid long Execute_ lines and handle
* the 2 different types of provider (SharedPtr and UObject.)
*/
class IAdvancedRenamer : public IAdvancedRenamerProvider
{
public:
	virtual ~IAdvancedRenamer() = default;

	virtual const TSharedRef<IAdvancedRenamerProvider>& GetProvider() const = 0;

	virtual const FAdvancedRenamerOptions& GetOptions() const = 0;

	/** Make sure to run UpdatePreviews or MarkDirty if you make changes directly. */
	virtual FAdvancedRenamerOptions& GetOptions() = 0;

	virtual void SetOption(const FAdvancedRenamerOptions& InOptions) = 0;

	virtual const TArray<TSharedPtr<FAdvancedRenamerPreview>>& GetPreviews() = 0;

	/** Returns the preview for the item at the given index. */
	virtual TSharedPtr<FAdvancedRenamerPreview> GetPreview(int32 InIndex) const = 0;

	/** True if there are any items actually renamed by the preview generator. */
	virtual bool HasRenames() const = 0;

	/** Whether the options have been updated. */
	virtual bool IsDirty() const = 0;
	virtual void MarkDirty() = 0;
	virtual void MarkClean() = 0;

	/** Executes the rename on the given name. */
	virtual FString ApplyRename(const FString& InName, int32 InIndex) const = 0;

	/** Returns true if any names actually changed. */
	virtual bool UpdatePreviews() = 0;

	/** Returns true if all items were updated without error. */
	virtual bool Execute() = 0;
};
