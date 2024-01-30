// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AvaMediaDefines.h"
#include "Animation/CurveSequence.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"

class FAvaBroadcastEditor;
class SAvaChannel;
class SGridPanel;
class UAvalancheBroadcast;

class SAvaBroadcastChannels : public SCompoundWidget
{
	struct FAvaChannelMaximizer
	{
		FAvaChannelMaximizer();

		void Reset();
		void ToggleMaximize(const TSharedRef<SAvaChannel>& InChannelWidget);
		
		float GetRowFill(int32 InRowIndex) const;
		float GetColumnFill(int32 InColumnIndex) const;

		TWeakPtr<SAvaChannel> ChannelWidgetWeak;
		FCurveSequence MaximizeSequence;
		bool bMaximizing  = false;
	};
	
public:
	
	SLATE_BEGIN_ARGS(SAvaBroadcastChannels) {}
	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);
	
	virtual ~SAvaBroadcastChannels() override;

	bool CanAddChannel();
	void AddChannel();
	
	float GetRowFill(int32 RowId) const;
	float GetColumnFill(int32 ColumnId) const;

	TSharedRef<SWidget> MakeChannelsToolbar();
	
	bool CanMaximizeChannel() const;
	void ToggleMaximizeChannel(const TSharedRef<SAvaChannel>& InWidget);
	
	void OnBroadcastChanged(EAvaBroadcastChange ChangedEvent);
	void RefreshChannelGrid();
	
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;;
	
protected:
	
	TWeakObjectPtr<UAvalancheBroadcast> BroadcastWeak;
	
	TWeakPtr<FAvaBroadcastEditor> BroadcastEditorWeak;
	
	TSharedPtr<SGridPanel> ChannelGrid;
	
	TMap<FName, TSharedPtr<SAvaChannel>> Channels;

	FAvaChannelMaximizer ChannelMaximizer;
};
