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

static FRWLock GParamIdLock;

// TODO: Expand scratch buffer system to become a general cache of adapter values.
// The assumption being that if we are grabbing gameplay-driven values, they do not vary over 
// the course of a schedule unless the 'tick' of the values (e.g. a component's tick function) runs
// during that time, at which point the values are reset and lazily updated
// We should cache these values in a FInstancedPropertyBag, per adapted object type (e.g. all 
// adapters for skel mesh components), per thread. This also allows us better handling of complex 
// types, object lifetimes and GC.

// TODO: Investigate allowing function libraries as adapters

struct FParamData
{
	FParamData(FName InName)
		: Name(InName)
	{}

	FName Name;
	int32 AdapterIndex = INDEX_NONE;
	uint32 BufferOffset = MAX_uint32;
};

struct FParamIdGlobalData
{
	TArray<FParamData> ParamData;
	TPagedArray<FParamAdapter, sizeof(FParamAdapter) * 256> ParamAdapters;
	TMap<FName, uint32> NameToParamId;
	uint32 SerialNumber = 0;
	uint32 ResultBufferSize = 0;
};

static FParamIdGlobalData GParamIdGlobalData;

// Buffers to hold values for function-wrapped parameter adapters 
thread_local TArray<uint8> ParamAdapterResultBuffer;
thread_local uint32 ParamAdapterBufferSerialNumber = 0;

#if WITH_DEV_AUTOMATION_TESTS
static FParamIdGlobalData GSandboxedParamIdGlobalData;
static std::atomic<bool> bGParamIdSandboxed = false;
#endif

static FParamIdGlobalData& GetParamIdData()
{
#if WITH_DEV_AUTOMATION_TESTS		
	if (bGParamIdSandboxed.load() == true)
	{
		return GSandboxedParamIdGlobalData;
	}
	else
#endif
	{
		return GParamIdGlobalData;
	}
}

void FParamId::Init()
{
	RefreshAdapters();
}

void FParamId::Destroy()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	FParamIdGlobalData& ParamIdData = GetParamIdData();
	ParamIdData.ParamData.Empty();
	ParamIdData.ParamAdapters.Empty();
	ParamIdData.NameToParamId.Empty();
}

void FParamId::RefreshAdapters()
{
	ResetAdapters();
	RegisterBuiltInAdapters();
	RefreshConfigAdapters();
}

void FParamId::ResetAdapters()
{
	FParamIdGlobalData& ParamIdGlobalData = GetParamIdData();
	
	ParamIdGlobalData.ResultBufferSize = 0;
	ParamIdGlobalData.ParamAdapters.Empty();

	// Reset existing IDs adapter data
	for(FParamData& ParamData : ParamIdGlobalData.ParamData)
	{
		ParamData.AdapterIndex = INDEX_NONE;
		ParamData.BufferOffset = MAX_uint32;

		// Unregister this param as built-in
		FParams::UnregisterBuiltInParameter(ParamData.Name);
	}
}

void FParamId::RegisterBuiltInAdapters()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	FParamIdGlobalData& ParamIdGlobalData = GetParamIdData();

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

		FName DeltaTimeName("UE_Frame_DeltaTime");
		const uint32 DeltaTimeNameFunctionIndex = MakeParamId_NoLock(DeltaTimeName);
		ParamIdGlobalData.ParamData[DeltaTimeNameFunctionIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num();
		FText TooltipText = LOCTEXT("DeltaTimeTooltip", "The current delta time");
		FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(DeltaTimeNameFunctionIndex, DeltaTimeName, FAnimNextParamType::GetType<float>(), TooltipText), MoveTemp(DeltaTimeFunction));
		FParams::RegisterBuiltInParameter(NewAdapter.Definition);

		// Calc buffer size for the result
		ParamIdGlobalData.ParamData[DeltaTimeNameFunctionIndex].BufferOffset = Align(ParamIdGlobalData.ResultBufferSize, alignof(float));
		ParamIdGlobalData.ResultBufferSize += sizeof(float);
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

		FName PhysicsTickName("UE_Physics_Tick");
		const uint32 PhysicsTickFunctionIndex = MakeParamId_NoLock(PhysicsTickName);
		ParamIdGlobalData.ParamData[PhysicsTickFunctionIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num();
		FText TooltipText = LOCTEXT("PhysicsTickTooltip", "The tick function of the physics scene");
		FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(PhysicsTickFunctionIndex, PhysicsTickName, FAnimNextParamType::GetType<FTickFunction>(), TooltipText), MoveTemp(PhysicsTickFunctionFunction));
		FParams::RegisterBuiltInParameter(NewAdapter.Definition);
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

		FName ReferencePoseName("UE_AnimNextMeshComponent_ReferencePose");
		const uint32 ReferencePoseIndex = MakeParamId_NoLock(ReferencePoseName);
		ParamIdGlobalData.ParamData[ReferencePoseIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num();
		FText TooltipText = LOCTEXT("ReferencePoseTooltip", "The reference pose of the skeletal mesh component");
		FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(ReferencePoseIndex, ReferencePoseName, FAnimNextParamType::GetType<FAnimNextGraphReferencePose>(), TooltipText), MoveTemp(ReferencePoseFunction));
		FParams::RegisterBuiltInParameter(NewAdapter.Definition);

		// Calc buffer size for the result
		ParamIdGlobalData.ParamData[ReferencePoseIndex].BufferOffset = Align(ParamIdGlobalData.ResultBufferSize, alignof(FAnimNextGraphReferencePose));
		ParamIdGlobalData.ResultBufferSize += sizeof(FAnimNextGraphReferencePose);
	}
}

