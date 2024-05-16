// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNode.h"

#include "ChaosLog.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowArchive.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Templates/TypeHash.h"
#include "Dataflow/DataflowNodeFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowNode)

const FName FDataflowNode::DataflowInput = TEXT("DataflowInput");
const FName FDataflowNode::DataflowOutput = TEXT("DataflowOutput");
const FName FDataflowNode::DataflowPassthrough = TEXT("DataflowPassthrough");
const FName FDataflowNode::DataflowIntrinsic = TEXT("DataflowIntrinsic");

const FLinearColor FDataflowNode::DefaultNodeTitleColor = FLinearColor(1.f, 1.f, 0.8f);
const FLinearColor FDataflowNode::DefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

const FName FDataflowAnyType::TypeName = TEXT("FDataflowAnyType");

//
// Inputs
//

bool FDataflowNode::OutputSupportsType(FName InName, FName InType) const
{
	if (const FDataflowOutput* Output = FindOutput(InName))
	{
		return Output->SupportsType(InType);
	}
	return false;
}

bool FDataflowNode::InputSupportsType(FName InName, FName InType) const
{
	if (const FDataflowInput* Input = FindInput(InName))
	{
		return Input->SupportsType(InType);
	}
	return false;
}

void FDataflowNode::AddInput(FDataflowInput* InPtr)
{
	if (InPtr)
	{
		for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
		{
			const FDataflowInput* const In = Elem.Value;
			ensureMsgf(!In->GetName().IsEqual(InPtr->GetName()), TEXT("Add Input Failed: Existing Node input already defined with name (%s)"), *InPtr->GetName().ToString());
		}

		check(InPtr->GetOwningNode() == this);

		const uint32 PropertyOffset = InPtr->GetOffset();
		if (ensure(!Inputs.Contains(PropertyOffset)))
		{
			Inputs.Add(PropertyOffset, InPtr);
		}
	}
}

FDataflowInput* FDataflowNode::FindInput(FName InName)
{
	for (TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		FDataflowInput* const Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return Con;
		}
	}
	return nullptr;
}


const FDataflowInput* FDataflowNode::FindInput(FName InName) const
{
	for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		const FDataflowInput* const Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return Con;
		}
	}
	return nullptr;
}

const FDataflowInput* FDataflowNode::FindInput(const void* Reference) const
{
	const int32 Key = GetConnectionOffsetFromReference(Reference);
	if (const FDataflowInput* const* Con = Inputs.Find(Key))
	{
		check(*Con && (*Con)->RealAddress() == Reference);
		return *Con;
	}
	return nullptr;
}

FDataflowInput* FDataflowNode::FindInput(void* Reference)
{
	const int32 Key = GetConnectionOffsetFromReference(Reference);
	if (FDataflowInput* const* Con = Inputs.Find(Key))
	{
		check(*Con && (*Con)->RealAddress() == Reference);
		return *Con;
	}
	return nullptr;
}

const FDataflowInput* FDataflowNode::FindInput(const FGuid& InGuid) const
{
	for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		const FDataflowInput* const Con = Elem.Value;
		if (Con->GetGuid() == InGuid)
		{
			return Con;
		}
	}
	return nullptr;
}

TArray< FDataflowInput* > FDataflowNode::GetInputs() const
{
	TArray< FDataflowInput* > Result;
	Inputs.GenerateValueArray(Result);
	return Result;
}

void FDataflowNode::ClearInputs()
{
	for (TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		FDataflowInput* const Con = Elem.Value;
		delete Con;
	}
	Inputs.Reset();
}

bool FDataflowNode::HasHideableInputs() const
{
	for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		const FDataflowInput* const Con = Elem.Value;
		if (Con->GetCanHidePin())
		{
			return true;
		}
	}
	return false;
}

bool FDataflowNode::HasHiddenInputs() const
{
	for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		const FDataflowInput* const Con = Elem.Value;
		if (Con->GetPinIsHidden())
		{
			return true;
		}
	}
	return false;
}

//
// Outputs
//


void FDataflowNode::AddOutput(FDataflowOutput* InPtr)
{
	if (InPtr)
	{
		for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
		{
			const FDataflowOutput* const Out = Elem.Value;
			ensureMsgf(!Out->GetName().IsEqual(InPtr->GetName()), TEXT("Add Output Failed: Existing Node output already defined with name (%s)"), *InPtr->GetName().ToString());
		}

		check(InPtr->GetOwningNode() == this);

		const uint32 PropertyOffset = InPtr->GetOffset();
		if (ensure(!Outputs.Contains(PropertyOffset)))
		{
			Outputs.Add(PropertyOffset, InPtr);
		}
	}
}

