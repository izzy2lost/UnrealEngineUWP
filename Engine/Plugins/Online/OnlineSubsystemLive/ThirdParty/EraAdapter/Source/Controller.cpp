//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "Controller.h"

using namespace EraAdapter::Windows::Xbox::Input;
using namespace Platform;
using namespace Windows::Foundation;
using namespace concurrency;

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerAddedEventArgs
//
ControllerAddedEventArgs::ControllerAddedEventArgs(
	EraAdapter::Windows::Xbox::Input::Controller^ controller
) :
	_controller(controller)
{
}

// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
//EraAdapter::Windows::Xbox::Input::Controller^
//ControllerAddedEventArgs::Controller::get()
//{
//	return _controller;
//}
::Windows::Gaming::Input::Gamepad^
ControllerAddedEventArgs::Controller::get()
{
	return _controller->_gamepad;
}

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerRemovedEventArgs
//
ControllerRemovedEventArgs::ControllerRemovedEventArgs(
	EraAdapter::Windows::Xbox::Input::Controller^ controller
) :
	_controller(controller)
{
}

// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
//EraAdapter::Windows::Xbox::Input::Controller^
//ControllerRemovedEventArgs::Controller::get()
//{
//	return _controller;
//}
::Windows::Gaming::Input::Gamepad^
ControllerRemovedEventArgs::Controller::get()
{
	return _controller->_gamepad;
}

///////////////////////////////////////////////////////////////////////////////
//
//  ControllerPairingChangedEventArgs
//
ControllerPairingChangedEventArgs::ControllerPairingChangedEventArgs(
	EraAdapter::Windows::Xbox::System::User^ user,
	EraAdapter::Windows::Xbox::System::User^ previousUser,
	EraAdapter::Windows::Xbox::Input::Controller^ controller
) :
	_user(user),
	_previousUser(previousUser),
	_controller(controller)
{
}

EraAdapter::Windows::Xbox::System::User^ 
ControllerPairingChangedEventArgs::User::get()
{
	return _user;
}

EraAdapter::Windows::Xbox::System::User^
ControllerPairingChangedEventArgs::PreviousUser::get()
{
	return _previousUser;
}

