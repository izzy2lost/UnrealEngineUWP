// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "IDetailCustomization.h"
#include "Widgets/Views/STreeView.h"

class FCollectionCustomizationTreeElement;
class UMovieGraphCollection;
class UMovieGraphConditionGroup;
class UMovieGraphConditionGroupQueryBase;

/**
 * A widget which indents the specified number of indent levels, with the appropriate styling for the collection tree view.
 */
class SMovieGraphCollectionTreeViewIndent final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeViewIndent) { }
		SLATE_ATTRIBUTE(int, NumIndents)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

/** The drag-drop operator for use within the collections tree view. */
class FMovieGraphCollectionTreeDragOp final : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(SMovieGraphCollectionTreeDragOp, FDragDropOperation)

	static TSharedRef<FMovieGraphCollectionTreeDragOp> New(const TSharedPtr<FCollectionCustomizationTreeElement>& InElement);

public:
	/** The tree element associated with this drag-drop operation. */
	TSharedPtr<FCollectionCustomizationTreeElement> Element;

	/** Whether the drag-drop operation is currently valid. */
	bool bIsValidOperation = false;
};

/** A tree view specifically for viewing the contents of a UMovieGraphCollection object. */
class SMovieGraphCollectionTreeView final : public STreeView<TSharedPtr<FCollectionCustomizationTreeElement>>
{
public:
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeView) { }
		SLATE_ARGUMENT(TWeakPtr<IDetailLayoutBuilder>, DetailBuilder)
		SLATE_ARGUMENT(TWeakObjectPtr<UMovieGraphCollection>, Collection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Called when an row in the tree view expands/collapses. Caches the expansion state of the tree so it can be restored at a later time. */
	void OnExpansionChanged(TSharedPtr<FCollectionCustomizationTreeElement> InElement, bool bInExpanded);

	/** Gets the children of the specified tree element. */
	void GetChildrenForTree(TSharedPtr<FCollectionCustomizationTreeElement> InItem, TArray<TSharedPtr<FCollectionCustomizationTreeElement>>& OutChildren);

	/** Restores the cached expansion state recursively through all tree elements. */
	void RestoreExpansionStateRecursive(const TSharedPtr<FCollectionCustomizationTreeElement>& InElement);

	/** Generates a tree row suitable for the type of tree element provided. */
	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FCollectionCustomizationTreeElement> InTreeElement, const TSharedRef<STableViewBase>& OwnerTable);

	/** Adds the root tree element. The root is not visible, but serves as the single entry point into the tree. */
	void AddRootElement();

	/** Adds condition group tree elements, one for each condition group in the provided collection. */
	void AddRootConditionGroupElements(const TObjectPtr<UMovieGraphCollection>& InCollection) const;

	/** Adds a new condition group element associated with the provided condition group. */
	void AddNewConditionGroupElement(UMovieGraphConditionGroup* InConditionGroup) const;

	/** Adds a new, empty condition group element to the tree, and adds a corresponding empty condition group to the underlying collection. */
	void AddConditionGroup();

	/** Removes the condition group associated with the provided element. The condition group is also removed from the underlying collection. */
	void RemoveConditionGroup(const TWeakPtr<FCollectionCustomizationTreeElement>& InConditionGroupElement);

	/** Refreshes the tree view while also taking care of restoring cached expansion state. Should always be called instead of RequestTreeRefresh(). */
	void RefreshView();

private:
	/** The collection associated with this tree view. */
	TWeakObjectPtr<UMovieGraphCollection> Collection = nullptr;
	
	/** The root-most elements in the tree. */
	TArray<TSharedPtr<FCollectionCustomizationTreeElement>> RootElements;
	
	/** The hashes of elements that are currently expanded in the tree. */
	TSet<uint32> ExpandedElements;

	/** The detail builder associated with this tree. Some items in the tree need to add widgets that can be fetched from the detail builder. */
	TWeakPtr<IDetailLayoutBuilder> DetailBuilder;
};

/**
 * A widget which controls the op type for a condition group or condition group query.
 */
class SMovieGraphCollectionTreeOpTypeWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeOpTypeWidget) { }
		SLATE_ATTRIBUTE(bool, IsConditionGroup)
		SLATE_ATTRIBUTE(TWeakPtr<FCollectionCustomizationTreeElement>, WeakTreeElement)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Whether this widget is associated with a condition group (else a condition group query). */
	bool bIsConditionGroup = false;

	/** The tree element this widget is being displayed in. */
	TWeakPtr<FCollectionCustomizationTreeElement> WeakTreeElement = nullptr;

