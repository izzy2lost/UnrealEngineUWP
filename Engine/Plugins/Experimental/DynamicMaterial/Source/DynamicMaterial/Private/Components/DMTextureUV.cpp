// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMTextureUV.h"
#include "Components/DMMaterialParameter.h"
#include "DMComponentPath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Model/DynamicMaterialModel.h"
#include "Serialization/CustomVersion.h"

namespace UE::DynamicMaterial::Private
{
	TMap<int32, FName> BaseParameterNames = {
		{ParamID::PivotX,   FName(TEXT("TextureUV_PivotX"))},
		{ParamID::PivotY,   FName(TEXT("TextureUV_PivotY"))},
		{ParamID::TilingX,   FName(TEXT("TextureUV_TilingX"))},
		{ParamID::TilingY,   FName(TEXT("TextureUV_TilingY"))},
		{ParamID::Rotation, FName(TEXT("TextureUV_Rotation"))},
		{ParamID::OffsetX,  FName(TEXT("TextureUV_OffsetX"))},
		{ParamID::OffsetY,  FName(TEXT("TextureUV_OffsetY"))},
	};
}

enum class EDMTextureUVVersion : int32
{
	Initial_Pre_20221102 = 0,
	Version_22021102 = 1,
	ScaleToTiling = 2,
	LatestVersion = ScaleToTiling
};

const FGuid UDMTextureUV::GUID(0xFCF57AFB, 0x50764284, 0xB9A9E659, 0xFFA02D33);
FCustomVersionRegistration GRegisterDMTextureUVVersion(UDMTextureUV::GUID, static_cast<int32>(EDMTextureUVVersion::LatestVersion), TEXT("DMTextureUV"));

const FString UDMTextureUV::OffsetXPathToken  = FString(TEXT("OffsetX"));
const FString UDMTextureUV::OffsetYPathToken  = FString(TEXT("OffsetY"));
const FString UDMTextureUV::PivotXPathToken   = FString(TEXT("PivotX"));
const FString UDMTextureUV::PivotYPathToken   = FString(TEXT("PivotY"));
const FString UDMTextureUV::RotationPathToken = FString(TEXT("Rotation"));
const FString UDMTextureUV::TilingXPathToken   = FString(TEXT("Tiling"));
const FString UDMTextureUV::TilingYPathToken   = FString(TEXT("TilingY"));

const FName UDMTextureUV::NAME_Offset     = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Offset);
const FName UDMTextureUV::NAME_Pivot      = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Pivot);
const FName UDMTextureUV::NAME_Rotation   = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Rotation);
const FName UDMTextureUV::NAME_Tiling      = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Tiling);

#if WITH_EDITOR
const FName UDMTextureUV::NAME_UVSource = GET_MEMBER_NAME_CHECKED(UDMTextureUV, UVSource);
const FName UDMTextureUV::NAME_bMirrorOnX = GET_MEMBER_NAME_CHECKED(UDMTextureUV, bMirrorOnX);
const FName UDMTextureUV::NAME_bMirrorOnY = GET_MEMBER_NAME_CHECKED(UDMTextureUV, bMirrorOnY);

const TMap<FName, bool> UDMTextureUV::TextureProperties = {
	{NAME_UVSource,   false},
	{NAME_Offset,     true},
	{NAME_Pivot,      true},
	{NAME_Rotation,   true},
	{NAME_Tiling,      true},
	{NAME_bMirrorOnX, false},
	{NAME_bMirrorOnY, false}
};
#endif

UDMTextureUV::UDMTextureUV()
{
#if WITH_EDITOR
	EditableProperties.Add(NAME_Offset);
	EditableProperties.Add(NAME_Pivot);
	EditableProperties.Add(NAME_Rotation);
	EditableProperties.Add(NAME_Tiling);
	EditableProperties.Add(NAME_bMirrorOnX);
	EditableProperties.Add(NAME_bMirrorOnY);
#endif
}

