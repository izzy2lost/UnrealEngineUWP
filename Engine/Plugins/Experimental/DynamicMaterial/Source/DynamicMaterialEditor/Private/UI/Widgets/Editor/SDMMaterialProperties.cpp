// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/SDMMaterialProperties.h"

#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"
#include "DetailLayoutBuilder.h"
#include "DMWorldSubsystem.h"
#include "Engine/World.h"
#include "ICustomDetailsView.h"
#include "Items/ICustomDetailsViewCustomCategoryItem.h"
#include "Items/ICustomDetailsViewCustomItem.h"
#include "Items/ICustomDetailsViewItem.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Styling/StyleColors.h"
#include "UI/Utils/DMWidgetStatics.h"
#include "UI/Widgets/Editor/SDMMaterialPropertySelector.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "UI/Widgets/Visualizers/SDMMaterialComponentPreview.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialProperties"

void SDMMaterialProperties::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SDMMaterialProperties::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget)
{
	EditorWidgetWeak = InEditorWidget;

	Content = TDMWidgetSlot<SWidget>(SharedThis(this), 0, CreateSlot_Content());
}

void SDMMaterialProperties::Validate()
{
	if (Content.HasBeenInvalidated())
	{
		Content << CreateSlot_Content();
	}
}

TSharedRef<SWidget> SDMMaterialProperties::CreateSlot_Content()
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModel* MaterialModel = MaterialModelBase->ResolveMaterialModel();

	if (!MaterialModel)
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return SNullWidget::NullWidget;
	}

	FCustomDetailsViewArgs Args;
	Args.bAllowGlobalExtensions = false;
	Args.bAllowResetToDefault = false;
	Args.bShowCategories = true;
	Args.OnExpansionStateChanged.AddSP(this, &SDMMaterialProperties::OnExpansionStateChanged);

	TSharedRef<ICustomDetailsView> DetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(Args);
	FCustomDetailsViewItemId RootId = DetailsView->GetRootItem()->GetItemId();

	auto CreateCategory = [&DetailsView, &RootId, MaterialModelBase](FName InName, const FText& InDisplayName)
		{
			TSharedRef<ICustomDetailsViewItem> CategoryItem = DetailsView->CreateCustomCategoryItem(InName, InDisplayName)->AsItem();
			CategoryItem->RefreshItemId();
			DetailsView->ExtendTree(RootId, ECustomDetailsTreeInsertPosition::Child, CategoryItem);

			bool bExpansionState = true;
			FDMWidgetStatics::Get().GetExpansionState(MaterialModelBase, InName, bExpansionState);

			DetailsView->SetItemExpansionState(
				CategoryItem->GetItemId(),
				bExpansionState ? ECustomDetailsViewExpansion::SelfExpanded : ECustomDetailsViewExpansion::Collapsed
			);

			return CategoryItem;
		};

	TSharedRef<ICustomDetailsViewItem> ActiveCategory = CreateCategory(TEXT("Active"), LOCTEXT("ActiveCategory", "Active Channels"));
	TSharedRef<ICustomDetailsViewItem> InactiveCategory = CreateCategory(TEXT("Inactive"), LOCTEXT("InactiveCategory", "Inactive Channels"));
	TSharedRef<ICustomDetailsViewItem> IncompatibleCategory = CreateCategory(TEXT("Incompatible"), LOCTEXT("IncompatibleCategory", "Incompatible Channels"));

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	// Active properties first.
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &DetailsView, &ActiveCategory, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled() && MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (bIsActive)
				{
					AddProperty(DetailsView, ActiveCategory, MaterialProperty);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	// Now inactive properties
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &DetailsView, &InactiveCategory, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled();
				const bool bIsValid = MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (!bIsActive && bIsValid)
				{
					AddProperty(DetailsView, InactiveCategory, MaterialProperty);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	// Now invalid properties
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &DetailsView, &IncompatibleCategory, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled();
				const bool bIsValid = MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (!bIsActive && !bIsValid)
				{
					AddProperty(DetailsView, IncompatibleCategory, MaterialProperty);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	DetailsView->RebuildTree(ECustomDetailsViewBuildType::InstantBuild);

	return DetailsView;
}

void SDMMaterialProperties::AddProperty(const TSharedRef<ICustomDetailsView>& InDetailsView, const TSharedRef<ICustomDetailsViewItem>& InCategory,
	UDMMaterialProperty* InProperty)
{
	if (!InProperty)
	{
		return;
	}

	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return;
	}

	TSharedPtr<ICustomDetailsViewCustomItem> Item = InDetailsView->CreateCustomItem(InProperty->GetClass()->GetFName());

	if (!Item.IsValid())
	{
		return;
	}

	Item->SetWholeRowWidget(CreatePropertyRow(InProperty));

	InDetailsView->ExtendTree(InCategory->GetItemId(), ECustomDetailsTreeInsertPosition::Child, Item->AsItem());
}

TSharedRef<SWidget> SDMMaterialProperties::CreatePropertyRow(UDMMaterialProperty* InProperty)
{
	// There are all ensured to be valid by the caller of this method
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();
	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	EDMMaterialPropertyType MaterialProperty = InProperty->GetMaterialProperty();

	TSharedRef<SBox> PreviewWidgetContainer = SNew(SBox)
		.WidthOverride(64.f)
		.HeightOverride(18.f);

	TSharedRef<SWidget> PropertyName = CreateSlot_PropertyName(MaterialProperty);

	TSharedRef<SWidget> Slider = SNullWidget::NullWidget;

	const bool bValidProperty = InProperty->IsValidForModel(*EditorOnlyData);
	const bool bPropertyEnabled = bValidProperty && InProperty->IsEnabled() && EditorOnlyData->GetSlotForMaterialProperty(MaterialProperty);;

	if (bPropertyEnabled)
	{
		MaterialProperty = InProperty->GetMaterialProperty();

		PreviewWidgetContainer = SNew(SBox)
			.WidthOverride(64.f)
			.HeightOverride(64.f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(1, 1, 1, 1.f))
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush(UE::DynamicMaterialEditor::Private::EditorDarkBackground))
				[
					SNew(SDMMaterialComponentPreview, EditorWidget.ToSharedRef(), InProperty)
					.PreviewSize(FVector2D(60.f, 60.f))
				]
			];

		PreviewWidgetContainer->SetCursor(EMouseCursor::Hand);
		PreviewWidgetContainer->SetOnMouseButtonUp(FPointerEventHandler::CreateSP(this, &SDMMaterialProperties::OnPropertyClicked, MaterialProperty));

		PropertyName->SetCursor(EMouseCursor::Hand);
		PropertyName->SetOnMouseButtonUp(FPointerEventHandler::CreateSP(this, &SDMMaterialProperties::OnPropertyClicked, MaterialProperty));

		Slider = CreateGlobalSlider(InProperty);
	}

	// If the property is just disabled, leave the slider widget blank.

	return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Top)
			.Padding(0.f, 5.f, 0.f, 5.f)
			[
				PreviewWidgetContainer
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Fill)
			.Padding(5.f, 5.f, 0.f, 5.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.VAlign(EVerticalAlignment::VAlign_Center)
					[
						CreateSlot_EnabledButton(MaterialProperty)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(5.f, 0.f, 0.f, 0.f)
					[
						PropertyName
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					Slider
				]
			];
}

