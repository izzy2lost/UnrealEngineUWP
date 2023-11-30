// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMenuBuilder;
class IConcertClient;
class IConcertClientSession;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamModel;
	class SHorizontalClientList;
}

namespace UE::MultiUserClient
{
	class FReplicationClient;
	class FReplicationClientManager;
	class FReassignObjectPropertiesLogic;

	/** Displayed in object rows of a client view for reassigning object ownership. */
	class SReassignObjectComboBox : public SCompoundWidget
	{
	public:

		/** Fills in the search terms based what the a widget instance would be displaying given its state. */
		static void PopulateSearchTerms(const IConcertClientSession& Session, const FReassignObjectPropertiesLogic& ReassignmentLogic, const FSoftObjectPath& ObjectPath, TArray<FString>& InOutSearchTerms);

		SLATE_BEGIN_ARGS(SReassignObjectComboBox)
		{}
			/** The object being reassigned by this widget. */
			SLATE_ARGUMENT(FSoftObjectPath, ManagedObject)
			SLATE_ARGUMENT(TSharedPtr<FText>, HighlightText)
			/** Used to to figure out child objects. */
			SLATE_ATTRIBUTE(ConcertClientSharedSlate::IReplicationStreamModel*, ConsolidatedModel)
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			TSharedRef<IConcertClient> InConcertClient,
			FReassignObjectPropertiesLogic& InReassignmentLogic,
			const FReplicationClientManager& InReplicationManager
		);
		virtual ~SReassignObjectComboBox() override;

	private:

		/** Used to get client display info */
		TSharedPtr<IConcertClient> ConcertClient;
		
		/** Does the logic of reassigning */
		FReassignObjectPropertiesLogic* ReassignmentLogic = nullptr;
		/** Used to get all clients and sort them. */
		const FReplicationClientManager* ReplicationManager = nullptr;

		/** The object being reassigned by this widget. */
		FSoftObjectPath ManagedObject;
		/** Used to to figure out child objects. */
		TAttribute<ConcertClientSharedSlate::IReplicationStreamModel*> ConsolidatedStreamModelAttribute;
		
		/** Displayed as content of combo button */
		TSharedPtr<ConcertClientSharedSlate::SHorizontalClientList> ComboClientList;
		/** Passed to client list for search highlighting */
		TSharedPtr<FText> HighlightText;

		void UpdateComboButtonContent() const;

		/** @return Whether the progress throbber should be visible */
		EVisibility GetThrobberVisibility() const;

		/** @return The menu to display for the combo box */
		TSharedRef<SWidget> MakeMenuContent() const;
		using FInlineAllocator = TInlineAllocator<24>;
		using FInlineObjectPathArray = TArray<FSoftObjectPath, FInlineAllocator>;
		/** Populates a menu which for reassigning a client to ObjectsToAssign. */
		void AddReassignSection(FMenuBuilder& MenuBuilder, const TArray<const FReplicationClient*>& SortedClients, TAttribute<FInlineObjectPathArray> ObjectsToAssign) const;

		/** @return All children of ManagedObject that need reassigning. */
		FInlineObjectPathArray GetChildrenOfManagedObject() const;
	};
}

