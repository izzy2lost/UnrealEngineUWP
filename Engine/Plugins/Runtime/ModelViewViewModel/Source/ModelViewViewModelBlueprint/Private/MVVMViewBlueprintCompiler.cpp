// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMViewBlueprintCompiler.h"
#include "Blueprint/WidgetTree.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMFieldPathHelper.h"
#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMMessageLog.h"
#include "PropertyPermissionList.h"
#include "MVVMFunctionGraphHelper.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMBindingName.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "View/MVVMViewModelContextResolver.h"
#include "WidgetBlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "MVVMViewBlueprintCompiler"

/**
When compiling the skeletal class
	CreateVariables()
		Add the less amount of errors here to have a more responsive editor and have the variable available in the editor.
		Create all the variables
		Create the public functions
When compiling the full class
	CreateVariables()
		same as skeletal, it may compile the skeletal then the full class. This will be called twice with the same instance.
	CreateFunctions()
		Create the private functions for bindings and events.
		Note The class is not linked
Once the full class is compiled, then add the bindings/events with
	PreCompile()
		Create the bindings and events data and send them to the library compiler.
	Compile()
		All bindings and events data are compiled. Create the view data with the compiled data.
 */

namespace UE::MVVM::Private
{
FAutoConsoleVariable CVarLogViewCompiledResult(
	TEXT("MVVM.LogViewCompiledResult"),
	false,
	TEXT("After the view is compiled log the compiled bindings and sources.")
);

#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
FAutoConsoleCommand CVarTestGenerateSetter(
	TEXT("MVVM.TestGenerateSetter"),
	TEXT("Generate a setter function base on the input string."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			return;
		}
		FMVVMViewBlueprintCompiler::TestGenerateSetter(nullptr, Args[0], Args[1], Args[2]);
	})
);
#endif

static const FText CouldNotCreateSourceFieldPathFormat = LOCTEXT("CouldNotCreateSourceFieldPath", "Couldn't create the source field path '{0}'. {1}");
static const FText CouldNotCreateDestinationFieldPathFormat = LOCTEXT("CouldNotCreateDestinationFieldPath", "Couldn't create the destination field path '{0}'. {1}");
static const FText PropertyPathIsInvalidFormat = LOCTEXT("PropertyPathIsInvalid", "The property path '{0}' is invalid.");

void RenameObjectToTransientPackage(UObject* ObjectToRename)
{
	const ERenameFlags RenFlags = REN_DoNotDirty | REN_ForceNoResetLoaders | REN_DontCreateRedirectors;

	ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
	ObjectToRename->SetFlags(RF_Transient);
	ObjectToRename->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
	FLinkerLoad::InvalidateExport(ObjectToRename);
}

FString PropertyPathToString(const UClass* InSelfContext, const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	if (!PropertyPath.IsValid())
	{
		return FString();
	}

	TStringBuilder<512> Result;
	switch (PropertyPath.GetSource(BlueprintView->GetOuterUMVVMWidgetBlueprintExtension_View()->GetOuterUWidgetBlueprint()))
	{
	case EMVVMBlueprintFieldPathSource::SelfContext:
		Result << InSelfContext->ClassGeneratedBy->GetName();
		break;
	case EMVVMBlueprintFieldPathSource::Widget:
		Result << PropertyPath.GetWidgetName();
		break;
	case EMVVMBlueprintFieldPathSource::ViewModel:
		if (const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(PropertyPath.GetViewModelId()))
		{
			Result << SourceViewModelContext->GetViewModelName();
		}
		else
		{
			Result << TEXT("<none>");
		}
		break;
	default:
		Result << TEXT("<none>");
		break;
	}

	FString BasePropertyPath = PropertyPath.GetPropertyPath(InSelfContext);
	if (BasePropertyPath.Len())
	{
		Result << TEXT('.');
		Result << MoveTemp(BasePropertyPath);
	}
	return Result.ToString();
}

FText PropertyPathToText(const UClass* InSelfContext, const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPathToString(InSelfContext, BlueprintView, PropertyPath));
}

FText GetViewModelIdText(const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return FText::FromString(PropertyPath.GetViewModelId().ToString(EGuidFormats::DigitsWithHyphensInBraces));
}

TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> AddObjectFieldPath(FCompiledBindingLibraryCompiler& BindingLibraryCompiler, const UWidgetBlueprintGeneratedClass* Class, FStringView ObjectPath, const UClass* ExpectedType)
{
	// Generate a path to read the value at runtime
	static const FText InvalidGetterFormat = LOCTEXT("ViewModelInvalidGetterWithReason", "Viewmodel has an invalid Getter. {0}");

	TValueOrError<TArray<FMVVMConstFieldVariant>, FText> GeneratedField = FieldPathHelper::GenerateFieldPathList(Class, ObjectPath, true);
	if (GeneratedField.HasError())
	{
		return MakeError(FText::Format(InvalidGetterFormat, GeneratedField.GetError()));
	}

	TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = BindingLibraryCompiler.AddObjectFieldPath(GeneratedField.GetValue(), ExpectedType, true);
	if (ReadFieldPathResult.HasError())
	{
		return MakeError(FText::Format(InvalidGetterFormat, ReadFieldPathResult.GetError()));
	}

	return MakeValue(ReadFieldPathResult.StealValue());
}


FMVVMViewBlueprintCompiler::FMVVMViewBlueprintCompiler(FWidgetBlueprintCompilerContext& InCreationContext, UMVVMBlueprintView* InBlueprintView)
	: WidgetBlueprintCompilerContext(InCreationContext)
	, BlueprintView(InBlueprintView)
	, BindingLibraryCompiler(InCreationContext.WidgetBlueprint())
{
	check(BlueprintView.IsValid());
}


void FMVVMViewBlueprintCompiler::AddMessage(const FText& MessageText, EMessageType MessageType) const
{
	switch (MessageType)
	{
	case EMessageType::Info:
		WidgetBlueprintCompilerContext.MessageLog.Note(*MessageText.ToString());
		break;
	case EMessageType::Warning:
		WidgetBlueprintCompilerContext.MessageLog.Warning(*MessageText.ToString());
		break;
	case EMessageType::Error:
		WidgetBlueprintCompilerContext.MessageLog.Error(*MessageText.ToString());
		break;
	default:
		break;
	}
}


void FMVVMViewBlueprintCompiler::AddMessages(TArrayView<TWeakPtr<FCompilerBinding>> Bindings, TArrayView<TWeakPtr<FCompilerEvent>> Events, const FText& MessageText, EMessageType MessageType) const
{
	for (TWeakPtr<FCompilerBinding>& Binding : Bindings)
	{
		AddMessageForBinding(Binding.Pin(), MessageText, MessageType, FMVVMBlueprintPinId());
	}
	for (TWeakPtr<FCompilerEvent>& Event : Events)
	{
		AddMessageForEvent(Event.Pin(), MessageText, MessageType, FMVVMBlueprintPinId());
	}

	if (Bindings.Num() == 0 && Events.Num() == 0)
	{
		AddMessage(MessageText, MessageType);
	}
}


void FMVVMViewBlueprintCompiler::AddMessageForBinding(const TSharedPtr<FCompilerBinding>& Binding, const FText& MessageText, EMessageType MessageType, const FMVVMBlueprintPinId& PinId) const
{
	const FMVVMBlueprintViewBinding* BindingPtr = Binding ? BlueprintView->GetBindingAt(Binding->Key.ViewBindingIndex) : nullptr;
	if (BindingPtr)
	{
		AddMessageForBinding(*BindingPtr, MessageText, MessageType, PinId);
	}
	else
	{
		AddMessage(MessageText, MessageType);
	}
}


FText GetJoinArgumentNames(const FMVVMBlueprintPinId& PinId)
{
	TArray<FText> JoinedNames;
	JoinedNames.Reserve(PinId.GetNames().Num());
	for (FName Name : PinId.GetNames())
	{
		JoinedNames.Add(FText::FromName(Name));
	}
	return FText::Join(LOCTEXT("NameSeperator", "."), JoinedNames);
}


void FMVVMViewBlueprintCompiler::AddMessageForBinding(const FMVVMBlueprintViewBinding& Binding, const FText& MessageText, EMessageType MessageType, const FMVVMBlueprintPinId& PinId) const
{
	const FText BindingName = FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint()));

	FText FormattedError;
	if (PinId.IsValid())
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormatWithArgument", "Binding '{0}': Argument '{1}' - {2}"), BindingName, UE::MVVM::Private::GetJoinArgumentNames(PinId), MessageText);
	}
	else
	{
		FormattedError = FText::Format(LOCTEXT("BindingFormat", "Binding '{0}': {1}"), BindingName, MessageText);
	}
	AddMessage(FormattedError, MessageType);

	static EBindingMessageType BindingMessageTypes[] = { EBindingMessageType ::Info, EBindingMessageType ::Warning, EBindingMessageType ::Error};
	FBindingMessage NewMessage = { FormattedError, BindingMessageTypes[static_cast<int32>(MessageType)] };
	BlueprintView->AddMessageToBinding(Binding.BindingId, NewMessage);
}


void FMVVMViewBlueprintCompiler::AddMessageForEvent(const TSharedPtr<FCompilerEvent>& Event, const FText& MessageText, EMessageType MessageType, const FMVVMBlueprintPinId& PinId) const
{
	const UMVVMBlueprintViewEvent* EventPtr = Event ? Event->Event.Get() : nullptr;
	if (EventPtr)
	{
		AddMessageForEvent(EventPtr, MessageText, MessageType, PinId);
	}
	else
	{
		AddMessage(MessageText, MessageType);
	}
}


void FMVVMViewBlueprintCompiler::AddMessageForEvent(const UMVVMBlueprintViewEvent* Event, const FText& MessageText, EMessageType MessageType, const FMVVMBlueprintPinId& PinId) const
{
	const FText EventName = Event->GetDisplayName(true);

	FText FormattedError;
	if (PinId.IsValid())
	{
		FormattedError = FText::Format(LOCTEXT("EventFormatWithArgument", "Event '{0}': Argument '{1}' - {2}"), EventName, UE::MVVM::Private::GetJoinArgumentNames(PinId), MessageText);
	}
	else
	{
		FormattedError = FText::Format(LOCTEXT("EventFormat", "Event '{0}': {1}"), EventName, MessageText);
	}
	AddMessage(FormattedError, MessageType);

	static UMVVMBlueprintViewEvent::EMessageType BindingMessageTypes[] = { UMVVMBlueprintViewEvent::EMessageType::Info, UMVVMBlueprintViewEvent::EMessageType::Warning, UMVVMBlueprintViewEvent::EMessageType::Error };
	UMVVMBlueprintViewEvent::FMessage NewMessage = { FormattedError, BindingMessageTypes[static_cast<int32>(MessageType)] };
	Event->AddCompilationToBinding(NewMessage);
}


void FMVVMViewBlueprintCompiler::AddMessageForViewModel(const FMVVMBlueprintViewModelContext& ViewModel, const FText& Message, EMessageType MessageType) const
{
	const FText FormattedError = FText::Format(LOCTEXT("ViewModelFormat", "Viewodel '{0}': {1}"), ViewModel.GetDisplayName(), Message);
	AddMessage(FormattedError, MessageType);
}


void FMVVMViewBlueprintCompiler::AddMessageForViewModel(const FText& ViewModelDisplayName, const FText& Message, EMessageType MessageType) const
{
	const FText FormattedError = FText::Format(LOCTEXT("ViewModelFormat", "Viewodel '{0}': {1}"), ViewModelDisplayName, Message);
	AddMessage(FormattedError, MessageType);
}


void FMVVMViewBlueprintCompiler::AddExtension(UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	WidgetBlueprintCompilerContext.AddExtension(Class, ViewExtension);
}


void FMVVMViewBlueprintCompiler::CleanOldData(UWidgetBlueprintGeneratedClass* ClassToClean, UObject* OldCDO)
{
	// Clean old View
	if (!WidgetBlueprintCompilerContext.Blueprint->bIsRegeneratingOnLoad && WidgetBlueprintCompilerContext.bIsFullCompile)
	{
		TArray<UObject*> Children;
		const bool bIncludeNestedObjects = false;
		ForEachObjectWithOuter(ClassToClean, [&Children](UObject* Child)
			{
				if (Cast<UMVVMViewClass>(Child))
				{
					Children.Add(Child);
				}
			}, bIncludeNestedObjects);		

		for (UObject* Child : Children)
		{
			RenameObjectToTransientPackage(Child);
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateVariables(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	if (!BlueprintView)
	{
		return;
	}

	if (!AreStepsValid())
	{
		return;
	}

	if (Context.GetCompileType() == EKismetCompileType::SkeletonOnly)
	{
		CreateWidgetMap(Context);
		CreateBindingList(Context);
		CreateEventList(Context);
		CreateRequiredProperties(Context);
		CreatePublicFunctionsDeclaration(Context);
	}

	auto CreateVariable = [&Context](const FCompilerUserWidgetProperty& UserWidgetProperty) -> FProperty*
	{
		FEdGraphPinType NewPropertyPinType(UEdGraphSchema_K2::PC_Object, NAME_None, UserWidgetProperty.AuthoritativeClass, EPinContainerType::None, false, FEdGraphTerminalType());
		FProperty* NewProperty = Context.CreateVariable(UserWidgetProperty.Name, NewPropertyPinType);
		if (NewProperty != nullptr)
		{
			NewProperty->SetPropertyFlags(CPF_BlueprintVisible | CPF_RepSkip | CPF_Transient | CPF_DuplicateTransient);
			if (UserWidgetProperty.bReadOnly)
			{
				NewProperty->SetPropertyFlags(CPF_BlueprintReadOnly);
			}
			NewProperty->SetPropertyFlags(UserWidgetProperty.bExposeOnSpawn ? CPF_ExposeOnSpawn : CPF_DisableEditOnInstance);

#if WITH_EDITOR
			if (!UserWidgetProperty.BlueprintSetter.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_PropertySetFunction, *UserWidgetProperty.BlueprintSetter);
			}
			if (!UserWidgetProperty.DisplayName.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_DisplayName, *UserWidgetProperty.DisplayName.ToString());
			}
			if (!UserWidgetProperty.CategoryName.IsEmpty())
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *UserWidgetProperty.CategoryName);
			}
			if (UserWidgetProperty.bExposeOnSpawn)
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
			}
			if (UserWidgetProperty.bPrivate)
			{
				NewProperty->SetMetaData(FBlueprintMetadata::MD_Private, TEXT("true"));
			}
#endif
		}
		return NewProperty;
	};

	for (FCompilerUserWidgetProperty& UserWidgetProperty : NeededUserWidgetProperties)
	{
		check(UserWidgetProperty.AuthoritativeClass);
		check(!UserWidgetProperty.Name.IsNone());
		UserWidgetProperty.Property = nullptr; // Skeletal set the property, Full needs the new property

		FMVVMConstFieldVariant UserWidgetPropertyField = BindingHelper::FindFieldByName(Context.GetGeneratedClass(), FMVVMBindingName(UserWidgetProperty.Name));

		// The class is not linked yet. It may not be available yet.
		if (UserWidgetPropertyField.IsEmpty())
		{
			for (FField* Field = Context.GetGeneratedClass()->ChildProperties; Field != nullptr; Field = Field->Next)
			{
				if (Field->GetFName() == UserWidgetProperty.Name)
				{
					if (CastField<FProperty>(Field))
					{
						UserWidgetPropertyField = FMVVMFieldVariant(CastField<FProperty>(Field));
					}
					else
					{
						WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FieldIsNotProperty", "The field for source '{0}' exists but is not a property."), UserWidgetProperty.DisplayName).ToString());
						bIsCreateVariableStepValid = false;
					}
					break;
				}
			}
			for (UField* Field = Context.GetGeneratedClass()->Children; Field != nullptr; Field = Field->Next)
			{
				if (Field->GetFName() == UserWidgetProperty.Name)
				{
					WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FieldIsNotProperty", "The field for source '{0}' exists but is not a property."), UserWidgetProperty.DisplayName).ToString());
					bIsCreateVariableStepValid = false;
					break;
				}
			}
		}

		if (UserWidgetPropertyField.IsEmpty())
		{
			UClass* ParentClass = Context.GetGeneratedClass()->GetSuperClass();
			if (const FProperty* Property = ParentClass->FindPropertyByName(UserWidgetProperty.Name))
			{
				UserWidgetPropertyField = FMVVMFieldVariant(Property);
			}
		}

		// Will always create viewmodel properties.
		// Will never create properties for animation or other Self.Object
		// Will create properties for widget when they are not already created.
		if (UserWidgetProperty.CreationType == FCompilerUserWidgetProperty::ECreationType::CreateOnlyIfDoesntExist && !UserWidgetPropertyField.IsEmpty())
		{
			// Viewmodel property cannot already exist. It will creates issue with initialization and with View::SetViewModel.
			const UClass* OwnerClass = Cast<UClass>(UserWidgetPropertyField.GetOwner());
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("PropertyAlreadyExistInParent", "There is already a property named '{0}' in scope '{1}'."), UserWidgetProperty.DisplayName, (OwnerClass ? OwnerClass->GetDisplayNameText() : FText::GetEmpty())).ToString());
			bIsCreateVariableStepValid = false;
			continue;
		}

		if (!UserWidgetPropertyField.IsEmpty())
		{
			if (UserWidgetPropertyField.IsFunction())
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FunctionCanBeSource", "Function can't be source. '{0}'."), UserWidgetProperty.DisplayName).ToString());
				bIsCreateVariableStepValid = false;
				continue;
			}

			if (!BindingHelper::IsValidForSourceBinding(UserWidgetPropertyField))
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("FieldNotAccessibleAtRuntime", "The field for source '{0}' exists but is not accessible at runtime."), UserWidgetProperty.DisplayName).ToString());
				bIsCreateVariableStepValid = false;
				continue;
			}

			ensure(UserWidgetPropertyField.IsProperty());
			const FProperty* Property = UserWidgetPropertyField.IsProperty() ? UserWidgetPropertyField.GetProperty() : nullptr;
			const FObjectProperty* ObjectProperty = CastField<const FObjectProperty>(Property);
			const bool bIsCompatible = ObjectProperty && UserWidgetProperty.AuthoritativeClass->IsChildOf(ObjectProperty->PropertyClass);
			if (!bIsCompatible)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("PropertyExistsAndNotCompatible", "There is already a property named '{0}' that is not compatible with the source of the same name."), UserWidgetProperty.DisplayName).ToString());
				bIsCreateVariableStepValid = false;
				continue;
			}

			const bool bIsBindWidget = FWidgetBlueprintEditorUtils::IsBindWidgetProperty(ObjectProperty);
			if (Context.GetGeneratedClass() != ObjectProperty->GetOwnerStruct() && !bIsBindWidget)
			{
				// Widget needs to be BindWidget to be reused as a property.
				const UClass* OwnerClass = Cast<UClass>(ObjectProperty->GetOwnerStruct());
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("WidgetPropertyAlreadyExist", "There is already a property named '{0}' in scope '{1}' for the widget. Are you missing a BindWidget?."), UserWidgetProperty.DisplayName, (OwnerClass ? OwnerClass->GetDisplayNameText() : FText::GetEmpty())).ToString());
				bIsCreateVariableStepValid = false;
				continue;
			}
		}

		// Can we reused the property or we need to create a new one.
		bool bCreateVariable = UserWidgetProperty.CreationType == FCompilerUserWidgetProperty::ECreationType::CreateOnlyIfDoesntExist
			|| (UserWidgetProperty.CreationType == FCompilerUserWidgetProperty::ECreationType::CreateIfDoesntExist && UserWidgetPropertyField.IsEmpty());
		if (bCreateVariable)
		{
			UserWidgetProperty.Property = CreateVariable(UserWidgetProperty);
		}
		else if (UserWidgetPropertyField.IsProperty())
		{
			UserWidgetProperty.Property = UserWidgetPropertyField.GetProperty();
		}

		if (UserWidgetProperty.Property == nullptr)
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("VariableCouldNotBeCreated", "The variable for '{0}' could not be created."), UserWidgetProperty.DisplayName).ToString());
			bIsCreateVariableStepValid = false;
			continue;
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateWidgetMap(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	// The widget tree is not created yet for SKEL class.
	//Context.GetGeneratedClass()->GetWidgetTreeArchetype()
	WidgetNameToWidgetPointerMap.Reset();

	TArray<UWidget*> Widgets;
	UWidgetBlueprint* WidgetBPToScan = Context.GetWidgetBlueprint();
	while (WidgetBPToScan != nullptr)
	{
		Widgets = WidgetBPToScan->GetAllSourceWidgets();
		if (Widgets.Num() != 0)
		{
			break;
		}
		WidgetBPToScan = WidgetBPToScan->ParentClass && WidgetBPToScan->ParentClass->ClassGeneratedBy ? Cast<UWidgetBlueprint>(WidgetBPToScan->ParentClass->ClassGeneratedBy) : nullptr;
	}

	for (UWidget* Widget : Widgets)
	{
		WidgetNameToWidgetPointerMap.Add(Widget->GetFName(), Widget);
	}
}


