// Copyright Epic Games, Inc. All Rights Reserved.

#include "STraceStatistics.h"

#include "Internationalization/Text.h"
#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

//TraceTools
#include "Services/SessionTraceControllerFilterService.h"
#include "TraceToolsStyle.h"

#define LOCTEXT_NAMESPACE "STraceStatistics"

namespace UE::TraceTools
{

STraceStatistics::STraceStatistics()
{
}

STraceStatistics::~STraceStatistics()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STraceStatistics::Construct(const FArguments& InArgs, TSharedPtr<ISessionTraceFilterService> InSessionFilterService)
{
	SessionFilterService = InSessionFilterService;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(EVerticalAlignment::VAlign_Top)
		[
			SNew(SBorder)
			.BorderImage(FTraceToolsStyle::GetBrush("FilterPresets.BackgroundBorder"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)

					// Trace Settings
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.Padding(0.0f, 10.0f, 0.0f, 0.0f)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Trace Settings", "Trace Settings"))
						.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					]

					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("ImportantCache", "Important Events Cache:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetSettingsOnOffText(SessionFilterService->GetSettings().bUseImportantCache); })
						]
					]

					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("WorkerThread", "Worker Thread:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetSettingsOnOffText(SessionFilterService->GetSettings().bUseWorkerThread); })
						]
					]

					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Tail Size", "Tail Size:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0f, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetSettingsMemoryValueText(SessionFilterService->GetSettings().TailSizeBytes); })
						]
					]
				]

				+ SHorizontalBox::Slot()
				.Padding(30.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)

					// Trace statistics
					+ SVerticalBox::Slot()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.Padding(0.0f, 10.0f, 0.0f, 0.0f)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Statistics", "Statistics"))
						.Font(FAppStyle::Get().GetFontStyle("NormalFontBold"))
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Bytes Sent", "Bytes Sent:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().BytesSent); })
						]
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Bytes Traced", "Bytes Traced:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().BytesTraced); })
						]
					]

								
					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Memory Used", "Memory Used:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().MemoryUsed); })
						]
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Cache Allocated", "Cache Allocated:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().CacheAllocated); })
						]
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
							
						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Cache Used", "Cache Used:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().CacheUsed); })
						]
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(0.0f, 2.0, 0.0f, 0.0f)
						.AutoWidth()
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::Foreground))
							.Text(LOCTEXT("Cache Waste", "Cache Waste:"))
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 2.0, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(EStyleColor::AccentGray))
							.Text_Lambda([this]() { return this->GetStatsMemoryValueText(SessionFilterService->GetStats().CacheWaste); })
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText STraceStatistics::GetSettingsOnOffText(bool InValue) const
{
	if (!SessionFilterService->HasSettings())
	{
		return LOCTEXT("N/A", "N/A");
	}

	if (InValue)
	{
		return LOCTEXT("On", "On");
	}

	return LOCTEXT("Off", "Off");
};

FText STraceStatistics::GetSettingsMemoryValueText(uint64 InValue) const
{
	if (!SessionFilterService->HasSettings())
	{
		return LOCTEXT("N/A", "N/A");
	}

	return FText::AsMemory(InValue);
}

FText STraceStatistics::GetStatsMemoryValueText(uint64 InValue) const
{
	if (!SessionFilterService->HasStats())
	{
		return LOCTEXT("N/A", "N/A");
	}

	return FText::AsMemory(InValue);
}

} // namespace UE::TraceTools

#undef LOCTEXT_NAMESPACE