// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Clients/LiveLinkHubUEClientInfo.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubModule.h"
#include "LiveLinkHubSessionData.h"
#include "LiveLinkTypes.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Subjects/LiveLinkHubSubjectSessionConfig.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"


#define LOCTEXT_NAMESPACE "LiveLinkHubSession"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientAddedToSession, FLiveLinkHubClientId);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientRemovedFromSession, FLiveLinkHubClientId);

class ULiveLinkFramePreProcessor;
class ULiveLinkFrameTranslator;

/**
 * Holds the state of the hub for an active session, can be swapped out with a different session using the session manager.
 */
class ILiveLinkHubSession
{
public:
	virtual ~ILiveLinkHubSession() = default;

	/** Get the configuration for a given subject. */
	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& SubjectKey) const = 0;

	/** Get the preprocessors and translator for a given subject. */
	virtual ULiveLinkHubSubjectProcessors* GetSubjectProcessors(const FLiveLinkSubjectKey& SubjectKey) const = 0;

	/** Change the outbound name of a subject for this session. */
	virtual void RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName) = 0;

	/** Add a client to this session. Note: Must be called from game thread. */
	virtual void AddClient(const FLiveLinkHubClientId& Client) = 0;

	/** Remove a client from this session. Note: Must be called from game thread. */
	virtual void RemoveClient(const FLiveLinkHubClientId& Client) = 0;

	/** Returns whether a client is in this session. */
	virtual bool IsClientInSession(const FLiveLinkHubClientId& Client) = 0;

	/** Set the preprocessors for a subject. */
	virtual void SetPreProcessors(const FLiveLinkSubjectKey& SubjectKey, TConstArrayView<ULiveLinkFramePreProcessor*> PreProcessors) = 0;

	/** Set the translators for a subject. */
	virtual void SetTranslator(const FLiveLinkSubjectKey& SubjectKey, ULiveLinkFrameTranslator* Translator) = 0;

	/** Get the list of clients in this session (The list of clients that can receive data from the hub) */
	virtual TArray<FLiveLinkHubClientId> GetSessionClients() const = 0;
};

class FLiveLinkHubSession : public ILiveLinkHubSession, public TSharedFromThis<FLiveLinkHubSession>
{
public:
	FLiveLinkHubSession(FOnClientAddedToSession& OnClientAddedToSession, FOnClientRemovedFromSession& OnClientRemovedFromSession)
		: OnClientAddedToSessionDelegate(OnClientAddedToSession)
		, OnClientRemovedFromSessionDelegate(OnClientRemovedFromSession)
	{
		RegisterDelegates();

		SessionData = TStrongObjectPtr<ULiveLinkHubSessionData>(NewObject<ULiveLinkHubSessionData>(GetTransientPackage()));
		SessionData->SubjectsConfig->Initialize();
	}

	FLiveLinkHubSession(ULiveLinkHubSessionData* InSessionData, FOnClientAddedToSession& OnClientAddedToSession, FOnClientRemovedFromSession& OnClientRemovedFromSession)
		: OnClientAddedToSessionDelegate(OnClientAddedToSession)
		, OnClientRemovedFromSessionDelegate(OnClientRemovedFromSession)
	{
		SessionData = TStrongObjectPtr<ULiveLinkHubSessionData>(InSessionData);
		RegisterDelegates();
		SessionData->SubjectsConfig->Initialize();
	}

	virtual ~FLiveLinkHubSession() override
	{
		UnregisterDelegates();
	}

	virtual void RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName) override
	{
		TMap<FLiveLinkSubjectKey, FLiveLinkHubSubjectProxy> SubjectProxies;
		{
			FReadScopeLock Locker(SessionDataLock);

			SubjectProxies = SessionData->SubjectsConfig->SubjectProxies;
		}

		if (FLiveLinkHubSubjectProxy* Proxy = SubjectProxies.Find(SubjectKey))
		{
			Proxy->SetOutboundName(NewName);
		}

		{
			// Copied over in a different step to avoid acquiring the rw lock in a method called by RenameSubject
			FWriteScopeLock Locker(SessionDataLock);
			SessionData->SubjectsConfig->SubjectProxies = MoveTemp(SubjectProxies);
		}
	}

	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& SubjectKey) const override
	{
		FReadScopeLock Locker(SessionDataLock);
		return SessionData->SubjectsConfig->GetSubjectConfig(SubjectKey);
	}

	virtual ULiveLinkHubSubjectProcessors* GetSubjectProcessors(const FLiveLinkSubjectKey& SubjectKey) const override
	{
		FReadScopeLock Locker(SessionDataLock);
		return SessionData->SubjectsConfig->GetSubjectProcessors(SubjectKey);
	}

	virtual TArray<FLiveLinkHubClientId> GetSessionClients() const override
	{
		FReadScopeLock Locker(SessionDataLock);
		return CachedSessionClients.Array();
	}

	virtual void AddClient(const FLiveLinkHubClientId& Client) override
	{
		check(IsInGameThread());

		{
			if (TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkProvider())
			{
				if (TOptional<FLiveLinkHubUEClientInfo> ClientInfo = LiveLinkProvider->GetClientInfo(Client))
				{
					FWriteScopeLock Locker(SessionDataLock);
					CachedSessionClients.Add(Client);
				}
			}
		}

		OnClientAddedToSessionDelegate.Broadcast(Client);
	}

	virtual void RemoveClient(const FLiveLinkHubClientId& Client) override
	{
		check(IsInGameThread());

		{
			FWriteScopeLock Locker(SessionDataLock);
			CachedSessionClients.Remove(Client);
		}

		OnClientRemovedFromSessionDelegate.Broadcast(Client);
	}

	virtual bool IsClientInSession(const FLiveLinkHubClientId& Client) override
	{
		FReadScopeLock Locker(SessionDataLock);
		return CachedSessionClients.Contains(Client);
	}

	virtual void SetPreProcessors(const FLiveLinkSubjectKey& SubjectKey, TConstArrayView<ULiveLinkFramePreProcessor*> PreProcessors) override
	{
		FWriteScopeLock Locker(SessionDataLock);
		if (TObjectPtr<ULiveLinkHubSubjectProcessors>* SubjectProcessors = SessionData->SubjectsConfig->SubjectProcessors.Find(SubjectKey))
		{
			(*SubjectProcessors)->PreProcessors = PreProcessors;
		}
	}

	virtual void SetTranslator(const FLiveLinkSubjectKey& SubjectKey, ULiveLinkFrameTranslator* Translator) override
	{
		FWriteScopeLock Locker(SessionDataLock);
		if (TObjectPtr<ULiveLinkHubSubjectProcessors>* SubjectProcessors = SessionData->SubjectsConfig->SubjectProcessors.Find(SubjectKey))
		{
			(*SubjectProcessors)->Translator = Translator;
		}
	}

	void AddRestoredClient(FLiveLinkHubUEClientInfo& InOutRestoredClientInfo)
	{
		if (const TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkProvider())
		{
			LiveLinkProvider->AddRestoredClient(InOutRestoredClientInfo);

			{
				FWriteScopeLock Locker(SessionDataLock);
				CachedSessionClients.Add(InOutRestoredClientInfo.Id);
			}
		}

		OnClientAddedToSessionDelegate.Broadcast(InOutRestoredClientInfo.Id);
	}

