// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSkeletonView.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEdNode.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "IEditableSkeleton.h"
#include "ISkeletonTree.h"
#include "ISkeletonTreeItem.h"
#include "ISkeletonEditorModule.h"
#include "Engine/SkeletalMesh.h"


FDataflowSkeletonView::FDataflowSkeletonView(UDataflowEditor* InDataflowEditor)
	: DataflowEditor(InDataflowEditor)
	, SkeletonEditor(nullptr)
	, SkeletalMesh(NewObject<USkeletalMesh>())
	, CollectionIndexRemap(TArray<int32>())

{
	check(DataflowEditor);

	SetSkeleton(NewObject<USkeleton>(SkeletalMesh, NAME_Name));
	if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowEditor->GetDataflowContent()))
	{
		if (SkeletalContent->GetDataflowAsset())
		{
			SetSkeleton(SkeletalContent->GetSkeleton());
		}
	}

}

FDataflowSkeletonView::~FDataflowSkeletonView()
{
	if (SkeletonEditor)
	{
		// remove widget delegates (see FDataflowCollectionSpreadSheet)
	}
}

TSharedPtr<ISkeletonTree> FDataflowSkeletonView::CreateEditor(FSkeletonTreeArgs& InSkeletonTreeArgs)
{
	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonEditor = SkeletonEditorModule.CreateSkeletonTree(GetSkeleton(), InSkeletonTreeArgs);
	SkeletonEditor->Refresh();
	// add widget delegates (see FDataflowCollectionSpreadSheet)
	return SkeletonEditor;
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
	if (SkeletonEditor)
	{
		SkeletonEditor->Refresh();
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
	if (!ensure(DataflowEditor)) return;

	bool bNeedsDefaultSkeleton = true;
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
								bNeedsDefaultSkeleton = false;
							}
						}
					}
				}
			}
		}
	}


	if (bNeedsDefaultSkeleton)
	{
		SetSkeleton(NewObject<USkeleton>(SkeletalMesh, NAME_Name));
		if (DataflowEditor)
		{
			if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowEditor->GetDataflowContent()))
			{
				if (SkeletalContent->GetDataflowAsset())
				{
					SetSkeleton(SkeletalContent->GetSkeleton());
				}
			}
		}
		if (SkeletonEditor)
		{
			SkeletonEditor->Refresh();
		}
	}
}

void FDataflowSkeletonView::ConstructionViewSelectionChanged(const TArray<UPrimitiveComponent*>& InSelectedComponents)
{
	if (ensure(SkeletonEditor))
	{
		SkeletonEditor->DeselectAll();
		for (UPrimitiveComponent* Component : InSelectedComponents)
		{
			SkeletonEditor->SetSelectedBone(FName(Component->GetName()), ESelectInfo::Type::Direct);
		}
		SkeletonEditor->Refresh();
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
	Collector.AddReferencedObject(SkeletalMesh);
}
