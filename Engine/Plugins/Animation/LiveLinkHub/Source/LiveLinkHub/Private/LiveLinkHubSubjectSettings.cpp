// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubSubjectSettings.h"

#include "LiveLinkHubClient.h"
#include "LiveLinkHubModule.h"
#include "Features/IModularFeatures.h"
#include "Clients/LiveLinkHubProvider.h"

void ULiveLinkHubSubjectSettings::Initialize(FLiveLinkSubjectKey InSubjectKey)
{
	ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	SubjectName = InSubjectKey.SubjectName.ToString();
	Key = InSubjectKey;

	OutboundName = SubjectName;

	Source = LiveLinkClient->GetSourceType(InSubjectKey.Source).ToString();
}

void ULiveLinkHubSubjectSettings::NotifyRename()
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	if (TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHubModule.GetLiveLinkProvider())
	{
		// Re-send the last static data with the new name.
		TPair<UClass*, FLiveLinkStaticDataStruct*> StaticData = Provider->GetLastSubjectStaticDataStruct(PreviousOutboundName);
		if (StaticData.Key && StaticData.Value)
		{
			FLiveLinkStaticDataStruct StaticDataCopy;
			StaticDataCopy.InitializeWith(*StaticData.Value);

			Provider->UpdateSubjectStaticData(*OutboundName, StaticData.Key, MoveTemp(StaticDataCopy));
		}

		// Then clear the old static data entry in the provider.
		Provider->RemoveSubject(PreviousOutboundName);
	}
}

void ULiveLinkHubSubjectSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, OutboundName))
	{
		PreviousOutboundName = *OutboundName;

		FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		if (TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHubModule.GetLiveLinkProvider())
		{
			Provider->SendClearSubjectToConnections(PreviousOutboundName);
		}
	}
}

void ULiveLinkHubSubjectSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, OutboundName))
	{
		if (PreviousOutboundName != *OutboundName)
		{
			if (OutboundName.IsEmpty())
			{
				OutboundName = PreviousOutboundName.ToString();
			}
			NotifyRename();
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, Translators)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, PreProcessors)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, Remapper))
	{
		FLiveLinkHubClient* LiveLinkClient = static_cast<FLiveLinkHubClient*>(&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName));
		LiveLinkClient->CacheSubjectSettings(Key, this);
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectSettings, TranslatorsProxy))
	{
		Translators.Reset(1);
		if (TranslatorsProxy)
		{
			Translators.Add(TranslatorsProxy);
		}

		ValidateProcessors();

		// Re-assign TranslatorsProxy in case the translator was denied in the validate function.
		if (Translators.Num())
		{
			TranslatorsProxy = Translators[0];
		}
		else
		{
			TranslatorsProxy = nullptr;
		}

		FLiveLinkHubClient* LiveLinkClient = static_cast<FLiveLinkHubClient*>(&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName));
		LiveLinkClient->CacheSubjectSettings(Key, this);
	}
}