#if WITH_EDITOR
void UDMTextureUV::SetUVSource(EDMUVSource InUVSource)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (UVSource == InUVSource)
	{
		return;
	}

	UVSource = InUVSource;

	OnTextureUVChanged(EDMUpdateType::Structure | EDMUpdateType::AllowParentUpdate);
}
#endif

void UDMTextureUV::SetOffset(const FVector2D& InOffset)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Offset.X, InOffset.X)
		&& FMath::IsNearlyEqual(Offset.Y, InOffset.Y))
	{
		return;
	}

	Offset = InOffset;

	OnTextureUVChanged(EDMUpdateType::Value);
}

void UDMTextureUV::SetPivot(const FVector2D& InPivot)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Pivot.X, InPivot.X)
		&& FMath::IsNearlyEqual(Pivot.Y, InPivot.Y))
	{
		return;
	}

	Pivot = InPivot;

	OnTextureUVChanged(EDMUpdateType::Value);
}

void UDMTextureUV::SetRotation(float InRotation)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Rotation, InRotation))
	{
		return;
	}

	Rotation = InRotation;

	OnTextureUVChanged(EDMUpdateType::Value);
}

void UDMTextureUV::SetTiling(const FVector2D& InTiling)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Tiling.X, InTiling.X)
		&& FMath::IsNearlyEqual(Tiling.Y, InTiling.Y))
	{
		return;
	}

	Tiling = InTiling;

	OnTextureUVChanged(EDMUpdateType::Value);
}

#if WITH_EDITOR
void UDMTextureUV::SetMirrorOnX(bool bInMirrorOnX)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (bMirrorOnX == bInMirrorOnX)
	{
		return;
	}

	bMirrorOnX = bInMirrorOnX;

	OnTextureUVChanged(EDMUpdateType::Structure | EDMUpdateType::AllowParentUpdate);
}

void UDMTextureUV::SetMirrorOnY(bool bInMirrorOnY)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (bMirrorOnY == bInMirrorOnY)
	{
		return;
	}

	bMirrorOnY = bInMirrorOnY;

	OnTextureUVChanged(EDMUpdateType::Structure | EDMUpdateType::AllowParentUpdate);
}
#endif

TArray<UDMMaterialParameter*> UDMTextureUV::GetParameters() const
{
	TArray<UDMMaterialParameter*> Parameters;
	Parameters.Reserve(MaterialParameters.Num());

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Parameters.Add(Pair.Value.Get());
	}

	return Parameters;
}

UDMMaterialParameter* UDMTextureUV::GetMaterialParameter(FName InPropertyName, int32 InComponent) const
{
	using namespace UE::DynamicMaterial;

	const TObjectPtr<UDMMaterialParameter>* ParameterPtr = nullptr;

	if (InPropertyName == NAME_Offset)
	{
		switch (InComponent)
		{
			case 0:
				ParameterPtr = MaterialParameters.Find(ParamID::OffsetX);
				break;

			case 1:
				ParameterPtr = MaterialParameters.Find(ParamID::OffsetY);
				break;
		}
	}
	else if (InPropertyName == NAME_Pivot)
	{
		switch (InComponent)
		{
			case 0:
				ParameterPtr = MaterialParameters.Find(ParamID::PivotX);
				break;

			case 1:
				ParameterPtr = MaterialParameters.Find(ParamID::PivotY);
				break;
		}
	}
	else if (InPropertyName == NAME_Rotation)
	{
		switch (InComponent)
		{
			case 0:
				ParameterPtr = MaterialParameters.Find(ParamID::Rotation);
				break;
		}
	}
	else if (InPropertyName == NAME_Tiling)
	{
		switch (InComponent)
		{
			case 0:
				ParameterPtr = MaterialParameters.Find(ParamID::TilingX);
				break;

			case 1:
				ParameterPtr = MaterialParameters.Find(ParamID::TilingY);
				break;
		}
	}

	if (ParameterPtr)
	{
		return (*ParameterPtr).Get();
	}

	return nullptr;
}

