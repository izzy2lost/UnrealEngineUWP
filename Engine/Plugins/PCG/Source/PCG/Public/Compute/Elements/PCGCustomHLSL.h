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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayAfter = "Tooltip", EditCondition = "bDisplayBufferSizeSettings", EditConditionHides, HideEditConditionToggle))
	EPCGPinBufferSizeMode BufferSizeMode = EPCGPinBufferSizeMode::FromFirstPin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayAfter = "BufferSizeMode", EditCondition = "(bDisplayBufferSizeSettings && BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount) || AllowedTypes == EPCGDataType::Param", EditConditionHides))
	int FixedBufferElementCount = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "Buffer Size Input Pins", Category = Settings, meta = (DisplayAfter = "BufferSizeMode", EditCondition = "bDisplayBufferSizeSettings && BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins", EditConditionHides, GetOptions = "GetInputPinNames"))
	TArray<FName> BufferSizeInputPinLabels;

	/** Select an input pin to copy attributes from. If left as 'None', this will be ignored. Note, this will copy attribute names only, not their values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayAfter = "BufferSizeMode", EditCondition = "bAllowEditInitializationPin", EditConditionHides, HideEditConditionToggle, GetOptions = "GetInputPinNamesAndNone"))
	FName InitializeFromPin = NAME_None;

	/** Add entries to create new attributes on data emitted by this pin. */
	UPROPERTY(EditAnywhere, DisplayName = "Attributes to Create", Category = Settings, meta = (DisplayAfter = "BufferSizeMode"))
	TArray<FPCGKernelAttributeKey> CreatedKernelAttributeKeys;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	bool bDisplayBufferSizeSettings = true;

	UPROPERTY(Transient)
	bool bAllowEditInitializationPin = false;
#endif // WITH_EDITORONLY_DATA
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
#endif
	//~End UObject interface

	//~Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return InputPins; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const { return true; }
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CustomHLSL")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTitle", "Custom HLSL"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTooltip", "Produces a HLSL compute shader which will be executed on the GPU."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }
#endif

	virtual bool IsKernelValid(FPCGContext* InContext = nullptr, bool bQuiet = true) const override;
	virtual FString GetCookedKernelSource(const TMap<FPCGKernelAttributeKey, int>& GlobalAttributeLookupTable) const override;
	virtual int ComputeKernelThreadCount(const UPCGDataBinding* Binding) const override;
	virtual FPCGDataCollectionDesc ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	/** Gets the GPU pin properties for the output pin with the given label. */
	const FPCGPinPropertiesGPU* GetOutputPinPropertiesGPU(const FName& InPinLabel) const;

#if WITH_EDITOR
	void UpdateDeclarations();
	void UpdateInputDeclarations();
	void UpdateOutputDeclarations();
	void UpdateHelperDeclarations();
	void UpdatePinSettings();
	void UpdateAttributeKeys();

	/** List of all non-advanced input pin names. */
	UFUNCTION()
	TArray<FName> GetInputPinNames() const;

	/** List of all non-advanced input pin names, prepended with 'Name_NONE'. */
	UFUNCTION()
	TArray<FName> GetInputPinNamesAndNone() const;
#endif

	const UPCGPin* GetInputPin(FName Label) const;
	const UPCGPin* GetOutputPin(FName Label) const;
	const UPCGPin* GetFirstInputPin() const;
	const UPCGPin* GetPointProcessingInputPin() const;
	const UPCGPin* GetFirstOutputPin() const;
	const UPCGPin* GetFirstPointOutputPin() const;
	int GetProcessingElemCountForInputPin(const UPCGPin* InputPin, const UPCGDataBinding* Binding) const;
	bool AreKernelAttributesValid(FPCGContext* InContext, FText* OutErrorText) const;

	/** Will the ThreadCountMultiplier value be applied when calculating the dispatch thread count. */
	bool IsThreadCountMultiplierInUse() const { return KernelType == EPCGKernelType::Custom && DispatchThreadCount != EPCGDispatchThreadCount::Fixed; }

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	EPCGKernelType KernelType = EPCGKernelType::PointProcessor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "KernelType == EPCGKernelType::PointGenerator", EditConditionHides))
	int PointCount = 256;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom", EditConditionHides))
	EPCGDispatchThreadCount DispatchThreadCount = EPCGDispatchThreadCount::FromFirstOutputPin;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount != EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int ThreadCountMultiplier = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::Fixed", EditConditionHides))
	int FixedThreadCount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "Input Pins", Category = "Settings|Thread Count", meta = (EditCondition = "KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins", EditConditionHides, GetOptions = "GetInputPinNames"))
	TArray<FName> ThreadCountInputPinLabels;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FPCGPinProperties> InputPins = Super::DefaultPointInputPinProperties();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FPCGPinPropertiesGPU> OutputPins = { FPCGPinPropertiesGPU(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point) };

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Source", meta = (MultiLine = true, Tooltip = ""))
	FString ShaderFunctions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Source", meta = (MultiLine = true, Tooltip = ""))
	FString ShaderSource;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Inputs", meta = (MultiLine = true))
	FString InputDeclarations;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Outputs", meta = (MultiLine = true))
	FString OutputDeclarations;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Helpers", meta = (MultiLine = true))
	FString HelperDeclarations;
};

class FPCGCustomHLSLElement : public IPCGElement
{
protected:
	// This will only be called if the custom HLSL node is not set up correctly (valid nodes are replaced with a compute graph element).
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
