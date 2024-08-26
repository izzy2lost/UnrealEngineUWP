// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/MovieScenePlatformConditionCustomization.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "Conditions/MovieScenePlatformCondition.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "MovieSceneDynamicBindingCustomization"

TSharedRef<IDetailCustomization> FMovieScenePlatformConditionCustomization::MakeInstance()
{
	return MakeShareable(new FMovieScenePlatformConditionCustomization);
}

void FMovieScenePlatformConditionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ValidPlatformsPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieScenePlatformCondition, ValidPlatforms), UMovieScenePlatformCondition::StaticClass());
	ValidPlatformsPropertyHandle->MarkHiddenByCustomization();
	IDetailCategoryBuilder& PlatformsCategory = DetailBuilder.EditCategory(TEXT("Valid Platforms"));

	const TArray<const FDataDrivenPlatformInfo*>& PlatformInfos = FDataDrivenPlatformInfoRegistry::GetSortedPlatformInfos(EPlatformInfoType::TruePlatformsOnly);

	const auto& GetCurrentValidPlatformNames = [](TSharedPtr<IPropertyHandle> ValidPlatformsPropertyHandle) -> TArray<FName>
	{
		TArray<FName> Names;

		TArray<void*> RawData;
		ValidPlatformsPropertyHandle->AccessRawData(RawData);

		if (RawData.Num() > 0)
		{
			if (TArray<FName>* CurrentValidPlatformNamesPtr = reinterpret_cast<TArray<FName>*>(RawData[0]))
			{
				Names = *CurrentValidPlatformNamesPtr;
			}
		}
		return Names;
	};

	auto GetComboButtonText = [SharedThis = StaticCastSharedRef<FMovieScenePlatformConditionCustomization>(AsShared()), GetCurrentValidPlatformNames]() -> FText
	{
		TArray<FName> CurrentValidPlatformNames = GetCurrentValidPlatformNames(SharedThis->ValidPlatformsPropertyHandle);
		TArray<FText> CurrentValidPlatforms;
		CurrentValidPlatforms.Reserve(CurrentValidPlatformNames.Num());

		for (const FName& PlatformName : CurrentValidPlatformNames)
		{
			CurrentValidPlatforms.Add(FText::FromName(PlatformName));
		}
		if (CurrentValidPlatforms.Num() > 3)
		{
			CurrentValidPlatforms.SetNum(3);
			CurrentValidPlatforms.Add(FText::FromString("..."));
		}

		return FText::Join(FText::FromString(", "), CurrentValidPlatforms);
	};

	PlatformsCategory.AddCustomRow(LOCTEXT("ValidPlatforms", "Valid Platforms"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ValidPlatforms", "Valid Platforms"))
			.ToolTipText(LOCTEXT("ValidPlatformsTooltip", "Which platforms will pass the condition"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			
			// Constructs the UI for bitmask property editing.
			SNew(SComboButton)
			.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda(GetComboButtonText)
				]
			.OnGetMenuContent_Lambda([&DetailBuilder, SharedThis = StaticCastSharedRef<FMovieScenePlatformConditionCustomization>(AsShared()), PlatformInfos, GetCurrentValidPlatformNames]()
				{
					TArray<FName> CurrentValidPlatformNames = GetCurrentValidPlatformNames(SharedThis->ValidPlatformsPropertyHandle);

					FMenuBuilder MenuBuilder(false, nullptr);

					for (int32 i = 0; i < PlatformInfos.Num(); ++i)
					{
						MenuBuilder.AddMenuEntry(
							FText::FromName(PlatformInfos[i]->IniPlatformName),
							FText(),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), PlatformInfos[i]->GetIconStyleName(EPlatformIconSize::Normal)),
							FUIAction
							(
								FExecuteAction::CreateLambda([SharedThis, i, PlatformInfos, CurrentValidPlatformNames, &DetailBuilder]()
									{
										TArray<FName> NewValidPlatformNames = CurrentValidPlatformNames;
										if (NewValidPlatformNames.Contains(PlatformInfos[i]->IniPlatformName))
										{
											NewValidPlatformNames.Remove(PlatformInfos[i]->IniPlatformName);
										}
										else
										{
											NewValidPlatformNames.Add(PlatformInfos[i]->IniPlatformName);
										}

										TArray<void*> RawData;
										SharedThis->ValidPlatformsPropertyHandle->AccessRawData(RawData);
										if (RawData.Num() == 1)
										{
											if (TArray<FName>* CurrentValidPlatformNamesPtr = reinterpret_cast<TArray<FName>*>(RawData[0]))
											{
												*CurrentValidPlatformNamesPtr = NewValidPlatformNames;
												SharedThis->ValidPlatformsPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
												DetailBuilder.ForceRefreshDetails();
											}
										}
									}),
								FCanExecuteAction(),
								FIsActionChecked::CreateLambda([SharedThis, i, PlatformInfos, CurrentValidPlatformNames]() -> bool
									{
										return CurrentValidPlatformNames.Contains(PlatformInfos[i]->IniPlatformName);
									})
							),
							NAME_None,
							EUserInterfaceActionType::Check);
					}

					return MenuBuilder.MakeWidget();
				})
			
		]; 
}

#undef LOCTEXT_NAMESPACE