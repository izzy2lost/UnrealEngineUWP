// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaybackEditor;
class IDetailsView;

class SAvaPlaybackDetailsView : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPlaybackDetailsView){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaybackEditor>& InPlaybackEditor);
	virtual ~SAvaPlaybackDetailsView() override;
	
	void OnPlaybackNodeSelectionChanged(const TArray<UObject*>& InSelectedObjects);

protected:

	TWeakPtr<FAvaPlaybackEditor> PlaybackEditorWeak;
	
	TSharedPtr<IDetailsView> DetailsView;
};
