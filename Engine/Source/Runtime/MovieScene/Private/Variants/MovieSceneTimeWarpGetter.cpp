// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variants/MovieSceneTimeWarpGetter.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneSection.h"

UMovieSceneTimeWarpGetter::UMovieSceneTimeWarpGetter()
{
	// Allow time-warps to be accessible across different packages so that 
	//     we can store them directly inside FMovieSceneSequenceTransforms
	SetFlags(RF_Public);
}

EMovieSceneChannelProxyType UMovieSceneTimeWarpGetter::PopulateChannelProxy(FMovieSceneChannelProxyData& OutProxyData, EAllowTopLevelChannels AllowTopLevel)
{
	return EMovieSceneChannelProxyType::Static;
}

bool UMovieSceneTimeWarpGetter::DeleteChannel(FMovieSceneTimeWarpVariant& OutVariant, FName ChannelName)
{
	return false;
}
