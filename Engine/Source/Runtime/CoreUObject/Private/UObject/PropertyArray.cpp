// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "UObject/ObjectMacros.h"
#include "Templates/Casts.h"
#include "UObject/PropertyPathName.h"
#include "UObject/PropertyTag.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"
#include "UObject/LinkerLoad.h"
#include "UObject/PropertyHelper.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/OverriddenPropertySet.h"

/*-----------------------------------------------------------------------------
	FArrayProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FArrayProperty)

FArrayProperty::FArrayProperty(FFieldVariant InOwner, const UECodeGen_Private::FArrayPropertyParams& Prop)
	: Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
	, Inner(nullptr)
{
	ArrayFlags = Prop.ArrayFlags;
	SetElementSize();
}

#if WITH_EDITORONLY_DATA
FArrayProperty::FArrayProperty(UField* InField)
	: FArrayProperty_Super(InField)
	, ArrayFlags(EArrayPropertyFlags::None)
{
	UArrayProperty* SourceProperty = CastChecked<UArrayProperty>(InField);
	Inner = CastField<FProperty>(SourceProperty->Inner->GetAssociatedFField());
	if (!Inner)
	{
		Inner = CastField<FProperty>(CreateFromUField(SourceProperty->Inner));
		SourceProperty->Inner->SetAssociatedFField(Inner);
	}
}
#endif // WITH_EDITORONLY_DATA

FArrayProperty::~FArrayProperty()
{
	delete Inner;
	Inner = nullptr;
}

void FArrayProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	if (Inner)
	{
		Inner->GetPreloadDependencies(OutDeps);
	}
}

void FArrayProperty::PostDuplicate(const FField& InField)
{
	const FArrayProperty& Source = static_cast<const FArrayProperty&>(InField);
	Inner = CastFieldChecked<FProperty>(FField::Duplicate(Source.Inner, this));
	Super::PostDuplicate(InField);
}

void FArrayProperty::LinkInternal(FArchive& Ar)
{
	//FLinkerLoad* MyLinker = GetLinker();
	//if( MyLinker )
	//{
	//	MyLinker->Preload(this);
	//}
	//Ar.Preload(Inner);
	Inner->Link(Ar);

	SetElementSize();
}
bool FArrayProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	checkSlow(Inner);

	FScriptArrayHelper ArrayHelperA(this, A);

	const int32 ArrayNum = ArrayHelperA.Num();
	if ( B == NULL )
	{
		return ArrayNum == 0;
	}

	FScriptArrayHelper ArrayHelperB(this, B);
	if ( ArrayNum != ArrayHelperB.Num() )
	{
		return false;
	}

	for ( int32 ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
	{
		if ( !Inner->Identical( ArrayHelperA.GetRawPtr(ArrayIndex), ArrayHelperB.GetRawPtr(ArrayIndex), PortFlags) )
		{
			return false;
		}
	}

	return true;
}

static bool CanBulkSerialize(FProperty* Property)
{
#if PLATFORM_LITTLE_ENDIAN
	// All numeric properties except TEnumAsByte
	uint64 CastFlags = Property->GetClass()->GetCastFlags();
	if (!!(CastFlags & CASTCLASS_FNumericProperty))
	{
		bool bEnumAsByte = (CastFlags & CASTCLASS_FByteProperty) != 0 && static_cast<FByteProperty*>(Property)->Enum;
		return !bEnumAsByte;
	}
#endif

	return false;
}

void FArrayProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	check(Inner);
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	const bool bIsTextFormat = UnderlyingArchive.IsTextFormat();
	const bool bUPS = UnderlyingArchive.UseUnversionedPropertySerialization();
	bool bExperimentalOverridableLogic = HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic);
	TOptional<FPropertyTag> MaybeInnerTag;

	// Ensure that the Inner itself has been loaded before calling SerializeItem() on it
	//UnderlyingArchive.Preload(Inner);

	FScriptArrayHelper ArrayHelper(this, Value);
	int32		n		= ArrayHelper.Num();

	// Custom branch for UPS to try and take advantage of bulk serialization
	if (bUPS && !bExperimentalOverridableLogic)
	{
		checkf(!UnderlyingArchive.ArUseCustomPropertyList, TEXT("Custom property lists are not supported with UPS"));
		checkf(!bIsTextFormat, TEXT("Text-based archives are not supported with UPS"));

		if (CanBulkSerialize(Inner))
		{
			// We need to enter the slot as *something* to keep the structured archive system happy,
			// but which maps down to straight writes to the underlying archive.
			FStructuredArchiveStream Stream = Slot.EnterStream();

			Stream.EnterElement() << n;

			if (UnderlyingArchive.IsLoading())
			{
				ArrayHelper.EmptyAndAddUninitializedValues(n);
			}

			Stream.EnterElement().Serialize(ArrayHelper.GetRawPtr(), n * Inner->ElementSize);
		}
		else
		{
			FStructuredArchiveArray Array = Slot.EnterArray(n);

			if (UnderlyingArchive.IsLoading())
			{
				ArrayHelper.EmptyAndAddValues(n);
			}

			FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
			for (int32 i = 0; i < n; ++i)
			{
#if WITH_EDITOR
				static const FName NAME_UArraySerialize = FName(TEXT("FArrayProperty::Serialize"));
				FName NAME_UArraySerializeCount = FName(NAME_UArraySerialize);
				NAME_UArraySerializeCount.SetNumber(i);
				FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_UArraySerializeCount);
#endif
				Inner->SerializeItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i));
			}
		}

		return;
	}

	if (bIsTextFormat && Inner->IsA<FStructProperty>())
	{
		MaybeInnerTag.Emplace(UnderlyingArchive, Inner, 0, (uint8*)Value, (uint8*)Defaults);	
		Slot << SA_ATTRIBUTE(TEXT("InnerStructName"), MaybeInnerTag.GetValue().StructName);
		Slot << SA_OPTIONAL_ATTRIBUTE(TEXT("InnerStructGuid"), MaybeInnerTag.GetValue().StructGuid, FGuid());
	}

	TOptional<FPropertyTag> SerializeFromMismatchedTag;

	// TODO: Should work for maps + sets too.
	auto SerializeContainerItem = [this, &SerializeFromMismatchedTag](FStructuredArchiveSlot Slot, uint8* Item)
	{
		if(SerializeFromMismatchedTag.IsSet())
		{
			const int64 StartOfProperty = Slot.GetUnderlyingArchive().Tell();
			FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Inner);
			switch(StructProperty->ConvertFromType(SerializeFromMismatchedTag.GetValue(), Slot, Item, nullptr, nullptr))
			{
				case EConvertFromTypeResult::Converted:
				case EConvertFromTypeResult::Serialized:
					return;
				case EConvertFromTypeResult::CannotConvert:
					// FStructProperty::ConvertFromType doesn't handle setting the default, so do it here.
					StructProperty->Struct->InitializeDefaultValue(Item);
					Slot.GetUnderlyingArchive().Seek(StartOfProperty + SerializeFromMismatchedTag.GetValue().Size);	// Skip this item
					return;
				case EConvertFromTypeResult::UseSerializeItem:
					// Fall through to default serialize
					break;
			}
		}
			
		Inner->SerializeItem(Slot, Item);
	};
	
	// Make sure the container is reloading accordingly to the value set in the property tag if any
	if (!bUPS && UnderlyingArchive.IsLoading() && FPropertyTagScope::GetCurrentPropertyTag())
	{
		bExperimentalOverridableLogic = FPropertyTagScope::GetCurrentPropertyTag()->bExperimentalOverridableLogic;
	}

	// *** Experimental *** Special serialization path for array with overridable serialization
	if(bExperimentalOverridableLogic)
	{
		checkf(!UnderlyingArchive.ArUseCustomPropertyList, TEXT("Using custom property list is not supported by overridable serialization"));

		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		if (UnderlyingArchive.IsLoading())
		{
			int32 NumReplaced = 0;
			FStructuredArchive::FArray ReplacedArray = Record.EnterArray(TEXT("Replaced"), NumReplaced);
			if (NumReplaced != INDEX_NONE)
			{
				ArrayHelper.EmptyAndAddValues(NumReplaced);
				for (int32 i = 0; i < NumReplaced; i++)
				{
					SerializeContainerItem(ReplacedArray.EnterElement(), ArrayHelper.GetRawPtr(i));
				}
			}
			else
			{
				// Only Array of Instanced subobject are handled here as sort of a set where the matching key is done using the archetype
				const FObjectProperty* InnerObjectProperty = CastFieldChecked<FObjectProperty>(Inner);
				checkf(InnerObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance), TEXT("Only supported code path here is the instanced subobjects"));

				FOverriddenPropertySet* OverriddenProperties = FOverridableSerializationLogic::GetOverriddenProperties();

				checkf(Defaults, TEXT("Expecting overridable serialization to have defaults to compare to"));

				FScriptArrayHelper DefaultsArrayHelper(this, Defaults);

				auto FindObject = [InnerObjectProperty](UObject* Object, UObject* Object2, FScriptArrayHelper& ArrayHelper) -> int32
				{
					if (Object)
					{
						const int32 ArrayNum = ArrayHelper.Num();
						for (int i = 0; i < ArrayNum; ++i)
						{
							UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i));
							if (CurrentObject == Object || (Object2 && CurrentObject == Object2))
							{
								return i;
							}
						}
					}
					return INDEX_NONE;
				};

				uint8* TempValueStorage = nullptr;
				ON_SCOPE_EXIT
				{
					if (TempValueStorage)
					{
						InnerObjectProperty->DestroyValue(TempValueStorage);
						FMemory::Free(TempValueStorage);
					}
				};

				int32 NumRemoved = 0;
				FStructuredArchive::FArray RemovedArray = Record.EnterArray(TEXT("Removed"), NumRemoved);
				if(NumRemoved != 0)
				{
					TArray<int32> IndicesToRemove;
					TempValueStorage = (uint8*)FMemory::Malloc(InnerObjectProperty->ElementSize);
					InnerObjectProperty->InitializeValue(TempValueStorage);

					for (int32 i = 0; i < NumRemoved; ++i)
					{
						{
							FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
							SerializeContainerItem(RemovedArray.EnterElement(), TempValueStorage);
						}

						if (UObject* RemovedSubObject = InnerObjectProperty->GetObjectPropertyValue(TempValueStorage) )
						{
							int32 Index = FindObject(RemovedSubObject, nullptr, DefaultsArrayHelper);
							if (Index != INDEX_NONE)
							{
								IndicesToRemove.Add(Index);
							}

							// Need to fetch the ArrayOverriddenPropertyNode every loop as the previous iteration might have reallocated the node.
							if (FOverriddenPropertyNode* ArrayOverriddenPropertyNode = OverriddenProperties ? OverriddenProperties->SetOverriddenPropertyOperation(EOverriddenPropertyOperation::Modified, UnderlyingArchive.GetSerializedPropertyChain(), /*Property*/nullptr) : nullptr)
							{
								// Rebuild the overridden info
								const FOverriddenPropertyNodeID RemovedSubObjectID(*RemovedSubObject);
								OverriddenProperties->SetSubPropertyOperation(EOverriddenPropertyOperation::Remove, *ArrayOverriddenPropertyNode, RemovedSubObjectID);
							}
						}
					}

					IndicesToRemove.Sort(TGreater<>());
					for(int32 IndexToRemove : IndicesToRemove)
					{
						ArrayHelper.RemoveValues(IndexToRemove);
					}
				}

				int32 NumModified = 0;
				FStructuredArchive::FArray ModifiedArray = Record.EnterArray(TEXT("Modified"), NumModified);
				if (NumModified != 0)
				{
					if (!TempValueStorage)
					{
						TempValueStorage = (uint8*)FMemory::Malloc(InnerObjectProperty->ElementSize);
						InnerObjectProperty->InitializeValue(TempValueStorage);
					}

					for (int32 i = 0; i < NumModified; ++i)
					{
						{
							FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
							SerializeContainerItem(ModifiedArray.EnterElement(), TempValueStorage);
						}

						if (UObject* ModifiedObject = InnerObjectProperty->GetObjectPropertyValue(TempValueStorage))
						{
							int32 Index = FindObject(ModifiedObject->GetArchetype(), ModifiedObject, ArrayHelper);
							if (Index != INDEX_NONE)
							{
								InnerObjectProperty->SetObjectPropertyValue(ArrayHelper.GetRawPtr(Index), ModifiedObject);
							}
						}
					}
				}

				int32 NumAdded = 0;
				FStructuredArchive::FArray AddedArray = Record.EnterArray(TEXT("Added"), NumAdded);
				if (NumAdded != 0)
				{
					if (!TempValueStorage)
					{
						TempValueStorage = (uint8*)FMemory::Malloc(InnerObjectProperty->ElementSize);
						InnerObjectProperty->InitializeValue(TempValueStorage);
					}

					int32 AddIndex = ArrayHelper.Num();
					for (int32 i = 0; i < NumAdded; ++i)
					{
						{
							FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
							SerializeContainerItem(AddedArray.EnterElement(), TempValueStorage);
						}

						if (UObject* AddedSubObject = InnerObjectProperty->GetObjectPropertyValue(TempValueStorage))
						{
							int32 Index = FindObject(AddedSubObject->GetArchetype(), AddedSubObject, ArrayHelper);
							if (Index == INDEX_NONE)
							{
								ArrayHelper.AddValue();
								Index = AddIndex++;
							}
							InnerObjectProperty->SetObjectPropertyValue(ArrayHelper.GetRawPtr(Index), AddedSubObject);

							// Need to fetch the ArrayOverriddenPropertyNode every loop as the previous iteration might have reallocated the node.
							if (FOverriddenPropertyNode* ArrayOverriddenPropertyNode = OverriddenProperties ? OverriddenProperties->SetOverriddenPropertyOperation(EOverriddenPropertyOperation::Modified, UnderlyingArchive.GetSerializedPropertyChain(), /*Property*/nullptr) : nullptr)
							{
								// Rebuild the overridden info
								const FOverriddenPropertyNodeID AddedSubObjectID(*AddedSubObject);
								OverriddenProperties->SetSubPropertyOperation(EOverriddenPropertyOperation::Add, *ArrayOverriddenPropertyNode, AddedSubObjectID);
							}
						}
					}
				}
			}
		}
		else
		{
			// Container for temporarily tracking some indices
			TArray<int32> RemovedIndices;
			TArray<int32> ModifiedIndices;
			TArray<int32> AddedIndices;

			bool bReplaceArray = false;
			if (!Defaults || !UnderlyingArchive.DoDelta() || UnderlyingArchive.IsTransacting())
			{
				bReplaceArray = true;
			}
			else
			{
				const FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(Inner);
				EOverriddenPropertyOperation ArrayOverrideOp = EOverriddenPropertyOperation::None;
				FOverriddenPropertySet* OverriddenProperties = FOverridableSerializationLogic::GetOverriddenProperties();
				if (OverriddenProperties)
				{
					ArrayOverrideOp = OverriddenProperties->GetOverriddenPropertyOperation(UnderlyingArchive.GetSerializedPropertyChain(), /*Property*/nullptr);
					bReplaceArray = ArrayOverrideOp == EOverriddenPropertyOperation::Replace;
				}
				else
				{
					bReplaceArray = !InnerObjectProperty || !InnerObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance);
				}

				if (!bReplaceArray)
				{
					// Only array of instanced subobjects are handled here as sort of a set where the matching key is done using the archetype
					checkf(InnerObjectProperty&& InnerObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance), TEXT("Expecting only arrays of instanced subobjects"));

					// We need to always serialize instanced subobjects to know if they have overridden values.
					const int32 ArrayNum = ArrayHelper.Num();
					for (int i = 0; i < ArrayNum; i++)
					{
						ModifiedIndices.Add(i);
					}

					if (OverriddenProperties && ArrayOverrideOp != EOverriddenPropertyOperation::None)
					{

						auto FindObject = [InnerObjectProperty](const FOverriddenPropertyNodeID ObjectToFind, FScriptArrayHelper& ArrayHelper) -> int32
						{
							const int32 ArrayNum = ArrayHelper.Num();
							for (int i = 0; i < ArrayNum; ++i)
							{
								if (UObject* CurrentObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i)))
								{
									if (ObjectToFind == FOverriddenPropertyNodeID(*CurrentObject))
									{
										return i;
									}
								}
							}
							return INDEX_NONE;
						};

						FScriptArrayHelper DefaultsArrayHelper(this, Defaults);

						if (const FOverriddenPropertyNode* ArrayOverriddenPropertyNode = OverriddenProperties->GetOverriddenPropertyNode(UnderlyingArchive.GetSerializedPropertyChain()))
						{
							for (const auto& Pair : ArrayOverriddenPropertyNode->SubPropertyNodeKeys)
							{
								const EOverriddenPropertyOperation OverrideOp = OverriddenProperties->GetSubPropertyOperation(Pair.Value);
								switch (OverrideOp)
								{
								case EOverriddenPropertyOperation::Remove:
									{
										const int32 DefaultIndex = FindObject(Pair.Key, DefaultsArrayHelper);
										if (DefaultIndex != INDEX_NONE)
										{
											RemovedIndices.Add(DefaultIndex);
										}
										break;
									}
								case EOverriddenPropertyOperation::Add:
									{
										const int32 Index = FindObject(Pair.Key, ArrayHelper);
										if(Index != INDEX_NONE)
										{
											AddedIndices.Add(Index);
											ModifiedIndices.Remove(Index);
										}
										break;
									}
								default:
									checkf(false, TEXT("Unsupported operation type"));
									break;
								}
							}
						}
					}
				}
			}

			int32 NumReplaced = bReplaceArray ? ArrayHelper.Num() : INDEX_NONE;
			FStructuredArchive::FArray ReplacedArray = Record.EnterArray(TEXT("Replaced"), NumReplaced);
			if(bReplaceArray)
			{
				const int32 ArrayNum = ArrayHelper.Num();
				for (int32 i =0; i < ArrayNum; ++i)
				{
					SerializeContainerItem(ReplacedArray.EnterElement(), ArrayHelper.GetRawPtr(i));
				}
			}
			else
			{
				checkf(Defaults, TEXT("Expecting overridable serialization to have defaults to compare to"));
				FScriptArrayHelper DefaultsArrayHelper(this, Defaults);

				int32 NumRemoved = RemovedIndices.Num();
				FStructuredArchive::FArray RemovedArray = Record.EnterArray(TEXT("Removed"), NumRemoved);
				for (int32 i : RemovedIndices)
				{
					SerializeContainerItem(RemovedArray.EnterElement(), DefaultsArrayHelper.GetRawPtr(i));
				}

				int32 NumModified = ModifiedIndices.Num();
				FStructuredArchive::FArray ModifiedArray = Record.EnterArray(TEXT("Modified"), NumModified);
				for (int32 i : ModifiedIndices)
				{
					SerializeContainerItem(ModifiedArray.EnterElement(), ArrayHelper.GetRawPtr(i));
				}

				int32 NumAdded = AddedIndices.Num();
				FStructuredArchive::FArray AddedArray = Record.EnterArray(TEXT("Added"), NumAdded);
				for (int32 i : AddedIndices)
				{
					SerializeContainerItem(AddedArray.EnterElement(), ArrayHelper.GetRawPtr(i));
				}
			}
		}
		return;
	}

	FStructuredArchiveArray Array = Slot.EnterArray(n);

	if( UnderlyingArchive.IsLoading() )
	{
		// If using a custom property list, don't empty the array on load. Not all indices may have been serialized, so we need to preserve existing values at those slots.
		if (UnderlyingArchive.ArUseCustomPropertyList)
		{
			const int32 OldNum = ArrayHelper.Num();
			if (n > OldNum)
			{
				ArrayHelper.AddValues(n - OldNum);
			}
			else if (n < OldNum)
			{
				ArrayHelper.RemoveValues(n, OldNum - n);
			}
		}
		else
		{
			ArrayHelper.EmptyAndAddValues(n);
		}
	}
	ArrayHelper.CountBytes( UnderlyingArchive );
	
	// Serialize a PropertyTag for the inner property of this array, allows us to validate the inner struct to see if it has changed
	if (UnderlyingArchive.UEVer() >= VER_UE4_INNER_ARRAY_TAG_INFO && Inner->IsA<FStructProperty>())
	{
		if (!MaybeInnerTag)
		{
			MaybeInnerTag.Emplace(UnderlyingArchive, Inner, 0, (uint8*)Value, (uint8*)Defaults);
			UnderlyingArchive << MaybeInnerTag.GetValue();
			Inner->AssignToTag(MaybeInnerTag.GetValue());
		}

		FPropertyTag& InnerTag = MaybeInnerTag.GetValue();

		if (UnderlyingArchive.IsLoading())
		{
			auto CanSerializeFromStructWithDifferentName = [](const FPropertyTag& PropertyTag, const FStructProperty* StructProperty)
			{
				return StructProperty
					&& StructProperty->Struct
					&& PropertyTag.StructGuid.IsValid()
					&& PropertyTag.StructGuid == StructProperty->Struct->GetCustomGuid();
			};

			// Check if the Inner property can successfully serialize, the type may have changed
			FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Inner);
			// if check redirector to make sure if the name has changed
			FName NewName = FLinkerLoad::FindNewNameForStruct(InnerTag.StructName);
			FName StructName = StructProperty->Struct->GetFName();
			if (NewName != NAME_None && NewName == StructName)
			{
				InnerTag.StructName = NewName;
			}

			if (InnerTag.StructName != StructProperty->Struct->GetFName()
				&& !CanSerializeFromStructWithDifferentName(InnerTag, StructProperty))
			{
				// Attempt mismatched tag serialization if available
				if ((StructProperty->Struct->StructFlags & STRUCT_SerializeFromMismatchedTag) && (InnerTag.Type != NAME_StructProperty || (InnerTag.StructName != StructProperty->Struct->GetFName())))
				{
					SerializeFromMismatchedTag = InnerTag;
				}
				else
				{
					UE_LOG(LogClass, Warning, TEXT("Array Property %s of %s contains a struct type mismatch (tag %s != prop %s) in package:  %s. If that struct got renamed, add an entry to ActiveStructRedirects."),
					*InnerTag.Name.ToString(), *GetName(), *InnerTag.StructName.ToString(), *CastFieldChecked<FStructProperty>(Inner)->Struct->GetName(), *UnderlyingArchive.GetArchiveName());

#if WITH_EDITOR
					// Ensure the structure is initialized
					for (int32 i = 0; i < n; i++)
					{
						StructProperty->Struct->InitializeDefaultValue(ArrayHelper.GetRawPtr(i));
					}
#endif // WITH_EDITOR

					if (!bIsTextFormat)
					{
						// Skip the property
						const int64 StartOfProperty = UnderlyingArchive.Tell();
						const int64 RemainingSize = InnerTag.Size - (UnderlyingArchive.Tell() - StartOfProperty);
						uint8 B;
						for (int64 i = 0; i < RemainingSize; i++)
						{
							UnderlyingArchive << B;
						}
					}
					return;
				}
			}
		}
	}

	FUObjectSerializeContext* Context = FUObjectThreadContext::Get().GetSerializeContext();
	if (Context && Context->bTrackSerializedPropertyPath && MaybeInnerTag)
	{
		Context->SerializedPropertyPath.PushType(MaybeInnerTag->StructName);
	}

	// need to know how much data this call to SerializeItem consumes, so mark where we are
	int64 DataOffset = UnderlyingArchive.Tell();

	// If we're using a custom property list, first serialize any explicit indices
	int32 i = 0;
	bool bSerializeRemainingItems = true;
	bool bUsingCustomPropertyList = UnderlyingArchive.ArUseCustomPropertyList;
	if (bUsingCustomPropertyList && UnderlyingArchive.ArCustomPropertyList != nullptr)
	{
		// Initially we only serialize indices that are explicitly specified (in order)
		bSerializeRemainingItems = false;

		const FCustomPropertyListNode* CustomPropertyList = UnderlyingArchive.ArCustomPropertyList;
		const FCustomPropertyListNode* PropertyNode = CustomPropertyList;
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
		while (PropertyNode && i < n && !bSerializeRemainingItems)
		{
			if (PropertyNode->Property != Inner)
			{
				// A null property value signals that we should serialize the remaining array values in full starting at this index
				if (PropertyNode->Property == nullptr)
				{
					i = PropertyNode->ArrayIndex;
				}

				bSerializeRemainingItems = true;
			}
			else
			{
				// Set a temporary node to represent the item
				FCustomPropertyListNode ItemNode = *PropertyNode;
				ItemNode.ArrayIndex = 0;
				ItemNode.PropertyListNext = nullptr;
				UnderlyingArchive.ArCustomPropertyList = &ItemNode;

				// Serialize the item at this array index
				i = PropertyNode->ArrayIndex;
				if (Context && Context->bTrackSerializedPropertyPath)
				{
					Context->SerializedPropertyPath.SetIndex(i);
					
					// broadcast that a property will be serialized
					Context->OnTaggedPropertySerialize.Broadcast(*Context);
				}
				SerializeContainerItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i));
				PropertyNode = PropertyNode->PropertyListNext;

				// Restore the current property list
				UnderlyingArchive.ArCustomPropertyList = CustomPropertyList;
			}
		}
	}

	if (bSerializeRemainingItems)
	{
		// Temporarily suspend the custom property list (as we need these items to be serialized in full)
		UnderlyingArchive.ArUseCustomPropertyList = false;

		// Serialize each item until we get to the end of the array
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Inner, this);
		while (i < n)
		{
#if WITH_EDITOR
			static const FName NAME_UArraySerialize = FName(TEXT("FArrayProperty::Serialize"));
			FName NAME_UArraySerializeCount = FName(NAME_UArraySerialize);
			NAME_UArraySerializeCount.SetNumber(i);
			FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_UArraySerializeCount);
