// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "StateTreeTypes.h"
#include "UObject/ObjectKey.h"
#include "Widgets/Views/STreeView.h"

enum class EStateTreeTransitionType : uint8;
template <typename OptionalType> struct TOptional;

class IPropertyHandle;
class SWidget;
class SSearchBox;
class SComboButton;
class UStateTree;
class UStateTreeState;

/**
 * Type customization for FStateTreeStateLink.
 */

class FStateTreeStateLinkDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	// Stores info about state
	struct FStateTreeStateItem : TSharedFromThis<FStateTreeStateItem>
	{
		FStateTreeStateItem() = default;
		
		FStateTreeStateItem(const FText& InDesc, const FText& InTooltipText, const FSlateBrush* InIcon, const EStateTreeTransitionType InType)
			: Desc(InDesc)
			, TooltipText(InTooltipText)
			, TransitionType(InType)
			, Icon(InIcon)
		{
		}

		FText Desc;
		FText TooltipText;
		EStateTreeTransitionType TransitionType = EStateTreeTransitionType::None;
		FGuid StateID = {};
		FSlateColor Color = FSlateColor(FLinearColor::White);
		const FSlateBrush* Icon = nullptr;
		bool bIsSubTree = false;
		bool bIsLinked = false;
		FText LinkedDesc;
		TArray<TSharedPtr<FStateTreeStateItem>> Children;
	};

	// Stores per session node expansion state for a node type.
	struct FStateExpansionState
	{
		TSet<FGuid> CollapsedStates;
	};

	void CacheStates();
	void CacheStates(TSharedPtr<FStateTreeStateItem> ParentNode, const UStateTreeState* State);
	void OnIdentifierChanged(const UStateTree& StateTree);

	TSharedRef<SWidget> GenerateStatePicker();
	FText GetCurrentStateDesc() const;
	const FSlateBrush* GetCurrentStateIcon() const;
	FSlateColor GetCurrentStateColor() const;
	FSlateFontInfo GetCurrentStateFont() const;
	bool IsValidLink() const;
	TOptional<EStateTreeTransitionType> GetTransitionType() const;
	static FSlateFontInfo GetItemFont(TSharedPtr<FStateTreeStateItem> Item);
	TSharedRef<ITableRow> GenerateStateItemRow(TSharedPtr<FStateTreeStateItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void GetStateItemChildren(TSharedPtr<FStateTreeStateItem> Item, TArray<TSharedPtr<FStateTreeStateItem>>& OutItems) const;
	void OnStateItemSelected(TSharedPtr<FStateTreeStateItem> SelectedItem, ESelectInfo::Type);
	void OnStateItemExpansionChanged(TSharedPtr<FStateTreeStateItem> ExpandedItem, bool bInExpanded) const;
	void OnSearchBoxTextChanged(const FText& NewText);
	static int32 FilterStateItemChildren(const TArray<FString>& FilterStrings, const bool bParentMatches, const TArray<TSharedPtr<FStateTreeStateItem>>& SourceArray, TArray<TSharedPtr<FStateTreeStateItem>>& OutDestArray);
	void ExpandAll(const TArray<TSharedPtr<FStateTreeStateItem>>& Items);
	
	static bool FindStateByIDRecursive(const TSharedPtr<FStateTreeStateItem>& Item, EStateTreeTransitionType TransitionType, const FGuid StateID, TArray<TSharedPtr<FStateTreeStateItem>>& OutPath);
	bool GetCurrentStateItem(TArray<TSharedPtr<FStateTreeStateItem>>& OutPath) const;
	
	void RestoreExpansionState();

	TSharedPtr<FStateTreeStateItem> RootItem;
	TSharedPtr<FStateTreeStateItem> FilteredRootItem;

	TSharedPtr<IPropertyHandle> NameProperty;
	TSharedPtr<IPropertyHandle> IDProperty;
	TSharedPtr<IPropertyHandle> LinkTypeProperty;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SSearchBox> SearchBox;
	
	TSharedPtr<STreeView<TSharedPtr<FStateTreeStateItem>>> StateItemTree;
	bool bIsRestoringExpansion = false;
	TWeakObjectPtr<const UStateTree> WeakStateTree = nullptr;

	// Save expansion state for each base node type. The expansion state does not persist between editor sessions. 
	static TMap<FObjectKey, FStateExpansionState> StateExpansionStates;

	// If set, hide selecting meta states like Next or (tree) Succeeded.
	bool bDirectStatesOnly = false;
	// If set, allow to select only states marked as subtrees.
	bool bSubtreesOnly = false;
	
	class IPropertyUtilities* PropUtils = nullptr;
	TSharedPtr<IPropertyHandle> StructProperty;
};
