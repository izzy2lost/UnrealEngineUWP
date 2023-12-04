// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubSubjectSessionConfig.h"

#include "Async/Async.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkHubModule.h"
#include "Modules/ModuleManager.h"
#include "LiveLinkClient.h"
#include "UObject/StrongObjectPtr.h"

ULiveLinkHubSubjectSessionConfig::ULiveLinkHubSubjectSessionConfig()
{
	if (!IsTemplate())
	{
		FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient.OnLiveLinkSubjectAdded().AddUObject(this, &ULiveLinkHubSubjectSessionConfig::OnSubjectAdded_AnyThread);
		LiveLinkClient.OnLiveLinkSubjectRemoved().AddUObject(this, &ULiveLinkHubSubjectSessionConfig::OnSubjectRemoved_AnyThread);

		constexpr bool bIncludeDisabledSubject = true;
		constexpr bool bIncludeVirtualSubject = true;
		for (const FLiveLinkSubjectKey& SubjectKey : LiveLinkClient.GetSubjects(bIncludeDisabledSubject, bIncludeVirtualSubject))
		{
			ULiveLinkHubSubjectProxy* SubjectSettings = NewObject<ULiveLinkHubSubjectProxy>();
			SubjectSettings->Initialize(SubjectKey, LiveLinkClient.GetSourceType(SubjectKey.Source).ToString());

			SubjectProxies.Add(SubjectKey, SubjectSettings);
		}
	}
}

ULiveLinkHubSubjectSessionConfig::~ULiveLinkHubSubjectSessionConfig()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
    {
    	FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
    	LiveLinkClient.OnLiveLinkSubjectRemoved().RemoveAll(this);
    	LiveLinkClient.OnLiveLinkSubjectAdded().RemoveAll(this);
    }
}

ULiveLinkHubSubjectProxy* ULiveLinkHubSubjectSessionConfig::GetSubjectConfig(const FLiveLinkSubjectKey& InSubject)
{
    ULiveLinkHubSubjectProxy* Settings = nullptr;

    if (TObjectPtr<ULiveLinkHubSubjectProxy>* SettingsPtr = SubjectProxies.Find(InSubject))
    {
    	Settings = SettingsPtr->Get();
    }

    return Settings;
}

void ULiveLinkHubSubjectSessionConfig::OnSubjectAdded_AnyThread(FLiveLinkSubjectKey SubjectKey)
{
	TWeakObjectPtr<ULiveLinkHubSubjectSessionConfig> Self = this;
	AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
	{
		ULiveLinkHubSubjectSessionConfig* Container = Self.Get();
		if (UObjectInitialized() && Container)
		{
			Container->OnSubjectAdded(Key);
		}
	});
}

void ULiveLinkHubSubjectSessionConfig::OnSubjectAdded(const FLiveLinkSubjectKey& SubjectKey)
{
	if (!SubjectProxies.Contains(SubjectKey))
	{
		ILiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		ULiveLinkHubSubjectProxy* SubjectSettings = NewObject<ULiveLinkHubSubjectProxy>();
		SubjectSettings->Initialize(SubjectKey, LiveLinkClient.GetSourceType(SubjectKey.Source).ToString());

		SubjectProxies.FindOrAdd(SubjectKey) = SubjectSettings;
	}
}

void ULiveLinkHubSubjectSessionConfig::OnSubjectRemoved_AnyThread(FLiveLinkSubjectKey SubjectKey)
{
	TWeakObjectPtr<ULiveLinkHubSubjectSessionConfig> Self = this;
	AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
	{
		ULiveLinkHubSubjectSessionConfig* Container = Self.Get();
		if (UObjectInitialized() && Container)
		{
			Container->OnSubjectAdded(Key);
		}
	});
}

void ULiveLinkHubSubjectSessionConfig::OnSubjectRemoved(const FLiveLinkSubjectKey& SubjectKey)
{
	SubjectProxies.Remove(SubjectKey);
}

void ULiveLinkHubSubjectProxy::Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource)
{
	SubjectName = SubjectKey.SubjectName.Name.ToString();
	SubjectKey = InSubjectKey;
	OutboundName = SubjectKey.SubjectName.Name.ToString();
	Source = MoveTemp(InSource);
}

void ULiveLinkHubSubjectProxy::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectProxy, OutboundName))
	{
		PreviousOutboundName = *OutboundName;
	}
}

void ULiveLinkHubSubjectProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetName() == GET_MEMBER_NAME_CHECKED(ULiveLinkHubSubjectProxy, OutboundName))
	{
		NotifyRename();
	}
}

FName ULiveLinkHubSubjectProxy::GetOutboundName() const
{
	if (bPendingOutboundNameChange)
	{
		return PreviousOutboundName;
	}

	return *OutboundName;
}

void ULiveLinkHubSubjectProxy::NotifyRename()
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	if (TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHubModule.GetLiveLinkProvider())
	{
		if (PreviousOutboundName != *OutboundName)
		{
			bPendingOutboundNameChange = true;

			// We need to keep the last subject static data to send it.
			Provider->SendClearSubjectToConnections(SubjectKey.SubjectName.Name);

			TPair<UClass*, FLiveLinkStaticDataStruct*> StaticData = Provider->GetLastSubjectStaticDataStruct(SubjectKey.SubjectName.Name);
			if (StaticData.Key && StaticData.Value)
			{
				FLiveLinkStaticDataStruct StaticDataCopy;
				StaticDataCopy.InitializeWith(*StaticData.Value);

				Provider->UpdateSubjectStaticData(*OutboundName, StaticData.Key, MoveTemp(StaticDataCopy));
			}

			Provider->RemoveSubject(PreviousOutboundName);

			bPendingOutboundNameChange = false;
			PreviousOutboundName = *OutboundName;
		}
	}
}
