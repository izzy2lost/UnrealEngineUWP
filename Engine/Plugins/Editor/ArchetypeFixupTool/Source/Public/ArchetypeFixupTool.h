// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncDetailViewDiff.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"

class SDetailsSplitter;
class FArchetypeFixupPanel;
struct FPropertySoftPath;
class IStructureDetailsView;

/**
 * This tool diffs multiple property bags of one format against the same number of property bags of another format.
 * 
 */
class ARCHETYPEFIXUPTOOL_API SArchetypeFixupTool : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArchetypeFixupTool)
	{}
		SLATE_ARGUMENT(TConstArrayView<TObjectPtr<UObject>>, Archetypes)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	void SetDockTab(const TSharedRef<SDockTab>& DockTab);
	void GenerateDetailsViews();
	static FLinearColor GetRowHighlightColor(const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode);
	bool IsResolved() const;
	FReply OnAutoMarkForDeletion() const;
	
private:
	FReply OnConfirmClicked() const;
	
	TWeakPtr<SDockTab> OwningDockTab;
	TStaticArray<TSharedPtr<FArchetypeFixupPanel>, 2> Panels;
	TSharedPtr<FAsyncDetailViewDiff> PanelDiff;

	TSharedPtr<SDetailsSplitter> Splitter;
};
