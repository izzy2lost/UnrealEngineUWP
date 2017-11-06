//-----------------------------------------------------------------------------
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

namespace EraAdapter {
namespace Windows {
namespace Xbox {
namespace Multiplayer {

///////////////////////////////////////////////////////////////////////////////
//
//  MultiplayerSessionReference
//
public ref class MultiplayerSessionReference sealed
{
public:
	MultiplayerSessionReference(
		Platform::String^ sessionName, 
		Platform::String^ serviceConfigurationId, 
		Platform::String^ sessionTemplateName
	);

	property Platform::String ^SessionName
	{
		Platform::String ^get();
	}

	property Platform::String ^ServiceConfigurationId
	{
		Platform::String ^get();
	}

	property Platform::String ^SessionTemplateName
	{
		Platform::String ^get();
	}

private:
	Platform::String ^_sessionName;
	Platform::String ^_serviceConfigurationId;
	Platform::String ^_sessionTemplateName;
};

}
}
}
}
