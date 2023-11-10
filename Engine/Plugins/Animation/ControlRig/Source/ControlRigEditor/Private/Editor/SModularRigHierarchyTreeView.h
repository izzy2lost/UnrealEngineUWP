// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STreeView.h"
#include "ModularRig.h"

class SSearchBox;
class SModularRigHierarchyTreeView;
class SModularRigHierarchyItem;
class FModularRigTreeElement;


DECLARE_DELEGATE_RetVal(const UModularRig*, FOnGetModularRigTreeHierarchy);
DECLARE_DELEGATE_OneParam(FOnModularRigTreeRequestDetailsInspection, const FString&);
DECLARE_DELEGATE_RetVal_TwoParams(FName, FOnModularRigTreeRenameElement, const FString& /*OldPath*/, const FName& /*NewName*/);
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnModularRigTreeVerifyElementNameChanged, const FString& /*OldPath*/, const FName& /*NewName*/, FText& /*OutErrorMessage*/);


typedef STreeView<TSharedPtr<FModularRigTreeElement>>::FOnMouseButtonClick FOnModularRigTreeMouseButtonClick;
typedef STreeView<TSharedPtr<FModularRigTreeElement>>::FOnMouseButtonDoubleClick FOnModularRigTreeMouseButtonDoubleClick;
typedef STableRow<TSharedPtr<FModularRigTreeElement>>::FOnCanAcceptDrop FOnModularRigTreeCanAcceptDrop;
typedef STableRow<TSharedPtr<FModularRigTreeElement>>::FOnAcceptDrop FOnModularRigTreeAcceptDrop;

struct CONTROLRIGEDITOR_API FModularRigTreeDelegates
{
	FOnGetModularRigTreeHierarchy OnGetHierarchy;
	FOnModularRigTreeMouseButtonClick OnMouseButtonClick;
	FOnModularRigTreeMouseButtonDoubleClick OnMouseButtonDoubleClick;
	FOnModularRigTreeCanAcceptDrop OnCanAcceptDrop;
	FOnModularRigTreeAcceptDrop OnAcceptDrop;
	FOnContextMenuOpening OnContextMenuOpening;
	FOnModularRigTreeRequestDetailsInspection OnRequestDetailsInspection;
	FOnModularRigTreeRenameElement OnRenameElement;
	FOnModularRigTreeVerifyElementNameChanged OnVerifyModuleNameChanged;
	
	FModularRigTreeDelegates()
	{
	}

	const UModularRig* GetHierarchy() const
	{
		if(OnGetHierarchy.IsBound())
		{
			return OnGetHierarchy.Execute();
		}
		return nullptr;
	}

	FName HandleRenameElement(const FString& OldPath, const FName& NewName) const
	{
		if(OnRenameElement.IsBound())
		{
			return OnRenameElement.Execute(OldPath, NewName);
		}
		return *OldPath;
	}

	bool HandleVerifyElementNameChanged(const FString& OldPath, const FName& NewName, FText& OutErrorMessage) const
	{
		if(OnVerifyModuleNameChanged.IsBound())
		{
			return OnVerifyModuleNameChanged.Execute(OldPath, NewName, OutErrorMessage);
		}
		return false;
	}
};


/** An item in the tree */
class FModularRigTreeElement : public TSharedFromThis<FModularRigTreeElement>
{
public:
	FModularRigTreeElement(const FString& InKey, TWeakPtr<SModularRigHierarchyTreeView> InTreeView, bool InSupportsRename);

public:
	/** Element Data to display */
	FString Key;
	FName ShortName;
	TArray<TSharedPtr<FModularRigTreeElement>> Children;

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigHierarchyTreeView> InTreeView, bool bPinned);

	void RequestRename();

	void RefreshDisplaySettings(const UModularRig* InHierarchy);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;

	/** The brush to use when rendering an icon */
	const FSlateBrush* IconBrush;

	/** The color to use when rendering an icon */
	FSlateColor IconColor;

	/** The color to use when rendering the label text */
	FSlateColor TextColor;
};

class SModularRigHierarchyItem : public STableRow<TSharedPtr<FModularRigTreeElement>>
{
public:
	
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigHierarchyTreeView> InTreeView, bool bPinned);

	void OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);
	static TPair<const FSlateBrush*, FSlateColor> GetBrushForElementType(const UModularRig* InHierarchy, const FString& InKey);
	static FLinearColor GetColorForControlType(ERigControlType InControlType, UEnum* InControlEnum);

