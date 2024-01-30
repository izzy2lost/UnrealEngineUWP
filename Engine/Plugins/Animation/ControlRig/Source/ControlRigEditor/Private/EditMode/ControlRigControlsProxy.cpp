// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/ControlRigControlsProxy.h"
#include "EditorModeManager.h"
#include "EditMode/ControlRigEditMode.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "MovieSceneCommonHelpers.h"
#include "PropertyHandle.h"
#include "Sequencer/ControlRigSequencerHelpers.h"
#include "ISequencer.h"
#include "CurveEditor.h"
#include "CurveModel.h"
#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "UnrealEdGlobals.h"
#include "UnrealEdMisc.h"
#include "Editor/UnrealEdEngine.h"
#include "ScopedTransaction.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SEnumCombo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigControlsProxy)

void UControlRigControlsProxy::AddControlRigControl(UControlRig* InControlRig, const FName& InName)
{
	if (InControlRig == nullptr)
	{
		return;
	}
	FRigControlElement* ControlElement = InControlRig->FindControl(InName);
	if (ControlElement == nullptr)
	{
		return;
	}

	FControlRigProxyItem& Item = ControlRigItems.FindOrAdd(InControlRig);
	Item.ControlRig = InControlRig;
	if (Item.ControlElements.Contains(ControlElement) == false)
	{
		Item.ControlElements.Add(ControlElement);
	}
	if (ControlRigItems.Num() > 1 || Item.ControlElements.Num() > 1 || SequencerItems.Num() > 0)
	{
		FString DisplayString = TEXT("Multiple");
		FName DisplayName(*DisplayString);
		Name = DisplayName;
	}
	else
	{
		Name = ControlElement->GetDisplayName();
	}
}

TArray<FRigControlElement*> UControlRigControlsProxy::GetControlElements() const
{
	TArray<FRigControlElement*> Elements;
	for (const TPair<UControlRig*, FControlRigProxyItem>& Items : ControlRigItems)
	{
		for (FRigControlElement* Element : Items.Value.ControlElements)
		{
			Elements.Add(Element);
		}
	}
	return Elements;
}

void UControlRigControlsProxy::ResetControlRigItems()
{
	ChildProxies.Reset();
	ControlRigItems.Reset();
	if (OwnerControlRig.IsValid())
	{
		AddControlRigControl(OwnerControlRig.Get(), OwnerControlElement->GetFName());
	}
}

void UControlRigControlsProxy::ResetItems()
{
	ResetControlRigItems();
	ResetSequencerItems();
}

void UControlRigControlsProxy::AddItem(UControlRigControlsProxy* ControlProxy)
{
	if (ControlProxy->OwnerControlRig.IsValid())
	{
		AddControlRigControl(ControlProxy->OwnerControlRig.Get(), ControlProxy->OwnerControlElement->GetFName());
	}
	else if(ControlProxy->OwnerObject.IsValid())
	{
		AddSequencerProxyItem(ControlProxy->OwnerObject.Get(), ControlProxy->OwnerBindingAndTrack.WeakTrack, ControlProxy->OwnerBindingAndTrack.Binding);
	}
}

void UControlRigControlsProxy::AddSequencerProxyItem(UObject* InObject, TWeakObjectPtr<UMovieSceneTrack>& InTrack, TSharedPtr<FTrackInstancePropertyBindings>& InBinding)
{
	if (InObject == nullptr)
	{
		return;
	}

	FSequencerProxyItem& Item = SequencerItems.FindOrAdd(InObject);
	Item.OwnerObject = InObject;
	bool bContainsBinding = false;
	for (FBindingAndTrack& Binding : Item.Bindings)
	{
		if (Binding.WeakTrack == InTrack && Binding.Binding->GetPropertyName() == InBinding->GetPropertyName())
		{
			bContainsBinding = true;
		}
	}
	if (bContainsBinding == false)
	{
		FBindingAndTrack BindingAndTrack(InBinding, InTrack.Get());
		Item.Bindings.Add(BindingAndTrack);
	}
	if (SequencerItems.Num() > 1 || Item.Bindings.Num() > 1 || ControlRigItems.Num() > 0)
	{
		FString DisplayString = TEXT("Multiple");
		FName DisplayName(*DisplayString);
		Name = DisplayName;
	}
	else
	{
		if (AActor* Actor = Cast<AActor>(InObject))
		{
			FName DisplayName(*Actor->GetActorLabel());
			Name = DisplayName;
		}
		else if (UActorComponent* Component = Cast<UActorComponent>(InObject))
		{
			FName DisplayName(*Component->GetName());
			Name = DisplayName;
		}
		else
		{
			Name = InBinding->GetPropertyName();
		}
	}
}

TArray<FBindingAndTrack> UControlRigControlsProxy::GetSequencerItems() const
{
	TArray<FBindingAndTrack> Elements;
	for (const TPair<UObject*, FSequencerProxyItem>& Items : SequencerItems)
	{
		for (const FBindingAndTrack& Element : Items.Value.Bindings)
		{
			Elements.Add(Element);
		}
	}
	return Elements;
}

