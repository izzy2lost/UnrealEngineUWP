// Copyright Epic Games, Inc. All Rights Reserved.

#include "Action/RCPropertyIdAction.h"

#include "Engine/StaticMesh.h"
#include "IPropertyIdHandler.h"
#include "IRemoteControlModule.h"
#include "Materials/MaterialInterface.h"
#include "RCVirtualProperty.h"
#include "RemoteControlPreset.h"
#include "RemoteControlPropertyIdRegistry.h"

URCPropertyIdAction::~URCPropertyIdAction()
{
	if (PresetWeakPtr.IsValid())
	{
		PresetWeakPtr->OnEntityUnexposed().RemoveAll(this);
		PresetWeakPtr->GetPropertyIdRegistry()->OnPropertyIdUpdated().RemoveAll(this);
	}
}

void URCPropertyIdAction::Execute() const
{
	if (!PresetWeakPtr.IsValid())
	{
		return;
	}
	
	if (PropertySelfContainer.IsEmpty())
	{
		return;
	}

	for (const TPair<FPropertyIdContainerKey, TObjectPtr<URCVirtualPropertySelfContainer>>& PropertyContainer : PropertySelfContainer)
	{
		if (FProperty* Property = PropertyContainer.Value->GetProperty())
		{
			FRemoteControlPropertyIdArgs PropertyIdArgs;
			PropertyIdArgs.VirtualProperty = PropertyContainer.Value;
			PropertyIdArgs.PropertyId = PropertyContainer.Key.PropertyId;
			PropertyIdArgs.SuperType = Property->GetClass()->GetFName();

			TSharedPtr<IPropertyIdHandler> PropertyIdHandler = IRemoteControlModule::Get().GetPropertyIdHandlerFor(Property);
			if (!PropertyIdHandler.IsValid())
			{
				continue;
			}
			const EPropertyBagPropertyType PropertyBagType = PropertyIdHandler->GetPropertyType(Property);

			if (PropertyBagType == EPropertyBagPropertyType::Enum)
			{
				PropertyIdArgs.SuperType = NAME_EnumProperty;
				PropertyIdArgs.SubType = PropertyIdHandler->GetPropertyTypeName(Property);
			}
			else if (PropertyBagType == EPropertyBagPropertyType::Object)
			{
				if (const TSharedPtr<FStructOnScope> StructOnScope = PropertyContainer.Value->CreateStructOnScope())
				{
					if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(PropertyContainer.Value->GetProperty()))
					{
						if (const uint8* PropertyValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(StructOnScope->GetStructMemory()))
						{
							PropertyIdArgs.SourceObject = ObjectProperty->GetObjectPropertyValue(PropertyValuePtr);
							if (PropertyIdArgs.SourceObject)
							{
								if (PropertyIdArgs.SourceObject->IsA(UMaterialInterface::StaticClass()))
								{
									PropertyIdArgs.SourceClass = UMaterialInterface::StaticClass();
								}
								else
								{
									PropertyIdArgs.SourceClass = PropertyIdArgs.SourceObject->GetClass();
								}
								PropertyIdArgs.SubType = PropertyIdArgs.SourceClass->GetFName();
							}
						}
					}
				}
			}
			else if (PropertyBagType == EPropertyBagPropertyType::Struct)
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct->GetFName() == NAME_LinearColor)
					{
						PropertyIdArgs.SubType = NAME_Color;
					}
					else
					{
						PropertyIdArgs.SubType = StructProperty->Struct->GetFName();
					}
				}
			}
			PresetWeakPtr->PerformChainReaction(PropertyIdArgs);
		}
	}
	Super::Execute();
}

void URCPropertyIdAction::UpdateEntityIds(const TMap<FGuid, FGuid>& InEntityIdMap)
{
	for (const TPair<FPropertyIdContainerKey, TObjectPtr<URCVirtualPropertySelfContainer>>& PropertyContainerEntry : PropertySelfContainer)
	{
		if (PropertyContainerEntry.Value)
		{
			PropertyContainerEntry.Value->UpdateEntityIds(InEntityIdMap);
		}
	}
	for (const TPair<FPropertyIdContainerKey, TObjectPtr<URCVirtualPropertySelfContainer>>& PropertyContainerEntry : CachedPropertySelfContainer)
	{
		if (PropertyContainerEntry.Value)
		{
			PropertyContainerEntry.Value->UpdateEntityIds(InEntityIdMap);
		}
	}
	
	Super::UpdateEntityIds(InEntityIdMap);
}

void URCPropertyIdAction::PostLoad()
{
	UObject::PostLoad();
	Initialize();
}

#if WITH_EDITOR
void URCPropertyIdAction::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	if (const FProperty* Property = PropertyChangedEvent.Property)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URCPropertyIdAction, PropertyId))
		{
			UpdatePropertyId();
		}
	}
}
#endif // WITH_EDITOR