void FMVVMViewBlueprintCompiler::CreateBindingList(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	ensure(Context.GetCompileType() == EKismetCompileType::SkeletonOnly);
	if (Context.GetCompileType() != EKismetCompileType::SkeletonOnly)
	{
		return;
	}

	ValidBindings.Reset();

	// Build the list of bindings that we should compile.
	for (int32 Index = 0; Index < BlueprintView->GetNumBindings(); ++Index)
	{
		FMVVMBlueprintViewBinding* BindingPtr = BlueprintView->GetBindingAt(Index);
		if (BindingPtr == nullptr)
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("BindingInvalidIndex", "Internal error: Tried to fetch binding for invalid index {0}."), Index).ToString());
			bIsCreateVariableStepValid = false;
			continue;
		}

		FMVVMBlueprintViewBinding& Binding = *BindingPtr;
		if (!Binding.bCompile)
		{
			continue;
		}

		const bool bHasConversionFunction = Binding.Conversion.GetConversionFunction(true) || Binding.Conversion.GetConversionFunction(false);
		if (bHasConversionFunction && Binding.BindingType == EMVVMBindingMode::TwoWay)
		{
			AddMessageForBinding(Binding, LOCTEXT("TwoWayBindingsWithConversion", "Two-way bindings are not allowed to use conversion functions."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsCreateVariableStepValid = false;
			continue;
		}

		if (IsForwardBinding(Binding.BindingType))
		{
			TSharedRef<FCompilerBinding> ValidBinding = MakeShared<FCompilerBinding>();
			ValidBinding->Key.ViewBindingIndex = Index;
			ValidBinding->Key.bIsForwardBinding = true;
			ValidBinding->bIsOneTimeBinding = IsOneTimeBinding(Binding.BindingType);

			ValidBindings.Add(ValidBinding);
		}
		if (IsBackwardBinding(Binding.BindingType))
		{
			TSharedRef<FCompilerBinding> ValidBinding = MakeShared<FCompilerBinding>();
			ValidBinding->Key.ViewBindingIndex = Index;
			ValidBinding->Key.bIsForwardBinding = false;
			ValidBinding->bIsOneTimeBinding = IsOneTimeBinding(Binding.BindingType);

			ValidBindings.Add(ValidBinding);
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateEventList(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	for (UMVVMBlueprintViewEvent* EventPtr : BlueprintView->GetEvents())
	{
		if (EventPtr == nullptr)
		{
			WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("EventInvalid", "Internal error: An event is invalid.").ToString());
			bIsCreateVariableStepValid = false;
			continue;
		}

		if (!EventPtr->bCompile)
		{
			continue;
		}

		if (!EventPtr->GetEventPath().HasPaths())
		{
			AddMessageForEvent(EventPtr, LOCTEXT("EventInvalidEventPath", "The event path is invalid."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsCreateVariableStepValid = false;
			continue;
		}

		TSharedRef<FCompilerEvent> ValidEvent = MakeShared<FCompilerEvent>();
		ValidEvent->Event = EventPtr;

		ValidEvents.Add(ValidEvent);
	}
}


void FMVVMViewBlueprintCompiler::CreateRequiredProperties(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	ensure(Context.GetCompileType() == EKismetCompileType::SkeletonOnly);
	if (Context.GetCompileType() != EKismetCompileType::SkeletonOnly)
	{
		return;
	}

	NeededBindingSources.Reset();
	NeededUserWidgetProperties.Reset();
	ViewModelCreatorContexts.Reset();
	ViewModelSettersToGenerate.Reset();

	TSet<FGuid> ViewModelGuids;
	for (const FMVVMBlueprintViewModelContext& ViewModelContext : BlueprintView->GetViewModels())
	{
		if (!ViewModelContext.GetViewModelId().IsValid())
		{
			AddMessageForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidGuid", "GUID is invalid."), EMessageType::Error);
			bIsCreateVariableStepValid = false;
			continue;
		}

		if (ViewModelGuids.Contains(ViewModelContext.GetViewModelId()))
		{
			AddMessageForViewModel(ViewModelContext, LOCTEXT("ViewmodelAlreadyAdded", "Identical viewmodel has already been added."), EMessageType::Error);
			bIsCreateVariableStepValid = false;
			continue;
		}

		ViewModelGuids.Add(ViewModelContext.GetViewModelId());

		if (ViewModelContext.GetViewModelClass() == nullptr || !ViewModelContext.IsValid())
		{
			AddMessageForViewModel(ViewModelContext, LOCTEXT("ViewmodelInvalidClass", "Invalid class."), EMessageType::Error);
			bIsCreateVariableStepValid = false;
			continue;
		}

		FName PropertyName = ViewModelContext.GetViewModelName();

		const bool bCreateSetterFunction = GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter
			&& (ViewModelContext.bCreateSetterFunction || ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual);
		FString SetterFunctionName;
		if (bCreateSetterFunction)
		{
			SetterFunctionName = TEXT("Set") + ViewModelContext.GetViewModelName().ToString();

			FCompilerViewModelSetter& ViewModelSetter = ViewModelSettersToGenerate.AddDefaulted_GetRef();
			ViewModelSetter.Class = ViewModelContext.GetViewModelClass();
			ViewModelSetter.PropertyName = PropertyName;
			ViewModelSetter.BlueprintSetter = SetterFunctionName;
			ViewModelSetter.DisplayName = ViewModelContext.GetDisplayName();
		}

		TSharedRef<FCompilerBindingSource> SourceContext = MakeShared<FCompilerBindingSource>();
		{
			SourceContext->AuthoritativeClass = ViewModelContext.GetViewModelClass();
			SourceContext->Name = PropertyName;
			SourceContext->Type = FCompilerBindingSource::EType::ViewModel;
			SourceContext->bIsOptional = ViewModelContext.bOptional;

			if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
			{
				SourceContext->bIsOptional = true;
			}
			else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
			{
				SourceContext->bIsOptional = false;
			}

			NeededBindingSources.Add(SourceContext);
		}

		{
			bool bIsPublicReadable = ViewModelContext.InstancedViewModel == nullptr && ViewModelContext.bCreateGetterFunction;

			FCompilerUserWidgetProperty& SourceVariable = NeededUserWidgetProperties.AddDefaulted_GetRef();
			SourceVariable.AuthoritativeClass = ViewModelContext.GetViewModelClass();
			SourceVariable.Name = PropertyName;
			SourceVariable.DisplayName = ViewModelContext.GetDisplayName();
			SourceVariable.CategoryName = TEXT("Viewmodel");
			SourceVariable.BlueprintSetter = SetterFunctionName;
			SourceVariable.CreationType = FCompilerUserWidgetProperty::ECreationType::CreateOnlyIfDoesntExist;
			SourceVariable.bExposeOnSpawn = bCreateSetterFunction;
			SourceVariable.bPrivate = !bIsPublicReadable;
			SourceVariable.bReadOnly = !bCreateSetterFunction;
		}

		{
			FCompilerViewModelCreatorContext& CreatorContext = ViewModelCreatorContexts.AddDefaulted_GetRef();
			CreatorContext.ViewModelContext = ViewModelContext;
			CreatorContext.Source = SourceContext;
		}
	}

	TSet<FName> WidgetSourcesCreated;
	TSet<FName> WidgetUserPropertyCreated;
	bool bSelfBindingSourceCreated = NeededBindingSources.ContainsByPredicate([](const TSharedRef<FCompilerBindingSource>& Other)
		{
			return Other->Type == FCompilerBindingSource::EType::Self;
		});
	const FName DefaultWidgetCategory = Context.GetWidgetBlueprint()->GetFName();
	auto GenerateCompilerContext = [Self = this, DefaultWidgetCategory, Class = Context.GetGeneratedClass(), &ViewModelGuids, &WidgetSourcesCreated, &WidgetUserPropertyCreated, &bSelfBindingSourceCreated](bool bInCreateSource, const FMVVMBlueprintPropertyPath& PropertyPath) -> TValueOrError<void, FText>
	{
		switch (PropertyPath.GetSource(Self->WidgetBlueprintCompilerContext.WidgetBlueprint()))
		{
		case EMVVMBlueprintFieldPathSource::SelfContext:
		{
			if (!bSelfBindingSourceCreated)
			{
				bSelfBindingSourceCreated = true;

				TSharedRef<FCompilerBindingSource> SourceContext = MakeShared<FCompilerBindingSource>();
				SourceContext->AuthoritativeClass = nullptr;
				SourceContext->Name = Self->WidgetBlueprintCompilerContext.WidgetBlueprint()->GetFName();
				SourceContext->Type = FCompilerBindingSource::EType::Self;
				SourceContext->bIsOptional = false;
				Self->NeededBindingSources.Add(SourceContext);
			}
			return MakeValue();
		}
		case EMVVMBlueprintFieldPathSource::Widget:
		{
			// Only do this once
			bool bNewCreateSource = !WidgetSourcesCreated.Contains(PropertyPath.GetWidgetName()) && bInCreateSource;
			bool bNewAddWidgetProperty = !WidgetUserPropertyCreated.Contains(PropertyPath.GetWidgetName());
			if (bNewCreateSource || bNewAddWidgetProperty)
			{
				UWidget** WidgetPtr = Self->WidgetNameToWidgetPointerMap.Find(PropertyPath.GetWidgetName());
				if (WidgetPtr == nullptr || *WidgetPtr == nullptr)
				{
					return MakeError(FText::Format(LOCTEXT("InvalidWidgetFormat", "Could not find the targeted widget: {0}"), FText::FromName(PropertyPath.GetWidgetName())));
				}
				UWidget* Widget = *WidgetPtr;

				if (bNewCreateSource)
				{
					FName PropertyName = PropertyPath.GetWidgetName();
					{
						TSharedRef<FCompilerBindingSource>* FoundCompilerSource = Self->NeededBindingSources.FindByPredicate([PropertyName](const TSharedRef<FCompilerBindingSource>& Other)
							{
								return Other->Name == PropertyName;
							});
						if (FoundCompilerSource != nullptr)
						{
							// It should be in the TSet<FName> WidgetSources
							return MakeError(FText::Format(LOCTEXT("ExistingWidgetSourceFormat", "Internal error. A widget source already exist: {0}"), FText::FromName(PropertyPath.GetWidgetName())));
						}
					}

					TSharedRef<FCompilerBindingSource> SourceContext = MakeShared<FCompilerBindingSource>();
					SourceContext->AuthoritativeClass = Widget->GetClass();
					SourceContext->Name = PropertyName;
					SourceContext->Type = FCompilerBindingSource::EType::Widget;
					SourceContext->bIsOptional = false;
					Self->NeededBindingSources.Add(SourceContext);

					WidgetSourcesCreated.Add(Widget->GetFName());
				}

				if (bNewAddWidgetProperty)
				{
					FCompilerUserWidgetProperty& SourceVariable = Self->NeededUserWidgetProperties.AddDefaulted_GetRef();
					SourceVariable.AuthoritativeClass = Widget->GetClass();
					SourceVariable.Name = PropertyPath.GetWidgetName();
					SourceVariable.DisplayName = FText::FromString(Widget->GetDisplayLabel());
					SourceVariable.CategoryName = TEXT("Widget");
					SourceVariable.CreationType = FCompilerUserWidgetProperty::ECreationType::CreateIfDoesntExist;
					SourceVariable.bExposeOnSpawn = false;
					//SourceVariable.bPrivate = true; Remove until the data is fix properly
					SourceVariable.bPrivate = false;
					SourceVariable.bReadOnly = true;

					WidgetUserPropertyCreated.Add(Widget->GetFName());
				}
			}
			break;
		}
		case EMVVMBlueprintFieldPathSource::ViewModel:
		{
			const FMVVMBlueprintViewModelContext* SourceViewModelContext = Self->BlueprintView->FindViewModel(PropertyPath.GetViewModelId());
			if (SourceViewModelContext == nullptr)
			{
				return MakeError(FText::Format(LOCTEXT("BindingViewModelNotFound", "Could not find viewmodel with GUID {0}."), GetViewModelIdText(PropertyPath)));
			}

			if (!ViewModelGuids.Contains(SourceViewModelContext->GetViewModelId()))
			{
				return MakeError(FText::Format(LOCTEXT("BindingViewModelInvalid", "Viewmodel {0} {1} was invalid."), SourceViewModelContext->GetDisplayName(), GetViewModelIdText(PropertyPath)));
			}
			break;
		}
		default:
			return MakeError(LOCTEXT("SourcePathNotSet", "A source path is required, but not set."));
		}
		return MakeValue();
	};

	// Find the start property for each path.
	//The full path will be tested later. We want to build the list of property needed before generating the graphs.
	for (const TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		const FMVVMBlueprintViewBinding& Binding = *(BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex));

		auto RunGenerateCompilerSourceContext = [Self = this, &GenerateCompilerContext, &Binding](bool bCreateSource, const FMVVMBlueprintPropertyPath& PropertyPath, const FMVVMBlueprintPinId& PinId)
			{
				TValueOrError<void, FText> SourceContextResult = GenerateCompilerContext(bCreateSource, PropertyPath);
				if (SourceContextResult.HasError())
				{
					Self->AddMessageForBinding(Binding, SourceContextResult.StealError(), EMessageType::Error, PinId);
					Self->bIsCreateVariableStepValid = false;
				}
			};

		UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(ValidBinding->Key.bIsForwardBinding);
		if (ConversionFunction)
		{
			// validate the sources
			for (const FMVVMBlueprintPin& Pin : ConversionFunction->GetPins())
			{
				if (Pin.UsedPathAsValue())
				{
					if (!Pin.IsValid())
					{
						AddMessageForBinding(Binding
							, FText::Format(LOCTEXT("InvalidFunctionArgumentPathName", "The conversion function {0} has an invalid argument."), FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint())))
							, EMessageType::Error
							, FMVVMBlueprintPinId()
						);
						bIsCreateVariableStepValid = false;
						continue;
					}
					RunGenerateCompilerSourceContext(true, Pin.GetPath(), Pin.GetId());
				}
			}

			// validate the destination
			const FMVVMBlueprintPropertyPath& BindingDestinationPath = ValidBinding->Key.bIsForwardBinding ? Binding.DestinationPath : Binding.SourcePath;
			RunGenerateCompilerSourceContext(false, BindingDestinationPath, FMVVMBlueprintPinId());
		}
		else
		{
			// if we aren't using a conversion function, validate the source and destination paths
			RunGenerateCompilerSourceContext(ValidBinding->Key.bIsForwardBinding, Binding.SourcePath, FMVVMBlueprintPinId());
			RunGenerateCompilerSourceContext(!ValidBinding->Key.bIsForwardBinding, Binding.DestinationPath, FMVVMBlueprintPinId());
		}
	}

	// Find the event property.
	//All inputs must also exist (for the BP to compile).
	for (TSharedRef<FCompilerEvent>& ValidEvent : ValidEvents)
	{
		UMVVMBlueprintViewEvent* EventPtr = ValidEvent->Event.Get();
		check(EventPtr);
		auto RunGenerateCompilerSourceContext = [Self = this, &GenerateCompilerContext, EventPtr](bool bCreateSource, const FMVVMBlueprintPropertyPath& PropertyPath, const FMVVMBlueprintPinId& PinId)
		{
			TValueOrError<void, FText> SourceContextResult = GenerateCompilerContext(bCreateSource, PropertyPath);
			if (SourceContextResult.HasError())
			{
				Self->AddMessageForEvent(EventPtr, SourceContextResult.StealError(), EMessageType::Error, PinId);
				Self->bIsCreateVariableStepValid = false;
			}
		};

		RunGenerateCompilerSourceContext(false, EventPtr->GetEventPath(), FMVVMBlueprintPinId());
		RunGenerateCompilerSourceContext(false, EventPtr->GetDestinationPath(), FMVVMBlueprintPinId());
		for (const FMVVMBlueprintPin& Pin : EventPtr->GetPins())
		{
			if (Pin.UsedPathAsValue())
			{
				if (!Pin.IsValid())
				{
					AddMessageForEvent(EventPtr
						, LOCTEXT("InvalidEventArgumentPathName", "The event has an invalid argument.")
						, EMessageType::Error
						, FMVVMBlueprintPinId()
					);
					bIsCreateVariableStepValid = false;
					continue;
				}
				RunGenerateCompilerSourceContext(false, Pin.GetPath(), Pin.GetId());
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreatePublicFunctionsDeclaration(const FWidgetBlueprintCompilerContext::FCreateVariableContext& Context)
{
	// Clean all previous intermediate function graph. It should stay alive. The graph lives on the Blueprint not on the class and it's used to generate the UFunction.
	{
		for (UEdGraph* OldGraph : BlueprintView->TemporaryGraph)
		{
			if (OldGraph)
			{
				RenameObjectToTransientPackage(OldGraph);
			}
		}
		BlueprintView->TemporaryGraph.Reset();
	}

	if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter)
	{
		for (FCompilerViewModelSetter& Setter : ViewModelSettersToGenerate)
		{
			ensure(Setter.SetterGraph == nullptr);

			Setter.SetterGraph = UE::MVVM::FunctionGraphHelper::CreateIntermediateFunctionGraph(
				WidgetBlueprintCompilerContext
				, Setter.BlueprintSetter
				, (FUNC_BlueprintCallable | FUNC_Public | FUNC_Final)
				, TEXT("Viewmodel")
				, false);
			BlueprintView->TemporaryGraph.Add(Setter.SetterGraph);

			if (Setter.SetterGraph == nullptr || Setter.SetterGraph->GetFName() != FName(*Setter.BlueprintSetter))
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("SetterNameAlreadyExists", "The setter name {0} already exists and could not be autogenerated."),
					FText::FromString(Setter.BlueprintSetter)
				).ToString()
				);
				bIsCreateVariableStepValid = false;
				continue;
			}

			UE::MVVM::FunctionGraphHelper::AddFunctionArgument(Setter.SetterGraph, const_cast<UClass*>(Setter.Class), "Viewmodel");
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateFunctions(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	if (!AreStepsValid())
	{
		return;
	}

	SourceViewModelDynamicCreatorContexts.Reset();

	CategorizeBindings(Context);
	CategorizeEvents(Context);
	CreateWriteFieldContexts(Context);
	CreateViewModelSetters(Context);
	CreateIntermediateGraphFunctions(Context);
}


void FMVVMViewBlueprintCompiler::CategorizeBindings(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	// Find the type of the bindings
	for (TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		FMVVMBlueprintViewBinding& Binding = *BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex);

		check(ValidBinding->Type == FCompilerBinding::EType::Unknown);
		ValidBinding->Type = FCompilerBinding::EType::Invalid;

		UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(ValidBinding->Key.bIsForwardBinding);
		if (ConversionFunction)
		{
			UClass* NewClass = WidgetBlueprintCompilerContext.NewClass;
			if (!ConversionFunction->IsValid(WidgetBlueprintCompilerContext.WidgetBlueprint()))
			{
				AddMessageForBinding(Binding, LOCTEXT("InvalidConversionFunction", "The conversion function is invalid."), EMessageType::Error, FMVVMBlueprintPinId());
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			// Make sure the graph is up to date
			UEdGraph* WrapperGraph = ConversionFunction->GetOrCreateIntermediateWrapperGraph(WidgetBlueprintCompilerContext);
			if (WrapperGraph == nullptr)
			{
				AddMessageForBinding(Binding, LOCTEXT("InvalidConversionFunctionGraph", "The conversion function graph could not be generated.")
					, EMessageType::Error
					, FMVVMBlueprintPinId()
				);
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			ConversionFunction->UpdatePinValues(WidgetBlueprintCompilerContext.WidgetBlueprint());

			if (ConversionFunction->HasOrphanedPin())
			{
				AddMessageForBinding(Binding, LOCTEXT("InvalidConversionFunctionGraphOrphaned", "The conversion function has an orphaned pin.")
					, EMessageType::Warning
					, FMVVMBlueprintPinId()
				);
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			if (ConversionFunction->GetPins().Num() == 0)
			{
				AddMessageForBinding(Binding
					, FText::Format(LOCTEXT("InvalidNumberOfFunctionPin", "The conversion function {0} has no source."), FText::FromString(Binding.GetDisplayNameString(WidgetBlueprintCompilerContext.WidgetBlueprint())))
					, EMessageType::Error
					, FMVVMBlueprintPinId()
				);
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			if (ConversionFunction->NeedsWrapperGraph(WidgetBlueprintCompilerContext.WidgetBlueprint()))
			{
				ValidBinding->Type = FCompilerBinding::EType::ComplexConversionFunction;
			}
			else
			{
				ValidBinding->Type = FCompilerBinding::EType::SimpleConversionFunction;
			}
			ValidBinding->ConversionFunction = ConversionFunction;

			// Because the editor use the destination to order the bindings, the destination path can be "valid". Pointing only to the WidgetName.
			//const FMVVMBlueprintPropertyPath& BindingSourcePath = ValidBinding->Key.bIsForwardBinding ? Binding.SourcePath : Binding.DestinationPath;
			//if (BindingSourcePath.IsValid())
			//{
			//	AddMessageForBinding(Binding, LOCTEXT("ShouldNotHaveSourceWarning", "Internal Error. The binding should not have a source."), EMessageType::Warning, FMVVMBlueprintPinId());
			//}
		}
		else
		{
			ValidBinding->Type = FCompilerBinding::EType::Assignment;
		}
	}
}


void FMVVMViewBlueprintCompiler::CategorizeEvents(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	for (TSharedRef<FCompilerEvent>& Event : ValidEvents)
	{
		Event->Type = FCompilerEvent::EType::Invalid;

		UMVVMBlueprintViewEvent* EventPtr = Event->Event.Get();
		UEdGraph* WrapperGraph = EventPtr->GetOrCreateWrapperGraph();
		if (WrapperGraph == nullptr)
		{
			AddMessageForEvent(EventPtr, LOCTEXT("InvalidEventGraph", "The event could not be generated."), EMessageType::Warning, FMVVMBlueprintPinId());
			bIsCreateFunctionsStepValid = false;
			continue;
		}

		EventPtr->UpdatePinValues();

		if (EventPtr->HasOrphanedPin())
		{
			AddMessageForEvent(EventPtr, LOCTEXT("InvalidEventGraphOrphaned", "The event has an orphaned pin."), EMessageType::Warning, FMVVMBlueprintPinId());
			bIsCreateFunctionsStepValid = false;
			continue;
		}

		Event->Type = FCompilerEvent::EType::Valid;
	}
}


void FMVVMViewBlueprintCompiler::CreateWriteFieldContexts(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	// Use the Skeleton class. The class bind and not all functions are generated yet
	UWidgetBlueprintGeneratedClass* NewSkeletonClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprintCompilerContext.Blueprint->SkeletonGeneratedClass);
	if (NewSkeletonClass == nullptr)
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("InvalidNewClass", "Internal error. The skeleton class is not valid.").ToString());
		bIsCreateFunctionsStepValid = false;
		return;
	}

	for (TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		const FMVVMBlueprintViewBinding& Binding = *BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex);

		if (ValidBinding->Type == FCompilerBinding::EType::Assignment
			|| ValidBinding->Type == FCompilerBinding::EType::SimpleConversionFunction
			|| ValidBinding->Type == FCompilerBinding::EType::ComplexConversionFunction)
		{
			const FMVVMBlueprintPropertyPath& DestinationPropertyPath = ValidBinding->Key.bIsForwardBinding ? Binding.DestinationPath : Binding.SourcePath;
			TValueOrError<FCreateFieldsResult, FText> FieldContextResult = CreateFieldContext(NewSkeletonClass, DestinationPropertyPath, false);
			if (FieldContextResult.HasError())
			{
				AddMessageForBinding(Binding, FieldContextResult.StealError(), EMessageType::Error, FMVVMBlueprintPinId());
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			// Test if it already exist
			const TArray<UE::MVVM::FMVVMConstFieldVariant>& SkeletalGeneratedFieldsResult = FieldContextResult.GetValue().SkeletalGeneratedFields;
			{
				TSharedRef<FGeneratedWriteFieldPathContext>* Found = GeneratedWriteFieldPaths.FindByPredicate([&SkeletalGeneratedFieldsResult](const TSharedRef<FGeneratedWriteFieldPathContext>& Other)
					{
						return Other->SkeletalGeneratedFields == SkeletalGeneratedFieldsResult;
					});
				if (Found)
				{
					// Temporary removing this message until the assets are fixed.
					//AddMessageForBinding(Binding
					//	, FText::Format(LOCTEXT("PropertyPathAlreadyUsed", "The property path '{0}' is already used by another binding."), PropertyPathToText(NewSkeletonClass, BlueprintView.Get(), DestinationPropertyPath))
					//	, EMessageType::Warning
					//	, FMVVMBlueprintPinId()
					//);
					ValidBinding->WritePath = *Found;
					continue;
				}
			}

			TSharedRef<FGeneratedWriteFieldPathContext> WriteFieldPath = MakeShared<FGeneratedWriteFieldPathContext>();
			GeneratedWriteFieldPaths.Add(WriteFieldPath);
			WriteFieldPath->UsedByBindings.AddUnique(ValidBinding);
			WriteFieldPath->GeneratedFields = MoveTemp(FieldContextResult.GetValue().GeneratedFields);
			WriteFieldPath->SkeletalGeneratedFields = MoveTemp(FieldContextResult.GetValue().SkeletalGeneratedFields);
			WriteFieldPath->GeneratedFrom = DestinationPropertyPath.GetSource(WidgetBlueprintCompilerContext.WidgetBlueprint());
			WriteFieldPath->bCanBeSetInNative = CanBeSetInNative(WriteFieldPath->SkeletalGeneratedFields);
			WriteFieldPath->bUseByNativeBinding = true; // the setter function can be in BP but the destination will be set in native

			// Assign the Destination to the binding
			ValidBinding->WritePath = WriteFieldPath;
		}
	}

	//The destination for event must also exist (for the BP to compile).
	for (TSharedRef<FCompilerEvent>& ValidEvent : ValidEvents)
	{
		UMVVMBlueprintViewEvent* EventPtr = ValidEvent->Event.Get();
		check(EventPtr);

		if (ValidEvent->Type == FCompilerEvent::EType::Valid)
		{
			TValueOrError<FCreateFieldsResult, FText> FieldContextResult = CreateFieldContext(NewSkeletonClass, EventPtr->GetDestinationPath(), false);
			if (FieldContextResult.HasError())
			{
				AddMessageForEvent(EventPtr
					, FText::Format(Private::PropertyPathIsInvalidFormat, PropertyPathToText(NewSkeletonClass, BlueprintView.Get(), EventPtr->GetDestinationPath()))
					, EMessageType::Error
					, FMVVMBlueprintPinId()
				);
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			// Test if it already exist
			const TArray<UE::MVVM::FMVVMConstFieldVariant>& SkeletalGeneratedFieldsResult = FieldContextResult.GetValue().SkeletalGeneratedFields;
			{
				TSharedRef<FGeneratedWriteFieldPathContext>* Found = GeneratedWriteFieldPaths.FindByPredicate([&SkeletalGeneratedFieldsResult](const TSharedRef<FGeneratedWriteFieldPathContext>& Other) { return Other->SkeletalGeneratedFields == SkeletalGeneratedFieldsResult; });
				if (Found)
				{
					(*Found)->UsedByEvents.AddUnique(ValidEvent);
					ValidEvent->WritePath = *Found;
				}
				else
				{
					TSharedRef<FGeneratedWriteFieldPathContext> WriteFieldPath = MakeShared<FGeneratedWriteFieldPathContext>();
					GeneratedWriteFieldPaths.Add(WriteFieldPath);
					WriteFieldPath->UsedByEvents.AddUnique(ValidEvent);
					WriteFieldPath->GeneratedFields = MoveTemp(FieldContextResult.GetValue().GeneratedFields);
					WriteFieldPath->SkeletalGeneratedFields = MoveTemp(FieldContextResult.GetValue().SkeletalGeneratedFields);
					WriteFieldPath->GeneratedFrom = EventPtr->GetDestinationPath().GetSource(WidgetBlueprintCompilerContext.WidgetBlueprint());
					WriteFieldPath->bCanBeSetInNative = CanBeSetInNative(WriteFieldPath->SkeletalGeneratedFields);
					WriteFieldPath->bUseByNativeBinding = false;

					// Assign the Destination to the events
					ValidEvent->WritePath = WriteFieldPath;
				}
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateViewModelSetters(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowGeneratedViewModelSetter)
	{
		for (FCompilerViewModelSetter& Setter : ViewModelSettersToGenerate)
		{
			if (ensure(Setter.SetterGraph != nullptr))
			{
				if (!UE::MVVM::FunctionGraphHelper::GenerateViewModelSetter(WidgetBlueprintCompilerContext, Setter.SetterGraph, Setter.PropertyName))
				{
					AddMessageForViewModel(Setter.DisplayName, LOCTEXT("SetterFunctionCouldNotBeGenerated", "The setter function could not be generated."), EMessageType::Warning);
					bIsCreateFunctionsStepValid = false;
					continue;
				}
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateIntermediateGraphFunctions(const FWidgetBlueprintCompilerContext::FCreateFunctionContext& Context)
{
	// Add function to set destination if needed
	for (TSharedRef<FGeneratedWriteFieldPathContext>& GeneratedDestination : GeneratedWriteFieldPaths)
	{
		// If the destination can't be set in cpp, we need to generate a BP function to set the value.
		if (!GeneratedDestination->bCanBeSetInNative && GeneratedDestination->bUseByNativeBinding)
		{
			auto AddErrorMessage = [Self = this, &GeneratedDestination](const FText& ErrorMsg)
			{
				Self->AddMessages(GeneratedDestination->UsedByBindings, GeneratedDestination->UsedByEvents, ErrorMsg, EMessageType::Error);
			};

			const FProperty* SetterProperty = nullptr;
			if (ensure(GeneratedDestination->SkeletalGeneratedFields.Num() > 0 && GeneratedDestination->SkeletalGeneratedFields.Last().IsProperty()))
			{
				SetterProperty = GeneratedDestination->SkeletalGeneratedFields.Last().GetProperty();
			}

			if (SetterProperty == nullptr)
			{
				AddErrorMessage(FText::Format(LOCTEXT("CantGetSetter", "Internal Error. The setter function was not created. {0}"), ::UE::MVVM::FieldPathHelper::ToText(GeneratedDestination->GeneratedFields)));
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			// create a setter function to be called from native. For now we follow the convention of Setter(Conversion(Getter))
			UEdGraph* GeneratedSetterGraph = UE::MVVM::FunctionGraphHelper::CreateIntermediateFunctionGraph(WidgetBlueprintCompilerContext, FString::Printf(TEXT("__Setter_%s"), *SetterProperty->GetName()), EFunctionFlags::FUNC_None, TEXT("AutogeneratedSetter"), false);
			if (GeneratedSetterGraph == nullptr)
			{
				AddErrorMessage(FText::Format(LOCTEXT("CantCreateSetter", "Internal Error. The setter function was not created. {0}"), ::UE::MVVM::FieldPathHelper::ToText(GeneratedDestination->GeneratedFields)));
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			UE::MVVM::FunctionGraphHelper::AddFunctionArgument(GeneratedSetterGraph, SetterProperty, "NewValue");

			// set GeneratedSetterFunction. Use the SkeletalSetterPath here to use the setter will be generated when the function is generated.
			if (!UE::MVVM::FunctionGraphHelper::GenerateIntermediateSetter(WidgetBlueprintCompilerContext, GeneratedSetterGraph, GeneratedDestination->SkeletalGeneratedFields))
			{
				AddErrorMessage(FText::Format(LOCTEXT("CantGeneratedSetter", "Internal Error. The setter function was not generated. {0}"), ::UE::MVVM::FieldPathHelper::ToText(GeneratedDestination->GeneratedFields)));
				bIsCreateFunctionsStepValid = false;
				continue;
			}

			// the new path can only be set later, once the function is compiled.
			GeneratedDestination->GeneratedFunctionName = GeneratedSetterGraph->GetFName();
		}
	}

	// Add Generated Conversion functions to the blueprint
	for (const TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		FMVVMBlueprintViewBinding& Binding = *BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex);

		if (ValidBinding->Type == FCompilerBinding::EType::ComplexConversionFunction)
		{
			UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(ValidBinding->Key.bIsForwardBinding);
			check(ConversionFunction);

			UEdGraph* WrapperGraph = ConversionFunction->GetOrCreateIntermediateWrapperGraph(WidgetBlueprintCompilerContext);
			if (ensure(WrapperGraph) && ConversionFunction->IsWrapperGraphTransient())
			{
				bool bAlreadyContained = WidgetBlueprintCompilerContext.Blueprint->FunctionGraphs.Contains(WrapperGraph);
				if (ensure(!bAlreadyContained))
				{
					Context.AddGeneratedFunctionGraph(WrapperGraph);
				}
			}
		}
	}

	// Add Generated event to the blueprint
	for (TSharedRef<FCompilerEvent>& Event : ValidEvents)
	{
		UMVVMBlueprintViewEvent* EventPtr = Event->Event.Get();
		if (Event->Type == FCompilerEvent::EType::Valid)
		{
			UEdGraph* WrapperGraph = EventPtr->GetOrCreateWrapperGraph();
			ensure(WrapperGraph);

			bool bAlreadyContained = WidgetBlueprintCompilerContext.Blueprint->FunctionGraphs.Contains(WrapperGraph);
			if (ensure(!bAlreadyContained))
			{
				Context.AddGeneratedFunctionGraph(WrapperGraph);
			}
		}
	}
}


bool FMVVMViewBlueprintCompiler::PreCompile(UWidgetBlueprintGeneratedClass* Class)
{
	if (!AreStepsValid())
	{
		return false;
	}

	FixCompilerBindingSelfSource(Class);
	AddWarningForPropertyWithMVVMAndLegacyBinding(Class);

	GeneratedReadFieldPaths.Reset();
	FixWriteFieldPathContext(Class);
	CreateReadFieldContexts(Class);
	CreateCreatorContentFromBindingSource(Class);

	if (!AreStepsValid())
	{
		return false;
	}

	PreCompileViewModelCreatorContexts(Class);
	PreCompileBindings(Class);
	PreCompileEvents(Class);

	return AreStepsValid();
}


bool FMVVMViewBlueprintCompiler::Compile(UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	if (!AreStepsValid())
	{
		return false;
	}

	TValueOrError<FCompiledBindingLibraryCompiler::FCompileResult, FText> CompileResult = BindingLibraryCompiler.Compile(BlueprintView->GetCompiledBindingLibraryId());
	if (CompileResult.HasError())
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("BindingCompilationFailed", "The binding compilation failed. {1}"), CompileResult.GetError()).ToString());
		return false;
	}
	CompileSources(CompileResult.GetValue(), Class, ViewExtension);
	CompileBindings(CompileResult.GetValue(), Class, ViewExtension);
	CompileEvaluateSources(CompileResult.GetValue(), Class, ViewExtension);
	CompileEvents(CompileResult.GetValue(), Class, ViewExtension);
	SortSourceFields(CompileResult.GetValue(), Class, ViewExtension);

	{
		ViewExtension->bInitializeSourcesOnConstruct = BlueprintView->GetSettings()->bInitializeSourcesOnConstruct;
		ViewExtension->bInitializeBindingsOnConstruct = ViewExtension->bInitializeSourcesOnConstruct ? BlueprintView->GetSettings()->bInitializeBindingsOnConstruct : false;
		ViewExtension->bInitializeEventsOnConstruct = BlueprintView->GetSettings()->bInitializeEventsOnConstruct;
	}
	{
		ViewExtension->OptionalSources = 0;
		for (int32 Index = 0; Index < ViewExtension->Sources.Num(); ++Index)
		{
			const FMVVMViewClass_Source& Source = ViewExtension->Sources[Index];
			if (Source.IsOptional())
			{
				FMVVMViewClass_SourceKey ClassSourceKey = FMVVMViewClass_SourceKey(Index);
				ViewExtension->OptionalSources |= ClassSourceKey.GetBit();
			}
		}
	}

	bool bResult = AreStepsValid();
	if (bResult)
	{
		ViewExtension->BindingLibrary = MoveTemp(CompileResult.GetValue().Library);

#if UE_WITH_MVVM_DEBUGGING
		if (CVarLogViewCompiledResult->GetBool())
		{
			UMVVMViewClass::FToStringArgs ToStringArgs = UMVVMViewClass::FToStringArgs::All();
			ToStringArgs.Source.bUseDisplayName = false;
			ToStringArgs.Binding.bUseDisplayName = false;
			ToStringArgs.Evaluate.bUseDisplayName = false;
			ToStringArgs.Event.bUseDisplayName = false;
			UE_LOG(LogMVVM, Log, TEXT("%s"), *ViewExtension->ToString(ToStringArgs));
		}
#endif
	}

	return AreStepsValid();
}


void FMVVMViewBlueprintCompiler::FixWriteFieldPathContext(UWidgetBlueprintGeneratedClass* Class)
{
	for (TSharedRef<FGeneratedWriteFieldPathContext>& GeneratedDestination : GeneratedWriteFieldPaths)
	{
		// If a graph was generated. Use it instead for the SkeletalGeneratedFields
		if (!GeneratedDestination->GeneratedFunctionName.IsNone())
		{
			UFunction* GeneratedFunction = Class->FindFunctionByName(GeneratedDestination->GeneratedFunctionName);
			if (GeneratedFunction == nullptr)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("CantFindGeneratedBindingSetterFunction", "Internal Error. The setter function was not generated.").ToString());
				bIsPreCompileStepValid = false;
				continue;
			}

			GeneratedDestination->GeneratedFields.Reset();
			GeneratedDestination->GeneratedFields.Add(UE::MVVM::FMVVMConstFieldVariant(GeneratedFunction));
			// note the function is not added on the skeletal class
			GeneratedDestination->SkeletalGeneratedFields.Reset();
			GeneratedDestination->SkeletalGeneratedFields.Add(UE::MVVM::FMVVMConstFieldVariant(GeneratedFunction));
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateReadFieldContexts(UWidgetBlueprintGeneratedClass* Class)
{
	auto AlreadyExist = [Self = this](const TArray<UE::MVVM::FMVVMConstFieldVariant>& SkeletalGeneratedFields) -> TSharedPtr<FGeneratedReadFieldPathContext>
		{
			// Test if it already exist
			TSharedRef<FGeneratedReadFieldPathContext>* Found = Self->GeneratedReadFieldPaths.FindByPredicate([&SkeletalGeneratedFields](const TSharedRef<FGeneratedReadFieldPathContext>& Other) { return Other->SkeletalGeneratedFields == SkeletalGeneratedFields; });
			return Found != nullptr ? *Found : TSharedPtr<FGeneratedReadFieldPathContext>();
		};

	for (TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		const FMVVMBlueprintViewBinding& Binding = *BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex);

		auto CreateBindingSourceContext = [Self = this, Class, &ValidBinding, &Binding](const FMVVMBlueprintPropertyPath& PropertyPath, const FMVVMBlueprintPinId& PinId) -> TValueOrError<FCreateFieldsResult, void>
		{
			TValueOrError<FCreateFieldsResult, FText> FieldContextResult = Self->CreateFieldContext(Class, PropertyPath, true);
			if (FieldContextResult.HasError())
			{
				Self->AddMessageForBinding(Binding
					, FText::Format(Private::PropertyPathIsInvalidFormat, PropertyPathToText(Class, Self->BlueprintView.Get(), PropertyPath))
					, EMessageType::Error
					, PinId
				);
				Self->bIsPreCompileStepValid = false;
				return MakeError();
			}

			return MakeValue(FieldContextResult.StealValue());
		};

		auto CreateFieldId = [Self = this, Class, &Binding, bIsOneTimeBinding = ValidBinding->bIsOneTimeBinding](const FMVVMBlueprintPropertyPath& PropertyPath, TSharedPtr<FGeneratedReadFieldPathContext>& ReadFieldContext, const FMVVMBlueprintPinId& PinId) -> TValueOrError<void, void>
		{
			if (!ReadFieldContext->NotificationField.IsValid())
			{
				TValueOrError<TSharedPtr<FCompilerNotifyFieldId>, FText> CreateFieldResult = Self->CreateNotifyFieldId(Class, ReadFieldContext, Binding);
				if (CreateFieldResult.HasError())
				{
					Self->AddMessageForBinding(Binding
						, FText::Format(LOCTEXT("CreateNotifyFieldIdFailedInvalidSelfContext", "The property path '{0}' is invalid. {1}"), PropertyPathToText(Class, Self->BlueprintView.Get(), PropertyPath), CreateFieldResult.StealError())
						, EMessageType::Error
						, PinId
					);
					return MakeError();
				}

				if (CreateFieldResult.GetValue())
				{
					// Sanity check
					{
						// if there is a FieldId associated with the read property
						if (CreateFieldResult.GetValue()->Source)
						{
							EMVVMBlueprintFieldPathSource PathSource = PropertyPath.GetSource(Self->WidgetBlueprintCompilerContext.WidgetBlueprint());
							bool bValidViewModel = CreateFieldResult.GetValue()->Source->Type == FCompilerBindingSource::EType::ViewModel && PathSource == EMVVMBlueprintFieldPathSource::ViewModel;
							bool bValidWidget = CreateFieldResult.GetValue()->Source->Type == FCompilerBindingSource::EType::Widget && PathSource == EMVVMBlueprintFieldPathSource::Widget;
							bool bDynamic = CreateFieldResult.GetValue()->Source->Type == FCompilerBindingSource::EType::DynamicViewmodel;
							bool bSelf = CreateFieldResult.GetValue()->Source->Type == FCompilerBindingSource::EType::Self && PathSource == EMVVMBlueprintFieldPathSource::SelfContext;
							if (!(bValidViewModel || bValidWidget || bDynamic || bSelf))
							{
								Self->AddMessageForBinding(Binding
									, FText::Format(LOCTEXT("CreateNotifyFieldIdFailedInvalidInvalidContext", "Internal error. The property path '{0}' is invalid. The context is invalid."), PropertyPathToText(Class, Self->BlueprintView.Get(), PropertyPath))
									, EMessageType::Error
									, PinId
								);
								return MakeError();
							}
						}
					}

					ReadFieldContext->NotificationField = CreateFieldResult.GetValue();
					ReadFieldContext->OptionalSource = ReadFieldContext->NotificationField->Source;
				}
			}
			return MakeValue();
		};

		if (ValidBinding->Type == FCompilerBinding::EType::Assignment)
		{
			const FMVVMBlueprintPropertyPath& BindingSourcePath = ValidBinding->Key.bIsForwardBinding ? Binding.SourcePath : Binding.DestinationPath;
			TValueOrError<FCreateFieldsResult, void> CreateSourceResult = CreateBindingSourceContext(BindingSourcePath, FMVVMBlueprintPinId());
			if (CreateSourceResult.HasError())
			{
				continue;
			}

			TSharedPtr<FGeneratedReadFieldPathContext> Found = AlreadyExist(CreateSourceResult.GetValue().SkeletalGeneratedFields);
			if (!Found)
			{
				Found = MakeShared<FGeneratedReadFieldPathContext>();
				Found->OptionalSource = MoveTemp(CreateSourceResult.GetValue().Source);
				Found->GeneratedFields = MoveTemp(CreateSourceResult.GetValue().GeneratedFields);
				Found->SkeletalGeneratedFields = MoveTemp(CreateSourceResult.GetValue().SkeletalGeneratedFields);

				if (CreateFieldId(BindingSourcePath, Found, FMVVMBlueprintPinId()).HasError())
				{
					bIsPreCompileStepValid = false;
					continue;
				}

				GeneratedReadFieldPaths.Add(Found.ToSharedRef());
			}

			Found->UsedByBindings.AddUnique(ValidBinding);
			ValidBinding->ReadPaths.Add(Found.ToSharedRef());
		}
		else if (ValidBinding->Type == FCompilerBinding::EType::SimpleConversionFunction
			|| ValidBinding->Type == FCompilerBinding::EType::ComplexConversionFunction)
		{
			UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(ValidBinding->Key.bIsForwardBinding);
			if (ensure(ConversionFunction))
			{
				for (const FMVVMBlueprintPin& Pin : ConversionFunction->GetPins())
				{
					if (Pin.UsedPathAsValue())
					{
						TValueOrError<FCreateFieldsResult, void> CreateSourceResult = CreateBindingSourceContext(Pin.GetPath(), Pin.GetId());
						if (CreateSourceResult.HasError())
						{
							continue;
						}

						TSharedPtr<FGeneratedReadFieldPathContext> Found = AlreadyExist(CreateSourceResult.GetValue().SkeletalGeneratedFields);
						if (!Found)
						{
							Found = MakeShared<FGeneratedReadFieldPathContext>();
							Found->OptionalSource = MoveTemp(CreateSourceResult.GetValue().Source);
							Found->GeneratedFields = MoveTemp(CreateSourceResult.GetValue().GeneratedFields);
							Found->SkeletalGeneratedFields = MoveTemp(CreateSourceResult.GetValue().SkeletalGeneratedFields);

							if (CreateFieldId(Pin.GetPath(), Found, Pin.GetId()).HasError())
							{
								bIsPreCompileStepValid = false;
								continue;
							}

							GeneratedReadFieldPaths.Add(Found.ToSharedRef());
						}

						Found->UsedByBindings.AddUnique(TWeakPtr<FCompilerBinding>(ValidBinding));
						ValidBinding->ReadPaths.Add(Found.ToSharedRef());
					}
				}
			}
		}
	}

	//The pins for event
	for (TSharedRef<FCompilerEvent>& ValidEvent : ValidEvents)
	{
		UMVVMBlueprintViewEvent* EventPtr = ValidEvent->Event.Get();
		check(EventPtr);
		if (ValidEvent->Type == FCompilerEvent::EType::Valid)
		{
			for (const FMVVMBlueprintPin& Pin : EventPtr->GetPins())
			{
				if (Pin.UsedPathAsValue())
				{
					TValueOrError<FCreateFieldsResult, FText> CreateSourceResult = CreateFieldContext(Class, Pin.GetPath(), true);
					if (CreateSourceResult.HasError())
					{
						AddMessageForEvent(ValidEvent
							, FText::Format(Private::PropertyPathIsInvalidFormat, PropertyPathToText(Class, BlueprintView.Get(), Pin.GetPath()))
							, EMessageType::Error
							, Pin.GetId()
						);
						bIsPreCompileStepValid = false;
					}

					TSharedPtr<FGeneratedReadFieldPathContext> Found = AlreadyExist(CreateSourceResult.GetValue().SkeletalGeneratedFields);
					if (!Found)
					{
						Found = MakeShared<FGeneratedReadFieldPathContext>();
						Found->OptionalSource = MoveTemp(CreateSourceResult.GetValue().Source);
						Found->GeneratedFields = MoveTemp(CreateSourceResult.GetValue().GeneratedFields);
						Found->SkeletalGeneratedFields = MoveTemp(CreateSourceResult.GetValue().SkeletalGeneratedFields);
						GeneratedReadFieldPaths.Add(Found.ToSharedRef());
					}

					Found->UsedByEvents.AddUnique(ValidEvent);
					ValidEvent->ReadPaths.Add(Found.ToSharedRef());
				}
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CreateCreatorContentFromBindingSource(UWidgetBlueprintGeneratedClass* Class)
{
	// Add all the needed sources that are not viewmodel (so not in the SourceCreators)
	for (const TSharedRef<FCompilerBindingSource>& Source : NeededBindingSources)
	{
		{
			const bool bSourceIsViewModel = (Source->Type == FCompilerBindingSource::EType::ViewModel || Source->Type == FCompilerBindingSource::EType::DynamicViewmodel);
			const FCompilerViewModelCreatorContext* FoundSourceCreator = ViewModelCreatorContexts.FindByPredicate([Source](const FCompilerViewModelCreatorContext& Other) { return Other.Source == Source; });
			if (bSourceIsViewModel && FoundSourceCreator == nullptr)
			{
				WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("ViewmodelSourceNotAdded", "Internal error. A viewmodel was not added to the compiled list.").ToString());
				bIsPreCompileStepValid = false;
			}
		}

		const bool bSourceIsWidget = Source->Type == FCompilerBindingSource::EType::Widget || Source->Type == FCompilerBindingSource::EType::Self;
		if (bSourceIsWidget)
		{
			FCompilerWidgetCreatorContext& CompiledSourceCreator = WidgetCreatorContexts.AddDefaulted_GetRef();
			CompiledSourceCreator.Source = Source;
			CompiledSourceCreator.bSelfReference = Source->Type == FCompilerBindingSource::EType::Self;
		}
	}
}


void FMVVMViewBlueprintCompiler::FixCompilerBindingSelfSource(UWidgetBlueprintGeneratedClass* Class)
{
	int32 Counter = 0;
	for (TSharedRef<FCompilerBindingSource>& Source : NeededBindingSources)
	{
		if (Source->Type == FCompilerBindingSource::EType::Self)
		{
			ensure(Source->AuthoritativeClass == nullptr);
			Source->AuthoritativeClass = Class;
			++Counter;
		}
	}

	if (Counter > 1)
	{
		AddMessage(LOCTEXT("MoreThanSeldContext", "Internal error. There is more than self context.")
			, EMessageType::Warning
		);
	}
}


void FMVVMViewBlueprintCompiler::AddWarningForPropertyWithMVVMAndLegacyBinding(UWidgetBlueprintGeneratedClass* Class)
{
	const TArray<FDelegateRuntimeBinding>& LegacyBindings = Class->Bindings;
	for (const TSharedRef<FGeneratedWriteFieldPathContext>& WriteFieldPath : GeneratedWriteFieldPaths)
	{
		FName MVVMObjectName;
		FName MVVMFieldName;
		if (WriteFieldPath->GeneratedFrom == EMVVMBlueprintFieldPathSource::SelfContext)
		{
			MVVMObjectName = Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetFName() : FName();
			MVVMFieldName = WriteFieldPath->SkeletalGeneratedFields.Num() > 0 ? WriteFieldPath->SkeletalGeneratedFields[0].GetName() : FName();
		}

		for (int32 Index = 0; Index < WriteFieldPath->SkeletalGeneratedFields.Num(); ++Index)
		{
			const UE::MVVM::FMVVMConstFieldVariant& Field = WriteFieldPath->SkeletalGeneratedFields[Index];
			const FObjectPropertyBase* ObjectProperty = Field.IsProperty() ? CastField<const FObjectPropertyBase>(Field.GetProperty()) : nullptr;
			if (ObjectProperty && ObjectProperty->PropertyClass->IsChildOf(UWidget::StaticClass()))
			{
				MVVMObjectName = ObjectProperty->GetFName();
				MVVMFieldName = WriteFieldPath->SkeletalGeneratedFields.IsValidIndex(Index+1) ? WriteFieldPath->SkeletalGeneratedFields[Index+1].GetName() : FName();
			}
		}

		for (const FDelegateRuntimeBinding& LegacyBinding : LegacyBindings)
		{
			if (LegacyBinding.ObjectName == MVVMObjectName && LegacyBinding.PropertyName == MVVMFieldName)
			{
				if (WriteFieldPath->UsedByBindings.Num())
				{
					AddMessages(WriteFieldPath->UsedByBindings
						, TArrayView<TWeakPtr<FCompilerEvent>>()
						, LOCTEXT("BindingConflictWithLegacy", "The binding is set on a property with legacy binding.")
						, EMessageType::Warning
					);
				}
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::PreCompileViewModelCreatorContexts(UWidgetBlueprintGeneratedClass* Class)
{
	for (FCompilerViewModelCreatorContext& SourceCreatorContext : ViewModelCreatorContexts)
	{
		const FMVVMBlueprintViewModelContext& ViewModelContext = SourceCreatorContext.ViewModelContext;
		checkf(ViewModelContext.GetViewModelClass(), TEXT("The viewmodel class is invalid. It was checked in CreateSourceList"));

		if (ViewModelContext.GetViewModelClass()->HasAllClassFlags(CLASS_Deprecated))
		{
			AddMessageForViewModel(ViewModelContext
				, FText::Format(LOCTEXT("ViewModelTypeDeprecated", "Viewmodel class '{0}' is deprecated and should not be used. Please update it in the View Models panel."), ViewModelContext.GetViewModelClass()->GetDisplayNameText())
				, EMessageType::Warning
			);
		}

		if (!ViewModelContext.GetViewModelClass()->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
		{
			AddMessageForViewModel(ViewModelContext
				, LOCTEXT("ViewmodelInvalidInterface", "The class doesn't implement the interface NotifyFieldValueChanged.")
				, EMessageType::Error
			);
			bIsPreCompileStepValid = false;
			continue;
		}

		// Test the creation mode. Dynamic source are always using Path.
		if (SourceCreatorContext.DynamicContext == nullptr)
		{
			if (!GetAllowedContextCreationType(ViewModelContext.GetViewModelClass()).Contains(ViewModelContext.CreationType))
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewModelContextCreationTypeInvalid", "It has an invalidate creation type. You can change it in the View Models panel.")
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}
		}

		if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
		{
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
		{
			if (ViewModelContext.GetViewModelClass()->HasAllClassFlags(CLASS_Abstract))
			{
				AddMessageForViewModel(ViewModelContext
					, FText::Format(LOCTEXT("ViewModelTypeAbstract", "Viewmodel class '{0}' is abstract and can't be created. You can change it in the View Models panel."), ViewModelContext.GetViewModelClass()->GetDisplayNameText())
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath)
		{
			if (ViewModelContext.ViewModelPropertyPath.IsEmpty())
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewModelInvalidGetter", "Viewmodel has an invalid Getter. You can select a new one in the View Models panel.")
					, EMessageType::Error
				);
				bIsPreCompileStepValid = true;
				continue;
			}

			TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = AddObjectFieldPath(BindingLibraryCompiler, Class, ViewModelContext.ViewModelPropertyPath, ViewModelContext.GetViewModelClass());
			if (ReadFieldPathResult.HasError())
			{
				AddMessageForViewModel(ViewModelContext
					, ReadFieldPathResult.GetError()
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}

			SourceCreatorContext.ReadPropertyPathHandle = ReadFieldPathResult.StealValue();
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection)
		{
			if (ViewModelContext.GlobalViewModelIdentifier.IsNone())
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewmodelInvalidGlobalIdentifier", "Viewmodel doesn't have a valid Global identifier. You can specify a new one in the Viewmodels panel.")
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Resolver)
		{
			if (!ViewModelContext.Resolver)
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewmodelInvalidResolver", "Viewmodel doesn't have a valid Resolver. You can specify a new one in the Viewmodels panel.")
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}
		}
		else
		{
			AddMessageForViewModel(ViewModelContext
				, LOCTEXT("ViewmodelInvalidCreationType", "Viewmodel doesn't have a valid creation type. You can select one in the Viewmodels panel.")
				, EMessageType::Error
			);
			bIsPreCompileStepValid = false;
			continue;
		}
	}

	for (FCompilerWidgetCreatorContext& WidgetCreator : WidgetCreatorContexts)
	{
		if (!WidgetCreator.Source->AuthoritativeClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
		{
			AddMessage(FText::Format(LOCTEXT("WidgetInvalidInterface", "The widget {0} class doesn't implement the interface NotifyFieldValueChanged."), FText::FromName(WidgetCreator.Source->Name))
				, EMessageType::Error
			);
			bIsPreCompileStepValid = false;
			continue;
		}

		if (!WidgetCreator.bSelfReference)
		{
			TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> ReadFieldPathResult = AddObjectFieldPath(BindingLibraryCompiler, Class, WidgetCreator.Source->Name.ToString(), WidgetCreator.Source->AuthoritativeClass);
			if (ReadFieldPathResult.HasError())
			{
				AddMessage(FText::Format(LOCTEXT("WidgetAddObjectFieldPathFailFormat", "The widget {0} creator failed. {1}")
					, FText::FromName(WidgetCreator.Source->Name)
					, ReadFieldPathResult.GetError()
				)
					, EMessageType::Error
				);
				bIsPreCompileStepValid = false;
				continue;
			}
			WidgetCreator.ReadPropertyPathHandle = ReadFieldPathResult.StealValue();
		}
	}
}


void FMVVMViewBlueprintCompiler::CompileSources(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	struct FSortData
	{
		FSortData() = default;
		FName SourceName;
		FName ParentSourceName;
		int32 SortIndex = -1;

		void CalculateSortIndex(TMap<FName, FSortData>& Map)
		{
			if (SortIndex < 0)
			{
				if (ParentSourceName.IsNone())
				{
					SortIndex = 0;
				}
				else
				{
					Map[ParentSourceName].CalculateSortIndex(Map);
					// calculate the Depth recursively
					SortIndex = Map[ParentSourceName].SortIndex + 1;
				}
			}
		}
	};

	TMap<FName, FSortData> SortDatas;
	TArray<FMVVMViewClass_Source> UnsortedSourceCreators;

	for (FCompilerViewModelCreatorContext& SourceCreatorContext : ViewModelCreatorContexts)
	{
		const FMVVMBlueprintViewModelContext& ViewModelContext = SourceCreatorContext.ViewModelContext;
		FMVVMViewClass_Source CompiledSourceCreator;

		ensure(ViewModelContext.GetViewModelClass() && ViewModelContext.GetViewModelClass()->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()));
		CompiledSourceCreator.ExpectedSourceType = ViewModelContext.GetViewModelClass();
		CompiledSourceCreator.PropertyName = ViewModelContext.GetViewModelName();

		bool bCanBeSet = ViewModelContext.bCreateSetterFunction;
		bool bCanBeEvaluated = SourceCreatorContext.DynamicContext.IsValid();
		bool bIsOptional = SourceCreatorContext.Source->bIsOptional;
		bool bCreateInstance = false;
		bool bIsUserWidgetProperty = !SourceCreatorContext.DynamicContext.IsValid();

		if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Manual)
		{
			bCanBeSet = true;
			ensure(bIsOptional == true);
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::CreateInstance)
		{
			bCreateInstance = true;
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath)
		{
			const FMVVMVCompiledFieldPath* CompiledFieldPath = CompileResult.FieldPaths.Find(SourceCreatorContext.ReadPropertyPathHandle);
			if (CompiledFieldPath == nullptr)
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewModelInvalidInitializationBindingNotGenerated", "The viewmodel initialization binding was not generated.")
					, EMessageType::Error
				);
				bIsCompileStepValid = false;
				continue;
			}

			CompiledSourceCreator.FieldPath = *CompiledFieldPath;
			ensure(bIsOptional == ViewModelContext.bOptional);
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection)
		{
			ensure(!ViewModelContext.GlobalViewModelIdentifier.IsNone());

			FMVVMViewModelContext GlobalViewModelInstance;
			GlobalViewModelInstance.ContextClass = ViewModelContext.GetViewModelClass();
			GlobalViewModelInstance.ContextName = ViewModelContext.GlobalViewModelIdentifier;
			if (!GlobalViewModelInstance.IsValid())
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewmodelGlobalContextIdentifier", "The context for viewmodel could not be created. Change the identifier.")
					, EMessageType::Warning
				);
			}

			CompiledSourceCreator.GlobalViewModelInstance = MoveTemp(GlobalViewModelInstance);
			ensure(bIsOptional == ViewModelContext.bOptional);
		}
		else if (ViewModelContext.CreationType == EMVVMBlueprintViewModelContextCreationType::Resolver)
		{
			UMVVMViewModelContextResolver* Resolver = DuplicateObject(ViewModelContext.Resolver.Get(), ViewExtension);
			if (!Resolver)
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewmodelFailedResolverDuplicate", "Internal error. The resolver could not be dupliated.")
					, EMessageType::Error
				);
				bIsCompileStepValid = false;
				continue;
			}

			CompiledSourceCreator.Resolver = Resolver;
			ensure(bIsOptional == ViewModelContext.bOptional);
		}
		else
		{
			AddMessageForViewModel(ViewModelContext
				, LOCTEXT("ViewModelWithoutValidCreationType", "The viewmodel doesn't have a valid creation type.")
				, EMessageType::Error
			);
			bIsCompileStepValid = false;
			continue;
		}

		FName ParentSourceName;
		if (SourceCreatorContext.DynamicContext)
		{
			if (SourceCreatorContext.DynamicContext->ParentSource)
			{
				ParentSourceName = SourceCreatorContext.DynamicContext->ParentSource->Name;
			}
			else
			{
				AddMessageForViewModel(ViewModelContext
					, LOCTEXT("ViewModelWithoutValidDyanmicParentSource", "The viewmodel doesn't have a valid parent source.")
					, EMessageType::Error
				);
				bIsCompileStepValid = false;
				continue;
			}
		}

		CompiledSourceCreator.Flags = 0;
		CompiledSourceCreator.Flags |= bCreateInstance ? (uint16)FMVVMViewClass_Source::EFlags::TypeCreateInstance : 0;
		CompiledSourceCreator.Flags |= bIsUserWidgetProperty ? (uint16)FMVVMViewClass_Source::EFlags::IsUserWidgetProperty : 0;
		CompiledSourceCreator.Flags |= bIsUserWidgetProperty ? (uint16)FMVVMViewClass_Source::EFlags::SetUserWidgetProperty : 0;
		CompiledSourceCreator.Flags |= bIsOptional ? (uint16)FMVVMViewClass_Source::EFlags::IsOptional : 0;
		CompiledSourceCreator.Flags |= bCanBeSet ? (uint16)FMVVMViewClass_Source::EFlags::CanBeSet : 0;
		CompiledSourceCreator.Flags |= bCanBeEvaluated ? (uint16)FMVVMViewClass_Source::EFlags::CanBeEvaluated : 0;
		CompiledSourceCreator.Flags |= (uint16)FMVVMViewClass_Source::EFlags::IsViewModel;

		FSortData SortData;
		SortData.SourceName = CompiledSourceCreator.GetName();
		SortData.ParentSourceName = ParentSourceName;
		SortDatas.Add(CompiledSourceCreator.GetName(), SortData);
		UnsortedSourceCreators.Add(MoveTemp(CompiledSourceCreator));
	}

	// The other sources needed by the view
	for (FCompilerWidgetCreatorContext& WidgetCreator : WidgetCreatorContexts)
	{
		FMVVMViewClass_Source CompiledSourceCreator;
		ensure(WidgetCreator.Source->AuthoritativeClass&& WidgetCreator.Source->AuthoritativeClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()));
		CompiledSourceCreator.ExpectedSourceType = const_cast<UClass*>(WidgetCreator.Source->AuthoritativeClass);
		CompiledSourceCreator.PropertyName = WidgetCreator.Source->Name;
		CompiledSourceCreator.Flags = 0;
		CompiledSourceCreator.Flags |= WidgetCreator.bSelfReference ? (uint16)FMVVMViewClass_Source::EFlags::SelfReference : 0;

		if (!WidgetCreator.bSelfReference)
		{
			const FMVVMVCompiledFieldPath* CompiledFieldPath = CompileResult.FieldPaths.Find(WidgetCreator.ReadPropertyPathHandle);
			if (CompiledFieldPath == nullptr)
			{
				AddMessage(FText::Format(LOCTEXT("WidgetInvalidInitializationBindingNotGenerated", "The widget {0} initialization binding was not generated."), FText::FromName(WidgetCreator.Source->Name))
					, EMessageType::Error
				);
				bIsCompileStepValid = false;
				continue;
			}
			CompiledSourceCreator.FieldPath = *CompiledFieldPath;
		}

		FSortData SortData;
		SortDatas.Add(CompiledSourceCreator.GetName(), SortData);
		UnsortedSourceCreators.Add(MoveTemp(CompiledSourceCreator));
	}

	// Sanity check
	{
		// Test if all NeededBindingSources in inside the UnsortedSourceCreators
		for (const TSharedRef<FCompilerBindingSource>& BindingSource : NeededBindingSources)
		{
			const FMVVMViewClass_Source* FoundClassSource = UnsortedSourceCreators.FindByPredicate([ToFindName = BindingSource->Name](const FMVVMViewClass_Source& Other){ return Other.GetName() == ToFindName; });
			if (FoundClassSource == nullptr)
			{
				AddMessage(FText::Format(LOCTEXT("CompileSources_MissingSources", "Internal error. The source {0} was not compiled.."), FText::FromName(BindingSource->Name))
					, EMessageType::Warning
				);
			}
		}

		ensure(SortDatas.Num() == UnsortedSourceCreators.Num());
	}

	// sort the source creators by priority and then by name
	if (UnsortedSourceCreators.Num() > 1)
	{
		for (auto& SortDataPair : SortDatas)
		{
			SortDataPair.Value.CalculateSortIndex(SortDatas);
		}

		UnsortedSourceCreators.Sort([&SortDatas](const FMVVMViewClass_Source& A, const FMVVMViewClass_Source& B)
			{
				int32 ASortIndex = SortDatas[A.GetName()].SortIndex;
				int32 BSortIndex = SortDatas[B.GetName()].SortIndex;
				if (ASortIndex == BSortIndex)
				{
					return A.GetName().LexicalLess(B.GetName());
				}
				return ASortIndex < BSortIndex;
			});
	}

	// Add the sorted viewmodel array to the ViewExtension
	ViewExtension->Sources = MoveTemp(UnsortedSourceCreators);

	if (ViewExtension->Sources.Num() > 64)
	{
		// The view use a uint64 bitfield to filter which viewmodel/source is valid/initialized.
		//You can use the MVVM.LogViewCompiledResult command to display the compile result and see the name of the sources.
		AddMessage(LOCTEXT("TooManySources", "There is too many sources for the view. Try spliting your widget into other widgets."), EMessageType::Error);
	}
}


void FMVVMViewBlueprintCompiler::PreCompileBindings(UWidgetBlueprintGeneratedClass* Class)
{
	auto TestExecutionMode = [Self = this](const FMVVMBlueprintViewBinding& Binding) -> bool
	{
		if (Binding.bOverrideExecutionMode)
		{
			if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsExecutionModeAllowed(Binding.OverrideExecutionMode))
			{
				Self->AddMessageForBinding(Binding
					, LOCTEXT("NotAllowedExecutionMode", "The binding has a restricted execution mode.")
					, EMessageType::Error
					, FMVVMBlueprintPinId()
				);
				return false;
			}
		}
		return true;
	};

	auto AddFieldIds = [Self = this, Class](FCompilerBinding& ValidBinding, const FMVVMBlueprintViewBinding& Binding) -> bool
	{
		// Add FieldId
		bool bHasFieldId = false;
		if (!ValidBinding.bIsOneTimeBinding)
		{
			for (const TSharedPtr<FGeneratedReadFieldPathContext>& ReadPath : ValidBinding.ReadPaths)
			{
				if (ReadPath->NotificationField)
				{
					if (ReadPath->NotificationField->LibraryCompilerHandle.IsValid())
					{
						bHasFieldId = true;
					}
					else if (ReadPath->NotificationField->Source) // it is maybe OneTIme
					{
						const UClass* SourceContextClass = ReadPath->NotificationField->Source->AuthoritativeClass;
						TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText> FieldIdResult = Self->BindingLibraryCompiler.AddFieldId(SourceContextClass, ReadPath->NotificationField->NotificationId.GetFieldName());
						if (FieldIdResult.HasError() || !FieldIdResult.GetValue().IsValid())
						{
							Self->AddMessageForBinding(Binding
								, FText::Format(LOCTEXT("CouldNotCreateFieldId", "Could not create Field. {0}"), FieldIdResult.GetError())
								, EMessageType::Error
								, FMVVMBlueprintPinId()
							);
							return false;
						}
						
						ReadPath->NotificationField->LibraryCompilerHandle = FieldIdResult.StealValue();
						bHasFieldId = ReadPath->NotificationField->LibraryCompilerHandle.IsValid();
					}
				}
			}
		}

		// Test correct numbers of FieldIds
		bool bRequiresValidFieldId = !ValidBinding.bIsOneTimeBinding;
		if (bRequiresValidFieldId && !bHasFieldId)
		{
			Self->AddMessageForBinding(Binding
				, LOCTEXT("CouldNotCreateSourceFields", "There is no field to bind to. The binding must be a OneTime binding.")
				, EMessageType::Error
				, FMVVMBlueprintPinId()
			);
			return false;
		}

		return true;
	};

	auto AddReadPaths = [Self = this, Class](FCompilerBinding& ValidBinding, const FMVVMBlueprintViewBinding& Binding) -> bool
	{
		bool bHasValidReadHandle = false;
		if (ValidBinding.Type != FCompilerBinding::EType::ComplexConversionFunction)
		{
			for (const TSharedPtr<FGeneratedReadFieldPathContext>& ReadPath : ValidBinding.ReadPaths)
			{
				if (ReadPath->LibraryCompilerHandle.IsValid())
				{
					bHasValidReadHandle = true;
				}
				else
				{
					TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddFieldPath(ReadPath->SkeletalGeneratedFields, true);
					if (FieldPathResult.HasError() || !FieldPathResult.GetValue().IsValid())
					{
						Self->AddMessageForBinding(Binding
							, FText::Format(Private::CouldNotCreateSourceFieldPathFormat, ::UE::MVVM::FieldPathHelper::ToText(ReadPath->GeneratedFields), FieldPathResult.GetError())
							, EMessageType::Error
							, FMVVMBlueprintPinId()
						);
						return false;
					}

					ReadPath->LibraryCompilerHandle = FieldPathResult.StealValue();
					bHasValidReadHandle = ReadPath->LibraryCompilerHandle.IsValid();
				}
			}
		}

		// Test read
		bool bRequiresReadHandle = ValidBinding.Type != FCompilerBinding::EType::ComplexConversionFunction;
		if (bRequiresReadHandle && !bHasValidReadHandle)
		{
			Self->AddMessageForBinding(Binding
				, LOCTEXT("CouldNotCreateSourceReadIsPresent", "Internal error. There should be no property to read from.")
				, EMessageType::Error
				, FMVVMBlueprintPinId()
			);
			return false;
		}

		return true;
	};

	auto AddWritePaths = [Self = this, Class](FCompilerBinding& ValidBinding, const FMVVMBlueprintViewBinding& Binding) -> bool
	{
		if (ValidBinding.WritePath->LibraryCompilerHandle.IsValid())
		{
			return true;
		}

		if (!ValidBinding.WritePath)
		{
			Self->AddMessageForBinding(Binding
				, LOCTEXT("InvalidWritePath", "Internal Error. The write path is invalid.")
				, EMessageType::Error
				, FMVVMBlueprintPinId()
			);
			return false;
		}

		TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddFieldPath(ValidBinding.WritePath->SkeletalGeneratedFields, false);
		if (FieldPathResult.HasError())
		{
			Self->AddMessageForBinding(Binding
				, FText::Format(Private::CouldNotCreateDestinationFieldPathFormat, ::UE::MVVM::FieldPathHelper::ToText(ValidBinding.WritePath->GeneratedFields), FieldPathResult.GetError())
				, EMessageType::Error
				, FMVVMBlueprintPinId()
			);
			return false;
		}
		ValidBinding.WritePath->LibraryCompilerHandle = FieldPathResult.StealValue();

		// test write

		return true;
	};

	auto AddConversionFunction = [Self = this, Class](FCompilerBinding& ValidBinding, const FMVVMBlueprintViewBinding& Binding) -> bool
	{
		const UFunction* ConversionFunction = nullptr;
		{
			UMVVMBlueprintViewConversionFunction* ViewConversionFunction = ValidBinding.ConversionFunction.Get();
			if (ViewConversionFunction)
			{
				ConversionFunction = ViewConversionFunction->GetCompiledFunction(Class);
				if (ConversionFunction == nullptr)
				{
					Self->AddMessageForBinding(Binding
						, FText::Format(LOCTEXT("ConversionFunctionNotFound", "The conversion function '{0}' could not be found."), FText::FromName(ViewConversionFunction->GetCompiledFunctionName(Class)))
						, EMessageType::Error
						, FMVVMBlueprintPinId()
					);
					return false;
				}

				TVariant<const UFunction*, TSubclassOf<UK2Node>> FunctionOrWrapperFunction = ViewConversionFunction->GetConversionFunction(Self->WidgetBlueprintCompilerContext.WidgetBlueprint());
				if (FunctionOrWrapperFunction.IsType<const UFunction*>())
				{
					if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsConversionFunctionAllowed(Self->WidgetBlueprintCompilerContext.WidgetBlueprint(), FunctionOrWrapperFunction.Get<const UFunction*>()))
					{
						Self->AddMessageForBinding(Binding
							, FText::Format(LOCTEXT("ConversionFunctionNotAllow", "The conversion function {0} is not allowed."), FText::FromName(FunctionOrWrapperFunction.Get<const UFunction*>()->GetFName()))
							, EMessageType::Error
							, FMVVMBlueprintPinId()
						);
						return false;
					}
				}
				else
				{
					Self->AddMessageForBinding(Binding
						, LOCTEXT("ConversionFunctionNodeNotAllow", "The conversion function node is not allowed.")
						, EMessageType::Error
						, FMVVMBlueprintPinId()
					);
					return false;
				}
			}
		}

		if (ConversionFunction != nullptr)
		{
			TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> FieldPathResult = Self->BindingLibraryCompiler.AddConversionFunctionFieldPath(Class, ConversionFunction);
			if (FieldPathResult.HasError())
			{
				Self->AddMessageForBinding(Binding
					, FText::Format(LOCTEXT("CouldNotCreateConversionFunctionFieldPath", "Couldn't create the conversion function field path '{0}'. {1}")
						, FText::FromString(ConversionFunction->GetPathName())
						, FieldPathResult.GetError())
					, EMessageType::Error
					, FMVVMBlueprintPinId()
				);
				return false;
			}

			ValidBinding.ConversionFunctionHandle = FieldPathResult.StealValue();
		}

		// Sanity check
		{
			const bool bShouldHaveConversionFunction = ValidBinding.Type == FCompilerBinding::EType::ComplexConversionFunction || ValidBinding.Type == FCompilerBinding::EType::SimpleConversionFunction;
			if ((ConversionFunction != nullptr) != bShouldHaveConversionFunction)
			{
				Self->AddMessageForBinding(Binding, LOCTEXT("ConversionFunctionShouldExist", "Internal error. The conversion function should exist."), EMessageType::Error, FMVVMBlueprintPinId());
				return false;
			}

			const bool bShouldHaveComplexConversionFunction = ValidBinding.Type == FCompilerBinding::EType::ComplexConversionFunction;
			if (bShouldHaveComplexConversionFunction != BindingHelper::IsValidForComplexRuntimeConversion(ConversionFunction))
			{
				Self->AddMessageForBinding(Binding, LOCTEXT("ConversionFunctionIsNotComplex", "Internal Error. The complex conversion function does not respect the prerequisite."), EMessageType::Error, FMVVMBlueprintPinId());
				return false;
			}
		}

		return true;
	};

	for (TSharedRef<FCompilerBinding>& ValidBinding : ValidBindings)
	{
		const FMVVMBlueprintViewBinding& Binding = *(BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex));
		if (ValidBinding->Type != FCompilerBinding::EType::Assignment
			&& ValidBinding->Type != FCompilerBinding::EType::ComplexConversionFunction
			&& ValidBinding->Type != FCompilerBinding::EType::SimpleConversionFunction)
		{
			AddMessageForBinding(Binding, LOCTEXT("UnsupportedBindingType", "The binding is invalid."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsPreCompileStepValid = false;
			continue;
		}

		if (!TestExecutionMode(Binding))
		{
			bIsPreCompileStepValid = false;
			continue;
		}

		if (!AddFieldIds(ValidBinding.Get(), Binding)
			|| !AddReadPaths(ValidBinding.Get(), Binding)
			|| !AddWritePaths(ValidBinding.Get(), Binding)
			|| !AddConversionFunction(ValidBinding.Get(), Binding))
		{
			bIsPreCompileStepValid = false;
			continue;
		}

		// Generate the binding
		TValueOrError<FCompiledBindingLibraryCompiler::FBindingHandle, FText> BindingResult = ValidBinding->Type == FCompilerBinding::EType::ComplexConversionFunction
			? BindingLibraryCompiler.AddComplexBinding(ValidBinding->WritePath->LibraryCompilerHandle, ValidBinding->ConversionFunctionHandle)
			: BindingLibraryCompiler.AddBinding(ValidBinding->ReadPaths[0]->LibraryCompilerHandle, ValidBinding->WritePath->LibraryCompilerHandle, ValidBinding->ConversionFunctionHandle);

		if (BindingResult.HasError())
		{
			AddMessageForBinding(Binding
				, FText::Format(LOCTEXT("CouldNotCreateBinding", "Could not create binding. {0}"), BindingResult.StealError())
				, EMessageType::Error
				, FMVVMBlueprintPinId()
			);
			bIsPreCompileStepValid = false;
			continue;
		}
		ValidBinding->BindingHandle = BindingResult.StealValue();
	}

	// Add bindings for the dynamic viewmodels
	for (TSharedRef<FCompilerSourceViewModelDynamicCreatorContext>& ViewModelDynamic : SourceViewModelDynamicCreatorContexts)
	{
		if (ensure(ViewModelDynamic->Source.IsValid() && ViewModelDynamic->ParentSource.IsValid()))
		{
			if (!ViewModelDynamic->NotificationId.IsValid())
			{
				AddMessage(FText::Format(LOCTEXT("InvalidNotificationFieldId", "{0} doesn't have a field Id."), FText::FromName(ViewModelDynamic->Source->Name)), EMessageType::Error);
				bIsPreCompileStepValid = false;
				continue;
			}

			const UClass* SourceContextClass = ViewModelDynamic->ParentSource->AuthoritativeClass;
			TValueOrError<FCompiledBindingLibraryCompiler::FFieldIdHandle, FText> FieldIdResult = BindingLibraryCompiler.AddFieldId(SourceContextClass, ViewModelDynamic->NotificationId.GetFieldName());
			if (FieldIdResult.HasError() || !FieldIdResult.GetValue().IsValid())
			{
				AddMessage(FText::Format(LOCTEXT("CouldNotCreateFieldIdForDynamic", "Could not create Field for {0}. {1}"), FText::FromName(ViewModelDynamic->Source->Name), FieldIdResult.GetError()), EMessageType::Error);
				bIsPreCompileStepValid = false;
				continue;
			}

			ViewModelDynamic->NotificationIdLibraryCompilerHandle = FieldIdResult.GetValue();
		}
	}
}


void FMVVMViewBlueprintCompiler::CompileBindings(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	static IConsoleVariable* CVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode"));
	ensure(CVarDefaultExecutionMode);
	if (!CVarDefaultExecutionMode)
	{
		WidgetBlueprintCompilerContext.MessageLog.Error(*LOCTEXT("CantFindDefaultExecutioMode", "The default execution mode cannot be found.").ToString());
		return;
	}

	// Sort the array to have a predictable list
	{
		UMVVMBlueprintView* LocalBlueprintView = BlueprintView.Get();
		ValidBindings.StableSort([LocalBlueprintView](const TSharedRef<FCompilerBinding>& A, const TSharedRef<FCompilerBinding>& B)
			{
				const FMVVMBlueprintViewBinding& BindingA = *(LocalBlueprintView->GetBindingAt(A->Key.ViewBindingIndex));
				const FMVVMBlueprintViewBinding& BindingB = *(LocalBlueprintView->GetBindingAt(B->Key.ViewBindingIndex));
				return BindingA.BindingId < BindingB.BindingId;
			});
	}

	ensure(ViewExtension->Bindings.Num() == 0);
	ViewExtension->Bindings.Reset(ValidBindings.Num());
	for (int32 ValidBindingIndex = 0; ValidBindingIndex < ValidBindings.Num(); ++ValidBindingIndex)
	{
		const TSharedRef<FCompilerBinding>& ValidBinding = ValidBindings[ValidBindingIndex];
		const FMVVMBlueprintViewBinding& Binding = *(BlueprintView->GetBindingAt(ValidBinding->Key.ViewBindingIndex));

		const FMVVMVCompiledBinding* CompiledBinding = CompileResult.Bindings.Find(ValidBinding->BindingHandle);
		if (CompiledBinding == nullptr)
		{
			AddMessageForBinding(Binding, LOCTEXT("CompiledBindingNotGenerated", "Could not generate compiled binding."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsCompileStepValid = false;
			continue;
		}

		FMVVMViewClass_BindingKey BindingKey = FMVVMViewClass_BindingKey(ViewExtension->Bindings.AddDefaulted());
		FMVVMViewClass_Binding& NewBinding = ViewExtension->Bindings[BindingKey.GetIndex()];
		NewBinding.Binding = CompiledBinding ? *CompiledBinding : FMVVMVCompiledBinding();
		NewBinding.ExecutionMode = Binding.bOverrideExecutionMode ? Binding.OverrideExecutionMode : (EMVVMExecutionMode)CVarDefaultExecutionMode->GetInt();
		NewBinding.SourceBitField = 0;
		NewBinding.EditorId = Binding.BindingId;

		NewBinding.Flags = 0;
		NewBinding.Flags |= (!ValidBinding->bIsOneTimeBinding) ? (uint8)FMVVMViewClass_Binding::EFlags::OneWay : 0;
		NewBinding.Flags |= (Binding.bOverrideExecutionMode) ? (uint8)FMVVMViewClass_Binding::EFlags::OverrideExecuteMode : 0;
		NewBinding.Flags |= (ValidBinding->ReadPaths.Num() > 1) ? (uint8)FMVVMViewClass_Binding::EFlags::Shared : 0;
		NewBinding.Flags |= (Binding.bEnabled) ? (uint8)FMVVMViewClass_Binding::EFlags::EnabledByDefault : 0;

		TArray<FMVVMViewClass_SourceKey, TInlineAllocator<16>>  SharedBindings;
		for (TSharedPtr<FGeneratedReadFieldPathContext> ReadPath : ValidBinding->ReadPaths)
		{
			if (ReadPath->OptionalSource == nullptr)
			{
				AddMessageForBinding(Binding, LOCTEXT("InvalidSourceInternal", "Internal error. The binding has an invalid source."), EMessageType::Error, FMVVMBlueprintPinId());
				bIsCompileStepValid = false;
				continue;
			}

			int32 ViewExtensionSourceCreatorsIndex = ViewExtension->Sources.IndexOfByPredicate([LookFor = ReadPath->OptionalSource->Name](const FMVVMViewClass_Source& Other)
				{
					return Other.GetName() == LookFor;
				});
			if (!ViewExtension->Sources.IsValidIndex(ViewExtensionSourceCreatorsIndex))
			{
				AddMessageForBinding(Binding, LOCTEXT("CompiledSourceCreatorNotGenerated", "Internal error. The source creator was not generated."), EMessageType::Error, FMVVMBlueprintPinId());
				bIsCompileStepValid = false;
				continue;
			}

			// Add the needed source.
			FMVVMViewClass_SourceKey FieldClassSourceKey = FMVVMViewClass_SourceKey(ViewExtensionSourceCreatorsIndex);
			NewBinding.SourceBitField |= FieldClassSourceKey.GetBit();

			// FieldId
			const bool bHasField = (ReadPath->NotificationField && !ValidBinding->bIsOneTimeBinding);
			const UE::FieldNotification::FFieldId* CompiledFieldId = bHasField ? CompileResult.FieldIds.Find(ReadPath->NotificationField->LibraryCompilerHandle) : nullptr;
			if (CompiledFieldId == nullptr && bHasField)
			{
				AddMessageForBinding(Binding, LOCTEXT("CompiledFieldNotGenerated", "Internal error. The FieldId was not generated."), EMessageType::Error, FMVVMBlueprintPinId());
				bIsCompileStepValid = false;
				continue;
			}

			bool bExecuteAtInitialization = ValidBinding->Key.bIsForwardBinding;

			FMVVMViewClass_Source& ClassSource = ViewExtension->Sources[ViewExtensionSourceCreatorsIndex];
			FMVVMViewClass_SourceBinding& NewSourceBinding = ClassSource.Bindings.AddDefaulted_GetRef();
			NewSourceBinding.BindingKey = BindingKey;

			NewSourceBinding.Flags = 0;
			NewSourceBinding.Flags |= (bExecuteAtInitialization) ? (uint8)FMVVMViewClass_SourceBinding::EFlags::ExecuteAtInitialization : 0;

			if (CompiledFieldId)
			{
				NewSourceBinding.FieldId = FFieldNotificationId(CompiledFieldId->GetName());
				ClassSource.FieldToRegisterTo.AddUnique(FMVVMViewClass_FieldId(*CompiledFieldId));

				// Count the number of shared instance of that binding. (they can come from the same Source)
				//This flag is used at runtime to know if we should delay the binding execution.
				//We only delay when the field changes. It should only be shared if it has field.
				SharedBindings.Add(FieldClassSourceKey);
			}
		}

		NewBinding.Flags |= (SharedBindings.Num() > 0) ? (uint8)FMVVMViewClass_Binding::EFlags::Shared : 0;

		// Only the last binding in the complex conversion.
		if (SharedBindings.Num() > 0)
		{
			SharedBindings.Sort([](const FMVVMViewClass_SourceKey& A, const FMVVMViewClass_SourceKey&B)
				{
					return A.GetIndex() < B.GetIndex();
				});

			//Only the last one has ExecuteAtInitialization. Remove the flag on all the others.
			for (int32 Index = 0; Index < SharedBindings.Num() - 2; ++Index)
			{
				FMVVMViewClass_Source& ClassSource = ViewExtension->Sources[SharedBindings[Index].GetIndex()];
				for (FMVVMViewClass_SourceBinding& SourceBinding : ClassSource.Bindings)
				{
					if (SourceBinding.GetBindingKey() == BindingKey)
					{
						SourceBinding.Flags &= ~(uint8)FMVVMViewClass_SourceBinding::EFlags::ExecuteAtInitialization;
					}
				}
			}
			// keep only one ExecuteAtInitialization on the last source
			{
				FMVVMViewClass_Source& ClassSource = ViewExtension->Sources[SharedBindings.Last().GetIndex()];
				bool bFound = false;
				for (int32 Index = ClassSource.Bindings.Num() - 1; Index >= 0; --Index)
				{
					FMVVMViewClass_SourceBinding& SourceBinding = ClassSource.Bindings[Index];
					if (SourceBinding.GetBindingKey() == BindingKey)
					{
						if (bFound)
						{
							SourceBinding.Flags &= ~(uint8)FMVVMViewClass_SourceBinding::EFlags::ExecuteAtInitialization;
						}
						bFound = true;
					}
				}
			}
		}
	}
}


void FMVVMViewBlueprintCompiler::CompileEvaluateSources(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	for (const TSharedRef<FCompilerSourceViewModelDynamicCreatorContext>& ViewModelDynamic : SourceViewModelDynamicCreatorContexts)
	{
		int32 ViewExtensionSourceCreatorsIndex = ViewExtension->Sources.IndexOfByPredicate([LookFor = ViewModelDynamic->Source->Name](const FMVVMViewClass_Source& Other)
			{
				return Other.GetName() == LookFor;
			});
		if (!ViewExtension->Sources.IsValidIndex(ViewExtensionSourceCreatorsIndex))
		{
			AddMessage(LOCTEXT("CompiledSourceCreatorNotGenerated", "Internal error. The source creator was not generated."), EMessageType::Error);
			bIsCompileStepValid = false;
			continue;
		}
		int32 ViewExtensionParentCreatorsIndex = ViewExtension->Sources.IndexOfByPredicate([LookFor = ViewModelDynamic->ParentSource->Name](const FMVVMViewClass_Source& Other)
			{
				return Other.GetName() == LookFor;
			});
		if (!ViewExtension->Sources.IsValidIndex(ViewExtensionParentCreatorsIndex))
		{
			AddMessage(LOCTEXT("CompiledParentSourceCreatorNotGenerated", "Internal error. The parent source creator was not generated."), EMessageType::Error);
			bIsCompileStepValid = false;
			continue;
		}

		const UE::FieldNotification::FFieldId* CompiledFieldId = CompileResult.FieldIds.Find(ViewModelDynamic->NotificationIdLibraryCompilerHandle);
		if (CompiledFieldId == nullptr)
		{
			AddMessage(LOCTEXT("CompiledFieldNotifyNotGenerated", "Internal error. The field notify was not generated."), EMessageType::Error);
			bIsCompileStepValid = false;
			continue;
		}

		FMVVMViewClass_EvaluateSource& NewBinding = ViewExtension->EvaluateSources.AddDefaulted_GetRef();
		NewBinding.ParentFieldId = FFieldNotificationId(CompiledFieldId->GetName());
		NewBinding.ParentSource = FMVVMViewClass_SourceKey(ViewExtensionParentCreatorsIndex);
		NewBinding.ToEvaluate = FMVVMViewClass_SourceKey(ViewExtensionSourceCreatorsIndex);

		FMVVMViewClass_Source& ClassSource = ViewExtension->Sources[ViewExtensionParentCreatorsIndex];
		ClassSource.FieldToRegisterTo.AddUnique(FMVVMViewClass_FieldId(*CompiledFieldId));

		ClassSource.Flags |= (uint16)FMVVMViewClass_Source::EFlags::HasEvaluatedBindings;
	}

	ViewExtension->EvaluateSources.StableSort([](const FMVVMViewClass_EvaluateSource& A, const FMVVMViewClass_EvaluateSource& B)
		{
			if (A.GetParentSource() == B.GetParentSource())
			{
				return A.GetFieldId().GetFieldName().Compare(B.GetFieldId().GetFieldName()) < 0;
			}
			return A.GetParentSource().GetIndex() < B.GetParentSource().GetIndex();
		});
}


void FMVVMViewBlueprintCompiler::PreCompileEvents(UWidgetBlueprintGeneratedClass* Class)
{
	if (!GetDefault<UMVVMDeveloperProjectSettings>()->bAllowBindingEvent && BlueprintView->GetEvents().Num() > 0)
	{
		WidgetBlueprintCompilerContext.MessageLog.Warning(*LOCTEXT("EventsAreNotAllowed", "Binding events are not allowed in your project settings.").ToString());
	}

	for (TSharedRef<FCompilerEvent>& ValidEvent : ValidEvents)
	{
		UMVVMBlueprintViewEvent* EventPtr = ValidEvent->Event.Get();
		check(EventPtr);
		UEdGraph* GeneratedGraph = EventPtr->GetOrCreateWrapperGraph();
		check(GeneratedGraph);

		// Does it resolve and are the field allowed
		TValueOrError<FCreateFieldsResult, FText> EventPathResult = CreateFieldContext(Class, EventPtr->GetEventPath(), true);
		if (EventPathResult.HasError())
		{
			AddMessageForEvent(EventPtr
				, FText::Format(Private::PropertyPathIsInvalidFormat, PropertyPathToText(Class, BlueprintView.Get(), EventPtr->GetEventPath()))
				, EMessageType::Error
				, FMVVMBlueprintPinId());
			bIsPreCompileStepValid = false;
			continue;
		}

		//Does EventPath resolves to a MulticastDelegateProperty
		const FMulticastDelegateProperty* DelegateProperty = EventPathResult.GetValue().SkeletalGeneratedFields.Last().IsProperty() ? CastField<const FMulticastDelegateProperty>(EventPathResult.GetValue().SkeletalGeneratedFields.Last().GetProperty()) : nullptr;
		if (DelegateProperty == nullptr)
		{
			AddMessageForEvent(EventPtr
				, FText::Format(LOCTEXT("EventPathIsNotMulticastDelegate", "The event {0} is not a multicast delegate."), PropertyPathToText(Class, BlueprintView.Get(), EventPtr->GetEventPath()))
				, EMessageType::Error
				, FMVVMBlueprintPinId());
			bIsPreCompileStepValid = false;
			continue;
		}

		// Does it still have the same signature
		if (!UE::MVVM::FunctionGraphHelper::IsFunctionEntryMatchSignature(GeneratedGraph, DelegateProperty->SignatureFunction))
		{
			AddMessageForEvent(EventPtr
				, FText::Format(LOCTEXT("EventPathFunctionSignatureError", "The event {0} doesn't match the function signature."), DelegateProperty->GetDisplayNameText())
				, EMessageType::Error
				, FMVVMBlueprintPinId());
			bIsPreCompileStepValid = false;
			continue;
		}

		// Generate the FieldPath to get the delegate property at runtime
		TValueOrError<FCompiledBindingLibraryCompiler::FFieldPathHandle, FText> EventFieldPathResult = BindingLibraryCompiler.AddFieldPath(EventPathResult.GetValue().SkeletalGeneratedFields, true);
		if (EventFieldPathResult.HasError())
		{
			AddMessageForEvent(EventPtr
				, FText::Format(Private::CouldNotCreateSourceFieldPathFormat, PropertyPathToText(Class, BlueprintView.Get(), EventPtr->GetEventPath()), EventFieldPathResult.GetError())
				, EMessageType::Error
				, FMVVMBlueprintPinId());
			bIsPreCompileStepValid = false;
			continue;
		}

		//todo ? this doesn't support long path?

		FName SourceName;
		switch (EventPtr->GetEventPath().GetSource(WidgetBlueprintCompilerContext.WidgetBlueprint()))
		{
		case EMVVMBlueprintFieldPathSource::SelfContext:
			SourceName = WidgetBlueprintCompilerContext.WidgetBlueprint()->GetFName();
			break;
		case EMVVMBlueprintFieldPathSource::Widget:
		{
			FName WidgetName = EventPtr->GetEventPath().GetWidgetName();
			checkf(!WidgetName.IsNone(), TEXT("The destination should have been checked and set bAreSourceContextsValid."));
			const bool bSourceIsUserWidget = WidgetName == Class->ClassGeneratedBy->GetFName();
			ensure(!bSourceIsUserWidget);
			
			SourceName = WidgetName;
			break;
		}
		case EMVVMBlueprintFieldPathSource::ViewModel:
		{
			const FMVVMBlueprintViewModelContext* SourceViewModelContext = BlueprintView->FindViewModel(EventPtr->GetEventPath().GetViewModelId());
			check(SourceViewModelContext);
			FName ViewModelName = SourceViewModelContext->GetViewModelName();
			SourceName = ViewModelName;
			break;
		}
		default:
			ensureAlwaysMsgf(false, TEXT("An EMVVMBlueprintFieldPathSource case was not checked."));
		}

		// No need to add the generated function to the field compiler.
		//They are in the BP generated code.

		ValidEvent->DelegateFieldPathHandle = EventFieldPathResult.StealValue();
		ValidEvent->GeneratedGraphName = EventPtr->GetWrapperGraphName();
		ValidEvent->SourceName = SourceName;
	}
}


void FMVVMViewBlueprintCompiler::CompileEvents(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	for (const TSharedRef<FCompilerEvent>& ValidEvent : ValidEvents)
	{
		const FMVVMVCompiledFieldPath* CompiledFieldPath = CompileResult.FieldPaths.Find(ValidEvent->DelegateFieldPathHandle);
		if (CompiledFieldPath == nullptr)
		{
			AddMessageForEvent(ValidEvent->Event.Get(), LOCTEXT("CompiledEventFieldPathNotGenerated", "Could not generate the event path."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsCompileStepValid = false;
			continue;
		}

		if (ValidEvent->GeneratedGraphName.IsNone() || Class->FindFunctionByName(ValidEvent->GeneratedGraphName) == nullptr)
		{
			AddMessageForEvent(ValidEvent->Event.Get(), LOCTEXT("CompiledEventFieldPathNotGenerated", "Could not generate the event path."), EMessageType::Error, FMVVMBlueprintPinId());
			bIsCompileStepValid = false;
			continue;
		}

		int32 FoundSourceIndex = INDEX_NONE;
		if (!ValidEvent->SourceName.IsNone())
		{
			FoundSourceIndex = ViewExtension->Sources.IndexOfByPredicate([ToFind = ValidEvent->SourceName](const FMVVMViewClass_Source& Other)
				{
					return Other.GetName() == ToFind;
				});
		}

		FMVVMViewClass_Event& NewBinding = ViewExtension->Events.AddDefaulted_GetRef();
		NewBinding.FieldPath = *CompiledFieldPath;
		NewBinding.UserWidgetFunctionName = ValidEvent->GeneratedGraphName;
		NewBinding.SourceToReevaluate = FoundSourceIndex != INDEX_NONE ? FMVVMViewClass_SourceKey(FoundSourceIndex) : FMVVMViewClass_SourceKey();
	}
}


void FMVVMViewBlueprintCompiler::SortSourceFields(const FCompiledBindingLibraryCompiler::FCompileResult& CompileResult, UWidgetBlueprintGeneratedClass* Class, UMVVMViewClass* ViewExtension)
{
	for (FMVVMViewClass_Source& Source : ViewExtension->Sources)
	{
		for (const FMVVMViewClass_FieldId& Field : Source.FieldToRegisterTo)
		{
			if (Field.GetFieldId().GetIndex() == INDEX_NONE)
			{
				AddMessage(LOCTEXT("SortSourceFieldsInvalidId", "Internal error. The id is invalid."), EMessageType::Error);
				bIsCompileStepValid = false;
				continue;
			}
		}

		Source.FieldToRegisterTo.StableSort([](const FMVVMViewClass_FieldId& A, const FMVVMViewClass_FieldId& B)
			{
				return A.GetFieldId().GetIndex() < B.GetFieldId().GetIndex();
			});
	}
}


TValueOrError<FMVVMViewBlueprintCompiler::FGetFieldsResult, FText> FMVVMViewBlueprintCompiler::GetFields(const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath) const
{
	FGetFieldsResult Result;
	if (!PropertyPath.IsValid())
	{
		ensureAlwaysMsgf(false, TEXT("Empty property path found. It should have been catch before."));
		return MakeError(FText::GetEmpty());
	}

	auto FindSource = [Self = this](const FName PropertyName, FCompilerBindingSource::EType ExpectedType) -> TValueOrError<TSharedPtr<FCompilerBindingSource>, FText>
	{
		const TSharedRef<FCompilerBindingSource>* Found = Self->NeededBindingSources.FindByPredicate([PropertyName](const TSharedRef<FCompilerBindingSource>& Other) { return Other->Name == PropertyName; });
		if (Found)
		{
			if ((*Found)->Type != ExpectedType)
			{
				return MakeError(LOCTEXT("NotExpectedSourceType", "Internal error. The source of the path is not of the expected type."));
			}
			return MakeValue(*Found);
		}
		return MakeValue(TSharedPtr<FCompilerBindingSource>());
	};

	switch (PropertyPath.GetSource(WidgetBlueprintCompilerContext.WidgetBlueprint()))
	{
	case EMVVMBlueprintFieldPathSource::ViewModel:
	{
		const FCompilerViewModelCreatorContext* FoundViewModelCreator = ViewModelCreatorContexts.FindByPredicate([ViewModelId = PropertyPath.GetViewModelId()](const FCompilerViewModelCreatorContext& Other){ return Other.ViewModelContext.GetViewModelId() == ViewModelId; });
		check(FoundViewModelCreator);
		const FName PropertyName = FoundViewModelCreator->ViewModelContext.GetViewModelName();
		const FCompilerUserWidgetProperty* FoundUserWidgetProperty = NeededUserWidgetProperties.FindByPredicate([PropertyName](const FCompilerUserWidgetProperty& Other){ return Other.Name == PropertyName; });
		check(FoundUserWidgetProperty);
		check(FoundViewModelCreator->ViewModelContext.GetViewModelClass());
		check(FoundUserWidgetProperty->AuthoritativeClass == FoundViewModelCreator->ViewModelContext.GetViewModelClass());

		TValueOrError<TSharedPtr<FCompilerBindingSource>, FText> FindSourceResult = FindSource(PropertyName, FCompilerBindingSource::EType::ViewModel);
		if (FindSourceResult.HasError())
		{
			return MakeError(FindSourceResult.StealError());
		}
		if (!FindSourceResult.GetValue().IsValid())
		{
			return MakeError(LOCTEXT("ViewModelShouldHaveSource", "Internal error. Viewmodel should have a source."));
		}

		Result.GeneratedFields = GetFields(Class, PropertyName, PropertyPath.GetFields(Class));
		Result.Source = FindSourceResult.StealValue();
		break;
	}
	case EMVVMBlueprintFieldPathSource::SelfContext:
	{
		TValueOrError<TSharedPtr<FCompilerBindingSource>, FText> FindSourceResult = FindSource(WidgetBlueprintCompilerContext.WidgetBlueprint()->GetFName(), FCompilerBindingSource::EType::Self);
		if (FindSourceResult.HasError())
		{
			return MakeError(FindSourceResult.StealError());
		}
		if (!FindSourceResult.GetValue().IsValid())
		{
			return MakeError(LOCTEXT("WidgetBlueprintShouldHaveSource", "Internal error. The blueprint should have a source."));
		}

		Result.GeneratedFields = GetFields(Class, FName(), PropertyPath.GetFields(Class));
		Result.Source = FindSourceResult.StealValue();
		break;
	}
	case EMVVMBlueprintFieldPathSource::Widget:
	{
		FName DestinationWidgetName = PropertyPath.GetWidgetName();
		check(WidgetNameToWidgetPointerMap.Contains(DestinationWidgetName));
		checkf(!DestinationWidgetName.IsNone(), TEXT("The destination should have been checked and set bAreSourceContextsValid."));

		const FCompilerUserWidgetProperty* FoundUserWidgetProperty = NeededUserWidgetProperties.FindByPredicate([DestinationWidgetName](const FCompilerUserWidgetProperty& Other) { return Other.Name == DestinationWidgetName; });
		check(FoundUserWidgetProperty);

		TValueOrError<TSharedPtr<FCompilerBindingSource>, FText> FindSourceResult = FindSource(DestinationWidgetName, FCompilerBindingSource::EType::Widget);
		if (FindSourceResult.HasError())
		{
			return MakeError(FindSourceResult.StealError());
		}

		Result.GeneratedFields = GetFields(Class, DestinationWidgetName, PropertyPath.GetFields(Class));
		Result.Source = FindSourceResult.StealValue();
		break;
	}
	default:
		ensureAlwaysMsgf(false, TEXT("Not supported yet."));
		Result.GeneratedFields = GetFields(Class, FName(), PropertyPath.GetFields(Class));
		break;
	}
	return MakeValue(MoveTemp(Result));
}


TArray<FMVVMConstFieldVariant> FMVVMViewBlueprintCompiler::GetFields(const UClass* Class, FName PropertyName, TArray<FMVVMConstFieldVariant> Properties)
{
	if (PropertyName.IsNone())
	{
		return Properties;
	}

	check(Class);
	FMVVMConstFieldVariant NewProperty = BindingHelper::FindFieldByName(Class, FMVVMBindingName(PropertyName));
	Properties.Insert(NewProperty, 0);
	return Properties;
}


TValueOrError<FMVVMViewBlueprintCompiler::FCreateFieldsResult, FText> FMVVMViewBlueprintCompiler::CreateFieldContext(const UWidgetBlueprintGeneratedClass* Class, const FMVVMBlueprintPropertyPath& PropertyPath, bool bForSourceReading) const
{
	FMVVMViewBlueprintCompiler::FCreateFieldsResult Result;

	// Evaluate the getter/setter path.
	TValueOrError<FGetFieldsResult, FText> GetFieldResult = GetFields(Class, PropertyPath);
	if (GetFieldResult.HasError())
	{
		return MakeError(GetFieldResult.StealError());
	}

	Result.Source = MoveTemp(GetFieldResult.GetValue().Source);
	Result.GeneratedFields = MoveTemp(GetFieldResult.GetValue().GeneratedFields);

	if (!IsPropertyPathValid(WidgetBlueprintCompilerContext.WidgetBlueprint(), Result.GeneratedFields))
	{
		return MakeError(FText::Format(Private::PropertyPathIsInvalidFormat, PropertyPathToText(Class, BlueprintView.Get(), PropertyPath)));
	}

	// Generate the path with property converted to BP function
	TValueOrError<TArray<FMVVMConstFieldVariant>, FText> SkeletalGeneratedFieldsResult = FieldPathHelper::GenerateFieldPathList(Result.GeneratedFields, bForSourceReading);
	if (SkeletalGeneratedFieldsResult.HasError() || !IsPropertyPathValid(WidgetBlueprintCompilerContext.WidgetBlueprint(), SkeletalGeneratedFieldsResult.GetValue()))
	{
		return MakeError(FText::Format(Private::CouldNotCreateSourceFieldPathFormat, PropertyPathToText(Class, BlueprintView.Get(), PropertyPath), SkeletalGeneratedFieldsResult.GetError()));
	}
	Result.SkeletalGeneratedFields = SkeletalGeneratedFieldsResult.StealValue();

	return MakeValue(Result);
}


TValueOrError<TSharedPtr<FMVVMViewBlueprintCompiler::FCompilerNotifyFieldId>, FText> FMVVMViewBlueprintCompiler::CreateNotifyFieldId(const UWidgetBlueprintGeneratedClass* Class, const TSharedPtr<FGeneratedReadFieldPathContext>& ReadFieldContext, const FMVVMBlueprintViewBinding& Binding)
{
	check(ReadFieldContext->SkeletalGeneratedFields.Num() > 0);

	// The path may contains another INotifyFieldValueChanged
	TValueOrError<FieldPathHelper::FParsedNotifyBindingInfo, FText> BindingInfoResult = FieldPathHelper::GetNotifyBindingInfoFromFieldPath(Class, ReadFieldContext->SkeletalGeneratedFields);
	if (BindingInfoResult.HasError())
	{
		return MakeError(BindingInfoResult.StealError());
	}

	const FieldPathHelper::FParsedNotifyBindingInfo& BindingInfo = BindingInfoResult.GetValue();
	if (!BindingInfo.NotifyFieldId.IsValid() || ReadFieldContext->OptionalSource == nullptr)
	{
		return MakeValue(TSharedPtr<FCompilerNotifyFieldId>());
	}


	FCompilerNotifyFieldId Result;
	Result.NotificationId = BindingInfo.NotifyFieldId;
	Result.Source = ReadFieldContext->OptionalSource;
	Result.ViewModelDynamic.Reset();

	auto GetClassFromField = [](UE::MVVM::FMVVMConstFieldVariant Field) -> const UClass*
	{
		if (Field.IsProperty())
		{
			const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Field.GetProperty());
			if (ObjectProperty)
			{
				return ObjectProperty->PropertyClass;
			}
		}
		else if (Field.IsFunction())
		{
			const FObjectPropertyBase* ReturnValue = CastField<const FObjectPropertyBase>(BindingHelper::GetReturnProperty(Field.GetFunction()));
			if (ReturnValue)
			{
				return ReturnValue->PropertyClass;
			}
		}
		return nullptr;
	};

	// Sanity check
	{
		const UClass* ExpectedClass = nullptr;
		if (BindingInfo.ViewModelIndex < 1 && BindingInfo.NotifyFieldClass)
		{
			ExpectedClass = ReadFieldContext->OptionalSource->AuthoritativeClass;
		}
		else if (BindingInfo.ViewModelIndex >= 0)
		{
			ExpectedClass = GetClassFromField(ReadFieldContext->SkeletalGeneratedFields[BindingInfo.ViewModelIndex]);
		}

		if (ExpectedClass == nullptr || BindingInfo.NotifyFieldClass == nullptr || !ExpectedClass->IsChildOf(BindingInfo.NotifyFieldClass))
		{
			return MakeError(LOCTEXT("InvalidNotifyFieldClassInternal", "Internal error. The viewmodel class doesn't matches."));
		}
	}

	// The INotifyFieldValueChanged/viewmodel is not the first and only INotifyFieldValueChanged/viewmodel property path.
	//Create a new source in PropertyPath creator mode. Create a special binding to update the viewmodel when it changes.
	//This binding (calling this function) will use the new source.
	if (BindingInfo.ViewModelIndex >= 1)
	{
		if (!GetDefault<UMVVMDeveloperProjectSettings>()->bAllowLongSourcePath)
		{
			return MakeError(LOCTEXT("DynamicSourceEntryNotSupport", "Long source entry is not supported. Add the viewmodel manually."));
		}

		for (int32 DynamicIndex = 1; DynamicIndex <= BindingInfo.ViewModelIndex; ++DynamicIndex)
		{
			if (!ReadFieldContext->SkeletalGeneratedFields.IsValidIndex(DynamicIndex))
			{
				return MakeError(LOCTEXT("DynamicSourceEntryInternalIndex", "Internal error. The source index is not valid."));
			}

			FName NewSourceName;
			FName NewParentSourceName;
			FString NewSourcePropertyPath;
			const UClass* NewSourceAuthoritativeClass = nullptr;
			{
				TStringBuilder<512> PropertyPathBuilder;
				TStringBuilder<512> DynamicNameBuilder;
				for (int32 Index = 0; Index <= DynamicIndex; ++Index)
				{
					if (Index > 0)
					{
						PropertyPathBuilder << TEXT('.');
						DynamicNameBuilder << TEXT('_');
					}
					PropertyPathBuilder << ReadFieldContext->SkeletalGeneratedFields[Index].GetName();
					DynamicNameBuilder << ReadFieldContext->SkeletalGeneratedFields[Index].GetName();

					if (Index == DynamicIndex - 1)
					{
						NewParentSourceName = FName(DynamicNameBuilder.ToString());
					}
				}

				NewSourceName = FName(DynamicNameBuilder.ToString());
				NewSourcePropertyPath = PropertyPathBuilder.ToString();

				const UClass* OwnerSkeletalClass = GetClassFromField(ReadFieldContext->SkeletalGeneratedFields[DynamicIndex]);
				if (OwnerSkeletalClass == nullptr)
				{
					return MakeError(LOCTEXT("DVM_GeneratedFieldInvalid", "Internal error. The GeneratedFiled is invalid."));
				}
				NewSourceAuthoritativeClass = OwnerSkeletalClass->GetAuthoritativeClass();
				if (NewSourceAuthoritativeClass == nullptr)
				{
					return MakeError(LOCTEXT("DVM_AuthoritativeClassInvalid", "Internal error. No authoritative class."));
				}
			}

			// Does the parent exist
			TSharedPtr<FCompilerBindingSource> ParentBindingSource;
			{
				const TSharedRef<FCompilerBindingSource>* FoundParentBindingSource = NeededBindingSources.FindByPredicate([NewParentSourceName](const TSharedRef<FCompilerBindingSource>& Other)
					{
						return Other->Name == NewParentSourceName;
					});
				if (FoundParentBindingSource == nullptr)
				{
					return MakeError(LOCTEXT("DVM_InvalidParentBindingSource", "Internal error. Can't find the parent binding source."));
				}
				ParentBindingSource = *FoundParentBindingSource;
			}

			// Did we already create the new source. It could be a dynamic or one added by the user
			{
				const TSharedRef<FCompilerBindingSource>* FoundBindingSource = NeededBindingSources.FindByPredicate([NewSourceName](const TSharedRef<FCompilerBindingSource>& Other)
					{
						return Other->Name == NewSourceName;
					});
				if (FoundBindingSource)
				{
					if ((*FoundBindingSource)->Type != FCompilerBindingSource::EType::DynamicViewmodel)
					{
						return MakeError(LOCTEXT("DVM_ViewmodelOfSameName", "The dynamic viewmodel cannot be added. There is already a source with that name."));
					}
					// is the class the same
					if (!(*FoundBindingSource)->AuthoritativeClass->IsChildOf(NewSourceAuthoritativeClass))
					{
						return MakeError(LOCTEXT("DVM_ExistingNotSameClass", "Internal error. The viewmodel was already added and is not the same type."));
					}
				}

				const TSharedRef<FCompilerSourceViewModelDynamicCreatorContext>* FoundViewModelDynamicCreatorContext = SourceViewModelDynamicCreatorContexts.FindByPredicate([NewSourceName](const TSharedRef<FCompilerSourceViewModelDynamicCreatorContext>& Other)
					{
						return Other->Source->Name == NewSourceName;
					});
				if (FoundViewModelDynamicCreatorContext)
				{
					if (FoundBindingSource == nullptr)
					{
						return MakeError(LOCTEXT("DVM_BindingSourceShouldExist", "Internal error. The binding source should exist."));
					}
					if ((*FoundViewModelDynamicCreatorContext)->Source != *FoundBindingSource)
					{
						return MakeError(LOCTEXT("DVM_BindingSourceShouldBeTheSame", "Internal error. The source should be the same."));
					}
					if ((*FoundViewModelDynamicCreatorContext)->ParentSource->Name != NewParentSourceName)
					{
						return MakeError(LOCTEXT("DVM_BindingSourceParentSameName", "Internal error. The parent name should be the same."));
					}
				}

				const FCompilerViewModelCreatorContext* FoundViewModelCreatorContext = ViewModelCreatorContexts.FindByPredicate([NewSourceName](const FCompilerViewModelCreatorContext& Other)
					{
						return Other.ViewModelContext.GetViewModelName() == NewSourceName;
					});
				if (FoundViewModelCreatorContext)
				{
					if (FoundBindingSource == nullptr)
					{
						return MakeError(LOCTEXT("DVM_BindingSourceShouldExist", "Internal error. The binding source should exist."));
					}
					if (FoundViewModelCreatorContext->Source != *FoundBindingSource)
					{
						return MakeError(LOCTEXT("DVM_BindingSourceShouldBeTheSame", "Internal error. The source should be the same."));
					}
					if (FoundViewModelDynamicCreatorContext == nullptr)
					{
						return MakeError(LOCTEXT("DVM_DynamicCreatorContextShouldExit", "Internal error. The creator context should exist."));
					}
					if (FoundViewModelCreatorContext->DynamicContext != *FoundViewModelDynamicCreatorContext)
					{
						return MakeError(LOCTEXT("DVM_DynamicContextSHouldBeSame", "Internal error. The creator context should be the same."));
					}
					if (FoundViewModelCreatorContext->ViewModelContext.CreationType != EMVVMBlueprintViewModelContextCreationType::PropertyPath)
					{
						return MakeError(LOCTEXT("DVM_DynamicContextShouldBePropertyPath", "Internal error. The existing creator context should use a property path."));
					}
					if (FoundViewModelCreatorContext->ViewModelContext.ViewModelPropertyPath != NewSourcePropertyPath)
					{
						return MakeError(LOCTEXT("DVM_DynamicContextSamePropertyPath", "Internal error. The existing creator context use the same property path."));
					}
				}

				if (FoundBindingSource && FoundViewModelCreatorContext == nullptr && FoundViewModelDynamicCreatorContext == nullptr)
				{
					return MakeError(LOCTEXT("DVM_MissingDefinition", "Internal error. There are missing definition for the dynamic viewmodel."));
				}

				if (FoundBindingSource)
				{
					Result.Source = *FoundBindingSource;
					Result.ViewModelDynamic = *FoundViewModelDynamicCreatorContext;
					continue; // already exist and correct to use.
				}
			}

			if (!NewSourceAuthoritativeClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
			{
				return MakeError(LOCTEXT("DVM_NewDynamicNotViewmodel", "The dynamic viewmodel is not an actual viewmodel."));
			}

			// Create the new source
			TSharedRef<FCompilerBindingSource> NewBindingSource = MakeShared<FCompilerBindingSource>();
			NewBindingSource->AuthoritativeClass = NewSourceAuthoritativeClass;
			NewBindingSource->Name = NewSourceName;
			NewBindingSource->Type = FCompilerBindingSource::EType::DynamicViewmodel;
			NewBindingSource->bIsOptional = ParentBindingSource->bIsOptional;
			NeededBindingSources.Add(NewBindingSource);

			Result.Source = NewBindingSource;

			TSharedRef<FCompilerSourceViewModelDynamicCreatorContext> NewViewModelDynamic = MakeShared<FCompilerSourceViewModelDynamicCreatorContext>();
			NewViewModelDynamic->Source = NewBindingSource;
			NewViewModelDynamic->ParentSource = ParentBindingSource;
			NewViewModelDynamic->NotificationId = FFieldNotificationId(ReadFieldContext->SkeletalGeneratedFields[DynamicIndex].GetName());
			SourceViewModelDynamicCreatorContexts.Add(NewViewModelDynamic);

			Result.ViewModelDynamic = NewViewModelDynamic;

			FCompilerViewModelCreatorContext& NewViewModelCreatorContext = ViewModelCreatorContexts.AddDefaulted_GetRef();
			NewViewModelCreatorContext.ViewModelContext = FMVVMBlueprintViewModelContext(NewSourceAuthoritativeClass, NewSourceName);
			NewViewModelCreatorContext.ViewModelContext.bCreateSetterFunction = false;
			NewViewModelCreatorContext.ViewModelContext.bOptional = NewBindingSource->bIsOptional;
			NewViewModelCreatorContext.ViewModelContext.CreationType = EMVVMBlueprintViewModelContextCreationType::PropertyPath;
			NewViewModelCreatorContext.ViewModelContext.ViewModelPropertyPath = NewSourcePropertyPath;
			NewViewModelCreatorContext.Source = NewBindingSource;
			NewViewModelCreatorContext.DynamicContext = NewViewModelDynamic;
		}
	}

	return MakeValue(MakeShared<FCompilerNotifyFieldId>(Result));
}


bool FMVVMViewBlueprintCompiler::IsPropertyPathValid(const UBlueprint* Context, TArrayView<const FMVVMConstFieldVariant> PropertyPath)
{
	const UStruct* CurrentContainer = Context->GeneratedClass ? Context->GeneratedClass : Context->SkeletonGeneratedClass;
	int32 PathLength = PropertyPath.Num();
	for (int32 Index = 0; Index < PathLength; Index++)
	{
		const FMVVMConstFieldVariant& Field = PropertyPath[Index];

		if (CurrentContainer == nullptr)
		{
			return false;
		}
		if (Field.IsEmpty())
		{
			return false;
		}
		if (Field.IsProperty())
		{
			if (Field.GetProperty() == nullptr)
			{
				return false;
			}

			if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsPropertyAllowed(Context, CurrentContainer, Field.GetProperty()))
			{
				return false;
			}
		}
		if (Field.IsFunction())
		{
			if (Field.GetFunction() == nullptr)
			{
				return false;
			}
			const UClass* CurrentContainerAsClass = Cast<const UClass>(CurrentContainer);
			if (CurrentContainerAsClass == nullptr)
			{
				return false;
			}
			if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsFunctionAllowed(Context, CurrentContainerAsClass, Field.GetFunction()))
			{
				return false;
			}
		}

		TValueOrError<const UStruct*, void> FieldAsContainerResult = UE::MVVM::FieldPathHelper::GetFieldAsContainer(Field);
		CurrentContainer = FieldAsContainerResult.HasValue() ? FieldAsContainerResult.GetValue() : nullptr;
	}
	return true;
}

bool FMVVMViewBlueprintCompiler::CanBeSetInNative(TArrayView<const FMVVMConstFieldVariant> PropertyPath)
{
	for (int32 Index = PropertyPath.Num() - 1; Index >= 0; --Index)
	{
		const FMVVMConstFieldVariant& Variant = PropertyPath[Index];
		// Stop the algo if the path is already a function.
		if (Variant.IsFunction())
		{
			return true;
		}

		// If the BP is defined in BP and has Net flags or FieldNotify flag, then the VaraibleSet K2Node need to be used to generate the proper byte-code.
		if (Variant.IsProperty())
		{
			// If it's an object then the path before the object doesn't matter.
			if (const FObjectPropertyBase* PropertyBase = CastField<const FObjectPropertyBase>(Variant.GetProperty()))
			{
				bool bLastPath = Index >= PropertyPath.Num() - 1;
				if (!bLastPath)
				{
					return true;
				}
			}

			if (Cast<UBlueprintGeneratedClass>(Variant.GetProperty()->GetOwnerStruct()))
			{
				if (Variant.GetProperty()->HasMetaData(FName("FieldNotify")) || Variant.GetProperty()->HasAnyPropertyFlags(CPF_Net))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FMVVMViewBlueprintCompiler::TestGenerateSetter(const UBlueprint* Context, FStringView ObjectName, FStringView FieldPath, FStringView FunctionName)
{
#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	{
		UObject* FoundObject = FindObject<UObject>(nullptr, ObjectName.GetData(), false);
		WidgetBlueprint = Cast<UWidgetBlueprint>(FoundObject);
	}

	if (WidgetBlueprint == nullptr)
	{
		return;
	}

	UWidgetBlueprintGeneratedClass* NewSkeletonClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->SkeletonGeneratedClass);
	TValueOrError<TArray<FMVVMConstFieldVariant>, FText> SkeletalSetterPathResult = FieldPathHelper::GenerateFieldPathList(NewSkeletonClass, FieldPath, false);
	if (SkeletalSetterPathResult.HasError())
	{
		return;
	}
	if (!IsPropertyPathValid(Context, SkeletalSetterPathResult.GetValue()))
	{
		return;
	}

	UEdGraph* GeneratedSetterGraph = UE::MVVM::FunctionGraphHelper::CreateFunctionGraph(WidgetBlueprint, FunctionName, EFunctionFlags::FUNC_None, TEXT(""), false);
	if (GeneratedSetterGraph == nullptr)
	{
		return;
	}

	const FProperty* SetterProperty = nullptr;
	if (SkeletalSetterPathResult.GetValue().Num() > 0 && ensure(SkeletalSetterPathResult.GetValue().Last().IsProperty()))
	{
		SetterProperty = SkeletalSetterPathResult.GetValue().Last().GetProperty();
	}
	if (SetterProperty == nullptr)
	{
		return;
	}

	UE::MVVM::FunctionGraphHelper::AddFunctionArgument(GeneratedSetterGraph, SetterProperty, "NewValue");
	UE::MVVM::FunctionGraphHelper::GenerateSetter(WidgetBlueprint, GeneratedSetterGraph, SkeletalSetterPathResult.GetValue());
#endif
}

} //namespace

#undef LOCTEXT_NAMESPACE
