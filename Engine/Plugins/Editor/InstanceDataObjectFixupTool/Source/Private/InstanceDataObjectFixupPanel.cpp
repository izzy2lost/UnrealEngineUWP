// Copyright Epic Games, Inc. All Rights Reserved.


#include "InstanceDataObjectFixupPanel.h"

#include "AsyncDetailViewDiff.h"
#include "DetailTreeNode.h"
#include "Widgets/Layout/LinkableScrollBar.h"
#include "InstanceDataObjectFixupDetailCustomization.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "UObject/PropertyBagRepository.h"

#include "UObject/OverriddenPropertySet.h"
#include "UObject/OverridableManager.h"

#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupPanel"

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

FInstanceDataObjectFixupPanel::FInstanceDataObjectFixupPanel(TConstArrayView<TObjectPtr<UObject>> InstanceDataObjects, EViewFlags InViewFlags)
	: Instances(InstanceDataObjects)
	, RedirectedPropertyTree(MakeShared<FRedirectedPropertyNode>())
	, ViewFlags(InViewFlags)
{
	InitRedirectedPropertyTree();
}

int32 FInstanceDataObjectFixupPanel::Find(UObject* Value) const
{
	return Instances.Find(Value);
}

TSharedPtr<IDetailsView>& FInstanceDataObjectFixupPanel::GenerateDetailsView(bool bScrollbarOnLeft)
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
				return MakeShared<FInstanceDataObjectFixupDetailCustomization>(DiffPanel);
			}));
		}
	}
	
	DetailsView->SetObjects(Instances, true);
	return DetailsView;
}

void FInstanceDataObjectFixupPanel::SetDiffAgainstLeft(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstLeft)
{
	DiffAgainstLeft = InDiffAgainstLeft;
}

void FInstanceDataObjectFixupPanel::SetDiffAgainstRight(const TSharedPtr<FAsyncDetailViewDiff>& InDiffAgainstRight)
{
	DiffAgainstRight = InDiffAgainstRight;
}

TSharedPtr<FAsyncDetailViewDiff> FInstanceDataObjectFixupPanel::GetDiffAgainstLeft() const
{
	return DiffAgainstLeft.Pin();
}

TSharedPtr<FAsyncDetailViewDiff> FInstanceDataObjectFixupPanel::GetDiffAgainstRight() const
{
	return DiffAgainstRight.Pin();
}

bool FInstanceDataObjectFixupPanel::ShouldSplitterIgnoreRow(const TWeakPtr<FDetailTreeNode>& WeakDetailTreeNode) const
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

bool FInstanceDataObjectFixupPanel::AreAllConflictsRedirected() const
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
					if (!Handle->IsCategoryHandle() && !MarkedForDelete.Contains(*Handle->CreateFPropertyPath()))
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

void FInstanceDataObjectFixupPanel::AutoApplyMarkDeletedActions()
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
					const FPropertyPath Path = LeftTreeNode->GetPropertyPath();
					if (Path.IsValid())
					{
						MarkForDelete(Path);
					}
				}
			}
			
			return ETreeTraverseControl::Continue;
		});
}

bool FInstanceDataObjectFixupPanel::HasViewFlag(EViewFlags Flag)
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
			UObject* Object = AsObjectProperty->GetObjectPropertyValue(Value);
			UE::FPropertyBagRepository& PropertyBagRepository = UE::FPropertyBagRepository::Get();
			if (UObject* Found = PropertyBagRepository.FindInstanceDataObject(Object))
			{
				Object = Found;
			}
			Value = Object;
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

static FPropertyChangedEvent ConstructChangeEventForRedirect(const FPropertyPath& Path, FEditPropertyChain& OutChain, TMap<FString, int32>& OutArrayIndices)
{
	FPropertyChangedEvent OutEvent = FPropertyChangedEvent(Path.GetLeafMostProperty().Property.Get(), EPropertyChangeType::ValueSet);
	for (int32 I = 0; I < Path.GetNumProperties(); ++I)
	{
		const FPropertyInfo& Info = Path.GetPropertyInfo(I);
		OutChain.AddTail(Info.Property.Get()); // only the head is used in OverrideProperty
		if (Info.ArrayIndex != INDEX_NONE)
		{
			OutArrayIndices.Add(Info.Property->GetName(), Info.ArrayIndex);
		}
		if (Info.Property->IsA<FArrayProperty>() || Info.Property->IsA<FSetProperty>() || Info.Property->IsA<FMapProperty>())
		{
			if (++I < Path.GetNumProperties())
			{
				OutArrayIndices.Add(Info.Property->GetName(), Path.GetPropertyInfo(I).ArrayIndex);
			}
		}
	}
	OutEvent.SetArrayIndexPerObject(MakeArrayView(&OutArrayIndices, 1));
	return OutEvent;
}

