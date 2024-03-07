// Copyright Epic Games, Inc. All Rights Reserved.

#include "SEditorPerformanceDialogs.h"
#include "Algo/Sort.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/FastDecimalFormat.h"
#include "Math/BasicMathExpressionEvaluator.h"
#include "Math/UnitConversion.h"
#include "Misc/ExpressionParser.h"
#include "Styling/StyleColors.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorPerformanceModule.h"
#include "Editor/EditorPerformanceSettings.h"

UE_DISABLE_OPTIMIZATION_SHIP

#define LOCTEXT_NAMESPACE "EditorPerformance"

void SEditorPerformanceReportDialog::Construct(const FArguments& InArgs)
{
	const float RowMargin = 0.0f;
	const float TitleMargin = 10.0f;
	const float ColumnMargin = 10.0f;
	const FMargin DefaultMarginFirstColumn(ColumnMargin, RowMargin);
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	FEditorPerformanceModule& EditorPerfModule = FModuleManager::LoadModuleChecked<FEditorPerformanceModule>("EditorPerformance");

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0, 20, 0, 0)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Margin(TitleMargin)
				.ColorAndOpacity(TitleColor)
				.Font(TitleFont)
				.Justification(ETextJustify::Left)
				.Text_Lambda([this,&EditorPerfModule]
					{ 
						return FText::FromString(*FString::Printf(TEXT("Profile : %s"), *EditorPerfModule.GetKPIProfileName())); 
					}
				)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 0)
		.Expose(SettingsGridSlot)
		[
			GetSettingsGridPanel()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 0)
		.Expose(KPIGridSlot)
		[
			GetKPIGridPanel()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 0)
		.Expose(InformationGridSlot)
		[
			GetInformationGridPanel()
		]
	];

	RegisterActiveTimer(5.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SEditorPerformanceReportDialog::UpdateGridPanels));
}

EActiveTimerReturnType SEditorPerformanceReportDialog::UpdateGridPanels(double InCurrentTime, float InDeltaTime)
{
	(*SettingsGridSlot)
	[
		GetSettingsGridPanel()
	];

	(*KPIGridSlot)
	[
		GetKPIGridPanel()
	];

	(*InformationGridSlot)
	[
		GetInformationGridPanel()
	];

	SlatePrepass(GetPrepassLayoutScaleMultiplier());

	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SEditorPerformanceReportDialog::GetSettingsGridPanel()
{
	TSharedRef<SGridPanel> Panel =
		SNew(SGridPanel);

	const float RowMargin = 0.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	const FMargin TitleMargin(0.0f, 10.0f, ColumnMargin, 10.0f);
	const FMargin TitleMarginFirstColumn(ColumnMargin, 10.0f);
	const FMargin DefaultMargin(0.0f, RowMargin, ColumnMargin, RowMargin);
	const FMargin DefaultMarginFirstColumn(ColumnMargin, RowMargin);

	int32 Row = 0;

	/*Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(TitleMarginFirstColumn)
			.ColorAndOpacity(TitleColor)
			.Font(TitleFont)
			.Text(LOCTEXT("Settings", "Settings"))
		];

	Row++;*/

	Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]
				{
					const UEditorPerformanceSettings* EditorPerformanceSettings = GetDefault<UEditorPerformanceSettings>();
					return EditorPerformanceSettings && EditorPerformanceSettings->bEnableNotifications ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
			{
				UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

				if (EditorPerformanceSettings)
				{
					EditorPerformanceSettings->bEnableNotifications = NewState == ECheckBoxState::Checked;
					EditorPerformanceSettings->PostEditChange();
					EditorPerformanceSettings->SaveConfig();
				}

				UpdateGridPanels(0.0f, 0.0f);
			})
			.Padding(FMargin(4.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EnableNotifications", "Enable Notifications"))
				.ColorAndOpacity(EStyleColor::Foreground)
			]
		];

	Panel->AddSlot(1, Row)
		.HAlign(HAlign_Left)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]
				{
					const UEditorPerformanceSettings* EditorPerformanceSettings = GetDefault<UEditorPerformanceSettings>();
					return EditorPerformanceSettings && EditorPerformanceSettings->bEnableSnapshots ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

					if (EditorPerformanceSettings)
					{
						EditorPerformanceSettings->bEnableSnapshots = NewState == ECheckBoxState::Checked;
						EditorPerformanceSettings->PostEditChange();
						EditorPerformanceSettings->SaveConfig();
					}

					UpdateGridPanels(0.0f, 0.0f);
				})
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableSnapshots", "Enable Snapshots"))
					.ColorAndOpacity(EStyleColor::Foreground)
				]
			];

	Panel->AddSlot(2, Row)
		.HAlign(HAlign_Left)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]
				{
					const UEditorPerformanceSettings* EditorPerformanceSettings = GetDefault<UEditorPerformanceSettings>();
					return EditorPerformanceSettings && EditorPerformanceSettings->bEnableTelemetry ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
			{
				UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

				if (EditorPerformanceSettings)
				{
					EditorPerformanceSettings->bEnableTelemetry = NewState == ECheckBoxState::Checked;
					EditorPerformanceSettings->PostEditChange();
					EditorPerformanceSettings->SaveConfig();
				}

				UpdateGridPanels(0.0f, 0.0f);
			})
					.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableTelemetry", "Enable Telemetry"))
					.ColorAndOpacity(EStyleColor::Foreground)
				]
		];

	Row++;

	return Panel;
}