private:
	/** Gets the names of all op types available. */
	template<typename T>
	static TArray<FName>* GetOpTypes();

	/** Gets a widget (icon + name) representing the given op. */
	TSharedRef<SWidget> GetOpTypeContents(const FName& InOpName, const bool bIsConditionGroupOp) const;

	/** Whether this widget is currently enabled. */
	bool IsWidgetEnabled() const;

	/** Sets the op type for the condition group or condition group query. */
	void SetOpType(const FName InNewOpType, ESelectInfo::Type SelectInfo) const;

	/** Gets the currently-assigned op type for the condition group. */
	FName GetCurrentConditionGroupOpType() const;

	/** Gets the currently-assigned op type for the condition group query. */
	FName GetCurrentConditionGroupQueryOpType() const;
};

/**
 * A widget which displays the "Add" menu for a condition group query, allowing the user to add content to the query (like an actor). The appearance
 * of the widget and behavior of the menu is largely delegated to the query itself.
 */
class SMovieGraphCollectionTreeAddQueryContentWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeAddQueryContentWidget) { }
		SLATE_ATTRIBUTE(TWeakPtr<FCollectionCustomizationTreeElement>, WeakTreeElement)
		SLATE_ATTRIBUTE(TWeakPtr<SMovieGraphCollectionTreeView>, WeakTreeView)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

/**
 * A ComboBox widget which displays the available condition group query types and updates the condition group when one is selected.
 */
class SMovieGraphCollectionTreeQueryTypeSelectorWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeQueryTypeSelectorWidget) { }
		SLATE_ATTRIBUTE(TWeakPtr<FCollectionCustomizationTreeElement>, WeakTreeElement)
		SLATE_ATTRIBUTE(TWeakPtr<SMovieGraphCollectionTreeView>, WeakTreeView)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Gets the query types which are available to be selected and added to the condition group. */
	static TArray<UClass*>* GetAvailableQueryTypes();

	/** Gets the widget that displays an individual query type. */
	TSharedRef<SWidget> GetQueryTypeContents(UClass* InTypeClass) const;

	/** Gets the display name of the currently selected query. */
	FText GetCurrentQueryTypeDisplayName() const;

	/** Gets the UClass associated with the currently selected query. */
	UClass* GetCurrentQueryType() const;

	/** Updates the query to the query type which was selected. */
	void SetQueryType(UClass* InNewQueryType, ESelectInfo::Type SelectInfo) const;

private:
	/** The tree element this widget is being displayed in. */
	TWeakPtr<FCollectionCustomizationTreeElement> WeakTreeElement = nullptr;

	/** The tree view associated with the tree element. */
	TWeakPtr<SMovieGraphCollectionTreeView> WeakTreeView = nullptr;
};

/**
 * Represents the data of one element displayed in the tree, which is one of the types in EElementType. Provides a way
 * of generating the children under the element (and caching the children for fast access later).
 */
class FCollectionCustomizationTreeElement : public TSharedFromThis<FCollectionCustomizationTreeElement>
{
public:
	/** The type of this element. */
	enum class EElementType : uint8
	{
		Root,					///< The root element; does not represent any actual data in the collection
		EmptyCollection,		///< Represents an "empty collection" notice within a collection
		ConditionGroup,			///< Represents a condition group within the collection
		EmptyConditionGroup,	///< Represents an "empty condition group" notice within a condition group
		Query,					///< Represents a query within a condition group
		QueryProperty			///< Represents a property within a query
	};
	
	explicit FCollectionCustomizationTreeElement(const EElementType InType);

	/** Whether the query associated with this element is currently enabled. */
	bool IsQueryEnabled() const;

	/**
	 * Gets the child elements nested under this element. Returns a cached result if available, otherwise calculates the
	 * children and returns the result. Call ClearCachedChildren() if the cache needs to be cleared (eg, when the tree
	 * is being refreshed).
	 */
	const TArray<TSharedPtr<FCollectionCustomizationTreeElement>>& GetChildren() const;

	/** Clears the cached result of GetChildren(). */
	void ClearCachedChildren() const;

	/** Gets the hash that uniquely identifies this element in the tree. */
	uint32 GetHash() const;

public:
	/** The type of item in the tree this element represents. */
	EElementType Type = EElementType::Root;

