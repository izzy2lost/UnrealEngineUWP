// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	class IReplicationStreamViewer;
	class IObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FAuthorityChangeTracker;
	class FGlobalAuthorityCache;
	class IClientAuthoritySynchronizer;
	class ISubmissionWorkflow;
}

namespace UE::MultiUserClient::SingleClientColumns
{
	/** @see UE::ConcertClientSharedSlate::ReplicationColumns::Subobject::ETopLevelColumnOrder */
	enum class ETopLevelObjectColumnOrder
	{
		ToggleAuthority = 0,
		Owner = 40
	};
	/** @see UE::ConcertClientSharedSlate::ReplicationColumns::Subobject::ESubobjectColumnOrder */
	enum class ESubobjectColumnOrder
	{
		ToggleAuthority = 0,
		ConflictWarning = 5,
		Owner = 20
	};
	/** @see UE::ConcertClientSharedSlate::ReplicationColumns::Property::EReplicationPropertyColumnOrder */
	enum class EPropertyColumnOrder
	{
		ConflictWarning = 5,
		Owner = 50
	};


	/********** Toggle authority **********/
	extern const FName ToggleTopLevelAuthorityColumnId;
	extern const FName ToggleSubobjectAuthorityColumnId;

	/**
	 * Checkbox placed in the top-level view.
	 * It gives / removes authority for the top-level object and all of its subobjects.
	 *
	 * @param ClientStreamModel Used to discover the client's registered subobjects
	 * @param ChangeTracker Used to determine the checkbox state and change authority.
	 * @param SubmissionWorkflow Used to determine whether changing authority is at all enabled
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ToggleTopLevelAuthority(
		ConcertClientSharedSlate::IObjectToPropertiesModel& ClientStreamModel,
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		);

	/**
	 * Checkbox placed in the subobject view
	 * It gives / removes authority for the object it is placed next to.
	 * 
	 * @param AuthoritySynchronizer Determines whether the client has authority or not.
	 * @param ChangeTracker Used to determine the checkbox state and change authority.
	 * @param AuthoritySynchronizer Used to determine whether changing authority is at all enabled
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn ToggleSubobjectAuthority(
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		);

	
	/********** Owner **********/
	extern const FName OwnerOfSubobjectColumnId;
	extern const FName OwnerOfPropertyColumnId;
	
	/**
	 * Displays the owner of a top-level object.
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @param InObjectModel Used to get subobjects for a top-level object
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn OwnerOfTopLevelObject(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		ConcertClientSharedSlate::IObjectToPropertiesModel& InObjectModel
		);

	/**
	 * Displays the owner of a subobject.
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn OwnerOfSubobject(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache
		);
	
	/**
	 * Displays the owner of a property.
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @param InViewerAttribute Used to determine which objects the property box is displaying
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn OwnerOfProperty(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const TAttribute<const ConcertClientSharedSlate::IReplicationStreamViewer*>& InViewerAttribute
		);

	
	/********** Conflict warning **********/
	extern const FName ConflictWarningTopLevelObjectColumnId;
	extern const FName ConflictWarningSubobjectColumnId;
	extern const FName ConflictWarningPropertyColumnId;
	
	/**
	 * Displays a warning symbol next to the checkbox if checking the checkbox would cause an authority conflict when submitted.
	 * 
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InAuthorityCache Used to determine which client owns the object
	 * @param ClientId ID of the client for which the icon is being created
	 * 
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn ConflictWarningForSubobject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		);

	/**
	 * Displays a warning symbol next to the checkbox if checking the checkbox would cause an authority conflict when submitted.
	 * 
	 * @param InClient The local Concert client used to look up other client display info
	 * @param InViewer Used to determine which objects the property box is displaying
	 * @param InAuthorityCache Used to determine which client owns the property
	 * @param ClientId ID of the client for which to check whether the property can be added to the stream
	 * 
	 * @return Column that can be placed in the table
	 */
	ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn ConflictWarningForProperty(
		TSharedRef<IConcertClient> InClient,
		TAttribute<const ConcertClientSharedSlate::IReplicationStreamViewer*> InViewer,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		);
}
