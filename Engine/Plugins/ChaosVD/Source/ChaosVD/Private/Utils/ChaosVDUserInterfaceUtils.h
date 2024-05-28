// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDSettingsManager.h"
#include "IStructureDetailsView.h"

class UToolMenu;

namespace Chaos::VisualDebugger::Utils
{
	TSharedRef<IStructureDetailsView> MakeStructDetailsViewForMenu();

	TSharedRef<IDetailsView> MakeObjectDetailsViewForMenu();

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
