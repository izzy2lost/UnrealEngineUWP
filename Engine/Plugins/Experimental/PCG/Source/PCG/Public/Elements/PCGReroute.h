// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGReroute.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGRerouteSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGRerouteSettings();

	//~Begin UPCGSettingsInterface interface
	virtual bool CanBeDisabled() const override { return false; }
	virtual bool CanBeDebugged() const override { return false; }
	//~End UPCGSettingsInterface interface

	//~Begin UPCGSettings interface
	virtual bool HasDynamicPins() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName("Reroute"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGRerouteElement", "NodeTitle", "Reroute"); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class PCG_API FPCGRerouteElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