void FParamId::RefreshConfigAdapters()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	FParamIdGlobalData& ParamIdGlobalData = GetParamIdData();

	for(const FAnimNextObjectAdapterConfig& AdapterConfig : GetDefault<UAnimNextConfig>()->ExposedClasses)
	{
		if(AdapterConfig.Class != nullptr && AdapterConfig.RootParameter != NAME_None)
		{
			// Ensure we can use the type as a pin in RigVM graphs 
			FRigVMRegistry::Get().RegisterObjectTypes( {{ AdapterConfig.Class, FRigVMRegistry::ERegisterObjectOperation::Class } });

			const FString RootParam(AdapterConfig.RootParameter.ToString());
			const FName RootParamName(*RootParam);
			const uint32 RootParamIndex = MakeParamId_NoLock(RootParamName);
			const FString NameParam = RootParam + TEXT("_Name");
			const FName NameParamName(*NameParam);
			const uint32 NameParamIndex = MakeParamId_NoLock(NameParamName);

			// TODO: add registration mechanism to allow for more than actor/component types
			FObjectAdapterFunction RootFunction;
			if(AdapterConfig.Class->IsChildOf(UActorComponent::StaticClass()))
			{
				RootFunction = [Class = AdapterConfig.Class, NameParamIndex](UObject* InContextObject, FParamId InId) -> uint8*
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
			ParamIdGlobalData.ParamData[RootParamIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num(); 
			FParamAdapter& NewRootAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(RootParamIndex, RootParamName, AdapterConfig.Class->GetDefaultObject()), MoveTemp(RootFunction));
			FParams::RegisterBuiltInParameter(NewRootAdapter.Definition);

			// Calc buffer size for the ptr's result
			ParamIdGlobalData.ParamData[RootParamIndex].BufferOffset = Align(ParamIdGlobalData.ResultBufferSize, alignof(UObject*));
			ParamIdGlobalData.ResultBufferSize += sizeof(UObject*);

			bool bNameParamAdded = false;

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
							if(NameParam.Compare(ParameterNameBuilder.ToString(), ESearchCase::IgnoreCase) == 0)
							{
								bNameParamAdded = true;
							}
							
							FObjectAdapterFunction AdapterFunction = [RootParamIndex, Property, Class = AdapterConfig.Class.Get()](UObject* InContextObject, FParamId InId) -> uint8*
							{
								// Grab object container
								if(const TObjectPtr<UObject>* Object = FParamStack::Get().GetParamPtr<TObjectPtr<UObject>>(FParamId(RootParamIndex)))
								{
									check(Class->IsChildOf(Property->GetOwnerClass()));
									return const_cast<uint8*>(Property->ContainerPtrToValuePtr<uint8>(*Object));
								}
								return nullptr;
							};

							FName ParameterName(ParameterNameBuilder.ToString());
							const uint32 NewIdIndex = MakeParamId_NoLock(ParameterName);
							ParamIdGlobalData.ParamData[NewIdIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num(); 
							FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(NewIdIndex, ParameterName, Property), MoveTemp(AdapterFunction));
							FParams::RegisterBuiltInParameter(NewAdapter.Definition);
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
							if(NameParam.Compare(ParameterNameBuilder.ToString(), ESearchCase::IgnoreCase) == 0)
							{
								bNameParamAdded = true;
							}

							FObjectAdapterFunction AdapterFunction = [RootParamIndex, WeakFunction = TWeakObjectPtr<UFunction>(Function), Class = AdapterConfig.Class.Get()](UObject* InContextObject, FParamId InId) -> uint8*
							{
								// Grab object to call the function with
								if(UFunction* Function = WeakFunction.Get())
								{
									if(const TObjectPtr<UObject>* ObjectPtr = FParamStack::Get().GetParamPtr<TObjectPtr<UObject>>(FParamId(RootParamIndex)))
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
							const uint32 NewIdIndex = MakeParamId_NoLock(ParameterName);
							ParamIdGlobalData.ParamData[NewIdIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num(); 
							FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(NewIdIndex, ParameterName, Function), MoveTemp(AdapterFunction));
							FParams::RegisterBuiltInParameter(NewAdapter.Definition);

							// Calc buffer size for the function's result
							ParamIdGlobalData.ParamData[NewIdIndex].BufferOffset = Align(ParamIdGlobalData.ResultBufferSize, ReturnProperty->GetMinAlignment());
							ParamIdGlobalData.ResultBufferSize += ReturnProperty->GetSize();
						}
					}
				}
			}

			if(!bNameParamAdded)
			{
				// Add name param if we haven't already above
				FText TooltipText;
#if WITH_EDITOR
				TooltipText = FText::Format(LOCTEXT("NameTooltipFormat", "The name of the {0}"), AdapterConfig.Class->GetDisplayNameText());
#endif
				FParams::RegisterBuiltInParameter(FParamDefinition(NameParamIndex, NameParamName, FAnimNextParamType::GetType<FName>(), TooltipText));
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
					TickFunctionFunction = [RootParamIndex](UObject* InContextObject, FParamId InId) -> uint8*
					{
						if (const TObjectPtr<UActorComponent>* Component = FParamStack::Get().GetParamPtr<TObjectPtr<UActorComponent>>(FParamId(RootParamIndex)))
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
					TickFunctionFunction = [RootParamIndex](UObject* InContextObject, FParamId InId) -> uint8*
					{
						if (const TObjectPtr<AActor>* Actor = FParamStack::Get().GetParamPtr<TObjectPtr<AActor>>(FParamId(RootParamIndex)))
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
				const uint32 NewIdIndex = MakeParamId_NoLock(ParameterName);
				ParamIdGlobalData.ParamData[NewIdIndex].AdapterIndex = ParamIdGlobalData.ParamAdapters.Num();
				FParamAdapter& NewAdapter = ParamIdGlobalData.ParamAdapters.Emplace_GetRef(FParamDefinition(NewIdIndex, ParameterName, ParamType, TooltipText, EParamDefinitionFlags::Mutable), MoveTemp(TickFunctionFunction));
				FParams::RegisterBuiltInParameter(NewAdapter.Definition);
			}
		}
	}

	// Refresh RigVM registry as we have updated allowed types
	FRigVMRegistry::Get().RefreshEngineTypes();

	// Increment result buffer serial so we resize on access
	ParamIdGlobalData.SerialNumber++;
	if(ParamIdGlobalData.SerialNumber == 0)
	{
		ParamIdGlobalData.SerialNumber++;
	}
}

