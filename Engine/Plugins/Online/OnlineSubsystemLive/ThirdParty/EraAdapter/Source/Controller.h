//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include "User.h"

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace Input {

///////////////////////////////////////////////////////////////////////////////
//
//  Enumerations
//

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerAddedEventArgs
//
ref class Controller;

public ref class ControllerAddedEventArgs sealed
{
public:
	// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
	//property EraAdapter::Windows::Xbox::Input::Controller^ Controller
	//{
	//	EraAdapter::Windows::Xbox::Input::Controller^ get();
	//}
	property ::Windows::Gaming::Input::Gamepad^ Controller
	{
		::Windows::Gaming::Input::Gamepad^ get();
	}

internal:
	ControllerAddedEventArgs(
		EraAdapter::Windows::Xbox::Input::Controller^ controller
	);

private:
	EraAdapter::Windows::Xbox::Input::Controller^ _controller;
};

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerRemovedEventArgs
//
public ref class ControllerRemovedEventArgs sealed
{
public:
	// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
	//property EraAdapter::Windows::Xbox::Input::Controller^ Controller
	//{
	//	EraAdapter::Windows::Xbox::Input::Controller^ get();
	//}
	property ::Windows::Gaming::Input::Gamepad^ Controller
	{
		::Windows::Gaming::Input::Gamepad^ get();
	}

internal:
	ControllerRemovedEventArgs(
		EraAdapter::Windows::Xbox::Input::Controller^ controller
	);

private:
	EraAdapter::Windows::Xbox::Input::Controller^ _controller;
};

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerPairingChangedEventArgs
//
public ref class ControllerPairingChangedEventArgs sealed
{
public:
	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

	property EraAdapter::Windows::Xbox::System::User^ PreviousUser
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

	// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
	//property EraAdapter::Windows::Xbox::Input::Controller^ Controller
	//{
	//	EraAdapter::Windows::Xbox::Input::Controller^ get();
	//}
	property ::Windows::Gaming::Input::Gamepad^ Controller
	{
		::Windows::Gaming::Input::Gamepad^ get();
	}
	

internal:
	ControllerPairingChangedEventArgs(
		EraAdapter::Windows::Xbox::System::User^ user,
		EraAdapter::Windows::Xbox::System::User^ previousUser,
		EraAdapter::Windows::Xbox::Input::Controller^ controller
	);

private:
	EraAdapter::Windows::Xbox::System::User^ _user;
	EraAdapter::Windows::Xbox::System::User^ _previousUser;
	EraAdapter::Windows::Xbox::Input::Controller^ _controller;
};

///////////////////////////////////////////////////////////////////////////////
//
//  Controller
//
public ref class Controller sealed
{
	friend ref class ControllerAddedEventArgs;
	friend ref class ControllerRemovedEventArgs;
	friend ref class ControllerPairingChangedEventArgs;

public:

	property uint32 Id
	{
		uint32 get();
	}

	property EraAdapter::Windows::Xbox::System::User^ User
	{
		EraAdapter::Windows::Xbox::System::User^ get();
	}

	property Platform::String^ Type
	{
		Platform::String^ get();
	}

	property ::Windows::Gaming::Input::Gamepad^ Gamepad
	{
		::Windows::Gaming::Input::Gamepad^ get();
	}

	static property ::Windows::Foundation::Collections::IVectorView<Controller^>^ Controllers
	{
		::Windows::Foundation::Collections::IVectorView<Controller^>^ get();
	}

	static event ::Windows::Foundation::EventHandler<ControllerAddedEventArgs^>^ ControllerAdded;
	static event ::Windows::Foundation::EventHandler<ControllerRemovedEventArgs^>^ ControllerRemoved;
	static event ::Windows::Foundation::EventHandler<ControllerPairingChangedEventArgs^>^ ControllerPairingChanged;

internal:
	Controller(
		uint32 id,
		::Windows::Gaming::Input::Gamepad^ gamepad
	);
	
private:
	::Windows::System::User^ _priorUser;
	::Windows::Foundation::EventRegistrationToken _gamepadUserChangedEventToken;
	::Windows::Gaming::Input::Gamepad^ _gamepad;
	uint32 _id;

	void OnGamepadUserChanged(::Windows::Gaming::Input::IGameController ^sender, ::Windows::System::UserChangedEventArgs ^args);

	static void
	StaticInit();

	static uint32 s_NextId;
	static std::once_flag s_StaticInitFlag;
	static std::mutex s_AllControllersLock;
	static Platform::Collections::Vector<Controller^>^ s_ActiveControllers;
	static Platform::Collections::Vector<Controller^>^ s_RemovedControllers;

	static void OnGamepadAdded(Platform::Object ^sender, ::Windows::Gaming::Input::Gamepad ^args);
	static void OnGamepadRemoved(Platform::Object ^sender, ::Windows::Gaming::Input::Gamepad ^args);
};

}
}
}
}
