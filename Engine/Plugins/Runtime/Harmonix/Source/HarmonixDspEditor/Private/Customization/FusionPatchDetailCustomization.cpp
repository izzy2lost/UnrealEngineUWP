// Copyright Epic Games, Inc. All Rights Reserved.
#include "FusionPatchDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "HarmonixDsp/FusionSampler/Settings/FusionPatchSettings.h"
#include "HarmonixDsp/FusionSampler/Settings/KeyzoneSettings.h"
#include "HarmonixDsp/FusionSampler/FusionPatch.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Widgets/SMinMaxSlider.h"

void FFusionPatchDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& FusionPatchDataCategory = DetailLayout.EditCategory(TEXT("Fusion Patch Data"));
	//IDetailCategoryBuilder& FusionPatchDataCategory = DetailLayout.EditCategory(TEXT("Fusion Patch Data"));
	IDetailCategoryBuilder& FilePathCategory = DetailLayout.EditCategory(TEXT("File Path"));

	//get a handle of Fusion Patch Data
	TSharedPtr<IPropertyHandle> FusionPatchDataHandle = DetailLayout.GetProperty("FusionPatchData");
	check(FusionPatchDataHandle);
	TSharedPtr<IPropertyHandle> PresetsHandle = FusionPatchDataHandle->GetChildHandle("Presets");
	check(PresetsHandle);
	TSharedPtr<IPropertyHandle> CurrentPresetIndexHandle = FusionPatchDataHandle->GetChildHandle("CurrentPresetIndex");
	check(CurrentPresetIndexHandle);

	//get a handle of the keyzones as array
	TSharedPtr<IPropertyHandleArray> KeyzonesHandle = FusionPatchDataHandle->GetChildHandle("Keyzones")->AsArray();
	check(KeyzonesHandle.IsValid());

	//get the number of keyzones in the current patch
	uint32 NumKeyzones;
	KeyzonesHandle->GetNumElements(NumKeyzones);

	//add all keyzones' name (sample path string) to an array for displaying in the dropdown menu
	AddKeyzonesNameToMenuArray(KeyzonesHandle, static_cast<int32>(NumKeyzones));
	if (!KeyzonesNameMenu.IsEmpty()) CurrentKeyzoneName = KeyzonesNameMenu[0];

	//get the current fusion patch being edited
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	TWeakObjectPtr<UObject> FusionPatch = Objects.Last();
	TWeakObjectPtr<UFusionPatch> FusionPatchBeingEdited = Cast<UFusionPatch>(FusionPatch);
	
	//add the dropdown menu displaying the sample path/name of keyzones to the name content of this row
	FusionPatchDataCategory.AddCustomRow(FText::FromString("Fusion Patch"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Selected Keyzone"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
		.ValueContent()
		.MinDesiredWidth(350.f)
		[
			//create the dropdown combo box displaying all the keyzones name/sample path for selection
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&KeyzonesNameMenu)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewKeyzoneName, ESelectInfo::Type)
				{
					CurrentKeyzoneName = NewKeyzoneName;
					CurrentKeyzoneIndex = KeyzonesNameMenu.Find(NewKeyzoneName);
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InKeyzoneName) -> TSharedRef<SWidget>
				{
					return SNew(STextBlock).Text(FText::FromString(*InKeyzoneName));
				})
				.InitiallySelectedItem(CurrentKeyzoneName)
				.Content()
				[
					SNew(STextBlock).Text_Lambda([this]() -> FText
					{
						if (CurrentKeyzoneName.IsValid())
						{
							return FText::FromString(*CurrentKeyzoneName);
						}

						return FText::FromString(TEXT("InvalidComboEntryText"));
					})
				]
		];

	DrawSelectedKeyzoneProperties(FusionPatchDataCategory, KeyzonesHandle, NumKeyzones);

	//add the presets properties since they're not customized 
	FusionPatchDataCategory.AddProperty(PresetsHandle);
	FusionPatchDataCategory.AddProperty(CurrentPresetIndexHandle);

	//hide the non-customized versions of these properties
	DetailLayout.HideProperty(FusionPatchDataHandle);
}



