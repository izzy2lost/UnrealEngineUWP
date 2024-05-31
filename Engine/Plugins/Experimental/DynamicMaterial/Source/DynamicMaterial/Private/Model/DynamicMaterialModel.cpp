// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DynamicMaterialModel.h"
#include "Components/DMMaterialParameter.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "DMComponentPath.h"
#include "DMDefs.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialModule.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"

#if WITH_EDITOR
#include "Components/DMTextureUV.h"
#include "Materials/Material.h"
#endif

const FString UDynamicMaterialModel::ValuesPathToken = FString(TEXT("Values"));
const FString UDynamicMaterialModel::ParametersPathToken = FString(TEXT("Parameters"));
const FLazyName UDynamicMaterialModel::GlobalOpacityValueName = FLazyName(TEXT("GlobalOpacityValue"));
const FLazyName UDynamicMaterialModel::GlobalOpacityParameterName = FLazyName(TEXT("GlobalOpacity"));
const FLazyName UDynamicMaterialModel::GlobalMetallicValueName = FLazyName(TEXT("GlobalMetallicValue"));
const FLazyName UDynamicMaterialModel::GlobalMetallicParameterName = FLazyName(TEXT("GlobalMetallic"));
const FLazyName UDynamicMaterialModel::GlobalSpecularValueName = FLazyName(TEXT("GlobalSpecularValue"));
const FLazyName UDynamicMaterialModel::GlobalSpecularParameterName = FLazyName(TEXT("GlobalSpecular"));
const FLazyName UDynamicMaterialModel::GlobalRoughnessValueName = FLazyName(TEXT("GlobalRoughnessValue"));
const FLazyName UDynamicMaterialModel::GlobalRoughnessParameterName = FLazyName(TEXT("GlobalRoughness"));
const FLazyName UDynamicMaterialModel::GlobalNormalValueName = FLazyName(TEXT("GlobalNormalValue"));
const FLazyName UDynamicMaterialModel::GlobalNormalParameterName = FLazyName(TEXT("GlobalNormal"));
const FLazyName UDynamicMaterialModel::GlobalAnisotropyValueName = FLazyName(TEXT("GlobalAnisotropyValue"));
const FLazyName UDynamicMaterialModel::GlobalAnisotropyParameterName = FLazyName(TEXT("GlobalAnisotropy"));
const FLazyName UDynamicMaterialModel::GlobalWorldPositionOffsetValueName = FLazyName(TEXT("GlobalWorldPositionOffsetValue"));
const FLazyName UDynamicMaterialModel::GlobalWorldPositionOffsetParameterName = FLazyName(TEXT("GlobalWorldPositionOffset"));
const FLazyName UDynamicMaterialModel::GlobalAmbientOcclusionValueName = FLazyName(TEXT("GlobalAmbientOcclusionValue"));
const FLazyName UDynamicMaterialModel::GlobalAmbientOcclusionParameterName = FLazyName(TEXT("GlobalAmbientOcclusion"));
const FLazyName UDynamicMaterialModel::GlobalRefractionValueName = FLazyName(TEXT("GlobalRefractionValue"));
const FLazyName UDynamicMaterialModel::GlobalRefractionParameterName = FLazyName(TEXT("GlobalRefraction"));
const FLazyName UDynamicMaterialModel::GlobalPixelDepthOffsetValueName = FLazyName(TEXT("GlobalPixelDepthOffsetValue"));
const FLazyName UDynamicMaterialModel::GlobalPixelDepthOffsetParameterName = FLazyName(TEXT("GlobalPixelDepthOffset"));
const FLazyName UDynamicMaterialModel::GlobalOffsetValueName = FLazyName(TEXT("GlobalOffsetValue"));
const FLazyName UDynamicMaterialModel::GlobalOffsetParameterName = FLazyName(TEXT("GlobalOffset"));
const FLazyName UDynamicMaterialModel::GlobalTilingValueName = FLazyName(TEXT("GlobalTilingValue"));
const FLazyName UDynamicMaterialModel::GlobalTilingParameterName = FLazyName(TEXT("GlobalTiling"));
const FLazyName UDynamicMaterialModel::GlobalRotationValueName = FLazyName(TEXT("GlobalRotationValue"));
const FLazyName UDynamicMaterialModel::GlobalRotationParameterName = FLazyName(TEXT("GlobalRotation"));

