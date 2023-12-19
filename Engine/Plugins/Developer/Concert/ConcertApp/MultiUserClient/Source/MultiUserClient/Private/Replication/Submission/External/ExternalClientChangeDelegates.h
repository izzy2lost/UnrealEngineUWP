// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

struct FObjectReplicationMap;

namespace UE::MultiUserClient
{
	DECLARE_DELEGATE_RetVal(const FObjectReplicationMap*, FGetStreamContent);
}
