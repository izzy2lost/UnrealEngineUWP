// Copyright Epic Games, Inc. All Rights Reserved.
#include "AsyncDetailViewDiff.h"

#include "DetailTreeNode.h"
#include "IDetailsViewPrivate.h"
#include "DiffUtils.h"
#include "PropertyHandleImpl.h"

namespace AsyncDetailViewDiffHelpers
{
	TArray<TWeakObjectPtr<UObject>> GetObjects(const TSharedPtr<FDetailTreeNode>& TreeNode)
	{
		TArray<UObject*> Result;
		if (const IDetailsViewPrivate* DetailsView = TreeNode->GetDetailsView())
		{
			return DetailsView->GetSelectedObjects();
		}
		return {};
	}

	FResolvedProperty GetResolvedProperty(const TSharedPtr<FPropertyNode>& PropertyNode, const UObject* Object)
	{
		if (PropertyNode && Object)
		{
			const TSharedRef<FPropertyPath> PropertyPath = FPropertyNode::CreatePropertyPath(PropertyNode.ToSharedRef());
			if (PropertyPath->IsValid())
			{
				return FPropertySoftPath(*PropertyPath).Resolve(Object);
			}
		}
		return FResolvedProperty();
	}
	
	bool MapKeysMatch(const TSharedRef<FPropertyNode>& MapPropertyNodeA, const TSharedRef<FPropertyNode>& MapPropertyNodeB, int32 KeyIndexA, int32 KeyIndexB,
		const UObject* OwningObjectA, const UObject* OwningObjectB)
	{
		const FMapProperty* MapPropertyA = CastField<FMapProperty>(MapPropertyNodeA->GetProperty());
		const FMapProperty* MapPropertyB = CastField<FMapProperty>(MapPropertyNodeB->GetProperty());
		if (!MapPropertyA || !MapPropertyB)
		{
			return false;
		}
		
		const FResolvedProperty ResolvedMapA = AsyncDetailViewDiffHelpers::GetResolvedProperty(MapPropertyNodeA, OwningObjectA);
		const FResolvedProperty ResolvedMapB = AsyncDetailViewDiffHelpers::GetResolvedProperty(MapPropertyNodeB, OwningObjectB);
		FScriptMapHelper MapHelperA(MapPropertyA, MapPropertyA->ContainerPtrToValuePtr<UObject*>(ResolvedMapA.Object));
		FScriptMapHelper MapHelperB(MapPropertyB, MapPropertyB->ContainerPtrToValuePtr<UObject*>(ResolvedMapB.Object));
		
		const void* KeyA = MapHelperA.FindNthKeyPtr(KeyIndexA);
		const void* KeyB = MapHelperB.FindNthKeyPtr(KeyIndexB);
		
		if (MapPropertyA->KeyProp->SameType(MapPropertyB->KeyProp))
		{
			return MapPropertyA->KeyProp->Identical(KeyA, KeyB, PPF_DeepComparison);
		}
		return false;
	}

	bool SetKeysMatch(const TSharedRef<FPropertyNode>& SetPropertyNodeA, const TSharedRef<FPropertyNode>& SetPropertyNodeB, int32 KeyIndexA, int32 KeyIndexB,
		const UObject* OwningObjectA, const UObject* OwningObjectB)
	{
		const FSetProperty* SetPropertyA = CastField<FSetProperty>(SetPropertyNodeA->GetProperty());
		const FSetProperty* SetPropertyB = CastField<FSetProperty>(SetPropertyNodeB->GetProperty());
		if (!SetPropertyA || !SetPropertyB)
		{
			return false;
		}
		
		const FResolvedProperty ResolvedSetA = AsyncDetailViewDiffHelpers::GetResolvedProperty(SetPropertyNodeA, OwningObjectA);
		const FResolvedProperty ResolvedSetB = AsyncDetailViewDiffHelpers::GetResolvedProperty(SetPropertyNodeB, OwningObjectB);
		FScriptSetHelper SetHelperA(SetPropertyA, SetPropertyA->ContainerPtrToValuePtr<UObject*>(ResolvedSetA.Object));
		FScriptSetHelper SetHelperB(SetPropertyB, SetPropertyB->ContainerPtrToValuePtr<UObject*>(ResolvedSetB.Object));
		
		const void* KeyA = SetHelperA.FindNthElementPtr(KeyIndexA);
		const void* KeyB = SetHelperB.FindNthElementPtr(KeyIndexB);
		
		if (SetPropertyA->ElementProp->SameType(SetPropertyB->ElementProp))
		{
			return SetPropertyA->ElementProp->Identical(KeyA, KeyB, PPF_DeepComparison);
		}
		return false;
	}
}




