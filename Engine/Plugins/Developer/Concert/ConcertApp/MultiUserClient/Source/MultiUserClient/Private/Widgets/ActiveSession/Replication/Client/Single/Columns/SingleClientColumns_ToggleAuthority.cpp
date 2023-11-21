// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "SOwnerClientList.h"
#include "Replication/Authority/AuthorityChangeTracker.h"
#include "Replication/Authority/EAuthorityMutability.h"
#include "Replication/Authority/IClientAuthoritySynchronizer.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Replication/Submission/ISubmissionWorkflow.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ClientName/SClientName.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.ToggleAuthority"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName ToggleTopLevelAuthorityColumnId = TEXT("ToggleTopLevelAuthorityColumn");
	const FName ToggleSubobjectAuthorityColumnId = TEXT("ToggleSubobjectAuthorityColumn");

	namespace Private::ToggleTopLevelAuthority
	{
		static ECheckBoxState GetCheckboxState(
			const ConcertClientSharedSlate::FReplicatedObjectData& ObjectData,
			ConcertClientSharedSlate::IReplicationStreamModel* ClientStreamModel,
			FAuthorityChangeTracker* ChangeTracker
			)
		{
			const FSoftObjectPath& ObjectPath = ObjectData.GetObjectPath();

			// Init the checkbox state to the top level object, if the top level object has properties associated with it...
			const bool bCanSetTopLevelState = ChangeTracker->CanSetAuthorityFor(ObjectPath)
				|| ChangeTracker->GetChangeAuthorityMutability(ObjectPath) == EAuthorityMutability::NotSupported;
			TOptional<ECheckBoxState> CheckBoxState = bCanSetTopLevelState
				? ChangeTracker->GetAuthorityStateAfterApplied(ObjectPath) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked
				: TOptional<ECheckBoxState>{};

			// ... and then determine whether all the other subobjects share that state (Check / Unchecked) or not (Undetermined)
			ClientStreamModel->ForEachSubobject(ObjectPath, [&CheckBoxState, &ChangeTracker](const FSoftObjectPath& Child)
			{
				if (!ChangeTracker->CanSetAuthorityFor(Child)
					&& ChangeTracker->GetChangeAuthorityMutability(Child) != EAuthorityMutability::NotSupported)
				{
					return EBreakBehavior::Continue;
				}

				// Init checkbox state
				const bool bCurrentAuthorityState = ChangeTracker->GetAuthorityStateAfterApplied(Child);
				const bool bIsFirstObject = !CheckBoxState.IsSet(); 
				if (bIsFirstObject)
				{
					CheckBoxState = bCurrentAuthorityState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					return EBreakBehavior::Continue;
				}

				// Return the checkbox state that all subobjects share, or Undetermined if mixed
				const bool bMustBeUndetermined = (*CheckBoxState == ECheckBoxState::Unchecked && bCurrentAuthorityState)
					|| (*CheckBoxState == ECheckBoxState::Checked && !bCurrentAuthorityState);
				if (bMustBeUndetermined)
				{
					CheckBoxState = ECheckBoxState::Undetermined;
					return EBreakBehavior::Break;
				}
				return EBreakBehavior::Continue;
			});
						
			return CheckBoxState
				? *CheckBoxState
				// If this case happens, there are no objects to check. In that case, it visually "looks better" to be unchecked
				: ECheckBoxState::Unchecked;
		}

		static void OnCheckboxStateChanged(
			bool bIsChecked,
			const ConcertClientSharedSlate::FReplicatedObjectData& ObjectData,
			ConcertClientSharedSlate::IReplicationStreamModel* ClientStreamModel,
			FAuthorityChangeTracker* ChangeTracker
			)
		{
			const FSoftObjectPath& ObjectPath = ObjectData.GetObjectPath();
			TArray<FSoftObjectPath> Paths { ObjectPath };
			ClientStreamModel->ForEachSubobject(ObjectPath, [&Paths](const FSoftObjectPath& Child)
			{
				Paths.Add(Child);
				return EBreakBehavior::Continue;
			});
			
			ChangeTracker->SetAuthorityIfAllowed(Paths, bIsChecked);
		}

		static bool IsEnabled(
			const ConcertClientSharedSlate::FReplicatedObjectData& ObjectData,
			ConcertClientSharedSlate::IReplicationStreamModel* ClientStreamModel,
			FAuthorityChangeTracker* ChangeTracker
			)
		{
			const FSoftObjectPath& TopLevelObjectPath = ObjectData.GetObjectPath();
			const bool bCanChangeTopLevel = ChangeTracker->CanSetAuthorityFor(TopLevelObjectPath);
			if (bCanChangeTopLevel)
			{
				return true;
			}
						
			bool bCanChangeAnySubobject = false;
			ClientStreamModel->ForEachSubobject(TopLevelObjectPath, [&ChangeTracker, &bCanChangeAnySubobject](const FSoftObjectPath& Child)
			{
				bCanChangeAnySubobject |= ChangeTracker->CanSetAuthorityFor(Child);
				return bCanChangeAnySubobject ? EBreakBehavior::Break : EBreakBehavior::Continue;
			});
			return bCanChangeAnySubobject;
		}
	}
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ToggleTopLevelAuthority(
		ConcertClientSharedSlate::IReplicationStreamModel& ClientStreamModel,
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		)
	{
		using namespace ConcertClientSharedSlate;
		using namespace Private::ToggleTopLevelAuthority;
		using FTopLevelColumnDelegates = TReplicationColumnDelegates<FReplicatedObjectData>;
		
		return MakeCheckboxColumn<FReplicatedObjectData>(
			ToggleTopLevelAuthorityColumnId,
				FTopLevelColumnDelegates(
					FTopLevelColumnDelegates::FGetColumnCheckboxState::CreateStatic(&GetCheckboxState, &ClientStreamModel, &ChangeTracker),
					FTopLevelColumnDelegates::FOnColumnCheckboxChanged::CreateStatic(&OnCheckboxStateChanged, &ClientStreamModel, &ChangeTracker),
					FTopLevelColumnDelegates::FGetToolTipText::CreateLambda([&ClientStreamModel, &ChangeTracker, &SubmissionWorkflow](const FReplicatedObjectData& ObjectData)
					{
						
						if (SubmissionWorkflow.GetUploadability() == EChangeUploadability::NotImplemented)
						{
							const ECheckBoxState CheckBoxState = GetCheckboxState(ObjectData, &ClientStreamModel, &ChangeTracker);
							const FText CheckBoxText = [CheckBoxState]()
							{
								switch (CheckBoxState)
								{
								case ECheckBoxState::Unchecked: return LOCTEXT("TopLevel.ChangeAuthority.ToolTip.NotSupported.Unchecked", "None of the subobjects are being replicated.");
								case ECheckBoxState::Checked: return LOCTEXT("TopLevel.ChangeAuthority.ToolTip.NotSupported.Checked", "All of the subobjects are being replicated.");
								case ECheckBoxState::Undetermined: return LOCTEXT("TopLevel.ChangeAuthority.ToolTip.NotSupported.Undetermined", "Some of the subobjects are being replicated.");
								default: checkNoEntry(); return FText::GetEmpty();
								}
							}();
							
							return FText::Format(
								LOCTEXT("TopLevel.ChangeAuthority.ToolTip.NotSupportedFmt", "{0}\nEditing remote clients is not implemented. You can only edit the local client."),
								CheckBoxText
								);
						}
						
						return IsEnabled(ObjectData, &ClientStreamModel, &ChangeTracker)
							? LOCTEXT("TopLevel.ChangeAuthority.Allowed", "Toggles whether this object and its subobjects will replicate the assigned properties.")
							: LOCTEXT("TopLevel.ChangeAuthority.Disallowed", "Toggles whether this object and its subobjects will replicate the assigned properties.\nDisabled because none of the subobjects can be replicated with their current configuration.");
					}),
					FTopLevelColumnDelegates::FIsEnabled::CreateStatic(&IsEnabled, &ClientStreamModel, &ChangeTracker)
				),
			FText::GetEmpty(),
			static_cast<int32>(ETopLevelObjectColumnOrder::ToggleAuthority)
		);
	}

	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ToggleObjectAuthority(
		FAuthorityChangeTracker& ChangeTracker,
		ISubmissionWorkflow& SubmissionWorkflow
		)
	{
		using namespace ConcertClientSharedSlate;
		using FSubobjectColumnDelegates = TReplicationColumnDelegates<FReplicatedObjectData>;
		return MakeCheckboxColumn<FReplicatedObjectData>(
			ToggleTopLevelAuthorityColumnId,
				FSubobjectColumnDelegates(
					FSubobjectColumnDelegates::FGetColumnCheckboxState::CreateLambda(
					[&ChangeTracker](const FReplicatedObjectData& ObjectData)
					{
						const bool bHasAuthority = ChangeTracker.GetAuthorityStateAfterApplied(ObjectData.GetObjectPath());
						return bHasAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					}),
					FSubobjectColumnDelegates::FOnColumnCheckboxChanged::CreateLambda(
					[&ChangeTracker](bool bIsChecked, const FReplicatedObjectData& ObjectData)
					{
						ChangeTracker.SetAuthorityIfAllowed({ ObjectData.GetObjectPath() }, bIsChecked);
					}),
					FSubobjectColumnDelegates::FGetToolTipText::CreateLambda([&ChangeTracker, &SubmissionWorkflow](const FReplicatedObjectData& ObjectData)
					{
						const bool bHasAuthority = ChangeTracker.GetAuthorityStateAfterApplied(ObjectData.GetObjectPath());
						const FText NotSupportedText = FText::Format(
							LOCTEXT("Subobject.ChangeAuthority.ToolTip.NotSupportedFmt", "{0}\nEditing remote clients is not implemented. You can only edit the local client."),
							bHasAuthority ? LOCTEXT("Subobject.ChangeAuthority.Replicating.True", "This object is being replicated.") : LOCTEXT("Subobject.ChangeAuthority.Replicating.False", "This object is not currently being replicated.")
							);
						if (SubmissionWorkflow.GetUploadability() == EChangeUploadability::NotImplemented)
						{
							return NotSupportedText;
						}
						
						switch (ChangeTracker.GetChangeAuthorityMutability(ObjectData.GetObjectPath()))
						{
						case EAuthorityMutability::Allowed: return LOCTEXT("Subobject.ChangeAuthority.ToolTip.Allowed", "Toggles whether this object should be replicated.");
						case EAuthorityMutability::NoProperties: return LOCTEXT("Subobject.ChangeAuthority.ToolTip.NoProperties", "Toggles whether this object should be replicated.\nAssign properties to this object first.\n");
						case EAuthorityMutability::NotSupported: return NotSupportedText;
						default: checkNoEntry(); return FText::GetEmpty();
						}
					}),
					FSubobjectColumnDelegates::FIsEnabled::CreateLambda([&ChangeTracker](const FReplicatedObjectData& ObjectData)
					{
						return ChangeTracker.CanSetAuthorityFor(ObjectData.GetObjectPath());
					})
				),
			FText::GetEmpty(),
			static_cast<int32>(ETopLevelObjectColumnOrder::ToggleAuthority)
		);
	}
}

#undef LOCTEXT_NAMESPACE