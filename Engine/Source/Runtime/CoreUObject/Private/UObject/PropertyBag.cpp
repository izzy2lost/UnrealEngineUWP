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

void FPropertyBag::Add(const FPropertyPathName& Path, FProperty* Property, void* Data, int32 ArrayIndex)
{
	FValue& Value = FindOrCreateValue(Path);

	const bool bPropertyChanged = Value.Tag.Prop != Property;

	if (bPropertyChanged)
	{
		// TODO: NullAr is a workaround to FPropertyTag requiring an archive to assert on versioned property serialization.
		FNullArchive NullAr;

		Value.Destroy();
		Value.Tag = FPropertyTag(NullAr, Property, INDEX_NONE, (uint8*)Data, nullptr);
	}

	Value.AllocateAndInitializeValue();

	// Subtract the offset because the property will re-add it.
	void* TargetBase = (uint8*)Value.Data - Property->GetOffset_ForInternal();

	void* Target = Property->ContainerPtrToValuePtr<void>(TargetBase);
	void* Source = Property->ContainerPtrToValuePtr<void>(Data, ArrayIndex);
	Property->CopySingleValue(Target, Source);
}

void FPropertyBag::Remove(const FPropertyPathName& Path)
{
	TArray<FNodeMap*, TInlineAllocator<8>> Nodes;
	Nodes.Add(&Properties);

	// Find the chain of nodes to the leaf property.
	for (int32 SegmentIndex = 0, SegmentCount = Path.GetSegmentCount() - 1; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const FPropertyPathNameSegment Segment = Path.GetSegment(SegmentIndex);
		if (TUniquePtr<FNode>* Node = Nodes[SegmentIndex]->Find(Segment.PackNameWithIndex()))
		{
			if (FNodeMap& NodeMap = (*Node)->Nodes; !NodeMap.IsEmpty())
			{
				Nodes.Add(&NodeMap);
				continue;
			}
		}
		return;
	}

	// Remove the leaf property and any empty nodes in the tail of the property chain.
	for (int32 SegmentIndex = Path.GetSegmentCount() - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		const FPropertyPathNameSegment Segment = Path.GetSegment(SegmentIndex);
		FNodeMap* SegmentNodes = Nodes[SegmentIndex];
		if (SegmentNodes->Remove(Segment.PackNameWithIndex()) == 0 || !SegmentNodes->IsEmpty())
		{
			return;
		}
	}
}

void FPropertyBag::LoadPropertyByTag(const FPropertyPathName& Path, const FPropertyTag& Tag, FStructuredArchiveSlot& ValueSlot, const void* Defaults)
{
	FArchive& UnderlyingArchive = ValueSlot.GetUnderlyingArchive();

	FValue& Value = FindOrCreateValue(Path);

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
		Value.Tag.ArrayIndex = INDEX_NONE;
	}

	// Serialize the value using the existing property from the tag.
	if (FProperty* Property = Value.Tag.Prop)
	{
		Value.AllocateAndInitializeValue();
		Tag.SerializeTaggedProperty(ValueSlot, Property, (uint8*)Value.Data, (const uint8*)Defaults);
		return;
	}

	// Construct a property from the tag and try to use it to serialize the value.
	FField* Field = FField::Construct(Tag.Type, {}, Tag.Name, RF_NoFlags);
	if (FProperty* Property = CastField<FProperty>(Field); Property && Property->LoadFromTag(Value.Tag))
	{
		Property->Link(UnderlyingArchive);
		Value.bOwnsProperty = true;
		Value.Tag.Prop = Property;
		Value.AllocateAndInitializeValue();
		Tag.SerializeTaggedProperty(ValueSlot, Property, (uint8*)Value.Data, nullptr);
		return;
	}
	delete Field;

	// Fall back to loading the serialized value.
	// Persisting this serialized value will require capturing version information from the archive.
	Value.AllocateAndInitializeValue();
	UnderlyingArchive.Serialize(Value.Data, Value.Tag.Size);
}

FPropertyBag::FValue& FPropertyBag::FindOrCreateValue(const FPropertyPathName& Path)
{
	const int32 SegmentCount = Path.GetSegmentCount();

	FNodeMap* NodeMap = &Properties;
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount - 1; ++SegmentIndex)
	{
		FPropertyPathNameSegment Segment = Path.GetSegment(SegmentIndex);
		TUniquePtr<FNode>& Node = NodeMap->FindOrAdd(Segment.PackNameWithIndex());
		if (!Node)
		{
			Node = MakeUnique<FNode>();
		}
		Node->Type = Segment.Type;
		NodeMap = &Node->Nodes;
	}

	FPropertyPathNameSegment LastSegment = Path.GetSegment(SegmentCount - 1);
	TUniquePtr<FNode>& Node = NodeMap->FindOrAdd(LastSegment.PackNameWithIndex());
	if (!Node)
	{
		Node = MakeUnique<FNode>();
	}
	Node->Type = LastSegment.Type;
	return Node->Value;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FPropertyBag::FValue::~FValue()
{
	Destroy();
}

void FPropertyBag::FValue::AllocateAndInitializeValue()
{
	if (Data)
	{
		return;
	}

	if (const FProperty* Property = Tag.Prop)
	{
		// TODO: Need to allocate only one element for arrays.
		Data = Property->AllocateAndInitializeValue();
	}
	else
	{
		Data = FMemory::Malloc(Tag.Size);
	}
}

void FPropertyBag::FValue::Destroy()
{
	if (const FProperty* Property = Tag.Prop)
	{
		if (Data)
		{
			// TODO: Need to destroy and free only one element for arrays.
			Property->DestroyAndFreeValue(Data);
			Data = nullptr;
		}

		if (bOwnsProperty)
		{
			bOwnsProperty = false;
			delete Property;
			Tag.Prop = nullptr;
		}
	}
	else
	{
		FMemory::Free(Data);
		Data = nullptr;
	}
}

int32 FPropertyBag::FValue::GetSize() const
{
	if (!Data)
	{
		return 0;
	}
	if (!Tag.Prop)
	{
		return Tag.Size;
	}
	// TODO: Need to include the size of only one element for arrays.
	return Tag.Prop->GetSize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FPropertyBag::FConstIterator::EnterNode()
{
	for (;;)
	{
		if (CurrentPath.GetSegmentCount() == NodeIterators.Num())
		{
			CurrentPath.Pop();
		}

		if (FNodeIterator& NodeIt = NodeIterators.Last())
		{
			FNode* Node = NodeIt.Value().Get();
			CurrentPath.Push(FPropertyPathNameSegment().SetNameWithIndex(NodeIt.Key()).SetType(Node->Type));
			++NodeIt;

			if (!Node->Nodes.IsEmpty())
			{
				NodeIterators.Push(Node->Nodes.CreateConstIterator());
			}

			if (Node->Value)
			{
				CurrentValue = &Node->Value;
				return;
			}
		}
		else if (NodeIterators.Num() > 1)
		{
			NodeIterators.Pop();
		}
		else
		{
			// End of the root node.
			CurrentValue = nullptr;
			return;
		}
	}
}

} // UE
