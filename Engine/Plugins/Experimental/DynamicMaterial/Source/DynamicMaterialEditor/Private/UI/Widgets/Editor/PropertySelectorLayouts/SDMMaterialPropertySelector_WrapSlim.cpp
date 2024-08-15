// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/PropertySelectorLayouts/SDMMaterialPropertySelector_WrapSlim.h"

#include "Components/DMMaterialProperty.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector_WrapSlim"

void SDMMaterialPropertySelector_WrapSlim::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	SDMMaterialPropertySelector_WrapBase::Construct(
		SDMMaterialPropertySelector_WrapBase::FArguments(),
		InEditorWidget
	);
}

TSharedRef<SWidget> SDMMaterialPropertySelector_WrapSlim::CreateSlot_PropertyList()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SWrapBox> NewSlotList = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(6.f, 3.f))
		.UseAllottedSize(true);

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return NewSlotList;
	}

	NewSlotList->AddSlot()
		[
			CreateSlot_SelectButton(EDMMaterialEditorMode::GlobalSettings, EDMMaterialPropertyType::None)
		];

	NewSlotList->AddSlot()
		[
			CreateSlot_SelectButton(EDMMaterialEditorMode::PropertyPreviews, EDMMaterialPropertyType::None)
		];

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
	{
		if (!PropertyPair.Value || !PropertyPair.Value->IsEnabled())
		{
			continue;
		}

		if (!EditorOnlyData->GetSlotForMaterialProperty(PropertyPair.Value->GetMaterialProperty()))
		{
			continue;
		}

		NewSlotList->AddSlot()
			[
				CreateSlot_SelectButton(EDMMaterialEditorMode::EditSlot, PropertyPair.Key)
			];
	}

	return NewSlotList;
}

#undef LOCTEXT_NAMESPACE
