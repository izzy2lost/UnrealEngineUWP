// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEffectorDetailCustomization.h"

#include "AvaEffectorsEditorStyle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Effector/AvaEffectorActor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "AvaClonerEffectorDetailCustomization"

DEFINE_LOG_CATEGORY_STATIC(LogAvaClonerEffectorDetailCustomization, Log, All);

void FAvaEffectorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	EasingPropertyHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Easing),
		AAvaEffectorActor::StaticClass()
	);

	if (!EasingPropertyHandle.IsValid())
	{
		return;
	}

	if (IDetailPropertyRow* EasingRow = DetailBuilder.EditDefaultProperty(EasingPropertyHandle))
	{
		PopulateEasingInfos();

		FDetailWidgetRow& CustomWidget = EasingRow->CustomWidget();

		CustomWidget.NameContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			EasingPropertyHandle->CreatePropertyNameWidget()
		];

		CustomWidget.ValueContent()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			SNew(SComboBox<FName>)
			.OptionsSource(&EasingNames)
			.InitiallySelectedItem(GetCurrentEasingName())
			.ToolTipText(LOCTEXT("EasingTooltip", "Easings sorted from most dramatic to least and specials at the end"))
			.OnGenerateWidget(this, &FAvaEffectorDetailCustomization::OnGenerateEasingEntry)
			.OnSelectionChanged(this, &FAvaEffectorDetailCustomization::OnSelectionChanged)
			.Content()
			[
				OnGenerateEasingEntry(NAME_None)
			]
		];
	}
}

void FAvaEffectorDetailCustomization::RegisterCustomSections() const
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	static const FName ClassName = AAvaEffectorActor::StaticClass()->GetFName();

	const TSharedRef<FPropertySection> StreamingSection = PropertyModule.FindOrCreateSection(ClassName, "Streaming", LOCTEXT("Effector.Streaming", "Streaming"));
	StreamingSection->RemoveCategory("World Partition");
	StreamingSection->RemoveCategory("Data Layers");
	StreamingSection->RemoveCategory("HLOD");

	const TSharedRef<FPropertySection> EffectorSection = PropertyModule.FindOrCreateSection(ClassName, "General", LOCTEXT("Effector.General", "General"));
	EffectorSection->AddCategory(TEXT("Effector"));

	const TSharedRef<FPropertySection> TypeSection = PropertyModule.FindOrCreateSection(ClassName, "Type", LOCTEXT("Effector.Type", "Type"));
	TypeSection->AddCategory(TEXT("Type"));

	const TSharedRef<FPropertySection> ModeSection = PropertyModule.FindOrCreateSection(ClassName, "Mode", LOCTEXT("Effector.Mode", "Mode"));
	ModeSection->AddCategory(TEXT("Mode"));

	const TSharedRef<FPropertySection> ForceSection = PropertyModule.FindOrCreateSection(ClassName, "Forces", LOCTEXT("Effector.Forces", "Forces"));
	ForceSection->AddCategory(TEXT("Force"));
}

void FAvaEffectorDetailCustomization::PopulateEasingInfos()
{
	EasingEnum = StaticEnum<EAvaClonerEasing>();
	if (EasingEnum.IsValid())
	{
		EasingNames.Empty();

		// Sort from most dramatic to least IN then OUT then IN OUT, then specials
		static const TArray<EAvaClonerEasing> SortedEasings
		{
			EAvaClonerEasing::InExpo,
			EAvaClonerEasing::InCirc,
			EAvaClonerEasing::InQuint,
			EAvaClonerEasing::InQuart,
			EAvaClonerEasing::InQuad,
			EAvaClonerEasing::InCubic,
			EAvaClonerEasing::InSine,
			EAvaClonerEasing::OutExpo,
			EAvaClonerEasing::OutCirc,
			EAvaClonerEasing::OutQuint,
			EAvaClonerEasing::OutQuart,
			EAvaClonerEasing::OutQuad,
			EAvaClonerEasing::OutCubic,
			EAvaClonerEasing::OutSine,
			EAvaClonerEasing::InOutExpo,
			EAvaClonerEasing::InOutCirc,
			EAvaClonerEasing::InOutQuint,
			EAvaClonerEasing::InOutQuart,
			EAvaClonerEasing::InOutQuad,
			EAvaClonerEasing::InOutCubic,
			EAvaClonerEasing::InOutSine,
			EAvaClonerEasing::Linear,
			EAvaClonerEasing::InBounce,
			EAvaClonerEasing::InBack,
			EAvaClonerEasing::InElastic,
			EAvaClonerEasing::OutBounce,
			EAvaClonerEasing::OutBack,
			EAvaClonerEasing::OutElastic,
			EAvaClonerEasing::InOutBounce,
			EAvaClonerEasing::InOutBack,
			EAvaClonerEasing::InOutElastic,
			EAvaClonerEasing::Random
		};

		check(EasingEnum->GetMaxEnumValue() == SortedEasings.Num());

		for (const EAvaClonerEasing& Easing : SortedEasings)
		{
			EasingNames.Add(FName(EasingEnum->GetNameStringByValue(static_cast<uint8>(Easing))));
		}
	}
}

