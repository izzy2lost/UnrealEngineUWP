// Copyright Epic Games, Inc. All Rights Reserved.


#include "ArchetypeFixupPanel.h"

#include "AsyncDetailViewDiff.h"
#include "DetailTreeNode.h"
#include "Widgets/Layout/LinkableScrollBar.h"
#include "ArchetypeFixupDetailCustomization.h"
#include "UObject/ArchetypeUtils.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "ArchetypeFixupPanel"

FRedirectedPropertyNode::FRedirectedPropertyNode(const FRedirectedPropertyNode& Other)
	: PropertyName(Other.PropertyName)
	, Type(Other.Type)
	, ArrayIndex(Other.ArrayIndex)
{
	// deep copy tree
	for (const TSharedPtr<FRedirectedPropertyNode>& Child : Other.Children)
	{
		Children.Add(MakeShared<FRedirectedPropertyNode>(*Child));
	}
}

FRedirectedPropertyNode::FRedirectedPropertyNode(const FPropertyInfo& InInfo, const TWeakPtr<FRedirectedPropertyNode>& InParent)
	: PropertyName(InInfo.Property->GetFName())
	, Type(InInfo.Property->GetID())
	, ArrayIndex(InInfo.ArrayIndex)
	, Parent(InParent)
{
}

FRedirectedPropertyNode::FRedirectedPropertyNode(FName InPropertyName, FName InType, int32 InArrayIndex, const TWeakPtr<FRedirectedPropertyNode>& InParent)
	: PropertyName(InPropertyName)
	, Type(InType)
	, ArrayIndex(InArrayIndex)
	, Parent(InParent)
{
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(const FPropertyPath& Path, int32 PathIndex)
{
	check(PathIndex <= Path.GetNumProperties());
	if (PathIndex == Path.GetNumProperties())
	{
		return SharedThis(this);
	}
	
	const FPropertyInfo& ChildInfo = Path.GetPropertyInfo(PathIndex);
	const TSharedPtr<FRedirectedPropertyNode> Child = FindOrAdd(ChildInfo);
	return Child->FindOrAdd(Path, PathIndex + 1);
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(const FPropertyInfo& ChildInfo)
{
	TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildInfo);
	if (!Child)
	{
		Child = MakeShared<FRedirectedPropertyNode>(ChildInfo, SharedThis(this));
		Children.Add(Child);
	}
	return Child;
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::FindOrAdd(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex)
{
	TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildPropertyName, ChildType, ChildArrayIndex);
	if (!Child)
	{
		Child = MakeShared<FRedirectedPropertyNode>(ChildPropertyName, ChildType, ChildArrayIndex, SharedThis(this));
		Children.Add(Child);
	}
	return Child;
}

bool FRedirectedPropertyNode::Remove(const FPropertyPath& Path, int32 PathIndex)
{
	if (TSharedPtr<FRedirectedPropertyNode> NodeToRemove = Find(Path, PathIndex))
	{
		do
		{
			const TSharedPtr<FRedirectedPropertyNode> ParentNode = NodeToRemove->Parent.Pin();
			if (ParentNode)
			{
				ParentNode->Remove(NodeToRemove->PropertyName, NodeToRemove->Type, NodeToRemove->ArrayIndex);
			}
			NodeToRemove = ParentNode;
		} while (NodeToRemove && NodeToRemove->Children.IsEmpty());
		return true;
	}
	return false;
}

bool FRedirectedPropertyNode::Remove(const FPropertyInfo& ChildInfo)
{
	const int32 Index = FindIndex(ChildInfo);
	if (Index != INDEX_NONE)
	{
		Children.RemoveAt(Index);
		return true;
	}
	return false;
}

bool FRedirectedPropertyNode::Remove(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex)
{
	const int32 Index = FindIndex(ChildPropertyName, ChildType, ChildArrayIndex);
	if (Index != INDEX_NONE)
	{
		Children.RemoveAt(Index);
		return true;
	}
	return false;
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(const FPropertyPath& Path, int32 PathIndex) const
{
	check(PathIndex <= Path.GetNumProperties());
	if (PathIndex == Path.GetNumProperties())
	{
		return SharedThis(const_cast<FRedirectedPropertyNode*>(this));
	}
	
	const FPropertyInfo& ChildInfo = Path.GetPropertyInfo(PathIndex);
	if (const TSharedPtr<FRedirectedPropertyNode> Child = Find(ChildInfo))
	{
		return Child->Find(Path, PathIndex + 1);
	}
	return {};
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(const FPropertyInfo& ChildInfo) const
{
	const int32 Index = FindIndex(ChildInfo);
	if (Index != INDEX_NONE)
	{
		return Children[Index];
	}
	return {};
}

TSharedPtr<FRedirectedPropertyNode> FRedirectedPropertyNode::Find(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex) const
{
	const int32 Index = FindIndex(ChildPropertyName, ChildType, ChildArrayIndex);
	if (Index != INDEX_NONE)
	{
		return Children[Index];
	}
	return {};
}

bool FRedirectedPropertyNode::Move(const FPropertyPath& FromPath, const FPropertyPath& ToPath)
{
	if (TSharedPtr<FRedirectedPropertyNode> NodeToMove = Find(FromPath))
	{
		const TSharedPtr<FRedirectedPropertyNode> Added = FindOrAdd(ToPath);

		// reparent children
		Added->Children = MoveTemp(NodeToMove->Children);
		for (const TSharedPtr<FRedirectedPropertyNode>& Child : Added->Children)
		{
			Child->Parent = Added;
		}

		do
		{
			const TSharedPtr<FRedirectedPropertyNode> ParentNode = NodeToMove->Parent.Pin();
			if (ParentNode)
			{
				ParentNode->Remove(NodeToMove->PropertyName, NodeToMove->Type, NodeToMove->ArrayIndex);
			}
			NodeToMove = ParentNode;
		} while (NodeToMove && NodeToMove->Children.IsEmpty());
		return true;
	}
	return false;
}

int32 FRedirectedPropertyNode::FindIndex(const FPropertyInfo& ChildInfo) const
{
	return FindIndex(ChildInfo.Property->GetFName(), ChildInfo.Property->GetID(), ChildInfo.ArrayIndex);
}

int32 FRedirectedPropertyNode::FindIndex(FName ChildPropertyName, FName ChildType, int32 ChildArrayIndex) const
{
	return Children.IndexOfByPredicate([ChildPropertyName, ChildType, ChildArrayIndex](const TSharedPtr<FRedirectedPropertyNode>& Child)
	{
		if (Child->ArrayIndex != INDEX_NONE)
		{
			// a matching index will always match regardless of type and name
			return Child->ArrayIndex == ChildArrayIndex;
		}
		return Child->Type == ChildType && Child->PropertyName == ChildPropertyName;
	});
}

FArchetypeFixupPanel::FArchetypeFixupPanel(TConstArrayView<TObjectPtr<UObject>> Archetypes, EViewFlags InViewFlags)
	: Instances(Archetypes)
	, RedirectedPropertyTree(MakeShared<FRedirectedPropertyNode>())
	, ViewFlags(InViewFlags)
{
	InitRedirectedPropertyTree();
}

int32 FArchetypeFixupPanel::Find(UObject* Value) const
{
	return Instances.Find(Value);
}

TSharedPtr<IDetailsView>& FArchetypeFixupPanel::GenerateDetailsView(bool bScrollbarOnLeft)
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.ExternalScrollbar = SAssignNew(LinkableScrollBar, SLinkableScrollBar);
	DetailsViewArgs.ScrollbarAlignment = bScrollbarOnLeft ? HAlign_Left : HAlign_Right;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	for (const UObject* Object : Instances)
	{
		if (HasViewFlag(EViewFlags::HideLooseProperties))
		{
			DetailsView->RegisterInstancedCustomPropertyLayout(Object->GetClass(), FOnGetDetailCustomizationInstance::CreateLambda([]()
			{
				return MakeShared<FHideLoosePropertiesCustomization>();
			}));
		}
		else if (HasViewFlag(EViewFlags::AllowRemapLooseProperties))
		{
			DetailsView->RegisterInstancedCustomPropertyLayout(Object->GetClass(), FOnGetDetailCustomizationInstance::CreateLambda([DiffPanel = SharedThis(this)]()
			{
				return MakeShared<FArchetypeFixupDetailCustomization>(DiffPanel);
			}));
		}
	}
	
	DetailsView->SetObjects(Instances, true);
	return DetailsView;
}

void FArchetypeFixupPanel::SetDiffAgainstLeft(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstLeft)
{
	DiffAgainstLeft = InDiffAgainstLeft;
}

void FArchetypeFixupPanel::SetDiffAgainstRight(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstRight)
{
	DiffAgainstRight = InDiffAgainstRight;
}

TSharedPtr<FAsyncDetailViewDiff> FArchetypeFixupPanel::GetDiffAgainstLeft() const
{
	return DiffAgainstLeft.Pin();
}

TSharedPtr<FAsyncDetailViewDiff> FArchetypeFixupPanel::GetDiffAgainstRight() const
{
	return DiffAgainstRight.Pin();
}

bool FArchetypeFixupPanel::ShouldSplitterIgnoreRow(const TWeakPtr<FDetailTreeNode>& WeakDetailTreeNode) const
{
	if (const TSharedPtr<FDetailTreeNode> DetailTreeNode = WeakDetailTreeNode.Pin())
	{
		if (const TSharedPtr<IPropertyHandle> Handle = DetailTreeNode->CreatePropertyHandle())
		{
			if (MarkedForDelete.Contains(*Handle->CreateFPropertyPath()))
			{
				return true;
			}
		}
	}
	return false;
}

bool FArchetypeFixupPanel::AreAllConflictsRedirected() const
{
	bool bFoundConflict = false;
	if (const TSharedPtr<FAsyncDetailViewDiff> Diff = DiffAgainstRight.Pin())
	{
		Diff->ForEach(ETreeTraverseOrder::PreOrder,
		[this, &bFoundConflict](const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			const TSharedPtr<FDetailTreeNode> TreeNode = DiffNode->ValueA.Pin();
			if (DiffNode->DiffResult == ETreeDiffResult::MissingFromTree2 && TreeNode)
			{
				if (const TSharedPtr<IPropertyHandle> Handle = TreeNode->CreatePropertyHandle())
				{
					if (!MarkedForDelete.Contains(*Handle->CreateFPropertyPath()))
					{
						bFoundConflict = true;
						return ETreeTraverseControl::Break;
					}
				}
			}
			return ETreeTraverseControl::Continue;
		});
	}
	return !bFoundConflict;
}

void FArchetypeFixupPanel::AutoApplyMarkDeletedActions()
{
	const TSharedPtr<FAsyncDetailViewDiff> Diff = DiffAgainstRight.Pin();
	if (!Diff)
	{
		return;
	}

	Diff->ForEach(ETreeTraverseOrder::PreOrder,
		[this] (const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			if (DiffNode->DiffResult == ETreeDiffResult::MissingFromTree2)
			{
				if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = DiffNode->ValueA.Pin())
				{
					MarkForDelete(LeftTreeNode->GetPropertyPath());
				}
			}
			
			return ETreeTraverseControl::Continue;
		});
}

bool FArchetypeFixupPanel::HasViewFlag(EViewFlags Flag)
{
	return static_cast<uint8>(Flag) & static_cast<uint8>(ViewFlags);
}

static void* ResolvePath(const FPropertyPath& Path, void* Value)
{
	for(int32 PathIndex = 0; PathIndex < Path.GetNumProperties(); ++PathIndex)
	{
		const FPropertyInfo& PropertyInfo = Path.GetPropertyInfo(PathIndex);
		const FProperty* Property = PropertyInfo.Property.Get();
		if (!Property)
		{
			return nullptr;
		}

		Value = Property->ContainerPtrToValuePtr<void>(Value, PropertyInfo.ArrayIndex != INDEX_NONE ? PropertyInfo.ArrayIndex : 0);

		if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
		{
			Value = AsObjectProperty->GetObjectPropertyValue(Value);
		}
		else if (PathIndex + 1 < Path.GetNumProperties())
		{
			if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(AsArrayProperty, Value);
				Value = Helper.GetElementPtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
			if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
			{
				FScriptSetHelper Helper(AsSetProperty, Value);
				Value = Helper.FindNthElementPtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
			if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(AsMapProperty, Value);
				Value = Helper.FindNthValuePtr(Path.GetPropertyInfo(++PathIndex).ArrayIndex);
			}
		}
	}
	
	return Value;
}

void FArchetypeFixupPanel::RedirectProperty(const FPropertyPath& From, const FPropertyPath& To)
{
	UArchetypeFixupUndoHandler* Snapshot = NewObject<UArchetypeFixupUndoHandler>();
	Snapshot->Init(SharedThis(this));
	GEditor->BeginTransaction(TEXT("ArchetypeFixupTool"), FText::Format(LOCTEXT("RedirectPropertyTransaction","Redirect {0} to {1}"), FText::FromString(From.ToString()), FText::FromString(To.ToString())), nullptr);
	
	if (const FPropertyPath* OriginalPath = OriginalPaths.Find(From))
	{
		if (To != *OriginalPath)
		{
			OriginalPaths.Add(To, *OriginalPath);
		}
		MarkedForDelete.Remove(*OriginalPath);
		OriginalPaths.Remove(From);
	}
	else
	{
		OriginalPaths.Add(To, From);
		MarkedForDelete.Remove(From);
	}

	if (To != From)
	{
		RedirectedPropertyTree->Move(From, To);
	}
	Snapshot->OnRedirect(From, To);
		
	for (UObject* Instance : Instances)
	{
		const FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
		const FProperty* DestinationProperty = To.GetLeafMostProperty().Property.Get();
		if (!ensure(SourceProperty && DestinationProperty))
		{
			continue;
		}
		const void* Source = ResolvePath(From, Instance);
		void* Destination = ResolvePath(To, Instance);
		
		if (!ensure(Source && Destination))
		{
			continue;
		}
	
		FPropertyChangedEvent ChangeEvent(To.GetRootProperty().Property.Get(), EPropertyChangeType::ValueSet);
		Instance->PreEditChange(ChangeEvent.Property);
		
		if (SourceProperty->SameType(DestinationProperty))
		{
			SourceProperty->CopyCompleteValue(Destination, Source);
		}
		else
		{
			FString ValueStr;
			SourceProperty->ExportText_Direct(ValueStr, Source, nullptr, Instance, PPF_Copy);
			DestinationProperty->ImportText_Direct(*ValueStr, Destination, Instance, PPF_Copy);
		}
		Instance->PostEditChangeProperty(ChangeEvent);
	}

	GEditor->EndTransaction();
	
	DetailsView->ForceRefresh();
}

void FArchetypeFixupPanel::OnRedirectProperty(FPropertyPath From, FPropertyPath To)
{
	RedirectProperty(From, To);
}

static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, FProperty* Property, void* Value);
static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, UStruct* Struct, void* StructValue)
{
	for (FProperty* Property : TFieldRange<FProperty>(Struct))
	{
		if (Property->ArrayDim == 1)
		{
			if (UE::WasPropertySetBySerialization(Struct, StructValue, Property))
			{
				const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(Property));
				void* Value = Property->ContainerPtrToValuePtr<void>(StructValue);
				InitRedirectedPropertyTreeRec(ChildNode, Property, Value);
			}
		}
		else
		{
			for (int32 StaticArrayIndex = 0; StaticArrayIndex < Property->ArrayDim; ++StaticArrayIndex)
            {
            	if (UE::WasPropertySetBySerialization(Struct, StructValue, Property, StaticArrayIndex))
            	{
            		const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(Property, StaticArrayIndex));
            		void* Value = Property->ContainerPtrToValuePtr<void>(StructValue, StaticArrayIndex);
            		InitRedirectedPropertyTreeRec(ChildNode, Property, Value);
            	}
            }
		}
	}
}

