//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#include "pch.h"
#include "MultiplayerSessionReference.h"

using namespace EraAdapter::Windows::Xbox::Multiplayer;

///////////////////////////////////////////////////////////////////////////////
//
//  MultiplayerSessionReference
//
MultiplayerSessionReference::MultiplayerSessionReference(
	Platform::String^ sessionName, 
	Platform::String^ serviceConfigurationId, 
	Platform::String^ sessionTemplateName
) :
	_sessionName(sessionName),
	_serviceConfigurationId(serviceConfigurationId),
	_sessionTemplateName(sessionTemplateName)
{
}

Platform::String^
MultiplayerSessionReference::SessionName::get()
{
	return _sessionName;
}

Platform::String^
MultiplayerSessionReference::ServiceConfigurationId::get()
{
	return _serviceConfigurationId;
}

Platform::String^
MultiplayerSessionReference::SessionTemplateName::get()
{
	return _sessionTemplateName;
}