// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/Transform.h"
#include "LiveLinkFramePreProcessor.h"
#include "LiveLinkFrameTranslator.h"
#include "LiveLinkSubjectSettings.h"
#include "Delegates/Delegate.h"
#include "LiveLinkTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkHubSubjectSessionConfig.generated.h"

/** Holds information and config for a subject for the duration of a livelink hub session.  */
USTRUCT()
struct FLiveLinkHubSubjectProxy
{
public:
	GENERATED_BODY()

	/** Initialize from a subject key. */
	void Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource);

	/** Get the outbound name for this subject, allows  */
	FName GetOutboundName() const;

	/** Change the outbound name for this subject proxy. */
	void SetOutboundName(FName NewName);

private:
	/** Notify clients that the old subject should be deleted and replaced with a new one with the updated name. */
	void NotifyRename();

private:
	/** Name of  the subject, */
	UPROPERTY(VisibleAnywhere, Category = "Subject Details")
	FString SubjectName;

	/** Name override that will be transmitted to clients instead of the subject name. */
	UPROPERTY(EditAnywhere, Category = "Subject Details")
	FString OutboundName;

	/** Source that contains the subject. */
	UPROPERTY(VisibleAnywhere, Category = "Subject Details")
	FString Source;

	/** SubjectKey for this subject, */
	UPROPERTY()
	FLiveLinkSubjectKey SubjectKey;

	/**
	 * If this is set, then the outbound name is currently undergoing a rename,
	 * If this is not set, then the OutboundName is committed.
	 */
	bool bPendingOutboundNameChange = false;

	/* Previous outbound name to be used for noticing clients to remove this entry from their subject list. */
	FName PreviousOutboundName;

	friend class SLiveLinkHubSubjectView;
};

UCLASS()
class ULiveLinkHubSubjectProcessors : public UObject
{
public:
	GENERATED_BODY()

	void Initialize(ULiveLinkSubjectSettings* InSubjectSettings);

	/** Preprocessors to apply to the livelink data coming through the hub. */
	UPROPERTY(EditAnywhere, Category="Processors", Instanced)
	TArray<TObjectPtr<ULiveLinkFramePreProcessor>> PreProcessors;

	/** Translators used to convert livelink data to a different role. */
	UPROPERTY(EditAnywhere, Category="Processors", Instanced)
	TObjectPtr<ULiveLinkFrameTranslator> Translator;
};

/** Config pertaining to livelink hub subjects for a given session. */
UCLASS()
class ULiveLinkHubSubjectSessionConfig : public UObject
{
public:
	GENERATED_BODY()

	void Initialize();

	/** Get the config for a livelinkhub subject. */
	TOptional<FLiveLinkHubSubjectProxy> GetSubjectConfig(const FLiveLinkSubjectKey& InSubject) const;

	/** Get the subject processors. */
	ULiveLinkHubSubjectProcessors* GetSubjectProcessors(const FLiveLinkSubjectKey& InSubject);

	/** Change the outbound name of a subject for the current session. */
	void RenameSubject(const FLiveLinkSubjectKey& SubjectKey, FName NewName);

private:
	/** Settings for subjects displayed in the livelink hub. */
	UPROPERTY(Transient)
	TMap<FLiveLinkSubjectKey, FLiveLinkHubSubjectProxy> SubjectProxies;

	/** 
	 * Processor settings held in a UObject to allow creating and editing them inline. 
	 * Note that the other parts of the session config are not held in a UObject since they can be accessed at any time on other threads.
	 */
	UPROPERTY(Instanced)
	TMap<FLiveLinkSubjectKey, TObjectPtr<ULiveLinkHubSubjectProcessors>> SubjectProcessors;

	friend class FLiveLinkHubSession;
};
