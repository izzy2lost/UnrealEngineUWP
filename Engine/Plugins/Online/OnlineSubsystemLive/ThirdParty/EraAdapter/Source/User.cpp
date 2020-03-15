//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "User.h"
#include "Controller.h"

using namespace EraAdapter::Windows::Xbox::System;
using namespace Platform;
using namespace Windows::Foundation;
using namespace concurrency;

///////////////////////////////////////////////////////////////////////////////
//
//  UserAddedEventArgs
//
UserAddedEventArgs::UserAddedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user
) :
	_user(user)
{
}

EraAdapter::Windows::Xbox::System::User^
UserAddedEventArgs::User::get()
{
	return _user;
}

///////////////////////////////////////////////////////////////////////////////
//
//  SignInCompletedEventArgs
//
SignInCompletedEventArgs::SignInCompletedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user
) : 
	_user(user) 
{
}

EraAdapter::Windows::Xbox::System::User^ 
SignInCompletedEventArgs::User::get()
{
	return _user;
}

///////////////////////////////////////////////////////////////////////////////
//
//  SignOutStartedEventArgs
//
SignOutStartedEventArgs::SignOutStartedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user
) :
	_user(user) 
{
}

EraAdapter::Windows::Xbox::System::User^ 
SignOutStartedEventArgs::User::get()
{
	return _user;
}

///////////////////////////////////////////////////////////////////////////////
//
//  SignOutCompletedEventArgs
//
SignOutCompletedEventArgs::SignOutCompletedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user
) : 
	_user(user)
{
}

EraAdapter::Windows::Xbox::System::User^ 
SignOutCompletedEventArgs::User::get()
{
	return _user;
}

::Windows::Foundation::HResult 
SignOutCompletedEventArgs::Result::get()
{
	::Windows::Foundation::HResult hr;
	hr.Value = S_OK;
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
//
//  UserRemovedEventArgs
//
UserRemovedEventArgs::UserRemovedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user
) : 
	_user(user) 
{
}

EraAdapter::Windows::Xbox::System::User^ 
UserRemovedEventArgs::User::get()
{
	return _user;
}

///////////////////////////////////////////////////////////////////////////////
//
//  UserDisplayInfo
//
UserDisplayInfo::UserDisplayInfo(
	Microsoft::Xbox::Services::Social::XboxUserProfile^ profile
) : 
	_profile(profile)
{
}

UserAgeGroup 
UserDisplayInfo::AgeGroup::get()
{
	return UserAgeGroup::Unknown;
}

Platform::String^ 
UserDisplayInfo::ApplicationDisplayName::get()
{
	return _profile->ApplicationDisplayName;
}

Platform::String^ 
UserDisplayInfo::GameDisplayName::get()
{
	return _profile->GameDisplayName;
}

uint32 
UserDisplayInfo::GamerScore::get()
{
	return _wtol(_profile->Gamerscore->Data());
}

Platform::String^ 
UserDisplayInfo::Gamertag::get()
{
	return _profile->Gamertag;
}

::Windows::Foundation::Collections::IVectorView<uint32>^ 
UserDisplayInfo::Privileges::get()
{
	throw ref new Platform::NotImplementedException();
}

uint32 
UserDisplayInfo::Reputation::get()
{
	throw ref new Platform::NotImplementedException();
}

///////////////////////////////////////////////////////////////////////////////
//
//  User
//
std::once_flag User::s_StaticInitFlag;
std::mutex User::s_AllUsersLock;
Platform::Collections::Vector<User^> ^User::s_AllUsers;

User::User(
	Microsoft::Xbox::Services::System::XboxLiveUser^ user,
	Microsoft::Xbox::Services::Social::XboxUserProfile^ profile
) : 
	_user(user), 
	_displayInfo(ref new UserDisplayInfo(profile)) 
{
}

::Windows::Foundation::Collections::IVectorView<::Windows::Gaming::Input::Gamepad^>^
User::Controllers::get()
{
	Platform::Collections::Vector<::Windows::Gaming::Input::Gamepad^>^ controllers =
		ref new Platform::Collections::Vector<::Windows::Gaming::Input::Gamepad^>();
	for each (auto controller in EraAdapter::Windows::Xbox::Input::Controller::Controllers)
	{
		if (controller->User == this)
		{
			controllers->Append(controller->Gamepad);
		}
	}
	return controllers->GetView();;
}