void UControlRigControlsProxy::ResetSequencerItems()
{
	ChildProxies.Reset();
	SequencerItems.Reset();
	if (OwnerObject.IsValid())
	{
		AddSequencerProxyItem(OwnerObject.Get(), OwnerBindingAndTrack.WeakTrack, OwnerBindingAndTrack.Binding);
	}
}

void UControlRigControlsProxy::AddChildProxy(UControlRigControlsProxy* ControlProxy)
{
	if (ChildProxies.Contains(ControlProxy) == false)
	{
		ChildProxies.Add(ControlProxy);
	}
}

void UControlRigControlsProxy::SelectionChanged(bool bInSelected)
{
	if (OwnerControlElement)
	{
		Modify();
		const FName PropertyName("bSelected");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		Binding.CallFunction<bool>(*this, bInSelected);
	}
}

void UControlRigControlsProxy::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigControlsProxy, bSelected))
	{
		if (OwnerControlElement && OwnerControlRig.IsValid())
		{
			FControlRigInteractionScope InteractionScope(OwnerControlRig.Get(), OwnerControlElement->GetKey());
			OwnerControlRig.Get()->SelectControl(OwnerControlElement->GetKey().Name, bSelected);
			OwnerControlRig.Get()->Evaluate_AnyThread();
		}
	}
#if WITH_EDITOR
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
		{
			EControlRigInteractionType InteractionType = EControlRigInteractionType::None;
			FProperty* Owner = PropertyChangedEvent.Property->GetOwnerProperty();
			if (FEditPropertyChain::TDoubleLinkedListNode* MemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
			{
				if (FProperty* MemberProperty = MemberNode->GetValue())
				{
					if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Location))
					{
						InteractionType = EControlRigInteractionType::Translate;
					}
					else if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Rotation))
					{
						InteractionType = EControlRigInteractionType::Rotate;
					}
					else if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Scale))
					{
						InteractionType = EControlRigInteractionType::Scale;
					}
				}
			}
			for (const TPair<UControlRig*, FControlRigProxyItem>& Items : ControlRigItems)
			{
				if (UControlRig* ControlRig = Items.Value.ControlRig.Get())
				{
					for (FRigControlElement* ControlElement : Items.Value.ControlElements)
					{
						if (ControlElement)
						{
							if (InteractionScopes.Contains(ControlElement) == false)
							{
								FControlRigInteractionScope* InteractionScope = new FControlRigInteractionScope(ControlRig, ControlElement->GetKey(), InteractionType);
								InteractionScopes.Add(ControlElement, InteractionScope);
							}
						}
					}
				}
			}
		}
		else
		{
			for (TPair<FRigControlElement*, FControlRigInteractionScope*>& Scope : InteractionScopes)
			{
				if (Scope.Value)
				{
					delete Scope.Value;
				}
			}
			InteractionScopes.Reset();
		}

		//set values
		FProperty* Property = PropertyChangedEvent.Property;
		FProperty* MemberProperty = nullptr;
		if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
		{
			MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
		}
		if (PropertyIsOnProxy(Property, MemberProperty))
		{
			FRigControlModifiedContext Context;
			Context.SetKey = EControlRigSetKey::DoNotCare;
			Context.KeyMask = (uint32)GetChannelToKeyFromPropertyName(Property->GetFName());

			for (const TPair<UControlRig*, FControlRigProxyItem>& Items : ControlRigItems)
			{
				if (UControlRig* ControlRig = Items.Value.ControlRig.Get())
				{
					//we do this backwards so ValueChanged later is set up correctly since that iterates in the other direction
					for (int32 Index = Items.Value.ControlElements.Num() - 1; Index >= 0; --Index)
					{
						FRigControlElement* ControlElement = Items.Value.ControlElements[Index];
						SetControlRigElementValueFromCurrent(ControlRig, ControlElement, Context);
					}
				}
			}
			for (TPair <UObject*, FSequencerProxyItem>& SItems : SequencerItems)
			{
				//we do this backwards so ValueChanged later is set up correctly since that iterates in the other direction
				for (int32 Index = SItems.Value.Bindings.Num() - 1; Index >= 0; --Index)
				{
					FBindingAndTrack& Binding = SItems.Value.Bindings[Index];
					SetBindingValueFromCurrent(SItems.Key, Binding.Binding, Context);
				}
			}
		}
	}
#endif
}

#if WITH_EDITOR
void UControlRigControlsProxy::PostEditUndo()
{
	for (const TPair<UControlRig*, FControlRigProxyItem>& Items : ControlRigItems)
	{
		if (UControlRig* ControlRig = Items.Value.ControlRig.Get())
		{
			for (FRigControlElement* ControlElement : Items.Value.ControlElements)
			{
				if (ControlElement && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlElement->GetKey().Name, ERigElementType::Control)))
				{
					ControlRig->SelectControl(ControlElement->GetKey().Name, bSelected);
				}
			}
		}
	}
}
#endif

