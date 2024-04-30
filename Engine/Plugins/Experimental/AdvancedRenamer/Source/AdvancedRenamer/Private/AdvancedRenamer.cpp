// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamer.h"

#include "AdvancedRenamerModule.h"
#include "Internationalization/Regex.h"

FAdvancedRenamer::FAdvancedRenamer(const TSharedRef<IAdvancedRenamerProvider>& InProvider, FAdvancedRenamerOptions* InInitialOptions)
	: Provider(InProvider)
{
	if (InInitialOptions)
	{
		SetOption(*InInitialOptions);
	}

	int32 Count = Num();
	check(Count > 0);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!CanRename(Index))
		{
			RemoveIndex(Index);
			--Index;
			--Count;
			continue;
		}

		const int32 Hash = GetHash(Index);
		FString OriginalName = GetOriginalName(Index);

		Previews.Add(MakeShared<FAdvancedRenamerPreview>(Hash, OriginalName));
	}
}

const TSharedRef<IAdvancedRenamerProvider>& FAdvancedRenamer::GetProvider() const
{
	return Provider;
}

const FAdvancedRenamerOptions& FAdvancedRenamer::GetOptions() const
{
	return Options;
}

FAdvancedRenamerOptions& FAdvancedRenamer::GetOptions()
{
	return Options;
}

void FAdvancedRenamer::SetOption(const FAdvancedRenamerOptions& InOptions)
{
	Options = InOptions;
	MarkDirty();
}

const TArray<TSharedPtr<FAdvancedRenamerPreview>>& FAdvancedRenamer::GetPreviews()
{
	return Previews;
}

TSharedPtr<FAdvancedRenamerPreview> FAdvancedRenamer::GetPreview(int32 InIndex) const
{
	if (Previews.IsValidIndex(InIndex))
	{
		return Previews[InIndex];
	}

	return nullptr;
}

bool FAdvancedRenamer::HasRenames() const
{
	return bHasRenames;
}

bool FAdvancedRenamer::IsDirty() const
{
	return bDirty;
}

void FAdvancedRenamer::MarkDirty()
{
	bDirty = true;
}

void FAdvancedRenamer::MarkClean()
{
	bDirty = false;
}

bool FAdvancedRenamer::UpdatePreviews()
{
	bHasRenames = false;
	const int32 Count = Previews.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Previews[Index].IsValid() || !IsValidIndex(Index))
		{
			RemoveIndex(Index);
			--Index;
			continue;
		}

		// Force recreation
		Previews[Index]->NewName = ApplyRename(Previews[Index]->OriginalName, Index);

		if (Previews[Index]->NewName.IsEmpty())
		{
			continue;
		}

		if (GetOriginalName(Index) == Previews[Index]->NewName)
		{
			continue;
		}

		bHasRenames = true;
	}

	MarkClean();

	return bHasRenames;
}

bool FAdvancedRenamer::Execute()
{
	if (!HasRenames())
	{
		UpdatePreviews();

		if (!HasRenames())
		{
			return false;
		}
	}

	const int32 Count = Previews.Num();
	bool bAllSuccess = true;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!Previews[Index].IsValid())
		{
			continue;
		}

		if (!IsValidIndex(Index))
		{
			continue;
		}

		if (Previews[Index]->NewName.IsEmpty())
		{
			continue;
		}

		if (!ExecuteRename(Index, Previews[Index]->NewName))
		{
			bAllSuccess = false;
		}
	}

	MarkClean();

	return bAllSuccess;
}

FString FAdvancedRenamer::ApplyRename(const FString& InOriginalName, int32 InIndex) const
{
	FString NewName = InOriginalName;

	if (Options.HasBaseName())
	{
		NewName = ApplyBaseName(NewName);
	}

	if (Options.HasPrefix())
	{
		NewName = ApplyPrefix(NewName);
	}

	if (Options.HasSuffix())
	{
		NewName = ApplySuffix(NewName, InIndex);
	}

	if (Options.HasSearchAndReplace())
	{
		switch (Options.SearchAndReplaceType)
		{
			default:
				break;

			case EAdvancedRenamerSeachAndReplaceType::PlainText:
				NewName = ApplySearchPlainText(NewName);
				break;

			case EAdvancedRenamerSeachAndReplaceType::RegularExpression:
				NewName = ApplySearchReplaceRegex(NewName);
				break;
		}
	}

	return NewName;
}