FDataflowOutput* FDataflowNode::FindOutput(uint32 InGuidHash)
{
	for (TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		FDataflowOutput* const Con = Elem.Value;
		if (GetTypeHash(Con->GetGuid()) == InGuidHash)
		{
			return Con;
		}
	}
	return nullptr;
}


FDataflowOutput* FDataflowNode::FindOutput(FName InName)
{
	for (TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		FDataflowOutput* const Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(FName InName) const
{
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		if (Con->GetName().IsEqual(InName))
		{
			return Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(uint32 InGuidHash) const
{
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		if (GetTypeHash(Con->GetGuid()) == InGuidHash)
		{
			return Con;
		}
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(const void* Reference) const
{
	const int32 Key = GetConnectionOffsetFromReference(Reference);
	if (const FDataflowOutput* const* Con = Outputs.Find(Key))
	{
		check(*Con && (*Con)->RealAddress() == Reference);
		return *Con;
	}
	return nullptr;
}

FDataflowOutput* FDataflowNode::FindOutput(void* Reference)
{
	const int32 Key = GetConnectionOffsetFromReference(Reference);
	if (FDataflowOutput* const* Con = Outputs.Find(Key))
	{
		check(*Con && (*Con)->RealAddress() == Reference);
		return *Con;
	}
	return nullptr;
}

const FDataflowOutput* FDataflowNode::FindOutput(const FGuid& InGuid) const
{
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		if (Con->GetGuid() == InGuid)
		{
			return Con;
		}
	}
	return nullptr;
}

int32 FDataflowNode::NumOutputs() const
{
	return Outputs.Num();
}


TArray< FDataflowOutput* > FDataflowNode::GetOutputs() const
{
	TArray< FDataflowOutput* > Result;
	Outputs.GenerateValueArray(Result);
	return Result;
}


void FDataflowNode::ClearOutputs()
{
	for (TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		FDataflowOutput* const Con = Elem.Value;
		delete Con;
	}
	Outputs.Reset();
}

bool FDataflowNode::HasHideableOutputs() const
{
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		if (Con->GetCanHidePin())
		{
			return true;
		}
	}
	return false;
}

bool FDataflowNode::HasHiddenOutputs() const
{
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		if (Con->GetPinIsHidden())
		{
			return true;
		}
	}
	return false;
}

TArray<Dataflow::FPin> FDataflowNode::GetPins() const
{
	TArray<Dataflow::FPin> RetVal;
	for (const TPair<int32, FDataflowInput*>& Elem : Inputs)
	{
		const FDataflowInput* const Con = Elem.Value;
		RetVal.Add({ Dataflow::FPin::EDirection::INPUT,Con->GetType(), Con->GetName(), Con->GetPinIsHidden()});
	}
	for (const TPair<int32, FDataflowOutput*>& Elem : Outputs)
	{
		const FDataflowOutput* const Con = Elem.Value;
		RetVal.Add({ Dataflow::FPin::EDirection::OUTPUT,Con->GetType(), Con->GetName(), Con->GetPinIsHidden() });
	}
	return RetVal;
}

void FDataflowNode::UnregisterPinConnection(const Dataflow::FPin& Pin)
{
	if (Pin.Direction == Dataflow::FPin::EDirection::INPUT)
	{
		for (TMap< int32, FDataflowInput*>::TIterator Iter = Inputs.CreateIterator(); Iter; ++Iter)
		{
			FDataflowInput* Con = Iter.Value();
			if (Con->GetName().IsEqual(Pin.Name) && Con->GetType().IsEqual(Pin.Type))
			{
				Iter.RemoveCurrent();
				delete Con;

				// Invalidate graph as this input might have had connections
				Invalidate();
				break;
			}
		}
	}
	else if (Pin.Direction == Dataflow::FPin::EDirection::OUTPUT)
	{
		for (TMap<int32, FDataflowOutput*>::TIterator Iter = Outputs.CreateIterator(); Iter; ++Iter)
		{
			FDataflowOutput* Con = Iter.Value();
			if (Con->GetName().IsEqual(Pin.Name) && Con->GetType().IsEqual(Pin.Type))
			{
				Iter.RemoveCurrent();
				delete Con;

				// Invalidate graph as this input might have had connections
				Invalidate();
				break;
			}
		}
	}
}

void FDataflowNode::Invalidate(const Dataflow::FTimestamp& InModifiedTimestamp)
{
	if (LastModifiedTimestamp < InModifiedTimestamp)
	{
		LastModifiedTimestamp = InModifiedTimestamp;

		for (TPair<int32, FDataflowOutput*>& Elem : Outputs)
		{
			FDataflowOutput* const Con = Elem.Value;
			Con->Invalidate(InModifiedTimestamp);
		}

		OnInvalidate();

		OnNodeInvalidatedDelegate.Broadcast(this);
	}
}

const FProperty* FDataflowNode::FindProperty(const UStruct* Struct, const void* InProperty, const FName& PropertyName, TArray<const FProperty*>* OutPropertyChain) const
{
	const FProperty* Property = nullptr;
	for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
	{
		if (InProperty == PropertyIt.Value() && (PropertyName == NAME_None || PropertyName == PropertyIt.Key()->GetName()))
		{
			Property = PropertyIt.Key();
			if (OutPropertyChain)
			{
				PropertyIt.GetPropertyChain(*OutPropertyChain);
			}
			break;
		}
	}
	return Property;
}

const FProperty* FDataflowNode::FindProperty(const UStruct* Struct, const FName& PropertyFullName, TArray<const FProperty*>* OutPropertyChain) const
{
	const FProperty* Property = nullptr;
	for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
	{
		TArray<const FProperty*> PropertyChain;
		PropertyIt.GetPropertyChain(PropertyChain);
		if (GetPropertyFullName(PropertyChain) == PropertyFullName)
		{
			Property = PropertyIt.Key();
			if (OutPropertyChain)
			{
				*OutPropertyChain = MoveTemp(PropertyChain);
			}
			break;
		}
	}
	return Property;
}

uint32 FDataflowNode::GetPropertyOffset(const TArray<const FProperty*>& PropertyChain)
{
	uint32 Offset = 0;
	for (const FProperty* const Property : PropertyChain)
	{
		Offset += (uint32)Property->GetOffset_ForInternal();
	}
	return Offset;
}

uint32 FDataflowNode::GetPropertyOffset(const FName& PropertyFullName) const
{
	uint32 Offset = 0;
	if (const TUniquePtr<const FStructOnScope> ScriptOnStruct =
		TUniquePtr<FStructOnScope>(const_cast<FDataflowNode*>(this)->NewStructOnScope()))  // The mutable Struct Memory is not accessed here, allowing for the const_cast and keeping this method const
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			FindProperty(Struct, PropertyFullName, &PropertyChain);
			Offset = GetPropertyOffset(PropertyChain);
		}
	}
	return Offset;
}

