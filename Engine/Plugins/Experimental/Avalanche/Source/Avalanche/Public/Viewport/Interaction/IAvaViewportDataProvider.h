// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#if WITH_EDITOR
#include "Containers/ArrayView.h"
#endif
#include "IAvaViewportDataProvider.generated.h"

class UObject;

#if WITH_EDITOR
struct FAvaViewportGuideInfo_Deprecated;
#endif

/** Interface for Objects that use UAvaSequence and need to be handled by IAvaSequencer */
UINTERFACE(MinimalAPI, NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UAvaViewportDataProvider : public UInterface
{
	GENERATED_BODY()
};

class IAvaViewportDataProvider
{
public:
	GENERATED_BODY()

	virtual UObject* ToUObject() = 0;

	virtual FName GetStartupCameraName() const = 0;

#if WITH_EDITOR
	virtual void SetStartupCameraName(FName InName) = 0;

	virtual TConstArrayView<FAvaViewportGuideInfo_Deprecated> GetViewportGuideData() const = 0;
	virtual void SetViewportGuideData(const TConstArrayView<FAvaViewportGuideInfo_Deprecated>& InGuideData) = 0;
#endif
};
