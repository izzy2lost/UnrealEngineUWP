//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
//*********************************************************
#include "MoviePlayer.h"
#include "UWPMovieStreamer.h"
#include "Modules/ModuleManager.h"

#if WIN10_SDK_VERSION >= 15063

class FUWPMoviePlayerModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		MovieStreamer = MakeShared<FUWPMovieStreamer>();
		GetMoviePlayer()->RegisterMovieStreamer(MovieStreamer);
	}

	virtual void ShutdownModule() override
	{
		if (MovieStreamer.IsValid())
		{
			MovieStreamer.Reset();
		}
	}

	TSharedPtr<FUWPMovieStreamer> MovieStreamer;

};

#else // WIN10_SDK_VERSION >= 15063

#pragma message ("UWPMoviePlayer requires building against the 15063 or higher (current detected version " PREPROCESSOR_TO_STRING(WIN10_SDK_VERSION) ")")

typedef FDefaultModuleImpl FUWPMoviePlayerModule;

#endif // WIN10_SDK_VERSION >= 15063

IMPLEMENT_MODULE(FUWPMoviePlayerModule, UWPMoviePlayer)