static void InitRedirectedPropertyTreeRec(const TSharedPtr<FRedirectedPropertyNode>& Node, FProperty* Property, void* Value)
{
	if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
	{
		InitRedirectedPropertyTreeRec(Node, AsStructProperty->Struct, Value);
	}
	else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Array(AsArrayProperty, Value);
		for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ++ArrayIndex)
		{
			const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(AsArrayProperty->Inner, ArrayIndex));
			InitRedirectedPropertyTreeRec(ChildNode, AsArrayProperty->Inner, Array.GetElementPtr(ArrayIndex));
		}
	}
	else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Set(AsSetProperty, Value);
		for (FScriptSetHelper::FIterator Itr = Set.CreateIterator(); Itr; ++Itr)
		{
			const TSharedPtr<FRedirectedPropertyNode>& ChildNode = Node->FindOrAdd(FPropertyInfo(AsSetProperty->ElementProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(ChildNode, AsSetProperty->ElementProp, Set.GetElementPtr(Itr));
		}
	}
	else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Map(AsMapProperty, Value);
		for (FScriptMapHelper::FIterator Itr = Map.CreateIterator(); Itr; ++Itr)
		{
			const TSharedPtr<FRedirectedPropertyNode>& KeyNode = Node->FindOrAdd(FPropertyInfo(AsMapProperty->KeyProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(KeyNode, AsMapProperty->KeyProp, Map.GetKeyPtr(Itr));
			const TSharedPtr<FRedirectedPropertyNode>& ValNode = Node->FindOrAdd(FPropertyInfo(AsMapProperty->ValueProp, Itr.GetLogicalIndex()));
			InitRedirectedPropertyTreeRec(ValNode, AsMapProperty->ValueProp, Map.GetValuePtr(Itr));
		}
	}
}

void FArchetypeFixupPanel::InitRedirectedPropertyTree()
{
	InitRedirectedPropertyTreeRec(RedirectedPropertyTree, Instances[0]->GetClass(), Instances[0]);
}

void UArchetypeFixupUndoHandler::Init(const TSharedRef<FArchetypeFixupPanel>& Panel)
{
	SetFlags(RF_Transactional);
	ArchetypePanel = Panel;
	OriginalPaths = Panel->OriginalPaths;
	MarkedForDelete = Panel->MarkedForDelete;
}

void UArchetypeFixupUndoHandler::OnRedirect(const FPropertyPath& From, const FPropertyPath& To)
{
	if (const TSharedPtr<FArchetypeFixupPanel> Panel = ArchetypePanel.Pin())
	{
		RedirectFrom = From;
		RedirectTo = To;
		++ChangeNum;
	}
	Modify();
}

void UArchetypeFixupUndoHandler::PostEditUndo()
{
	if (const TSharedPtr<FArchetypeFixupPanel> Panel = ArchetypePanel.Pin())
	{
		if (RedirectTo != RedirectFrom)
		{
			Panel->RedirectedPropertyTree->Move(RedirectTo, RedirectFrom);
			Swap(RedirectTo, RedirectFrom);
		}
		
		Swap(Panel->OriginalPaths, OriginalPaths);
		Swap(Panel->MarkedForDelete, MarkedForDelete);
		Panel->DetailsView->ForceRefresh();
	}
}

bool FArchetypeFixupPanel::IsInRedirectedPropertyTree(const FPropertyPath& Path) const
{
	return RedirectedPropertyTree->Find(Path).IsValid();
}

const FPropertyPath& FArchetypeFixupPanel::GetOriginalPath(const FPropertyPath& Path) const
{
	if (const FPropertyPath* Found = OriginalPaths.Find(Path))
	{
		return *Found;
	}
	return Path;
}

void FArchetypeFixupPanel::MarkForDelete(const FPropertyPath& CurrentPath)
{
	// undo any existing redirection on this node
	if (const FPropertyPath* OriginalPath = OriginalPaths.Find(CurrentPath))
	{
		// move this property back to it's original location before marking it for delete
		const FPropertyPath PathCopy = *OriginalPath; // RedirectProperty will invalidate pointers. copy path by value so it doesn't get destroyed.
		RedirectProperty(CurrentPath, PathCopy);
		MarkedForDelete.Add(PathCopy);
	}
	else
	{
		MarkedForDelete.Add(CurrentPath);
	}
}

void FArchetypeFixupPanel::OnMarkForDelete(FPropertyPath Path)
{
	MarkForDelete(Path);
}

#undef LOCTEXT_NAMESPACE
