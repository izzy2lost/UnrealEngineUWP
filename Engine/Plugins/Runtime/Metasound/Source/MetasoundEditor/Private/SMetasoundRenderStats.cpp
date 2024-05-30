// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetasoundRenderStats.h"

#include "Components/AudioComponent.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "MetasoundSource.h"
#include "MetasoundGenerator.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SMetaSoundRenderStats::Construct(const FArguments& InArgs)
{
	SVerticalBox::Construct(SVerticalBox::FArguments());
	AddSlot()
	.HAlign(HAlign_Left)
	[
		SAssignNew(RenderStatsCostWidget, STextBlock)
		.Visibility(EVisibility::HitTestInvisible)
		.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
		.ColorAndOpacity(FLinearColor(1, 1, 1, 0.30f))
	];

	AddSlot()
	.HAlign(HAlign_Left)
	[
		SAssignNew(RenderStatsCPUWidget, STextBlock)
		.Visibility(EVisibility::HitTestInvisible)
		.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
		.ColorAndOpacity(FLinearColor(1, 1, 1, 0.30f))
	];
}

void SMetaSoundRenderStats::Update(bool bIsPlaying, const UMetaSoundSource* InSource)
{
	using namespace Metasound;

	// Reset maximum values when play restarts
	if (bIsPlaying && !bPreviousIsPlaying)
	{
		MaxRelativeRenderCost = 0.f;
		MaxCPUCoreUtilization = 0;
	}
	bPreviousIsPlaying = bIsPlaying;

	if (RenderStatsCPUWidget.IsValid() && RenderStatsCostWidget.IsValid())
	{
		double CPUCoreUtilization = 0;
		float RelativeRenderCost = 0.f;

		// Find generator for playing preview component. 
		if (bIsPlaying && InSource)
		{
			if (const UAudioComponent* PreviewComponent = GEditor->GetPreviewAudioComponent())
			{
				TSharedPtr<FMetasoundGenerator> Generator = InSource->GetGeneratorForAudioComponent(PreviewComponent->GetAudioComponentID()).Pin();
				if (Generator.IsValid())
				{
					// Update render stats
					CPUCoreUtilization = Generator->GetCPUCoreUtilization();
					MaxCPUCoreUtilization = FMath::Max(MaxCPUCoreUtilization, CPUCoreUtilization);

					RelativeRenderCost = Generator->GetRelativeRenderCost();
					MaxRelativeRenderCost = FMath::Max(MaxRelativeRenderCost, RelativeRenderCost);
				}
			}
		}

		// Display updated render stats. 
		FString CPUCoreUtilizationString = FString::Printf(TEXT("%3.2f%% (%3.2f%% Max) CPU Core"), 100. * CPUCoreUtilization, 100. * MaxCPUCoreUtilization);
		RenderStatsCPUWidget->SetText(FText::FromString(CPUCoreUtilizationString));

		FString RenderCostString = FString::Printf(TEXT("%3.2f (%3.2f Max) Relative Render Cost"), RelativeRenderCost, MaxRelativeRenderCost);
		RenderStatsCostWidget->SetText(FText::FromString(RenderCostString));
	}
}