uint32 FDataflowNode::GetConnectionOffsetFromReference(const void* Reference) const
{
	return (uint32)((size_t)Reference - (size_t)this);
}

FString FDataflowNode::GetPropertyFullNameString(const TConstArrayView<const FProperty*>& PropertyChain)
{
	FString PropertyFullName;
	for (const FProperty* const Property : PropertyChain)
	{
		const FString PropertyName = Property->GetName();
		PropertyFullName = PropertyFullName.IsEmpty() ?
			PropertyName :
			FString::Format(TEXT("{0}.{1}"), { PropertyName, PropertyFullName });
	}
	return PropertyFullName;
}

FName FDataflowNode::GetPropertyFullName(const TArray<const FProperty*>& PropertyChain)
{
	const FString PropertyFullName = GetPropertyFullNameString(TConstArrayView<const FProperty*>(PropertyChain));
	return FName(*PropertyFullName);
}

FText FDataflowNode::GetPropertyDisplayNameText(const TArray<const FProperty*>& PropertyChain)
{
#if WITH_EDITORONLY_DATA  // GetDisplayNameText() is only available if WITH_EDITORONLY_DATA
	FText PropertyText;
	for (const FProperty* const Property : PropertyChain)
	{
		static const FTextFormat TextFormat(NSLOCTEXT("DataflowNode", "PropertyDisplayNameTextConcatenator", "{0}.{1}"));
		PropertyText = PropertyText.IsEmpty() ?
			Property->GetDisplayNameText() :
			FText::Format(TextFormat, Property->GetDisplayNameText(), PropertyText);
	}
	return PropertyText;
#else
	return FText::FromName(GetPropertyFullName(PropertyChain));
#endif
}

