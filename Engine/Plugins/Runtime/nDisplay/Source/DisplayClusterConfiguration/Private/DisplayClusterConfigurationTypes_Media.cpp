// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes_Media.h"


///////////////////////////////////////////////////
// FDisplayClusterConfigurationMediaNodeBackbuffer

bool FDisplayClusterConfigurationMediaNodeBackbuffer::IsMediaOutputAssigned() const
{
	// Just see if any media output instances are assigned
	const bool bAnyMediaAssigned = MediaOutputs.ContainsByPredicate([](const FDisplayClusterConfigurationMediaOutput& Item)
		{
			return IsValid(Item.MediaOutput);
		});

	return bAnyMediaAssigned;
}


///////////////////////////////////////////////////
// FDisplayClusterConfigurationMediaViewport

bool FDisplayClusterConfigurationMediaViewport::IsMediaInputAssigned() const
{
	return IsValid(MediaInput.MediaSource);
}

bool FDisplayClusterConfigurationMediaViewport::IsMediaOutputAssigned() const
{
	// Just see if any media output instances are assigned
	const bool bAnyMediaAssigned = MediaOutputs.ContainsByPredicate([](const FDisplayClusterConfigurationMediaOutput& Item)
		{
			return IsValid(Item.MediaOutput);
		});

	return bAnyMediaAssigned;
}


///////////////////////////////////////////////////
// FDisplayClusterConfigurationMediaICVFX

bool FDisplayClusterConfigurationMediaICVFX::IsMediaInputAssigned(const FString& NodeId) const
{
	return IsValid(GetMediaSource(NodeId));
}

bool FDisplayClusterConfigurationMediaICVFX::IsMediaOutputAssigned(const FString& NodeId) const
{
	const TArray<FDisplayClusterConfigurationMediaOutputGroup> NodeOutputGroups = GetMediaOutputGroups(NodeId);

	for (const FDisplayClusterConfigurationMediaOutputGroup& NodeOutputGroup : NodeOutputGroups)
	{
		if (IsValid(NodeOutputGroup.MediaOutput))
		{
			return true;
		}
	}

	return false;
}


//@note
// The way how media is configured for ICVFX cameras technically allows to have multiple inputs assigned
// to the same camera. Yes, this is something that contradicts to a single input concept. However, it provides
// a very user-friendly GUI. So to follow the single input concept, we always return the first media input found.
UMediaSource* FDisplayClusterConfigurationMediaICVFX::GetMediaSource(const FString& NodeId) const
{
	// Look up for a group that contains node ID specified
	for (const FDisplayClusterConfigurationMediaInputGroup& MediaInputGroup : MediaInputGroups)
	{
		const bool bNodeFound = MediaInputGroup.ClusterNodes.ItemNames.ContainsByPredicate([NodeId](const FString& Item)
			{
				return Item.Equals(NodeId, ESearchCase::IgnoreCase);
			});

		if (bNodeFound && IsValid(MediaInputGroup.MediaSource))
		{
			return MediaInputGroup.MediaSource;
		}
	}

	return nullptr;
}

TArray<FDisplayClusterConfigurationMediaOutputGroup> FDisplayClusterConfigurationMediaICVFX::GetMediaOutputGroups(const FString& NodeId) const
{
	return MediaOutputGroups.FilterByPredicate([NodeId](const FDisplayClusterConfigurationMediaOutputGroup& Item)
		{
			return Item.ClusterNodes.ItemNames.Contains(NodeId);
		});
}

/** Returns all tiles bound to a specific cluster node. */
bool FDisplayClusterConfigurationMediaICVFX::GetMediaInputTiles(const FString& NodeId, TArray<FDisplayClusterConfigurationMediaUniformTileInput>& OutInputTiles) const
{
	// Here we iterate through tiled media input groups, and look for a group that contains a cluster node ID specified.
	for (const FDisplayClusterConfigurationMediaTiledInputGroup& TiledInputGroup : TiledMediaInputGroups)
	{
		// Look for NodeId in the group
		const bool bFoundGroupWithNodeId = TiledInputGroup.ClusterNodes.ItemNames.ContainsByPredicate([NodeId](const FString& Item)
			{
				return Item.Equals(NodeId, ESearchCase::IgnoreCase);
			});

		// If found
		if (bFoundGroupWithNodeId)
		{
			OutInputTiles = TiledInputGroup.Tiles;
			return true;
		}
	}

	return false;
}

/** Returns all tiles bound to a specific cluster node. */
bool FDisplayClusterConfigurationMediaICVFX::GetMediaOutputTiles(const FString& NodeId, TArray<FDisplayClusterConfigurationMediaUniformTileOutput>& OutOutputTiles) const
{
	// Here we iterate through tiled media output groups, and look for a group that contains a cluster node ID specified.
	for (const FDisplayClusterConfigurationMediaTiledOutputGroup& TiledOutputGroup : TiledMediaOutputGroups)
	{
		// Look for NodeId in the group
		const bool bFoundGroupWithNodeId = TiledOutputGroup.ClusterNodes.ItemNames.ContainsByPredicate([NodeId](const FString& Item)
			{
				return Item.Equals(NodeId, ESearchCase::IgnoreCase);
			});

		// If found
		if (bFoundGroupWithNodeId)
		{
			OutOutputTiles = TiledOutputGroup.Tiles;
			return true;
		}
	}

	return false;
}
