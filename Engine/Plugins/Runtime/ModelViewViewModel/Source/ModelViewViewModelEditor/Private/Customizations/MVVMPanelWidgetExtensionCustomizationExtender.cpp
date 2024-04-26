// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMPanelWidgetExtensionCustomizationExtender.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PanelWidget.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Extensions/MVVMViewBlueprintPanelWidgetExtension.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModel.h"
#include "MVVMDeveloperProjectSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MVVMPanelWidgetExtensionCustomizationExtender"

namespace UE::MVVM
{

TSharedPtr<FMVVMPanelWidgetExtensionCustomizationExtender> FMVVMPanelWidgetExtensionCustomizationExtender::MakeInstance()
{
	return MakeShared<FMVVMPanelWidgetExtensionCustomizationExtender>();
}

void FMVVMPanelWidgetExtensionCustomizationExtender::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout, const TArrayView<UWidget*> InWidgets, const TSharedRef<FWidgetBlueprintEditor>& InWidgetBlueprintEditor)
{
	// multi-selection not supported for the data
	if (InWidgets.Num() == 1)
	{
		if (UPanelWidget* Panel = Cast<UPanelWidget>(InWidgets[0]))
		{
			if (Panel->CanHaveMultipleChildren())
			{
				if (GetDefault<UMVVMDeveloperProjectSettings>()->IsExtensionSupportedForPanelClass(Panel->GetClass()))
				{
					FName NAME_ViewmodelExtension = "ViewmodelExtension";
					Widget = Panel;
					WidgetBlueprintEditor = InWidgetBlueprintEditor;

					// Only do a customization if we have a MVVM blueprint view class on this blueprint.
					if (GetExtensionViewForSelectedWidgetBlueprint())
					{
						IDetailCategoryBuilder& MVVMCategory = InDetailLayout.EditCategory("Viewmodel");

						bIsExtensionAdded = GetPanelWidgetExtension() != nullptr;

						// Add a button that controls adding/removing the extension on the panel widget
						MVVMCategory.AddCustomRow(FText::FromString(TEXT("Viewmodel")))
						.RowTag(NAME_ViewmodelExtension)
						.NameContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("VMSupport", "Viewmodel Support"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
						.ValueContent()
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.OnClicked(this, &FMVVMPanelWidgetExtensionCustomizationExtender::ModifyExtension)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(SImage)
										.Image(this, &FMVVMPanelWidgetExtensionCustomizationExtender::GetExtensionButtonIcon)
									]
									+ SHorizontalBox::Slot()
									.Padding(FMargin(3.0f, 0.0f, 0.0f, 0.0f))
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(STextBlock)
										.TextStyle(FAppStyle::Get(), "SmallButtonText")
										.Text(this, &FMVVMPanelWidgetExtensionCustomizationExtender::GetExtensionButtonText)
									]
								]
							]
						];

						if (UMVVMBlueprintViewExtension_PanelWidget* PanelExtension = GetPanelWidgetExtension())
						{
							IDetailPropertyRow* PanelExtensionPropertyRow = MVVMCategory.AddExternalObjects({ PanelExtension});
							TSharedPtr<IPropertyHandle> PanelExtensionObjectHandle = PanelExtensionPropertyRow->GetPropertyHandle();
							PanelExtensionPropertyRow->Visibility(EVisibility::Collapsed);

							// "Entry Widget Class" property row
							EntryClassHandle = PanelExtensionObjectHandle->GetChildHandle("EntryWidgetClass");
							EntryClassHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FMVVMPanelWidgetExtensionCustomizationExtender::HandleEntryClassChanged, false));
							HandleEntryClassChanged(true);
							IDetailPropertyRow& EntryClassRow = MVVMCategory.AddProperty(EntryClassHandle);

							// "Entry Viewmodel" property row
							MVVMCategory.AddCustomRow(FText::FromString(TEXT("Viewmodel")))
								.RowTag(NAME_ViewmodelExtension)
								.NameContent()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("EntryVM", "Entry Viewmodel"))
									.Font(IDetailLayoutBuilder::GetDetailFont())
								]
								.ValueContent()
								.HAlign(HAlign_Fill)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										SNew(SComboButton)
										.OnGetMenuContent(this, &FMVVMPanelWidgetExtensionCustomizationExtender::OnGetViewModelsMenuContent)
										.ButtonContent()
										[
											SNew(STextBlock)
											.Text(this, &FMVVMPanelWidgetExtensionCustomizationExtender::OnGetSelectedViewModel)
										]
									]
									+ SHorizontalBox::Slot()
									.AutoWidth()
									[
										PropertyCustomizationHelpers::MakeClearButton(
											FSimpleDelegate::CreateSP(this, &FMVVMPanelWidgetExtensionCustomizationExtender::ClearEntryViewModel))
									]
								];

							// "Slot template" property row
							IDetailPropertyRow* SlotDetailRow = MVVMCategory.AddExternalObjects({ PanelExtension->SlotObj }, EPropertyLocation::Default,
								FAddPropertyParams()
								.CreateCategoryNodes(false)
								.AllowChildren(true)
								.HideRootObjectNode(false)
							);

							TSharedPtr<IPropertyHandle> SlotPropertyHandle = SlotDetailRow->GetPropertyHandle();

							SlotDetailRow->CustomWidget(true)
								.RowTag(NAME_ViewmodelExtension)
								.NameContent()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SlotTemplate", "Slot Template"))
									.Font(IDetailLayoutBuilder::GetDetailFont())
								]
								.ValueContent()
								[
									SlotPropertyHandle->CreatePropertyValueWidget()
								];

							// Because AddExternalObjects was used the property system will not add a reset to default widget by default 
							SlotDetailRow->OverrideResetToDefault(
								FResetToDefaultOverride::Create(
									FIsResetToDefaultVisible::CreateLambda([SlotPropertyHandle](TSharedPtr<IPropertyHandle> Handle)
										{
											return SlotPropertyHandle->CanResetToDefault();
										}),
									FResetToDefaultHandler::CreateLambda([SlotPropertyHandle](TSharedPtr<IPropertyHandle> Handle)
										{
											return SlotPropertyHandle->ResetToDefault();
										})
									)
							);
						}
					}
				}
			}
		}
	}
}