void FInstanceDataObjectFixupPanel::RedirectProperty(const FPropertyPath& From, const FPropertyPath& To)
{
	UInstanceDataObjectFixupUndoHandler* Snapshot = NewObject<UInstanceDataObjectFixupUndoHandler>();
	Snapshot->Init(SharedThis(this));
	GEditor->BeginTransaction(TEXT("InstanceDataObjectFixupTool"), FText::Format(LOCTEXT("RedirectPropertyTransaction","Redirect {0} to {1}"), FText::FromString(From.ToString()), FText::FromString(To.ToString())), nullptr);

	FProperty* SourceProperty = From.GetLeafMostProperty().Property.Get();
	check(SourceProperty);
	FProperty* DestinationProperty = To.IsValid() ? To.GetLeafMostProperty().Property.Get() : nullptr;
	FRevertInfo* ToRevertInfo = nullptr;
	TOptional<FRevertInfo> FromRevertInfo;
	
	if (const FRevertInfo* Info = RevertInfo.Find(From))
	{
		FromRevertInfo = *Info;
		if (DestinationProperty)
		{
			if (DestinationProperty->HasAnyPropertyFlags(CPF_Transient) != Info->bWasTransient)
            {
            	// toggle transient flag if needed
            	DestinationProperty->PropertyFlags ^= CPF_Transient;
            }
            if (!Info->bWasHidden)
            {
            	DestinationProperty->RemoveMetaData(TEXT("Hidden"));
            }
            DestinationProperty->RemoveMetaData(TEXT("Redirected"));
		}
		
		
		if (To.IsValid() && To != Info->OriginalPath)
		{
			TArray<uint8> OriginalValue;
			
			ToRevertInfo = &RevertInfo.Add(To, {
				.OriginalPath = Info->OriginalPath,
				.bWasTransient = SourceProperty->HasAnyPropertyFlags(CPF_Transient),
				.bWasHidden = SourceProperty->HasMetaData(TEXT("Hidden"))
			});
		}
		MarkedForDelete.Remove(Info->OriginalPath);
		RevertInfo.Remove(From);
	}
	else
	{
		if (To.IsValid())
		{
			ToRevertInfo = &RevertInfo.Add(To, {
				.OriginalPath = From,
				.bWasTransient = SourceProperty->HasAnyPropertyFlags(CPF_Transient)
			});
			MarkedForDelete.Remove(From);
		}
	}
	
	if (To != From)
	{
		if (To.IsValid())
		{
			RedirectedPropertyTree->Move(From, To);
		}
		if (SourceProperty->HasMetaData(TEXT("isLoose")))
		{
			SourceProperty->PropertyFlags |= CPF_Transient;
			SourceProperty->SetMetaData(TEXT("Hidden"), TEXT("True"));
			SourceProperty->SetMetaData(TEXT("Redirected"), TEXT("True"));
		}
	}

	Snapshot->OnRedirect(From, To);
	
	if (!DestinationProperty)
	{
		MarkedForDelete.Add(From);
		GEditor->EndTransaction();
        DetailsView->ForceRefresh();
		return; // delete actions don't need data copied
	}
	

	const uint8* FromRevertInfoItr = FromRevertInfo ? FromRevertInfo->OriginalValue.GetData() : nullptr;
	for (UObject* Instance : Instances)
	{
		void* Source = ResolvePath(From, Instance);
		void* Destination = ResolvePath(To, Instance);
		
		if (!ensure(Source && Destination))
		{
			continue;
		}
	

		// construct change event
		FEditPropertyChain Chain;
		TMap<FString, int32> ArrayIndices;
		FPropertyChangedEvent ChangeEvent = ConstructChangeEventForRedirect(To, Chain, ArrayIndices);
		FOverridableManager::Get().PreOverrideProperty(*Instance, Chain);
		Instance->PreEditChange(ChangeEvent.Property);

		if (ToRevertInfo)
		{
			// cache the destination value so it can be reverted later
			const int32 Size = DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
			ToRevertInfo->OriginalValue.AddZeroed(Size);
			uint8* Buffer = ToRevertInfo->OriginalValue.GetData() + (ToRevertInfo->OriginalValue.Num() - Size);
			DestinationProperty->CopyCompleteValue(Buffer, Destination);
		}
		
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
		
		if (FromRevertInfo)
		{
			// apply FromRevertInfo to From
			SourceProperty->CopyCompleteValue(Source, FromRevertInfoItr);
			FromRevertInfoItr += DestinationProperty->ArrayDim * DestinationProperty->ElementSize;
		}
		Instance->PostEditChangeProperty(ChangeEvent);
		FOverridableManager::Get().PostOverrideProperty(*Instance, ChangeEvent, Chain);
	}

	GEditor->EndTransaction();
	
	DetailsView->ForceRefresh();
}

