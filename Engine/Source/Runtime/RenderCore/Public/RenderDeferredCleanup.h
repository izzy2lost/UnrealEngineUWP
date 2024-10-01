// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

/**
 * The base class of objects that need to defer deletion until the render command queue has been flushed.
 */
class FDeferredCleanupInterface
{
public:
	virtual ~FDeferredCleanupInterface() {}
};

/**
 * Adds the specified deferred cleanup object to the current set of pending cleanup objects.
 */
extern RENDERCORE_API void BeginCleanup(FDeferredCleanupInterface* CleanupObject);
