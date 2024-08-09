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
class UDataflowBaseContent;

typedef TSet<class UObject*> FGraphPanelSelectionSet;

/*
* FDataflowEditorCommandsImpl
* 
*/
class DATAFLOWEDITOR_API FDataflowEditorCommandsImpl : public TBaseCharacterFXEditorCommands<FDataflowEditorCommandsImpl>
{
public:

	FDataflowEditorCommandsImpl();

	// Note: We need to explicitly disable warnings on these constructors/operators for clang to be happy with deprecated variables
	// (See Confluence page "How to Deprecate Code in UE > How to deprecate variables inside a UStruct")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~FDataflowEditorCommandsImpl() = default;
	FDataflowEditorCommandsImpl(const FDataflowEditorCommandsImpl&) = default;
	FDataflowEditorCommandsImpl(FDataflowEditorCommandsImpl&&) = default;
	FDataflowEditorCommandsImpl& operator=(const FDataflowEditorCommandsImpl&) = default;
	FDataflowEditorCommandsImpl& operator=(FDataflowEditorCommandsImpl&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// TBaseCharacterFXEditorCommands<> interface
	 virtual void RegisterCommands() override;

	// TInteractiveToolCommands<>
	 virtual void GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) override;

	/**
	* Add or remove commands relevant to Tool to the given UICommandList.
	* Call this when the active tool changes (eg on ToolManager.OnToolStarted / OnToolEnded)
	* @param bUnbind if true, commands are removed, otherwise added
	*/
	 static void UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind = false);
	
	TSharedPtr<FUICommandInfo> EvaluateNode;
	TSharedPtr<FUICommandInfo> CreateComment;
	TSharedPtr<FUICommandInfo> ToggleEnabledState;
	TSharedPtr<FUICommandInfo> ToggleObjectSelection;
	TSharedPtr<FUICommandInfo> ToggleFaceSelection;
	TSharedPtr<FUICommandInfo> ToggleVertexSelection;
	TSharedPtr<FUICommandInfo> AddOptionPin;
	TSharedPtr<FUICommandInfo> RemoveOptionPin;
	TSharedPtr<FUICommandInfo> ZoomToFitGraph;

	TMap<FName, TSharedPtr<FUICommandInfo>> SetConstructionViewModeCommands;

	TMap< FName, TSharedPtr<FUICommandInfo> > CreateNodesMap;

	UE_DEPRECATED(5.5, "Dataflow Tool commands are now stored in FDataflowToolRegistry")
	const static FString BeginWeightMapPaintToolIdentifier;
	UE_DEPRECATED(5.5, "Dataflow Tool commands are now stored in FDataflowToolRegistry")
	TSharedPtr<FUICommandInfo> BeginWeightMapPaintTool;
	
	const static FString AddWeightMapNodeIdentifier;
	TSharedPtr<FUICommandInfo> AddWeightMapNode;

	const static FString RebuildSimulationSceneIdentifier;
	TSharedPtr<FUICommandInfo> RebuildSimulationScene;
	
	const static FString PauseSimulationSceneIdentifier;
	TSharedPtr<FUICommandInfo> PauseSimulationScene;

	const static FString StartSimulationSceneIdentifier;
	TSharedPtr<FUICommandInfo> StartSimulationScene;
	
	const static FString StepSimulationSceneIdentifier;
	TSharedPtr<FUICommandInfo> StepSimulationScene;
	
	// @todo(brice) Remove Example Tools
	//const static FString BeginAttributeEditorToolIdentifier;
	//TSharedPtr<FUICommandInfo> BeginAttributeEditorTool;
	//
	//const static FString BeginMeshSelectionToolIdentifier;
	//TSharedPtr<FUICommandInfo> BeginMeshSelectionTool;
};

//@todo(brice) Merge this into the above class
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
	* OnNotifyPropertyPreChange
	*/
	static void OnNotifyPropertyPreChange(TSharedPtr<IStructureDetailsView> PropertiesEditor, UDataflow* Graph, class FEditPropertyChain* PropertyAboutToChange);

	/*
	*  OnPropertyValueChanged
	*/
	static void OnPropertyValueChanged(UDataflow* Graph, TSharedPtr<Dataflow::FEngineContext>& Context, Dataflow::FTimestamp& OutLastNodeTimestamp, const FPropertyChangedEvent& PropertyChangedEvent, const TSet<TObjectPtr<UObject>>& NewSelection = TSet<TObjectPtr<UObject>>());
	static void OnAssetPropertyValueChanged(TObjectPtr<UDataflowBaseContent> Content, const FPropertyChangedEvent& PropertyChangedEvent);

	/*
	*  OnSelectedNodesChanged
	*/
	static void OnSelectedNodesChanged(TSharedPtr<IStructureDetailsView> PropertiesEditor, UObject* Asset, UDataflow* Graph, const TSet<TObjectPtr<UObject>>& NewSelection);

	/*
	*  ToggleEnabledState
	*/
	static void ToggleEnabledState(UDataflow* Graph);

	/*
	*  DuplicateNodes
	*/
	static void DuplicateNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, const FGraphPanelSelectionSet& SelectedNodes);

	/*
	*  CopyNodes
	*/
	static void CopyNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, const FGraphPanelSelectionSet& SelectedNodes);

	/*
	*  PasteSelectedNodes
	*/
	static void PasteNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor);

	/*
	*  RenameNode
	*/
	static void RenameNode(const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, UEdGraphNode* EdNode);
};

