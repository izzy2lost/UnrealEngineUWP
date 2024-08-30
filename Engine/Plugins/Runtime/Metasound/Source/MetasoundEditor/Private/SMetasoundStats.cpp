// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetasoundStats.h"

#include "Components/AudioComponent.h"
#include "Containers/UnrealString.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundGenerator.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "MetaSoundEditor"

namespace Metasound::Editor
{
	namespace StatsPrivate
	{
		const FLinearColor BaseTextColor(1, 1, 1, 0.30f);
	}

	void SPageStats::Construct(const FArguments& InArgs)
	{
		SVerticalBox::Construct(SVerticalBox::FArguments());

		TSharedRef<SHorizontalBox> PageWidgetBox = SNew(SHorizontalBox);
		PageWidgetBox->AddSlot()
			.Padding(2.0f)
			.HAlign(HAlign_Center)
			[
				SAssignNew(PageTextWidget, STextBlock)
				.Visibility(EVisibility::HitTestInvisible)
				.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
				.ColorAndOpacity(StatsPrivate::BaseTextColor)
			];

		ExecImageWidget = SNew(SImage)
			.Image(Style::CreateSlateIcon("MetasoundEditor.Page.Executing").GetIcon())
			.DesiredSizeOverride(FVector2D(24.f, 24.f))
			.ColorAndOpacity(FStyleColors::AccentGreen)
			.Visibility(EVisibility::Collapsed);

		PageWidgetBox->AddSlot()
			.Padding(2.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()[ ExecImageWidget.ToSharedRef()];

		AddSlot().HAlign(HAlign_Left) [ PageWidgetBox ];
	}

	void SPageStats::SetExecVisibility(TAttribute<EVisibility> InVisibility)
	{
		ExecImageWidget->SetVisibility(MoveTemp(InVisibility));
	}

	void SPageStats::Update(const FMetaSoundPageSettings* PageSettings, const FText& Header, const FSlateColor* ColorOverride)
	{
		using namespace Engine;

		FText PageInfo;
		EVisibility Visibility = EVisibility::Collapsed;
		if (PageTextWidget.IsValid() && PageSettings)
		{
			PageInfo = FText::Format(LOCTEXT("PageStatsFormat", "{0}: {1}"), Header, FText::FromString(PageSettings->Name.ToString()));
			Visibility = EVisibility::Visible;
		}

		PageTextWidget->SetText(PageInfo);
		PageTextWidget->SetColorAndOpacity(ColorOverride ? *ColorOverride : StatsPrivate::BaseTextColor);
	}

	void SRenderStats::Construct(const FArguments& InArgs)
	{
		SVerticalBox::Construct(SVerticalBox::FArguments());
		AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SAssignNew(RenderStatsCostWidget, STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
			.ColorAndOpacity(FLinearColor(1, 1, 1, 0.30f))
		];

		AddSlot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SAssignNew(RenderStatsCPUWidget, STextBlock)
			.Visibility(EVisibility::HitTestInvisible)
			.TextStyle(FAppStyle::Get(), "Graph.ZoomText")
			.ColorAndOpacity(FLinearColor(1, 1, 1, 0.30f))
		];
	}

	void SRenderStats::Update(bool bIsPlaying, const UMetaSoundSource* InSource)
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
} // namespace Metasound::Editor
#undef LOCTEXT_NAMESPACE
