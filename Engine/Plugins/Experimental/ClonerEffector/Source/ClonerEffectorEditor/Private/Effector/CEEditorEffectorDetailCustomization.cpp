// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/CEEditorEffectorDetailCustomization.h"

#include "CEClonerEffectorShared.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Effector/CEEffectorActor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Styles/CEEditorStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CEEditorEffectorDetailCustomization"

DEFINE_LOG_CATEGORY_STATIC(LogCEEditorEffectorDetailCustomization, Log, All);

void FCEEditorEffectorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	EasingPropertyHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Easing),
		ACEEffectorActor::StaticClass()
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
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			EasingPropertyHandle->CreatePropertyNameWidget()
		];

		CustomWidget.ValueContent()
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			SNew(SComboBox<FName>)
			.ComboBoxStyle(&FCEEditorStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
			.OptionsSource(&EasingNames)
			.InitiallySelectedItem(GetCurrentEasingName())
			.ToolTipText(LOCTEXT("EasingTooltip", "Easings sorted from most dramatic to least and specials at the end"))
			.OnGenerateWidget(this, &FCEEditorEffectorDetailCustomization::OnGenerateEasingEntry)
			.OnSelectionChanged(this, &FCEEditorEffectorDetailCustomization::OnSelectionChanged)
			.ContentPadding(0.f)
			.Content()
			[
				OnGenerateEasingEntry(NAME_None)
			]
		];
	}
}

void FCEEditorEffectorDetailCustomization::RegisterCustomSections() const
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	static const FName ClassName = ACEEffectorActor::StaticClass()->GetFName();

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

void FCEEditorEffectorDetailCustomization::PopulateEasingInfos()
{
	EasingEnum = StaticEnum<ECEClonerEasing>();
	if (EasingEnum.IsValid())
	{
		EasingNames.Empty();

		// Sort from most dramatic to least IN then OUT then IN OUT, then specials
		static const TArray<ECEClonerEasing> SortedEasings
		{
			ECEClonerEasing::InExpo,
			ECEClonerEasing::InCirc,
			ECEClonerEasing::InQuint,
			ECEClonerEasing::InQuart,
			ECEClonerEasing::InQuad,
			ECEClonerEasing::InCubic,
			ECEClonerEasing::InSine,
			ECEClonerEasing::OutExpo,
			ECEClonerEasing::OutCirc,
			ECEClonerEasing::OutQuint,
			ECEClonerEasing::OutQuart,
			ECEClonerEasing::OutQuad,
			ECEClonerEasing::OutCubic,
			ECEClonerEasing::OutSine,
			ECEClonerEasing::InOutExpo,
			ECEClonerEasing::InOutCirc,
			ECEClonerEasing::InOutQuint,
			ECEClonerEasing::InOutQuart,
			ECEClonerEasing::InOutQuad,
			ECEClonerEasing::InOutCubic,
			ECEClonerEasing::InOutSine,
			ECEClonerEasing::Linear,
			ECEClonerEasing::InBounce,
			ECEClonerEasing::InBack,
			ECEClonerEasing::InElastic,
			ECEClonerEasing::OutBounce,
			ECEClonerEasing::OutBack,
			ECEClonerEasing::OutElastic,
			ECEClonerEasing::InOutBounce,
			ECEClonerEasing::InOutBack,
			ECEClonerEasing::InOutElastic,
			ECEClonerEasing::Random
		};

		check(EasingEnum->GetMaxEnumValue() == SortedEasings.Num());

		for (const ECEClonerEasing& Easing : SortedEasings)
		{
			EasingNames.Add(FName(EasingEnum->GetNameStringByValue(static_cast<uint8>(Easing))));
		}
	}
}

TSharedRef<SWidget> FCEEditorEffectorDetailCustomization::OnGenerateEasingEntry(FName InName) const
{
	TSharedPtr<SWidget> ImageWidget;
	TSharedPtr<SWidget> TextWidget;
	const TSharedPtr<SHorizontalBox> HorizontalWidget = SNew(SHorizontalBox).Visibility(EVisibility::Visible);

	static const FVector2D ImageSizeClosed(16.f, 16.f);
	static const FVector2D ImageSizeOpened(32.f, 32.f);

	// If none = update with current value else set fixed value once
	if (InName == NAME_None)
	{
		SAssignNew(ImageWidget, SImage)
		.ColorAndOpacity(FAppStyle::GetSlateColor("SelectionColor"))
		.DesiredSizeOverride(ImageSizeClosed)
		.Image(this, &FCEEditorEffectorDetailCustomization::GetEasingImage, InName);

		SAssignNew(TextWidget, STextBlock)
		.Justification(ETextJustify::Center)
		.Text(this, &FCEEditorEffectorDetailCustomization::GetEasingText, InName);
	}
	else
	{
		// We need to switch color on hover to avoid having the selected color equals the image color
		const TSharedPtr<SWidget> HoverWidget = HorizontalWidget;

		SAssignNew(ImageWidget, SImage)
		.ColorAndOpacity_Static(&FCEEditorEffectorDetailCustomization::GetImageColorAndOpacity, HoverWidget)
		.DesiredSizeOverride(ImageSizeOpened)
		.Image(GetEasingImage(InName));

		SAssignNew(TextWidget, STextBlock)
		.Justification(ETextJustify::Center)
		.Text(GetEasingText(InName));
	}

	// Make inner widgets hit test invisible and only horizontal is hit testable

	HorizontalWidget->AddSlot()
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SScaleBox)
			.Visibility(EVisibility::HitTestInvisible)
			.Stretch(EStretch::UserSpecified)
			.UserSpecifiedScale(1.5)
			[
				ImageWidget.ToSharedRef()
			]
		];

	HorizontalWidget->AddSlot()
		.FillWidth(1.f)
		.Padding(8.f, 2.f)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			[
				TextWidget.ToSharedRef()
			]
		];

	return HorizontalWidget.ToSharedRef();
}

void FCEEditorEffectorDetailCustomization::OnSelectionChanged(FName InSelection, ESelectInfo::Type InSelectInfo) const
{
	if (EasingEnum.IsValid() && EasingPropertyHandle.IsValid())
	{
		const int32 Idx = EasingEnum->GetValueByNameString(InSelection.ToString());
		if (Idx != INDEX_NONE && EasingPropertyHandle->SetValue(static_cast<uint8>(Idx), EPropertyValueSetFlags::DefaultFlags) != FPropertyAccess::Success)
		{
			UE_LOG(LogCEEditorEffectorDetailCustomization, Warning, TEXT("ClonerEffectorDetailCustomization : Cannot set property value %s on selection"), *InSelection.ToString())
		}
	}
}

FName FCEEditorEffectorDetailCustomization::GetCurrentEasingName() const
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

const FSlateBrush* FCEEditorEffectorDetailCustomization::GetEasingImage(FName InName) const
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

	return FCEEditorStyle::Get().GetBrush(FName(TEXT("EasingIcons.") + InName.ToString()));
}

FText FCEEditorEffectorDetailCustomization::GetEasingText(FName InName) const
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

FSlateColor FCEEditorEffectorDetailCustomization::GetImageColorAndOpacity(const TSharedPtr<SWidget> InWidget)
{
	if (InWidget.IsValid())
	{
		return InWidget->IsHovered() ? FLinearColor::White : FAppStyle::GetSlateColor("SelectionColor");
	}

	return FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE
