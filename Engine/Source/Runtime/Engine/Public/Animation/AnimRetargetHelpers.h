// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UPoseAsset;

namespace UE::Anim::RetargetHelpers
{

#if WITH_EDITOR

enum class ERetargetSourceAssetStatus
{
	NoRetargetDataSet = 0,
	RetargetSourceMissing,
	RetargetDataOk,
};

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UAnimSequence* InAsset);

ERetargetSourceAssetStatus CheckRetargetSourceAssetData(const UPoseAsset* InAsset);

bool ShouldCheckRetargetSourceAssetData();

#endif // WITH_EDITOR

} // end namespace UE::Anim::RetargetHelpers
