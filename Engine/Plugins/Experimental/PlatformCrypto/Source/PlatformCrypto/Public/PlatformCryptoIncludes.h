// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// @ATG_CHANGE : BEGIN - allow more platforms to use AES-GCM
#if PLATFORM_XBOXONE || PLATFORM_WINDOWS || PLATFORM_UWP
// @ATG_CHANGE : END
	#include "EncryptionContextBCrypt.h"
#else
	#include "EncryptionContextOpenSSL.h"
#endif
