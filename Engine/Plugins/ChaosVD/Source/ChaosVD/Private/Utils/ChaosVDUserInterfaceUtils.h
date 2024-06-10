// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDSettingsManager.h"
#include "IStructureDetailsView.h"
#include "SEnumCombo.h"

class UToolMenu;

namespace Chaos::VisualDebugger::Utils
{
	TSharedRef<IStructureDetailsView> MakeStructDetailsViewForMenu();

	TSharedRef<IDetailsView> MakeObjectDetailsViewForMenu();

	template <typename EnumType>
	TSharedRef<SWidget> MakeEnumMenuEntryWidget(const FText& MenuEntryLabel, const SEnumComboBox::FOnEnumSelectionChanged&& EnumValueChanged, const TAttribute<int32>&& CurrentValueAttribute)
	{
		return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.f)
				[
					SNew(STextBlock)
					.Text(MenuEntryLabel)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SEnumComboBox, StaticEnum<EnumType>())
					.CurrentValue(CurrentValueAttribute)
					.OnEnumSelectionChanged(EnumValueChanged)
				];
	}

	UENUM()
	enum class EChaosVDSaveSettingsOptions
	{
		None = 0,
		ShowSaveButton = 1 << 0,
		ShowResetButton = 1 << 1
	};
	ENUM_CLASS_FLAGS(EChaosVDSaveSettingsOptions)

	void CreateMenuEntryForObject(UToolMenu* Menu, UObject* Object, EChaosVDSaveSettingsOptions MenuEntryOptions = EChaosVDSaveSettingsOptions::None);

	template <typename Object>
	void CreateMenuEntryForSettingsObject(UToolMenu* Menu, EChaosVDSaveSettingsOptions MenuEntryOptions = EChaosVDSaveSettingsOptions::None)
	{
		CreateMenuEntryForObject(Menu, FChaosVDSettingsManager::Get().GetSettingsObject<Object>(), MenuEntryOptions);
	}

	template <typename TStruct>
	void SetStructToDetailsView(TStruct* NewStruct, TSharedRef<IStructureDetailsView>& InDetailsView)
	{
		TSharedPtr<FStructOnScope> StructDataView = nullptr;

		if (NewStruct)
		{
			StructDataView = MakeShared<FStructOnScope>(TStruct::StaticStruct(), reinterpret_cast<uint8*>(NewStruct));
		}

		InDetailsView->SetStructureData(StructDataView);
	}
}
