//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace Multiplayer { ref class MultiplayerSessionReference; }
namespace System { ref class User; }
namespace UI {

///////////////////////////////////////////////////////////////////////////////
//
//  Enumerations
//
public enum class AccountPickerOptions
{
	None								= 0x0000,
	AllowGuests							= 0x0001,
	AllowOnlyDistinctControllerTypes	= 0x0002,
};

///////////////////////////////////////////////////////////////////////////////
//
//  AccountPickerResult
//
public ref class AccountPickerResult sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

internal:
	AccountPickerResult(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SystemUI
//
public ref class SystemUI sealed
{
public:
	static ::Windows::Foundation::IAsyncOperation<AccountPickerResult^>^
		ShowAccountPickerAsync(
			Platform::Object ^controller,
			AccountPickerOptions options
		);

	static ::Windows::Foundation::IAsyncAction^
		ShowSendGameInvitesAsync(
			EraAdapter::Windows::Xbox::System::User^ user,
			EraAdapter::Windows::Xbox::Multiplayer::MultiplayerSessionReference^ sessionRef,
			Platform::String^ customActivationContext
		);

	static ::Windows::Foundation::IAsyncAction^
		ShowSendGameInvitesAsync(
			EraAdapter::Windows::Xbox::System::User^ user,
			EraAdapter::Windows::Xbox::Multiplayer::MultiplayerSessionReference ^sessionRef,
			Platform::String ^inviteDisplayTextId,
			Platform::String ^customActivationContext);

	static ::Windows::Foundation::IAsyncAction^
		ShowProfileCardAsync(
			EraAdapter::Windows::Xbox::System::User^ user,
			Platform::String ^targetXuid
		);

	static ::Windows::Foundation::IAsyncAction^
		LaunchAchievementsAsync(
			EraAdapter::Windows::Xbox::System::User^ user,
			uint32 titleId
		);
};

}
}
}
}