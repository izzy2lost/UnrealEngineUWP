// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserPropertyAccess.h"

#include "IObjectChooser.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/UnrealType.h"
#include "Logging/LogMacros.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "IPropertyAccessEditor.h"
#endif

#define LOCTEXT_NAMESPACE "ChooserPropertyAccess"
		
FName FChooserPropertyBinding::GetUniqueId() const
{
	FString Result;
	bool First = true;
	for (const auto& Element : PropertyBindingChain)
	{
		if (First)
		{
			First = false;
		}
		else
		{
			Result += TEXT(".");
		}

		Result += Element.ToString();
	}

	return FName(Result);
}


struct FCompiledBindingCacheId
{
	FName BindingPath;
	const UStruct* Type;
	
	bool operator == (const FCompiledBindingCacheId& Other) const
	{
		 return Type == Other.Type
		  && BindingPath == Other.BindingPath;
	}
};

FORCEINLINE uint32 GetTypeHash(const FCompiledBindingCacheId& Id)
{
	return HashCombineFast(GetTypeHash(Id.Type), GetTypeHash(Id.BindingPath));
}

TMap<FCompiledBindingCacheId, TWeakPtr<UE::Chooser::FCompiledBinding>> CompiledBindingCache;
FCriticalSection CompiledBindingCacheLock;


void FChooserPropertyBinding::Compile(IHasContextClass* Owner, bool bForce)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompilePropertyChain);

#if WITH_EDITORONLY_DATA
	CompileMessage = FText();
