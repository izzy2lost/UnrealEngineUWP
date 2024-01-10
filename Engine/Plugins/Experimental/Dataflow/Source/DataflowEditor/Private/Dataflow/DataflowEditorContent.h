// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Templates/SharedPointer.h"

#include "DataflowEditorContent.generated.h"

class FDataflowEditorToolkit;
class UDataflow;
class USkeletalMesh;
class USkeleton;
class UAnimationAsset;

/** 
 * Dataflow datas that will be used within the editor classes to evaluate the graph
 */
UCLASS()
class UDataflowEditorContent : public UObject
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
