// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PCGElement.h"
#include "PCGSettings.h"

#include "PCGReroute.generated.h"

namespace PCGNamedRerouteConstants
{
	const FName InvisiblePinLabel = TEXT("InvisiblePin");
}

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
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Reroute; }
	virtual bool CanUserEditTitle() const override { return false; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

/** Base class for both reroute declaration and usage to share implementation, but also because they use the same visual node representation in the editor. */
UCLASS(ClassGroup = (Procedural))
class PCG_API UPCGNamedRerouteBaseSettings : public UPCGRerouteSettings
{
	GENERATED_BODY()
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGNamedRerouteDeclarationSettings : public UPCGNamedRerouteBaseSettings
{
	GENERATED_BODY()

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
#if WITH_EDITOR
	virtual bool CanUserEditTitle() const override { return true; }
#endif
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGNamedRerouteUsageSettings : public UPCGNamedRerouteBaseSettings
{
	GENERATED_BODY()

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;

public:
	UPROPERTY(BlueprintReadWrite, Category = Settings)
	TObjectPtr<const UPCGNamedRerouteDeclarationSettings> Declaration;
};

class PCG_API FPCGRerouteElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