#endif
	
	TConstArrayView<FInstancedStruct> ContextData = Owner->GetContextData();

	int CompiledBindingSerialNumber = 0;
	
	if (PropertyBindingChain.IsEmpty() || !ContextData.IsValidIndex(ContextIndex))
	{
#if WITH_EDITORONLY_DATA
		CompileMessage = LOCTEXT("No Property Bound", "No Property Bound");
#endif
		CompiledBinding = nullptr;
		return;
	}

	const UStruct* StructType = nullptr;
	if (ContextData.IsValidIndex(ContextIndex))
	{
		if (const FContextObjectTypeClass* ClassContext = ContextData[ContextIndex].GetPtr<FContextObjectTypeClass>())
		{
			StructType = ClassContext->Class;
		}
		else if (const FContextObjectTypeStruct* StructContext = ContextData[ContextIndex].GetPtr<FContextObjectTypeStruct>())
		{
			StructType = StructContext->Struct;
		}
	}

	if(StructType == nullptr)
	{
#if WITH_EDITORONLY_DATA
		CompileMessage = FText::Format(LOCTEXT("No valid struct", "No valid Context Object/Struct at index: {0}"), FText::FromString(FString::FromInt(ContextIndex)));
#endif
		CompiledBinding = nullptr;
		return;
	}
		
	FCompiledBindingCacheId Id;
	Id.Type = StructType;
	Id.BindingPath = GetUniqueId();

	{
		FScopeLock Lock(&CompiledBindingCacheLock);
		if (TWeakPtr<UE::Chooser::FCompiledBinding>* Binding =  CompiledBindingCache.Find(Id))
		{
			if (Binding->IsValid())
			{
				bool bUseCachedBinding = true;
#if WITH_EDITOR
				if (bForce && CompiledBinding)
				{
					// we've triggered a recompile due to dependency changes.
					// if the one in the cache has a higher serial number, then it has already been recompiled by some other reference, so use the cached one.
					// otherwise, remove the currently cached one, recompile, and increment the serial number
					if (Binding->Pin()->SerialNumber <= CompiledBinding->SerialNumber)
					{
						bUseCachedBinding = false;
						CompiledBindingSerialNumber = CompiledBinding->SerialNumber + 1;
						CompiledBindingCache.Remove(Id);
					}
				}
#endif

				if (bUseCachedBinding)
				{
					CompiledBinding = Binding->Pin();

#if WITH_EDITOR
					for (const UStruct* Dependency : CompiledBinding->Dependencies)
					{
						Owner->AddCompileDependency(Dependency);
					}
#endif
					return;
				}
			}
		}
	}

	TSharedPtr<UE::Chooser::FCompiledBinding> NewCompiledBinding = MakeShared<UE::Chooser::FCompiledBinding>();
	UE::Chooser::FCompiledBinding& OutCompiledBinding = *NewCompiledBinding.Get();

	OutCompiledBinding.TargetType = StructType;
	OutCompiledBinding.CompiledChain.SetNum(0);
	OutCompiledBinding.ContextIndex = ContextIndex;
	
	const int PropertyChainLength = PropertyBindingChain.Num();
	int CurrentOffset = 0;
	for(int PropertyChainIndex = 0; PropertyChainIndex < PropertyChainLength - 1; PropertyChainIndex++)
	{
#if WITH_EDITOR
		Owner->AddCompileDependency(StructType);
		OutCompiledBinding.Dependencies.AddUnique(StructType);
#endif

		bool bFound = false;
		if (const FStructProperty* StructProperty = FindFProperty<FStructProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
		{
			bFound = true;
			// accumulate offsets in structs
			CurrentOffset += StructProperty->GetOffset_ForInternal();
			StructType = StructProperty->Struct;
		}
		else if (const FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
		{
			bFound = true;
			// when we hit an  object reference, create a chain element with the current offset
			CurrentOffset += ObjectProperty->GetOffset_ForInternal();
			OutCompiledBinding.CompiledChain.Add(UE::Chooser::FCompiledBindingElement(CurrentOffset));
			StructType = ObjectProperty->PropertyClass;
			// clear the offset, to start accumulating again relative to the new object base
			CurrentOffset = 0;
		}
		// check if it's a member function
		else if (const UClass* ClassType = Cast<const UClass>(StructType))
		{
			if (UFunction* Function = ClassType->FindFunctionByName(PropertyBindingChain[PropertyChainIndex]))
			{
				bFound = true;
				ensure(CurrentOffset == 0);
				OutCompiledBinding.CompiledChain.Add(UE::Chooser::FCompiledBindingElement(Function));
				StructType = CastField<FObjectProperty>(Function->GetReturnProperty())->PropertyClass;
			}
		}

		if (!bFound)
		{
#if WITH_EDITORONLY_DATA
			CompileMessage = FText::Format(LOCTEXT("Property Not Found", "Property/Function: {0} not Found on Class/Struct: {1}"), FText::FromName(PropertyBindingChain[PropertyChainIndex]), StructType->GetDisplayNameText());
#endif
			CompiledBinding = nullptr;
			return;
		}
	}
	
	#if WITH_EDITOR
   	Owner->AddCompileDependency(StructType);
	OutCompiledBinding.Dependencies.AddUnique(StructType);
    #endif

	bool bFound = false;
	if (const FProperty* BaseProperty = FindFProperty<FProperty>(StructType, PropertyBindingChain.Last()))
	{
		bFound = true;
		
		// last element should be the actual property - add it's offset to whatever was accumulated from struct offsets
		CurrentOffset += BaseProperty->GetOffset_ForInternal();
		OutCompiledBinding.CompiledChain.Add(UE::Chooser::FCompiledBindingElement(CurrentOffset));

		if (BaseProperty->IsA<FFloatProperty>())
		{
			OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::FLOAT;
		}
		else if (BaseProperty->IsA<FDoubleProperty>())
		{
			OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::DOUBLE;
		}
		else if (BaseProperty->IsA<FIntProperty>())
		{
			OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::INT32;
		}
	}
	else
	{
		// handle function calls 
		if (const UClass* ClassType = Cast<const UClass>(StructType))
		{
			if (UFunction* Function = ClassType->FindFunctionByName(PropertyBindingChain.Last()))
			{
				bFound = true;
				
				const FProperty* ReturnProperty = Function->GetReturnProperty();
				OutCompiledBinding.CompiledChain.Add(UE::Chooser::FCompiledBindingElement(Function));
				if (ReturnProperty->IsA<FFloatProperty>())
				{
					OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::FLOAT;
				}
				else if (ReturnProperty->IsA<FDoubleProperty>())
				{
					OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::DOUBLE;
				}
				else if (ReturnProperty->IsA<FIntProperty>())
				{
					OutCompiledBinding.PropertyType = UE::Chooser::EPropertyNumericalType::INT32;
				}
			}
		 }
	}

	if (bFound)
	{
		FScopeLock Lock(&CompiledBindingCacheLock);
#if WITH_EDITORONLY_DATA
		NewCompiledBinding->SerialNumber = CompiledBindingSerialNumber;
#endif
		CompiledBindingCache.Add(Id, NewCompiledBinding);
		CompiledBinding = NewCompiledBinding;
	}
	else
	{
#if WITH_EDITORONLY_DATA
		CompileMessage = FText::Format(LOCTEXT("Property Not Found", "Property/Function: {0} not Found on Class/Struct: {1}"), FText::FromName(PropertyBindingChain.Last()), StructType->GetDisplayNameText());
#endif
		CompiledBinding = nullptr;
 	}
}


namespace UE::Chooser
{

	uint8* ResolveCompiledPropertyChain(FChooserEvaluationContext& Context, const FCompiledBinding& CompiledBinding)
	{
		uint8* Result = nullptr;
		const UStruct* InputType = nullptr;

		if(!Context.Params.IsValidIndex(CompiledBinding.ContextIndex))
		{
			UE_LOG(LogChooser, Warning, TEXT("Invalid Index {%d} while resolving compiled property chain."), CompiledBinding.ContextIndex);
			return nullptr;
		}
		
		if (FChooserEvaluationInputObject* ObjectInput = Context.Params[CompiledBinding.ContextIndex].GetMutablePtr<FChooserEvaluationInputObject>())
		{
			UObject* Object = ObjectInput->Object.Get();
			Result = reinterpret_cast<uint8*>(Object);
			if (Object)
			{
				InputType = Object->GetClass();
			}
		}
		else
		{
			Result = Context.Params[CompiledBinding.ContextIndex].GetMutableMemory();
			InputType = Context.Params[CompiledBinding.ContextIndex].GetScriptStruct();
		}

		if (Result == nullptr || InputType == nullptr)
		{
			return nullptr;
		}

		if (!InputType->IsChildOf(CompiledBinding.TargetType))
		{
			UE_LOG(LogChooser, Warning, TEXT("Property Binding compiled for type: {%s} is being evaluated on incompatible type: {%s}."), ToCStr(CompiledBinding.TargetType->GetName()), ToCStr(InputType->GetName()));
			return nullptr;
		}
		
		for (int i = 0; i<CompiledBinding.CompiledChain.Num() - 1; i++)
		{
			if (Result)
			{
				const FCompiledBindingElement& Element = CompiledBinding.CompiledChain[i];
				if (!Element.bIsFunction)
				{
					Result = *reinterpret_cast<uint8**>(Result + CompiledBinding.CompiledChain[i].Offset);
				}
				else
				{
					UObject* Object = reinterpret_cast<UObject*>(Result);
					if (Element.Function->IsNative())
					{
						FFrame Stack(Object, Element.Function, nullptr, nullptr, Element.Function->ChildProperties);
						Element.Function->Invoke(Object, Stack, &Result);
					}
					else
					{
						Object->ProcessEvent(Element.Function, &Result);
					}
				}
			}
		}

		return Result;
	}
	
	bool ResolvePropertyChain(FChooserEvaluationContext& Context, const FChooserPropertyBinding& PropertyBinding, const void*& OutContainer, const UStruct*& OutStructType)
	{
		if (Context.Params.IsValidIndex(PropertyBinding.ContextIndex))
		{
			if (FChooserEvaluationInputObject* ObjectParam = Context.Params[PropertyBinding.ContextIndex].GetMutablePtr<FChooserEvaluationInputObject>())
			{
				OutContainer = ObjectParam->Object;
				if (OutContainer)
				{
					OutStructType = ObjectParam->Object->GetClass();
				}
				else
				{
					OutStructType = nullptr;
				}
			}
			else
			{
				OutContainer = Context.Params[PropertyBinding.ContextIndex].GetMutableMemory();
				OutStructType = Context.Params[PropertyBinding.ContextIndex].GetScriptStruct();
			}
			
			if (OutContainer == nullptr || OutStructType == nullptr)
			{
				return false;
			}

			return ResolvePropertyChain(OutContainer, OutStructType, PropertyBinding.PropertyBindingChain);
		}

		return false;
	}
	
	bool ResolvePropertyChain(const void*& Container, const UStruct*& StructType, const TArray<FName>& PropertyBindingChain)
	{
		if (PropertyBindingChain.Num() == 0)
		{
			return false;
		}
	
		const int PropertyChainLength = PropertyBindingChain.Num();
		for(int PropertyChainIndex = 0; PropertyChainIndex < PropertyChainLength - 1; PropertyChainIndex++)
		{
			if (const FStructProperty* StructProperty = FindFProperty<FStructProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
			{
				StructType = StructProperty->Struct;
				Container = StructProperty->ContainerPtrToValuePtr<void>(Container);
			}
			else if (const FObjectProperty* ObjectProperty = FindFProperty<FObjectProperty>(StructType, PropertyBindingChain[PropertyChainIndex]))
			{
				StructType = ObjectProperty->PropertyClass;
				Container = *ObjectProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Container);
				if (Container == nullptr)
				{
					return false;
				}
			}
			else
			{
				// check if it's a member function
				if (const UClass* ClassType = Cast<const UClass>(StructType))
				{
					if (UFunction* Function = ClassType->FindFunctionByName(PropertyBindingChain[PropertyChainIndex]))
					{
						UObject* Object = reinterpret_cast<UObject*>(const_cast<void*>(Container));
						if (Function->IsNative())
						{
							FFrame Stack(Object, Function, nullptr, nullptr, Function->ChildProperties);
							Function->Invoke(Object, Stack, &Container);
						}
						else
						{
							Object->ProcessEvent(Function, &Container);
						}
						
						if (Container == nullptr)
						{
							return false;
						}
						else
						{
							StructType = reinterpret_cast<UObject*>(const_cast<void*>(Container))->GetClass();
						}
					}
					else
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
		}
	
		return true;
	}
	
#if WITH_EDITOR
	void CopyPropertyChain(const TArray<FBindingChainElement>& InBindingChain, FChooserPropertyBinding& OutPropertyBinding)
	{
		OutPropertyBinding.PropertyBindingChain.Empty();

		if (InBindingChain.Num() == 0)
		{
			OutPropertyBinding.ContextIndex = -1;
		}
		else
		{
			OutPropertyBinding.ContextIndex = InBindingChain[0].ArrayIndex;
		}

		for (int32 i = 1; i < InBindingChain.Num(); ++i)
		{
			OutPropertyBinding.PropertyBindingChain.Emplace(InBindingChain[i].Field.GetFName());
		}
	}
#endif

	
}

#undef LOCTEXT_NAMESPACE