TSharedRef<SWidget> FAvaEffectorDetailCustomization::OnGenerateEasingEntry(FName InName) const
{
	TSharedPtr<SWidget> ImageWidget;
	TSharedPtr<SWidget> TextWidget;

	static const FVector2D ImageSizeClosed(16.f, 16.f);
	static const FVector2D ImageSizeOpened(32.f, 32.f);

	// If none = update with current value else set fixed value once
	if (InName == NAME_None)
	{
		SAssignNew(ImageWidget, SImage)
		.DesiredSizeOverride(ImageSizeClosed)
		.Image(this, &FAvaEffectorDetailCustomization::GetEasingImage, InName);

		SAssignNew(TextWidget, STextBlock)
		.Text(this, &FAvaEffectorDetailCustomization::GetEasingText, InName);
	}
	else
	{
		SAssignNew(ImageWidget, SImage)
		.DesiredSizeOverride(ImageSizeOpened)
		.Image(GetEasingImage(InName));

		SAssignNew(TextWidget, STextBlock)
		.Text(GetEasingText(InName));
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f)
		[
			ImageWidget.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(5.f, 2.f)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			TextWidget.ToSharedRef()
		];
}

void FAvaEffectorDetailCustomization::OnSelectionChanged(FName InSelection, ESelectInfo::Type InSelectInfo) const
{
	if (EasingEnum.IsValid() && EasingPropertyHandle.IsValid())
	{
		const int32 Idx = EasingEnum->GetValueByNameString(InSelection.ToString());
		if (Idx != INDEX_NONE && EasingPropertyHandle->SetValue(static_cast<uint8>(Idx), EPropertyValueSetFlags::DefaultFlags) != FPropertyAccess::Success)
		{
			UE_LOG(LogAvaClonerEffectorDetailCustomization, Warning, TEXT("ClonerEffectorDetailCustomization : Cannot set property value %s on selection"), *InSelection.ToString())
		}
	}
}

FName FAvaEffectorDetailCustomization::GetCurrentEasingName() const
{
	uint8 CurrentValue;
	if (EasingPropertyHandle.IsValid() && EasingPropertyHandle->GetValue(CurrentValue) == FPropertyAccess::Result::Success)
	{
		if (EasingEnum.IsValid())
		{
			return FName(EasingEnum->GetNameStringByValue(CurrentValue));
		}
	}
	return NAME_None;
}

const FSlateBrush* FAvaEffectorDetailCustomization::GetEasingImage(FName InName) const
{
	if (InName == NAME_None)
	{
		InName = GetCurrentEasingName();

		// multiple values
		if (InName == NAME_None)
		{
			return nullptr;
		}
	}

	return FAvaEffectorsEditorStyle::Get().GetBrush(FName(TEXT("AvalancheIcons.Easing.") + InName.ToString()));
}

FText FAvaEffectorDetailCustomization::GetEasingText(FName InName) const
{
	if (InName == NAME_None)
	{
		InName = GetCurrentEasingName();

		// multiple values
		if (InName == NAME_None)
		{
			return LOCTEXT("MultipleValue", "Multiple values selected");
		}
	}

	if (EasingEnum.IsValid())
	{
		const int32 EnumValue = EasingEnum->GetValueByNameString(InName.ToString());
		return EasingEnum->GetDisplayNameTextByValue(EnumValue);
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