FName UDMTextureUV::GetMaterialParameterName(FName InPropertyName, int32 InComponent) const
{
	if (UDMMaterialParameter* Parameter = GetMaterialParameter(InPropertyName, InComponent))
	{
		return Parameter->GetParameterName();
	}

	return NAME_None;
}

#if WITH_EDITOR
bool UDMTextureUV::SetMaterialParameterName(FName InPropertyName, int32 InComponent, FName InNewName)
{
	if (UDMMaterialParameter* Parameter = GetMaterialParameter(InPropertyName, InComponent))
	{
		const FName CurrentName = Parameter->GetParameterName();
		Parameter->RenameParameter(InNewName);
		return Parameter->GetParameterName() != CurrentName;
	}

	return false;
}

void UDMTextureUV::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	if (GetOuter() != InMaterialModel)
	{
		Rename(nullptr, InMaterialModel, UE::DynamicMaterial::RenameFlags);
	}

	// Reset this map as it holds copies of the parameters from the copied-from object.
	// They will not be in this model's parameter list and will share the same name as the old parameters.
	// Just empty the list and create new parameters.
	for (TMap<int32, TObjectPtr<UDMMaterialParameter>>::TIterator It(MaterialParameters); It; ++It)
	{
		UDMMaterialParameter* Parameter = It->Value.Get();

		if (!Parameter || InMaterialModel->ConditionalFreeParameter(Parameter))
		{
			It.RemoveCurrent();
		}
	}

	MaterialParameters.Empty();

	// Create new parameters.
	CreateParameterNames();
}
#endif

void UDMTextureUV::SetMIDParameters(UMaterialInstanceDynamic* InMID)
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
	check(MaterialParameters.IsEmpty() == false);

	using namespace UE::DynamicMaterial;

	auto UpdateMID = [InMID](FName InParamName, float InValue)
	{
		if (FMath::IsNearlyEqual(InValue, InMID->K2_GetScalarParameterValue(InParamName)) == false)
		{
			InMID->SetScalarParameterValue(InParamName, InValue);
		}
	};

	UpdateMID(MaterialParameters[ParamID::PivotX]->GetParameterName(), GetPivot().X);
	UpdateMID(MaterialParameters[ParamID::PivotY]->GetParameterName(), GetPivot().Y);
	UpdateMID(MaterialParameters[ParamID::TilingX]->GetParameterName(), GetTiling().X);
	UpdateMID(MaterialParameters[ParamID::TilingY]->GetParameterName(), GetTiling().Y);
	UpdateMID(MaterialParameters[ParamID::Rotation]->GetParameterName(), GetRotation());
	UpdateMID(MaterialParameters[ParamID::OffsetX]->GetParameterName(), GetOffset().X);
	UpdateMID(MaterialParameters[ParamID::OffsetY]->GetParameterName(), GetOffset().Y);
}

#if WITH_EDITOR
bool UDMTextureUV::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->Modify(bInAlwaysMarkDirty);
	}

	return bSaved;
}
#endif

void UDMTextureUV::Update(EDMUpdateType InUpdateType)
{
	if (!FDMUpdateGuard::CanUpdate())
	{
		return;
	}

	if (!IsComponentValid())
	{
		return;
	}

#if WITH_EDITOR
	if (HasComponentBeenRemoved())
	{
		return;
	}

	MarkComponentDirty();
#endif

	Super::Update(InUpdateType);

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		MaterialModel->OnTextureUVUpdated(this);
	}
}

UDynamicMaterialModel* UDMTextureUV::GetMaterialModel() const
{
	return Cast<UDynamicMaterialModel>(GetOuterSafe());
}