UDynamicMaterialModel::UDynamicMaterialModel()
{
	DynamicMaterialInstance = nullptr;

#if WITH_EDITOR
	EditorOnlyDataSI.SetObject(nullptr);
#endif

	auto AddFloatParameter = [this](FName InPropertyName, FName InParameterName, float InDefaultValue = 1.f, bool bSetValueRange = true)
		{
			UDMMaterialValueFloat1* Property = CreateDefaultSubobject<UDMMaterialValueFloat1>(InPropertyName);
			GlobalParameterValues.Add(InPropertyName, Property);

#if WITH_EDITOR
			if (bSetValueRange)
			{
				Property->SetValueRange({0.f, 1.f});
			}

			Property->SetDefaultValue(InDefaultValue);
			Property->ApplyDefaultValue();
#endif

			UDMMaterialParameter* Parameter = CreateDefaultSubobject<UDMMaterialParameter>(FName(*(InParameterName.GetPlainNameString() + TEXT("Parameter"))));
			Parameter->ParameterName = InParameterName;
			Property->Parameter = Parameter;

			ParameterMap.Add(InParameterName, Parameter);
		};

	auto AddVector2Parameter = [this](FName InPropertyName, FName InParameterName, const FVector2D& InDefaultValue)
		{
			UDMMaterialValueFloat2* Property = CreateDefaultSubobject<UDMMaterialValueFloat2>(InPropertyName);
			GlobalParameterValues.Add(InPropertyName, Property);

#if WITH_EDITOR
			Property->SetDefaultValue(InDefaultValue);
			Property->ApplyDefaultValue();
#endif

			UDMMaterialParameter* Parameter = CreateDefaultSubobject<UDMMaterialParameter>(FName(*(InParameterName.GetPlainNameString() + TEXT("Parameter"))));
			Parameter->ParameterName = InParameterName;
			Property->Parameter = Parameter;

			ParameterMap.Add(InParameterName, Parameter);
		};

	FDMUpdateGuard Guard;
	AddFloatParameter(GlobalOpacityValueName, GlobalOpacityParameterName);
	AddFloatParameter(GlobalMetallicValueName, GlobalMetallicParameterName);
	AddFloatParameter(GlobalSpecularValueName, GlobalSpecularParameterName);
	AddFloatParameter(GlobalRoughnessValueName, GlobalRoughnessParameterName);
	AddFloatParameter(GlobalNormalValueName, GlobalNormalParameterName);
	AddFloatParameter(GlobalAnisotropyValueName, GlobalAnisotropyParameterName);
	AddFloatParameter(GlobalWorldPositionOffsetValueName, GlobalWorldPositionOffsetParameterName);
	AddFloatParameter(GlobalAmbientOcclusionValueName, GlobalAmbientOcclusionParameterName);
	AddFloatParameter(GlobalRefractionValueName, GlobalRefractionParameterName);
	AddFloatParameter(GlobalPixelDepthOffsetValueName, GlobalPixelDepthOffsetParameterName);

	AddVector2Parameter(GlobalOffsetValueName, GlobalOffsetParameterName, FVector2D::ZeroVector);
	AddVector2Parameter(GlobalTilingValueName, GlobalTilingParameterName, FVector2D::UnitVector);
	AddFloatParameter(GlobalRotationValueName, GlobalRotationParameterName, /* Default Value */ 0.f, /* Set Value Range */ false);
}

void UDynamicMaterialModel::SetDynamicMaterialInstance(UDynamicMaterialInstance* InDynamicMaterialInstance)
{
	DynamicMaterialInstance = InDynamicMaterialInstance;

#if WITH_EDITOR
	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->PostEditorDuplicate();
	}