TSharedRef<SWidget> SDMMaterialProperties::CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FDMMaterialEditorPage Page = {EDMMaterialEditorMode::EditSlot, InMaterialProperty};
	const FText Format = LOCTEXT("PropertyEnableFormat", "Toggle the {0} property.\n\nProperty must be valid for the Material Type.");
	const FText ToolTip = FText::Format(Format, SDMMaterialPropertySelector::GetSelectButtonText(Page, /* Short Name */ false));

	return SNew(SCheckBox)
		.IsEnabled(this, &SDMMaterialProperties::GetPropertyEnabledEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialProperties::GetPropertyEnabledState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialProperties::OnPropertyEnabledStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip);
}

TSharedRef<SWidget> SDMMaterialProperties::CreateSlot_PropertyName(EDMMaterialPropertyType InMaterialProperty)
{
	const FDMMaterialEditorPage Page = {EDMMaterialEditorMode::EditSlot, InMaterialProperty};

	return SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(SDMMaterialPropertySelector::GetSelectButtonText(Page, /* Short Name */ false))
		.ToolTipText(SDMMaterialPropertySelector::GetSelectButtonText(Page, /* Short Name */ false));
}

bool SDMMaterialProperties::GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return false;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return false;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return false;
	}

	UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!Property)
	{
		return false;
	}

	return Property->IsValidForModel(*EditorOnlyData);
}

ECheckBoxState SDMMaterialProperties::GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return ECheckBoxState::Unchecked;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return ECheckBoxState::Unchecked;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return ECheckBoxState::Unchecked;
	}

	UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!Property)
	{
		return ECheckBoxState::Unchecked;
	}

	return (Property->IsEnabled() && EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty))
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMMaterialProperties::OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty)
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return;
	}

	const bool bEnabled = InState == ECheckBoxState::Checked;

	MaterialProperty->SetEnabled(bEnabled);

	if (bEnabled)
	{
		UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty);

		if (!Slot)
		{
			EditorOnlyData->AddSlotForMaterialProperty(InMaterialProperty);
		}
	}

	Content.Invalidate();

	// Make sure we go back to the property previews
	EditorWidget->EditProperties();
}