FReply FMVVMPanelWidgetExtensionCustomizationExtender::ModifyExtension()
{
	if (UMVVMBlueprintViewExtension_PanelWidget* PanelExtension = GetPanelWidgetExtension())
	{
		if (UPanelWidget* WidgetPtr = Widget.Get())
		{
			GetExtensionViewForSelectedWidgetBlueprint()->RemoveBlueprintWidgetExtension(PanelExtension, WidgetPtr->GetFName());
			bIsExtensionAdded = false;
		}
	}
	else
	{
		CreatePanelWidgetViewExtensionIfNotExisting();
		bIsExtensionAdded = true;
	}
	return FReply::Handled();
}

void FMVVMPanelWidgetExtensionCustomizationExtender::CreatePanelWidgetViewExtensionIfNotExisting()
{
	if (UMVVMWidgetBlueprintExtension_View* Extension = GetExtensionViewForSelectedWidgetBlueprint())
	{
		if (UPanelWidget* WidgetPtr = Widget.Get())
		{
			if (Extension->GetBlueprintExtensionsForWidget(WidgetPtr->GetFName()).IsEmpty())
			{
				UMVVMBlueprintViewExtension* NewExtension = Extension->CreateBlueprintWidgetExtension(UMVVMBlueprintViewExtension_PanelWidget::StaticClass(), WidgetPtr->GetFName());
				UMVVMBlueprintViewExtension_PanelWidget* NewPanelWidgetExtension = CastChecked<UMVVMBlueprintViewExtension_PanelWidget>(NewExtension);
				NewPanelWidgetExtension->WidgetName = WidgetPtr->GetFName();

				UPanelSlot* SlotObj = NewObject<UPanelSlot>(NewPanelWidgetExtension, WidgetPtr->GetSlotClass(), NAME_None, RF_Transactional);
				NewPanelWidgetExtension->SlotObj = SlotObj;
			}
		}
	}
}

