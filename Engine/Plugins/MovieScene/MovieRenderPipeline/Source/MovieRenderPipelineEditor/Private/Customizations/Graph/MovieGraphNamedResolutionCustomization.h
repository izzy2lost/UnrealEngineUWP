// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/MovieGraphNamedResolution.h"
#include "Graph/MovieGraphProjectSettings.h"

#include "MovieRenderPipelineCoreModule.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyTypeCustomization.h"
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

	/**
	 * Generates a widget for the given option with the option name as STextBlock with tooltip text .
	 * @param InOption The name of the option.
	 * @return A shared reference to the generated widget.
	 */
	TSharedRef<SWidget> MakeWidgetForOption(FName InOption) const
	{
		return
			SNew(STextBlock)
			.Text(FText::FromName(InOption))
			.ToolTipText(this, &FMovieGraphNamedResolutionCustomization::GetOptionTooltipText, InOption)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		;
	}

	TSharedRef<SWidget> CreateOptionComboBox()
	{
		return
			SNew(SComboBox<FName>)
			.OnComboBoxOpening(this, &FMovieGraphNamedResolutionCustomization::RepopulateOptions)
			.OptionsSource(&ComboBoxOptions)
			.OnSelectionChanged(this, &FMovieGraphNamedResolutionCustomization::OnComboBoxSelectionChanged)
			.OnGenerateWidget(this, &FMovieGraphNamedResolutionCustomization::MakeWidgetForOption)
			.InitiallySelectedItem(GetCurrentOptionAssigningIfNeeded())
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::FromName(GetCurrentOptionAssigningIfNeeded());
				})
				.ToolTipText_Lambda([this]()
				{
					return CachedSelectedOptionTooltipText;
				})
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	TSharedRef<SWidget> CreateCustomResolutionWidget(const TSharedRef<IPropertyHandle> InStructPropertyHandle)
	{
		const TSharedRef<IPropertyHandle> ResolutionPropHandle = InStructPropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FMovieGraphNamedResolution, Resolution)).ToSharedRef();
		
		return
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]()
			{
				return GetCurrentOptionAssigningIfNeeded().IsEqual(FMovieGraphNamedResolution::CustomEntryName) ?
					EVisibility::Visible : EVisibility::Collapsed;
			})

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Width")))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0, 0, 8, 0)
			[
				ResolutionPropHandle->GetChildHandle(0)->CreatePropertyValueWidget()
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Height")))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				ResolutionPropHandle->GetChildHandle(1)->CreatePropertyValueWidget()
			];
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
	
	//~ Begin IPropertyTypeCustomization interface
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		StructPropertyHandle = InStructPropertyHandle;
		
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

		HeaderRow
		.NameContent()
		[
			// Show "Resolution" as the row name
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.Padding(0, 2, 8, 2)
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				CreateOptionComboBox()
			]

			+ SVerticalBox::Slot()
			.Padding(0, 2, 8, 2)
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				CreateCustomResolutionWidget(InStructPropertyHandle)
			]
		];
	}

	// Skip customizing children
	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> InStructPropertyHandle,
		IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override { }
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
	}

	/**
	 * Weak ptr to the handle of the struct which owns this customization
	 */
	TWeakPtr<IPropertyHandle> StructPropertyHandle;

	/**
	 * The option currently selected in the combobox
	 */
	FName CurrentOption = NAME_None;
	/**
	 * The tooltip text for the currently selected option
	 */
	FText CachedSelectedOptionTooltipText;

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
