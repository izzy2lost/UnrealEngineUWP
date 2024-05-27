// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubSubjectSessionConfig.h"

#include "Async/Async.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubClient.h"
#include "LiveLinkHubModule.h"
#include "LiveLinkSubjectSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"


void ULiveLinkHubSubjectSessionConfig::Initialize()
{
	FLiveLinkHubClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkHubClient>(ILiveLinkClient::ModularFeatureName);

	constexpr bool bIncludeDisabledSubject = true;
	constexpr bool bIncludeVirtualSubject = true;

	for (const FLiveLinkSubjectKey& SubjectKey : LiveLinkClient.GetSubjects(bIncludeDisabledSubject, bIncludeVirtualSubject))
	{
		FLiveLinkHubSubjectProxy SubjectSettings;
		SubjectSettings.Initialize(SubjectKey, LiveLinkClient.GetSourceType(SubjectKey.Source).ToString());

		SubjectProxies.Add(SubjectKey, MoveTemp(SubjectSettings));

		ULiveLinkHubSubjectProcessors* Processors = nullptr;
		if (TObjectPtr<ULiveLinkHubSubjectProcessors>* ProcessorsPtr = SubjectProcessors.Find(SubjectKey))
		{
			Processors = *ProcessorsPtr;
		}
		else
		{
			Processors = NewObject<ULiveLinkHubSubjectProcessors>(this);
			SubjectProcessors.Add(SubjectKey, Processors);
		}

		// Apply restored settings to the subject.
		ULiveLinkSubjectSettings* Settings = Cast<ULiveLinkSubjectSettings>(LiveLinkClient.GetSubjectSettings(SubjectKey));

		Settings->PreProcessors = Processors->PreProcessors;
		Settings->Translators.Reset();

		if (Processors->Translator)
		{
			Settings->Translators.Add(Processors->Translator);
		}

		if (Settings->ValidateProcessors())
		{
			LiveLinkClient.CacheSubjectSettings(SubjectKey, Settings);
		}
	}
}

TOptional<FLiveLinkHubSubjectProxy> ULiveLinkHubSubjectSessionConfig::GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const
{
	TOptional<FLiveLinkHubSubjectProxy> Settings;

    if (const FLiveLinkHubSubjectProxy* SettingsPtr = SubjectProxies.Find(InSubject))
    {
    	Settings = *SettingsPtr;
    }

    return Settings;
}

ULiveLinkHubSubjectProcessors* ULiveLinkHubSubjectSessionConfig::GetSubjectProcessors(const FLiveLinkSubjectKey& InSubject)
{
	if (TObjectPtr<ULiveLinkHubSubjectProcessors>* SettingsPtr = SubjectProcessors.Find(InSubject))
	{
		return *SettingsPtr;
	}

	return nullptr;
}


void ULiveLinkHubSubjectSessionConfig::RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName)
{
	if (FLiveLinkHubSubjectProxy* Proxy = SubjectProxies.Find(SubjectKey))
	{
		Proxy->SetOutboundName(NewName);
	}
}

void FLiveLinkHubSubjectProxy::Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource)
{
	SubjectName = InSubjectKey.SubjectName.Name.ToString();
	SubjectKey = InSubjectKey;
	OutboundName = InSubjectKey.SubjectName.Name.ToString();
	Source = MoveTemp(InSource);
}

FName FLiveLinkHubSubjectProxy::GetOutboundName() const
{
	if (bPendingOutboundNameChange)
	{
		return PreviousOutboundName;
	}

	return *OutboundName;
}

void FLiveLinkHubSubjectProxy::SetOutboundName(FName NewName)
{
	PreviousOutboundName = *OutboundName;
	OutboundName = *NewName.ToString();

	NotifyRename();
}

void FLiveLinkHubSubjectProxy::NotifyRename()
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	if (TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHubModule.GetLiveLinkProvider())
	{
		if (PreviousOutboundName != *OutboundName)
		{
			bPendingOutboundNameChange = true;

			Provider->SendClearSubjectToConnections(PreviousOutboundName);

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

			bPendingOutboundNameChange = false;
			PreviousOutboundName = *OutboundName;
		}
	}
}

void ULiveLinkHubSubjectProcessors::Initialize(ULiveLinkSubjectSettings* InSubjectSettings)
{
	PreProcessors.Append(InSubjectSettings->PreProcessors);

	Translator = nullptr;
	if (InSubjectSettings->Translators.Num() && InSubjectSettings->Translators[0])
	{
		Translator = InSubjectSettings->Translators[0];
	}
}
