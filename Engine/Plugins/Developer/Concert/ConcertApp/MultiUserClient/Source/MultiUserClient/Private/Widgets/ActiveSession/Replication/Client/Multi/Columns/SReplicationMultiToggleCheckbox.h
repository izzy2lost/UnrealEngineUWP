// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
enum class ECheckBoxState : uint8;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamModel;
}

namespace UE::MultiUserClient
{
	class FReplicationClientManager;
}

namespace UE::MultiUserClient
{
	/**
	 * Checkbox in a combo button.
	 * 
	 * The checkbox can either the object it was created for or all of its child objects.
	 * If the checkbox in the combo button is pressed it defaults to toggling just the object.
	 *
	 * TODO UE-200487: Make sure this widget updates when a remote client changes their setting for being editable
	 */
	class SReplicationMultiToggleCheckbox : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationMultiToggleCheckbox)
		{}
		    SLATE_ARGUMENT(FSoftObjectPath, Object)
			SLATE_ATTRIBUTE(ConcertSharedSlate::IReplicationStreamModel*, ConsolidatedStreamModelAttribute)
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
		    FReplicationClientManager& InClientManager,
		    TSharedRef<IConcertClient> InConcertClient
		);

	private:

		/** The object path this checkbox was created for */
		FSoftObjectPath Object;

		/** Used to access all clients for toggling authority. */
		FReplicationClientManager* ClientManager = nullptr;
		/** Used to get children of Object */
		TAttribute<ConcertSharedSlate::IReplicationStreamModel*> ConsolidatedStreamModelAttribute;
		
		/** Used to look up client display names in case of conflicts. */
		TSharedPtr<IConcertClient> ConcertClient;

		/** @return Tooltip for the combo box. Informs user what the end result is if enabled or why it's disabled. */
		FText GetRootToolTipText() const;
		
		/** @return Goes through all clients that can be edited and returns whether all of them have authority. */
		ECheckBoxState GetCheckboxStateForObject(FSoftObjectPath InObject) const;
		/** @return Whether there are any clients for which the authority can be changed. */
		bool IsCheckboxEnabledForObject(FSoftObjectPath InObject) const;
		/** Gives all editable clients which have something registered to Object authority or takes it away. */
		void OnCheckboxStateChangedForObject(ECheckBoxState NewState, FSoftObjectPath InObject) const;

		/** @return Menu widget that with the options to remove or give authority to this and other objects. */
		TSharedRef<SWidget> GetDropDownMenuContent();
		
		void ToggleChildren() const;
		bool CanToggleChildren() const;

		void ToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const;
		bool CanToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const;
		
		EVisibility GetWarningVisibility() const;
		FText GetWarningToolTipText() const;
		TArray<FGuid> GetNonEditableClients() const;
	};
}

