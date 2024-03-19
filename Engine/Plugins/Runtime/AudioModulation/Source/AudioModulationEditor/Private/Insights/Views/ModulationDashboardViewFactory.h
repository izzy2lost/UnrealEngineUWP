// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Views/TableDashboardViewFactory.h"
#include "Widgets/Input/SCheckBox.h"

namespace AudioModulationEditor
{
	class FAudioModulationDashboardViewFactory : public UE::Audio::Insights::FTraceObjectTableDashboardViewFactory
	{
	public:
		FAudioModulationDashboardViewFactory();
		virtual ~FAudioModulationDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual UE::Audio::Insights::EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

	protected:
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<UE::Audio::Insights::IDashboardDataViewEntry> InRowData, const FName& InColumnName) override;
		virtual void ProcessEntries(UE::Audio::Insights::FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual const TMap<FName, UE::Audio::Insights::FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;

		virtual void OnSelectionChanged(TSharedPtr<UE::Audio::Insights::IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo) override;

	};
} // namespace AudioModulationEditor
