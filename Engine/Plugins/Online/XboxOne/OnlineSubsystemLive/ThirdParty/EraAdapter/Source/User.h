//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace System {

///////////////////////////////////////////////////////////////////////////////
//
//  Enumerations
//
public enum class UserAgeGroup
{
	Unknown = 0x0000,
	Child = 0x0001,
	Teen = 0x0002,
	Adult = 0x0003,
};

public enum class UserLocation
{
	Local = 0x0000,
	Remote = 0x0001,
};

///////////////////////////////////////////////////////////////////////////////
//
//  UserAddedEventArgs
//
ref class User;

public ref class UserAddedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

internal:
	UserAddedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SignInCompletedEventArgs
//
public ref class SignInCompletedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

internal:
	SignInCompletedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SignOutStartedEventArgs
//
public ref class SignOutStartedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

internal:
	SignOutStartedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  SignOutCompletedEventArgs
//
public ref class SignOutCompletedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

	property ::Windows::Foundation::HResult Result
	{
		::Windows::Foundation::HResult get();
	}

internal:
	SignOutCompletedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  UserRemovedEventArgs
//
public ref class UserRemovedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

internal:
	UserRemovedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
};

///////////////////////////////////////////////////////////////////////////////
//
//  UserDisplayInfo
//
public ref class UserDisplayInfo sealed
{
public:

	property UserAgeGroup AgeGroup
	{
		UserAgeGroup get();
	}

	property Platform::String^ ApplicationDisplayName
	{
		Platform::String^ get();
	}

	property Platform::String^ GameDisplayName
	{
		Platform::String^ get();
	}

	property uint32 GamerScore
	{
		uint32 get();
	}

	property Platform::String^ Gamertag
	{
		Platform::String^ get();
	}

	property ::Windows::Foundation::Collections::IVectorView<uint32>^ Privileges
	{
		::Windows::Foundation::Collections::IVectorView<uint32>^ get();
	}

	property uint32 Reputation
	{
		uint32 get();
	}

internal:
	UserDisplayInfo(
		Microsoft::Xbox::Services::Social::XboxUserProfile^ profile
	);

private:
	Microsoft::Xbox::Services::Social::XboxUserProfile^ _profile;
};

///////////////////////////////////////////////////////////////////////////////
//
//  User
//
public ref class User sealed
{
public:

	property ::Windows::Foundation::Collections::IVectorView<::Windows::Gaming::Input::Gamepad^>^ Controllers
	{
		::Windows::Foundation::Collections::IVectorView<::Windows::Gaming::Input::Gamepad^>^ get();
	}

	property UserDisplayInfo^ DisplayInfo
	{
		UserDisplayInfo^ get();
	}

	property uint32 Id
	{
		uint32 get();
	}

	property bool IsSignedIn
	{
		bool get();
	}

	property bool IsGuest
	{
		bool get();
	}

	property UserLocation Location
	{
		UserLocation get();
	}

	property User^ Sponsor
	{
		User^ get();
	}

	static property ::Windows::Foundation::Collections::IVectorView<User^>^ Users
	{
		::Windows::Foundation::Collections::IVectorView<User^>^ get();
	}

	property Platform::String^ XboxUserHash
	{
		Platform::String^ get();
	}

	property Platform::String^ XboxUserId
	{
		Platform::String^ get();
	}

	::Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::System::GetTokenAndSignatureResult^>^
		GetTokenAndSignatureAsync(Platform::String^ httpMethod, Platform::String^ url, Platform::String^ headers) 
	{
		return _user->GetTokenAndSignatureAsync(httpMethod, url, headers);
	}

	static event ::Windows::Foundation::EventHandler<UserAddedEventArgs^>^ UserAdded;
	static event ::Windows::Foundation::EventHandler<SignInCompletedEventArgs^>^ SignInCompleted;
	static event ::Windows::Foundation::EventHandler<SignOutCompletedEventArgs^>^ SignOutCompleted;
	static event ::Windows::Foundation::EventHandler<SignOutStartedEventArgs^>^ SignOutStarted;
	static event ::Windows::Foundation::EventHandler<UserRemovedEventArgs^>^ UserRemoved;

	static User^ 
	ShimUserFromXSAPIUser(
		Microsoft::Xbox::Services::System::XboxLiveUser^ user
	);

	static Microsoft::Xbox::Services::System::XboxLiveUser^
	XSAPIUserFromShimUser(
		User^ user
	);

	static User^ 
	ShimUserFromControllerUser(
		::Windows::System::User^ user
	);

internal:
	static User^ 
	CreateFromXSAPISignin(
		Microsoft::Xbox::Services::System::XboxLiveUser^ user, 
		Microsoft::Xbox::Services::Social::XboxUserProfile^ profile
	);

private:
	User(
		Microsoft::Xbox::Services::System::XboxLiveUser^ user,
		Microsoft::Xbox::Services::Social::XboxUserProfile^ profile
	);

	static void 
	StaticInit();

	static void 
	OnSignOutCompleted(
		Platform::Object^ sender, 
		Microsoft::Xbox::Services::System::SignOutCompletedEventArgs^ args
	);

	Microsoft::Xbox::Services::System::XboxLiveUser^ _user;
	UserDisplayInfo^ _displayInfo;

	static std::once_flag s_StaticInitFlag;
	static std::mutex s_AllUsersLock;
	static Platform::Collections::Vector<User^>^ s_AllUsers;
};

}
}
}
}