#endif
			if (Context && Context->bTrackSerializedPropertyPath)
			{
				Context->SerializedPropertyPath.SetIndex(i);
					
				// broadcast that a property will be serialized
				Context->OnTaggedPropertySerialize.Broadcast(*Context);
			}
			SerializeContainerItem(Array.EnterElement(), ArrayHelper.GetRawPtr(i++));
		}

		// Restore use of the custom property list (if it was previously enabled)
		UnderlyingArchive.ArUseCustomPropertyList = bUsingCustomPropertyList;
	}

	if (MaybeInnerTag.IsSet() && UnderlyingArchive.IsSaving() && !bIsTextFormat)
	{
		FPropertyTag& InnerTag = MaybeInnerTag.GetValue();

		// set the tag's size
		InnerTag.Size = IntCastChecked<int32>(UnderlyingArchive.Tell() - DataOffset);

		if (InnerTag.Size > 0)
		{
			// mark our current location
			DataOffset = UnderlyingArchive.Tell();

			// go back and re-serialize the size now that we know it
			UnderlyingArchive.Seek(InnerTag.SizeOffset);
			UnderlyingArchive << InnerTag.Size;

			// return to the current location
			UnderlyingArchive.Seek(DataOffset);
		}
	}

	if (Context && Context->bTrackSerializedPropertyPath)
	{
		Context->SerializedPropertyPath.SetIndex(INDEX_NONE);
	}
}

