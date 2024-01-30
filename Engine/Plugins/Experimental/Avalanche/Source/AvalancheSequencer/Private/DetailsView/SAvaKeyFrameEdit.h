// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SKeyEditInterface.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FAvaEaseCurveTool;
class FAvaSequencer;

namespace UE::Sequencer
{
	class FSequencerSelection;
}

class SAvaKeyFrameEdit : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaKeyFrameEdit) {}
		SLATE_ATTRIBUTE(FKeyEditData, KeyEditData)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FAvaSequencer> InSequencer);

protected:
	TWeakPtr<FAvaSequencer> AvaSequencerWeak;
	TWeakPtr<UE::Sequencer::FSequencerSelection> SequencerSelectionWeak;

	TSharedPtr<FAvaEaseCurveTool> CurveEaseTool;

	TAttribute<FKeyEditData> KeyEditData;
};