void URCPropertyIdAction::UpdatePropertyId()
{
	if (URemoteControlPreset* Preset = PresetWeakPtr.Get())
	{
		PropertySelfContainer.Empty();
		const TObjectPtr<URemoteControlPropertyIdRegistry> PropertyIdRegistry = Preset->GetPropertyIdRegistry();

		for (const FGuid& TargetProperty : PropertyIdRegistry->GetEntityIdsList())
		{
			if (const TSharedPtr<FRemoteControlProperty> TargetRCProperty = Preset->GetExposedEntity<FRemoteControlProperty>(TargetProperty).Pin())
			{
				if (PropertyIdRegistry->Contains(TargetRCProperty->PropertyId, PropertyId))
				{
					if (FProperty* Property = TargetRCProperty->GetProperty())
					{
						TSharedPtr<IPropertyIdHandler> PropertyIdHandler = IRemoteControlModule::Get().GetPropertyIdHandlerFor(Property);
						if (!PropertyIdHandler.IsValid())
						{
							continue;
						}

						FString PrefixPropertyClassName = TargetRCProperty->PropertyId.ToString() + TEXT(".");
						FName PropertyClassName = FName(PrefixPropertyClassName + PropertyIdHandler->GetPropertyTypeName(Property).ToString());
						const FName& NewPropertyIdName = *(TargetRCProperty->PropertyId.ToString() + TEXT(".") + PropertyClassName.ToString());
						const FProperty* PropToDuplicate = PropertyIdHandler->GetPropertyInsideContainer(Property);
						FPropertyIdContainerKey CurrentKey = { TargetRCProperty->PropertyId, PropertyClassName };

						if (CachedPropertySelfContainer.Contains(CurrentKey))
						{
							PropertySelfContainer.Add(CurrentKey, CachedPropertySelfContainer[CurrentKey]);
						}
						else
						{
							CachedPropertySelfContainer.Add(CurrentKey, NewObject<URCVirtualPropertySelfContainer>(this));

							// Special case for LinearColor, since we want to base the copy on Color struct here we add the property instead of duplicating it
							bool bColorAdded = false;

							if (const FStructProperty* StructProp = CastField<FStructProperty>(PropToDuplicate))
							{
								if (StructProp->Struct)
								{
									if (StructProp->Struct->GetFName() == NAME_LinearColor ||
										StructProp->Struct->GetFName() == NAME_Color)
									{
										CachedPropertySelfContainer[CurrentKey]->AddProperty(NewPropertyIdName,
										PropertyIdHandler->GetPropertyType(Property),
										PropertyIdHandler->GetPropertyTypeObject(Property));

#if WITH_EDITORONLY_DATA
										FProperty* BagProperty = CachedPropertySelfContainer[CurrentKey]->GetProperty();
										FStructProperty* StructProperty = CastField<FStructProperty>(BagProperty);
										if (StructProperty && StructProperty->Struct &&
											!StructProperty->HasMetaData("OnlyUpdateOnInteractionEnd"))
										{
											StructProperty->AppendMetaData({{FName("OnlyUpdateOnInteractionEnd"), TEXT("true")}});
										}
#endif
										bColorAdded = true;
									}
								}
							}

							if (!bColorAdded)
							{
								CachedPropertySelfContainer[CurrentKey]->DuplicateProperty(NewPropertyIdName, PropToDuplicate);
							}

							CachedPropertySelfContainer[CurrentKey]->PresetWeakPtr = PresetWeakPtr;
							PropertySelfContainer.Add(CurrentKey, CachedPropertySelfContainer[CurrentKey]);
						}
					}
				}
			}
		}
#if WITH_EDITOR
		if (PresetWeakPtr.IsValid())
		{
			PresetWeakPtr->GetPropertyIdRegistry()->OnPropertyIdActionNeedsRefresh().Broadcast();
		}
#endif
	}
}

void URCPropertyIdAction::Initialize()
{
	if (PresetWeakPtr.IsValid())
	{
		PresetWeakPtr->OnEntityUnexposed().AddUObject(this, &URCPropertyIdAction::OnEntityUnexposed);
		PresetWeakPtr->GetPropertyIdRegistry()->OnPropertyIdUpdated().AddUObject(this, &URCPropertyIdAction::UpdatePropertyId);
	}
}

void URCPropertyIdAction::OnEntityUnexposed(URemoteControlPreset* InPreset, const FGuid& InGuid)
{
	if (InPreset)
	{
		const TWeakPtr<FRemoteControlField> UnexposedEntity = StaticCastWeakPtr<FRemoteControlField>(InPreset->GetExposedEntity(InGuid));
		if (UnexposedEntity.IsValid())
		{
			//If the propertyId of the property unexposed is the same of this PropertyIdAction refresh, otherwise don't update it
			if (UnexposedEntity.Pin()->PropertyId == PropertyId)
			{
				InPreset->GetPropertyIdRegistry()->RemoveIdentifiedField(UnexposedEntity.Pin()->GetId());
				InPreset->GetPropertyIdRegistry()->OnPropertyIdUpdated().Broadcast();
			}
			return;
		}
	}
	//If there is anything wrong with the InPreset or InGuid it won't be able to check the PropertyId
	//We refresh to be sure to not miss anything
	UpdatePropertyId();
}