UDMMaterialComponent* UDMTextureUV::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	auto GetParameter = [this, &InPath, &InPathSegment](int32 InParamId) -> UDMMaterialComponent*
		{
			if (const TObjectPtr<UDMMaterialParameter>* ParameterPtr = MaterialParameters.Find(InParamId))
			{
				return *ParameterPtr;
			}

			return UDMMaterialLinkedComponent::GetSubComponentByPath(InPath, InPathSegment);
		};

	using namespace UE::DynamicMaterial;

	if (InPathSegment.GetToken() == OffsetXPathToken)
	{
		return GetParameter(ParamID::OffsetX);
	}

	if (InPathSegment.GetToken() == OffsetYPathToken)
	{
		return GetParameter(ParamID::OffsetY);
	}

	if (InPathSegment.GetToken() == PivotXPathToken)
	{
		return GetParameter(ParamID::PivotX);
	}

	if (InPathSegment.GetToken() == PivotYPathToken)
	{
		return GetParameter(ParamID::PivotY);
	}

	if (InPathSegment.GetToken() == RotationPathToken)
	{
		return GetParameter(ParamID::Rotation);
	}

	if (InPathSegment.GetToken() == TilingXPathToken)
	{
		return GetParameter(ParamID::TilingX);
	}

	if (InPathSegment.GetToken() == TilingYPathToken)
	{
		return GetParameter(ParamID::TilingY);
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

#if WITH_EDITOR
void UDMTextureUV::GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const
{
	using namespace UE::DynamicMaterial::Private;

	// Replace parameter object names with the base parameter name
	if (OutChildComponentPathComponents.IsEmpty() == false)
	{
		for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& MaterialParameter : MaterialParameters)
		{
			if (OutChildComponentPathComponents.Last() == MaterialParameter.Value->GetComponentPathComponent())
			{
				OutChildComponentPathComponents.Last() = BaseParameterNames[MaterialParameter.Key].ToString();
				break;
			}
		}
	}

	Super::GetComponentPathInternal(OutChildComponentPathComponents);
}

void UDMTextureUV::CreateParameterNames()
{
	if (!IsComponentValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

	using namespace UE::DynamicMaterial;

	auto CreateParam = [this, MaterialModel](int32 ParamId)
	{
		if (!MaterialParameters.Contains(ParamId))
		{
			using namespace UE::DynamicMaterial::Private;

			MaterialParameters.Add(ParamId, MaterialModel->CreateUniqueParameter(BaseParameterNames[ParamId]));
			MaterialParameters[ParamId]->SetParentComponent(this);
		}
	};

	CreateParam(ParamID::PivotX);
	CreateParam(ParamID::PivotY);
	CreateParam(ParamID::TilingX);
	CreateParam(ParamID::TilingY);
	CreateParam(ParamID::Rotation);
	CreateParam(ParamID::OffsetX);
	CreateParam(ParamID::OffsetY);
}

void UDMTextureUV::RemoveParameterNames()
{
	if (!IsComponentValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

	if (GUndo)
	{
		MaterialModel->Modify();
	}

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		if (GUndo)
		{
			Pair.Value->Modify();
		}

		MaterialModel->FreeParameter(Pair.Value);
	}
}
#endif

void UDMTextureUV::OnTextureUVChanged(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	Update(InUpdateType);

#if WITH_EDITOR
	if (EnumHasAnyFlags(InUpdateType, EDMUpdateType::AllowParentUpdate) && ParentComponent)
	{
		ParentComponent->Update(InUpdateType);
	}
#endif
}

#if WITH_EDITOR
void UDMTextureUV::OnComponentAdded()
{
	if (!IsComponentValid())
	{
		return;
	}

	CreateParameterNames();

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		MaterialModel->AddRuntimeComponentReference(this);
	}
	
	Super::OnComponentAdded();
}

void UDMTextureUV::OnComponentRemoved()
{
	RemoveParameterNames();

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		MaterialModel->RemoveRuntimeComponentReference(this);
	}

	Super::OnComponentRemoved();
}

void UDMTextureUV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsComponentValid())
	{
		return;
	}

	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	if (PropertyChangedEvent.MemberProperty->GetFName() == NAME_Offset
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Pivot
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Rotation
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Tiling)
	{
		OnTextureUVChanged(EDMUpdateType::Value | EDMUpdateType::AllowParentUpdate);
	}
	else if (PropertyChangedEvent.MemberProperty->GetFName() == NAME_UVSource
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_bMirrorOnX
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_bMirrorOnY)
	{
		OnTextureUVChanged(EDMUpdateType::Structure | EDMUpdateType::AllowParentUpdate);
	}
}