#endif
}

UDMMaterialValueFloat1* UDynamicMaterialModel::GetGlobalOpacityValue() const
{
	return GetTypedGlobalParameterValue<UDMMaterialValueFloat1>(GlobalOpacityValueName);
}

UDMMaterialValue* UDynamicMaterialModel::GetGlobalParameterValue(FName InName) const
{
	if (const TObjectPtr<UDMMaterialValue>* ValuePtr = GlobalParameterValues.Find(InName))
	{
		return Cast<UDMMaterialValue>(*ValuePtr);
	}
	
	return nullptr;
}

bool UDynamicMaterialModel::IsModelValid() const
{
	return (!HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) && IsValid(this));
}

UDMMaterialComponent* UDynamicMaterialModel::GetComponentByPath(const FString& InPath) const
{
	FDMComponentPath Path(InPath);
	return GetComponentByPath(Path);
}

UDMMaterialComponent* UDynamicMaterialModel::GetComponentByPath(FDMComponentPath& InPath) const
{
	if (InPath.IsLeaf())
	{
		return nullptr;
	}

	const FDMComponentPathSegment FirstComponent = InPath.GetFirstSegment();

	if (FirstComponent.GetToken() == ValuesPathToken)
	{
		int32 ValueIndex;

		if (FirstComponent.GetParameter(ValueIndex))
		{
			if (Values.IsValidIndex(ValueIndex))
			{
				return Values[ValueIndex]->GetComponentByPath(InPath);
			}
		}

		return nullptr;
	}

	if (FirstComponent.GetToken() == ParametersPathToken)
	{
		FString ParameterStr;

		if (FirstComponent.GetParameter(ParameterStr))
		{
			const FName ParameterName = FName(*ParameterStr);

			if (const TWeakObjectPtr<UDMMaterialParameter>* ParameterPtr = ParameterMap.Find(ParameterName))
			{
				return (*ParameterPtr)->GetComponentByPath(InPath);
			}
		}

		return nullptr;
	}

#if WITH_EDITOR
	if (IDynamicMaterialModelEditorOnlyDataInterface* EditorOnlyData = GetEditorOnlyData())
	{
		return EditorOnlyData->GetSubComponentByPath(InPath, FirstComponent);
	}
#endif

	return nullptr;
}