bool FArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );
	return 1;
}

void FArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	
	SerializeSingleField(Ar, Inner, this);
	checkSlow(Inner);
}
void FArrayProperty::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	if (Inner)
	{
		Inner->AddReferencedObjects(Collector);
	}
}

FString FArrayProperty::GetCPPTypeCustom(FString* ExtendedTypeText, uint32 CPPExportFlags, const FString& InnerTypeText, const FString& InInnerExtendedTypeText) const
{
	if (ExtendedTypeText != NULL)
	{
		FString InnerExtendedTypeText = InInnerExtendedTypeText;
		if (InnerExtendedTypeText.Len() && InnerExtendedTypeText.Right(1) == TEXT(">"))
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		else if (!InnerExtendedTypeText.Len() && InnerTypeText.Len() && InnerTypeText.Right(1) == TEXT(">"))
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *InnerTypeText, *InnerExtendedTypeText);
	}
	return TEXT("TArray");
}

FString FArrayProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	checkSlow(Inner);
	FString InnerExtendedTypeText;
	FString InnerTypeText;
	if ( ExtendedTypeText != NULL )
	{
		InnerTypeText = Inner->GetCPPType(&InnerExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider array inners to be "arguments or return values"
	}
	return GetCPPTypeCustom(ExtendedTypeText, CPPExportFlags, InnerTypeText, InnerExtendedTypeText);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FString FArrayProperty::GetCPPTypeForwardDeclaration() const
{
	checkSlow(Inner);
	return Inner->GetCPPTypeForwardDeclaration();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FString FArrayProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(Inner);
	ExtendedTypeText = Inner->GetCPPType();
	return TEXT("TARRAY");
}
void FArrayProperty::ExportText_Internal( FString& ValueStr, const void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	checkSlow(Inner);

	uint8* TempArrayStorage = nullptr;
	void* PropertyValuePtr = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		// Allocate temporary map as we first need to initialize it with the value provided by the getter function and then export it
		TempArrayStorage = (uint8*)AllocateAndInitializeValue();
		PropertyValuePtr = TempArrayStorage;
		FProperty::GetValue_InContainer(ContainerOrPropertyPtr, PropertyValuePtr);
	}
	else
	{
		PropertyValuePtr = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	}

	ON_SCOPE_EXIT
	{
		DestroyAndFreeValue(TempArrayStorage);
	};

	FScriptArrayHelper ArrayHelper(this, PropertyValuePtr);

	int32 DefaultSize = 0;
	if (DefaultValue)
	{
		FScriptArrayHelper DefaultArrayHelper(this, DefaultValue);
		DefaultSize = DefaultArrayHelper.Num();
		DefaultValue = DefaultArrayHelper.GetRawPtr(0);
	}

	ExportTextInnerItem(ValueStr, Inner, ArrayHelper.GetRawPtr(0), ArrayHelper.Num(), DefaultValue, DefaultSize, Parent, PortFlags, ExportRootScope);
}

