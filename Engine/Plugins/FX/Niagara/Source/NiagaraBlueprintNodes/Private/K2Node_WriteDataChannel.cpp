// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_WriteDataChannel.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "KismetCompiler.h"
#include "NiagaraBlueprintUtil.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"

#define LOCTEXT_NAMESPACE "K2Node_WriteDataChannel"

UK2Node_WriteDataChannel::UK2Node_WriteDataChannel()
{
	FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelLibrary, WriteToNiagaraDataChannelSingle), UNiagaraDataChannelLibrary::StaticClass());
}

void UK2Node_WriteDataChannel::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (DataChannel && DataChannel->Get()->GetVersion() != DataChannelVersion && HasValidBlueprint())
	{
		ReconstructNode();
	}
#endif
}

void UK2Node_WriteDataChannel::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	if (DataChannel)
	{
		for (const FNiagaraDataChannelVariable& InVar : DataChannel->Get()->GetVariables())
		{
			if (IgnoredVariables.Contains(InVar.Version))
			{
				continue;
			}
			UEdGraphPin* NewPin = CreatePin(EGPD_Input, FNiagaraBlueprintUtil::TypeDefinitionToBlueprintType(InVar.GetType()), InVar.GetName());

#if WITH_EDITORONLY_DATA
			NewPin->PersistentGuid = InVar.Version;
#endif
		}

#if WITH_EDITORONLY_DATA
		DataChannelVersion = DataChannel->Get()->GetVersion();
#endif
	}
}

void UK2Node_WriteDataChannel::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

#if WITH_EDITORONLY_DATA
	if (Pin == GetChannelSelectorPin())
	{
		if (UNiagaraDataChannelAsset* ChannelAsset = Cast<UNiagaraDataChannelAsset>(Pin->DefaultObject))
		{
			DataChannel = ChannelAsset;
			ReconstructNode();
		}
	}
#endif
}

void UK2Node_WriteDataChannel::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

#if WITH_EDITORONLY_DATA
	if (Pin == GetChannelSelectorPin())
	{
		if (UNiagaraDataChannelAsset* ChannelAsset = Cast<UNiagaraDataChannelAsset>(Pin->DefaultObject))
		{
			DataChannel = Pin->LinkedTo.Num() == 0 ? ChannelAsset : nullptr;
			ReconstructNode();
		}
	}
#endif
}

void UK2Node_WriteDataChannel::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (InActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		InActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_WriteDataChannel::GetMenuCategory() const
{
	static FText MenuCategory = LOCTEXT("MenuCategory", "Niagara Data Channel");
	return MenuCategory;
}

void UK2Node_WriteDataChannel::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	if (!DataChannel)
	{
		return;
	}
	ExpandSplitPins(CompilerContext, SourceGraph);
	
	// create function call node to init the writer object 
	UK2Node_CallFunction* CreateWriterNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CreateWriterNode->SetFromFunction(UNiagaraDataChannelLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelLibrary, WriteToNiagaraDataChannel)));
	CreateWriterNode->AllocateDefaultPins();

	// transfer the input pins over
	static TArray<TPair<FName, FName>> PinsToTransfer = { {"Channel", "Channel"}, {"SearchParams", "SearchParams"}, {"bVisibleToBlueprint", "bVisibleToGame"},
		{"bVisibleToNiagaraCPU", "bVisibleToCPU"}, {"bVisibleToNiagaraGPU", "bVisibleToGPU"}};
	
	for (TPair<FName, FName> Pair : PinsToTransfer)
	{
		UEdGraphPin* OrgInputPin = FindPinChecked(Pair.Key, EGPD_Input);
		UEdGraphPin* NewInputPin = CreateWriterNode->FindPinChecked(Pair.Value, EGPD_Input);
		CompilerContext.MovePinLinksToIntermediate(*OrgInputPin, *NewInputPin);
	}
	CreateWriterNode->FindPinChecked(FName("Count"))->DefaultValue = FString::FromInt(1);
	
	UEdGraphPin* OldExecPin = FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	UEdGraphPin* NewExecPin = CreateWriterNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	CompilerContext.MovePinLinksToIntermediate(*OldExecPin, *NewExecPin);

	// create the write function nodes
	UEdGraphPin* LastExecPin = CreateWriterNode->FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	UEdGraphPin* WriterResultPin = CreateWriterNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
	for (const FNiagaraDataChannelVariable& InVar : DataChannel->Get()->GetVariables())
	{
		if (IgnoredVariables.Contains(InVar.Version))
		{
			continue;
		}
		UEdGraphPin* VarInputPin = FindPinChecked(InVar.GetName(), EGPD_Input);
		if (VarInputPin == nullptr)
		{
			CompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("NoInputPinFound", "Missing input pin for variable '{0}' - @@"), FText::FromName(InVar.GetName())).ToString(), this);
			continue;
		}
		
		UFunction* WriteFunc = GetWriteFunctionForType(InVar.GetType());
		if (WriteFunc == nullptr)
		{
			CompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("NoWriteFuncFound", "Unable to find a write function for data channel variable '{0}' type {1}, looks like the type is not yet supported by UNiagaraDataChannelWriter. (Source Pin @@)"), FText::FromName(InVar.GetName()), FText::FromString(InVar.GetType().GetName())).ToString(), VarInputPin);
			continue;
		}
		
		UK2Node_CallFunction* WriteDataNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		WriteDataNode->SetFromFunction(WriteFunc);
		WriteDataNode->AllocateDefaultPins();

		// connect input pins of the write function
		if (!CompilerContext.GetSchema()->TryCreateConnection(WriterResultPin, WriteDataNode->FindPinChecked(UEdGraphSchema_K2::PN_Self)))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("NoWriterConnection", "Unable to connect writer object result (UNiagaraDataChannelWriter) to write function pins. @@").ToString(), this);
			continue;
		}
		WriteDataNode->FindPinChecked(FName("VarName"), EGPD_Input)->DefaultValue = InVar.GetName().ToString();
		CompilerContext.MovePinLinksToIntermediate(*VarInputPin, *WriteDataNode->FindPinChecked(FName("InData"), EGPD_Input));

		// connect exec pins
		CompilerContext.GetSchema()->TryCreateConnection(LastExecPin, WriteDataNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input));
		LastExecPin = WriteDataNode->FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	}


	// connect the last exec pin
	UEdGraphPin* OldThenPin = FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);
	CompilerContext.MovePinLinksToIntermediate(*OldThenPin, *LastExecPin);
}

