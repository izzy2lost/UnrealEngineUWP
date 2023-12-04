// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "LiveLinkTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkHubSubjectSessionConfig.generated.h"

/** Config pertaining to livelink hub subjects for a given session. */
UCLASS()
class ULiveLinkHubSubjectSessionConfig : public UObject
{
public:
	GENERATED_BODY()

	ULiveLinkHubSubjectSessionConfig();
	virtual ~ULiveLinkHubSubjectSessionConfig() override;

	/** Get the config for a livelinkhub subject. */
	ULiveLinkHubSubjectProxy* GetSubjectConfig(const FLiveLinkSubjectKey& InSubject);

private:
	/** AnyThread handler for the SubjectAdded delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectAdded_AnyThread(FLiveLinkSubjectKey SubjectKey);
	/** Handles updating the tree view when a subject is added. */
	void OnSubjectAdded(const FLiveLinkSubjectKey& SubjectKey);
	/** AnyThread handler for the SubjectRemoved delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectRemoved_AnyThread(FLiveLinkSubjectKey SubjectKey);
	/** Handles updating the tree view when a subject is removed. */
	void OnSubjectRemoved(const FLiveLinkSubjectKey& SubjectKey);

private:
	/** Settings for subjects displayed in the livelink hub. */
	UPROPERTY(Instanced)
	TMap<FLiveLinkSubjectKey, TObjectPtr<ULiveLinkHubSubjectProxy>> SubjectProxies;
};


/** Holds information and config for a subject for the duration of a livelink hub session.  */
UCLASS()
class ULiveLinkHubSubjectProxy : public UObject
{
public:
	GENERATED_BODY()

	/** Initialize from a subject key. */
	void Initialize(const FLiveLinkSubjectKey& InSubjectKey, FString InSource);

	/** Get the outbound name for this subject, allows  */
	FName GetOutboundName() const;

	//~ Begin UObject interface
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
private:
	/** Notify clients that the old subject should be deleted and replaced with a new one with the updated name. */
	void NotifyRename();

private:
	/** Name of  the subject, */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink")
	FString SubjectName;

	/** Name override that will be transmitted to clients instead of the subject name. */
	UPROPERTY(EditAnywhere, Category = "LiveLink")
	FString OutboundName;

	/** Source that contains the subject. */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink")
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
};