UMVVMBlueprintViewExtension_PanelWidget* FMVVMPanelWidgetExtensionCustomizationExtender::GetPanelWidgetExtension() const
{
	if (UMVVMWidgetBlueprintExtension_View* ViewClass = GetExtensionViewForSelectedWidgetBlueprint())
	{
		if (UPanelWidget* WidgetPtr = Widget.Get())
		{
			for (UMVVMBlueprintViewExtension* Extension : ViewClass->GetBlueprintExtensionsForWidget(WidgetPtr->GetFName()))
			{
				if (UMVVMBlueprintViewExtension_PanelWidget* PanelWidgetExtension = Cast<UMVVMBlueprintViewExtension_PanelWidget>(Extension))
				{
					return PanelWidgetExtension;
				}
			}
		}
	}

	return nullptr;
}

UMVVMWidgetBlueprintExtension_View* FMVVMPanelWidgetExtensionCustomizationExtender::GetExtensionViewForSelectedWidgetBlueprint() const
{
	if (const TSharedPtr<FWidgetBlueprintEditor> BPEditor = WidgetBlueprintEditor.Pin())
	{
		if (const UWidgetBlueprint* Blueprint = BPEditor->GetWidgetBlueprintObj())
		{
			return UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(Blueprint);
		}
	}

	return nullptr;
}

void FMVVMPanelWidgetExtensionCustomizationExtender::ClearEntryViewModel()
{
	SetEntryViewModel(FGuid());
}

void FMVVMPanelWidgetExtensionCustomizationExtender::HandleEntryClassChanged(bool bIsInit)
{
	// Update the cached value of entry class
	void* EntryClassPtr = nullptr;
	TSubclassOf<UUserWidget>* EntryClassValue = nullptr;
	bool bEntryClassChanged = false;

	if (EntryClassHandle->IsValidHandle() && EntryClassHandle->GetValueData(EntryClassPtr) == FPropertyAccess::Success)
	{
		EntryClassValue = reinterpret_cast<TSubclassOf<UUserWidget>*>(EntryClassPtr);
	}

	if (!EntryClassValue || !EntryClass.Get() || EntryClass.Get() != *EntryClassValue)
	{
		bEntryClassChanged = true;
	}
	EntryClass = EntryClassValue ? *EntryClassValue : nullptr;

	// Update other values that depend on the entry class (only if the cached value actually changed)
	if (bEntryClassChanged && EntryClass)
	{
		if (UUserWidget* EntryCDO = Cast<UUserWidget>(EntryClass->ClassDefaultObject))
		{
			EntryWidgetBlueprint = Cast<UWidgetBlueprint>(EntryCDO->GetClass()->ClassGeneratedBy);
		}

		// Clear the saved entry viewmodel if we're not calling this from CustomizeDetails (not initializing the customizer)
		if (!bIsInit)
		{
			SetEntryViewModel(FGuid(), false);
		}
	}
}

