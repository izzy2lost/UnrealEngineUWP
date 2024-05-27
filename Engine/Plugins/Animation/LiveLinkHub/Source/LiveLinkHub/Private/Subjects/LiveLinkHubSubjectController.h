// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHub.h"
#include "LiveLinkHubModule.h"
#include "LiveLinkTypes.h"
#include "Modules/ModuleManager.h"
#include "Session/LiveLinkHubSession.h"
#include "Session/LiveLinkHubSessionManager.h"
#include "SLiveLinkHubSubjectView.h"

/** Controller responsible for handling the hub's subjects and creating the subject view. */
class FLiveLinkHubSubjectController
{
public:
	FLiveLinkHubSubjectController()
	{
		SubjectModel = MakeShared<FLiveLinkHubSubjectModel>();

		const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
		LiveLinkHubModule.GetSessionManager()->OnActiveSessionChanged().AddRaw(this, &FLiveLinkHubSubjectController::OnActiveSessionChanged);
	}

	~FLiveLinkHubSubjectController()
	{
		if (const FLiveLinkHubModule* LiveLinkHubModule = FModuleManager::Get().GetModulePtr<FLiveLinkHubModule>("LiveLinkHub"))
		{
			if (const TSharedPtr<ILiveLinkHubSessionManager> SessionManager = LiveLinkHubModule->GetSessionManager())
			{
				SessionManager->OnActiveSessionChanged().RemoveAll(this);
			}
		}
	}

	/** Create the widget for displaying a subject's settings. */
	TSharedRef<SWidget> MakeSubjectView()
	{
		return SAssignNew(SubjectsView, SLiveLinkHubSubjectView, SubjectModel.ToSharedRef())
			.OnRenameSubject_Raw(this, &FLiveLinkHubSubjectController::OnSubjectRenamed)
			.OnProcessorModified_Raw(this, &FLiveLinkHubSubjectController::OnSubjectProcessorModified);
	}

	/** Set the displayed subject in the subject view. */
	void SetSubject(const FLiveLinkSubjectKey& Subject) const
	{
		SubjectsView->SetSubject(Subject);
	}

	/** Handle modifying the session config for the specified subject. */
	void OnSubjectRenamed(const FLiveLinkSubjectKey& SubjectKey, FName NewName) const
	{
		if (TSharedPtr<ILiveLinkHubSessionManager> SessionManager = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub()->GetSessionManager())
		{
			SessionManager->GetCurrentSession()->RenameSubject(SubjectKey, NewName);
		}
	}

	void OnSubjectProcessorModified(const FLiveLinkSubjectKey& SubjectKey, const TArray<ULiveLinkFramePreProcessor*>& UpdatedPreprocessors, ULiveLinkFrameTranslator* UpdatedTranslator) const
	{
		FLiveLinkHubClient& LiveLinkClient = static_cast<FLiveLinkHubClient&>(IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName));
		ULiveLinkSubjectSettings* Settings = Cast<ULiveLinkSubjectSettings>(LiveLinkClient.GetSubjectSettings(SubjectKey));

		Settings->PreProcessors = UpdatedPreprocessors;
		Settings->Translators.Reset();

		if (UpdatedTranslator)
		{
			Settings->Translators.Add(UpdatedTranslator);
		}

		// todo: If we are removing the translator, we will need to do additional handling to restore the original static data that was overriden
		if (Settings->ValidateProcessors())
		{
			// Apply to the underlying data from the session.
			if (const TSharedPtr<ILiveLinkHubSessionManager> SessionManager = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub()->GetSessionManager())
			{
				SessionManager->GetCurrentSession()->SetPreProcessors(SubjectKey, Settings->PreProcessors);

				if (Settings->Translators.Num())
				{
					SessionManager->GetCurrentSession()->SetTranslator(SubjectKey, Settings->Translators[0]);
				}
				else
				{
					SessionManager->GetCurrentSession()->SetTranslator(SubjectKey, nullptr);
				}
			}

			// Apply to the livelink client
			LiveLinkClient.CacheSubjectSettings(SubjectKey, Settings);
		}
		else
		{
			// Refresh the view since the underlying data might have been rolled back.
			SubjectsView->SetSubject(SubjectKey);
		}
	}

	/** Handle updating the subject details when the session has been swapped out for a different one. */
	void OnActiveSessionChanged(const TSharedRef<ILiveLinkHubSession>& ActiveSession) const
	{
		if (SubjectsView)
		{
			SubjectsView->RefreshSubjectDetails(ActiveSession);
		}
	}

private:
	/** View widget for the selected subject. */
	TSharedPtr<SLiveLinkHubSubjectView> SubjectsView;
	/** Model responsible for the subjects data. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
};