void FArrayProperty::ExportTextInnerItem(FString& ValueStr, const FProperty* Inner, const void* PropertyValue, int32 PropertySize, const void* DefaultValue, int32 DefaultSize, UObject* Parent, int32 PortFlags, UObject* ExportRootScope)
{
	checkSlow(Inner);

	uint8* StructDefaults = NULL;
	const FStructProperty* StructProperty = CastField<FStructProperty>(Inner);

	const bool bReadableForm = (0 != (PPF_BlueprintDebugView & PortFlags));
	const bool bExternalEditor = (0 != (PPF_ExternalEditor & PortFlags));

	// ArrayProperties only export a diff because array entries are cleared and recreated upon import. Static arrays are overwritten when importing,
	// so we export the entire struct to ensure all data is copied over correctly. Behavior is currently inconsistent when copy/pasting between the two types.
	// In the future, static arrays could export diffs if the property being imported to is reset to default before the import.
	// When exporting to an external editor, we want to save defaults so all information is available for editing
	if ( StructProperty != NULL && Inner->ArrayDim == 1 && !bExternalEditor )
	{
		checkSlow(StructProperty->Struct);
		StructDefaults = (uint8*)FMemory::Malloc(StructProperty->Struct->GetStructureSize() * Inner->ArrayDim);
		StructProperty->InitializeValue(StructDefaults);
	}

	int32 Count = 0;
	for( int32 i=0; i<PropertySize; i++ )
	{
		++Count;
		if(!bReadableForm)
		{
			if ( Count == 1 )
			{
				ValueStr += TCHAR('(');
			}
			else
			{
				ValueStr += TCHAR(',');
			}
		}
		else
		{
			if(Count > 1)
			{
				ValueStr += TCHAR('\n');
			}
			ValueStr += FString::Printf(TEXT("[%i] "), i);
		}

		uint8* PropData = (uint8*)PropertyValue + i * Inner->ElementSize;

		// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
		uint8* PropDefault = nullptr;
		if (bExternalEditor)
		{
			PropDefault = PropData;
		}
		else if (StructProperty)
		{
			PropDefault = StructDefaults;
		}
		else
		{
			if (DefaultValue && DefaultSize > i)
			{
				PropDefault = (uint8*)DefaultValue + i * Inner->ElementSize;
			}
		}

		Inner->ExportTextItem_Direct( ValueStr, PropData, PropDefault, Parent, PortFlags|PPF_Delimited, ExportRootScope );
	}

	if ((Count > 0) && !bReadableForm)
	{
		ValueStr += TEXT(")");
	}
	if (StructDefaults)
	{
		StructProperty->DestroyValue(StructDefaults);
		FMemory::Free(StructDefaults);
	}
}

