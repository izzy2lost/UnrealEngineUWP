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

	void UpdateLockImage()
	{
		LockImage->SetImage(GetLockBrush());
		LockImage->SetToolTipText(GetLockTooltipText());
	}

	void ToggleAspectRatioLock()
	{
		bLockedAspectRatio = !bLockedAspectRatio;
		if (bLockedAspectRatio) { CacheAspectRatio(); }

		UpdateLockImage();
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

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		const TSharedPtr<IPropertyHandle> ResolutionPropertyHandle =
			InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMovieGraphNamedResolution, Resolution));

		ResolutionPropertyHandle->SetOnChildPropertyValuePreChange(
			FSimpleDelegate::CreateSP(this, &FMovieGraphNamedResolutionCustomization::OnCustomResolutionPreManualChange));
		ResolutionPropertyHandle->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(
			this, &FMovieGraphNamedResolutionCustomization::OnCustomResolutionManualChange));

		if (const TSharedPtr<IPropertyHandle> PropertyXHandle = ResolutionPropertyHandle->GetChildHandle(0))
		{			
			IDetailPropertyRow& PropertyXRow = StructBuilder.AddProperty(PropertyXHandle.ToSharedRef());
			ResolutionXPropertyHandle = PropertyXHandle;

			// Prevent showing Reset to Default
			PropertyXRow.OverrideResetToDefault(FResetToDefaultOverride::Hide());

			PropertyXRow.CustomWidget()
			.NameContent()
			[
				PropertyXHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.HAlign(HAlign_Fill)
			[
				PropertyXHandle->CreatePropertyValueWidget()
			]
			.ExtensionContent()
			[
				MakeLockExtensionWidget()
			];

			UpdateLockImage();
		}

		if (const TSharedPtr<IPropertyHandle> PropertyYHandle = ResolutionPropertyHandle->GetChildHandle(1))
		{			
			IDetailPropertyRow& PropertyYRow = StructBuilder.AddProperty(PropertyYHandle.ToSharedRef());
			ResolutionYPropertyHandle = PropertyYHandle;

			// Prevent showing Reset to Default
			PropertyYRow.OverrideResetToDefault(FResetToDefaultOverride::Hide());

			// Custom widget to enforce length and padding on value widget
			PropertyYRow.CustomWidget()
			.NameContent()
			[
				PropertyYHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.Padding(FMargin(0, 0, 24, 0))
				[
					PropertyYHandle->CreatePropertyValueWidget()
				]
			];
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

	void CacheAspectRatio()
	{
		CurrentAspectRatio = (double)CustomEntry.Resolution.X / CustomEntry.Resolution.Y;
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

	void OnCustomResolutionPreManualChange()
	{
		// If the custom resolution is changed, switch to 'Custom' option
		if (!CurrentOption.IsEqual(FMovieGraphNamedResolution::CustomEntryName))
		{
			UpdateCustomEntryValues();

			// Switch to 'Custom' option
			SetCurrentOptionAndCacheTooltipText(FMovieGraphNamedResolution::CustomEntryName);
		}
	}

	int32 RoundToNearestEvenNumber(double InNumberToRound) const
	{
		// Round to nearest even
		const int32 Floored = FMath::FloorToInt(InNumberToRound);
		const int32 Ceiled = FMath::CeilToInt(InNumberToRound);

		return Floored % 2 == 0 ? Floored : Ceiled;
	}

	void OnCustomResolutionManualChange(const FPropertyChangedEvent& Event) const
	{
		if (!ensureMsgf(ResolutionXPropertyHandle, TEXT("%hs: `ResolutionXPropertyHandle` is null."), __FUNCTION__))
		{
			return;
		}
		if (!ensureMsgf(ResolutionYPropertyHandle, TEXT("%hs: `ResolutionYPropertyHandle` is null."), __FUNCTION__))
		{
			return;
		}

		// Update CustomEntry values
		int32 ResolutionX = 0;
		ResolutionXPropertyHandle->GetValue(ResolutionX);

		CustomEntry.Resolution.X = ResolutionX;

		int32 ResolutionY = 0;
		ResolutionYPropertyHandle->GetValue(ResolutionY);

		CustomEntry.Resolution.Y = ResolutionY;

		// Enforce aspect ratio if desired
		if (bLockedAspectRatio && Event.ChangeType != EPropertyChangeType::Interactive)
		{
			if (Event.Property == ResolutionXPropertyHandle->GetProperty()) // X Changed
			{
				const double NewYResolution = (double)CustomEntry.Resolution.X / CurrentAspectRatio;

				// Specify an interactive change to avoid feedback loop
				ResolutionYPropertyHandle->SetValue(RoundToNearestEvenNumber(NewYResolution), EPropertyValueSetFlags::InteractiveChange);
			}
			else // Y Changed
			{
				const double NewXResolution = (double)CustomEntry.Resolution.Y * CurrentAspectRatio;
				ResolutionXPropertyHandle->SetValue(RoundToNearestEvenNumber(NewXResolution), EPropertyValueSetFlags::InteractiveChange);
			}
		}
	}

	/**
	 * Updates the struct which owns this customization with the data from the matched named resolution.
	 */
	void UpdateOwningStruct() const
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

			// Mark owning packages dirty
			TArray<UObject*> OutObjects;
			PinnedStructPropertyHandle->GetOuterObjects(OutObjects);

			for (const UObject* Object : OutObjects)
			{
				if (!Object)
				{
					continue;
				}
				ensureAlwaysMsgf(Object->MarkPackageDirty(), TEXT("%hs: Failed to mark package of %s dirty!"), __FUNCTION__, *Object->GetName());
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