bool FDataflowNode::InitConnectionParametersFromPropertyReference(const FStructOnScope& StructOnScope, const void* PropertyRef, const FName& PropertyName, Dataflow::FConnectionParameters& OutParams)
{
		if (const UStruct* Struct = StructOnScope.GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			const FProperty* const Property = FindProperty(Struct, PropertyRef, PropertyName, &PropertyChain);
			if (ensure(Property && PropertyChain.Num()))
			{
				FString ExtendedType;
				const FString CPPType = Property->GetCPPType(&ExtendedType);
				OutParams.Type = FName(CPPType + ExtendedType);
				OutParams.Name = GetPropertyFullName(PropertyChain);
				OutParams.Property = Property;
				OutParams.Owner = this;
				OutParams.Offset = GetConnectionOffsetFromReference(PropertyRef);
				check(OutParams.Offset == GetPropertyOffset(PropertyChain));
				return true;
			}
		}
	return false;
}

FDataflowInput* FDataflowNode::RegisterInputConnectionInternal(const void* InProperty, const FName& PropertyName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		Dataflow::FInputParameters InputParams;
		if (InitConnectionParametersFromPropertyReference(*ScriptOnStruct, InProperty, PropertyName, InputParams))
		{
			FDataflowInput* const Input = new FDataflowInput(InputParams);
			check(Input->RealAddress() == InProperty);
			AddInput(Input);
			check(FindInput(InProperty) == Input);
			return Input;
		}
	}
	return nullptr;
}

void FDataflowNode::UnregisterInputConnection(const void* InProperty, const FName& PropertyName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			const FProperty* const Property =
				FindProperty(Struct, InProperty, PropertyName, &PropertyChain);
			if (ensure(Property && PropertyChain.Num()))
			{
				const uint32 Offset = GetPropertyOffset(PropertyChain);
				if (FDataflowInput* const* const Input = Inputs.Find(Offset))
				{
					Inputs.Remove(Offset);
					delete *Input;

					// Invalidate graph as this input might have had connections
					Invalidate();
				}
			}
		}
	}
}

FDataflowOutput* FDataflowNode::RegisterOutputConnectionInternal(const void* InProperty, const void* Passthrough, const FName& PropertyName, const FName& PassthroughName)
{
	if (TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		Dataflow::FOutputParameters OutputParams;
		if (InitConnectionParametersFromPropertyReference(*ScriptOnStruct, InProperty, PropertyName, OutputParams))
		{
			FDataflowOutput* OutputConnection = new FDataflowOutput(OutputParams);
			check(OutputConnection->RealAddress() == InProperty);

			TArray<const FProperty*> PassthroughPropertyChain;
			if (FindProperty(ScriptOnStruct->GetStruct(), Passthrough, PassthroughName, &PassthroughPropertyChain))
			{
				const uint32 PassthroughOffset = GetConnectionOffsetFromReference(Passthrough);
				check(PassthroughOffset == GetPropertyOffset(PassthroughPropertyChain));
				OutputConnection->SetPassthroughOffset(PassthroughOffset);
			}

			AddOutput(OutputConnection);
			check(FindOutput(InProperty) == OutputConnection);
			return OutputConnection;
		}
	}
	return nullptr;
}

uint32 FDataflowNode::GetValueHash()
{
	//UE_LOG(LogChaos, Warning, TEXT("%s"), *GetName().ToString())

	uint32 Hash = 0;
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
			{
				if (const FProperty* const Property = PropertyIt.Key())
				{
					if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
					{
						//
						// Note : [CacheContextPropertySupport]
						// 
						// Some UPROPERTIES do not support hash values.For example, FFilePath, is a struct 
						// that is not defined using USTRUCT, and does not support the GetTypeValue() function.
						// These types of attributes need to return a Zero(0) hash, to indicate that the Hash 
						// is not supported.To add property hashing support, add GetTypeValue to the properties 
						// supporting USTRUCT(See Class.h  UScriptStruct::GetStructTypeHash)
						// 
						if (!StructProperty->Struct) return 0;
						if (!StructProperty->Struct->GetCppStructOps()) return 0;
					}

					if (Property->PropertyFlags & CPF_HasGetValueTypeHash)
					{
						// uint32 CrcHash = FCrc::MemCrc32(PropertyIt.Value(), Property->ElementSize);
						// UE_LOG(LogChaos, Warning, TEXT("( %lu \t%s"), (unsigned long)CrcHash, *Property->GetName())

						if (Property->PropertyFlags & CPF_TObjectPtr)
						{
							// @todo(dataflow) : Do something about TObjectPtr<T>
						}
						else
						{
							Hash = HashCombine(Hash, Property->GetValueTypeHash(PropertyIt.Value()));
						}
					}
				}
			}
		}
	}
	return Hash;
}