#if WITH_EDITOR
IDynamicMaterialModelEditorOnlyDataInterface* UDynamicMaterialModel::GetEditorOnlyData() const
{
	if (IsValid(EditorOnlyDataSI.GetObject()))
	{
		return EditorOnlyDataSI.GetInterface();
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::GetValueByName(FName InName) const
{
	for (UDMMaterialValue* Value : Values)
	{
		if (Value->GetParameter() && Value->GetParameter()->GetParameterName() == InName)
		{
			return Value;
		}
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::GetValueByIndex(int32 Index) const
{
	if (Values.IsValidIndex(Index))
	{
		return Values[Index];
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::AddValue(EDMValueType InValueType)
{
	return AddValue(UDMValueDefinitionLibrary::GetValueDefinition(InValueType).GetValueClass());
}

UDMMaterialValue* UDynamicMaterialModel::AddValue(TSubclassOf<UDMMaterialValue> InValueClass)
{
	UDMMaterialValue* NewValue = UDMMaterialValue::CreateMaterialValue(this, TEXT(""), InValueClass, false);
	Values.Add(NewValue);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->OnValueListUpdate();
	}

	return NewValue;
}

void UDynamicMaterialModel::AddRuntimeComponentReference(UDMMaterialComponent* InValue)
{
	RuntimeComponents.Add(InValue);
}

void UDynamicMaterialModel::RemoveRuntimeComponentReference(UDMMaterialComponent* InValue)
{
	RuntimeComponents.Remove(InValue);
}

void UDynamicMaterialModel::RemoveValueByName(FName InName)
{
	int32 FoundIndex = Values.IndexOfByPredicate([InName](UDMMaterialValue* Value)
		{
			return Value->GetParameter() && Value->GetParameter()->GetParameterName() == InName;
		});

	if (FoundIndex == INDEX_NONE)
	{
		return;
	}

	Values.RemoveAt(FoundIndex);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
		ModelEditorOnlyData->OnValueListUpdate();
	}
}

void UDynamicMaterialModel::RemoveValueByIndex(int32 Index)
{
	if (Values.IsValidIndex(Index))
	{
		Values.RemoveAt(Index);

		if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
		{
			ModelEditorOnlyData->RequestMaterialBuild();
			ModelEditorOnlyData->OnValueListUpdate();
		}
	}
}

bool UDynamicMaterialModel::HasParameterName(FName InParameterName) const
{
	if (Values.ContainsByPredicate([InParameterName](const UDMMaterialValue* InValue)
		{
			return IsValid(InValue) && InValue->GetParameter() && InValue->GetParameter()->GetParameterName() == InParameterName;
		}))
	{
		return true;
	}

	const TWeakObjectPtr<UDMMaterialParameter>* ParameterWeak = ParameterMap.Find(InParameterName);

	return (ParameterWeak && ParameterWeak->IsValid());
}

UDMMaterialParameter* UDynamicMaterialModel::CreateUniqueParameter(FName InBaseName)
{
	check(!InBaseName.IsNone());

	UDMMaterialParameter* NewParameter = nullptr;

	{
		const FDMInitializationGuard InitGuard;

		NewParameter = NewObject<UDMMaterialParameter>(this, NAME_None, RF_Transactional);
		check(NewParameter);

		RenameParameter(NewParameter, InBaseName);
	}

	ParameterMap.Emplace(NewParameter->GetParameterName(), NewParameter);
	NewParameter->SetComponentState(EDMComponentLifetimeState::Added);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}

	return NewParameter;
}

void UDynamicMaterialModel::RenameParameter(UDMMaterialParameter* InParameter, FName InBaseName)
{
	check(InParameter);
	check(!InBaseName.IsNone());

	if (!InParameter->ParameterName.IsNone())
	{
		FreeParameter(InParameter);
	}

	if (GUndo)
	{
		InParameter->Modify();
	}

	InParameter->ParameterName = CreateUniqueParameterName(InBaseName);

	ParameterMap.Emplace(InParameter->ParameterName, InParameter);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::FreeParameter(UDMMaterialParameter* InParameter)
{
	check(InParameter);

	if (InParameter->ParameterName.IsNone())
	{
		return;
	}

	for (TMap<FName, TWeakObjectPtr<UDMMaterialParameter>>::TIterator It(ParameterMap); It; ++It)
	{
		const TWeakObjectPtr<UDMMaterialParameter>& ParameterWeak = It->Value;

		if (ParameterWeak.IsValid() == false || ParameterWeak->GetParameterName() == InParameter->GetParameterName())
		{
			It.RemoveCurrent();
		}
	}

	if (GUndo)
	{
		InParameter->Modify();
	}

	InParameter->ParameterName = NAME_None;
	InParameter->SetComponentState(EDMComponentLifetimeState::Removed);
}

bool UDynamicMaterialModel::ConditionalFreeParameter(UDMMaterialParameter* InParameter)
{
	check(InParameter);

	// Parameteres without names are not in the map
	if (InParameter->ParameterName.IsNone())
	{
		return true;
	}

	TWeakObjectPtr<UDMMaterialParameter>* ParameterPtr = ParameterMap.Find(InParameter->ParameterName);

	// Parameter name isn't in the map or it's mapped to a different object.
	if (!ParameterPtr || InParameter != ParameterPtr->Get())
	{
		return true;
	}

	// We're in the map at the given name.
	return false;
}

void UDynamicMaterialModel::ResetData()
{
	Values.Empty();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->ResetData();
	}
}
#endif

void UDynamicMaterialModel::PostLoad()
{
	Super::PostLoad();

	FixGlobalParameterValues();

#if WITH_EDITOR
	SetFlags(RF_Transactional);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	if (GlobalOpacityValue)
	{
		if (UDMMaterialValueFloat1* NewOpacityValue = Cast<UDMMaterialValueFloat1>(GetGlobalParameterValue(GlobalOpacityValueName)))
		{
			NewOpacityValue->SetValue(GlobalOpacityValue->GetValue());
		}

		GlobalOpacityValue = nullptr;
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData();

	if (!ModelEditorOnlyData)
	{
		EditorOnlyDataSI = FDynamicMaterialModule::CreateEditorOnlyData(this);
		EditorOnlyDataSI->LoadDeprecatedModelData(this);
	}

	ReinitComponents();
#endif
}

#if WITH_EDITOR
void UDynamicMaterialModel::PostEditUndo()
{
	Super::PostEditUndo();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostEditImport()
{
	Super::PostEditImport();

	FixGlobalVars();
	PostEditorDuplicate();
	ReinitComponents();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	FixGlobalVars();
	PostEditorDuplicate();
	ReinitComponents();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostEditorDuplicate()
{
	for (const TObjectPtr<UDMMaterialValue>& Value : Values)
	{
		if (GUndo)
		{
			Value->Modify();
		}

		Value->PostEditorDuplicate(this, nullptr);
	}

	for (const TPair<FName, TObjectPtr<UDMMaterialValue>>& ValuePair : GlobalParameterValues)
	{
		if (GUndo)
		{
			ValuePair.Value->Modify();
		}

		ValuePair.Value->PostEditorDuplicate(this, nullptr);
	}

	for (const TPair<FName, TWeakObjectPtr<UDMMaterialParameter>>& ParameterPair : ParameterMap)
	{
		if (UDMMaterialParameter* Parameter = ParameterPair.Value.Get())
		{
			if (GUndo)
			{
				ParameterPair.Value->Modify();
			}

			Parameter->PostEditorDuplicate(this, nullptr);
		}
	}

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->PostEditorDuplicate();
	}
}
#endif

void UDynamicMaterialModel::FixGlobalParameterValues()
{
	for (TPair<FName, TObjectPtr<UDMMaterialValue>>& ParameterPair : GlobalParameterValues)
	{
		if (!ParameterPair.Value || !ParameterPair.Value->HasAnyFlags(RF_ArchetypeObject))
		{
			continue;
		}

		const FName Name = ParameterPair.Value->GetFName();

		UDMMaterialValue* Local = FindObjectFast<UDMMaterialValue>(this, Name, /* Exact class */ false);

		if (!Local)
		{
			UE_LOG(LogDynamicMaterial, Error, TEXT("Archtype global parameter found and could not find local one. [%s]"), *Name.ToString());
			continue;
		}

		ParameterPair.Value = Local;
	}
}

#if WITH_EDITOR
void UDynamicMaterialModel::ReinitComponents()
{
	if (IsValid(DynamicMaterial))
	{
		if (GUndo)
		{
			DynamicMaterial->Modify();
		}

		DynamicMaterial->AtomicallySetFlags(RF_DuplicateTransient);
	}

	// Clean up old parameters
	ParameterMap.Empty();

	TArray<UObject*> Subobjects;
	GetObjectsWithOuter(this, Subobjects, false);

	for (UObject* Subobject : Subobjects)
	{
		if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Subobject))
		{
			if (UDMMaterialParameter* Parameter = Value->GetParameter())
			{
				ParameterMap.Emplace(Parameter->GetParameterName(), Parameter);
			}
		}
		else if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Subobject))
		{
			TArray<UDMMaterialParameter*> Parameters = TextureUV->GetParameters();

			for (UDMMaterialParameter* Parameter : Parameters)
			{
				ParameterMap.Emplace(Parameter->GetParameterName(), Parameter);
			}
		}
	}

	FixGlobalVars();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->ReinitComponents();
	}
}

