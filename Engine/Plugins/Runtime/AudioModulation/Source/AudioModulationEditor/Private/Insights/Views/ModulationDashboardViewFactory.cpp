// Copyright Epic Games, Inc. All Rights Reserved.
#include "ModulationDashboardViewFactory.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "Editor.h"
#include "IAudioInsightsModule.h"
#include "Internationalization/Text.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "AudioModulationInsights"

namespace AudioModulationEditor
{

	FAudioModulationDashboardViewFactory::FAudioModulationDashboardViewFactory()
	{
		Providers = TArray<TSharedPtr<UE::Audio::Insights::FTraceProviderBase>>();
	}

	FAudioModulationDashboardViewFactory::~FAudioModulationDashboardViewFactory()
	{
	}

	FName FAudioModulationDashboardViewFactory::GetName() const
	{
		return "AudioModulation";
	}

	FText FAudioModulationDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioInsights_Modulation_DisplayName", "Audio Modulation");
	}

	TSharedRef<SWidget> FAudioModulationDashboardViewFactory::GenerateWidgetForColumn(TSharedRef<UE::Audio::Insights::IDashboardDataViewEntry> InRowData, const FName& InColumnName)
	{
		return SNullWidget::NullWidget;
	}

	void FAudioModulationDashboardViewFactory::ProcessEntries(UE::Audio::Insights::FTraceTableDashboardViewFactory::EProcessReason Reason)
	{
	}

	FSlateIcon FAudioModulationDashboardViewFactory::GetIcon() const
	{
		return FSlateIcon();
	}

	UE::Audio::Insights::EDefaultDashboardTabStack FAudioModulationDashboardViewFactory::GetDefaultTabStack() const
	{
		return UE::Audio::Insights::EDefaultDashboardTabStack::Analysis;
	}

	TSharedRef<SWidget> FAudioModulationDashboardViewFactory::MakeWidget()
	{
		if (!DashboardWidget.IsValid())
		{
			DashboardWidget = UE::Audio::Insights::FTraceTableDashboardViewFactory::MakeWidget();

			if (FilteredEntriesListView.IsValid())
			{
				FilteredEntriesListView->SetSelectionMode(ESelectionMode::Single);
			}
		}

		return DashboardWidget->AsShared();
	}

	const TMap<FName, UE::Audio::Insights::FTraceTableDashboardViewFactory::FColumnData>& FAudioModulationDashboardViewFactory::GetColumns() const
	{
		static const TMap<FName, UE::Audio::Insights::FTraceTableDashboardViewFactory::FColumnData> ColumnData;
		
		return ColumnData;
	}

	void FAudioModulationDashboardViewFactory::SortTable()
	{
		
	}

	void FAudioModulationDashboardViewFactory::OnSelectionChanged(TSharedPtr<UE::Audio::Insights::IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo)
	{

	}

} // namespace AudioModulationEditor

#undef LOCTEXT_NAMESPACE
