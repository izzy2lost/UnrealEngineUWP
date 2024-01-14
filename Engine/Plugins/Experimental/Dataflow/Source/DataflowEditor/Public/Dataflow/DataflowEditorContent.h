// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowPatternVertexType.h"
#include "Dataflow/DataflowEdNode.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Templates/SharedPointer.h"

#include "DataflowEditorContent.generated.h"

class FDataflowEditorToolkit;
class UDataflow;
class USkeletalMesh;
class USkeleton;
class UAnimationAsset;
class SDataflowGraphEditor;
struct FManagedArrayCollection;


UCLASS()
class UDataflowEditorContextObject : public UObject
{
	GENERATED_BODY()
public:

	// @todo(brice) : Is this even needed?
	void SetDataflowGraphEditor(TWeakPtr<SDataflowGraphEditor> InEditor) {DataflowGraphEditor = InEditor;}

	/** Selection Collection Access */
	void SetPrimarySelectedNode(TObjectPtr<UDataflowEdNode> InSelectedNode) { PrimarySelectedNode = InSelectedNode; }
	const TObjectPtr<UDataflowEdNode> GetPrimarySelectedNode() const { return PrimarySelectedNode; }

	/** Render Collection used to generate the DynamicMesh3D on the PrimarySelection */
	void SetPrimaryRenderCollection(TSharedPtr<FManagedArrayCollection> InCollection) { PrimaryRenderCollection = InCollection; }
	const TSharedPtr<const FManagedArrayCollection> GetPrimaryRenderCollection() const { return PrimaryRenderCollection; }

	/** ViewMode Access */
	void SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InMode) {ConstructionViewMode = InMode;}
	Dataflow::EDataflowPatternVertexType GetConstructionViewMode() const { return ConstructionViewMode; }

	/** Get a single selected node of the specified type. Return nullptr if the specified node is not selected, or if multiple nodes are selected*/
	template<typename NodeType>
	NodeType* GetPrimarySelectedNodeOfType() const 
	{
		if (PrimarySelectedNode && PrimarySelectedNode->GetDataflowNode()) 
		{
			return PrimarySelectedNode->GetDataflowNode()->AsType<NodeType>();
		}
		return nullptr;
	}

protected:

	TSharedPtr<FManagedArrayCollection> PrimaryRenderCollection = nullptr;
	TObjectPtr<UDataflowEdNode> PrimarySelectedNode = nullptr;
	TWeakPtr<SDataflowGraphEditor> DataflowGraphEditor;
	Dataflow::EDataflowPatternVertexType ConstructionViewMode = Dataflow::EDataflowPatternVertexType::Sim3D;
};


/** 
 * Dataflow datas that will be used within the editor classes to evaluate the graph
 */
UCLASS()
class UDataflowEditorContent : public UDataflowEditorContextObject
{
	GENERATED_BODY()

public:
	UDataflowEditorContent();

	/** Check if the datas flow datas are valid */
	bool IsValid() const { return DataflowOwner && DataflowAsset;}

	/** Check if the datas flow datas are dirty and need to be re-renmdered */
	bool IsDirty() const { return bIsDirty; }
	void SetIsDirty(bool InDirty) { bIsDirty = InDirty; }

	//bool HasCompleteSkeletalSetup() const { return SkeletalMesh != nullptr && Skeleton!=nullptr; }
	void SetSkeletalMesh(TObjectPtr<USkeletalMesh> InMesh) { SkeletalMesh = InMesh; bIsDirty = true; }
	TObjectPtr<USkeletalMesh>& GetSkeletalMesh() { return SkeletalMesh; }
	const TObjectPtr<USkeletalMesh>& GetSkeletalMesh() const { return SkeletalMesh; }

	/** Data flow object owner */
	void SetDataflowOwner(TObjectPtr<UObject> InOwner) { DataflowOwner = InOwner; bIsDirty = true;}
	TObjectPtr<UObject>& GetDataflowOwner() { return DataflowOwner; }
	const TObjectPtr<UObject>& GetDataflowOwner() const { return DataflowOwner; }

	/** Data flow asset that we will edit */
	void SetDataflowAsset(TObjectPtr<UDataflow> InAsset) { DataflowAsset = InAsset;  bIsDirty = true;}
	TObjectPtr<UDataflow>& GetDataflowAsset() { return DataflowAsset; }
	const TObjectPtr<UDataflow>& GetDataflowAsset() const { return DataflowAsset; }

	/** Data flow terminal path for evaluation */
	void SetDataflowTerminal(FString InPath) { DataflowTerminal = InPath;  bIsDirty = true;}
	FString GetDataflowTerminal() const { return DataflowTerminal; }

	/** Data flow skeleton*/
	void SetSkeleton(TObjectPtr<USkeleton> InSkeleton) { Skeleton = InSkeleton;  bIsDirty = true;}
	TObjectPtr<USkeleton>& GetSkeleton() { return Skeleton; }
	const TObjectPtr<USkeleton>& GetSkeleton() const { return Skeleton; }

	/** Animation asset to be used to preview simulation */
	void SetAnimationAsset(TObjectPtr<UAnimationAsset> InAnimation) { AnimationAsset = InAnimation; bIsDirty = true;}
	TObjectPtr<UAnimationAsset>& GetAnimationAsset() { return AnimationAsset; }
	const TObjectPtr<UAnimationAsset>& GetAnimationAsset() const { return AnimationAsset; }

	/**  Engine context to be used for dataflow evaluation */
	void SetDataflowContext(TSharedPtr<Dataflow::FEngineContext> InContext) { DataflowContext = InContext; bIsDirty = true;}
	TSharedPtr<Dataflow::FEngineContext>& GetDataflowContext() { return DataflowContext; }
	const TSharedPtr<Dataflow::FEngineContext>& GetDataflowContext() const { return DataflowContext; }

	/** Last data flow evaluated node time stamp */
	void SetLastModifiedTimestamp(Dataflow::FTimestamp InTimestamp) { LastModifiedTimestamp = InTimestamp; bIsDirty = true;}
	Dataflow::FTimestamp& GetLastModifiedTimestamp() { return LastModifiedTimestamp; }
	const Dataflow::FTimestamp& GetLastModifiedTimestamp() const { return LastModifiedTimestamp; }
	

private:
	/** Data flow object owner */
	TObjectPtr<UObject> DataflowOwner = nullptr;

	/** Data flow asset that we will edit */
	TObjectPtr<UDataflow> DataflowAsset = nullptr;

	/** Data flow skeletal mesh*/
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** Data flow skeleton*/
	TObjectPtr<USkeleton> Skeleton = nullptr;

	/** Data flow terminal path for evaluation */
	FString DataflowTerminal = "";

	/** Animation asset to be used to preview simulation */
	TObjectPtr<UAnimationAsset> AnimationAsset = nullptr;

	/**  Engine context to be used for dataflow evaluation */
	TSharedPtr<Dataflow::FEngineContext> DataflowContext = nullptr;

	/** Last data flow evaluated node time stamp */
	Dataflow::FTimestamp LastModifiedTimestamp = Dataflow::FTimestamp::Invalid;

	bool bIsDirty = true;

};