const TCHAR* FArrayProperty::ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* OwnerObject, int32 PortFlags, FOutputDevice* ErrorText) const
{
	uint8* TempArrayStorage = nullptr;
	ON_SCOPE_EXIT
	{
		if (TempArrayStorage)
		{
			// TempArrayStorage is used by property setter so if it was allocated call the setter now
			FProperty::SetValue_InContainer(ContainerOrPropertyPtr, TempArrayStorage);

			// Destroy and free the temp array used by property setter
			DestroyAndFreeValue(TempArrayStorage);
		}
	};

	void* ArrayPtr = nullptr;
	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		// Allocate temporary map as we first need to initialize it with the parsed items and then use the setter to update the property
		TempArrayStorage = (uint8*)AllocateAndInitializeValue();
		ArrayPtr = TempArrayStorage;
	}
	else
	{
		ArrayPtr = PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType);
	}

	FScriptArrayHelper ArrayHelper(this, ArrayPtr);
	return ImportTextInnerItem(Buffer, Inner, ArrayPtr, PortFlags, OwnerObject, &ArrayHelper, ErrorText);
}

const TCHAR* FArrayProperty::ImportTextInnerItem( const TCHAR* Buffer, const FProperty* Inner, void* Data, int32 PortFlags, UObject* Parent, FScriptArrayHelper* ArrayHelper, FOutputDevice* ErrorText )
{
	checkSlow(Inner);

	// If we export an empty array we export an empty string, so ensure that if we're passed an empty string
	// we interpret it as an empty array.
	if (*Buffer == TCHAR('\0') || *Buffer == TCHAR(')') || *Buffer == TCHAR(','))
	{
		if (ArrayHelper)
		{
			ArrayHelper->EmptyValues();
		}
		return Buffer;
	}

	if ( *Buffer++ != TCHAR('(') )
	{
		return NULL;
	}

	if (ArrayHelper)
	{
		ArrayHelper->EmptyValues();
		ArrayHelper->ExpandForIndex(0);
	}

	SkipWhitespace(Buffer);

	if (*Buffer == TCHAR(')'))
	{
		// We didn't find any items. Clear the array again
		if (ArrayHelper)
		{
			ArrayHelper->EmptyValues();
		}
		return Buffer;
	}

	int32 Index = 0;
	while (*Buffer != TCHAR(')'))
	{
		SkipWhitespace(Buffer);

		if (*Buffer != TCHAR(','))
		{
			uint8* Address = ArrayHelper ? ArrayHelper->GetRawPtr(Index) : ((uint8*)Data + Inner->ElementSize * Index);
			// Parse the item
			checkf(ArrayHelper == nullptr || Inner->GetOffset_ForInternal() == 0, TEXT("Expected the Inner property of the FArrayProperty."));
			Buffer = Inner->ImportText_Direct(Buffer, Address, Parent, PortFlags | PPF_Delimited, ErrorText);

			if(!Buffer)
			{
				return NULL;
			}

			SkipWhitespace(Buffer);
		}


		if (*Buffer == TCHAR(','))
		{
			Buffer++;
			Index++;
			if (ArrayHelper)
			{
				ArrayHelper->ExpandForIndex(Index);
			}
			else if (Index >= Inner->ArrayDim)
			{
				ErrorText->Logf(ELogVerbosity::Warning, TEXT("%s is a fixed-sized array of %i values. Additional data after %i has been ignored during import."), *Inner->GetName(), Inner->ArrayDim, Inner->ArrayDim);
				break;
			}
		}
		else
		{
			break;
		}
	}

	// Make sure we ended on a )
	if (*Buffer++ != TCHAR(')'))
	{
		return NULL;
	}

	return Buffer;
}

