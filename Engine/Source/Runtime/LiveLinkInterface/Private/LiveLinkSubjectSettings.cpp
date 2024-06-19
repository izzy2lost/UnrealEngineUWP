// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkSubjectSettings.h"

#include "LiveLinkFrameInterpolationProcessor.h"
#include "LiveLinkFramePreProcessor.h"
#include "LiveLinkFrameTranslator.h"
#include "LiveLinkRole.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkSubjectSettings)

#if WITH_EDITOR
DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkSubjectSettings, Warning, Warning);

ULiveLinkSubjectSettings::ULiveLinkSubjectSettings()
{
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		bRebroadcastSubject = GetDefault<ULiveLinkDefaultSubjectSettings>()->bRebroadcastSubjectsByDefault;
		bAllowModifyingRebroadcast = GetDefault<ULiveLinkDefaultSubjectSettings>()->bAllowEditingRebroadcastProperty;
	}
}

bool ULiveLinkSubjectSettings::ValidateProcessors()
{
	UClass* RoleClass = Role.Get();
	if (RoleClass == nullptr)
	{
		PreProcessors.Reset();
		InterpolationProcessor = nullptr;
		Translators.Reset();
		return false;
	}
	else
	{
		bool bValidProcessors = true;
		for (int32 Index = 0; Index < PreProcessors.Num(); ++Index)
		{
			if (ULiveLinkFramePreProcessor* PreProcessor = PreProcessors[Index])
			{
				check(PreProcessor->GetRole() != nullptr);
				if (!RoleClass->IsChildOf(PreProcessor->GetRole()))
				{
					UE_LOG(LogLiveLinkSubjectSettings, Warning, TEXT("Role '%s' is not supported by pre processors '%s'"), *RoleClass->GetName(), *PreProcessor->GetName());
					PreProcessors[Index] = nullptr;
					bValidProcessors = false;
				}
			}
		}

		if (InterpolationProcessor)
		{
			check(InterpolationProcessor->GetRole() != nullptr);
			if (!RoleClass->IsChildOf(InterpolationProcessor->GetRole()))
			{
				UE_LOG(LogLiveLinkSubjectSettings, Warning, TEXT("Role '%s' is not supported by interpolation '%s'"), *RoleClass->GetName(), *InterpolationProcessor->GetName());
				InterpolationProcessor = nullptr;
				bValidProcessors = false;
			}
		}

		for (int32 Index = 0; Index < Translators.Num(); ++Index)
		{
			if (ULiveLinkFrameTranslator* Translator = Translators[Index])
			{
				check(Translator->GetFromRole() != nullptr);
				if (!RoleClass->IsChildOf(Translator->GetFromRole()))
				{
					UE_LOG(LogLiveLinkSubjectSettings, Warning, TEXT("Role '%s' is not supported by translator '%s'"), *RoleClass->GetName(), *Translator->GetName());
					Translators[Index] = nullptr;
					bValidProcessors = false;
				}
			}
		}

		return bValidProcessors;
	}
}

void ULiveLinkSubjectSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkSubjectSettings, PreProcessors)
	 || PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkSubjectSettings, InterpolationProcessor)
	|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkSubjectSettings, Translators))
	{
		ValidateProcessors();
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR
