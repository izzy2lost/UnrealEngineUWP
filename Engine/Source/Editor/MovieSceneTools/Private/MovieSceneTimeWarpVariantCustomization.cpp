// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTimeWarpVariantCustomization.h"
#include "Variants/MovieSceneTimeWarpVariant.h"
#include "Variants/MovieSceneTimeWarpGetter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "ScopedTransaction.h"

#include "UObject/Class.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "MovieSceneTimeWarpVariantCustomization"

namespace UE::MovieScene
{

void FMovieSceneTimeWarpVariantCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = StructPropertyHandle;

	auto DeduceType = [this](const void* RawData, const int32, const int32) -> bool
	{
		const FMovieSceneTimeWarpVariant* Variant = static_cast<const FMovieSceneTimeWarpVariant*>(RawData);

		if (Variant->GetType() == EMovieSceneTimeWarpType::Custom)
		{
			this->bIsFixed = false;

			UMovieSceneTimeWarpGetter* Getter = Variant->AsCustom();
			if (!Getter)
			{
				this->Class.Reset();
				return false;
			}
			else if (Class && Getter->GetClass() != Class.GetValue())
			{
				this->Class.Reset();
				return false;
			}
			else
			{
				this->Class = Getter->GetClass();
			}
		}

		return true;
	};

	StructPropertyHandle->EnumerateConstRawData(DeduceType);

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SComboButton)
		.ForegroundColor(FSlateColor::UseForeground())
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.OnGetMenuContent(this, &FMovieSceneTimeWarpVariantCustomization::BuildTypePickerMenu)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FMovieSceneTimeWarpVariantCustomization::GetTypeComboLabel)
		]
	];
}

void FMovieSceneTimeWarpVariantCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (bIsFixed)
	{
		ChildBuilder.AddCustomRow(FText())
		.ValueContent()
		[
			SNew(SSpinBox<double>)
			.Style(FAppStyle::Get(), "Sequencer.HyperlinkSpinBox")
			.Font(FAppStyle::GetFontStyle("Sequencer.FixedFont"))
			.OnValueCommitted(this, &FMovieSceneTimeWarpVariantCustomization::OnCommitFixedPlayRate)
			.OnValueChanged(this, &FMovieSceneTimeWarpVariantCustomization::SetFixedPlayRate)
			.MinValue(TOptional<double>())
			.MaxValue(TOptional<double>())
			.OnEndSliderMovement(this, &FMovieSceneTimeWarpVariantCustomization::SetFixedPlayRate)
			.Value(this, &FMovieSceneTimeWarpVariantCustomization::GetFixedPlayRate)
		];
	}
	else if (Class.IsSet())
	{
		// All same external type
	}
	else
	{
	}

}

void FMovieSceneTimeWarpVariantCustomization::OnCommitFixedPlayRate(double InValue, ETextCommit::Type Type)
{
	SetFixedPlayRate(InValue);
}

void FMovieSceneTimeWarpVariantCustomization::SetFixedPlayRate(double InValue)
{
	FScopedTransaction Transaction(LOCTEXT("ChangeValue_Transation", "Change Time Warp"));

	PropertyHandle->NotifyPreChange();

	bool bNeedsRefresh = false;
	auto SetFixedPlayRate = [InValue, &bNeedsRefresh](void* RawData, const int32, const int32) -> bool
	{
		FMovieSceneTimeWarpVariant* Variant = static_cast<FMovieSceneTimeWarpVariant*>(RawData);
		// Refresh the children if any of the edited values are not already fixed
		bNeedsRefresh |= Variant->GetType() != EMovieSceneTimeWarpType::FixedPlayRate;
		Variant->Set(InValue);
		return true;
	};
	PropertyHandle->EnumerateRawData(SetFixedPlayRate);

	bIsFixed = true;
	Class.Reset();

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
	if (bNeedsRefresh)
	{
		PropertyHandle->RequestRebuildChildren();
	}
}

double FMovieSceneTimeWarpVariantCustomization::GetFixedPlayRate() const
{
	double Value = 0.0;

	auto GatherFixedPlayRate = [&Value](void* RawData, const int32, const int32) -> bool
	{
		FMovieSceneTimeWarpVariant* Variant = static_cast<FMovieSceneTimeWarpVariant*>(RawData);
		if (Variant->GetType() == EMovieSceneTimeWarpType::FixedPlayRate)
		{
			Value = Variant->AsFixedPlayRate();
		}
		return true;
	};

	PropertyHandle->EnumerateRawData(GatherFixedPlayRate);
	return Value;
}