TSharedRef<SWidget> FFusionPatchDetailCustomization::CreateMinMaxSliderWidget(TSharedPtr<IPropertyHandle> MinValuePropertyHandle, TSharedPtr<IPropertyHandle> MaxValuePropertyHandle)
{
	int8 CurrentMinValue = 0.0f;
	MinValuePropertyHandle->GetValue(CurrentMinValue);
	const int32 ClampMinValue = MinValuePropertyHandle->GetIntMetaData("ClampMin");
	int8 CurrentMaxValue = 0.0f;
	MaxValuePropertyHandle->GetValue(CurrentMaxValue);
	const int32 ClampMaxValue = MinValuePropertyHandle->GetIntMetaData("ClampMax");
	TSharedPtr<SMinMaxSlider> CustomRangeSlider;
	TSharedRef<SWidget> OutWidget = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.Padding(0, 5, 0, 5)
		.AutoWidth()
		[
			MinValuePropertyHandle->CreatePropertyValueWidget()
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SAssignNew(CustomRangeSlider, SMinMaxSlider)
			.MinValue(ClampMinValue)
			.MaxValue(ClampMaxValue)
			.LowerHandleValue(static_cast<float>(CurrentMinValue))
			.UpperHandleValue(static_cast<float>(CurrentMaxValue))
			.OnLowerHandleValueChanged_Lambda([this, MinValuePropertyHandle, MaxValuePropertyHandle](float NewValue)
			{
				int8 MaxValue;
				MaxValuePropertyHandle->GetValue(MaxValue);
				int8 NewMinValue = FMath::RoundToInt(NewValue);
				MinValuePropertyHandle->SetValue(NewMinValue <= MaxValue ? NewMinValue : MaxValue);
			})
			.OnUpperHandleValueChanged_Lambda([this, MinValuePropertyHandle, MaxValuePropertyHandle](float NewValue)
			{
				int8 MinValue;
				MinValuePropertyHandle->GetValue(MinValue);
				int8 NewMaxValue = FMath::RoundToInt(NewValue);
				MaxValuePropertyHandle->SetValue(NewMaxValue >= MinValue ? NewMaxValue : MinValue);
			})
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0, 5, 0, 5)
		[
			MaxValuePropertyHandle->CreatePropertyValueWidget()
		];

	//if the min or max note numeric boxes are edited, modify the slider values accordingly
	MinValuePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, CustomRangeSlider, MinValuePropertyHandle, MaxValuePropertyHandle]()
	{
		int8 MinValue;
		MinValuePropertyHandle->GetValue(MinValue);
		int8 MaxValue;
		MaxValuePropertyHandle->GetValue(MaxValue);
		CustomRangeSlider->SetLowerValue(static_cast<float>(MinValue <= MaxValue ? MinValue : MaxValue));
		CustomRangeSlider->SetUpperValue(static_cast<float>(MaxValue >= MinValue ? MaxValue : MinValue));
	}));

	MaxValuePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this, CustomRangeSlider, MinValuePropertyHandle, MaxValuePropertyHandle]()
	{
		int8 MinValue;
		MinValuePropertyHandle->GetValue(MinValue);
		int8 MaxValue;
		MaxValuePropertyHandle->GetValue(MaxValue);
		CustomRangeSlider->SetUpperValue(static_cast<float>(MaxValue >= MinValue ? MaxValue : MinValue));
		CustomRangeSlider->SetLowerValue(static_cast<float>(MinValue <= MaxValue ? MinValue : MaxValue));
	}));
	
	return OutWidget;
}

FDetailWidgetRow& FFusionPatchDetailCustomization::AddCustomMinMaxSliderRow(IDetailCategoryBuilder& FusionPatchDataCategory, const FText& DisplayName, TSharedPtr<IPropertyHandle> MinPropertyHandle, TSharedPtr<IPropertyHandle> MaxPropertyHandle)
{
	return FusionPatchDataCategory.AddCustomRow(FText::FromString("Fusion Patch"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(DisplayName)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(350.f)
		[
			CreateMinMaxSliderWidget(MinPropertyHandle, MaxPropertyHandle)
		];
}

void FFusionPatchDetailCustomization::AddKeyzonesNameToMenuArray(TSharedPtr<IPropertyHandleArray> KeyzonesHandle, int32 NumKeyzones)
{
	for (int32 KeyzoneIndex = 0; KeyzoneIndex < static_cast<int32>(NumKeyzones); ++KeyzoneIndex)
	{
		TSharedPtr<IPropertyHandle> SamplePathHandle = KeyzonesHandle->GetElement(KeyzoneIndex)->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, SamplePath));
		FString CurrentSamplePath;
		if (SamplePathHandle->GetValue(CurrentSamplePath) == FPropertyAccess::Success)
		{
			FString Path;
			FString Filename;
			FString Ext;
			FPaths::Split(CurrentSamplePath, Path, Filename, Ext);
			if (Filename.IsEmpty())
			{
				KeyzonesNameMenu.Add(MakeShareable(new FString(CurrentSamplePath)));
			}
			else
			{
				KeyzonesNameMenu.Add(MakeShareable(new FString(Filename)));
			}
		}
	}
}

void FFusionPatchDetailCustomization::DrawSelectedKeyzoneProperties(IDetailCategoryBuilder& FusionPatchDataCategory, TSharedPtr<IPropertyHandleArray> KeyzonesHandle, int32 NumKeyzones)
{
	for (int32 KeyzoneIndex = 0; KeyzoneIndex < static_cast<int32>(NumKeyzones); ++KeyzoneIndex)
	{
		TFunction<EVisibility()> CustomVisibility = [this, KeyzoneIndex]() -> EVisibility
		{
			if (KeyzoneIndex != CurrentKeyzoneIndex)
			{
				return EVisibility::Collapsed;
			}
			else
			{
				return EVisibility::Visible;
			}
		};
		
		TSharedRef<IPropertyHandle> KeyzoneHandle = KeyzonesHandle->GetElement(KeyzoneIndex);
		uint32 NumChildren = 0;
		KeyzoneHandle->GetNumChildren(NumChildren);
		for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
		{
			TSharedPtr<IPropertyHandle> PropertyHandle = KeyzoneHandle->GetChildHandle(ChildIdx);
			check(PropertyHandle);

			if (PropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinNote))
			{
				TSharedPtr<IPropertyHandle> MaxPropertyHandle = KeyzoneHandle->GetChildHandle(ChildIdx + 1);
				check(MaxPropertyHandle);
				FDetailWidgetRow& MinMaxRow = AddCustomMinMaxSliderRow(FusionPatchDataCategory, NSLOCTEXT("FusionPatch_Details", "NoteRange", "Note Range (Min/Max)"), PropertyHandle, MaxPropertyHandle);
				MinMaxRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(CustomVisibility)));
				// increment ChildIdx since we customized two properties
				++ChildIdx;
			}
			else if (PropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinVelocity))
			{
				TSharedPtr<IPropertyHandle> MaxPropertyHandle = KeyzoneHandle->GetChildHandle(ChildIdx + 1);
				check(MaxPropertyHandle);
				FDetailWidgetRow& MinMaxRow = AddCustomMinMaxSliderRow(FusionPatchDataCategory, NSLOCTEXT("FusionPatch_Details", "VelocityRange", "Velocity Range (Min/Max)"), PropertyHandle, MaxPropertyHandle);
				MinMaxRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(CustomVisibility)));
				// increment ChildIdx since we customized two properties
				++ChildIdx;
			}
			else // default property
			{
				IDetailPropertyRow& PropertyRow = FusionPatchDataCategory.AddProperty(PropertyHandle);
				PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(CustomVisibility)));
			}

		}
	}
}