bool TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>::AreValuesEqual(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB) const
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeA = TreeNodeA.Pin();
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeB = TreeNodeB.Pin();
	if (!PinnedTreeNodeA || !PinnedTreeNodeB)
	{
		return PinnedTreeNodeA == PinnedTreeNodeB;
	}

	const TSharedPtr<IPropertyHandle> PropertyHandleA = PinnedTreeNodeA->CreatePropertyHandle();
	const TSharedPtr<IPropertyHandle> PropertyHandleB = PinnedTreeNodeB->CreatePropertyHandle();
	if (!PropertyHandleA || !PropertyHandleB)
	{
		// category nodes
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}
	const TSharedPtr<FPropertyNode>& PropertyNodeA = StaticCastSharedPtr<FPropertyHandleBase>(PropertyHandleA)->GetPropertyNode();
	const TSharedPtr<FPropertyNode>& PropertyNodeB = StaticCastSharedPtr<FPropertyHandleBase>(PropertyHandleB)->GetPropertyNode();
	if (!PropertyNodeA || !PropertyNodeB)
	{
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}

	TArray<void*> DataValuesA;
	TArray<void*> DataValuesB;
	PropertyHandleA->AccessRawData(DataValuesA);
	PropertyHandleB->AccessRawData(DataValuesB);
	
	if(DataValuesA.IsEmpty() || DataValuesB.IsEmpty())
	{
		return true;
	}
	
	const TArray<TWeakObjectPtr<UObject>> OwningObjectsA = AsyncDetailViewDiffHelpers::GetObjects(PinnedTreeNodeA);
	const TArray<TWeakObjectPtr<UObject>> OwningObjectsB = AsyncDetailViewDiffHelpers::GetObjects(PinnedTreeNodeB);

	if (!ensure(OwningObjectsA.Num() == DataValuesA.Num() && OwningObjectsB.Num() == DataValuesB.Num()))
	{
		return true;
	}
	return DiffUtils::Identical(PropertyHandleA, PropertyHandleB, OwningObjectsA, OwningObjectsB);
}

bool TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>::AreMatching(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB) const
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeA = TreeNodeA.Pin();
	const TSharedPtr<FDetailTreeNode> PinnedTreeNodeB = TreeNodeB.Pin();
	if (!PinnedTreeNodeA || !PinnedTreeNodeB)
	{
		return PinnedTreeNodeA == PinnedTreeNodeB;
	}

	
	TArray<TSharedRef<FPropertyNode>> PropertyNodesA;
	TArray<TSharedRef<FPropertyNode>> PropertyNodesB;
	PinnedTreeNodeA->GetAllPropertyNodes(PropertyNodesA);
	PinnedTreeNodeB->GetAllPropertyNodes(PropertyNodesB);
	if (PropertyNodesA.Num() != PropertyNodesB.Num())
	{
		return false;
	}
	if (PropertyNodesA.IsEmpty())
	{
		// category nodes
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}

	const TArray<TWeakObjectPtr<UObject>> OwningObjectsA = AsyncDetailViewDiffHelpers::GetObjects(PinnedTreeNodeA);
	const TArray<TWeakObjectPtr<UObject>> OwningObjectsB = AsyncDetailViewDiffHelpers::GetObjects(PinnedTreeNodeB);
	if (OwningObjectsA.IsEmpty() || OwningObjectsB.IsEmpty())
	{
		return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
	}
	
	// because multi-edit puts matching keys together, we can assume all objects share the same key for this property
	// and just look at the first instance
	const UObject* OwningObjectB = OwningObjectsB[0].Get();
	const UObject* OwningObjectA = OwningObjectsA[0].Get();
	
	for (int32 PropNodeIndex = 0; PropNodeIndex < PropertyNodesA.Num(); ++PropNodeIndex)
	{
		const TSharedRef<FPropertyNode>& PropertyNodeA = PropertyNodesA[PropNodeIndex];
		const TSharedRef<FPropertyNode>& PropertyNodeB = PropertyNodesB[PropNodeIndex];

		// property nodes
		const int32 ArrayIndexA = PropertyNodeA->GetArrayIndex();
		const int32 ArrayIndexB = PropertyNodeB->GetArrayIndex();
		const FProperty* PropertyA = PropertyNodeA->GetProperty();
		const FProperty* PropertyB = PropertyNodeB->GetProperty();
		
		if (ArrayIndexA != INDEX_NONE && ArrayIndexB != INDEX_NONE)
		{
			const TSharedRef<FPropertyNode> ParentPropertyNodeA = PropertyNodeA->GetParentNode()->AsShared();
			const TSharedRef<FPropertyNode> ParentPropertyNodeB = PropertyNodeB->GetParentNode()->AsShared();
			const FProperty* ParentPropertyA = ParentPropertyNodeA->GetProperty();
			const FProperty* ParentPropertyB = ParentPropertyNodeB->GetProperty();
			
			// sets and maps are stored by index in the property tree so we need to dig their keys out of the data
			// and compare those instead
			if (ParentPropertyA->IsA<FMapProperty>() || ParentPropertyB->IsA<FMapProperty>())
			{
				if (!AsyncDetailViewDiffHelpers::MapKeysMatch(ParentPropertyNodeA, ParentPropertyNodeB, ArrayIndexA, ArrayIndexB, OwningObjectA, OwningObjectB))
				{
					return false;
				}
			}
			if (ParentPropertyA->IsA<FSetProperty>() || ParentPropertyB->IsA<FSetProperty>())
			{
				if (!AsyncDetailViewDiffHelpers::SetKeysMatch(ParentPropertyNodeA, ParentPropertyNodeB, ArrayIndexA, ArrayIndexB, OwningObjectA, OwningObjectB))
				{
					return false;
				}
			}
		}
		
		if (ArrayIndexA != ArrayIndexB)
		{
			return false;
		}
		const FName PropertyNameA = PropertyA ? PropertyA->GetFName() : NAME_None;
		const FName PropertyNameB = PropertyB ? PropertyB->GetFName() : NAME_None;

		if (PropertyNameA != PropertyNameB)
		{
			return false;
		}
	}
	return true;
}

void TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>::GetChildren(const TWeakPtr<FDetailTreeNode>& InParent, TArray<TWeakPtr<FDetailTreeNode>>& OutChildren) const
{
	const TSharedPtr<FDetailTreeNode> PinnedParent = InParent.Pin();
	if (PinnedParent)
	{
		TArray<TSharedRef<FDetailTreeNode>> Children;
		PinnedParent->GetChildren(Children);
		for (TSharedRef<FDetailTreeNode> Child : Children)
		{
			OutChildren.Add(Child);
		}
	}
}

bool TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>::ShouldMatchByValue(const TWeakPtr<FDetailTreeNode>& TreeNode) const
{
	const TSharedPtr<FDetailTreeNode> PinnedTreeNode = TreeNode.Pin();
	if (!PinnedTreeNode)
	{
		return false;
	}
	const TSharedPtr<FPropertyNode> PropertyNodeA = PinnedTreeNode->GetPropertyNode();
	if (!PropertyNodeA || !PropertyNodeA->GetParentNode())
	{
		return false;
	}
	
	const int32 ArrayIndex = PropertyNodeA->GetArrayIndex();
	const FArrayProperty* ParentArrayProperty = CastField<FArrayProperty>(PropertyNodeA->GetParentNode()->GetProperty());
	
	// match array elements by value rather than by index
	return ParentArrayProperty && ArrayIndex != INDEX_NONE;
}

FAsyncDetailViewDiff::FAsyncDetailViewDiff(TSharedRef<IDetailsView> InLeftView, TSharedRef<IDetailsView> InRightView)
	: TAsyncTreeDifferences(RootNodesAttribute(InLeftView), RootNodesAttribute(InRightView))
	, LeftView(InLeftView)
	, RightView(InRightView)
{}

void FAsyncDetailViewDiff::GetPropertyDifferences(TArray<FSingleObjectDiffEntry>& OutDiffEntries) const
{
	ForEach(ETreeTraverseOrder::PreOrder, [&](const TUniquePtr<DiffNodeType>& Node)->ETreeTraverseControl
	{
		FPropertyPath PropertyPath;
		FPropertyPath RightPropertyPath;
		if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = Node->ValueA.Pin())
		{
			PropertyPath = LeftTreeNode->GetPropertyPath();
		}
		else if (const TSharedPtr<FDetailTreeNode> RightTreeNode = Node->ValueB.Pin())
		{
			PropertyPath = RightTreeNode->GetPropertyPath();
		}

		// only include tree nodes with properties
		if (!PropertyPath.IsValid())
		{
			return ETreeTraverseControl::Continue;
		}
		
		EPropertyDiffType::Type PropertyDiffType;
        switch(Node->DiffResult)
        {
        case ETreeDiffResult::MissingFromTree1: 
            PropertyDiffType = EPropertyDiffType::PropertyAddedToB;
            break;
        case ETreeDiffResult::MissingFromTree2: 
            PropertyDiffType = EPropertyDiffType::PropertyAddedToA;
            break;
        case ETreeDiffResult::DifferentValues: 
            PropertyDiffType = EPropertyDiffType::PropertyValueChanged;
            break;
        default:
        	// only include changes
            return ETreeTraverseControl::Continue;
        }

		OutDiffEntries.Add(FSingleObjectDiffEntry(PropertyPath, PropertyDiffType));
        return ETreeTraverseControl::SkipChildren; // only include top-most properties
	});
}

