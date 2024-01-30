// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAvaBroadcastEditor;
class FAvaOutputTileItem;
class IDetailsView;

class  SAvaBroadcastDetailsView : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaBroadcastDetailsView) {}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);
	virtual ~SAvaBroadcastDetailsView() override;

	bool IsMediaOutputEditingEnabled() const;
	void OnMediaOutputSelectionChanged(const TSharedPtr<FAvaOutputTileItem>& InSelectedItem);

	EVisibility GetEmptySelectionTextVisibility() const;
	
protected:

	TWeakPtr<FAvaBroadcastEditor> BroadcastEditorWeak;
	
	TSharedPtr<IDetailsView> DetailsView;
};
