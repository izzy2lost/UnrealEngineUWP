//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "SystemUI.h"
#include "User.h"
#include "MultiplayerSessionReference.h"

using namespace EraAdapter::Windows::Xbox::UI;
using namespace EraAdapter::Windows::Xbox::System;
using namespace EraAdapter::Windows::Xbox::Multiplayer;
using namespace Platform;
using namespace ::Windows::Foundation;
using namespace concurrency;

///////////////////////////////////////////////////////////////////////////////
//
//  AccountPickerResult
//
AccountPickerResult::AccountPickerResult(
	EraAdapter::Windows::Xbox::System::User^ user
) : 
	_user(user) 
{
}

EraAdapter::Windows::Xbox::System::User^ 
AccountPickerResult::User::get()
{
	return _user;
}

///////////////////////////////////////////////////////////////////////////////
//
//  SystemUI
//
IAsyncOperation<AccountPickerResult^>^
SystemUI::ShowAccountPickerAsync(
	Object^ controller,
	AccountPickerOptions options
)
{
	auto user = ref new ::Microsoft::Xbox::Services::System::XboxLiveUser();
	auto signInTask = create_task(user->SignInAsync(nullptr)).then(
		[user](::Microsoft::Xbox::Services::System::SignInResult^ signInResult)
	{
		switch (signInResult->Status)
		{
		case ::Microsoft::Xbox::Services::System::SignInStatus::Success:
			return create_task((ref new Microsoft::Xbox::Services::XboxLiveContext(user))->ProfileService->GetUserProfileAsync(user->XboxUserId));

		default:
			return task_from_result<Microsoft::Xbox::Services::Social::XboxUserProfile^>(nullptr);
		}
	}).then(
		[user](Microsoft::Xbox::Services::Social::XboxUserProfile ^profile)
	{
		return ref new AccountPickerResult(profile != nullptr ? User::CreateFromXSAPISignin(user, profile) : nullptr);
	});
	return create_async([signInTask]() { return signInTask; });
}

IAsyncAction^
SystemUI::ShowSendGameInvitesAsync(
	User^ user, 
	MultiplayerSessionReference^ sessionRef, 
	String^ customActivationContext
)
{
	auto xsapiSessionRef = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(sessionRef->ServiceConfigurationId, sessionRef->SessionTemplateName, sessionRef->SessionName);
	return ::Microsoft::Xbox::Services::System::TitleCallableUI::ShowGameInviteUIAsync(xsapiSessionRef, nullptr);
}

IAsyncAction^
SystemUI::ShowSendGameInvitesAsync(
	User^ user,
	MultiplayerSessionReference^ sessionRef, 
	String^ inviteDisplayTextId, 
	String^ customActivationContext
)
{
	auto xsapiSessionRef = ref new Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference(sessionRef->ServiceConfigurationId, sessionRef->SessionTemplateName, sessionRef->SessionName);
	return ::Microsoft::Xbox::Services::System::TitleCallableUI::ShowGameInviteUIAsync(xsapiSessionRef, inviteDisplayTextId);
}

IAsyncAction^ 
SystemUI::ShowProfileCardAsync(
	User^ user, 
	String^ targetXuid)
{
	return ::Microsoft::Xbox::Services::System::TitleCallableUI::ShowProfileCardUIAsync(targetXuid);
}

IAsyncAction^
SystemUI::LaunchAchievementsAsync(
	User^ user,
	uint32 titleId)
{
	::Windows::System::User^ winUser = User::XSAPIUserFromShimUser(user)->WindowsSystemUser;

	return ::Microsoft::Xbox::Services::System::TitleCallableUI::ShowTitleAchievementsUIForUserAsync(titleId, winUser);
}
