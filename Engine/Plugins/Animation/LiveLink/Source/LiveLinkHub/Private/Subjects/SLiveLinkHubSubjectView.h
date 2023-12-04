// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "IDetailsView.h"
#include "LiveLinkHubSubjectModel.h"
#include "LiveLinkTypes.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

/**
 * Provides the UI that displays information about a livelink hub subject.
 */
class SLiveLinkHubSubjectView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubSubjectView) {}
	SLATE_END_ARGS()

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs, const TSharedRef<ILiveLinkHubSubjectModel>& InSubjectModel)
	{
		SubjectModel = InSubjectModel;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = NAME_None;
		DetailsViewArgs.bShowCustomFilterOption = false;
		DetailsViewArgs.bShowOptions = false;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		ChildSlot
		[
			SettingsDetailsView.ToSharedRef()
		];
	}

	/** Set the subject to be displayed in the details view. */
	void SetSubject(const FLiveLinkSubjectKey& InSubjectKey)
	{
		SubjectKey = InSubjectKey;
		SettingsDetailsView->SetObject(SubjectModel->GetSubjectConfig(SubjectKey));
	}

	/** Get the outbound name for a subject. */
	FString GetSubjectOutboundName(const FLiveLinkSubjectKey& InSubjectKey)
	{
		if (ULiveLinkHubSubjectProxy* SubjectProxy = SubjectModel->GetSubjectConfig(InSubjectKey))
		{
			return SubjectProxy->GetOutboundName().ToString();
		}
		return InSubjectKey.SubjectName.ToString();
	}

private:
	/** Details for the selected subject. */
	TSharedPtr<IDetailsView> SettingsDetailsView;
	/** Subject being shown. */
	FLiveLinkSubjectKey SubjectKey;
	/** Model that holds the data for a given subject. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
};
