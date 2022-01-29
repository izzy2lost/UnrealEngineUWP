// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	UWPSurvey.h: UWP platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
* UWP implementation of FGenericPlatformSurvey
**/
struct FUWPPlatformSurvey : public FGenericPlatformSurvey
{
	// default implementation for now
};

typedef FUWPPlatformSurvey FPlatformSurvey;