void UDMTextureUV::PreEditUndo()
{
	Super::PreEditUndo();

	UVSource_PreUndo = UVSource;
	bMirrorOnX_PreUndo = bMirrorOnX;
	bMirrorOnY_PreUndo = bMirrorOnY;
}

void UDMTextureUV::PostEditUndo()
{
	Super::PostEditUndo();

	if (UVSource != UVSource_PreUndo
		|| bMirrorOnX != bMirrorOnX_PreUndo
		|| bMirrorOnY != bMirrorOnY_PreUndo)
	{
		OnTextureUVChanged(EDMUpdateType::Structure | EDMUpdateType::AllowParentUpdate);
	}
	else
	{
		OnTextureUVChanged(EDMUpdateType::Value | EDMUpdateType::AllowParentUpdate);
	}
}

void UDMTextureUV::PostLoad()
{
	Super::PostLoad();

	if (!IsComponentValid())
	{
		return;
	}

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		MaterialModel->AddRuntimeComponentReference(this);
	}

	if (MaterialParameters.IsEmpty())
	{
		CreateParameterNames();
	}

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->SetParentComponent(this);
	}

	/*
	 * @TODO GetLinkerCustomVersion() isn't used here to trigger these updates because it always returns 
	 * the latest version regardless of what was saved to the archive.
	 * Inside the function, it is unable to find a Loader and thus fails in this way.
	 */

	if (bNeedsPostLoadStructureUpdate)
	{
		OnTextureUVChanged(EDMUpdateType::Structure);
	}
	else if (bNeedsPostLoadValueUpdate)
	{
		OnTextureUVChanged(EDMUpdateType::Value);
	}

	bNeedsPostLoadStructureUpdate = false;
	bNeedsPostLoadValueUpdate = false;
}

void UDMTextureUV::PostEditImport()
{
	Super::PostEditImport();

	if (!IsComponentValid())
	{
		return;
	}

	if (MaterialParameters.IsEmpty())
	{
		CreateParameterNames();
	}

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->SetParentComponent(this);
	}
}

UDMTextureUV* UDMTextureUV::CreateTextureUV(UObject* InOuter)
{
	UDMTextureUV* NewTextureUV = NewObject<UDMTextureUV>(InOuter, NAME_None, RF_Transactional);
	check(NewTextureUV);

	return NewTextureUV;
}
#endif

void UDMTextureUV::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(UDMTextureUV::GUID);

	Super::Serialize(Ar);

	// @See UDMTextureUV::PostLoad

	int32 TextureUVVersion = Ar.CustomVer(UDMTextureUV::GUID);

	while (TextureUVVersion != static_cast<int32>(EDMTextureUVVersion::LatestVersion))
	{
		switch (TextureUVVersion)
		{
			case INDEX_NONE:
			case static_cast<int32>(EDMTextureUVVersion::Initial_Pre_20221102):
				Offset.X *= -1;
				Rotation *= -360.f;
				Tiling = FVector2D(1.f, 1.f) / Tiling;
				++TextureUVVersion;
#if WITH_EDITORONLY_DATA
				bNeedsPostLoadValueUpdate = true;
#endif
				break;

			case static_cast<int32>(EDMTextureUVVersion::Version_22021102):
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				Tiling.X = (Scale.X != 0.f) ? (1.f / Scale.X) : 1.f;
				Tiling.Y = (Scale.Y != 0.f) ? (1.f / Scale.Y) : 1.f;
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				++TextureUVVersion;
				break;

			case static_cast<int32>(EDMTextureUVVersion::LatestVersion):
				// Do nothing
				break;

			default:
				TextureUVVersion = static_cast<int32>(EDMTextureUVVersion::LatestVersion);
				break;
		}
	}
}
