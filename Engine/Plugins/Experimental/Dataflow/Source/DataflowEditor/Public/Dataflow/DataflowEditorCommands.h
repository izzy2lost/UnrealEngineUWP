// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "BaseCharacterFXEditorCommands.h"
#include "Styling/AppStyle.h"

class FDragDropEvent;
struct FDataflowOutput;
struct FGeometry;
class IStructureDetailsView;
class UDataflow;
class UDataflowEdNode;
struct FDataflowNode;
class UEdGraphNode;
class SDataflowGraphEditor;
class UDataflowEditorContent;

typedef TSet<class UObject*> FGraphPanelSelectionSet;

/*
* FDataflowEditorCommandsImpl
* 
*/
class DATAFLOWEDITOR_API FDataflowEditorCommandsImpl : public TBaseCharacterFXEditorCommands<FDataflowEditorCommandsImpl>
{
public:

	FDataflowEditorCommandsImpl();

	// TBaseCharacterFXEditorCommands<> interface
	 virtual void RegisterCommands() override;

	// TInteractiveToolCommands<>
	 virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;

	TSharedPtr< FUICommandInfo > EvaluateNode;
	TSharedPtr< FUICommandInfo > CreateComment;
	TSharedPtr< FUICommandInfo > ToggleEnabledState;
	TSharedPtr<FUICommandInfo> ToggleObjectSelection;
	TSharedPtr<FUICommandInfo> ToggleFaceSelection;
	TSharedPtr<FUICommandInfo> ToggleVertexSelection;
	TSharedPtr< FUICommandInfo > AddOptionPin;
	TSharedPtr< FUICommandInfo > RemoveOptionPin;
	TSharedPtr< FUICommandInfo > ZoomToFitGraph;

	TMap< FName, TSharedPtr<FUICommandInfo> > CreateNodesMap;

	// Temporary, just to test the editor, will be replace by the selection tool
	const static FString BeginAttributeEditorToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginAttributeEditorTool;

	const static FString BeginMeshSelectionToolIdentifier;
	TSharedPtr<FUICommandInfo> BeginMeshSelectionTool;
};

class DATAFLOWEDITOR_API FDataflowEditorCommands
{
public:
	typedef TFunction<void(FDataflowNode*, FDataflowOutput*)> FGraphEvaluationCallback;
	typedef TFunction<void(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)> FOnDragDropEventCallback;

	static void Register();
	static void Unregister();

	static const FDataflowEditorCommandsImpl& Get();

	/*
	*  EvaluateSelectedNodes
	*/
	static void EvaluateSelectedNodes(const FGraphPanelSelectionSet& SelectedNodes, FGraphEvaluationCallback);

	/*
	* EvaluateGraph
	*/
	static void EvaluateNode(Dataflow::FContext& Context, Dataflow::FTimestamp& OutLastNodeTimestamp,
		const UDataflow* Dataflow, const FDataflowNode* Node = nullptr, const FDataflowOutput* Out = nullptr, 
		FString NodeName = FString()); // @todo(Dataflow) deprecate  

	static void EvaluateTerminalNode(Dataflow::FContext& Context, Dataflow::FTimestamp& OutLastNodeTimestamp,
		const UDataflow* Dataflow, const FDataflowNode* Node = nullptr, const FDataflowOutput* Out = nullptr,
		UObject* InAsset = nullptr, FString NodeName = FString());

	/*
	*  DeleteNodes
	*/
	static void DeleteNodes(UDataflow* Graph, const FGraphPanelSelectionSet& SelectedNodes);

	/*
	* OnNodeVerifyTitleCommit
	*/
	static bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage);

	/*
	* OnNodeTitleCommitted
	*/
	static void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode);

	/*
	*  OnPropertyValueChanged
	*/
	static void OnPropertyValueChanged(UDataflow* Graph, TSharedPtr<Dataflow::FEngineContext>& Context, Dataflow::FTimestamp& OutLastNodeTimestamp, const FPropertyChangedEvent& PropertyChangedEvent, const TSet<UObject*>& SelectedNodes = TSet<UObject*>());
	static void OnAssetPropertyValueChanged(TObjectPtr<UDataflowEditorContent> Content, const FPropertyChangedEvent& PropertyChangedEvent);

	/*
	*  OnSelectedNodesChanged
	*/
	static void OnSelectedNodesChanged(TSharedPtr<IStructureDetailsView> PropertiesEditor, UObject* Asset, UDataflow* Graph, const TSet<UObject*>& NewSelection);

	/*
	*  ToggleEnabledState
	*/
	static void ToggleEnabledState(UDataflow* Graph);

	/*
	*  DuplicateNodes
	*/
	static void DuplicateNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, const FGraphPanelSelectionSet& SelectedNodes);



};