void FDataflowNode::ValidateProperties()
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, this); PropertyIt; ++PropertyIt)
			{
				if (const FProperty* const Property = PropertyIt.Key())
				{
					if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
					{
						if (!StructProperty->Struct || !StructProperty->Struct->GetCppStructOps())
						{
							// See Note : [CacheContextPropertySupport]
							FString StructPropertyName;
							StructProperty->GetName(StructPropertyName);
							UE_LOG(LogChaos, Warning, 
								TEXT("Dataflow: Context caching disable for graphs with node '%s' due to non-hashed UPROPERTY '%s'."), 
								*GetName().ToString(), *StructPropertyName)
						}
					}
				}
			}
		}
	}
}

bool FDataflowNode::ValidateConnections()
{
	bHasValidConnections = true;
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			for (FPropertyValueIterator PropertyIt(FProperty::StaticClass(), Struct, ScriptOnStruct->GetStructMemory()); PropertyIt; ++PropertyIt)
			{
				const FProperty* const Property = PropertyIt.Key();
				check(Property);

				if (Property->HasMetaData(FDataflowNode::DataflowInput))
				{
					TArray<const FProperty*> PropertyChain;
					PropertyIt.GetPropertyChain(PropertyChain);
					const FName PropName(GetPropertyFullName(PropertyChain));

					if (!FindInput(PropName))
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterInputConnection in constructor for (%s:%s)"), *GetName().ToString(), *PropName.ToString())
							bHasValidConnections = false;
					}
				}
				if (Property->HasMetaData(FDataflowNode::DataflowOutput))
				{
					TArray<const FProperty*> PropertyChain;
					PropertyIt.GetPropertyChain(PropertyChain);
					const FName PropName(GetPropertyFullName(PropertyChain));

					const FDataflowOutput* const OutputConnection = FindOutput(PropName);
					if(!OutputConnection)
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing dataflow RegisterOutputConnection in constructor for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bHasValidConnections = false;
					}
					// If OutputConnection is valid, validate passthrough connections if they exist
					else if (const FString* PassthroughName = Property->FindMetaData(FDataflowNode::DataflowPassthrough))
					{
						void* PassthroughConnectionAddress = OutputConnection->GetPassthroughRealAddress();
						if (PassthroughConnectionAddress == nullptr)
						{
							UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough registration for (%s:%s)"), *GetName().ToString(), *PropName.ToString());
							bHasValidConnections = false;
						}

						// Assume passthrough name is relative to current property name.
						FString FullPassthroughName;
						if (PropertyChain.Num() <= 1)
						{
							FullPassthroughName = *PassthroughName;
						}
						else
						{
							FullPassthroughName = FString::Format(TEXT("{0}.{1}"), { GetPropertyFullNameString(TConstArrayView<const FProperty*>(&PropertyChain[1], PropertyChain.Num() - 1)), *PassthroughName});
						}

						const FDataflowInput* PassthroughConnectionInput = FindInput(FName(FullPassthroughName));
						const FDataflowInput* PassthroughConnectionInputFromArg = FindInput(PassthroughConnectionAddress);

						if(PassthroughConnectionInputFromArg != PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Mismatch in declared and registered DataflowPassthrough connection; (%s:%s vs %s)"), *GetName().ToString(), *PropName.ToString(), *PassthroughConnectionInputFromArg->GetName().ToString());
							bHasValidConnections = false;
						}

						if(!PassthroughConnectionInput)
						{
							UE_LOG(LogChaos, Warning, TEXT("Incorrect DataflowPassthrough Connection set for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bHasValidConnections = false;
						}

						else if(OutputConnection->GetType() != PassthroughConnectionInput->GetType())
						{
							UE_LOG(LogChaos, Warning, TEXT("DataflowPassthrough connection types mismatch for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
							bHasValidConnections = false;
						}
					}
					else if(OutputConnection->GetPassthroughRealAddress()) 
					{
						UE_LOG(LogChaos, Warning, TEXT("Missing DataflowPassthrough declaration for (%s:%s)"), *GetName().ToString(),*PropName.ToString());
						bHasValidConnections = false;
					}
				}
			}
		}
	}
#endif
	return bHasValidConnections;
}

FString FDataflowNode::GetToolTip()
{
	Dataflow::FFactoryParameters FactoryParameters = ::Dataflow::FNodeFactory::GetInstance()->GetParameters(GetType());

	return FactoryParameters.ToolTip;
}

