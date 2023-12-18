// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VirtualSubjects/LiveLinkBlueprintVirtualSubjectFactory.h"
#include "EnhancedInputVirtualSubjectFactory.generated.h"

/**
 * 
 */
UCLASS()
class ENHANCEDINPUTLIVELINK_API UEnhancedInputVirtualSubjectFactory : public ULiveLinkBlueprintVirtualSubjectFactory
{
	GENERATED_BODY()

public:

	UEnhancedInputVirtualSubjectFactory();

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	//~ Begin UFactory Interface
};
