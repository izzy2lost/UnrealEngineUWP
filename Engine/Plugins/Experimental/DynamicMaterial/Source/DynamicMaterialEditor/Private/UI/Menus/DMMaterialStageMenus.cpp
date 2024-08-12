// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Menus/DMMaterialStageMenus.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "ScopedTransaction.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UI/Menus/DMMenuContext.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialStage.h"
#include "UI/Widgets/SDMMaterialEditor.h"

#define LOCTEXT_NAMESPACE "FDMMaterialStageMenus"

namespace UE::DynamicMaterialEditor::Private
{
	const FLazyName StageMenuName = TEXT("MaterialDesigner.MaterialStage");
	const FLazyName StageMenuToggleName = TEXT("StageToggle");
}

TSharedRef<SWidget> FDMMaterialStageMenus::GenerateStageMenu(const TSharedPtr<SDMMaterialSlotEditor>& InSlotWidget,
	const TSharedPtr<SDMMaterialStage>& InStageWidget)
{
	using namespace UE::DynamicMaterialEditor::Private;

	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ToolMenus->IsMenuRegistered(StageMenuName))
	{
		UToolMenu* NewToolMenu = UDMMenuContext::GenerateContextMenuDefault(StageMenuName);

		if (!NewToolMenu)
		{
			return SNullWidget::NullWidget;
		}

		NewToolMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&FDMMaterialStageMenus::AddStageSection));
	}

	FToolMenuContext MenuContext(UDMMenuContext::CreateStage(InSlotWidget->GetEditorWidget(), InStageWidget));

	return ToolMenus->GenerateWidget(StageMenuName, MenuContext);
}

void FDMMaterialStageMenus::AddStageSection(UToolMenu* InMenu)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!IsValid(InMenu) || InMenu->ContainsSection(StageMenuToggleName))
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

	UDMMaterialLayerObject* Layer = Stage->GetLayer();
	if (!Layer)
	{
		return;
	}

	UDMMaterialSlot* Slot = Layer->GetSlot();
	if (!Slot)
	{
		return;
	}

	const bool bAllowRemoveLayer = Slot->CanRemoveLayer(Layer);
	const EDMMaterialLayerStage StageType = Layer->GetStageType(Stage);

	FToolMenuSection& NewSection = InMenu->AddSection(StageMenuToggleName, LOCTEXT("MaterialStageMenu", "Material Stage"));

	if (!bAllowRemoveLayer && StageType != EDMMaterialLayerStage::Mask)
	{
		return;
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

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("ToggleLayer", "Toggle Layer"),
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
	}

	if (bAllowRemoveLayer)
	{
		if (TSharedPtr<SDMMaterialEditor> EditorWidget = MenuContext->GetEditorWidget())
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("RemoveLayer", "Remove Layer"),
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

#undef LOCTEXT_NAMESPACE
