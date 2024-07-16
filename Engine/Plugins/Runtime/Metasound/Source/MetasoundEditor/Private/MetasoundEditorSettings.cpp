// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorSettings.h"

#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "AudioWidgetsStyle.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Internationalization/Text.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UObject/UnrealType.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorSettings)

#define LOCTEXT_NAMESPACE "MetasoundEditorSettings"

UMetasoundEditorSettings::UMetasoundEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// pin type colors
	DefaultPinTypeColor = FLinearColor(0.750000f, 0.6f, 0.4f, 1.0f);			// light brown

	AudioPinTypeColor = FLinearColor(1.0f, 0.3f, 1.0f, 1.0f);					// magenta
	BooleanPinTypeColor = FLinearColor(0.300000f, 0.0f, 0.0f, 1.0f);			// maroon
	FloatPinTypeColor = FLinearColor(0.357667f, 1.0f, 0.060000f, 1.0f);			// bright green
	IntPinTypeColor = FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f);		// green-blue
	ObjectPinTypeColor = FLinearColor(0.0f, 0.4f, 0.910000f, 1.0f);				// sharp blue
	StringPinTypeColor = FLinearColor(1.0f, 0.0f, 0.660537f, 1.0f);				// bright pink
	TimePinTypeColor = FLinearColor(0.3f, 1.0f, 1.0f, 1.0f);					// cyan
	TriggerPinTypeColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);					// white
	WaveTablePinTypeColor = FLinearColor(0.580392f, 0.0f, 0.827450f, 1.0f);		// purple

	NativeNodeTitleColor = FLinearColor(0.4f, 0.85f, 0.35f, 1.0f);				// pale green
	AssetReferenceNodeTitleColor = FLinearColor(0.047f, 0.686f, 0.988f);		// sky blue
	InputNodeTitleColor = FLinearColor(0.168f, 1.0f, 0.7294f);					// sea foam
	OutputNodeTitleColor = FLinearColor(1.0f, 0.878f, 0.1686f);					// yellow
	VariableNodeTitleColor = FLinearColor(0.211f, 0.513f, 0.035f);				// copper
}

#if WITH_EDITOR
void UMetasoundEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetasoundEditorSettings, bPinMetaSoundPatchInAssetMenu)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetasoundEditorSettings, bPinMetaSoundSourceInAssetMenu))
	{
		FNotificationInfo Info(LOCTEXT("MetasoundEditorSettings_ChangeRequiresEditorRestart", "Change to Asset Menu Settings requires editor restart in order for changes to take effect."));
		Info.bFireAndForget = true;
		Info.ExpireDuration = 2.0f;
		Info.bUseThrobber = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}
#endif // WITH_EDITOR

const FAudioMaterialKnobStyle* UMetasoundEditorSettings::GetKnobStyle() const
{
	if (const UObject* Style = KnobStyleOverride.TryLoad())
	{
		if (const USlateWidgetStyleAsset* SlateWidgetStyleAsset = CastChecked<USlateWidgetStyleAsset>(Style))
		{
			return SlateWidgetStyleAsset->GetStyle<FAudioMaterialKnobStyle>();
		}
	}

	return &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialKnobStyle>("AudioMaterialKnob.Style");
}

const FAudioMaterialSliderStyle* UMetasoundEditorSettings::GetSliderStyle() const
{
	if (const UObject* Style = SliderStyleOverride.TryLoad())
	{
		if (const USlateWidgetStyleAsset* SlateWidgetStyleAsset = CastChecked<USlateWidgetStyleAsset>(Style))
		{
			return SlateWidgetStyleAsset->GetStyle<FAudioMaterialSliderStyle>();
		}
	}

	return &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialSliderStyle>("AudioMaterialSlider.Style");
}

const FAudioMaterialButtonStyle* UMetasoundEditorSettings::GetButtonStyle() const
{
	if (const UObject* Style = ButtonStyleOverride.TryLoad())
	{
		if (const USlateWidgetStyleAsset* SlateWidgetStyleAsset = CastChecked<USlateWidgetStyleAsset>(Style))
		{
			return SlateWidgetStyleAsset->GetStyle<FAudioMaterialButtonStyle>();
		}
	}

	return &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialButtonStyle>("AudioMaterialButton.Style");
}

const FAudioMaterialMeterStyle* UMetasoundEditorSettings::GetMeterStyle() const
{
	if (const UObject* Style = MeterStyleOverride.TryLoad())
	{
		if (const USlateWidgetStyleAsset* SlateWidgetStyleAsset = CastChecked<USlateWidgetStyleAsset>(Style))
		{
			return SlateWidgetStyleAsset->GetStyle<FAudioMaterialMeterStyle>();
		}
	}

	return &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialMeterStyle>("AudioMaterialMeter.Style");
}

Metasound::Engine::FAuditionPageInfo UMetasoundEditorSettings::GetAuditionPageInfo(const FMetasoundFrontendDocument& InDocument) const
{
	using namespace Metasound::Engine;

	FAuditionPageInfo PreviewInfo { .PlatformName = AuditionPlatform };

	TSet<FGuid> DocPageIds;
	InDocument.RootGraph.IterateGraphPages([&DocPageIds](const FMetasoundFrontendGraph& PageGraph)
	{
		DocPageIds.Add(PageGraph.PageID);
	});

	auto PageIsCooked = [&DocPageIds, &PreviewInfo](const FMetaSoundPageSettings& PageSettings)
	{
		if (DocPageIds.Contains(PageSettings.UniqueId))
		{
			return PageSettings.IsCooked.GetValueForPlatform(PreviewInfo.PlatformName);
		}

		return false;
	};

	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
	{
		if (const FMetaSoundPageSettings* TargetPageSettings = Settings->FindPageSettings(AuditionTargetPage))
		{
			const FGuid& TargetPageID = TargetPageSettings->UniqueId;
			const TArray<FMetaSoundPageSettings>& PageSettingsArray = Settings->GetPageSettings();
			bool bFoundMatch = false;
			for (int32 Index = PageSettingsArray.Num() - 1; Index >= 0; --Index)
			{
				const FMetaSoundPageSettings& PageSettings = PageSettingsArray[Index];
				bFoundMatch |= PageSettings.UniqueId == TargetPageID;
				if (bFoundMatch)
				{
					if (PageIsCooked(PageSettings))
					{
						PreviewInfo.PageID = PageSettings.UniqueId;
						return PreviewInfo;
					}
				}
			}
		}
	}

	return PreviewInfo;

}
#undef LOCTEXT_NAMESPACE // "MetaSoundEditor"