FReply SDMMaterialProperties::OnPropertyClicked(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, 
	EDMMaterialPropertyType InMaterialProperty)
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return FReply::Handled();
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return FReply::Handled();
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return FReply::Handled();
	}

	UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty);

	if (!Slot)
	{
		return FReply::Handled();
	}

	EditorWidget->SelectProperty(InMaterialProperty);

	return FReply::Handled();
}

TSharedRef<SWidget> SDMMaterialProperties::CreateGlobalSlider(UDMMaterialProperty* InProperty)
{
	if (!InProperty)
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return SNullWidget::NullWidget;
	}

	UObject* AlphaObject = InProperty->GetComponent(UDynamicMaterialModelEditorOnlyData::AlphaValueName);

	if (!AlphaObject)
	{
		return SNullWidget::NullWidget;
	}

	if (UDynamicMaterialModelDynamic* MaterialModelDynamic = Cast<UDynamicMaterialModelDynamic>(MaterialModelBase))
	{
		AlphaObject = MaterialModelDynamic->GetComponentDynamic(AlphaObject->GetFName());

		if (!AlphaObject)
		{
			return SNullWidget::NullWidget;
		}
	}

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = nullptr;

	if (UWorld* World = MaterialModelBase->GetWorld())
	{
		if (const UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
		{
			KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
		}
	}

	TGuardValue<bool> ConstructGuard(bConstructing, true);

	FCustomDetailsViewArgs Args;
	Args.KeyframeHandler = KeyframeHandler;
	Args.bAllowGlobalExtensions = true;
	Args.bAllowResetToDefault = true;
	Args.bShowCategories = false;

	TSharedRef<ICustomDetailsView> DetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(Args);
	FCustomDetailsViewItemId RootId = DetailsView->GetRootItem()->GetItemId();

	FDMPropertyHandle PropertyHandle = FDMWidgetStatics::Get().GetPropertyHandle(this, AlphaObject, UDMMaterialValue::ValueName);

	TSharedRef<ICustomDetailsViewItem> AlphaItem = DetailsView->CreateDetailTreeItem(PropertyHandle.DetailTreeNode.ToSharedRef());

	if (UDMMaterialValue* AlphaValue = Cast<UDMMaterialValue>(AlphaObject))
	{
		AlphaItem->SetResetToDefaultOverride(FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateUObject(AlphaValue, &UDMMaterialValue::CanResetToDefault),
			FResetToDefaultHandler::CreateUObject(AlphaValue, &UDMMaterialValue::ResetToDefault)
		));
	}
	else if (UDMMaterialValueDynamic* AlphaValueDynamic = Cast<UDMMaterialValueDynamic>(AlphaObject))
	{
		AlphaItem->SetResetToDefaultOverride(FResetToDefaultOverride::Create(
			FIsResetToDefaultVisible::CreateUObject(AlphaValueDynamic, &UDMMaterialValueDynamic::CanResetToDefault),
			FResetToDefaultHandler::CreateUObject(AlphaValueDynamic, &UDMMaterialValueDynamic::ResetToDefault)
		));
	}

	AlphaItem->MakeWidget(nullptr, SharedThis(this));

	TSharedPtr<SWidget> ValueWidget = AlphaItem->GetWidget(ECustomDetailsViewWidgetType::Value);
	TSharedPtr<SWidget> ExtensionWidget = AlphaItem->GetWidget(ECustomDetailsViewWidgetType::Extensions);

	const FText PropertyGlobalSliderToolTipFormat = LOCTEXT("PropertyGlobalSliderToolTipFormat", "Change the global {0} value.");

	const FText PropertyGlobalSliderToolTip = FText::Format(
		PropertyGlobalSliderToolTipFormat,
		UE::DynamicMaterialEditor::Private::GetMaterialPropertyLongDisplayName(InProperty->GetMaterialProperty())
	);

	return SNew(SHorizontalBox)
		.ToolTipText(PropertyGlobalSliderToolTip)
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.HeightOverride(32.f)
			.VAlign(VAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				ValueWidget.IsValid() ? ValueWidget.ToSharedRef() : SNullWidget::NullWidget
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SBox)
			.HeightOverride(32.f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				ExtensionWidget.IsValid() ? ExtensionWidget.ToSharedRef() : SNullWidget::NullWidget
			]
		];
}

void SDMMaterialProperties::OnExpansionStateChanged(const TSharedRef<ICustomDetailsViewItem>& InItem, bool bInExpansionState)
{
	if (bConstructing)
	{
		return;
	}

	const FCustomDetailsViewItemId& ItemId = InItem->GetItemId();

	if (ItemId.GetItemType() != static_cast<uint32>(EDetailNodeType::Category))
	{
		return;
	}

	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase();

	if (!MaterialModelBase)
	{
		return;
	}

	FDMWidgetStatics::Get().SetExpansionState(MaterialModelBase, *ItemId.GetItemName(), bInExpansionState);
}

#undef LOCTEXT_NAMESPACE