	/** The collection associated with this element. */
	TObjectPtr<UMovieGraphCollection> Collection = nullptr;

	/** If this element represents a condition group, this points to the underlying condition group (else nullptr). */
	TObjectPtr<UMovieGraphConditionGroup> ConditionGroup = nullptr;

	/** If this element represents a condition group query, this points to the underlying query (else nullptr). */
	TObjectPtr<UMovieGraphConditionGroupQueryBase> Query = nullptr;

	/** If this element represents a query property, this points to the underlying property (else nullptr). */
	FProperty* QueryProperty = nullptr;

	/** This element's parent element in the tree.  */
	TWeakPtr<const FCollectionCustomizationTreeElement> ParentElement = nullptr;

private:
	/** The (cached) child elements nested under this element. */
	mutable TArray<TSharedPtr<FCollectionCustomizationTreeElement>> ChildrenCache;

	/** The (cached) hash of this element. */
	mutable uint32 ElementHash = 0;
};

/** Specialized row class that spans all columns within the tree view. */
class SMovieGraphCollectionTreeItem_Stretch final : public STableRow<TSharedPtr<FCollectionCustomizationTreeElement>>
{
public:
	/** Gets the widgets that should be displayed within the column (which stretches across all columns in the tree view). */
	TArray<TSharedRef<SWidget>> GetColumnWidgets() const;
	
	void Construct(const FArguments& InArgs, const TSharedPtr<SMovieGraphCollectionTreeView>& InOwnerTable,
		const TSharedPtr<FCollectionCustomizationTreeElement>& InTreeElement);

private:
	/** The tree element associated with this item. */
	TWeakPtr<FCollectionCustomizationTreeElement> WeakTreeElement;
};

/**
 * The widget that is responsible for generating the per-column content for each element in the tree.
 */
class SMovieGraphCollectionTreeItem final : public SMultiColumnTableRow<TSharedPtr<FCollectionCustomizationTreeElement>>
{
	SLATE_BEGIN_ARGS(SMovieGraphCollectionTreeItem) { }
	SLATE_END_ARGS()

	inline static const FName ColumnID_Name = TEXT("Name");
	inline static const FName ColumnID_Value = TEXT("Value");

	void Construct(const FArguments& InArgs, const TSharedPtr<SMovieGraphCollectionTreeView>& InOwnerTable,
		const TSharedPtr<FCollectionCustomizationTreeElement>& InTreeElement, const TWeakPtr<IDetailLayoutBuilder>& InDetailBuilder);

	/** DragDetected handler for this widget within the tree. */
	FReply HandleOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& MouseEvent) const;

	/** CanAcceptDrop handler for this widget within the tree. */
	TOptional<EItemDropZone> HandleOnCanAcceptDrop(const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone, const TSharedPtr<FCollectionCustomizationTreeElement> TargetItem);

	/** AcceptDrop handler for this widget within the tree. */
	FReply HandleOnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FCollectionCustomizationTreeElement> TargetItem) const;

	/** Generates the widgets needed for condition group columns. */
	TSharedRef<SWidget> GenerateConditionGroupColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement);

	/** Generates the widgets needed for condition group query columns. */
	TSharedRef<SWidget> GenerateConditionGroupQueryColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement);

	/** Generates the widgets needed for condition group query property columns. */
	TSharedRef<SWidget> GenerateQueryPropertyColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement);

	/** Convenience method for refreshing the tree view. */
	void RefreshTreeView() const;

	//~ Begin SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End SMultiColumnTableRow overrides

private:
	/** The element that this tree item is displaying. */
	TWeakPtr<FCollectionCustomizationTreeElement> WeakTreeElement;

	/** The tree that owns this tree element. */
	TWeakPtr<SMovieGraphCollectionTreeView> WeakTreeView;

	/** The details builder associated with the customization. */
	TWeakPtr<IDetailLayoutBuilder> DetailBuilder;
};

/** Customize how the Collection node appears in the details panel. */
class FMovieGraphCollectionsCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder) override;
	//~ End IDetailCustomization interface

private:
	/** The tree view displaying condition groups and the queries contained in them. */
	TSharedPtr<SMovieGraphCollectionTreeView> TreeView = nullptr;

	/** The details builder associated with the customization. */
	TWeakPtr<IDetailLayoutBuilder> DetailBuilder;
};
