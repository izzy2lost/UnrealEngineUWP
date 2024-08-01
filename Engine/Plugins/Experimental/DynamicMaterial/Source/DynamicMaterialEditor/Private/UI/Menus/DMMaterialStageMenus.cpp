// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Menus/DMMaterialStageMenus.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "ScopedTransaction.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "UI/Menus/DMMenuContext.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialStage.h"
#include "UI/Widgets/SDMMaterialEditor.h"

#define LOCTEXT_NAMESPACE "FDMMaterialStageMenus"

FName FDMMaterialStageMenus::GetStageMenuName()
{
	return "MaterialDesigner.MaterialStage";
}

FName FDMMaterialStageMenus::GetStageToggleSectionName()
{
	return "StageToggle";
}

UToolMenu* FDMMaterialStageMenus::GenerateStageMenu(const TSharedPtr<SDMMaterialSlotEditor>& InSlotWidget, 
	const TSharedPtr<SDMMaterialStage>& InStageWidget)
{
	UToolMenu* NewToolMenu = UDMMenuContext::GenerateContextMenuStage(GetStageMenuName(), InSlotWidget->GetEditorWidget(), InStageWidget);
	if (!NewToolMenu)
	{
		return nullptr;
	}

	AddStageSection(NewToolMenu);

	return NewToolMenu;
}

void FDMMaterialStageMenus::AddStageSection(UToolMenu* InMenu)
{
	if (!IsValid(InMenu) || InMenu->ContainsSection(GetStageToggleSectionName()))
	{
		return;
	}

	const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	UDMMaterialStage* Stage = MenuContext->GetStage();
	if (!Stage)
	{
		return;
	}

	UDMMaterialLayerObject* Layer = MenuContext->GetLayer();
	if (!Layer)
	{
		return;
	}

	UDMMaterialSlot* Slot = Layer->GetSlot();
	if (!Slot)
	{
		return;
	}

	const int32 LayerIndex = Layer->FindIndex();

	const bool bAllowRemoveLayer = Slot->CanRemoveLayer(Layer);
	const EDMMaterialLayerStage StageType = Layer->GetStageType(Stage);

	if (bAllowRemoveLayer || StageType == EDMMaterialLayerStage::Mask)
	{
		FToolMenuSection& NewSection = InMenu->AddSection(GetStageToggleSectionName(), LOCTEXT("MaterialStageMenu", "Material Stage"));

		if (bAllowRemoveLayer)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ToggleLayer", "Toggle"),
				LOCTEXT("ToggleLayerTooltip", "Toggle the entire layer on and off.\n\n"
					"Warning: Toggling a layer off may result in inputs being reset where incompatibilities are found.\n\nAlt+Left Click"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateWeakLambda(
					Layer,
					[Layer]()
					{
						FScopedTransaction Transaction(LOCTEXT("ToggleAllStageEnabled", "Toggle All Stage Enabled"));

						for (UDMMaterialStage* Stage : Layer->GetStages(EDMMaterialLayerStage::All))
						{
							Stage->Modify();
							Stage->SetEnabled(!Stage->IsEnabled());
						}
					}
				))
			);

			if (StageType == EDMMaterialLayerStage::Base)
			{
				NewSection.AddMenuEntry(NAME_None,
					LOCTEXT("ToggleLayerBase", "Toggle Base"),
					LOCTEXT("ToggleLayerBaseTooltip", "Toggle the layer base on and off.\n\n"
						"Warning: Toggling a layer base off may result in inputs being reset where incompatibilities are found.\n\nAlt+Shift+Left Click"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						Layer,
						[Layer]()
						{
							FScopedTransaction Transaction(LOCTEXT("ToggleBaseStageEnabled", "Toggle Base Stage Enabled"));

							if (UDMMaterialStage* Stage = Layer->GetStage(EDMMaterialLayerStage::Base))
							{
								Stage->Modify();
								Stage->SetEnabled(!Stage->IsEnabled());
							}
						}
					))
				);
			}
		}

		if (StageType == EDMMaterialLayerStage::Mask)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ToggleLayerMask", "Toggle Mask"),
				LOCTEXT("ToggleLayerMaskTooltip", "Toggle the layer mask on and off.\n\nAlt+Shift+Left Click"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateWeakLambda(
					Layer,
					[Layer]()
					{
						FScopedTransaction Transaction(LOCTEXT("ToggleBaseStageEnabled", "Toggle Base Stage Enabled"));

						if (UDMMaterialStage* Stage = Layer->GetStage(EDMMaterialLayerStage::Mask))
						{
							Stage->Modify();
							Stage->SetEnabled(!Stage->IsEnabled());
						}
					}
				))
			);
		}

		if (bAllowRemoveLayer)
		{
			if (TSharedPtr<SDMMaterialEditor> EditorWidget = MenuContext->GetEditorWidget())
			{
				NewSection.AddMenuEntry(NAME_None,
					LOCTEXT("RemoveLayer", "Remove"),
					LOCTEXT("RemoveLayerTooltip", "Remove this layer from its Material Slot.\n\n"
						"Warning: Removing a stage off may result in inputs being reset where incompatibilities are found."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						Layer,
						[Layer]()
						{
							FScopedTransaction Transaction(LOCTEXT("RemoverLayer", "Remover Layer"));

							if (UDMMaterialSlot* Slot = Layer->GetSlot())
							{
								Slot->RemoveLayer(Layer);
							}
						}
					))
				);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
