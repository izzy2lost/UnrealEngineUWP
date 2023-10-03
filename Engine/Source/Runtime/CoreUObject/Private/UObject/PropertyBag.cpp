// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyBag.h"

#include "Serialization/NullArchive.h"
#include "UObject/UnrealType.h"

namespace UE
{

FPropertyBag::FPropertyBag() = default;
FPropertyBag::~FPropertyBag() = default;

FPropertyBag::FPropertyBag(FPropertyBag&&) = default;
FPropertyBag& FPropertyBag::operator=(FPropertyBag&&) = default;

void FPropertyBag::Empty()
{
	Properties.Empty();
}

void FPropertyBag::Copy(const UStruct* Struct, void* Data, EPropertyFlags SkipFlags)
{
	EFieldIterationFlags IterationFlags = EFieldIterationFlags::IncludeAll;
	if (SkipFlags & CPF_Deprecated)
	{
		IterationFlags &= ~EFieldIterationFlags::IncludeDeprecated;
	}

	for (TFieldIterator<FProperty> It(Struct, IterationFlags); It; ++It)
	{
		if (FProperty* Property = *It; !(Property->GetPropertyFlags() & SkipFlags))
		{
			for (int32 ArrayIndex = 0, ArrayDim = Property->ArrayDim; ArrayIndex < ArrayDim; ++ArrayIndex)
			{
				Add(Property, Data, ArrayIndex);
			}
		}
	}
}

void FPropertyBag::Add(FProperty* Property, void* Data, int32 ArrayIndex)
{
	// TODO: NullAr is a workaround to FPropertyTag requiring an archive to assert on versioned property serialization.
	FNullArchive NullAr;

	FPropertyBag::FValue& Value = Properties.FindOrAdd(Property->GetFName());

	const bool bPropertyChanged = Value.Tag.Prop != Property;

	if (bPropertyChanged)
	{
		Value.Destroy();
		Value.Tag = FPropertyTag(NullAr, Property, INDEX_NONE, (uint8*)Data, nullptr);
	}

	Value.EnsurePropertyData(ArrayIndex);

	// Subtract the offset because the property will re-add it.
	void* TargetBase = (uint8*)Value.Data.Memory - Property->GetOffset_ForInternal();

	void* Target = Property->ContainerPtrToValuePtr<void>(TargetBase, ArrayIndex);
	void* Source = Property->ContainerPtrToValuePtr<void>(Data, ArrayIndex);
	Property->CopySingleValue(Target, Source);
}

void FPropertyBag::Remove(FName Name)
{
	Properties.Remove(Name);
}

void FPropertyBag::LoadPropertyByTag(const FPropertyTag& Tag, FStructuredArchiveSlot& ValueSlot, const void* Defaults)
{
	FArchive& UnderlyingArchive = ValueSlot.GetUnderlyingArchive();

	FPropertyBag::FValue& Value = Properties.FindOrAdd(Tag.Name);

	const bool bPropertyChanged =
		(Value.Tag.Prop != Tag.Prop && Value.Tag.Prop && Tag.Prop) ||
		Value.Tag.Type != Tag.Type ||
		Value.Tag.Name != Tag.Name ||
		Value.Tag.StructName != Tag.StructName ||
		Value.Tag.EnumName != Tag.EnumName ||
		Value.Tag.InnerType != Tag.InnerType ||
		Value.Tag.ValueType != Tag.ValueType ||
		Value.Tag.StructGuid != Tag.StructGuid ||
		Value.Tag.PropertyGuid != Tag.PropertyGuid;

	if (bPropertyChanged)
	{
		Value.Destroy();
		Value.Tag = Tag;
		Value.Tag.Size = 0;
		Value.Tag.ArrayIndex = INDEX_NONE;
	}

	if (FProperty* Property = Value.Tag.Prop)
	{
		Value.EnsurePropertyData(Tag.ArrayIndex);

		// Subtract the offset because the property will re-add it.
		void* TargetBase = (uint8*)Value.Data.Memory - Property->GetOffset_ForInternal();

		void* Target = Property->ContainerPtrToValuePtr<void>(TargetBase, Tag.ArrayIndex);
		Tag.SerializeTaggedProperty(ValueSlot, Property, (uint8*)Target, (const uint8*)Defaults);

		return;
	}

	const FValue::FData Data = {FMemory::Malloc(Tag.Size), Tag.Size};
	UnderlyingArchive.Serialize(Data.Memory, Data.Size);

	if (LIKELY(Tag.ArrayIndex == 0))
	{
		Value.Data = Data;
	}
	else
	{
		Value.EnsureArrayData(Tag.ArrayIndex);

		FBitReference AllocationFlag = (*Value.ArrayAllocationFlags)[Tag.ArrayIndex];
		if (UNLIKELY(AllocationFlag))
		{
			FMemory::Free((*Value.ArrayData)[Tag.ArrayIndex].Memory);
		}
		else
		{
			AllocationFlag = true;
		}
		Value.ArrayData->Add(Tag.ArrayIndex, Data);
	}
}

void FPropertyBag::AssignElement(FPropertyBagElement& Element, const FName& Name, const FValue& Value)
{
	FProperty* Property = Value.Tag.Prop;
	Element.Name = Name;
	Element.Property = Property;

	if (Property)
	{
		Element.ArrayIndex = 0;
		Element.ArrayDim = Property->ArrayDim;
		Element.Value = (uint8*)Value.Data.Memory;
	}
	else if (Value.ArrayData)
	{
		Element.ArrayIndex = Value.ArrayAllocationFlags->Find(/*bValue*/ true);
		Element.ArrayDim = Value.ArrayAllocationFlags->Num();
		Element.Value = (*Value.ArrayData)[Element.ArrayIndex].Memory;
	}
	else
	{
		Element.ArrayIndex = 0;
		Element.ArrayDim = 1;
		Element.Value = Value.Data.Memory;
	}
}

bool FPropertyBag::AdvanceElement(FPropertyBagElement& Element, const FValue& Value)
{
	if (const FProperty* Property = Element.Property)
	{
		Element.Value = (uint8*)Element.Value + Property->ElementSize;
		return true;
	}

	Element.ArrayIndex = Value.ArrayAllocationFlags->FindFrom(/*bValue*/ true, Element.ArrayIndex);
	if (Element.ArrayIndex >= 0)
	{
		Element.Value = (*Value.ArrayData)[Element.ArrayIndex].Memory;
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FPropertyBag::FValue::~FValue()
{
	Destroy();
}

void FPropertyBag::FValue::Destroy()
{
	const auto DestroyValue = [Property = Tag.Prop](const FData& D)
	{
		if (Property)
		{
			Property->DestroyAndFreeValue(D.Memory);
		}
		else
		{
			FMemory::Free(D.Memory);
		}
	};

	if (Data.Memory)
	{
		DestroyValue(Data);
		Data = {};
	}

	if (ArrayData)
	{
		for (TTuple<int32, FData>& IndexAndData : *ArrayData)
		{
			DestroyValue(IndexAndData.Value);
		}
		delete ArrayData;
		ArrayData = nullptr;
	}

	delete ArrayAllocationFlags;
	ArrayAllocationFlags = nullptr;
}

void FPropertyBag::FValue::EnsurePropertyData(int32 ArrayIndex)
{
	const FProperty* const Property = Tag.Prop;
	const int32 ArrayDim = Property->ArrayDim;

	if (!Data.Memory)
	{
		Data.Memory = Property->AllocateAndInitializeValue();
		Data.Size = Property->GetSize();

		if (ArrayDim > 1)
		{
			ArrayAllocationFlags = new TBitArray<>(/*bValue*/ false, ArrayDim);
		}
	}

	if (ArrayDim > 1)
	{
		(*ArrayAllocationFlags)[ArrayIndex] = true;
	}
}

void FPropertyBag::FValue::EnsureArrayData(int32 ArrayIndex)
{
	if (ArrayAllocationFlags)
	{
		ArrayAllocationFlags->PadToNum(ArrayIndex + 1, /*bValue*/ false);
		return;
	}

	ArrayData = new TMap<int32, FValue::FData>();
	ArrayAllocationFlags = new TBitArray<>(/*bValue*/ false, ArrayIndex + 1);

	// Move the scalar value into array storage as the first element.
	if (Data.Memory)
	{
		(*ArrayAllocationFlags)[0] = true;
		ArrayData->Add(0, Data);
		Data = {};
	}
}

} // UE
