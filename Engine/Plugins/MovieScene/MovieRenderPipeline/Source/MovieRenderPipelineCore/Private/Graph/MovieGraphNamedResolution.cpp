// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphNamedResolution.h"

#include "Graph/MovieGraphProjectSettings.h"

FMovieGraphNamedResolution::FMovieGraphNamedResolution(const FName& InResolutionProfileName)
{
	// Find a matching custom entry from Project Settings
	if (const UMovieGraphProjectSettings* MovieGraphProjectSettings =
		GetDefault<UMovieGraphProjectSettings>())
	{
		if (const FMovieGraphNamedResolution* Match = MovieGraphProjectSettings->FindNamedResolutionForOption(InResolutionProfileName))
		{
			*this = *Match;
			return;
		}
	}

	// Otherwise make a new one
	ProfileName = InResolutionProfileName;
}
