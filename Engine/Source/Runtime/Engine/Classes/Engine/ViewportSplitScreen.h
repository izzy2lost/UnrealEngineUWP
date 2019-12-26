// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 
 * Various data declarations relating to split screen on a GameViewportClient
 *
 * @see ESplitScreenType
 * @see FPerPlayerSplitscreenData
 * @see FSplitscreenData
 */
 
/**
 * Enum of the different splitscreen types
 */
namespace ESplitScreenType
{
	enum Type
	{
		// No split
		None,
		// 2 player horizontal split
		TwoPlayer_Horizontal,
		// 2 player vertical split
		TwoPlayer_Vertical,
		// 3 Player split with 1 player on top and 2 on bottom
		ThreePlayer_FavorTop,
		// 3 Player split with 1 player on bottom and 2 on top
		ThreePlayer_FavorBottom,
		//3 Player vertical split
		ThreePlayer_Vertical,
		//3 Player horizontal split
		ThreePlayer_Horizontal,
		// 4 Player grid split
		FourPlayer_Grid,
		// 4 Player vertical split
		FourPlayer_Vertical,
		// 4 Player horizontal split
		FourPlayer_Horizontal,
		// 5 Player split with 2 players on top and 3 on bottom
		FivePlayer_FavorTop,
		// 5 Player split with 3 players on top and 2 on bottom
		FivePlayer_FavorBottom,
		// 5 Player vertical split
		FivePlayer_Vertical,
		// 5 Player horizontal split
		FivePlayer_Horizontal,
		// 6 Player grid split
		SixPlayer_Grid,
		// 6 Player vertical split
		SixPlayer_Vertical,
		// 6 Player horizontal split
		SixPlayer_Horizontal,
		// 7 Player split with 3 players on top and 4 on bottom
		SevenPlayer_FavorTop,
		// 7 Player split with 4 players on top and 3 on bottom
		SevenPlayer_FavorBottom,
		// 7 Player vertical split
		SevenPlayer_Vertical,
		// 7 Player horizontal split
		SevenPlayer_Horizontal,
		// 8 Player grid split
		EightPlayer_Grid,
		// 8 Player vertical split
		EightPlayer_Vertical,
		// 8 Player horizontal split
		EightPlayer_Horizontal,

		SplitTypeCount
	};

	// Deprecated old FourPlayer grid enum value
	UE_DEPRECATED(4.21, "FourPlayer is now FourPlayer_Grid")
	const Type FourPlayer = FourPlayer_Grid;
}

/** Structure to store splitscreen data. */
struct FPerPlayerSplitscreenData
{
	float SizeX;
	float SizeY;
	float OriginX;
	float OriginY;


	FPerPlayerSplitscreenData()
		: SizeX(0)
		, SizeY(0)
		, OriginX(0)
		, OriginY(0)
	{
	}

	FPerPlayerSplitscreenData(float NewSizeX, float NewSizeY, float NewOriginX, float NewOriginY)
		: SizeX(NewSizeX)
		, SizeY(NewSizeY)
		, OriginX(NewOriginX)
		, OriginY(NewOriginY)
	{
	}

};

/** Structure containing all the player splitscreen datas per splitscreen configuration. */
struct FSplitscreenData
{
	TArray<struct FPerPlayerSplitscreenData> PlayerData;
};