private:
	TWeakPtr<FModularRigTreeElement> WeakRigTreeElement;
 	FModularRigTreeDelegates Delegates;

	FText GetName(bool bUseShortName) const;
	FText GetItemTooltip() const;

	static TMap<FSoftObjectPath, FSlateBrush> IconPathToBrush;

	friend class SModularRigHierarchyTreeView; 
};

class SModularRigHierarchyTreeView : public STreeView<TSharedPtr<FModularRigTreeElement>>
{
public:

	SLATE_BEGIN_ARGS(SModularRigHierarchyTreeView)
		: _AutoScrollEnabled(false)
	{}
		SLATE_ARGUMENT(FModularRigTreeDelegates, RigTreeDelegates)
		SLATE_ARGUMENT(bool, AutoScrollEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SModularRigHierarchyTreeView() {}

	/** Performs auto scroll */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	bool bRequestRenameSelected = false;

	/** Save a snapshot of the internal map that tracks item expansion before tree reconstruction */
	void SaveAndClearSparseItemInfos()
	{
		// Only save the info if there is something to save (do not overwrite info with an empty map)
		if (!SparseItemInfos.IsEmpty())
		{
			OldSparseItemInfos = SparseItemInfos;
		}
		ClearExpandedItems();
	}

	/** Restore the expansion infos map from the saved snapshot after tree reconstruction */
	void RestoreSparseItemInfos(TSharedPtr<FModularRigTreeElement> ItemPtr)
	{
		for (const auto& Pair : OldSparseItemInfos)
		{
			if (Pair.Key->Key == ItemPtr->Key)
			{
				// the SparseItemInfos now reference the new element, but keep the same expansion state
				SparseItemInfos.Add(ItemPtr, Pair.Value);
				break;
			}
		}
	}


	TSharedPtr<FModularRigTreeElement> FindElement(const FString& InElementKey);
	static TSharedPtr<FModularRigTreeElement> FindElement(const FString& InElementKey, TSharedPtr<FModularRigTreeElement> CurrentItem);
	bool AddElement(FString InKey, FString InParentKey = FString());
	bool AddElement(const FRigModuleInstance* InElement);
	void AddSpacerElement();
	bool ReparentElement(const FString InKey, const FString InParentKey);
	void RefreshTreeView(bool bRebuildContent = true);
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FModularRigTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bPinned);
	void HandleGetChildrenForTree(TSharedPtr<FModularRigTreeElement> InItem, TArray<TSharedPtr<FModularRigTreeElement>>& OutChildren);

	TArray<FString> GetSelectedKeys() const;
	void SetSelection(const TArray<TSharedPtr<FModularRigTreeElement>>& InSelection);
	const TArray<TSharedPtr<FModularRigTreeElement>>& GetRootElements() const { return RootElements; }
	FModularRigTreeDelegates& GetRigTreeDelegates() { return Delegates; }

	/** Given a position, return the item under that position. If nothing is there, return null. */
	const TSharedPtr<FModularRigTreeElement>* FindItemAtPosition(FVector2D InScreenSpacePosition) const;

private:

	/** A temporary snapshot of the SparseItemInfos in STreeView, used during RefreshTreeView() */
	TSparseItemMap OldSparseItemInfos;

	/** Backing array for tree view */
	TArray<TSharedPtr<FModularRigTreeElement>> RootElements;
	
	/** A map for looking up items based on their key */
	TMap<FString, TSharedPtr<FModularRigTreeElement>> ElementMap;

	/** A map for looking up a parent based on their key */
	TMap<FString, FString> ParentMap;

	FModularRigTreeDelegates Delegates;

	bool bAutoScrollEnabled;
	FVector2D LastMousePosition;
	double TimeAtMousePosition;

	friend class SModularRigHierarchy;
};

class SSearchableModularRigHierarchyTreeView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSearchableModularRigHierarchyTreeView) {}
		SLATE_ARGUMENT(FModularRigTreeDelegates, RigTreeDelegates)
		SLATE_ARGUMENT(FText, InitialFilterText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSearchableModularRigHierarchyTreeView() {}
	TSharedRef<SModularRigHierarchyTreeView> GetTreeView() const { return TreeView.ToSharedRef(); }

private:

	TSharedPtr<SModularRigHierarchyTreeView> TreeView;
};
