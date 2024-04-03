// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/UnrealType.h"
#include "UObject/PropertyOptional.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "UObject/PropertyVisitor.h"

DEFINE_LOG_CATEGORY(LogType);

ENUM_CLASS_FLAGS(FPropertyValueIterator::EPropertyValueFlags)

#define EPropertyValueFlags_ContainerMask (EPropertyValueFlags::IsOptional | EPropertyValueFlags::IsArray | EPropertyValueFlags::IsMap | EPropertyValueFlags::IsSet | EPropertyValueFlags::IsStruct)

FPropertyValueIterator::FPropertyValueIterator(
	FFieldClass* InPropertyClass,
	const UStruct* InStruct,
	const void* InStructValue,
	EPropertyValueIteratorFlags InRecursionFlags,
	EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedPropertyFlags)
	: PropertyClass(InPropertyClass)
	, RecursionFlags(InRecursionFlags)
	, DeprecatedPropertyFlags(InDeprecatedPropertyFlags)
	, bSkipRecursionOnce(false)
	, bMatchAll(InPropertyClass == FProperty::StaticClass())
{
	FPropertyValueStackEntry Entry(InStructValue);
	FillStructProperties(InStruct, Entry);
	if (Entry.ValueArray.Num() > 0)
	{
		PropertyIteratorStack.Emplace(MoveTemp(Entry));

		while (NextValue(InRecursionFlags));
	}
}

