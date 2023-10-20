// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertClientSharedSlate
{
	class IObjectToPropertiesModel;
}

namespace UE::MultiUserClient
{
	class FAuthorityChangeTracker;
	class IClientAuthoritySynchronizer;
	class ISubmissionWorkflow;
}

namespace UE::MultiUserClient::StreamEditorColumns
{
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
}
