// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "IStructureDetailsView.h"
#include "IPropertyRowGenerator.h"

#include "Delegates/DelegateCombinations.h"
#include "DetailsViewArgs.h"
#include "Features/IModularFeatures.h"
#include "IDetailsView.h"
#include "ISinglePropertyView.h"
#include "LiveLinkHubSubjectModel.h"
#include "LiveLinkSubjectSettings.h"
#include "LiveLinkTypes.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"

DECLARE_DELEGATE_TwoParams(FOnRenameLiveLinkHubSubject, const FLiveLinkSubjectKey& /*SubjectKey*/, FName /*NewName*/);
DECLARE_DELEGATE_ThreeParams(FOnSubjectProcessorModified, const FLiveLinkSubjectKey& /*SubjectKey*/,  const TArray<TSubclassOf<ULiveLinkFramePreProcessor>>& /*UpdatedPreProcessors*/, TSubclassOf<ULiveLinkFrameTranslator> /*UpdatedTranslator*/);

/**
 * Provides the UI that displays information about a livelink hub subject.
 */
class SLiveLinkHubSubjectView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLiveLinkHubSubjectView) {}
	SLATE_ARGUMENT(FLiveLinkSubjectKey, SubjectKey)
	SLATE_EVENT(FOnRenameLiveLinkHubSubject, OnRenameSubject)
	SLATE_EVENT(FOnSubjectProcessorModified, OnProcessorModified)
	SLATE_END_ARGS()

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs, const TSharedRef<ILiveLinkHubSubjectModel>& InSubjectModel)
	{
		SubjectModel = InSubjectModel;
		OnRenameSubjectDelegate = InArgs._OnRenameSubject;
		OnProcessorModifiedDelegate = InArgs._OnProcessorModified;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = NAME_None;
		DetailsViewArgs.bShowCustomFilterOption = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = false;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FStructureDetailsViewArgs StructureDetailsArgs;

		SubjectData = MakeShared<TStructOnScope<FLiveLinkHubSubjectProxy>>();

		SettingsDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsArgs, SubjectData);
		SettingsDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SLiveLinkHubSubjectView::OnSubjectPropertyModified);

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SettingsDetailsView->GetWidget().ToSharedRef()
			]
		];
	}

	/** Clear the subject details. */
	void RefreshSubjectDetails(const TSharedRef<ILiveLinkHubSession>& ActiveSession)
	{
		SubjectData->Reset();
		SettingsDetailsView->SetStructureData(nullptr);

		SetSubject(SubjectKey);
	}

	/** Set the subject to be displayed in the details view. */
	void SetSubject(const FLiveLinkSubjectKey& InSubjectKey)
	{
		SubjectKey = InSubjectKey;

		if (TOptional<FLiveLinkHubSubjectProxy> SubjectProxy = SubjectModel->GetSubjectConfig(InSubjectKey))
		{
			const FLiveLinkHubSubjectProxy& Proxy = *SubjectProxy;
			SubjectData->InitializeAs<FLiveLinkHubSubjectProxy>(*SubjectProxy);
			SettingsDetailsView->SetStructureData(SubjectData);
		}
	}

	/** Get the outbound name for a subject. */
	FString GetSubjectOutboundName(const FLiveLinkSubjectKey& InSubjectKey) const
	{
		if (TOptional<FLiveLinkHubSubjectProxy> SubjectProxy = SubjectModel->GetSubjectConfig(InSubjectKey))
		{
			return SubjectProxy->GetOutboundName().ToString();
		}
		return InSubjectKey.SubjectName.ToString();
	}

	/** Handler called when a subject property is modified, used to trigger a rename on the session's subject config. */
	void OnSubjectPropertyModified(const FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (SubjectData)
		{
			if (FLiveLinkHubSubjectProxy* Proxy = SubjectData->Get())
			{
				if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FLiveLinkHubSubjectProxy, OutboundName))
				{
					OnRenameSubjectDelegate.ExecuteIfBound(SubjectKey, Proxy->GetOutboundName());
				}
				else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FLiveLinkHubSubjectProxy, PreProcessors)
					|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FLiveLinkHubSubjectProxy, Translator))
				{
					OnProcessorModifiedDelegate.ExecuteIfBound(SubjectKey, Proxy->PreProcessors, Proxy->Translator);
				}
			}
		}
	}

private:
	/** Details for the selected subject. */
	TSharedPtr<IStructureDetailsView> SettingsDetailsView;
	/** Subject being shown. */
	FLiveLinkSubjectKey SubjectKey;
	/** Model that holds the data for a given subject. */
	TSharedPtr<ILiveLinkHubSubjectModel> SubjectModel;
	/** Struct on scope used to display subject data in the structure details view. */
	TSharedPtr<TStructOnScope<FLiveLinkHubSubjectProxy>> SubjectData;
	/** Delegate called when the outbound name is changed by the user. */
	FOnRenameLiveLinkHubSubject OnRenameSubjectDelegate;
	/** Delegate called when a translator or preprocessor is modified by the user. */
	FOnSubjectProcessorModified OnProcessorModifiedDelegate;
};