UserDisplayInfo^ 
User::DisplayInfo::get()
{
	return _displayInfo;
}

uint32 
User::Id::get()
{
	return 0;
}

bool 
User::IsSignedIn::get()
{
	return _user->IsSignedIn;
}

bool 
User::IsGuest::get()
{
	return false;
}

UserLocation 
User::Location::get()
{
	return UserLocation::Local;
}

User^
User::Sponsor::get()
{
	return nullptr;
}

::Windows::Foundation::Collections::IVectorView<User^>^ 
User::Users::get()
{
	std::call_once(s_StaticInitFlag, StaticInit);
	return s_AllUsers->GetView();
}

Platform::String^ 
User::XboxUserHash::get()
{
	return _user->XboxUserId;
}

Platform::String^ 
User::XboxUserId::get()
{
	return _user->XboxUserId;
}

User^
User::ShimUserFromXSAPIUser(
	Microsoft::Xbox::Services::System::XboxLiveUser^ user
)
{
	std::call_once(s_StaticInitFlag, StaticInit);

	std::lock_guard<std::mutex> lockInstance(s_AllUsersLock);

	for (auto existingUser : s_AllUsers)
	{
		if (existingUser->XboxUserId == user->XboxUserId)
		{
			return existingUser;
		}
	}

	throw ref new Platform::InvalidArgumentException();
}

User^
User::CreateFromXSAPISignin(
	Microsoft::Xbox::Services::System::XboxLiveUser ^user, 
	Microsoft::Xbox::Services::Social::XboxUserProfile ^profile
)
{
	std::call_once(s_StaticInitFlag, StaticInit);

	User ^newUserShim = nullptr;
	{
		std::lock_guard<std::mutex> lockInstance(s_AllUsersLock);

		for (auto existingUser : s_AllUsers)
		{
			if (existingUser->XboxUserId == user->XboxUserId)
			{
				return existingUser;
			}
		}

		newUserShim = ref new User(user, profile);
		s_AllUsers->Append(newUserShim);
	}

	if (newUserShim->IsSignedIn)
	{
		UserAdded(nullptr, ref new UserAddedEventArgs(newUserShim));
		SignInCompleted(nullptr, ref new SignInCompletedEventArgs(newUserShim));
	}

	return newUserShim;
}

Microsoft::Xbox::Services::System::XboxLiveUser^
User::XSAPIUserFromShimUser(
	User ^user
)
{
	return user->_user;
}

User^
User::ShimUserFromControllerUser(
	::Windows::System::User ^user
)
{
	// If there were actually multiple users we'd be in trouble.
	std::call_once(s_StaticInitFlag, StaticInit);
	std::lock_guard<std::mutex> lockInstance(s_AllUsersLock);
	return s_AllUsers->Size > 0 ? s_AllUsers->GetAt(0) : nullptr;
}

void 
User::OnSignOutCompleted(
	Platform::Object ^sender, 
	Microsoft::Xbox::Services::System::SignOutCompletedEventArgs ^args
)
{
	std::call_once(s_StaticInitFlag, StaticInit);

	User ^signedOutUserShim = nullptr;
	{
		std::lock_guard<std::mutex> lockInstance(s_AllUsersLock);
		for (uint32_t i = 0; i < s_AllUsers->Size; ++i)
		{
			auto userShim = s_AllUsers->GetAt(i);
			if (userShim->XboxUserId == args->User->XboxUserId)
			{
				signedOutUserShim = userShim;
				s_AllUsers->RemoveAt(i);
				break;
			}
		}
	}
	
	if (signedOutUserShim != nullptr)
	{
		SignOutStarted(nullptr, ref new SignOutStartedEventArgs(signedOutUserShim));
		SignOutCompleted(nullptr, ref new SignOutCompletedEventArgs(signedOutUserShim));
		UserRemoved(nullptr, ref new UserRemovedEventArgs(signedOutUserShim));
	}
}

void 
User::StaticInit()
{
	std::lock_guard<std::mutex> lockInstance(s_AllUsersLock);
	if (s_AllUsers == nullptr)
	{
		s_AllUsers = ref new Platform::Collections::Vector<User^>();
		Microsoft::Xbox::Services::System::XboxLiveUser::SignOutCompleted += ref new ::Windows::Foundation::EventHandler<Microsoft::Xbox::Services::System::SignOutCompletedEventArgs ^>(&User::OnSignOutCompleted);
	}
}