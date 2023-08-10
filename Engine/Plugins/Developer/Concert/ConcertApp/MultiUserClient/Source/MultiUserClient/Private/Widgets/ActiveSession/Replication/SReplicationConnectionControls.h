// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMultiUserReplicationClientProfileAsset;
enum class EJoinReplicationErrorCode : uint8;
class UJoinReplicationSessionArgs;

namespace UE::MultiUserClient
{
	class SReplicationConnectionControls : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationConnectionControls)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		UMultiUserReplicationClientProfileAsset* GetClientProfile() const;

		// Join Button
		FReply OnJoinButtonClicked();
		bool IsJoinButtonEnabled() const { return IsJoinButtonEnabled(nullptr); }
		FText GetJoinButtonTooltip() const;
		bool IsJoinButtonEnabled(FText* ErrorReason) const;
		EVisibility GetJoinButtonVisibility() const;

		// Leave button
		FReply OnLeaveButtonClicked();
		EVisibility GetLeaveButtonVisibility() const { return GetJoinButtonVisibility() == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible; }

		// Settings
		EVisibility GetCogWheelSettingsVisibility() const { return GetJoinButtonVisibility(); }
		EVisibility GetOverrideSettingsVisibility() const { return GetJoinButtonVisibility(); }
	};
}


