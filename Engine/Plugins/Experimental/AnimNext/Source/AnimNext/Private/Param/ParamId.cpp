// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamId.h"

#include "AnimNextConfig.h"
#include "DataRegistry.h"
#include "ReferencePose.h"
#include "Misc/ScopeRWLock.h"
#include "HAL/ThreadSingleton.h"
#include "Param/AnimNextObjectAdapterConfig.h"
#include "Param/ParamAdapter.h"
#include "Param/ParamDefinition.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Param/Params.h"
#include "Param/ParamStack.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Engine/World.h"
#include "Graph/AnimNext_LODPose.h"
#include "Component/AnimNextMeshComponent.h"
#include "AnimNextStats.h"

DEFINE_STAT(STAT_AnimNext_ParamIdLock)

#define LOCTEXT_NAMESPACE "ParamId"

namespace UE::AnimNext
{

// TODO: Expand scratch buffer system to become a general cache of adapter values.
// The assumption being that if we are grabbing gameplay-driven values, they do not vary over 
// the course of a schedule unless the 'tick' of the values (e.g. a component's tick function) runs
// during that time, at which point the values are reset and lazily updated
// We should cache these values in a FInstancedPropertyBag, per adapted object type (e.g. all 
// adapters for skel mesh components), per thread. This also allows us better handling of complex 
// types, object lifetimes and GC.

// TODO: Investigate allowing function libraries as adapters
	
struct FParamIdGlobalData
{
	// Hash table used to index the ParamAdapterData 
	FHashTable HashTable;
	TArray<FParamAdapter> ParamAdapterData;
	uint32 SerialNumber = 0;
	uint32 ResultBufferSize = 0;
};

static FParamIdGlobalData GParamIdGlobalData;

// Buffers to hold values for function-wrapped parameter adapters 
thread_local TArray<uint8> ParamAdapterResultBuffer;
thread_local uint32 ParamAdapterBufferSerialNumber = 0;
	
void FParamId::Init()
{
	RefreshAdapters();
}

void FParamId::Destroy()
{
	GParamIdGlobalData.HashTable.Clear();
	GParamIdGlobalData.ParamAdapterData.Empty();
}

void FParamId::RefreshAdapters()
{
	ResetAdapters();
	RegisterBuiltInAdapters();
	RefreshConfigAdapters();
}

void FParamId::ResetAdapters()
{
	// Reset existing IDs adapter data
	for(FParamAdapter& ParamAdapterData : GParamIdGlobalData.ParamAdapterData)
	{
		// Unregister this param as built-in
		FParams::UnregisterBuiltInParameter(ParamAdapterData.Definition.GetName());
	}
	
	GParamIdGlobalData.HashTable.Clear();
	GParamIdGlobalData.ParamAdapterData.Empty();
	GParamIdGlobalData.ResultBufferSize = 0;
}

template<typename ReturnType, bool bRequiresTempStorage>
static void RegisterBuiltInAdapter(FName InName, FText InTooltipText, FObjectAdapterFunction&& InAdapterFunction, EParamDefinitionFlags InDefinitionFlags = EParamDefinitionFlags::None)
{
	FParamDefinition Definition(InName, FAnimNextParamType::GetType<ReturnType>(), InTooltipText, InDefinitionFlags);
	uint32 Index = GParamIdGlobalData.ParamAdapterData.Emplace(FParamAdapter(MoveTemp(Definition), MoveTemp(InAdapterFunction)));
	GParamIdGlobalData.HashTable.Add(Definition.GetId().GetHash(), Index);
	FParams::RegisterBuiltInParameter(Definition);

	if constexpr (bRequiresTempStorage)
	{
		FParamAdapter& ParamAdapterData = GParamIdGlobalData.ParamAdapterData[Index];

		// Calc buffer size for the result
		ParamAdapterData.BufferOffset = Align(GParamIdGlobalData.ResultBufferSize, alignof(ReturnType));
		GParamIdGlobalData.ResultBufferSize += sizeof(ReturnType);
	}
}

static void RegisterBuiltInObjectAdapter(FName InName, UObject* InObject, FObjectAdapterFunction&& InAdapterFunction)
{
	FParamDefinition Definition(InName, InObject);
	uint32 Index = GParamIdGlobalData.ParamAdapterData.Emplace(FParamAdapter(MoveTemp(Definition), MoveTemp(InAdapterFunction)));
	GParamIdGlobalData.HashTable.Add(Definition.GetId().GetHash(), Index);
	FParams::RegisterBuiltInParameter(Definition);

	FParamAdapter& ParamAdapterData = GParamIdGlobalData.ParamAdapterData[Index];

	// Calc buffer size for the result
	ParamAdapterData.BufferOffset = Align(GParamIdGlobalData.ResultBufferSize, alignof(UObject*));
	GParamIdGlobalData.ResultBufferSize += sizeof(UObject*);
}

static void RegisterBuiltInPropertyAdapter(FName InName, const FProperty* InProperty, FObjectAdapterFunction&& InAdapterFunction)
{
	FParamDefinition Definition(InName, InProperty);
	uint32 Index = GParamIdGlobalData.ParamAdapterData.Emplace(FParamAdapter(MoveTemp(Definition), MoveTemp(InAdapterFunction)));
	GParamIdGlobalData.HashTable.Add(Definition.GetId().GetHash(), Index);
	FParams::RegisterBuiltInParameter(Definition);
}

static void RegisterBuiltInFunctionAdapter(FName InName, UFunction* InFunction, FObjectAdapterFunction&& InAdapterFunction)
{
	FParamDefinition Definition(InName, InFunction);
	uint32 Index = GParamIdGlobalData.ParamAdapterData.Emplace(FParamAdapter(MoveTemp(Definition), MoveTemp(InAdapterFunction)));
	GParamIdGlobalData.HashTable.Add(Definition.GetId().GetHash(), Index);
	FParams::RegisterBuiltInParameter(Definition);

	FParamAdapter& ParamAdapterData = GParamIdGlobalData.ParamAdapterData[Index];

	// Calc buffer size for the result
	const FProperty* ReturnProperty = InFunction->GetReturnProperty();
	ParamAdapterData.BufferOffset = Align(GParamIdGlobalData.ResultBufferSize, ReturnProperty->GetMinAlignment());
	GParamIdGlobalData.ResultBufferSize += ReturnProperty->GetSize();
}
	
void FParamId::RegisterBuiltInAdapters()
{
	// Register world delta time
	{
		FObjectAdapterFunction DeltaTimeFunction = [](UObject* InContextObject, FParamId InId) -> uint8*
		{
			if (UWorld* World = InContextObject->GetWorld())
			{
				float* ReturnValue = GetAdapterReturnValue<float>(InId);
				*ReturnValue = World->GetDeltaSeconds();
				return reinterpret_cast<uint8*>(ReturnValue);
			}
			return nullptr;
		};

		RegisterBuiltInAdapter<float, true>("UE_Frame_DeltaTime", LOCTEXT("DeltaTimeTooltip", "The current delta time"), MoveTemp(DeltaTimeFunction));
	}
	
	// Register physics tick function
	{
		FObjectAdapterFunction PhysicsTickFunctionFunction = [](UObject* InContextObject, FParamId InId) -> uint8*
		{
			if (UWorld* World = InContextObject->GetWorld())
			{
				return reinterpret_cast<uint8*>(&World->EndPhysicsTickFunction);
			}
			return nullptr;
		};

		RegisterBuiltInAdapter<FEndPhysicsTickFunction, false>("UE_Physics_Tick", LOCTEXT("PhysicsTickTooltip", "The tick function of the physics scene"), MoveTemp(PhysicsTickFunctionFunction));
	}

	// Register ref pose accessor
	{
		FObjectAdapterFunction ReferencePoseFunction = [](UObject* InContextObject, FParamId InId) -> uint8*
		{
			static FParamId ComponentParam("UE_AnimNextMeshComponent");
			if (const TObjectPtr<UAnimNextMeshComponent>* SkeletalMeshComponent = FParamStack::Get().GetParamPtr<TObjectPtr<UAnimNextMeshComponent>>(ComponentParam))
			{
				if(*SkeletalMeshComponent)
				{
					FDataHandle RefPoseHandle = FDataRegistry::Get()->GetOrGenerateReferencePose(SkeletalMeshComponent->Get());
					const FReferencePose& RefPose = RefPoseHandle.GetRef<FReferencePose>();
					FAnimNextGraphReferencePose* ReturnValue = GetAdapterReturnValue<FAnimNextGraphReferencePose>(InId);
					*ReturnValue = FAnimNextGraphReferencePose(&RefPose);
					return reinterpret_cast<uint8*>(ReturnValue);
				}
			}
			return nullptr;
		};

		RegisterBuiltInAdapter<FAnimNextGraphReferencePose, true>("UE_AnimNextMeshComponent_ReferencePose", LOCTEXT("ReferencePoseTooltip", "The reference pose of the skeletal mesh component"), MoveTemp(ReferencePoseFunction));
	}
}

void FParamId::RefreshConfigAdapters()
{
	for(const FAnimNextObjectAdapterConfig& AdapterConfig : GetDefault<UAnimNextConfig>()->ExposedClasses)
	{
		if(AdapterConfig.Class != nullptr && AdapterConfig.RootParameter != NAME_None)
		{
			// Ensure we can use the type as a pin in RigVM graphs 
			FRigVMRegistry::Get().RegisterObjectTypes( {{ AdapterConfig.Class, FRigVMRegistry::ERegisterObjectOperation::Class } });

			const FString RootParam(AdapterConfig.RootParameter.ToString());
			const FName RootParamName(*RootParam);
			const FParamId RootParamId(RootParamName);

			// TODO: add registration mechanism to allow for more than actor/component types
			FObjectAdapterFunction RootFunction;
			if(AdapterConfig.Class->IsChildOf(UActorComponent::StaticClass()))
			{
				RootFunction = [Class = AdapterConfig.Class](UObject* InContextObject, FParamId InId) -> uint8*
				{
					if (const UActorComponent* ContextComponent = Cast<UActorComponent>(InContextObject))
					{
						if (AActor* Actor = ContextComponent->GetOwner())
						{
							UActorComponent* ActorComponent = Actor->FindComponentByClass(Class.Get());
							UActorComponent** ReturnValue = GetAdapterReturnValue<UActorComponent*>(InId);
							*ReturnValue = ActorComponent;
							return reinterpret_cast<uint8*>(ReturnValue);
						}
					}

					return nullptr;
				};
			}
			else if(AdapterConfig.Class->IsChildOf(AActor::StaticClass()))
			{
				RootFunction = [Class = AdapterConfig.Class.Get()](UObject* InContextObject, FParamId InId) -> uint8*
				{
					if (const UActorComponent* ContextComponent = Cast<UActorComponent>(InContextObject))
					{
						// Actors cannot be redirected by name
						if(AActor* Actor = ContextComponent->GetOwner())
						{
							if(Actor->GetClass()->IsChildOf(Class))
							{
								AActor** ReturnValue = GetAdapterReturnValue<AActor*>(InId);
								*ReturnValue = Actor;
								return reinterpret_cast<uint8*>(ReturnValue);
							}
						}
					}

					return nullptr;
				};
			}

			// TODO: Add runtime mechanism to prevent accessing object data:
			// - When an object's tick function is not bound in a schedule (and when we have non-linear schedules,
			//   cannot be concurrent)
			// - When an object's tick function is not concurrently running (if necessary - should be mitigated by the above)
			
			// Add root param
			RegisterBuiltInObjectAdapter(RootParamName, AdapterConfig.Class->GetDefaultObject(), MoveTemp(RootFunction));
			
			// Add object's BP-exposed properties
			if(AdapterConfig.FilterType != EAnimNextObjectAdapterFilterType::AllowOnlyFunctions)
			{
				auto IsNameAllowed = [&AdapterConfig](FName InName)
				{
					return   AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowOnlyProperties ||
							 AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowAllPropertiesAndFunctions ||
							(AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowList && AdapterConfig.FilteredFields.Contains(InName)) ||
							(AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::DenyList && !AdapterConfig.FilteredFields.Contains(InName));
				};

				for(TFieldIterator<FProperty> It(AdapterConfig.Class, EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces); It; ++It)
				{
					const FProperty* Property = *It;
					if(Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
					{
						if(FParamTypeHandle::FromProperty(Property).IsValid() && IsNameAllowed(Property->GetFName()))
						{
							TStringBuilder<128> ParameterNameBuilder;
							ParameterNameBuilder.Appendf(TEXT("%s_%s"), *RootParam, *Property->GetName());
							
							FObjectAdapterFunction AdapterFunction = [RootParamId, Property, Class = AdapterConfig.Class.Get()](UObject* InContextObject, FParamId InId) -> uint8*
							{
								// Grab object container
								if(const TObjectPtr<UObject>* Object = FParamStack::Get().GetParamPtr<TObjectPtr<UObject>>(RootParamId))
								{
									check(Class->IsChildOf(Property->GetOwnerClass()));
									return const_cast<uint8*>(Property->ContainerPtrToValuePtr<uint8>(*Object));
								}
								return nullptr;
							};

							FName ParameterName(ParameterNameBuilder.ToString());
							RegisterBuiltInPropertyAdapter(ParameterName, Property, MoveTemp(AdapterFunction));
						}
					}
				}
			}

			// Add object's BP-exposed accessor functions
			if(AdapterConfig.FilterType != EAnimNextObjectAdapterFilterType::AllowOnlyProperties)
			{
				auto IsNameAllowed = [&AdapterConfig](FName InName)
				{
					return   AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowOnlyFunctions ||
							 AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowAllPropertiesAndFunctions ||
							(AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::AllowList && AdapterConfig.FilteredFields.Contains(InName)) ||
							(AdapterConfig.FilterType == EAnimNextObjectAdapterFilterType::DenyList && !AdapterConfig.FilteredFields.Contains(InName));
				};

				for(TFieldIterator<UFunction> It(AdapterConfig.Class, EFieldIterationFlags::IncludeSuper | EFieldIterationFlags::IncludeInterfaces); It; ++It)
				{
					UFunction* Function = *It;

					// We add only accessor functions that have valid return types
					const FProperty* ReturnProperty = Function->GetReturnProperty();
					if(ReturnProperty != nullptr && Function->NumParms == 1 && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) && IsNameAllowed(Function->GetFName()))
					{
						if(FParamTypeHandle::FromProperty(ReturnProperty).IsValid())
						{
							FString FunctionName = Function->GetName();
							FunctionName.RemoveFromStart(TEXT("K2_"));
							FunctionName.RemoveFromStart(TEXT("Get"));
							
							TStringBuilder<128> ParameterNameBuilder;
							ParameterNameBuilder.Appendf(TEXT("%s_%s"), *RootParam, *FunctionName);

							FObjectAdapterFunction AdapterFunction = [RootParamId, WeakFunction = TWeakObjectPtr<UFunction>(Function), Class = AdapterConfig.Class.Get()](UObject* InContextObject, FParamId InId) -> uint8*
							{
								// Grab object to call the function with
								if(UFunction* Function = WeakFunction.Get())
								{
									if(const TObjectPtr<UObject>* ObjectPtr = FParamStack::Get().GetParamPtr<TObjectPtr<UObject>>(RootParamId))
									{
										if(UObject* Object = *ObjectPtr)
										{
											check(Object->GetClass()->IsChildOf(Class));
											check(Class->IsChildOf(Function->GetOuterUClass()));
											const FProperty* ReturnProperty = Function->GetReturnProperty();
											check(ReturnProperty);

											uint8* ReturnValuePtr = GetScratchAreaForParamIdAdapter(InId);
											ReturnProperty->InitializeValue(ReturnValuePtr);
											FFrame Stack(Object, Function, nullptr, nullptr, Function->ChildProperties);
											Function->Invoke(Object, Stack, ReturnValuePtr);
											return ReturnValuePtr;
										}
									}
								}
								return nullptr;
							};

							FName ParameterName(ParameterNameBuilder.ToString());
							RegisterBuiltInFunctionAdapter(ParameterName, Function, MoveTemp(AdapterFunction));
						}
					}
				}
			}

			if(AdapterConfig.bRegisterTickFunction)
			{
				TStringBuilder<128> ParameterNameBuilder;
				ParameterNameBuilder.Appendf(TEXT("%s_Tick"), *RootParam);

				FObjectAdapterFunction TickFunctionFunction;
				FAnimNextParamType ParamType = FAnimNextParamType::GetType<FTickFunction>();
				if(AdapterConfig.Class->IsChildOf(UActorComponent::StaticClass()))
				{
					ParamType = FAnimNextParamType::GetType<FActorComponentTickFunction>();
					TickFunctionFunction = [RootParamId](UObject* InContextObject, FParamId InId) -> uint8*
					{
						if (const TObjectPtr<UActorComponent>* Component = FParamStack::Get().GetParamPtr<TObjectPtr<UActorComponent>>(RootParamId))
						{
							if(Component->Get())
							{
								return reinterpret_cast<uint8*>(&Component->Get()->PrimaryComponentTick);
							}
						}
						return nullptr;
					};
				}
				else if(AdapterConfig.Class->IsChildOf(AActor::StaticClass()))
				{
					ParamType = FAnimNextParamType::GetType<FActorTickFunction>();
					TickFunctionFunction = [RootParamId](UObject* InContextObject, FParamId InId) -> uint8*
					{
						if (const TObjectPtr<AActor>* Actor = FParamStack::Get().GetParamPtr<TObjectPtr<AActor>>(RootParamId))
						{
							if(Actor->Get())
							{
								return reinterpret_cast<uint8*>(&Actor->Get()->PrimaryActorTick);
							}
						}
						return nullptr;
					};
				}

				FText TooltipText;
#if WITH_EDITOR
				TooltipText = FText::Format(LOCTEXT("TickFunctionTooltipFormat", "The tick function of a {0}"), AdapterConfig.Class->GetDisplayNameText());
#endif
				FName ParameterName(ParameterNameBuilder.ToString());
				RegisterBuiltInAdapter<FTickFunction, false>(ParameterName, TooltipText, MoveTemp(TickFunctionFunction), EParamDefinitionFlags::Mutable);
			}
		}
	}

	// Refresh RigVM registry as we have updated allowed types
	FRigVMRegistry::Get().RefreshEngineTypes();

	// Increment result buffer serial so we resize on access
	GParamIdGlobalData.SerialNumber++;
	if(GParamIdGlobalData.SerialNumber == 0)
	{
		GParamIdGlobalData.SerialNumber++;
	}
}

FParamId::FParamId(FName InName)
	: Name(InName)
	, Hash(GetTypeHash(InName))
{
}

FParamId::FParamId(FName InName, uint32 InHash)
	: Name(InName)
	, Hash(InHash)
{
	checkSlow(Hash == GetTypeHash(Name));
}

const FParamAdapter* FParamId::GetAdapter(FParamId InId)
{
	for(uint32 Index = GParamIdGlobalData.HashTable.First(InId.GetHash()); GParamIdGlobalData.HashTable.IsValid(Index); Index = GParamIdGlobalData.HashTable.Next(Index))
	{
		if(GParamIdGlobalData.ParamAdapterData[Index].Definition.GetName() == InId.GetName())
		{
			return &GParamIdGlobalData.ParamAdapterData[Index];
		}
	}
	return nullptr;
}

uint8* FParamId::GetScratchAreaForParamIdAdapter(FParamId InId)
{
	if(const FParamAdapter* Adapter = GetAdapter(InId))
	{
		if(ParamAdapterBufferSerialNumber != GParamIdGlobalData.SerialNumber)
		{
			// realloc thread-local storage for results
			ParamAdapterResultBuffer.SetNumZeroed(GParamIdGlobalData.ResultBufferSize);
			ParamAdapterBufferSerialNumber = GParamIdGlobalData.SerialNumber;
		}

		return &ParamAdapterResultBuffer[Adapter->BufferOffset];
	}
	return nullptr;
}

}

#undef LOCTEXT_NAMESPACE