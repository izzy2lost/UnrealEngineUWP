// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/MovieGraphNamedResolution.h"
#include "Graph/MovieGraphProjectSettings.h"

#include "MovieRenderPipelineCoreModule.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "FMovieGraphNamedResolutionCustomization"

/* Customizes how named resolutions are displayed in the details pane. */
class MOVIERENDERPIPELINEEDITOR_API FMovieGraphNamedResolutionCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FMovieGraphNamedResolutionCustomization>();
	}

	virtual ~FMovieGraphNamedResolutionCustomization() override
	{
		UMovieGraphProjectSettings* MovieGraphProjectSettings = GetMutableDefault<UMovieGraphProjectSettings>();
		if (ensureAlwaysMsgf(MovieGraphProjectSettings, TEXT("%hs: Failed to find UMovieGraphProjectSettings!"), __FUNCTION__))
		{
			MovieGraphProjectSettings->OnSettingChanged().Remove(OnProjectSettingsModifiedHandle);
		}
	}

protected:

	bool IsCurrentOptionValid() const
	{
		return ComboBoxOptions.Contains(CurrentOption);
	}
	
	/**
	 * Checks to see if CurrentOption is None, and if so calls RepopulateOptions() which sets CurrentOption to the first option.
	 * Otherwise just returns CurrentOption.
	 * @return The current option selected in the combobox
	 */
	const FName& GetCurrentOptionAssigningIfNeeded()
	{
		if (!IsCurrentOptionValid())
		{
			RepopulateOptions();
		}
		return CurrentOption;
	}

	void SetCurrentOptionAndCacheTooltipText(const FName& InOption)
	{
		if (ComboBox)
		{
			ComboBox->SetSelectedItem(InOption);
		}
		CurrentOption = InOption;
		CachedSelectedOptionTooltipText = GetOptionTooltipText(CurrentOption);
	}

	/**
	 * Adds all ProfileNames from DefaultNamedResolutions to the combobox,
	 * and sets CurrentOption to the first option if it's None or isn't a known option.
	 */
	void RepopulateOptions()
	{
		ComboBoxOptions.Reset(ComboBoxOptions.Num());

		const UMovieGraphProjectSettings* MovieGraphProjectSettings = GetDefault<UMovieGraphProjectSettings>();
		if (ensureAlwaysMsgf(MovieGraphProjectSettings, TEXT("%hs: Failed to find UMovieGraphProjectSettings!"), __FUNCTION__))
		{
			for (const FMovieGraphNamedResolution& Resolution : MovieGraphProjectSettings->DefaultNamedResolutions)
			{
				ComboBoxOptions.AddUnique(Resolution.ProfileName);
			}
		}

		ComboBoxOptions.AddUnique(FMovieGraphNamedResolution::CustomEntryName);

		if (!IsCurrentOptionValid())
		{
			SetCurrentOptionAndCacheTooltipText(FMovieGraphNamedResolution::CustomEntryName);
		}
	}

	/**
	 * Returns the tooltip text for the given option by looking up its Description
	 * in DefaultNamedResolutions from Project Settings.
	 * If a match isn't found, the tooltip text will be the name of the option.
	 * @param InOption The name of the option.
	 * @return The tooltip text for the option.
	 */
	FText GetOptionTooltipText(FName InOption) const
	{
		if (InOption.IsEqual(CurrentOption))
		{
			return CachedSelectedOptionTooltipText;
		}
		if (const FMovieGraphNamedResolution* Match = FindNamedResolutionForOption(InOption))
		{
			return FText::FromString(Match->Description);
		}
		return FText::FromName(InOption);
	}

	/**
	 * Tries to find a match for the given option in DefaultNamedResolutions from Project Settings.
	 * @param InOption The name of the option.
	 * @return A raw pointer to an FMovieGraphNamedResolution if a match is found, nullptr otherwise.
	 */
	const FMovieGraphNamedResolution* FindNamedResolutionForOption(const FName& InOption) const
	{
		// Return the custom entry
		if (InOption.IsEqual(FMovieGraphNamedResolution::CustomEntryName))
		{
			return &CustomEntry;
		}

		// Find a matching custom entry from Project Settings
		const UMovieGraphProjectSettings* MovieGraphProjectSettings = GetDefault<UMovieGraphProjectSettings>();
		if (ensureAlwaysMsgf(MovieGraphProjectSettings, TEXT("%hs: Failed to find UMovieGraphProjectSettings!"), __FUNCTION__))
		{
			return MovieGraphProjectSettings->FindNamedResolutionForOption(InOption);
		}

		UE_LOG(LogMovieRenderPipeline, Error, TEXT("%hs: No match found for FMovieGraphNamedResolution %s!"), __FUNCTION__, *InOption.ToString());
		return nullptr;
	}

	FText GetWidgetTextForOption(const FName& InOption) const
	{
		const FMovieGraphNamedResolution* NamedResolution = FindNamedResolutionForOption(InOption);
		if (!ensureAlwaysMsgf(NamedResolution, TEXT("%hs: Failed to find FMovieGraphNamedResolution for option %s!"), __FUNCTION__, *InOption.ToString()))
		{
			return LOCTEXT("UnknownNamedResolutionComboboxItemWidgetText", "Unknown Named Resolution");
		}


		FNumberFormattingOptions FormattingOptions;
		FormattingOptions.UseGrouping = false;
		
		return FText::Format(LOCTEXT("NamedResolutionComboboxItemWidgetText", "{0} - {1}x{2}"),
				FText::FromName(InOption),
				FText::AsNumber(NamedResolution->Resolution.X, &FormattingOptions),
				FText::AsNumber(NamedResolution->Resolution.Y, &FormattingOptions));
	}

	FSlateFontInfo GetWidgetFontForOptions() const
	{
		return IDetailLayoutBuilder::GetDetailFont();
	}

	/**
	 * Generates a widget for the given option with the option name as STextBlock with tooltip text .
	 * @param InOption The name of the option.
	 * @return A shared reference to the generated widget.
	 */
	TSharedRef<SWidget> MakeWidgetForOption(FName InOption) const
	{
		return
			SNew(STextBlock)
			.Text(GetWidgetTextForOption(InOption))
			.ToolTipText(this, &FMovieGraphNamedResolutionCustomization::GetOptionTooltipText, InOption)
			.Font(this, &FMovieGraphNamedResolutionCustomization::GetWidgetFontForOptions);
		;
	}

	TSharedRef<SWidget> CreateOptionComboBox()
	{
		ComboBox = 
			SNew(SComboBox<FName>)
			.OnComboBoxOpening(this, &FMovieGraphNamedResolutionCustomization::RepopulateOptions)
			.OptionsSource(&ComboBoxOptions)
			.OnSelectionChanged(this, &FMovieGraphNamedResolutionCustomization::OnComboBoxSelectionChanged)
			.OnGenerateWidget(this, &FMovieGraphNamedResolutionCustomization::MakeWidgetForOption)
			.InitiallySelectedItem(GetCurrentOptionAssigningIfNeeded())
			[
				// We specifically don't use MakeWidgetForOption here for performance reasons
				// as it will need to be called each frame to stay current (especially for 'Custom')
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return GetWidgetTextForOption(GetCurrentOptionAssigningIfNeeded());
				})
				.ToolTipText_Lambda([this]()
				{
					return CachedSelectedOptionTooltipText;
				})
				.Font(this, &FMovieGraphNamedResolutionCustomization::GetWidgetFontForOptions)
			];
		return ComboBox.ToSharedRef(); 
	}

	void BindToOnProjectSettingsModified()
	{
		UMovieGraphProjectSettings* MovieGraphProjectSettings = GetMutableDefault<UMovieGraphProjectSettings>();
		if (ensureAlwaysMsgf(MovieGraphProjectSettings, TEXT("%hs: Unable to get UMovieGraphProjectSettings."), __FUNCTION__))
		{
			OnProjectSettingsModifiedHandle =
				MovieGraphProjectSettings->OnSettingChanged().AddSP(
					this, &FMovieGraphNamedResolutionCustomization::OnProjectSettingsChanged);
		}
	}

	const FSlateBrush* GetLockBrush() const
	{
		return bLockedAspectRatio ? FAppStyle::GetBrush(TEXT("Icons.Link")) : FAppStyle::GetBrush(TEXT("Icons.Unlink"));
	}

	FText GetLockTooltipText() const
	{
		return bLockedAspectRatio ?
			FText::Format(LOCTEXT("LockedAspectRatioTooltipFormat", "Aspect ratio is locked ({0})"), FText::AsNumber(CurrentAspectRatio)) :
			LOCTEXT("UnlockedAspectRatioTooltip", "Aspect Ratio is unlocked");
	}
	
	//~ Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		StructPropertyHandle = InStructPropertyHandle;
		CreateOptionComboBox();
		
		const TSharedRef<IPropertyHandle> NameProp = InStructPropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FMovieGraphNamedResolution, ProfileName)).ToSharedRef();
		
		// Try to 'load' selected option from the struct handle
		FName LoadedOption = NAME_None;
		if (NameProp->GetValue(LoadedOption) == FPropertyAccess::Result::Success && !LoadedOption.IsNone())
		{
			SetCurrentOptionAndCacheTooltipText(LoadedOption);
		}
		
		// Set up option list for first time. If LoadedOption doesn't exist, use the first option in the list.
		RepopulateOptions();

		// Repopulate options when the project settings change
		BindToOnProjectSettingsModified();

		// Prevent reset to default from being shown
		HeaderRow.OverrideResetToDefault(FResetToDefaultOverride::Hide());
			
		HeaderRow
		.NameContent()
		[
			// Show "Resolution" as the row name
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.Padding(0, 2, 8, 2)
			.HAlign(HAlign_Fill)
			[
				ComboBox.ToSharedRef()
			]
		];
	}

	void UpdateAspectRatioLockImage()
	{
		LockImage->SetImage(GetLockBrush());
		LockImage->SetToolTipText(GetLockTooltipText());
	}

	void ToggleAspectRatioLock()
	{
		bLockedAspectRatio = !bLockedAspectRatio;
		if (bLockedAspectRatio) { CacheAspectRatio(); }

		UpdateAspectRatioLockImage();
	}

	TSharedRef<SWidget> MakeLockExtensionWidget()
	{
		return SNew(SButton)
		.OnClicked_Lambda([this]()
		{
			ToggleAspectRatioLock();
			
			return FReply::Handled();
		})
		.ContentPadding(FMargin(0, 0, 4, 0))
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		[
			SAssignNew(LockImage, SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
	}

	void CacheAspectRatio()
	{
		float X = (float)CustomEntry.Resolution.X;
		float Y = (float)CustomEntry.Resolution.Y;
		
		if (const FMovieGraphNamedResolution* SelectedResolution = FindNamedResolutionForOption(GetCurrentOptionAssigningIfNeeded()))
		{
			X = (float)SelectedResolution->Resolution.X;
			Y = (float)SelectedResolution->Resolution.Y;
		}
		CurrentAspectRatio = X / Y;
	}

	void UpdateCustomEntryValues()
	{
		if (!ensureMsgf(ResolutionXPropertyHandle, TEXT("%hs: `ResolutionXPropertyHandle` is null."), __FUNCTION__))
		{
			return;
		}
		if (!ensureMsgf(ResolutionYPropertyHandle, TEXT("%hs: `ResolutionYPropertyHandle` is null."), __FUNCTION__))
		{
			return;
		}
			
		// Set the custom resolution to the preset resolution
		int32 ResolutionX = 0;
		ResolutionXPropertyHandle->GetValue(ResolutionX);

		CustomEntry.Resolution.X = ResolutionX;

		int32 ResolutionY = 0;
		ResolutionYPropertyHandle->GetValue(ResolutionY);

		CustomEntry.Resolution.Y = ResolutionY;

		// Cache the current aspect ratio
		CacheAspectRatio();
	}

	int32 RoundToNearestEvenNumber(double InNumberToRound) const
	{
		// Round to nearest even
		const int32 Floored = FMath::FloorToInt(InNumberToRound);
		const int32 Ceiled = FMath::CeilToInt(InNumberToRound);

		return Floored % 2 == 0 ? Floored : Ceiled;
	}
	
	int32 GetCurrentlySelectedResolutionByAxis(const EAxis::Type Axis)
	{
		if (const FMovieGraphNamedResolution* const CurrentNamedResolution = FindNamedResolutionForOption(GetCurrentOptionAssigningIfNeeded()))
		{
			return Axis == EAxis::Y ? CurrentNamedResolution->Resolution.Y : CurrentNamedResolution->Resolution.X;
		}

		return INDEX_NONE;
	}

	void OnCustomSliderValueChanged(uint32 NewValue, EAxis::Type Axis)
	{
		// If the custom resolution is changed, switch to 'Custom' option
		if (!CurrentOption.IsEqual(FMovieGraphNamedResolution::CustomEntryName))
		{
			UpdateCustomEntryValues();

			// Switch to 'Custom' option
			SetCurrentOptionAndCacheTooltipText(FMovieGraphNamedResolution::CustomEntryName);
		}

		// Enforce aspect ratio if desired
		if (Axis == EAxis::Y)
		{
			CustomEntry.Resolution.Y = NewValue;
			
			if (bLockedAspectRatio)
			{
				const double NewXResolution = (double)CustomEntry.Resolution.Y * CurrentAspectRatio;
				CustomEntry.Resolution.X = RoundToNearestEvenNumber(NewXResolution);
			}
		}
		else // X Changed
		{
			CustomEntry.Resolution.X = NewValue;
			
			if (bLockedAspectRatio)
			{
				const double NewYResolution = (double)CustomEntry.Resolution.X / CurrentAspectRatio;
				CustomEntry.Resolution.Y = RoundToNearestEvenNumber(NewYResolution);
			}
		}
	}

	void OnCustomSliderValueCommitted(uint32 NewValue, ETextCommit::Type, EAxis::Type Axis)
	{
		OnCustomSliderValueChanged(NewValue, Axis);
		
		UpdateOwningStruct();
	}

	void AddCustomRowForResolutionAxis(
		EAxis::Type Axis, IDetailChildrenBuilder& StructBuilder, const FString& FilterString,
		TSharedRef<SWidget> NameContentWidget, TSharedPtr<SWidget> AspectRatioLockExtensionWidget = nullptr)
	{
		const FSlateFontInfo PropertyFontStyle = FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") );
		const TOptional<uint32> PropertyMaxValue = TOptional<uint32>();
	
		FDetailWidgetRow& CustomRow = StructBuilder.AddCustomRow(FText::FromString(FilterString))
		.NameContent()
		[
			NameContentWidget
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			// We make our own value widget because we don't want the widget tied to the property handle
			// as the property handle gets re-instanced every time a blueprint is edited.
			SNew(SBox)
			// Maintain spacing when there would be no extension widget
			.Padding(0, 0, Axis == EAxis::Y ? 24 /* Assuming image is 16x16 + 8 padding */ : 0, 0) 
			[
				SNew(SNumericEntryBox<uint32>)
				.Font(PropertyFontStyle)
				.AllowSpin(true)
				.MinSliderValue(2)
				.MinValue(2)
				.MaxValue(PropertyMaxValue)
				.MaxSliderValue(PropertyMaxValue)
				.Delta(2)
				.Value_Lambda([this, Axis] (){ return GetCurrentlySelectedResolutionByAxis(Axis); }) // Lambda to bypass const requirement
				.OnValueChanged(this, &FMovieGraphNamedResolutionCustomization::OnCustomSliderValueChanged, Axis)
				.OnValueCommitted(this, &FMovieGraphNamedResolutionCustomization::OnCustomSliderValueCommitted, Axis)
			]
		]
		.OverrideResetToDefault(FResetToDefaultOverride::Hide()); // Prevent showing Reset to Default

		if (AspectRatioLockExtensionWidget.IsValid())
		{
			CustomRow
			.ExtensionContent()
			[
				AspectRatioLockExtensionWidget.ToSharedRef()
			];
		}
	}

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		const TSharedPtr<IPropertyHandle> ResolutionPropertyHandle =
			InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieGraphNamedResolution, Resolution));
		
		if (const TSharedPtr<IPropertyHandle> PropertyXHandle = ResolutionPropertyHandle->GetChildHandle(0))
		{
			ResolutionXPropertyHandle = PropertyXHandle;

			AddCustomRowForResolutionAxis(
				EAxis::X, StructBuilder, PropertyXHandle->GeneratePathToProperty(), PropertyXHandle->CreatePropertyNameWidget(), MakeLockExtensionWidget());
			
			UpdateAspectRatioLockImage();
		}

		if (const TSharedPtr<IPropertyHandle> PropertyYHandle = ResolutionPropertyHandle->GetChildHandle(1))
		{			
			ResolutionYPropertyHandle = PropertyYHandle;
			
			AddCustomRowForResolutionAxis(
				EAxis::Y, StructBuilder, PropertyYHandle->GeneratePathToProperty(), PropertyYHandle->CreatePropertyNameWidget());
		}

		CacheAspectRatio();
	}
	//~ End IPropertyTypeCustomization interface
	
	void OnProjectSettingsChanged(UObject*, FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (PropertyChangedEvent.MemberProperty->GetFName().IsEqual(GET_MEMBER_NAME_CHECKED(UMovieGraphProjectSettings, DefaultNamedResolutions)))
		{
			RepopulateOptions();
		}
	}

	/**
	 * Updates the struct which owns this customization with the data from the matched named resolution.
	 */
	void UpdateOwningStruct()
	{
		const TSharedPtr<IPropertyHandle> PinnedStructPropertyHandle = StructPropertyHandle.Pin();
		if (!PinnedStructPropertyHandle)
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("%hs: Unable to pin StructPropertyHandle."), __FUNCTION__);
			return;
		}

		const FMovieGraphNamedResolution* NamedResolution = FindNamedResolutionForOption(CurrentOption);
		if (!NamedResolution)
		{
			return;
		}
		
		void* StructData = nullptr;
		if (PinnedStructPropertyHandle->GetValueData(StructData) == FPropertyAccess::Success && StructData != nullptr)
		{
			// Copy the data from our matched NamedResolution to the owning struct
			FMemory::Memcpy(StructData, NamedResolution, sizeof(FMovieGraphNamedResolution));

			// Transact on outer objects
			TArray<UObject*> OutObjects;
			PinnedStructPropertyHandle->GetOuterObjects(OutObjects);

			for (UObject* Object : OutObjects)
			{
				if (Object)
				{
					constexpr bool bAlwaysMarkDirty = true;	
					Object->Modify(bAlwaysMarkDirty);
				}
			}
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("%hs: Unable to get struct data from StructPropertyHandle."), __FUNCTION__);
		}
	}

	void OnComboBoxSelectionChanged(const FName NewValue, ESelectInfo::Type)
	{
		SetCurrentOptionAndCacheTooltipText(NewValue);

		UpdateOwningStruct();

		CacheAspectRatio();
	}

	/**
	 * The widget that allows the end user to select the output resolution
	 */
	TSharedPtr<SComboBox<FName>> ComboBox;

	/**
	 * The image that indicates whether the aspect ratio is locked
	 */
	TSharedPtr<SImage> LockImage;

	/**
	 * Weak ptr to the handle of the struct which owns this customization
	 */
	TWeakPtr<IPropertyHandle> StructPropertyHandle;

	/**
	 * Handle of the widget representing the X dimension of the 'Custom' resolution
	 */
	TSharedPtr<IPropertyHandle> ResolutionXPropertyHandle;
	/**
	 * Handle of the widget representing the Y dimension of the 'Custom' resolution
	 */
	TSharedPtr<IPropertyHandle> ResolutionYPropertyHandle;

	/**
	 * The option currently selected in the combobox
	 */
	FName CurrentOption = NAME_None;
	
	/**
	 * The tooltip text for the currently selected option
	 */
	FText CachedSelectedOptionTooltipText;

	/**
	 * If true, changing one dimension of the custom resolution will also
	 * change the other dimension to maintain the aspect ratio.
	 */
	bool bLockedAspectRatio = true;

	/**
	 * The aspect ratio of the currently selected resolution.
	 * Updates when the aspect ratio lock is applied, a named resolution is selected,
	 * or when the 'Custom' resolution values are changed.
	 */
	float CurrentAspectRatio = 0.0f;

	/**
	 * The options for the combo box, populated from UMovieGraphProjectSettings::DefaultNamedResolutions.
	 * A 'custom' entry is added to the end of the list, representing an arbitrary user-defined resolution.
	 */
	TArray<FName> ComboBoxOptions;

	/**
	 * A binding that updates the ComboBoxOptions when UMovieGraphProjectSettings::DefaultNamedResolutions changes
	 */
	FDelegateHandle OnProjectSettingsModifiedHandle;


	/**
	 * Default FMovieGraphNamedResolution for the 'custom' option
	 */
	inline static FMovieGraphNamedResolution CustomEntry =
		FMovieGraphNamedResolution(FMovieGraphNamedResolution::CustomEntryName, FIntPoint(1920, 1080), FMovieGraphNamedResolution::CustomEntryName.ToString());
};

#undef LOCTEXT_NAMESPACE