TPair<int32, int32> FAsyncDetailViewDiff::ForEachRow(const TFunction<ETreeTraverseControl(const TUniquePtr<DiffNodeType>&, int32, int32)>& Method) const
{
	const TSharedPtr<IDetailsView> LeftDetailsView = LeftView.Pin();
	const TSharedPtr<IDetailsView> RightDetailsView = RightView.Pin();
	if (!LeftDetailsView || !RightDetailsView)
	{
		return {0,0};
	}
	
	int32 LeftRowNum = 0;
	int32 RightRowNum = 0;
	ForEach(
		ETreeTraverseOrder::PreOrder,
		[&LeftDetailsView,&RightDetailsView,&Method,&LeftRowNum,&RightRowNum](const TUniquePtr<DiffNodeType>& DiffNode)->ETreeTraverseControl
		{
			bool bFoundLeftRow = false;
			if (const TSharedPtr<FDetailTreeNode> LeftTreeNode = DiffNode->ValueA.Pin())
			{
				if (!LeftDetailsView->IsAncestorCollapsed(LeftTreeNode.ToSharedRef()))
				{
					bFoundLeftRow = true;
				}
			}
			
			bool bFoundRightRow = false;
			if (const TSharedPtr<FDetailTreeNode> RightTreeNode = DiffNode->ValueB.Pin())
			{
				if (!RightDetailsView->IsAncestorCollapsed(RightTreeNode.ToSharedRef()))
				{
					bFoundRightRow = true;
				}
			}

			ETreeTraverseControl Control = ETreeTraverseControl::SkipChildren;
			if (bFoundRightRow || bFoundLeftRow)
			{
				Control = Method(DiffNode, LeftRowNum, RightRowNum);
			}
			
			if (bFoundLeftRow)
			{
				++LeftRowNum;
			}
			if (bFoundRightRow)
			{
				++RightRowNum;
			}
			
			return Control;
		}
	);
	return {LeftRowNum, RightRowNum};
}

TArray<FVector2f> FAsyncDetailViewDiff::GenerateScrollSyncRate() const
{
	TArray<FIntVector2> MatchingRows;

	// iterate matching rows of both details panels simultaneously
	auto [LeftRowCount, RightRowCount] = ForEachRow(
		[&MatchingRows](const TUniquePtr<DiffNodeType>& DiffNode, int32 LeftRow, int32 RightRow)->ETreeTraverseControl
		{
			// if both trees share this row, sync scrolling here
			if (DiffNode->ValueA.IsValid() && DiffNode->ValueB.IsValid())
			{
				if (MatchingRows.IsEmpty() || (MatchingRows.Last().X != LeftRow && MatchingRows.Last().Y != RightRow))
				{
					MatchingRows.Emplace(LeftRow, RightRow);
				}
			}
			return ETreeTraverseControl::Continue;
		}
	);

	TArray<FVector2f> FixedPoints;
	FixedPoints.Emplace(0.f, 0.f);
	
	// normalize fixed points
	for (FIntVector2& MatchingRow : MatchingRows)
	{
		FixedPoints.Emplace(
			StaticCast<float>(MatchingRow.X) / LeftRowCount,
			StaticCast<float>(MatchingRow.Y) / RightRowCount
		);
	}
	FixedPoints.Emplace(1.f, 1.f);
	
	return FixedPoints;
}

TAttribute<TArray<TWeakPtr<FDetailTreeNode>>> FAsyncDetailViewDiff::RootNodesAttribute(TWeakPtr<IDetailsView> DetailsView)
{
	return TAttribute<TArray<TWeakPtr<FDetailTreeNode>>>::CreateLambda([DetailsView]()
	{
		TArray<TWeakPtr<FDetailTreeNode>> Result;
		if (const TSharedPtr<IDetailsViewPrivate> Details = StaticCastSharedPtr<IDetailsViewPrivate>(DetailsView.Pin()))
		{
			Details->GetHeadNodes(Result);
		}
		return Result;
	});
}

