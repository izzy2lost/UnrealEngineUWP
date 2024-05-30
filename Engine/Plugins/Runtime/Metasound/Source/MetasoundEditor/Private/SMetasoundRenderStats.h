// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"

class UMetaSoundSource;
class STextBlock;

/** Widget for displaying render stats of a previewing MetaSound.. */
class SMetaSoundRenderStats : public SVerticalBox
{
public:
	SLATE_BEGIN_ARGS( SMetaSoundRenderStats) {};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Update(bool bIsPlaying, const UMetaSoundSource* InSource);
private:
	TSharedPtr<STextBlock> RenderStatsCostWidget;
	TSharedPtr<STextBlock> RenderStatsCPUWidget;

	bool bPreviousIsPlaying = false;
	double MaxCPUCoreUtilization = 0;
	float MaxRelativeRenderCost = 0.f;
};

