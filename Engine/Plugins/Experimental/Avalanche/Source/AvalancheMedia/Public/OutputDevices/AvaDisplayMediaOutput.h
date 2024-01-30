// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCoreDefinitions.h"
#include "MediaOutput.h"

#include "AvaDisplayMediaOutput.generated.h"

/**
 * Output Media to a display adapter.
 */
UCLASS(BlueprintType, meta = (MediaIOCustomLayout = "AvaDisplay"))
class AVALANCHEMEDIA_API UAvaDisplayMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

public:
	UAvaDisplayMediaOutput(const FObjectInitializer& ObjectInitializer);

	/** The device, port and video settings that correspond to the output. */
	UPROPERTY(EditAnywhere, Category = "AvaDisplay", meta = (DisplayName = "Configuration"))
	FMediaIOOutputConfiguration OutputConfiguration;

	//~ UMediaOutput interface
public:
	virtual bool Validate(FString& FailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface
};
