// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"


/**
 * Base class for nDisplay media initializer implementations.
 */
class DISPLAYCLUSTERMODULARFEATURESEDITOR_API IDisplayClusterModularFeatureMediaInitializer
	: public IModularFeature
{
public:
	/** Public feature name */
	static const FName ModularFeatureName;

public:
	virtual ~IDisplayClusterModularFeatureMediaInitializer() = default;

public:

	/**
	 * Checks if media subject is supported by the initializer
	 *
	 * @param MediaSubject - UMediaSource or UMediaOutput instance
	 * @return true if media subject supported
	 */
	virtual bool IsMediaSubjectSupported(const UObject* MediaSubject) = 0;

	/**
	 * Performs initialization of a media subject
	 *
	 * @param MediaSubject   - UMediaSource or UMediaOutput instance
	 * @param OwnerName      - Owner name (e.g. ICVFX camera component)
	 * @param OwnerUniqueIdx - Unique owner index
	 * @param TilePos        - TIle XY-position
	 * 
	 * @return true if media subject supported
	 */
	virtual void InitializeMediaSubjectForTile(UObject* MediaSubject, const FString& OwnerName, uint8 OwnerUniqueIdx, const FIntPoint& TilePos) = 0;
};