TSharedRef<SWidget> SEditorPerformanceReportDialog::GetInformationGridPanel()
{
	TSharedRef<SGridPanel> Panel =
		SNew(SGridPanel);

	const float RowMargin = 0.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	const FMargin TitleMargin(0.0f, 10.0f, ColumnMargin, 10.0f);
	const FMargin TitleMarginFirstColumn(ColumnMargin, 10.0f);
	const FMargin DefaultMargin(0.0f, RowMargin, ColumnMargin, RowMargin);
	const FMargin DefaultMarginFirstColumn(ColumnMargin, RowMargin);

	int32 Row = 0;

	Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(TitleMarginFirstColumn)
			.ColorAndOpacity(TitleColor)
			.Font(TitleFont)
			.Justification(ETextJustify::Left)
			.Text(LOCTEXT("InformationTitle", "Information"))
		];

	Row++;

	Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMarginFirstColumn)
			.ColorAndOpacity(EStyleColor::Foreground)
			.Font(TitleFont)
			.Justification(ETextJustify::Left)
			.Text(LOCTEXT("DynamicText", "TODO : Some information about about what to do with this wanring"))
		];

	Row++;

	return Panel;
}

TSharedRef<SWidget> SEditorPerformanceReportDialog::GetKPIGridPanel()
{
	TSharedRef<SGridPanel> Panel =
		SNew(SGridPanel);

	const float RowMargin = 0.0f;
	const float ColumnMargin = 10.0f;
	const FSlateColor TitleColor = FStyleColors::AccentWhite;
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	const FMargin TitleMargin(0.0f, 10.0f, ColumnMargin, 10.0f);
	const FMargin TitleMarginFirstColumn(ColumnMargin, 10.0f);
	const FMargin DefaultMargin(0.0f, RowMargin, ColumnMargin, RowMargin);
	const FMargin DefaultMarginFirstColumn(ColumnMargin, RowMargin);

	const UEditorPerformanceSettings* EditorPerformanceSettings = GetDefault<UEditorPerformanceSettings>();
	FEditorPerformanceModule& EditorPerfModule = FModuleManager::LoadModuleChecked<FEditorPerformanceModule>("EditorPerformance");

	const bool EnableNotifcations = EditorPerformanceSettings && EditorPerformanceSettings->bEnableNotifications;
	const bool ShowWarningsOnly = EditorPerformanceSettings && EditorPerformanceSettings->bShowWarningsOnly;
	
	int32 Row = 0;

	Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(TitleMarginFirstColumn)
			.ColorAndOpacity(TitleColor)
			.Font(TitleFont)
			.Text(LOCTEXT("Measurements", "Measurements"))
		];

	Row++;

	Panel->AddSlot(0, Row)
		.HAlign(HAlign_Left)
		[
			SNew(SComboBox<FName>)
			.OptionsSource(&WarningFilterOptions)
			.InitiallySelectedItem( EditorPerformanceSettings && EditorPerformanceSettings->bShowWarningsOnly ? WarningFilterOptions[1] : WarningFilterOptions[0] )
			.OnGenerateWidget_Lambda([](FName Name)
				{	
					return SNew(STextBlock)
						.Text(FText::FromString(*Name.ToString()));
				})
			.OnSelectionChanged_Lambda([this](FName Name, ESelectInfo::Type)
			{
				UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

				if (EditorPerformanceSettings)
				{
					EditorPerformanceSettings->bShowWarningsOnly = (Name == WarningFilterOptions[1]);
				}

				UpdateGridPanels(0.0f, 0.0f);
			})
			.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString((EditorPerformanceSettings && EditorPerformanceSettings->bShowWarningsOnly) ? *WarningFilterOptions[1].ToString() : *WarningFilterOptions[0].ToString()) )
			]
		];

	Row++;

	Panel->AddSlot(1, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.ColorAndOpacity(TitleColor)
			.Font(TitleFont)
			.Text(LOCTEXT("CurrentValueColumn", "Current"))
		];

	Panel->AddSlot(3, Row)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Margin(DefaultMargin)
			.ColorAndOpacity(TitleColor)
			.Font(TitleFont)
			.Text(LOCTEXT("ExpectedValueColumn", "Expected"))
		];

	if (EnableNotifcations)
	{
		Panel->AddSlot(5, Row)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Margin(DefaultMargin)
				.ColorAndOpacity(TitleColor)
				.Font(TitleFont)
				.Text(LOCTEXT("NotifyColumn", "Notify"))
			];
	}

	Row++;
	
	EditorPerfModule.UpdateKPIs();

	TMap<FName, TArray<FKPIValue>> SortedKPIValues;

	for (FKPIValues::TConstIterator It(EditorPerfModule.GetKPIValues()); It; ++It)
	{
		const FKPIValue& KPIValue = It->Value;

		if (ShowWarningsOnly && KPIValue.GetState()!=FKPIValue::Bad )
		{
			continue;
		}

		if (SortedKPIValues.Find(KPIValue.Category)!=nullptr)
		{
			SortedKPIValues[KPIValue.Category].Emplace(KPIValue);
		}
		else
		{
			TArray<FKPIValue> KPIArray;
			KPIArray.Emplace(KPIValue);
			SortedKPIValues.Emplace(KPIValue.Category, KPIArray);
		}
	}

	for (TMap<FName, TArray<FKPIValue>>::TConstIterator It(SortedKPIValues); It; ++It)
	{
		const TArray<FKPIValue>& KPIValues = It->Value;
		const FName& Category = It->Key;

		// Render the category name
		Panel->AddSlot(0, Row)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Margin(TitleMarginFirstColumn)
				.ColorAndOpacity(TitleColor)
				.Font(TitleFont)
				.Text(FText::FromString(*Category.ToString()))
			];
		
		Row++;

		for (const FKPIValue& KPIValue : KPIValues)
		{
			const FKPIValue::EState KPIValueState = KPIValue.GetState();

			const FSlateColor KPIColor = KPIValueState==FKPIValue::Bad ? EStyleColor::Warning : EStyleColor::Foreground;
			const FSlateBrush* KPIWarningIcon = FAppStyle::Get().GetBrush("EditorPerformance.Report.Warning");
			const float KPIIconSize = 8.0f;
			const FName& KPIName = KPIValue.Name;

			Panel->AddSlot(0, Row)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Margin(DefaultMarginFirstColumn)
					.ColorAndOpacity(EStyleColor::Foreground)
					.Text(FText::FromString(*KPIName.ToString()))
				];

			if (KPIValueState != FKPIValue::NotSet)
			{
				Panel->AddSlot(1, Row)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Margin(DefaultMargin)
						.ColorAndOpacity(KPIColor)
						.Text(FText::FromString(*FKPIValue::GetValueAsString(KPIValue.CurrentValue, KPIValue.DisplayType)))
					];

				Panel->AddSlot(2, Row)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Margin(DefaultMargin)
						.ColorAndOpacity(KPIColor)
						.Text(FText::FromString(*FKPIValue::GetComparisonAsString(KPIValue.Compare)))
					];

				Panel->AddSlot(3, Row)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Margin(DefaultMargin)
						.ColorAndOpacity(KPIColor)
						.Text(FText::FromString(*FKPIValue::GetValueAsString(KPIValue.ThresholdValue, KPIValue.DisplayType)))
					];

				if (KPIValueState != FKPIValue::Good)
				{
					Panel->AddSlot(4, Row)
						[
							SNew(SImage)
							.Image(KPIWarningIcon)
						];
				}
			}
			else
			{
				Panel->AddSlot(1, Row)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Margin(DefaultMargin)
						.ColorAndOpacity(KPIColor)
						.Text(LOCTEXT("PendingValue", "..."))
					];
			}

			if (EnableNotifcations)
			{
				Panel->AddSlot(5, Row)
					.HAlign(HAlign_Left)
					[
						SNew(SComboBox<FName>)
						.OptionsSource(&NotifcationOptions)
						.InitiallySelectedItem(EditorPerformanceSettings->NotificationList.Find(KPIName) != INDEX_NONE ? NotifcationOptions[0] : NotifcationOptions[1])
						.OnGenerateWidget_Lambda([KPIName, &EditorPerfModule](FName Name)
							{
								return SNew(STextBlock)
									.Text(FText::FromString(*Name.ToString()));
							})
						.OnSelectionChanged_Lambda([this, KPIName, &EditorPerfModule](FName Name, ESelectInfo::Type)
						{
							UEditorPerformanceSettings* EditorPerformanceSettings = GetMutableDefault<UEditorPerformanceSettings>();

							if (EditorPerformanceSettings)
							{
								if (Name == NotifcationOptions[0])
								{
									if (EditorPerformanceSettings->NotificationList.Find(KPIName) == INDEX_NONE)
									{
										// Add this KPI to the notification list
										EditorPerformanceSettings->NotificationList.Emplace(KPIName);
									}
								}
								else
								{
									// Remove this KPI to the notification ignore list
									EditorPerformanceSettings->NotificationList.Remove(KPIName);
								}

								EditorPerformanceSettings->PostEditChange();
								EditorPerformanceSettings->SaveConfig();
							}

							UpdateGridPanels(0.0f, 0.0f);
						})
						.Content()
						[
							SNew(STextBlock)
							.Text(FText::FromString(EditorPerformanceSettings->NotificationList.Find(KPIName) != INDEX_NONE ? *NotifcationOptions[0].ToString() : *NotifcationOptions[1].ToString() ))
						]
					];
			}

			Row++;
		}
	}

	return Panel;
}

#undef LOCTEXT_NAMESPACE

UE_ENABLE_OPTIMIZATION_SHIP