FString FAdvancedRenamer::ApplyBaseName(const FString& InOriginalName) const
{
	if (Options.BaseName.IsEmpty())
	{
		return InOriginalName;
	}

	return Options.BaseName;
}

FString FAdvancedRenamer::ApplyPrefix(const FString& InOriginalName) const
{
	FString Output = InOriginalName;

	if (!Options.RemovePrefixSeparator.IsEmpty())
	{
		const int32 PrefixStart = Output.Find(Options.RemovePrefixSeparator, ESearchCase::IgnoreCase);

		if (PrefixStart >= 0)
		{
			Output = Output.Mid(PrefixStart + Options.RemovePrefixSeparator.Len());
		}
	}
	else if (Options.RemovePrefixCharacterCount > 0)
	{
		Output = Output.RightChop(Options.RemovePrefixCharacterCount);
	}

	if (!Options.AddPrefix.IsEmpty())
	{
		Output = Options.AddPrefix + Output;
	}

	return Output;
}

FString FAdvancedRenamer::ApplySuffix(const FString& InOriginalName, int32 InIndex) const
{
	static constexpr TCHAR FirstDigit = '0';
	static constexpr TCHAR LastDigit = '9';

	FString Output = InOriginalName;

	if (!Options.RemoveSuffixSeparator.IsEmpty())
	{
		const int32 SuffixStart = Output.Find(Options.RemoveSuffixSeparator, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (SuffixStart >= 0)
		{
			Output = Output.Mid(0, SuffixStart);
		}
	}
	else if (Options.RemoveSuffixCharacterCount > 0)
	{
		Output = Output.LeftChop(Options.RemoveSuffixCharacterCount);
	}

	if (Options.bRemoveSuffixNumber || Options.bAddSuffixNumber)
	{
		int32 LastDigitIndex = Output.Len() - 1;

		while (LastDigitIndex >= 0)
		{
			if (Output[LastDigitIndex] < FirstDigit || Output[LastDigitIndex] > LastDigit)
			{
				break;
			}

			--LastDigitIndex;
		}

		Output = Output.Mid(0, LastDigitIndex + 1);
	}

	if (!Options.AddSuffix.IsEmpty())
	{
		Output = Output + Options.AddSuffix;
	}

	if (Options.bAddSuffixNumber)
	{
		const int32 Current = Options.AddSuffixNumberStart + (InIndex * Options.AddSuffixNumberStep);
		Output += FString::FromInt(Current);
	}

	return Output;
}

FString FAdvancedRenamer::ApplySearchPlainText(const FString& InOriginalName) const
{
	if (Options.SearchAndReplaceFromText.IsEmpty())
	{
		return InOriginalName;
	}

	return InOriginalName.Replace(
		*Options.SearchAndReplaceFromText, 
		*Options.SearchAndReplaceToText, 
		Options.SearchAndReplaceCase
	);
}

FString FAdvancedRenamer::ApplySearchReplaceRegex(const FString& InOriginalName) const
{
	if (Options.SearchAndReplaceFromText.IsEmpty())
	{
		return InOriginalName;
	}

	const FRegexPattern RegexPattern = FRegexPattern(
		Options.SearchAndReplaceFromText,
		Options.SearchAndReplaceCase == ESearchCase::IgnoreCase ? ERegexPatternFlags::CaseInsensitive : ERegexPatternFlags::None
	);

	return RegexReplace(InOriginalName, RegexPattern, Options.SearchAndReplaceToText);
}

FString FAdvancedRenamer::RegexReplace(const FString& InOriginalString, const FRegexPattern& InPattern, const FString& InReplaceString) const
{
	static const FString EscapeString = TEXT("\\");
	static constexpr TCHAR EscapeChar = '\\';
	static const FString GroupString = TEXT("$");
	static constexpr TCHAR GroupChar = '$';
	static constexpr TCHAR FirstDigit = '0';
	static constexpr TCHAR LastDigit = '9';
	static constexpr TCHAR NullChar = 0;

	FRegexMatcher Matcher(InPattern, InOriginalString);

	FString Output = "";
	int32 StartCharIndex = 0;
	bool bReadingGroupName = false;
	int32 GroupIndex = INDEX_NONE;

	while (Matcher.FindNext())
	{
		// Add on part of string after start/previous match
		if (StartCharIndex != Matcher.GetMatchBeginning())
		{
			Output += InOriginalString.Mid(StartCharIndex, Matcher.GetMatchBeginning() - StartCharIndex);
		}

		bool bEscaped = false;

		for (int32 CharIndex = 0; CharIndex <= InReplaceString.Len(); ++CharIndex)
		{
			const TCHAR& Char = CharIndex < InReplaceString.Len() ? InReplaceString[CharIndex] : NullChar;

			if (bReadingGroupName)
			{
				// Build group index
				if (Char >= FirstDigit && Char <= LastDigit)
				{
					if (GroupIndex == INDEX_NONE)
					{
						GroupIndex = 0;
					}
					else if (GroupIndex > 0)
					{
						GroupIndex *= 10;
					}

					const int32 NextDigit = static_cast<int32>(Char - FirstDigit);
					GroupIndex += NextDigit;
					continue;
				}
				// We've read a group index, add it to the output string
				else if (GroupIndex > 0)
				{
					if (Matcher.GetCaptureGroupBeginning(GroupIndex) == INDEX_NONE)
					{
						UE_LOG(LogARP, Error, TEXT("Regex: Capture group does not exist %d."), GroupIndex);
					}

					Output += Matcher.GetCaptureGroup(GroupIndex);
				}
				// $0 matches the entire matched string
				else if (GroupIndex == 0)
				{
					Output += InOriginalString.Mid(Matcher.GetMatchBeginning(), Matcher.GetMatchEnding() - Matcher.GetMatchBeginning());
				}
				// An unescaped $
				else
				{
					UE_LOG(LogARP, Error, TEXT("Regex: Unescaped %s."), *GroupString);

					Output += GroupString;
				}

				bReadingGroupName = false;
				// Continue regular parsing of this character.
			}

			// Check for special chars
			if (!bEscaped)
			{
				if (Char == EscapeChar)
				{
					bEscaped = true;
					continue;
				}

				if (Char == GroupChar)
				{
					bReadingGroupName = true;
					GroupIndex = INDEX_NONE;
					continue;
				}
			}
			else
			{
				// If the last char is a \ assume that it's not an escape char
				if (Char == NullChar)
				{
					UE_LOG(LogARP, Error, TEXT("Regex: Unescaped %s."), *EscapeString);

					Output += EscapeChar;
				}
			}

			if (Char != NullChar)
			{
				Output += Char;
			}

			bEscaped = false;
		}

		StartCharIndex = Matcher.GetMatchEnding();
	}

	// Add on the end of the string after the last match
	if (StartCharIndex < InOriginalString.Len())
	{
		Output += InOriginalString.Mid(StartCharIndex);
	}

	return Output;
}

int32 FAdvancedRenamer::Num() const
{
	return Provider->Num();
}

bool FAdvancedRenamer::IsValidIndex(int32 InIndex) const
{
	return Provider->IsValidIndex(InIndex);
}

uint32 FAdvancedRenamer::GetHash(int32 InIndex) const
{
	return Provider->GetHash(InIndex);
}

FString FAdvancedRenamer::GetOriginalName(int32 InIndex) const
{
	return Provider->GetOriginalName(InIndex);
}

bool FAdvancedRenamer::RemoveIndex(int32 InIndex)
{
	// Can fail during construction when indices that aren't renameable are removed from the provider before
	// they are added to ListData.
	if (Previews.IsValidIndex(InIndex))
	{
		Previews.RemoveAt(InIndex);
	}

	return Provider->RemoveIndex(InIndex);
}

bool FAdvancedRenamer::CanRename(int32 InIndex) const
{
	return Provider->CanRename(InIndex);
}

bool FAdvancedRenamer::ExecuteRename(int32 InIndex, const FString& InNewName)
{
	return Provider->ExecuteRename(InIndex, InNewName);
}