FText FMovieSceneTimeWarpVariantCustomization::GetTypeComboLabel() const
{
	if (bIsFixed)
	{
		return LOCTEXT("FixedPlayRateLabel", "Fixed Play Rate");
	}
	else if (Class.IsSet())
	{
		return Class.GetValue()->GetDisplayNameText();
	}
	else
	{
		return LOCTEXT("MixedTypesLabel", "<< Mixed Types >>");
	}
}

void FMovieSceneTimeWarpVariantCustomization::ChangeClassType(UClass* InClass)
{
	FScopedTransaction Transaction(LOCTEXT("ChangeType_Transation", "Change Time Warp Type"));

	if (!InClass || !InClass->IsChildOf(UMovieSceneTimeWarpGetter::StaticClass()))
	{
		return;
	}

	PropertyHandle->NotifyPreChange();

	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);

	bool bNeedsRefresh = false;
	auto SetFixedPlayRate = [InClass, &Objects, &bNeedsRefresh](void* RawData, const int32 Index, const int32 Num) -> bool
	{
		FMovieSceneTimeWarpVariant* Variant = static_cast<FMovieSceneTimeWarpVariant*>(RawData);

		if (Num != Objects.Num())
		{
			return false;
		}

		// Refresh the children if any of the edited values are already fixed, or have a different class
		UMovieSceneTimeWarpGetter* Existing = Variant->GetType() == EMovieSceneTimeWarpType::Custom ? Variant->AsCustom() : nullptr;
		if (!Existing || Existing->GetClass() != InClass)
		{
			UObject* Object = Objects[Index];
			check(Object);
			Object->Modify();

			bNeedsRefresh = true;

			UMovieSceneTimeWarpGetter* New = NewObject<UMovieSceneTimeWarpGetter>(Object, InClass, NAME_None, RF_Transactional);
			New->InitializeDefaults();
			Variant->Set(New);
		}
		return true;
	};
	PropertyHandle->EnumerateRawData(SetFixedPlayRate);

	Class = InClass;
	bIsFixed = false;

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
	if (bNeedsRefresh)
	{
		PropertyHandle->RequestRebuildChildren();
	}
}

bool FMovieSceneTimeWarpVariantCustomization::IsFixed() const
{
	return bIsFixed;
}

void FMovieSceneTimeWarpVariantCustomization::SetFixed()
{
	SetFixedPlayRate(1.0);
}

TSharedRef<SWidget> FMovieSceneTimeWarpVariantCustomization::BuildTypePickerMenu()
{
	class FMovieSceneTimeWarpGetterFilter : public IClassViewerFilter
	{
	public:
		bool IsClassAllowed(const FClassViewerInitializationOptions&, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs>) override
		{
			return !InClass->HasAnyClassFlags(CLASS_Abstract) && InClass->IsChildOf(UMovieSceneTimeWarpGetter::StaticClass());
		}
		
		bool IsUnloadedClassAllowed(
			const FClassViewerInitializationOptions&,
			const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
			TSharedRef<FClassViewerFilterFuncs>) override
		{
			return !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract) && InUnloadedClassData->IsChildOf(UMovieSceneTimeWarpGetter::StaticClass());
		}
	};

	bool bShouldCloseWindowAfterMenuSelection = false;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("TimeWarpTypesHeader", "Choose a Time Warp:"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("FixedPlayRate_Label", "Fixed Play Rate"),
			LOCTEXT("FixedPlayRate_Tip",   "Change this time warp to have a fixed (constant) play rate."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FMovieSceneTimeWarpVariantCustomization::SetFixed)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSeparator();

		FClassViewerInitializationOptions ClassViewerOptions;
		ClassViewerOptions.Mode        = EClassViewerMode::ClassPicker;
		ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
		ClassViewerOptions.ClassFilters.Add(MakeShared<FMovieSceneTimeWarpGetterFilter>());

		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
		TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(ClassViewerOptions, FOnClassPicked::CreateSP(this, &FMovieSceneTimeWarpVariantCustomization::ChangeClassType));

		MenuBuilder.AddWidget(ClassViewer, FText(), true, false);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

} // namespace UE::MovieScene

#undef LOCTEXT_NAMESPACE