UK2Node::ERedirectType UK2Node_WriteDataChannel::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	
	ERedirectType Result = UK2Node::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	if (ERedirectType_None == Result && K2Schema && K2Schema->ArePinTypesCompatible(NewPin->PinType, OldPin->PinType) && (NewPin->PersistentGuid == OldPin->PersistentGuid) && OldPin->PersistentGuid.IsValid())
	{
		Result = ERedirectType_Name;
	}

	return Result;
}

void UK2Node_WriteDataChannel::PreloadRequiredAssets()
{
	Super::PreloadRequiredAssets();

	if (DataChannel)
	{
		PreloadObject(DataChannel);
		PreloadObject(DataChannel->Get());
	}
}

bool UK2Node_WriteDataChannel::ShouldShowNodeProperties() const
{
	return true;
}

UNiagaraDataChannel* UK2Node_WriteDataChannel::GetDataChannel() const
{
	return DataChannel ? DataChannel->Get() : nullptr;
}

UEdGraphPin* UK2Node_WriteDataChannel::GetChannelSelectorPin() const
{
	return FindPinChecked(FName("Channel"), EGPD_Input);
}

UFunction* UK2Node_WriteDataChannel::GetWriteFunctionForType(const FNiagaraTypeDefinition& TypeDef)
{
	if (TypeDef == FNiagaraTypeHelper::GetDoubleDef() || TypeDef == FNiagaraTypeDefinition::GetFloatDef() || TypeDef == FNiagaraTypeDefinition::GetHalfDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteFloat));
	}
	if (TypeDef == FNiagaraTypeHelper::GetVector2DDef() || TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteVector2D));
	}
	if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WritePosition));
	}
	if (TypeDef == FNiagaraTypeHelper::GetVectorDef() || TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteVector));
	}
	if (TypeDef == FNiagaraTypeHelper::GetVector4Def() || TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteVector4));
	}
	if (TypeDef == FNiagaraTypeHelper::GetQuatDef() || TypeDef == FNiagaraTypeDefinition::GetQuatDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteQuat));
	}
	if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteLinearColor));
	}
	if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteInt));
	}
	if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteBool));
	}
	if (TypeDef.GetStruct() == FNiagaraSpawnInfo::StaticStruct())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteSpawnInfo));
	}
	if (TypeDef.GetStruct() == FNiagaraID::StaticStruct())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteID));
	}
	if (TypeDef.GetEnum())
	{
		return UNiagaraDataChannelWriter::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UNiagaraDataChannelWriter, WriteEnum));
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
