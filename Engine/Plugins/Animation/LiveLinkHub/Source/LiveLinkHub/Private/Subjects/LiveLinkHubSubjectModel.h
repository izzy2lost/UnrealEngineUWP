// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubModule.h"
#include "LiveLinkHubSubjectSessionConfig.h"
#include "LiveLinkTypes.h"
#include "Session/LiveLinkHubSession.h"
#include "Session/LiveLinkHubSessionManager.h"


/** Subject Model used by the view to access the subject settings. */
class ILiveLinkHubSubjectModel
{
public:
	virtual ~ILiveLinkHubSubjectModel() = default;

	/** Get the settings for a livelinkhub subject. */
	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const = 0;

	/** Get the preprocessors and translator for a livelinkhub subject. */
	virtual ULiveLinkHubSubjectProcessors* GetSubjectProcessors(const FLiveLinkSubjectKey& InSubject) const = 0;
};

/** Implementation of the ILiveLinkHubSubjectModel. */
class FLiveLinkHubSubjectModel : public ILiveLinkHubSubjectModel
{
public:
	/** Get the settings for a livelinkhub subject. */
	virtual TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const override
	{
		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		if (const TSharedPtr<ILiveLinkHubSession> Session = LiveLinkHubModule.GetSessionManager()->GetCurrentSession())
		{
			return Session->GetSubjectConfig(InSubject);
		}
		return {};
	}

	/** Get the settings for a livelinkhub subject. */
	virtual ULiveLinkHubSubjectProcessors* GetSubjectProcessors(const FLiveLinkSubjectKey& InSubject) const override
	{
		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		if (const TSharedPtr<ILiveLinkHubSession> Session = LiveLinkHubModule.GetSessionManager()->GetCurrentSession())
		{
			return Session->GetSubjectProcessors(InSubject);
		}
		return nullptr;
	}
};
