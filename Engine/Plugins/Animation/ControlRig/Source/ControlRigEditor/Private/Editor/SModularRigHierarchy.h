// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Editor/SModularRigHierarchyTreeView.h"
#include "ControlRigBlueprint.h"
#include "DragAndDrop/GraphNodeDragDropOp.h"
#include "Editor/RigVMEditor.h"

class SModularRigHierarchy;
class FControlRigEditor;
class SSearchBox;
class FUICommandList;
class URigVMBlueprint;
class UControlRig;
struct FAssetData;
class FMenuBuilder;
class UToolMenu;
struct FToolMenuContext;

class FModuleRigHierarchyDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FModuleRigHierarchyDragDropOp, FDragDropOperation)

	static TSharedRef<FModuleRigHierarchyDragDropOp> New(const TArray<FString>& InElements);

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	/** @return true if this drag operation contains property paths */
	bool HasElements() const
	{
		return Elements.Num() > 0;
	}

	/** @return The property paths from this drag operation */
	const TArray<FString>& GetElements() const
	{
		return Elements;
	}

	FString GetJoinedElementNames() const;

private:

	/** Data for the property paths this item represents */
	TArray<FString> Elements;
};

/** Widget allowing editing of a control rig's structure */
class SModularRigHierarchy : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SModularRigHierarchy) {}
	SLATE_END_ARGS()

	~SModularRigHierarchy();

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	FControlRigEditor* GetControlRigEditor() const
	{
		if(ControlRigEditor.IsValid())
		{
			return ControlRigEditor.Pin().Get();
		}
		return nullptr;
	}

private:

	void OnEditorClose(const FRigVMEditor* InEditor, URigVMBlueprint* InBlueprint);

	/** Bind commands that this widget handles */
	void BindCommands();

	/** SWidget interface */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Rebuild the tree view */
	void RefreshTreeView(bool bRebuildContent = true);

	/** Return all selected keys */
	TArray<FString> GetSelectedKeys() const;

	/** Create a new item */
	void HandleNewItem();
	void HandleNewItem(UClass* InClass, const FString& InParentPath);

	/** Rename item */
	bool CanRenameModule() const;
	void HandleRenameModule();
	FName HandleRenameModule(const FString& InOldPath, const FName& InNewName);
	bool HandleVerifyNameChanged(const FString& InOldPath, const FName& InNewName, FText& OutErrorMessage);

	/** Delete items */
	void HandleDeleteModules();
	void HandleDeleteModules(const TArray<FString>& InPaths);

	/** Reparent items */
	void HandleReparentModules(const TArray<FString>& InPaths, const FString& InParentPath);

	/** Set Selection Changed */
	void OnSelectionChanged(TSharedPtr<FModularRigTreeElement> Selection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SWidget > CreateContextMenuWidget();
	void OnItemClicked(TSharedPtr<FModularRigTreeElement> InItem);
	void OnItemDoubleClicked(TSharedPtr<FModularRigTreeElement> InItem);
	
	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// reply to a drag operation
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	// reply to a drop operation on item
	TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem);
	FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem);

	// SWidget Overrides
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	static const FName ContextMenuName;
	static void CreateContextMenu();
	UToolMenu* GetContextMenu();
	TSharedPtr<FUICommandList> GetContextMenuCommands() const;
	
	/** Our owning control rig editor */
	TWeakPtr<FControlRigEditor> ControlRigEditor;

	/** Tree view widget */
	TSharedPtr<SModularRigHierarchyTreeView> TreeView;

	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UModularRig> ControlRigBeingDebuggedPtr;
	
	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	bool IsSingleSelected() const;
	
	UModularRig* GetHierarchy() const;
	UModularRig* GetDefaultHierarchy() const;
	const UModularRig* GetHierarchyForTreeView() const { return GetHierarchy(); }
	FName CreateUniqueName(const FName& InBasePath) const;
	void OnRequestDetailsInspection(const FString& InKey);
	void ClearDetailPanel() const;

	void HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint);
	void HandleSetObjectBeingDebugged(UObject* InObject);

public:

	friend class FModularRigTreeElement;
	friend class SModularRigHierarchyItem;
	friend class UControlRigBlueprintEditorLibrary;
};

