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

	/** Data flow object owner */
	TObjectPtr<UObject> DataflowOwner = nullptr;

	/** Data flow asset that we will edit */
	TObjectPtr<UDataflow> DataflowAsset = nullptr;

	/** Data flow terminal path for evaluation */
	FString DataflowTerminal = "";

	/** Data flow skeletal mesh*/
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** Data flow skeleton*/
	TObjectPtr<USkeleton> Skeleton = nullptr;

	/** Animation asset to be used to preview simulation */
	TObjectPtr<UAnimationAsset> AnimationAsset;

	/**  Engine context to be used for dataflow evaluation */
	TSharedPtr<Dataflow::FEngineContext> DataflowContext = nullptr;

	/** Last data flow evaluated node time stamp */
	Dataflow::FTimestamp LastModifiedTimestamp = Dataflow::FTimestamp::Invalid;

	/** Boolean to check if the skelmesh is valid*/
	bool bHasValidSkeletalMesh = false;
};
