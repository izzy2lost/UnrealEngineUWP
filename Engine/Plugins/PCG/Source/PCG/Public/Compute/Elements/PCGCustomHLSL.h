// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGCustomHLSL.generated.h"

class UPCGPin;

/** Type of kernel allows us to make decisions about execution automatically, streamlining authoring. */
UENUM()
enum class EPCGKernelType : uint8
{
	PointProcessor UMETA(Tooltip = "Kernel executes on each point in first input pin."),
	PointGenerator UMETA(Tooltip = "Kernel executes for fixed number of points, configurable on node."),
	Custom UMETA(Tooltip = "Execution thread counts and output buffer sizes configurable on node. All data read/write indices must be manually bounds checked."),
};

/** Total number of threads that will be dispatched for this kernel. */
UENUM()
enum class EPCGDispatchThreadCount : uint8
{
	FromOutput UMETA(DisplayName = "Match First Output Pin", Tooltip = "One thread per pin data element."),
	FromFirstInput UMETA(DisplayName = "Match First Input Pin", Tooltip = "One thread per pin data element."),
	FromSecondInput UMETA(DisplayName = "Match Second Input Pin", Tooltip = "One thread per pin data element."),
	FirstInputXSecondInput UMETA(DisplayName = "First Input X Second Input"),
	Fixed UMETA(DisplayName = "Fixed Thread Count"),

	// TODO: Could support inlined expression evaluation for dispatch thread count.
};

/** Prouduces a HLSL compute shader which will be executed on the GPU. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCustomHLSLSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return InputPins; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return OutputPins; }
	virtual bool ShouldExecuteOnGPU() const override;
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const { return true; }
#if WITH_EDITOR
	virtual bool DisplayExecuteOnGPUSetting() const override { return false; }
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CustomHLSL")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTitle", "Custom HLSL"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTooltip", "Prouduces a HLSL compute shader which will be executed on the GPU."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }
#endif

	virtual TArray<FPCGKernelAttributeKey> GetKernelAttributeKeys() const override;
	virtual FPCGDataCollectionDesc ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const override;
	virtual int ComputeKernelThreadCount(const UPCGDataBinding* Binding) const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

#if WITH_EDITOR
	void UpdateDeclarations();
	void UpdatePinSettings();
#endif

public:
	bool IsKernelValid(FPCGContext* InContext = nullptr) const;
	
	FString GetCookedKernelSource(const TMap<FPCGKernelAttributeKey, int>& GlobalAttributeLookupTable) const;
	FString GetKernelEntryPoint() const { return TEXT("Main"); }
	FIntVector GetThreadGroupSize() const { return FIntVector(64, 1, 1); }

	int GetPointCount() const { return PointCount; }
	int GetFixedThreadCount() const { return FixedThreadCount; }

	const UPCGPin* GetFirstInputPin() const;
	const UPCGPin* GetSecondInputPin() const;
	const UPCGPin* GetPointProcessingInputPin() const;
	const UPCGPin* GetSecondPointProcessingInputPin() const;
	const UPCGPin* GetFirstOutputPin() const;
	const UPCGPin* GetFirstPointOutputPin() const;

	int GetProcessingElemCountForInputPin(const UPCGPin* InputPin, const UPCGDataBinding* Binding) const;
	virtual const UPCGPin* GetExecutionPin() const { return GetPointProcessingInputPin(); }

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGKernelType KernelType = EPCGKernelType::PointProcessor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::PointGenerator", EditConditionHides))
	int PointCount = 256;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::Custom", EditConditionHides))
	EPCGDispatchThreadCount DispatchThreadCount = EPCGDispatchThreadCount::FromOutput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount != EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int ThreadCountMultiplier = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int FixedThreadCount = 1;

public:
	/** Dump the cooked HLSL into the log after it is generated. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDumpCookedHLSL = false;

	/** Enable use of 'WriteDebugValue(uint Index, float Value)' function in your kernel. Allows you to write float values to a buffer for logging on the CPU. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bPrintShaderDebugValues = false;

	/** Size (in number of floats) of the shader debug print buffer. */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition="bPrintShaderDebugValues", EditConditionHides))
	int DebugBufferSize = 16;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FPCGPinProperties> InputPins = Super::DefaultPointInputPinProperties();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FPCGPinProperties> OutputPins = Super::DefaultPointOutputPinProperties();

protected:
	UPROPERTY(Transient, VisibleAnywhere, Category = "Settings|Declarations", meta = (MultiLine = true))
	FString InputDeclarations;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Settings|Declarations", meta = (MultiLine = true))
	FString OutputDeclarations;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (MultiLine = true, Tooltip = ""))
	FString ShaderFunctions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (MultiLine = true, Tooltip = ""))
	FString ShaderSource;
};

class FPCGCustomHLSLElement : public IPCGElement
{
protected:
	// This will only be called if the custom HLSL node is not set up correctly (valid nodes are replaced with a compute graph element).
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
