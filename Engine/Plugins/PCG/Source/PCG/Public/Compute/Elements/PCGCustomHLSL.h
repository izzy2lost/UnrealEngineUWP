// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Compute/PCGPinPropertiesGPU.h"

#include "Compute/IPCGNodeSourceTextProvider.h"

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
	FromFirstOutputPin UMETA(Tooltip = "One thread per pin data element."),
	Fixed UMETA(DisplayName = "Fixed Thread Count"),
	FromProductOfInputPins UMETA(Tooltip = "Dispatches a thread per element in the product of one or more pins. So if there are 4 data elements in pin A and 6 data elements in pin B, 24 threads will be dispatched."),
};

/** Produces a HLSL compute shader which will be executed on the GPU. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGCustomHLSLSettings
	: public UPCGSettings
	, public IPCGNodeSourceTextProvider
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
	virtual bool HasOverridableParams() const override { return false; }
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const override { return true; }
	virtual bool UseSeed() const override { return true; }
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CustomHLSL")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTitle", "Custom HLSL"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGCustomHLSLElement", "NodeTooltip", "Produces a HLSL compute shader which will be executed on the GPU."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }

	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
	virtual bool OnlyExposePreconfiguredSettings() const override { return true; }
#endif

	virtual FString GetAdditionalTitleInformation() const override;
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	virtual bool IsKernelValid(FPCGContext* InContext = nullptr, bool bQuiet = true) const override;
	virtual FString GetCookedKernelSource(const TMap<FName, FPCGKernelAttributeIDAndType>& GlobalAttributeLookupTable) const override;
	virtual const TArray<FPCGKernelAttributeKey> GetKernelAttributeKeys() const { return KernelAttributeKeys; }
	virtual int ComputeKernelThreadCount(const UPCGDataBinding* Binding) const override;
	virtual FPCGDataCollectionDesc ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const override;

protected:
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

#if WITH_EDITOR
	//~Begin IPCGNodeSourceTextProvider interface
	FString GetShaderText() const override;
	FString GetDeclarationsText() const override;
	FString GetShaderFunctionsText() const override;
	void SetShaderFunctionsText(const FString& NewFunctionsText) override;
	void SetShaderText(const FString& NewText) override;
	bool IsShaderTextReadOnly() const override;
	void ApplySourceChanges() override;
	//~End IPCGNodeSourceTextProvider interface
#endif

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

#if WITH_EDITOR
	/** Holds input pin labels from PreEditChange, used in PostEditPropertyChange to update any references in output pin setup. */
	TArray<FName> InputPinLabelsPreEditChange;
#endif

protected:
	/** Optional functions that can be called from the source. Intended to be edited using the Node Source Editor window. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Source", meta = (MultiLine = true))
	FString ShaderFunctions;

	/** Shader code that forms the body of the kernel. Intended to be edited using the Node Source Editor window. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Source", meta = (MultiLine = true))
	FString ShaderSource;

	/** Inputs data accessors that can be used from the shader code. Intended to be viewed using the Node Source Editor window. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Inputs", meta = (MultiLine = true))
	FString InputDeclarations;

	/** Output data accessors that can be used from the shader code. Intended to be viewed using the Node Source Editor window. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Outputs", meta = (MultiLine = true))
	FString OutputDeclarations;

	/** Helper data and functions that can be used from the shader code. Intended to be viewed using the Node Source Editor window. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Declarations|Helpers", meta = (MultiLine = true))
	FString HelperDeclarations;

	/** Attributes statically detected as being read, written, or created by this node. */
	TArray<FPCGKernelAttributeKey> KernelAttributeKeys;

	/** Maps pins to their attribute keys and whether or not they were created on the GPU. */
	TMap<FName, TArray<TTuple<FPCGKernelAttributeKey, bool /*bCreatedOnGPU*/>>> PinToAttributeKeys;
};

class FPCGCustomHLSLElement : public IPCGElement
{
protected:
	// This will only be called if the custom HLSL node is not set up correctly (valid nodes are replaced with a compute graph element).
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
