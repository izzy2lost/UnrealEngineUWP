// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationMultiToggleCheckbox.h"

#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#include "Algo/AnyOf.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "SReplicationMultiToggleCheckbox"

namespace UE::MultiUserClient
{
	void SReplicationMultiToggleCheckbox::Construct(
		const FArguments& InArgs,
		FReplicationClientManager& InClientManager,
		TSharedRef<IConcertClient> InConcertClient
		)
	{
		Object = InArgs._Object;
		ConsolidatedStreamModelAttribute = InArgs._ConsolidatedStreamModelAttribute;
		check(ConsolidatedStreamModelAttribute.IsSet() || ConsolidatedStreamModelAttribute.IsBound());
		
		ClientManager = &InClientManager;
		ConcertClient = MoveTemp(InConcertClient);
		
		ChildSlot
		[
			SNew(SComboButton)
			.HasDownArrow(true)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.ForegroundColor(FSlateColor::UseStyle())
			.ContentPadding(FMargin(2.0f, 2.0f))
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				.ToolTipText(this, &SReplicationMultiToggleCheckbox::GetRootToolTipText)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SReplicationMultiToggleCheckbox::GetCheckboxStateForObject, Object)
					.IsEnabled(this, &SReplicationMultiToggleCheckbox::IsCheckboxEnabledForObject, Object)
					.OnCheckStateChanged(this, &SReplicationMultiToggleCheckbox::OnCheckboxStateChangedForObject, Object)
				]
			
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					.Visibility(this, &SReplicationMultiToggleCheckbox::GetWarningVisibility)
					.ToolTipText(this, &SReplicationMultiToggleCheckbox::GetWarningToolTipText)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.WarningWithColor"))
					]
				]
			]
			.OnGetMenuContent(this, &SReplicationMultiToggleCheckbox::GetDropDownMenuContent)
		];
	}

	FText SReplicationMultiToggleCheckbox::GetRootToolTipText() const
	{
		const bool bHasProperties = IsCheckboxEnabledForObject(Object);
		if (!bHasProperties)
		{
			// TODO UE-200496 Update text if we predict there to be a conflict
			return LOCTEXT("Toggle.ToolTip.NoProperties", "Assign properties first.");
		}
		
		switch (GetCheckboxStateForObject(Object))
		{
		case ECheckBoxState::Unchecked: return LOCTEXT("Toggle.ToolTip.Unchecked", "Not replicating.");
		case ECheckBoxState::Checked: return LOCTEXT("Toggle.ToolTip.Checked", "Replicating all assigned objects.");
		case ECheckBoxState::Undetermined: return LOCTEXT("Toggle.ToolTip.Undetermined", "Replicating some assigned objects, but not all.");
		default: return FText::GetEmpty();
		}
	}

	ECheckBoxState SReplicationMultiToggleCheckbox::GetCheckboxStateForObject(FSoftObjectPath InObject) const
	{
		// The checkbox shows
		// - checked if all editable clients with the object registered have authority
		// - unchecked if all editable clients with the object registered do not have authority
		// - undetermined otherwise
		const TArray<FGuid> ClientsWithAuthority = ClientManager->GetAuthorityCache().GetClientsWithAuthorityOverObject(InObject);

		// TODO UE-200496 Predict conflicts
		
		bool bHadAnyEditableClient = false;
		ECheckBoxState ConsolidatedState = ECheckBoxState::Undetermined;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, &ClientsWithAuthority, &bHadAnyEditableClient, &ConsolidatedState](const FGuid& ClientId)
		{
			// The state of the checkbox always skips non-editable clients
			const FReplicationClient* Client = ClientManager->FindClient(ClientId);
			if (!ensure(Client) || !Client->AllowsEditing())
			{
				return EBreakBehavior::Continue;
			}
			
			bHadAnyEditableClient = true;
			const bool bHasAuthority = ClientsWithAuthority.Contains(Client->GetEndpointId());
			const ECheckBoxState ClientState = bHasAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			
			if (ConsolidatedState == ECheckBoxState::Undetermined)
			{
				ConsolidatedState = ClientState;
			}
			else if (ConsolidatedState != ClientState)
			{
				ConsolidatedState = ECheckBoxState::Undetermined;
				return EBreakBehavior::Break;
			}
			
			return EBreakBehavior::Continue;
		});
		
		return bHadAnyEditableClient ? ConsolidatedState : ECheckBoxState::Unchecked;
	}

	bool SReplicationMultiToggleCheckbox::IsCheckboxEnabledForObject(FSoftObjectPath InObject) const
	{
		// TODO UE-200496 Predict conflicts
		
		bool bHasAtLeastOneEditableClient = false;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, &bHasAtLeastOneEditableClient](const FGuid& ClientId)
		{
			const FReplicationClient* Client = ClientManager->FindClient(ClientId);
			bHasAtLeastOneEditableClient = ensure(Client) && Client->AllowsEditing();
			return bHasAtLeastOneEditableClient ? EBreakBehavior::Break : EBreakBehavior::Continue;
		});
		return bHasAtLeastOneEditableClient;
	}

	void SReplicationMultiToggleCheckbox::OnCheckboxStateChangedForObject(ECheckBoxState NewState, FSoftObjectPath InObject) const
	{
		const bool bShouldHaveAuthority = NewState == ECheckBoxState::Checked;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(InObject, [this, InObject, bShouldHaveAuthority](const FGuid& ClientId)
		{
			FReplicationClient* Client = ClientManager->FindClient(ClientId);
			if (ensure(Client) && Client->AllowsEditing())
			{
				Client->GetAuthorityDiffer().SetAuthorityIfAllowed({ InObject }, bShouldHaveAuthority);
			}
			
			return EBreakBehavior::Continue;
		});
	}

	TSharedRef<SWidget> SReplicationMultiToggleCheckbox::GetDropDownMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("OnlyThis", "Toggle object only"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					const ECheckBoxState OldCheckboxState = GetCheckboxStateForObject(Object);
					const bool bGiveAuthority = OldCheckboxState == ECheckBoxState::Unchecked;
					OnCheckboxStateChangedForObject(bGiveAuthority ? ECheckBoxState::Checked : ECheckBoxState::Unchecked, Object);
				}),
				FCanExecuteAction::CreateLambda([this](){ return IsCheckboxEnabledForObject(Object); })
				)
			);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ThisAndChildren", "Toggle children only"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::ToggleChildren),
				FCanExecuteAction::CreateSP(this, &SReplicationMultiToggleCheckbox::CanToggleChildren)
				)
			);
		
		return MenuBuilder.MakeWidget();
	}

	void SReplicationMultiToggleCheckbox::ToggleChildren() const
	{
		const TArray<FSoftObjectPath> Subobjects = ConsolidatedStreamModelAttribute.Get()->GetSubobjects(Object);
		ToggleObjects(Subobjects);
	}

	bool SReplicationMultiToggleCheckbox::CanToggleChildren() const
	{
		const TArray<FSoftObjectPath> Subobjects = ConsolidatedStreamModelAttribute.Get()->GetSubobjects(Object);
		return CanToggleObjects(Subobjects);
	}

	void SReplicationMultiToggleCheckbox::ToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const
	{
		if (Objects.IsEmpty())
		{
			return;
		}
		
		ECheckBoxState ConsolidatedCheckboxState = GetCheckboxStateForObject(Objects[0]);
		for (int32 i = 1; i < Objects.Num(); ++i)
		{
			const ECheckBoxState ObjectState = GetCheckboxStateForObject(Objects[i]);
			if (ConsolidatedCheckboxState != ObjectState)
			{
				// If the states are different, pretend they are all Checked so below we default to toggling off
				ConsolidatedCheckboxState = ECheckBoxState::Checked;
				break;
			}
		}

		const ECheckBoxState StateToSet = ConsolidatedCheckboxState == ECheckBoxState::Unchecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		for (const FSoftObjectPath& ObjectPath : Objects)
		{
			OnCheckboxStateChangedForObject(StateToSet, ObjectPath);
		}
	}

	bool SReplicationMultiToggleCheckbox::CanToggleObjects(TConstArrayView<FSoftObjectPath> Objects) const
	{
		return Algo::AnyOf(Objects, [this](const FSoftObjectPath& ObjectPath)
		{
			return IsCheckboxEnabledForObject(ObjectPath);
		});
	}

	EVisibility SReplicationMultiToggleCheckbox::GetWarningVisibility() const
	{
		const bool bContainsNonEditableClients = !GetNonEditableClients().IsEmpty();
		return IsCheckboxEnabledForObject(Object) && bContainsNonEditableClients
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	FText SReplicationMultiToggleCheckbox::GetWarningToolTipText() const
	{
		const TArray<FGuid> NonEditableClients = GetNonEditableClients();
		if (NonEditableClients.IsEmpty())
		{
			// Avoid text substitution below
			return FText::GetEmpty();
		}
		
		FString Clients = FString::JoinBy(NonEditableClients, TEXT(", "), [this](const FGuid& ClientEndpointId)
		{
			return ClientUtils::GetClientDisplayName(*ConcertClient, ClientEndpointId);
		});
		return FText::Format(
			LOCTEXT("WarningIcon.ToolTip", "Has clients that cannot be edited remotely.\nClients: {0}"),
			FText::FromString(MoveTemp(Clients))
			);
	}

	TArray<FGuid> SReplicationMultiToggleCheckbox::GetNonEditableClients() const
	{
		TArray<FGuid> Result;
		ClientManager->GetAuthorityCache().ForEachClientWithObjectInStream(Object, [this, &Result](const FGuid& ClientId)
		{
			// The warning icon cares only about editable clients
			const FReplicationClient* Client = ClientManager->FindClient(ClientId);
			if (ensure(Client) && !Client->AllowsEditing())
			{
				Result.Add(ClientId);
			}
			return EBreakBehavior::Continue;
		});
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE