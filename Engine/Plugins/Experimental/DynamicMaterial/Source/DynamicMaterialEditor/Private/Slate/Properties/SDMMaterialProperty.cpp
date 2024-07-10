// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMMaterialProperty.h"
#include "Components/DMMaterialProperty.h"
#include "DynamicMaterialEditorStyle.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMSlot.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SDMMaterialProperty::Construct(const FArguments& InArgs, const TSharedRef<SDMSlot>& InMaterialSlotWidget, EDMMaterialPropertyType InProperty)
{
	MaterialSlotWidgetWeak = InMaterialSlotWidget;
	Property = InProperty;

	TSharedPtr<SDMEditor> MaterialEditor = InMaterialSlotWidget->GetEditorWidget();

	if (ensure(MaterialEditor.IsValid()))
	{
		UDynamicMaterialModelBase* MaterialModelBase = MaterialEditor->GetMaterialModelBase();

		if (ensure(IsValid(MaterialModelBase)))
		{
			UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelBase);

			if (ensure(IsValid(ModelEditorOnlyData)))
			{
				TMap<EDMMaterialPropertyType, UDMMaterialProperty*> EditorProperties = ModelEditorOnlyData->GetMaterialProperties();
				UDMMaterialProperty* const* MaterialProperty = EditorProperties.Find(Property);

				if (ensure(MaterialProperty) && ensure(IsValid(*MaterialProperty)))
				{
					ChildSlot
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SNew(SColorBlock)
							.Color(FLinearColor::Black)
							.CornerRadius(FVector4(6.f, 6.f, 6.f, 6.f))
						]

						+ SOverlay::Slot()
						.Padding(1.f)
						[
							SNew(SColorBlock)
							.Color(FStyleColors::Highlight.GetSpecifiedColor())
							.CornerRadius(FVector4(5.f, 5.f, 5.f, 5.f))
						]

						+ SOverlay::Slot()
						[
							SNew(STextBlock)
							.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
							.Text((*MaterialProperty)->GetDescription())
							.ColorAndOpacity(FLinearColor::White)
							.Margin(FMargin(8.f, 5.f, 8.f, 4.f))
						]
					];
				}
			}
		}
	}
}
