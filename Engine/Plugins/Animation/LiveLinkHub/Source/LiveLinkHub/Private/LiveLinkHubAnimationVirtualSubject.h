// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkAnimationVirtualSubject.h"

#include "Clients/LiveLinkHubProvider.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubModule.h"

#include "LiveLinkHubAnimationVirtualSubject.generated.h"

/**
 * Animation virtual subject used in LiveLinkHub.
 * Shows options for the subject and broadcasts static data when the skeleton is updated.
 */
UCLASS()
class ULiveLinkHubAnimationVirtualSubject : public ULiveLinkAnimationVirtualSubject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, ILiveLinkClient* InLiveLinkClient) override
	{
		ULiveLinkAnimationVirtualSubject::Initialize(InSubjectKey, InRole, InLiveLinkClient);
		SubjectName = InSubjectKey.SubjectName.ToString();

		FLiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(FLiveLinkClient::ModularFeatureName);
		Source = Client->GetSourceType(InSubjectKey.Source).ToString();
	}

	/** Whether this subject is rebroadcasted */
	virtual bool IsRebroadcasted() const
	{
		// todo: Decide this based on the session? Provider should decide how to handle it .
		return true;
	}

	virtual void PostSkeletonRebuild() override
	{
		if (HasValidStaticData())
		{
			if (const TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkProvider())
			{
				// Update the static data since the final skeleton was changed.
				const FLiveLinkSubjectFrameData& CurrentSnapshot = GetFrameSnapshot();

				FLiveLinkStaticDataStruct StaticDataCopy;
				StaticDataCopy.InitializeWith(CurrentSnapshot.StaticData);

				LiveLinkProvider->UpdateSubjectStaticData(SubjectKey.SubjectName, Role, MoveTemp(StaticDataCopy));
			}
		}
	}

public:
	/** Name of the virtual subject. */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink")
	FString SubjectName;

	/** Source that contains the subject. */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink")
	FString Source;
};