// NOTE: unwrapping type at this layer to make OnlineSubsystemCalls source compatible
//EraAdapter::Windows::Xbox::Input::Controller^
//ControllerPairingChangedEventArgs::Controller::get()
//{
//	return _controller;
//}
::Windows::Gaming::Input::Gamepad^
ControllerPairingChangedEventArgs::Controller::get()
{
	return _controller->_gamepad;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Controller
//
uint32 Controller::s_NextId= 0ul;
std::once_flag Controller::s_StaticInitFlag;
std::mutex Controller::s_AllControllersLock;
Platform::Collections::Vector<Controller^>^ Controller::s_ActiveControllers;
Platform::Collections::Vector<Controller^>^ Controller::s_RemovedControllers;

Controller::Controller(
	uint32 id,
	::Windows::Gaming::Input::Gamepad^ gamepad
) :
	_id(id),
	_gamepad(gamepad)
{
	_priorUser = _gamepad->User;
	_gamepadUserChangedEventToken = _gamepad->UserChanged += 
		ref new ::Windows::Foundation::TypedEventHandler<::Windows::Gaming::Input::IGameController ^, ::Windows::System::UserChangedEventArgs ^>(
			this, &Controller::OnGamepadUserChanged
			);
}

uint32 
Controller::Id::get()
{
	return _id;
}

EraAdapter::Windows::Xbox::System::User^
Controller::User::get()
{
	return EraAdapter::Windows::Xbox::System::User::ShimUserFromControllerUser(_gamepad->User);
}

Platform::String^ 
Controller::Type::get()
{
	static String^ ControllerType = Platform::StringReference(L"Windows.Xbox.Input.Controller");
	return ControllerType;
}

::Windows::Gaming::Input::Gamepad^
Controller::Gamepad::get()
{
	return _gamepad;
}

void 
Controller::StaticInit()
{
	std::lock_guard<std::mutex> lockInstance(s_AllControllersLock);
	if (s_ActiveControllers == nullptr)
	{
		s_ActiveControllers = ref new Platform::Collections::Vector<Controller^>();
	}
	if (s_RemovedControllers == nullptr)
	{
		s_RemovedControllers = ref new Platform::Collections::Vector<Controller^>();
	}

	::Windows::Gaming::Input::Gamepad::GamepadAdded += ref new ::Windows::Foundation::EventHandler<::Windows::Gaming::Input::Gamepad ^>(&Controller::OnGamepadAdded);
	::Windows::Gaming::Input::Gamepad::GamepadRemoved += ref new ::Windows::Foundation::EventHandler<::Windows::Gaming::Input::Gamepad ^>(&Controller::OnGamepadRemoved);

	auto startingGamepads = ::Windows::Gaming::Input::Gamepad::Gamepads;
	for (uint32 i = 0; i < startingGamepads->Size; i++)
	{
		// Windows doesn't assign a controller ID, we'll track pseudo Ids
		s_ActiveControllers->Append(ref new Controller(s_NextId++, startingGamepads->GetAt(i)));
	}
}

void EraAdapter::Windows::Xbox::Input::Controller::OnGamepadAdded(Platform::Object ^sender, ::Windows::Gaming::Input::Gamepad ^args)
{
	Controller^ addedController = nullptr;
	{
		std::lock_guard<std::mutex> lockInstance(s_AllControllersLock);
		// re-add a removed controller if it matches
		for (uint32 i = 0; i < s_RemovedControllers->Size; i++)
		{
			Controller^ previouslyRemovedController = s_RemovedControllers->GetAt(i);
			if (previouslyRemovedController->_gamepad->Equals(args))
			{
				addedController = previouslyRemovedController;
				s_RemovedControllers->RemoveAt(i);
				s_ActiveControllers->Append(addedController);
				break;
			}
		}
		if (!addedController)
		{
			// else, add a new controller
			addedController = ref new Controller(s_NextId++, args);
			s_ActiveControllers->Append(addedController);
		}
	}
	ControllerAdded(sender, ref new	ControllerAddedEventArgs(addedController));
}


void Controller::OnGamepadRemoved(Platform::Object ^sender, ::Windows::Gaming::Input::Gamepad ^args)
{
	Controller^ removedController = nullptr;
	{
		std::lock_guard<std::mutex> lockInstance(s_AllControllersLock);
		for (uint32 i = 0; i < s_ActiveControllers->Size; i++)
		{
			Controller^ activeController = s_ActiveControllers->GetAt(i);
			if (activeController->_gamepad->Equals(args))
			{
				removedController = activeController;
				s_ActiveControllers->RemoveAt(i);
				s_RemovedControllers->Append(removedController);
				break;
			}
		}
	}
	if (removedController)
	{
		ControllerRemoved(sender, ref new ControllerRemovedEventArgs(removedController));
	}
}


void Controller::OnGamepadUserChanged(::Windows::Gaming::Input::IGameController ^sender, ::Windows::System::UserChangedEventArgs ^args)
{
	Controller^ foundController = nullptr;
	{
		std::lock_guard<std::mutex> lockInstance(s_AllControllersLock);
		for (uint32 i = 0; i < s_ActiveControllers->Size; i++)
		{
			Controller^ activeController = s_ActiveControllers->GetAt(i);
			if (activeController->_gamepad->Equals(sender))
			{
				foundController = activeController;
			}
		}
	}
	if (foundController)
	{
		ControllerPairingChanged(this, ref new	ControllerPairingChangedEventArgs(
			foundController->User,
			EraAdapter::Windows::Xbox::System::User::ShimUserFromControllerUser(foundController->_priorUser),
			foundController)
		);
		foundController->_priorUser = foundController->_gamepad->User;
	}
}

::Windows::Foundation::Collections::IVectorView<Controller^>^ Controller::Controllers::get()
{
	std::call_once(s_StaticInitFlag, StaticInit);
	return s_ActiveControllers->GetView();
}