void UDynamicMaterialModel::FixGlobalVars()
{
	auto FixGlobalVar = [this](FName InValueName, FName InParameterName)
		{
			UDMMaterialValue* Property = GetGlobalParameterValue(InValueName);

			if (!Property)
			{
				return;
			}

			UDMMaterialParameter* Parameter = Property->GetParameter();

			if (!Parameter || Parameter->GetParameterName() != InParameterName)
			{
				if (Property->SetParameterName(InParameterName))
				{
					return;
				}

				Parameter = Property->GetParameter();

				if (!Parameter)
				{
					return;
				}
			}
	
			if (const TWeakObjectPtr<UDMMaterialParameter>* FoundParameterPtr = ParameterMap.Find(InParameterName))
			{
				UDMMaterialParameter* FoundParameter = FoundParameterPtr->Get();

				if (!FoundParameter || FoundParameter->HasAnyFlags(RF_ArchetypeObject)
					|| (Parameter && FoundParameter != Parameter))
				{
					if (Parameter)
					{
						ParameterMap[InParameterName] = Parameter;
					}
					else
					{
						ParameterMap.Remove(InParameterName);
					}
				}
			}

			if (Property && Parameter != Property->Parameter)
			{
				if (GUndo)
				{
					Parameter->Modify();
					Property->Modify();
				}

				Parameter->ParameterName = InParameterName;
				Property->Parameter = Parameter;
			}
		};

	FixGlobalVar(GlobalOpacityValueName, GlobalOpacityParameterName);
	FixGlobalVar(GlobalRoughnessValueName, GlobalRoughnessParameterName);
	FixGlobalVar(GlobalNormalValueName, GlobalNormalParameterName);
	FixGlobalVar(GlobalSpecularValueName, GlobalSpecularParameterName);
	FixGlobalVar(GlobalMetallicValueName, GlobalMetallicParameterName);
	FixGlobalVar(GlobalAnisotropyValueName, GlobalAnisotropyParameterName);
	FixGlobalVar(GlobalWorldPositionOffsetValueName, GlobalWorldPositionOffsetParameterName);
	FixGlobalVar(GlobalAmbientOcclusionValueName, GlobalAmbientOcclusionParameterName);
	FixGlobalVar(GlobalRefractionValueName, GlobalRefractionParameterName);
	FixGlobalVar(GlobalPixelDepthOffsetValueName, GlobalPixelDepthOffsetParameterName);
	FixGlobalVar(GlobalOffsetValueName, GlobalOffsetParameterName);
	FixGlobalVar(GlobalTilingValueName, GlobalTilingParameterName);
	FixGlobalVar(GlobalRotationValueName, GlobalRotationParameterName);
}

FName UDynamicMaterialModel::CreateUniqueParameterName(FName InBaseName)
{
	int32 CurrentTest = 0;
	FName UniqueName = InBaseName;

	auto UpdateName = [&InBaseName, &CurrentTest, &UniqueName]()
		{
			++CurrentTest;
			UniqueName = FName(InBaseName.ToString() + TEXT("_") + FString::FromInt(CurrentTest));
		};

	bool bIsNameUnique = false;

	while (!bIsNameUnique)
	{
		bIsNameUnique = true;

		if (Values.ContainsByPredicate([UniqueName](const UDMMaterialValue* Value)
			{
				return Value->GetMaterialParameterName() == UniqueName;
			}))
		{
			bIsNameUnique = false;
			UpdateName();
		}
	}

	bIsNameUnique = false;

	while (!bIsNameUnique)
	{
		bIsNameUnique = true;

		if (const TWeakObjectPtr<UDMMaterialParameter>* CurrentParameter = ParameterMap.Find(UniqueName))
		{
			if (CurrentParameter->IsValid())
			{
				bIsNameUnique = false;
				UpdateName();
			}
			else
			{
				ParameterMap.Remove(UniqueName);
				break; // Not needed, but informative.
			}
		}
	}

	return UniqueName;
}
#endif
