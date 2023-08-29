// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSlotDefinitionDataProxyDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SmartObjectDefinition.h"
#include "ScopedTransaction.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "InstancedStructDetails.h"
#include "SmartObjectViewModel.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"
#include "Modules/ModuleManager.h"
#include "StructViewerModule.h"
#include "StructViewerFilter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "SmartObjectEditorStyle.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"



////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FSmartObjectSlotDefinitionDataProxyDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectSlotDefinitionDataProxyDetails);
}

void FSmartObjectSlotDefinitionDataProxyDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	DataPropertyHandle = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinitionDataProxy, Data));
	check(DataPropertyHandle);

	// Get ID and viewmodel from definition data.
	const FGuid ItemID = GetItemID();
	TSharedPtr<FSmartObjectViewModel> ViewModel = GetViewModel();
	
	HeaderRow
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Padding(FMargin(4,1))
			.BorderImage_Lambda([ViewModel, ItemID]()
			{
				bool bSelected = false;
				if (ViewModel.IsValid())
				{
					bSelected = ViewModel->IsSelected(ItemID);
				}
				return bSelected ? FSmartObjectEditorStyle::Get().GetBrush("ItemSelection") : nullptr;
			})
			.OnMouseButtonDown_Lambda([ViewModel, ItemID](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
			{
				if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
				{
					const bool bToggleSelection = MouseEvent.IsShiftDown() || MouseEvent.IsControlDown();
					if (ViewModel.IsValid())
					{
						if (bToggleSelection)
						{
							if (ViewModel->IsSelected(ItemID))
							{
								ViewModel->RemoveFromSelection(ItemID);
							}
							else
							{
								ViewModel->AddToSelection(ItemID);
							}
						}
						else
						{
							ViewModel->SetSelection({ ItemID });
						}
					}
				}
				return FReply::Unhandled();
			})
			
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0, 0, 4, 0))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("SCS.Component"))
					.ColorAndOpacity(FColor(255,255,255,128))
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(ComboButton, SComboButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
					.OnGetMenuContent(this, &FSmartObjectSlotDefinitionDataProxyDetails::GenerateStructPicker)
					.ContentPadding(0)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &FSmartObjectSlotDefinitionDataProxyDetails::GetDefinitionDataName)
							.ToolTipText(LOCTEXT("SelectDefinitionDataType", "Select Definition Data Type"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					StructProperty->CreateDefaultPropertyButtonWidgets()
				]
			]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectSlotDefinitionDataProxyDetails::OnCopy)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectSlotDefinitionDataProxyDetails::OnPaste)));
}

FText FSmartObjectSlotDefinitionDataProxyDetails::GetDefinitionDataName() const
{
	check(StructProperty);
	// Note: We pick the first struct, we assume that multi-selection is not used.
	const UScriptStruct* Struct = nullptr;
	StructProperty->EnumerateConstRawData([&Struct](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			Struct = static_cast<const FSmartObjectSlotDefinitionDataProxy*>(RawData)->Data.GetScriptStruct();
			return false; // stop
		}
		return true;
	});

	if (Struct)
	{
		return Struct->GetDisplayNameText();
	}
	return LOCTEXT("None", "None");
}

FGuid FSmartObjectSlotDefinitionDataProxyDetails::GetItemID() const
{
	// Note: We pick the first ID, we assume that multi-selection is not used.
	FGuid ItemID;
	StructProperty->EnumerateConstRawData([&ItemID](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			ItemID = static_cast<const FSmartObjectSlotDefinitionDataProxy*>(RawData)->ID;
			return false; // stop
		}
		return true;
	});
	return ItemID;
}

TSharedPtr<FSmartObjectViewModel> FSmartObjectSlotDefinitionDataProxyDetails::GetViewModel() const
{
	const USmartObjectDefinition* Definition = nullptr;
	
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < OuterObjects.Num(); ObjectIdx++)
	{
		if (const USmartObjectDefinition* OuterDefinition = Cast<USmartObjectDefinition>(OuterObjects[ObjectIdx]))
		{
			Definition = OuterDefinition;
			break;
		}
		if (const USmartObjectDefinition* OuterDefinition = OuterObjects[ObjectIdx]->GetTypedOuter<USmartObjectDefinition>())
		{
			Definition = OuterDefinition;
			break;
		}
	}

	return FSmartObjectViewModel::Get(Definition);
}

void FSmartObjectSlotDefinitionDataProxyDetails::OnCopy() const
{
	FString Value;
	if (StructProperty->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FSmartObjectSlotDefinitionDataProxyDetails::OnPaste() const
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	FScopedTransaction Transaction(LOCTEXT("PasteDefinitionData", "Paste Definition Data"));

	StructProperty->NotifyPreChange();

	if (StructProperty->SetValueFromFormattedString(PastedText, EPropertyValueSetFlags::InstanceObjects) == FPropertyAccess::Success)
	{
		// Reset GUIDs on paste
		StructProperty->EnumerateRawData([](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				FSmartObjectSlotDefinitionDataProxy& DataItem = *static_cast<FSmartObjectSlotDefinitionDataProxy*>(RawData);
				DataItem.ID = FGuid::NewGuid();
			}
			return true;
		});

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();

		if (PropUtils)
		{
			PropUtils->ForceRefresh();
		}
	}
	else
	{
		Transaction.Cancel();
	}
}

void FSmartObjectSlotDefinitionDataProxyDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	check(DataPropertyHandle);
	// Add instance directly as child.
	TSharedRef<FInstancedStructDataDetails> DataDetails = MakeShared<FInstancedStructDataDetails>(DataPropertyHandle);
	StructBuilder.AddCustomBuilder(DataDetails);
}

TSharedRef<SWidget> FSmartObjectSlotDefinitionDataProxyDetails::GenerateStructPicker()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";

	const bool bExcludeBaseStruct = DataPropertyHandle->HasMetaData(NAME_ExcludeBaseStruct);
	const bool bAllowNone = !(DataPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = DataPropertyHandle->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = DataPropertyHandle->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FInstancedStructFilter> StructFilter = MakeShared<FInstancedStructFilter>();
	StructFilter->BaseStruct = TBaseStructure<FSmartObjectSlotDefinitionData>::Get();
	StructFilter->bAllowUserDefinedStructs = false;
	StructFilter->bAllowBaseStruct = !bExcludeBaseStruct;

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;

	FOnStructPicked OnPicked(FOnStructPicked::CreateSP(this, &FSmartObjectSlotDefinitionDataProxyDetails::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, OnPicked)
			]
		];
}

void FSmartObjectSlotDefinitionDataProxyDetails::OnStructPicked(const UScriptStruct* InStruct)
{
	if (DataPropertyHandle && DataPropertyHandle->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		DataPropertyHandle->NotifyPreChange();

		StructProperty->EnumerateRawData([InStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				InstancedStruct->InitializeAs(InStruct);
			}
			return true;
		});

		DataPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		DataPropertyHandle->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
