// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkTypes.h"
#include "Subjects/LiveLinkHubSubjectSessionConfig.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Note: This is just a stub implementation until the session manager work is submitted.
 */
class ILiveLinkHubSession
{
public:
	virtual ~ILiveLinkHubSession() = default;
	virtual ULiveLinkHubSubjectProxy* GetSubjectConfig(const FLiveLinkSubjectKey& SubjectKey) const = 0;
};

class FLiveLinkHubSession : public ILiveLinkHubSession
{
public:
	FLiveLinkHubSession()
	{
		SessionConfig = TStrongObjectPtr<ULiveLinkHubSubjectSessionConfig>(NewObject<ULiveLinkHubSubjectSessionConfig>());
	}

	virtual ~FLiveLinkHubSession() override = default;

	virtual ULiveLinkHubSubjectProxy* GetSubjectConfig(const FLiveLinkSubjectKey& SubjectKey) const override
	{
		return SessionConfig->GetSubjectConfig(SubjectKey);
	}

private:
	TStrongObjectPtr<ULiveLinkHubSubjectSessionConfig> SessionConfig;
};

class ILiveLinkHubSessionManager
{
public:
	virtual ~ILiveLinkHubSessionManager() = default;

	// Todo: Real implementation must be thread safe
	virtual TSharedPtr<ILiveLinkHubSession> GetCurrentSession() const = 0;
};

class FLiveLinkHubSessionManager : public ILiveLinkHubSessionManager
{
public:
	FLiveLinkHubSessionManager()
	{
		CurrentSession = MakeShared<FLiveLinkHubSession>();
	}

	virtual ~FLiveLinkHubSessionManager() override = default;

	virtual TSharedPtr<ILiveLinkHubSession> GetCurrentSession() const override
	{
		return CurrentSession;
	}

private:
	TSharedPtr<ILiveLinkHubSession> CurrentSession;
};