FText FMVVMPanelWidgetExtensionCustomizationExtender::OnGetSelectedViewModel() const
{
	if (const UPanelWidget* WidgetPtr = Widget.Get())
	{
		if (EntryClass)
		{
			if (UMVVMBlueprintViewExtension_PanelWidget* PanelWidgetExtension = GetPanelWidgetExtension())
			{
				if (const UUserWidget* EntryUserWidget = Cast<UUserWidget>(EntryClass->ClassDefaultObject))
				{
					if (const UWidgetBlueprint* EntryBlueprint = Cast<UWidgetBlueprint>(EntryUserWidget->GetClass()->ClassGeneratedBy))
					{
						if (const UMVVMWidgetBlueprintExtension_View* EntryWidgetExtension = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(EntryBlueprint))
						{
							if (const UMVVMBlueprintView* EntryWidgetView = EntryWidgetExtension->GetBlueprintView())
							{
								if (const FMVVMBlueprintViewModelContext* ViewModelContext = EntryWidgetView->FindViewModel(PanelWidgetExtension->GetEntryViewModelId()))
								{
									return FText::FromName(ViewModelContext->GetViewModelName());
								}
							}
						}
					}
				}
			}
		}
	}

	return LOCTEXT("NoViewmodel", "No Viewmodel");
}

FText FMVVMPanelWidgetExtensionCustomizationExtender::GetExtensionButtonText() const
{
	return bIsExtensionAdded ? LOCTEXT("RemoveVMExt", "Remove Viewmodel Extension") : LOCTEXT("AddVMExt", "Add Viewmodel Extension");
}

const FSlateBrush* FMVVMPanelWidgetExtensionCustomizationExtender::GetExtensionButtonIcon() const
{
	return bIsExtensionAdded ? FAppStyle::Get().GetBrush("Icons.X") : FAppStyle::Get().GetBrush("Icons.Plus");
}

TSharedRef<SWidget> FMVVMPanelWidgetExtensionCustomizationExtender::OnGetViewModelsMenuContent()
{
	FMenuBuilder MenuBuilder(true, NULL);

	if (EntryClass)
	{
		// Find all viewmodels in the entry widget
		if (const UWidgetBlueprint* EntryWidgetBlueprintPtr = EntryWidgetBlueprint.Get())
		{
			if (const UMVVMWidgetBlueprintExtension_View* EntryWidgetExtension = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(EntryWidgetBlueprintPtr))
			{
				if (const UMVVMBlueprintView* EntryWidgetView = EntryWidgetExtension->GetBlueprintView())
				{
					const TArrayView<const FMVVMBlueprintViewModelContext> ViewModels = EntryWidgetView->GetViewModels();
					for (const FMVVMBlueprintViewModelContext& EntryViewModel : ViewModels)
					{
						// Create the menu action for this entry viewmodel
						FUIAction ItemAction(FExecuteAction::CreateSP(this, &FMVVMPanelWidgetExtensionCustomizationExtender::SetEntryViewModel, EntryViewModel.GetViewModelId(), true));
						MenuBuilder.AddMenuEntry(FText::FromName(EntryViewModel.GetViewModelName()), TAttribute<FText>(), FSlateIcon(), ItemAction);
					}
				}
			}
		}
	}
	return MenuBuilder.MakeWidget();
}

void FMVVMPanelWidgetExtensionCustomizationExtender::SetEntryViewModel(FGuid InEntryViewModelId, bool bMarkModified)
{
	if (UMVVMWidgetBlueprintExtension_View* Extension = GetExtensionViewForSelectedWidgetBlueprint())
	{
		if (const UPanelWidget* WidgetPtr = Widget.Get())
		{
			if (UMVVMBlueprintViewExtension_PanelWidget* PanelWidgetExtension = GetPanelWidgetExtension())
			{
				if (PanelWidgetExtension->EntryViewModelId != InEntryViewModelId)
				{
					const FScopedTransaction Transaction(LOCTEXT("SetEntryViewModel", "Set Entry ViewModel"));
					PanelWidgetExtension->Modify();
					PanelWidgetExtension->EntryViewModelId = InEntryViewModelId;
					if (bMarkModified)
					{
						if (const TSharedPtr<FWidgetBlueprintEditor> BPEditor = WidgetBlueprintEditor.Pin())
						{
							if (UWidgetBlueprint* Blueprint = BPEditor->GetWidgetBlueprintObj())
							{
								FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
							}
						}
					}
				}
			}
		}
	}
}

}
#undef LOCTEXT_NAMESPACE