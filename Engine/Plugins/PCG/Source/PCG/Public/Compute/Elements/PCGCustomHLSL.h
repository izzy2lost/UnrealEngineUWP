// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGCustomHLSL.generated.h"

class UPCGPin;

/** Method for computing the size of a pin on a GPU node. */
UENUM()
enum class EPCGPinBufferSizeMode : uint8
{
	FromFirstPin UMETA(DisplayName = "Match First Input Pin"),
	FromProductOfInputPins UMETA(Tooltip = "Dispatches a thread per element in the product of one or more pins. So if there are 4 data elements in pin A and 6 data elements in pin B, 24 threads will be dispatched."),
	FixedElementCount,
};

/** An extension of the pin properties that adds hints for GPU thread count / buffer size calculations. */
USTRUCT(BlueprintType)
struct PCG_API FPCGPinPropertiesGPU : public FPCGPinProperties
{
	GENERATED_BODY()

public:
	FPCGPinPropertiesGPU() = default;

	explicit FPCGPinPropertiesGPU(const FName& InLabel, EPCGDataType InAllowedTypes)
		: FPCGPinProperties(InLabel, InAllowedTypes)
	{
	}

#if WITH_EDITOR
	bool CanEditChange(const FEditPropertyChain& PropertyChain) const;
#endif

public:
	/** Compute graphs use this to calculate the buffer size of output pins. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GPU Buffer Size", meta = (EditCondition = "bDisplayBufferSizeSettings", EditConditionHides, HideEditConditionToggle))
	EPCGPinBufferSizeMode BufferSizeMode = EPCGPinBufferSizeMode::FromFirstPin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GPU Buffer Size", meta = (EditCondition = "(bDisplayBufferSizeSettings && BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount) || AllowedTypes == EPCGDataType::Param", EditConditionHides))
	int FixedBufferElementCount = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "Input Pins", Category = "GPU Buffer Size", meta = (EditCondition = "bDisplayBufferSizeSettings && BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins", EditConditionHides))
	TArray<FName> BufferSizeInputPinLabels;

	/** Select an input pin to copy attributes from. If left as 'None', this will be ignored. Note, this will copy attribute names only, not their values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GPU Buffer Size", meta = (EditCondition = "bAllowEditInitializationPin", EditConditionHides, HideEditConditionToggle))
	FName InitializeFromPin = NAME_None;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	bool bDisplayBufferSizeSettings = true;

	UPROPERTY(Transient)
	bool bAllowEditInitializationPin = false;
#endif // WITH_EDITORONLY_DATA

	/** Add entries to create new attributes on data emitted by this pin. */
	UPROPERTY(EditAnywhere, DisplayName = "Attributes to Create", Category = Settings)
	TArray<FPCGKernelAttributeKey> CreatedKernelAttributeKeys;
};

template<>
struct TStructOpsTypeTraits<FPCGPinPropertiesGPU> : public TStructOpsTypeTraitsBase2<FPCGPinPropertiesGPU>
{
	enum
	{
		WithCanEditChange = true,
	};
};

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
	FromFirstOutputPin UMETA(Tooltip = "One thread per pin data element."),
	Fixed UMETA(DisplayName = "Fixed Thread Count"),
	FromProductOfInputPins UMETA(Tooltip = "Dispatches a thread per element in the product of one or more pins. So if there are 4 data elements in pin A and 6 data elements in pin B, 24 threads will be dispatched."),
};

/** Produces a HLSL compute shader which will be executed on the GPU. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCustomHLSLSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGCustomHLSLSettings();

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return InputPins; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const { return true; }
#if WITH_EDITOR
	virtual bool DisplayExecuteOnGPUSetting() const override { return false; }
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CustomHLSL")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTitle", "Custom HLSL"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTooltip", "Produces a HLSL compute shader which will be executed on the GPU."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }
#endif

	virtual FPCGDataCollectionDesc ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const override;
	virtual int ComputeKernelThreadCount(const UPCGDataBinding* Binding) const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

protected:
	/** Gets the GPU pin properties for the output pin with the given label. */
	const FPCGPinPropertiesGPU* GetOutputPinPropertiesGPU(const FName& InPinLabel) const;

#if WITH_EDITOR
	void UpdateDeclarations();
	void UpdatePinSettings();
	void UpdateAttributeKeys();
#endif

public:
	bool IsKernelValid(FPCGContext* InContext = nullptr, bool bQuiet = true) const;
	
	FString GetCookedKernelSource(const TMap<FPCGKernelAttributeKey, int>& GlobalAttributeLookupTable) const;
	FString GetKernelEntryPoint() const { return TEXT("Main"); }
	FIntVector GetThreadGroupSize() const { return FIntVector(64, 1, 1); }

	int GetPointCount() const { return PointCount; }
	int GetFixedThreadCount() const { return FixedThreadCount; }

	const UPCGPin* GetInputPin(FName Label) const;
	const UPCGPin* GetOutputPin(FName Label) const;
	const UPCGPin* GetFirstInputPin() const;
	const UPCGPin* GetPointProcessingInputPin() const;
	const UPCGPin* GetFirstOutputPin() const;
	const UPCGPin* GetFirstPointOutputPin() const;

	int GetProcessingElemCountForInputPin(const UPCGPin* InputPin, const UPCGDataBinding* Binding) const;
	virtual const UPCGPin* GetExecutionPin() const { return GetPointProcessingInputPin(); }

protected:
	bool AreKernelAttributesValid(FPCGContext* InContext, FText* OutErrorText) const;

	/** Will the ThreadCountMultiplier value be applied when calculating the dispatch thread count. */
	bool IsThreadCountMultiplierInUse() const { return KernelType == EPCGKernelType::Custom && DispatchThreadCount != EPCGDispatchThreadCount::Fixed; }

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGKernelType KernelType = EPCGKernelType::PointProcessor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::PointGenerator", EditConditionHides))
	int PointCount = 256;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom", EditConditionHides))
	EPCGDispatchThreadCount DispatchThreadCount = EPCGDispatchThreadCount::FromFirstOutputPin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount != EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int ThreadCountMultiplier = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int FixedThreadCount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "Input Pins", Category = "Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins", EditConditionHides))
	TArray<FName> ThreadCountInputPinLabels;

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
	TArray<FPCGPinPropertiesGPU> OutputPins = { FPCGPinPropertiesGPU(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point) };

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