FText FDataflowNode::GetPinDisplayName(const FName& PropertyFullName)
{
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			TArray<const FProperty*> PropertyChain;
			if (FindProperty(Struct, PropertyFullName, &PropertyChain))
			{
				return GetPropertyDisplayNameText(PropertyChain);
			}
		}
	}

	return FText();
}

FString FDataflowNode::GetPinToolTip(const FName& PropertyFullName)
{
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			if (const FProperty* const Property = FindProperty(Struct, PropertyFullName))
			{
				if (Property->HasMetaData(TEXT("Tooltip")))
				{
					const FString ToolTipStr = Property->GetToolTipText(true).ToString();
					if (ToolTipStr.Len() > 0)
					{
						TArray<FString> OutArr;
						const int32 NumElems = ToolTipStr.ParseIntoArray(OutArr, TEXT(":\r\n"));

						if (NumElems == 2)
						{
							return OutArr[1];  // Return tooltip meta text
						}
						else if (NumElems == 1)
						{
							return OutArr[0];  // Return doc comment
						}
					}
				}
			}
		}
	}
#endif

	return "";
}

TArray<FString> FDataflowNode::GetPinMetaData(const FName& PropertyFullName)
{
#if WITH_EDITORONLY_DATA
	if (const TUniquePtr<FStructOnScope> ScriptOnStruct = TUniquePtr<FStructOnScope>(NewStructOnScope()))
	{
		if (const UStruct* const Struct = ScriptOnStruct->GetStruct())
		{
			if (const FProperty* const Property = FindProperty(Struct, PropertyFullName))
			{
				TArray<FString> MetaDataStrArr;
				if (Property->HasMetaData(FDataflowNode::DataflowPassthrough))
				{
					MetaDataStrArr.Add("Passthrough");
				}
				if (Property->HasMetaData(FDataflowNode::DataflowIntrinsic))
				{
					MetaDataStrArr.Add("Intrinsic");
				}

				return MetaDataStrArr;
			}
		}
	}
#endif

	return TArray<FString>();
}

void FDataflowNode::CopyNodeProperties(const TSharedPtr<FDataflowNode> CopyFromDataflowNode)
{
	TArray<uint8> NodeData;

	FObjectWriter ArWriter(NodeData);
	CopyFromDataflowNode->SerializeInternal(ArWriter);

	FObjectReader ArReader(NodeData);
	this->SerializeInternal(ArReader);
}


void FDataflowNode::ForwardInput(Dataflow::FContext& Context, const void* InputReference, const void* Reference) const
{
	if (const FDataflowOutput* Output = FindOutput(Reference))
	{
		if (const FDataflowInput* Input = FindInput(InputReference))
		{
			// we need to pull the value first so the upstream of the graph evaluate 
			Input->PullValue(Context);
			Output->ForwardInput(InputReference, Context);
		}
		else
		{
			checkfSlow(false, TEXT("This input could not be found within this node, check this has been properly registered in the node constructor"));
		}
	}
	else
	{
		checkfSlow(false, TEXT("This output could not be found within this node, check this has been properly registered in the node constructor"));
	}
}

bool FDataflowNode::TrySetConnectionType(FDataflowConnection* Connection, FName NewType)
{
	if (Connection)
	{
		if (Connection->IsAnyType() && Connection->GetType() != NewType && !FDataflowConnection::IsAnyType(NewType))
		{
			Connection->SetConcreteType(NewType);
			if (Connection->GetDirection() == Dataflow::FPin::EDirection::INPUT)
			{
				OnInputTypeChanged((FDataflowInput*)Connection);
			}
			if (Connection->GetDirection() == Dataflow::FPin::EDirection::OUTPUT)
			{
				OnOutputTypeChanged((FDataflowOutput*)Connection);
			}
			return true;
		}
	}
	return false;
}

bool FDataflowNode::SetInputConcreteType(void* InputReference, FName NewType)
{
	if (FDataflowInput* Input = FindInput(InputReference))
	{
		if (Input->GetType() != NewType)
		{
			return Input->SetConcreteType(NewType);
		}
	}
	return false;
}

bool FDataflowNode::SetOutputConcreteType(void* OutputReference, FName NewType)
{
	if (FDataflowOutput* Output = FindOutput(OutputReference))
	{
		if (Output->GetType() != NewType)
		{
			return Output->SetConcreteType(NewType);
		}
	}
	return false;
}