private:
	/** Register livelink client delegates used to update the config's subject data. */
	void RegisterDelegates()
	{
		FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient.OnLiveLinkSubjectAdded().AddRaw(this, &FLiveLinkHubSession::OnSubjectAdded_AnyThread);
		LiveLinkClient.OnLiveLinkSubjectRemoved().AddRaw(this, &FLiveLinkHubSession::OnSubjectRemoved_AnyThread);
	}

	/** Unregister livelink client delegates. */
	void UnregisterDelegates()
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkClient.OnLiveLinkSubjectRemoved().RemoveAll(this);
			LiveLinkClient.OnLiveLinkSubjectAdded().RemoveAll(this);
		}
	}

	/** AnyThread handler for the SubjectAdded delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectAdded_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<FLiveLinkHubSession> WeakSession = AsShared();
		AsyncTask(ENamedThreads::GameThread, [WeakSession, Key = MoveTemp(SubjectKey)]
			{
				if (TSharedPtr<FLiveLinkHubSession> Session = WeakSession.Pin())
				{
					Session->OnSubjectAdded(Key);
				}
			});
	}

	/** Handles updating the tree view when a subject is added. */
	void OnSubjectAdded(const FLiveLinkSubjectKey& SubjectKey)
	{
		if (!SessionData->SubjectsConfig->SubjectProxies.Contains(SubjectKey))
		{
			ILiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

			FLiveLinkHubSubjectProxy SubjectSettings;
			SubjectSettings.Initialize(SubjectKey, LiveLinkClient.GetSourceType(SubjectKey.Source).ToString());

			{
				FWriteScopeLock Locker(SessionDataLock);
				SessionData->SubjectsConfig->SubjectProxies.FindOrAdd(SubjectKey) = MoveTemp(SubjectSettings);

				ULiveLinkHubSubjectProcessors* SubjectProcessors = NewObject<ULiveLinkHubSubjectProcessors>(SessionData->SubjectsConfig);
				SubjectProcessors->Initialize(Cast<ULiveLinkSubjectSettings>(LiveLinkClient.GetSubjectSettings(SubjectKey)));

				SessionData->SubjectsConfig->SubjectProcessors.FindOrAdd(SubjectKey) = SubjectProcessors;
			}
		}
	}

	/** AnyThread handler for the SubjectRemoved delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectRemoved_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<FLiveLinkHubSession> WeakSession = AsShared();
		AsyncTask(ENamedThreads::GameThread, [WeakSession, Key = MoveTemp(SubjectKey)]
			{
				if (TSharedPtr<FLiveLinkHubSession> Session = WeakSession.Pin())
				{
					Session->OnSubjectRemoved(Key);
				}
			});
	}

	/** Handles updating the tree view when a subject is removed. */
	void OnSubjectRemoved(const FLiveLinkSubjectKey& SubjectKey)
	{
		FWriteScopeLock Locker(SessionDataLock);
		SessionData->SubjectsConfig->SubjectProxies.Remove(SubjectKey);
		SessionData->SubjectsConfig->SubjectProcessors.Remove(SubjectKey);
	}

private:
	/** List of clients in the current session. These represent the unreal instances than can receive data from the hub. */
	TSet<FLiveLinkHubClientId> CachedSessionClients;

	/** Holds data for this session. */
	TStrongObjectPtr<ULiveLinkHubSessionData> SessionData;

	/** Delegate used to notice the hub about clients being added to this session. */
	FOnClientAddedToSession& OnClientAddedToSessionDelegate;

	/** Delegate used to notice the hub about clients being removed from this session. */
	FOnClientRemovedFromSession& OnClientRemovedFromSessionDelegate;

	/** Lock used to access the client config from different threads. */
	mutable FRWLock SessionDataLock;

	friend class FLiveLinkHubSessionManager;
};

#undef LOCTEXT_NAMESPACE