FParamId::FParamId(FName InName)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_ParamIdLock);

	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	ParameterIndex = MakeParamId_NoLock(InName);
}

uint32 FParamId::MakeParamId_NoLock(FName InName)
{
	FParamIdGlobalData& ParamIdData = GetParamIdData();

	// Param names should not be 'none'
	check(InName != NAME_None);
	// Param names should not contain periods. Use underscores.
	// Periods are only a display concern and we want parameters to be expressible as members of objects & structures. 
	check(!InName.ToString().Contains(TEXT(".")));

	uint32 Index = InvalidIndex;
	if (const uint32* FoundIndex = ParamIdData.NameToParamId.Find(InName))
	{
		Index = *FoundIndex;
	}
	else
	{
		Index = ParamIdData.ParamData.Num();
		ParamIdData.NameToParamId.Add(InName, Index);
		ParamIdData.ParamData.Add(InName);
	}

	return Index;
}

FName FParamId::ToName() const
{
	FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
	return GetParamIdData().ParamData[ParameterIndex].Name;
}

const FParamAdapter* FParamId::GetAdapter(FParamId InId)
{
	FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
	const FParamIdGlobalData& ParamIdData = GetParamIdData();
	const int32 DefinitionIndex = ParamIdData.ParamData[InId.ParameterIndex].AdapterIndex;
	return DefinitionIndex != INDEX_NONE ? &ParamIdData.ParamAdapters[DefinitionIndex] : nullptr;
}

uint8* FParamId::GetScratchAreaForParamIdAdapter(FParamId InId)
{
	uint32 BufferOffset;
	uint32 SerialNumber;
	uint32 ResultBufferSize;

	{
		FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
		const FParamIdGlobalData& ParamIdData = GetParamIdData();
		BufferOffset = ParamIdData.ParamData[InId.ParameterIndex].BufferOffset;
		SerialNumber = ParamIdData.SerialNumber;
		ResultBufferSize = ParamIdData.ResultBufferSize;
	}

	if(ParamAdapterBufferSerialNumber != SerialNumber)
	{
		// realloc thread-local storage for results
		ParamAdapterResultBuffer.SetNumZeroed(ResultBufferSize);
		ParamAdapterBufferSerialNumber = SerialNumber;
	}

	return &ParamAdapterResultBuffer[BufferOffset];
}

FParamId FParamId::GetMaxParamId()
{
	FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
	return FParamId((uint32)GetParamIdData().ParamData.Num());
}

#if WITH_DEV_AUTOMATION_TESTS
void FParamId::BeginTestSandbox()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	check(bGParamIdSandboxed.load() == false);
	bGParamIdSandboxed.exchange(true);
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamData.Empty();
}

void FParamId::EndTestSandbox()
{
	FRWScopeLock Lock(GParamIdLock, SLT_Write);
	check(bGParamIdSandboxed.load() == true);
	bGParamIdSandboxed.exchange(false);
	GSandboxedParamIdGlobalData.NameToParamId.Empty();
	GSandboxedParamIdGlobalData.ParamData.Empty();
}
#endif

}

#undef LOCTEXT_NAMESPACE