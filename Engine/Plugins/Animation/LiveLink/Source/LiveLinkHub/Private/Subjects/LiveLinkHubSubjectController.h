// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Clients/LiveLinkHubProvider.h"
#include "LiveLinkTypes.h"
#include "SLiveLinkHubSubjectView.h"

/** Controller responsible for handling the hub's subjects and creating the subject view. */
class FLiveLinkHubSubjectController
{
public:
	FLiveLinkHubSubjectController()
	{
		SubjectModel = MakeShared<FLiveLinkHubSubjectModel>();
	}

	/** Create the widget for displaying a subject's settings. */
	TSharedRef<SWidget> MakeSubjectView()
	{
		return SAssignNew(SubjectsView, SLiveLinkHubSubjectView, SubjectModel.ToSharedRef());
	}

	/** Set the displayed subject in the subject view. */
	void SetSubject(const FLiveLinkSubjectKey& Subject)
	{
		SubjectsView->SetSubject(Subject);
	}

private:
	/** View widget for the selected subject. */
	TSharedPtr<SLiveLinkHubSubjectView> SubjectsView;
	/** Model responsible for the subjects data. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
};
