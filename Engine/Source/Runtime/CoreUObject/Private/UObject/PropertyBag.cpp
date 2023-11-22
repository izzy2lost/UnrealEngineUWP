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
	// TODO: NullAr is a workaround to FPropertyTag requiring an archive to assert on versioned property serialization.
	FNullArchive NullAr;

	FValue& Value = FindOrCreateValue(Path);

	const bool bPropertyChanged = Value.Tag.Prop != Property;

	if (bPropertyChanged)
	{
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
			if (FNodeMap* NodeMap = (*Node)->ValueOrNodes.TryGet<FNodeMap>())
			{
				Nodes.Add(NodeMap);
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

	Value.AllocateAndInitializeValue();

	if (FProperty* Property = Value.Tag.Prop)
	{
		Tag.SerializeTaggedProperty(ValueSlot, Property, (uint8*)Value.Data, (const uint8*)Defaults);
	}
	else
	{
		UnderlyingArchive.Serialize(Value.Data, Value.Tag.Size);
	}
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
		if (!Node->ValueOrNodes.IsType<FNodeMap>())
		{
			Node->ValueOrNodes.Emplace<FNodeMap>();
		}
		Node->Type = Segment.Type;
		NodeMap = &Node->ValueOrNodes.Get<FNodeMap>();
	}

	FPropertyPathNameSegment LastSegment = Path.GetSegment(SegmentCount - 1);
	TUniquePtr<FNode>& Node = NodeMap->FindOrAdd(LastSegment.PackNameWithIndex());
	if (!Node)
	{
		Node = MakeUnique<FNode>();
	}
	if (!Node->ValueOrNodes.IsType<FValue>())
	{
		Node->ValueOrNodes.Emplace<FValue>();
	}
	Node->Type = LastSegment.Type;
	return Node->ValueOrNodes.Get<FValue>();
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
	if (Data)
	{
		if (const FProperty* Property = Tag.Prop)
		{
			// TODO: Need to destroy and free only one element for arrays.
			Property->DestroyAndFreeValue(Data);
		}
		else
		{
			FMemory::Free(Data);
		}
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

FPropertyBag::FConstIterator::FConstIterator(const FNodeIterator& NodeIt)
{
	NodeIterators.Push(NodeIt);
	EnterNode();
}

FPropertyBag::FConstIterator& FPropertyBag::FConstIterator::operator++()
{
	++NodeIterators.Last();
	EnterNode();
	return *this;
}

inline void FPropertyBag::FConstIterator::EnterNode()
{
	for (;;)
	{
		if (CurrentPath.GetSegmentCount() == NodeIterators.Num())
		{
			CurrentPath.Pop();
		}

		if (FNodeIterator& NodeIt = NodeIterators.Last())
		{
			const TUniquePtr<FNode>& Node = NodeIt.Value();
			CurrentPath.Push(FPropertyPathNameSegment().SetNameWithIndex(NodeIt.Key()).SetType(Node->Type));

			if (FValue* Value = Node->ValueOrNodes.TryGet<FValue>())
			{
				CurrentValue = Value;
				return;
			}
			else if (const FNodeMap* NodeMap = Node->ValueOrNodes.TryGet<FNodeMap>())
			{
				NodeIterators.Push(NodeMap->CreateConstIterator());
			}
			else
			{
				checkNoEntry();
			}
		}
		else if (NodeIterators.Num() > 1)
		{
			NodeIterators.Pop();
			++NodeIterators.Last();
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
