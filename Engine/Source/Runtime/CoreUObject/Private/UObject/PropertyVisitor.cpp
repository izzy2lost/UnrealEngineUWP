// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyVisitor.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

FString FPropertyVisitorPath::ToString(const TCHAR* Separator /*= TEXT(".")*/) const
{
	TStringBuilder<FName::StringBufferSize> PropertyPath;
	bool bFirstEntry = true;
	for (const FPropertyVisitorInfo& Entry : Path)
	{
		bool bDisplayPropertyName = false;
		bool bDisplayIndex = false;
		FString Suffix(TEXT(""));
		switch (Entry.PropertyInfo)
		{
			case EPropertyVisitorInfoType::None:
				bDisplayPropertyName = true;
				break;
			case EPropertyVisitorInfoType::StaticArrayIndex:
				bDisplayPropertyName = true;
				bDisplayIndex = true;
				break;
			case EPropertyVisitorInfoType::ContainerIndex:
				bDisplayIndex = true;
				break;
			case EPropertyVisitorInfoType::MapKey:
				bDisplayIndex = true;
				Suffix = TEXT("Key");
				break;
			case EPropertyVisitorInfoType::MapValue:
				bDisplayIndex = true;
				Suffix = TEXT("Value");
				break;
			default:
				checkf(false, TEXT("Unsupported enum value"));
				break;
		}
		if (bDisplayPropertyName)
		{
			if (bFirstEntry)
			{
				bFirstEntry = false;
			}
			else
			{
				PropertyPath.Append(Separator);
			}
			PropertyPath.Append(Entry.Property->GetAuthoredName());
		}
		if (bDisplayIndex)
		{
			checkf(Entry.Index != INDEX_NONE, TEXT("Expecting the index to be valid"));
			PropertyPath.Appendf(TEXT("[%d]"), Entry.Index);
		}
		if (Suffix.Len())
		{
			PropertyPath.Append(Separator);
			PropertyPath.Append(Suffix);
		}
	}

	return PropertyPath.ToString();
}

bool FPropertyVisitorPath::Contained(const FPropertyVisitorPath& Other, bool* bIsEqual) const
{
	int32 i;
	for (i = 0; i < Path.Num(); ++i)
	{
		if (i >= Other.Path.Num())
		{
			// The other path is smaller than this one, so not contained in
			break;
		}

		const FPropertyVisitorInfo& PathInfo = Path[i];
		const FPropertyVisitorInfo& OtherPathInfo = Other.Path[i];

		if (PathInfo.Property != OtherPathInfo.Property)
		{
			// The property is different, so not contained in
			break;
		}

		if (PathInfo.PropertyInfo != OtherPathInfo.PropertyInfo)
		{
			if (PathInfo.PropertyInfo != EPropertyVisitorInfoType::None)
			{
				// The property info type is different and it is not none, so not contained in
				break;
			}
		}
		else if (PathInfo.Index != OtherPathInfo.Index)
		{
			// The index is different, so not contained in
			break;
		}
	}
	if (bIsEqual)
	{
		*bIsEqual = i == Other.Path.Num();
	}
	return i == Path.Num();
}

void* FPropertyVisitorPath::GetPropertyDataPtr(UObject* Object) const
{
	void* DataPtr = nullptr;
	checkf(Object, TEXT("Expecting an valid object"));

	int32 MatchedPathDepth = 0;
	Object->GetClass()->Visit(Object, [this, &DataPtr, &MatchedPathDepth](const FPropertyVisitorPath& InPath, void* Data)
	{
		if (InPath.Num() <= MatchedPathDepth)
		{
			// We've returned a level that we previously found a match in; we can stop now
			return EPropertyVisitorControlFlow::Stop;
		}
		bool bIsEqual = false;
		if (InPath.Contained(*this, &bIsEqual))
		{
			MatchedPathDepth = InPath.Num();
			if(bIsEqual)
			{
				DataPtr = Data;
				return EPropertyVisitorControlFlow::Stop;
			}
			return EPropertyVisitorControlFlow::StepInto;
		}
		return EPropertyVisitorControlFlow::StepOver;
	});

	return DataPtr;
}