// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathName.h"

#include "Misc/StringBuilder.h"
#include "Templates/TypeHash.h"

namespace UE
{

inline bool FPropertyPathName::FSegment::operator==(const FSegment& Segment) const
{
	return NameWithIndex == Segment.NameWithIndex && Type == Segment.Type;
}

inline bool FPropertyPathName::FSegment::operator<(const FSegment& Segment) const
{
	return Compare(Segment) < 0;
}

inline int32 FPropertyPathName::FSegment::Compare(const FSegment& Segment) const
{
	if (int32 CompareNameWithIndex = NameWithIndex.Compare(Segment.NameWithIndex))
	{
		return CompareNameWithIndex;
	}
	return Type.CompareLexical(Segment.Type);
}

bool FPropertyPathName::operator==(const FPropertyPathName& Path) const
{
	const int32 SegmentCount = Segments.Num();
	if (SegmentCount != Path.Segments.Num())
	{
		return false;
	}

	for (int32 SegmentIndex = SegmentCount - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		if (!(Segments[SegmentIndex] == Path.Segments[SegmentIndex]))
		{
			return false;
		}
	}

	return true;
}

bool FPropertyPathName::operator<(const FPropertyPathName& Path) const
{
	const int32 SegmentCountA = Segments.Num();
	const int32 SegmentCountB = Path.Segments.Num();
	const int32 SegmentCountMin = FPlatformMath::Min(SegmentCountA, SegmentCountB);

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCountMin; ++SegmentIndex)
	{
		if (const int32 Compare = Segments[SegmentIndex].Compare(Path.Segments[SegmentIndex]))
		{
			return Compare < 0;
		}
	}

	return SegmentCountA < SegmentCountB;
}

void FPropertyPathName::PushTypeInternal(FName Type)
{
	FNameEntryId& LastType = Segments.Last().Type;

	TStringBuilder<256> CombinedTypes;
	FName::GetEntry(LastType)->AppendNameToString(CombinedTypes);
	CombinedTypes.AppendChar(' ');
	Type.AppendString(CombinedTypes);

	LastType = FName(CombinedTypes).GetDisplayIndex();
}

FName FPropertyPathName::PopType()
{
	if (Segments.IsEmpty())
	{
		return FName();
	}

	FNameEntryId& LastType = Segments.Last().Type;

	if (LastType.IsNone())
	{
		return FName();
	}

	FName PoppedType;
	TStringBuilder<256> CombinedTypes;
	FName::GetEntry(LastType)->AppendNameToString(CombinedTypes);

	const int32 Index = String::FindLastChar(CombinedTypes, TEXT(' '));
	if (Index == INDEX_NONE)
	{
		PoppedType = FName::CreateFromDisplayId(LastType, NAME_NO_NUMBER_INTERNAL);
		LastType = FNameEntryId();
	}
	else
	{
		const FStringView CombinedTypesView(CombinedTypes);
		PoppedType = FName(CombinedTypesView.RightChop(Index + 1));
		LastType = FName(CombinedTypesView.Left(Index)).GetDisplayIndex();
	}

	return PoppedType;
}

void FPropertyPathName::ToString(FStringBuilderBase& Out, FStringView Separator) const
{
	bool bFirst = true;
	for (const FSegment& Segment : Segments)
	{
		if (bFirst)
		{
			bFirst = false;
		}
		else
		{
			Out.Append(Separator);
		}

		FPropertyPathNameSegment UnpackedSegment = Segment.Unpack();
		Out << UnpackedSegment.Name;

		if (const int32 Index = UnpackedSegment.Index; Index != INDEX_NONE)
		{
			Out << TEXT('[') << Index << TEXT(']');
		}

		if (!UnpackedSegment.Type.IsNone())
		{
			Out << TEXTVIEW(" (") << UnpackedSegment.Type << TEXT(')');
		}
	}
}

uint32 GetTypeHash(const FPropertyPathName& Path)
{
	uint32 Hash = 0;
	for (const FPropertyPathName::FSegment& Segment : Path.Segments)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.NameWithIndex));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.Type));
	}
	return Hash;
}

} // UE