FPropertyValueIterator::EPropertyValueFlags FPropertyValueIterator::GetPropertyValueFlags(const FProperty* Property) const
{
	EPropertyValueFlags Flags = EPropertyValueFlags::None;
	if (RecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
	{
		uint64 CastFlags = Property->GetClass()->GetCastFlags();
		Flags = EPropertyValueFlags(  !!(CastFlags & CASTCLASS_FArrayProperty)    * uint32(EPropertyValueFlags::IsArray)
									| !!(CastFlags & CASTCLASS_FMapProperty)      * uint32(EPropertyValueFlags::IsMap)
									| !!(CastFlags & CASTCLASS_FSetProperty)      * uint32(EPropertyValueFlags::IsSet)
									| !!(CastFlags & CASTCLASS_FStructProperty)   * uint32(EPropertyValueFlags::IsStruct)
									| !!(CastFlags & CASTCLASS_FOptionalProperty) * uint32(EPropertyValueFlags::IsOptional));
	}
	if (bMatchAll || Property->IsA(PropertyClass))
	{
		Flags |= EPropertyValueFlags::IsMatch;
	}
	return Flags;
}

void FPropertyValueIterator::FillStructProperties(const UStruct* Struct, FPropertyValueStackEntry& Entry)
{
	Struct->Visit(const_cast<void*>(Entry.Owner), [this, &Entry](const FPropertyVisitorPath& Path, void* Data)
	{
		if (const FProperty* InnerProperty = Path.Top().Property)
		{
			if ((DeprecatedPropertyFlags & EFieldIteratorFlags::IncludeDeprecated) != 0 || !InnerProperty->HasAllPropertyFlags(CPF_Deprecated))
			{
				EPropertyValueFlags InnerFlags = GetPropertyValueFlags(InnerProperty);
				if (InnerFlags != EPropertyValueFlags::None)
				{
					Entry.ValueArray.Emplace(BasePairType(InnerProperty, Data), InnerFlags);
				}
			}
		}
		return EPropertyVisitorControlFlow::StepOver;
	});
}

bool FPropertyValueIterator::NextValue(EPropertyValueIteratorFlags InRecursionFlags)
{
	check(PropertyIteratorStack.Num() > 0)
	FPropertyValueStackEntry& Entry = PropertyIteratorStack.Last();

	// If we have pending values, deal with them
	if (Entry.NextValueIndex < Entry.ValueArray.Num())
	{
		const bool bIsPropertyMatchProcessed = Entry.ValueIndex == Entry.NextValueIndex;
		Entry.ValueIndex = Entry.NextValueIndex;
		Entry.NextValueIndex = Entry.ValueIndex + 1;

		const FProperty* Property = Entry.ValueArray[Entry.ValueIndex].Key.Key;
		const void* PropertyValue = Entry.ValueArray[Entry.ValueIndex].Key.Value;
		const EPropertyValueFlags PropertyValueFlags = Entry.ValueArray[Entry.ValueIndex].Value;
		check(PropertyValueFlags != EPropertyValueFlags::None);

		// Handle matching properties
		if (!bIsPropertyMatchProcessed && EnumHasAnyFlags(PropertyValueFlags, EPropertyValueFlags::IsMatch))
		{
			if (EnumHasAnyFlags(PropertyValueFlags, EPropertyValueFlags_ContainerMask))
			{
				// this match is also a container/struct, so recurse into it next time
				Entry.NextValueIndex = Entry.ValueIndex;
			}
			return false; // Break at this matching property
		}

		// Handle container properties
		check(EnumHasAnyFlags(PropertyValueFlags, EPropertyValueFlags_ContainerMask));
		if (InRecursionFlags == EPropertyValueIteratorFlags::FullRecursion)
		{
			FPropertyValueStackEntry NewEntry(PropertyValue);
			Property->Visit(const_cast<void*>(PropertyValue), [this, &NewEntry](const FPropertyVisitorPath& Path, void* Data)
			{
				if (const FProperty* InnerProperty = Path.Top().Property)
				{
					if ((DeprecatedPropertyFlags & EFieldIteratorFlags::IncludeDeprecated) != 0 || !InnerProperty->HasAllPropertyFlags(CPF_Deprecated))
					{
						// Visit any properties at the top level that contains inner properties and are not object references
						if (Path.Num() == 1 && Path.Top().bContainsInnerProperties && !InnerProperty->IsA<FObjectPropertyBase>())
						{
							return EPropertyVisitorControlFlow::StepInto;
						}

						EPropertyValueFlags InnerFlags = GetPropertyValueFlags(InnerProperty);
						if (InnerFlags != EPropertyValueFlags::None)
						{
							NewEntry.ValueArray.Emplace(BasePairType(InnerProperty, Data), InnerFlags);
						}
					}
				}
				return EPropertyVisitorControlFlow::StepOver;
			});
			
			if (NewEntry.ValueArray.Num() > 0)
			{
				PropertyIteratorStack.Emplace(MoveTemp(NewEntry));
				return true; // NextValue should be called again to move to the top of the stack
			}
		}
	}

	if (Entry.NextValueIndex == Entry.ValueArray.Num())
	{
		PropertyIteratorStack.Pop();
	}

	// NextValue should be called again to continue iteration
	return PropertyIteratorStack.Num() > 0;
}

void FPropertyValueIterator::IterateToNext()
{
	if (bSkipRecursionOnce)
	{
		bSkipRecursionOnce = false;
		
		if (!NextValue(EPropertyValueIteratorFlags::NoRecursion))
		{
			return;
		}
	}

	EPropertyValueIteratorFlags LocalRecursionFlags = RecursionFlags;	
	while (NextValue(LocalRecursionFlags));
}

void FPropertyValueIterator::GetPropertyChain(TArray<const FProperty*>& PropertyChain) const
{
	PropertyChain.Reserve(PropertyIteratorStack.Num());
	// Iterate over struct/container property stack, starting at the inner most property
	for (int32 StackIndex = PropertyIteratorStack.Num() - 1; StackIndex >= 0; StackIndex--)
	{
		const FPropertyValueStackEntry& Entry = PropertyIteratorStack[StackIndex];

		// Index should always be valid
		const FProperty* Property = Entry.ValueArray[Entry.ValueIndex].Key.Key;
		PropertyChain.Add(Property);
	}
}

FString FPropertyValueIterator::GetPropertyPathDebugString() const
{
	TStringBuilder<FName::StringBufferSize> PropertyPath;
	for (int32 StackIndex = 0; StackIndex < PropertyIteratorStack.Num(); StackIndex++)
	{
		const FPropertyValueStackEntry& Entry = PropertyIteratorStack[StackIndex];

		// Index should always be valid
		const BasePairType& PropertyAndValue = Entry.ValueArray[Entry.ValueIndex].Key;
		const FProperty* Property = PropertyAndValue.Key;
		const void* ValuePtr = PropertyAndValue.Value;

		PropertyPath.Append(Property->GetAuthoredName());

		int32 NextStackIndex = StackIndex + 1;

		if (NextStackIndex < PropertyIteratorStack.Num())
		{
			if (CastField<FOptionalProperty>(Property))
			{
				PropertyPath.Append(TEXT("?"));
				StackIndex++;
			}
			else if (CastField<FArrayProperty>(Property))
			{
				const FPropertyValueStackEntry& NextEntry = PropertyIteratorStack[StackIndex+1];
			
				PropertyPath.Append(TEXT("["));
				PropertyPath.Appendf(TEXT("%d"), NextEntry.ValueIndex);
				PropertyPath.Append(TEXT("]"));

				StackIndex++;
			}
			else if (CastField<FSetProperty>(Property))
			{
			
			}
			else if (CastField<FMapProperty>(Property))
			{
				const FPropertyValueStackEntry& NextEntry = PropertyIteratorStack[StackIndex+1];

				const FProperty* NextProperty = NextEntry.GetPropertyValue().Key;
				const void* NextValuePtr = NextEntry.GetPropertyValue().Value;

				FString KeyStr;
				NextProperty->ExportText_Direct(KeyStr, NextValuePtr, nullptr, nullptr, PPF_None);
				
				PropertyPath.Append(TEXT("["));
				PropertyPath.Append(KeyStr);
				PropertyPath.Append(TEXT("]"));
			}

			NextStackIndex = StackIndex + 1;

			if (NextStackIndex < PropertyIteratorStack.Num())
			{
				PropertyPath.Append(TEXT("."));
			}
		}
	}

	return PropertyPath.ToString();
}
