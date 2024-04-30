// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSkeletonView.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEdNode.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "IEditableSkeleton.h"
#include "ISkeletonTree.h"
#include "ISkeletonTreeItem.h"
#include "Engine/SkeletalMesh.h"

//#include "Widgets/SCompoundWidget.h"

FDataflowSkeletonView::FDataflowSkeletonView(UDataflowEditor* InDataflowEditor)
	: DataflowEditor(InDataflowEditor)
	, SkeletonEditor(nullptr)
	, SkeletalMesh(NewObject<USkeletalMesh>())
	, SelectedNode(nullptr)
	, CollectionIndexRemap(TArray<int32>())

{
	SkeletalMesh->SetSkeleton(NewObject<USkeleton>(SkeletalMesh, NAME_Name));
}

FDataflowSkeletonView::~FDataflowSkeletonView()
{
	if (SkeletonEditor)
	{
		// remove widget delegates (see FDataflowCollectionSpreadSheet)
	}
}

void FDataflowSkeletonView::SetSupportedOutputTypes()
{
	GetSupportedOutputTypes().Empty();

	GetSupportedOutputTypes().Add("FManagedArrayCollection");
}

void FDataflowSkeletonView::SetSkeleton(USkeleton* Skeleton)
{
	if (Skeleton)
	{
		SkeletalMesh = NewObject<USkeletalMesh>();
		SkeletalMesh->SetSkeleton(Skeleton);
		SkeletalMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());
	}
	else
	{
		SkeletalMesh = NewObject<USkeletalMesh>();
		SkeletalMesh->SetSkeleton(NewObject<USkeleton>(SkeletalMesh, NAME_Name));
	}
}

USkeleton* FDataflowSkeletonView::GetSkeleton()
{
	if (SkeletalMesh)
	{
		return SkeletalMesh->GetSkeleton();
	}
	return nullptr;
}

void FDataflowSkeletonView::UpdateViewData()
{
	if(!ensure(DataflowEditor)) return;

	if (TObjectPtr<UDataflowEdNode> EdNode = GetSelectedNode())
	{
		if (GetSelectedNode()->IsBound())
		{
			if (TSharedPtr<FDataflowNode> Node = GetSelectedNode()->DataflowGraph->FindBaseNode(GetSelectedNode()->DataflowNodeGuid))
			{
				if (FDataflowOutput* Output = Node->FindOutput(FName("Collection")))
				{
					if (TObjectPtr<UDataflowBaseContent> EditorContent = DataflowEditor->GetDataflowContent())
					{
						if (EditorContent->GetDataflowAsset())
						{
							Dataflow::FTimestamp TimeStamp = EditorContent->GetLastModifiedTimestamp();
							if (EditorContent->GetDataflowContext())
							{
								FManagedArrayCollection DefaultCollection;
								const FManagedArrayCollection& Result = Output->GetValue(*EditorContent->GetDataflowContext(), DefaultCollection);

								SelectedNode = EdNode;
								SkeletalMesh = NewObject<USkeletalMesh>();
								TObjectPtr<USkeleton> Skeleton = NewObject<USkeleton>(SkeletalMesh, Node->Name);

								FGeometryCollectionEngineConversion::ConvertCollectionToSkeleton(Result, Skeleton, CollectionIndexRemap);
								SkeletalMesh->SetSkeleton(Skeleton);
								SkeletalMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());

								if (SkeletonEditor)
								{
									SkeletonEditor->GetEditableSkeleton()->RecreateBoneTree(SkeletalMesh);
									SkeletonEditor->SetSkeletalMesh(SkeletalMesh);
									SkeletonEditor->Refresh();
								}
							}
						}
					}
				}
			}
		}
	}
}

void FDataflowSkeletonView::SetSkeletonEditor(TSharedPtr<ISkeletonTree>& InSkeletonEditor)
{
	ensure(!SkeletonEditor);

	SkeletonEditor = InSkeletonEditor;

	if (SkeletonEditor)
	{
		// add widget delegates (see FDataflowCollectionSpreadSheet)
	}
}

void FDataflowSkeletonView::SkeletonViewSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	if (SkeletonEditor)
	{
		//TArray<UObject*> Objects;
		//Algo::TransformIf(InSelectedItems, Objects,
		//	[](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; },
		//	[](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });
		//DetailsView->SetObjects(Objects);
	}
}

void FDataflowSkeletonView::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowNodeView::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SelectedNode);
	Collector.AddReferencedObject(SkeletalMesh);
}
