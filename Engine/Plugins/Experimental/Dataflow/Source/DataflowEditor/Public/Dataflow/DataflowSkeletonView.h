// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowView.h"

class UDataflowEditor;
class ISkeletonTree;
class ISkeletonTreeItem;
class USkeleton;
class USkeletalMesh;
class UDataflowEdNode;

/**
*
* Class to handle the SelectionView widget
*
*/
class FDataflowSkeletonView : public FDataflowNodeView
{
public:
	FDataflowSkeletonView(UDataflowEditor* InDataflowEditor = nullptr);
	~FDataflowSkeletonView();

	/** Set the Skeleton Tree*/
	void SetSkeletonEditor(TSharedPtr<ISkeletonTree>& InSkeletonTree);

	/** Set selection types*/
	virtual void SetSupportedOutputTypes() override;

	/** Skeleton Access */
	USkeleton* GetSkeleton();

	/** Update Data*/
	void SetSkeleton(USkeleton* Skeleton);

	/** Update the view */
	virtual void UpdateViewData() override;

	/** Add GC managed objects*/
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Selection View Callbacks */
	void SkeletonViewSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo);

private:
	UDataflowEditor* DataflowEditor = nullptr;

	TSharedPtr<ISkeletonTree> SkeletonEditor;


	/* Skeletal Mesh in the SkeletalViewer*/
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	TObjectPtr<UDataflowEdNode> SelectedNode = nullptr;

	/* Rempping from the selected node to the SkeletalMesh*/
	TArray<int32> CollectionIndexRemap;
};

