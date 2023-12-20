// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <memory>
/**
 * 
 */
struct MIXERENGINE_API MaterialInfo
{
	FString							name;
	int32							id;
};

typedef std::shared_ptr<MaterialInfo>		MaterialInfoPtr;