#if WITH_EDITORONLY_DATA
void FArrayProperty::AppendSchemaHash(FBlake3& Builder, bool bSkipEditorOnly) const
{
	Super::AppendSchemaHash(Builder, bSkipEditorOnly);
	if (Inner)
	{
		Inner->AppendSchemaHash(Builder, bSkipEditorOnly);
	}
}
#endif

void FArrayProperty::AddCppProperty(FProperty* Property)
{
	check(!Inner);
	check(Property);

	Inner = Property;
}

void FArrayProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(Count==1); // this was never supported, apparently
	FScriptArrayHelper SrcArrayHelper(this, Src);
	FScriptArrayHelper DestArrayHelper(this, Dest);

	int32 Num = SrcArrayHelper.Num();
	if ( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
	{
		DestArrayHelper.EmptyAndAddValues(Num);
	}
	else
	{
		DestArrayHelper.EmptyAndAddUninitializedValues(Num);
	}
	if (Num)
	{
		size_t Size = Inner->ElementSize;
		uint8* SrcData = (uint8*)SrcArrayHelper.GetRawPtr();
		uint8* DestData = (uint8*)DestArrayHelper.GetRawPtr();
		if( !(Inner->PropertyFlags & CPF_IsPlainOldData) )
		{
			for( int32 i=0; i<Num; i++ )
			{
				Inner->CopyCompleteValue( DestData + i * Size, SrcData + i * Size );
			}
		}
		else
		{
			FMemory::Memcpy( DestData, SrcData, Num*Size );
		}
	}
}
void FArrayProperty::ClearValueInternal( void* Data ) const
{
	FScriptArrayHelper ArrayHelper(this, Data);
	ArrayHelper.EmptyValues();
}
void FArrayProperty::DestroyValueInternal( void* Dest ) const
{
	FScriptArrayHelper ArrayHelper(this, Dest);
	ArrayHelper.EmptyValues();

	//@todo UE potential double destroy later from this...would be ok for a script array, but still
	ArrayHelper.DestroyContainer_Unsafe();
}
bool FArrayProperty::PassCPPArgsByRef() const
{
	return true;
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void FArrayProperty::InstanceSubobjects( void* Data, void const* DefaultData, UObject* InOwner, FObjectInstancingGraph* InstanceGraph )
{
	if( Data && Inner->ContainsInstancedObjectProperty())
	{
		FScriptArrayHelper ArrayHelper(this, Data);
		FScriptArrayHelper DefaultArrayHelper(this, DefaultData);

		int32 InnerElementSize = Inner->ElementSize;
		void* TempElement = FMemory_Alloca(InnerElementSize);

		for( int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ElementIndex++ )
		{
			uint8* DefaultValue = (DefaultData && ElementIndex < DefaultArrayHelper.Num()) ? DefaultArrayHelper.GetRawPtr(ElementIndex) : nullptr;
			FMemory::Memmove(TempElement, ArrayHelper.GetRawPtr(ElementIndex), InnerElementSize);
			Inner->InstanceSubobjects( TempElement, DefaultValue, InOwner, InstanceGraph );
			if (ElementIndex < ArrayHelper.Num())
			{
				FMemory::Memmove(ArrayHelper.GetRawPtr(ElementIndex), TempElement, InnerElementSize);
			}
			else
			{
				Inner->DestroyValue(TempElement);
			}
		}
	}
}

bool FArrayProperty::SameType(const FProperty* Other) const
{
	return Super::SameType(Other) && Inner && Inner->SameType(((FArrayProperty*)Other)->Inner);
}

EConvertFromTypeResult FArrayProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, const uint8* Defaults)
{
	// TODO: The ArrayProperty Tag really doesn't have adequate information for
	// many types. This should probably all be moved in to ::SerializeItem

	if (Tag.Type == NAME_ArrayProperty && Tag.InnerType != NAME_None && Tag.InnerType != Inner->GetID())
	{
		void* ArrayPropertyData = ContainerPtrToValuePtr<void>(Data);

		int32 ElementCount = 0;

		if (Slot.GetUnderlyingArchive().IsTextFormat())
		{
			Slot.EnterArray(ElementCount);
		}
		else
		{
			Slot.GetUnderlyingArchive() << ElementCount;
		}

		FScriptArrayHelper ScriptArrayHelper(this, ArrayPropertyData);
		ScriptArrayHelper.EmptyAndAddValues(ElementCount);

		FPropertyTag InnerPropertyTag;
		InnerPropertyTag.Type = Tag.InnerType;
		InnerPropertyTag.ArrayIndex = 0;

		if (Slot.GetArchiveState().UEVer() >= VER_UE4_INNER_ARRAY_TAG_INFO && Tag.InnerType == NAME_StructProperty)
		{
			Slot.GetUnderlyingArchive() << InnerPropertyTag;
		}

		// Convert properties from old type to new type automatically if types are compatible (array case)
		if (ElementCount > 0)
		{
			FStructuredArchive::FStream ValueStream = Slot.EnterStream();

			EConvertFromTypeResult ConvertResult = Inner->ConvertFromType(InnerPropertyTag, ValueStream.EnterElement(), ScriptArrayHelper.GetRawPtr(0), DefaultsStruct, nullptr);
			if (ConvertResult == EConvertFromTypeResult::Converted || ConvertResult == EConvertFromTypeResult::Serialized)
			{
				for (int32 i = 1; i < ElementCount; ++i)
				{
					ConvertResult = Inner->ConvertFromType(InnerPropertyTag, ValueStream.EnterElement(), ScriptArrayHelper.GetRawPtr(i), DefaultsStruct, nullptr);
					check(ConvertResult == EConvertFromTypeResult::Converted || ConvertResult == EConvertFromTypeResult::Serialized);
				}

				return EConvertFromTypeResult::Converted;
			}
			// TODO: Implement SerializeFromMismatchedTag handling for arrays of structs
			else
			{
				UE_LOG(LogClass, Warning, TEXT("Array Inner Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.InnerType.ToString(), *Inner->GetID().ToString(), *Slot.GetUnderlyingArchive().GetArchiveName() );
				return EConvertFromTypeResult::CannotConvert;
			}
		}
		else
		{
			return EConvertFromTypeResult::Converted;
		}
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

FField* FArrayProperty::GetInnerFieldByName(const FName& InName)
{
	if (Inner && Inner->GetFName() == InName)
	{
		return Inner;
	}
	return nullptr;
}

void FArrayProperty::GetInnerFields(TArray<FField*>& OutFields)
{
	if (Inner)
	{
		OutFields.Add(Inner);
		Inner->GetInnerFields(OutFields);
	}
}

void* FArrayProperty::GetValueAddressAtIndex_Direct(const FProperty* InInner, void* InValueAddress, int32 Index) const
{
	FScriptArrayHelper ArrayHelper(this, InValueAddress);
	checkf(Inner == InInner, TEXT("Passed in inner must be identical to the array property inner property"));
	if (Index < ArrayHelper.Num() && Index >= 0)
	{
		return ArrayHelper.GetRawPtr(Index);
	}
	else
	{
		return nullptr;
	}
}

bool FArrayProperty::LoadFromTag(const FPropertyTag& Tag)
{
	if (!Super::LoadFromTag(Tag))
	{
		return false;
	}

	FField* Field = FField::Construct(Tag.InnerType, {}, Tag.Name, RF_NoFlags);
	if (FProperty* Property = CastField<FProperty>(Field))
	{
		FPropertyTag InnerTag = Tag;
		InnerTag.Type = Tag.InnerType;
		InnerTag.InnerType = {};
		// Skip property types that are missing the name of the inner type.
		// Structs have their name in a tag in the serialized data, but we cannot
		// proceed safely unless we know if the struct used native serialization.
		if (/*!Property->IsA<FStructProperty>() &&*/
			!Property->IsA<FByteProperty>() &&
			!Property->IsA<FEnumProperty>() &&
			Property->LoadFromTag(InnerTag))
		{
			Inner = Property;
			return true;
		}
	}
	delete Field;
	return false;
}

void FArrayProperty::SaveToTag(FPropertyTag& Tag)
{
	Super::SaveToTag(Tag);

	const FProperty* LocalInner = Inner;
	check(LocalInner);
	Tag.InnerType = LocalInner->GetID();
}
