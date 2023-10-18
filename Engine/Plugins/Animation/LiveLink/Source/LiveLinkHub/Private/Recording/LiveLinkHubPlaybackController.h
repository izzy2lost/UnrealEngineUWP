// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** Stub class for the playback controller. Will be replaced with the real implementation in a future CL. */
class FLiveLinkHubPlaybackController
{
public:
	FLiveLinkHubPlaybackController() = default;

	bool IsInPlayback() const
	{
		return false;
	}

	bool IsLooping() const
	{
		return false;
	}

	void SetLooping(bool bInLoop)
	{
	}
	
public:
	void Start()
	{
	}
};
