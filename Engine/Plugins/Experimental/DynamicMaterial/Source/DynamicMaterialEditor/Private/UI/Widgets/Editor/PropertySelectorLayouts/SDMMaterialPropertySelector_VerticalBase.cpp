// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/PropertySelectorLayouts/SDMMaterialPropertySelector_VerticalBase.h"

#include "DMDefs.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector_VerticalBase"

void SDMMaterialPropertySelector_VerticalBase::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	SDMMaterialPropertySelector::Construct(
		SDMMaterialPropertySelector::FArguments(),
		InEditorWidget
	);
}

TSharedRef<SWidget> SDMMaterialPropertySelector_VerticalBase::CreateSlot_PropertyList()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SGridPanel> NewSlotList = SNew(SGridPanel)
		.FillColumn(PropertySelectorColumns::Select, 1.f);

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return NewSlotList;
	}

	int32 Row = 0;

	NewSlotList->AddSlot(PropertySelectorColumns::Select, Row)
		[
			CreateSlot_SelectButton(EDMMaterialPropertyType::None)
		];

	++Row;

	NewSlotList->AddSlot(PropertySelectorColumns::Select, Row)
		[
			CreateSlot_SelectButton(EDMMaterialPropertyType::Any)
		];

	++Row;

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
	{
		if (IsCustomMaterialProperty(PropertyPair.Key))
		{
			continue;
		}

		NewSlotList->AddSlot(PropertySelectorColumns::Enable, Row)
			[
				CreateSlot_EnabledButton(PropertyPair.Key)
			];

		NewSlotList->AddSlot(PropertySelectorColumns::Select, Row)
			[
				CreateSlot_SelectButton(PropertyPair.Key)
			];

		++Row;
	}

	return NewSlotList;
}

#undef LOCTEXT_NAMESPACE
