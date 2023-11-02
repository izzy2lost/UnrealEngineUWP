// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_CallFunction.h"
#include "NiagaraDataChannel.h"
#include "K2Node_WriteDataChannel.generated.h"


UCLASS(MinimalAPI)
class UK2Node_WriteDataChannel : public UK2Node_CallFunction
{
	GENERATED_BODY()

public:
	UK2Node_WriteDataChannel();

	virtual void PostLoad() override;

	// //~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	// //~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	virtual void PreloadRequiredAssets() override;
	virtual bool ShouldShowNodeProperties() const override;
	//~ End K2Node Interface

	NIAGARABLUEPRINTNODES_API UNiagaraDataChannel* GetDataChannel() const;

	UPROPERTY()
	TSet<FGuid> IgnoredVariables;
	
private:
	UEdGraphPin* GetChannelSelectorPin() const;
	UFunction* GetWriteFunctionForType(const FNiagaraTypeDefinition& TypeDef);

	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelAsset> DataChannel;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid DataChannelVersion;
#endif
};