void FInstanceDataObjectFixupPanel::OnRedirectProperty(FPropertyPath From, FPropertyPath To)
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
			if (UE::FPropertyBagRepository::WasPropertySetBySerialization(Struct, StructValue, Property))
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
            	if (UE::FPropertyBagRepository::WasPropertySetBySerialization(Struct, StructValue, Property, StaticArrayIndex))
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
	else if (const FObjectProperty* AsObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (AsObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
		{
			if (UObject* Object = AsObjectProperty->GetObjectPropertyValue(Value))
            {
				UE::FPropertyBagRepository& PropertyBagRepository = UE::FPropertyBagRepository::Get();
				if (UObject* Found = PropertyBagRepository.FindInstanceDataObject(Object))
				{
					Object = Found;
				}
            	InitRedirectedPropertyTreeRec(Node, Object->GetClass(), Object);
            }
		}
		
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

void FInstanceDataObjectFixupPanel::InitRedirectedPropertyTree()
{
	InitRedirectedPropertyTreeRec(RedirectedPropertyTree, Instances[0]->GetClass(), Instances[0]);
}

void UInstanceDataObjectFixupUndoHandler::Init(const TSharedRef<FInstanceDataObjectFixupPanel>& Panel)
{
	InstanceDataObjectPanel = Panel;
	RevertInfo = Panel->RevertInfo;
	MarkedForDelete = Panel->MarkedForDelete;
	SetFlags(RF_Transactional);
}

void UInstanceDataObjectFixupUndoHandler::OnRedirect(const FPropertyPath& From, const FPropertyPath& To)
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = InstanceDataObjectPanel.Pin())
	{
		RedirectFrom = From;
		RedirectTo = To;
		++ChangeNum;
	}
	Modify();
}

void UInstanceDataObjectFixupUndoHandler::PostEditUndo()
{
	if (const TSharedPtr<FInstanceDataObjectFixupPanel> Panel = InstanceDataObjectPanel.Pin())
	{
		if (RedirectTo != RedirectFrom)
		{
			if (RedirectTo.IsValid() && RedirectFrom.IsValid())
			{
				Panel->RedirectedPropertyTree->Move(RedirectTo, RedirectFrom);
			}
			Swap(RedirectTo, RedirectFrom);
		}
		
		Swap(Panel->RevertInfo, RevertInfo);
		Swap(Panel->MarkedForDelete, MarkedForDelete);
		Panel->DetailsView->ForceRefresh();
	}
}

bool FInstanceDataObjectFixupPanel::IsInRedirectedPropertyTree(const FPropertyPath& Path) const
{
	return RedirectedPropertyTree->Find(Path).IsValid();
}

const FPropertyPath& FInstanceDataObjectFixupPanel::GetOriginalPath(const FPropertyPath& Path) const
{
	if (const FRevertInfo* Found = RevertInfo.Find(Path))
	{
		return Found->OriginalPath;
	}
	return Path;
}

void FInstanceDataObjectFixupPanel::MarkForDelete(const FPropertyPath& CurrentPath)
{
	// undo any existing redirection on this node
	if (const FRevertInfo* Found = RevertInfo.Find(CurrentPath))
	{
		// move this property back to it's original location before marking it for delete
		const FPropertyPath PathCopy = Found->OriginalPath; // RedirectProperty will invalidate pointers. copy path by value so it doesn't get destroyed.
		RedirectProperty(CurrentPath, PathCopy);
		RedirectProperty(PathCopy, {});
	}
	else
	{
		RedirectProperty(CurrentPath, {});
	}
}

void FInstanceDataObjectFixupPanel::OnMarkForDelete(FPropertyPath Path)
{
	MarkForDelete(Path);
}

#undef LOCTEXT_NAMESPACE
