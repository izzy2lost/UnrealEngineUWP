// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RCPanelExposedEntitiesListSettings.generated.h"

enum class ERCFieldGroupType : uint8;
enum class ERCFieldGroupOrder : uint8;

/** Data for exposed entities list settings */
USTRUCT()
struct FRCPanelExposedEntitiesListSettingsData
{
	GENERATED_BODY()

	/** The field group type for the entity list. */
	UPROPERTY()
	ERCFieldGroupType FieldGroupType;

	/** Whether the field groups are expanded */
	UPROPERTY()
	ERCFieldGroupOrder FieldGroupOrder;
};
