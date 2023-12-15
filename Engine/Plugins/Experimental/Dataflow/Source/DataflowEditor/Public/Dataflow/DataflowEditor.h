// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditor.h"

#include "Dataflow/DataflowEngine.h"
#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowAssetFactory.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowSchema.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Templates/SharedPointer.h"

#include "DataflowEditor.generated.h"

class FDataflowEditorToolkit;
class USkeletalMesh;
class USkeleton;
class UAnimationAsset;

/** 
 * Dataflow datas that will be used within the editor classes to evaluate the graph
 */
struct DATAFLOWEDITOR_API FDataflowEditorDatas
{
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
	Dataflow::FTimestamp LastNodeTimestamp = Dataflow::FTimestamp::Invalid;

	/** Boolean to check if the skelmesh is valid*/
	bool bHasValidSkeletalMesh = false;
};

/** 
 * The actual asset editor class doesn't have that much in it, intentionally. 
 * 
 * Our current asset editor guidelines ask us to place as little business logic as possible
 * into the class, instead putting as much of the non-UI code into the subsystem as possible,
 * and the UI code into the toolkit (which this class owns).
 *
 * However, since we're using a mode and the Interactive Tools Framework, a lot of our business logic
 * ends up inside the mode and the tools, not the subsystem. The front-facing code is mostly in
 * the asset editor toolkit, though the mode toolkit has most of the things that deal with the toolbar
 * on the left.
 */

UCLASS()
class DATAFLOWEDITOR_API UDataflowEditor : public UBaseCharacterFXEditor
{
	GENERATED_BODY()

public:

	// UBaseCharacterFXEditor interface
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	virtual void Initialize(const TArray<TObjectPtr<UObject>>& InObjects) override;

private :

	friend class FDataflowEditorToolkit;
	
	// Dataflow editor is the owner of the object list to edit/process and the dataflow mode
	// is the one holding the dynamic mesh components to be rendered in the viewport
	// It is why the data flow asset/owner/skelmesh have been added here. Could be added
	// in the subsystem if necessary
	FDataflowEditorDatas DataflowDatas;
};

DECLARE_LOG_CATEGORY_EXTERN(LogDataflowEditor, Log, All);