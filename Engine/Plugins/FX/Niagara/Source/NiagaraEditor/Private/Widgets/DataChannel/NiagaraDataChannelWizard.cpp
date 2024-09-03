// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelWizard.h"

#include "EdGraphSchema_Niagara.h"
#include "NiagaraClipboard.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelPublic.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelRead.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelWrite.h"
#include "ViewModels/NiagaraScratchPadScriptViewModel.h"
#include "ViewModels/NiagaraScriptGraphViewModel.h"
#include "IDetailsView.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"


using namespace UE::Niagara::Wizard;

#define LOCTEXT_NAMESPACE "NiagaraDataChannelWizard"

namespace UE::Niagara::Wizard::DataChannel
{
	struct FSelectAssetPageBase : FModuleWizardPage
	{
		FSelectAssetPageBase()
		{
			Name = LOCTEXT("AssetPageName", "Select asset");
		}
		virtual ~FSelectAssetPageBase() override = default;

		virtual bool CanGoToNextPage() const override { return GetAsset() != nullptr; };
		virtual bool CanCompleteWizard() const override { return CanGoToNextPage(); };

		virtual UNiagaraDataChannelAsset* GetAsset() const = 0;
		
		UNiagaraDataChannel* GetDataChannel() const
		{
			if (UNiagaraDataChannelAsset* ChannelAsset = GetAsset())
			{
				return ChannelAsset->Get();
			}			
			return nullptr;
		}
		
		TSharedRef<SWidget> GetDetailsViewContent(UObject* DetailsViewObject)
		{
			TSharedRef<IDetailsView> DetailsView = Utilities::CreateDetailsView();
			DetailsView->SetObject(DetailsViewObject, true);
			
			return SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(15)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AssetPageLabel", "Please select which data channel you want to use"))
				]
				+SVerticalBox::Slot()
				[
					DetailsView
				];
		}
	};

	struct FSelectVariablesPageBase : FModuleWizardPage
	{
		explicit FSelectVariablesPageBase(FSelectAssetPageBase* InPreviousPage) : PreviousPage(InPreviousPage)
		{
			Name = LOCTEXT("VariablesPageName", "Select variables");
		}

		virtual ~FSelectVariablesPageBase() override = default;

		virtual bool CanGoToNextPage() const override { return VariablesToProcess.Num() > 0; };
		virtual bool CanCompleteWizard() const override { return AllVariables.IsEmpty() || CanGoToNextPage(); };

		virtual FText GetHeaderLabel() = 0;

		virtual void RefreshContent() override
		{
			FObjectKey NewDataChannelRef;
			TArray<FNiagaraDataChannelVariable> DataChannelVariables;
			if (UNiagaraDataChannelAsset* ChannelAsset = PreviousPage->GetAsset())
			{
				NewDataChannelRef = ChannelAsset;
				DataChannelVariables = ChannelAsset->Get()->GetVariables();
			}
			if (NewDataChannelRef != LastDataChannelRef)
			{
				ModuleName = CreateNewModuleName(); 
			} 

			bool bCheckAll = AllVariables.IsEmpty() || NewDataChannelRef != LastDataChannelRef;
			AllVariables.Empty(DataChannelVariables.Num());
			for (const FNiagaraDataChannelVariable& Var : DataChannelVariables)
			{
				*AllVariables.Add_GetRef(MakeShared<FNiagaraDataChannelVariable>()).Get() = Var;
				if (bCheckAll)
				{
					VariablesToProcess.Add(Var.Version);
				}
			}

			if (VarListView.IsValid())
			{
				VarListView->RebuildList();
			}
			LastDataChannelRef = NewDataChannelRef;
		}

		virtual TSharedRef<SWidget> GetContent() override
		{
			return SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(2)
				.AutoHeight()
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]
				+SVerticalBox::Slot()
				.Padding(15)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(GetHeaderLabel())
				]
				+SVerticalBox::Slot()
				[
					SAssignNew(VarListView, SListView<TSharedPtr<FNiagaraDataChannelVariable>>)
						.ListItemsSource(&AllVariables)
						.OnGenerateRow(this, &FSelectVariablesPageBase::GenerateRow)
						.SelectionMode(ESelectionMode::Single)
				]
				+SVerticalBox::Slot()
				.Padding(0, 10)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WriteModuleNameText", "Module Name: "))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0)
					[
						SNew( SEditableTextBox )
						.MinDesiredWidth(200)
						.Text(this, &FSelectVariablesPageBase::GetModuleNameText)
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextCommitted(this, &FSelectVariablesPageBase::SetModuleName)
					]
				];
		}

		FText GetModuleNameText() const
		{
			return ModuleName;
		}

		void SetModuleName(const FText& NewName, ETextCommit::Type)
		{
			ModuleName = NewName;
		}

		FText CreateNewModuleName() const
		{
			FText AssetName;
			if (UNiagaraDataChannelAsset* DataChannelAsset = PreviousPage->GetAsset())
			{
				AssetName = FText::FromString(DataChannelAsset->GetName());
			}
			return GetFormattedModuleName(AssetName);
		}

		virtual FText GetFormattedModuleName(const FText& AssetName) const = 0;

		TSharedRef<ITableRow> GenerateRow(const TSharedPtr<FNiagaraDataChannelVariable> Var, const TSharedRef<STableViewBase>& OwnerTable)
		{
			FLinearColor TypeColor = UEdGraphSchema_Niagara::GetTypeColor(Var->GetType());
			return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
				.Padding(FMargin(5, 0))
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &FSelectVariablesPageBase::OnCheckStateChanged, *Var.Get())
					.IsChecked(this, &FSelectVariablesPageBase::OnGetCheckState, *Var.Get())
					.ToolTipText(FText::Format(LOCTEXT("VariablesSelectionTooltipFmt", "Name: {0}\nType: {1}"), FText::FromName(Var->GetName()), Var->GetType().GetNameText()))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.AutoWidth()
						[
							SNew(SImage)
							.ColorAndOpacity(TypeColor)
							.Image(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Module.TypeIconPill"))
						]
						+ SHorizontalBox::Slot()
						.Padding(4, 2, 2, 2)
						[
							SNew(STextBlock)
							.MinDesiredWidth(150)
							.Text(FText::FromName(Var->GetName()))
						]
					]
				];
		}

		void OnCheckStateChanged(const ECheckBoxState NewState, FNiagaraDataChannelVariable Var)
		{
			if (NewState == ECheckBoxState::Checked)
			{
				VariablesToProcess.Add(Var.Version);
			}
			else if (NewState == ECheckBoxState::Unchecked)
			{
				VariablesToProcess.Remove(Var.Version);
			}
		}

		ECheckBoxState OnGetCheckState(FNiagaraDataChannelVariable Var) const
		{
			return VariablesToProcess.Contains(Var.Version) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		TArray<TSharedPtr<FNiagaraDataChannelVariable>> AllVariables;
		TSet<FGuid> VariablesToProcess;
		FSelectAssetPageBase* PreviousPage;
		FObjectKey LastDataChannelRef;
		FText ModuleName;

		TSharedPtr<SListView<TSharedPtr<FNiagaraDataChannelVariable>>> VarListView;
	};
}

TSharedRef<FModuleWizardModel> DataChannel::CreateReadNDCModuleWizardModel()
{
	struct FSelectAssetPage : FSelectAssetPageBase
	{
		virtual UNiagaraDataChannelAsset* GetAsset() const override
		{
			if (UNiagaraDataChannelReadModuleData* ModuleData = Data.Get())
			{
				return ModuleData->DataChannel;
			}
			return nullptr;
		}
		
		virtual TSharedRef<SWidget> GetContent() override
		{
			Data.Reset(NewObject<UNiagaraDataChannelReadModuleData>());
			return GetDetailsViewContent(Data.Get());
		}

		
		TStrongObjectPtr<UNiagaraDataChannelReadModuleData> Data;
	};

	struct FSelectVariablesPage : FSelectVariablesPageBase
	{
		explicit FSelectVariablesPage(FSelectAssetPage* InPreviousPage) : FSelectVariablesPageBase(InPreviousPage)
		{}
		virtual ~FSelectVariablesPage() override = default;

		virtual FText GetHeaderLabel() override
		{
			return LOCTEXT("VariablesPageLabel", "Please select the variables to read from the data channel");
		}
		
		virtual FText GetFormattedModuleName(const FText& AssetName) const override
		{
			return FText::Format(LOCTEXT("ReadModuleNameFmt", "Read {0}"), AssetName);
		}
	};
	
	struct FReadNDCModel : FModuleWizardModel
	{
		FReadNDCModel()
		{
			AssetPage = MakeShared<FSelectAssetPage>();
			VariablesPage = MakeShared<FSelectVariablesPage>(AssetPage.Get());
			Pages.Add(AssetPage.ToSharedRef());
			Pages.Add(VariablesPage.ToSharedRef());
		}
		virtual ~FReadNDCModel() override = default;
		
		virtual void GenerateNewModuleContent(TSharedPtr<FNiagaraScratchPadScriptViewModel> ScratchPadScriptViewModel, const TArray<const UNiagaraNodeFunctionCall*>& PreviousModules) override
		{
			FText ScriptName = VariablesPage->ModuleName;
			ScratchPadScriptViewModel->SetScriptName(ScriptName.IsEmptyOrWhitespace() ? VariablesPage->CreateNewModuleName() : ScriptName);
			
			UNiagaraDataChannel* Channel = AssetPage->GetDataChannel();
			UNiagaraGraph* Graph = ScratchPadScriptViewModel->GetGraphViewModel()->GetGraph();
			if (Channel && Graph)
			{
				const UEdGraphSchema_Niagara* GraphSchema = Graph->GetNiagaraSchema();
				UNiagaraNodeParameterMapGet* MapGetNode = Utilities::FindSingleNodeChecked<UNiagaraNodeParameterMapGet>(Graph);
				UNiagaraNodeParameterMapSet* MapSetNode = Utilities::FindSingleNodeChecked<UNiagaraNodeParameterMapSet>(Graph);

				// Add inputs
				UEdGraphPin* DIPin = Utilities::AddReadParameterPin(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), FName("Data Channel"), MapGetNode);
				UEdGraphPin* IndexPin = Utilities::AddReadParameterPin(FNiagaraTypeDefinition::GetIntDef(), FName("Read Index"), MapGetNode);

				// Call read function
				if (UNiagaraNodeFunctionCall* ReadFunction = Utilities::CreateDataInterfaceFunctionNode(UNiagaraDataInterfaceDataChannelRead::StaticClass(), FName("Read"), Graph))
				{
					// connect index input pins
					ReadFunction->AutowireNewNode(DIPin);
					UEdGraphPin* IndexInput = ReadFunction->GetInputPin(1);
					if (IndexInput && IndexInput->GetName() == TEXT("Index"))
					{
						GraphSchema->TryCreateConnection(IndexPin, IndexInput);
					}

					// create and connect read success output pin
					UEdGraphPin* SuccessVarPin = Utilities::AddWriteParameterPin(FNiagaraTypeDefinition::GetBoolDef(), FName("Output.Module.ReadSuccess"), MapSetNode);
					UEdGraphPin* SuccessOutPin = ReadFunction->GetOutputPin(0);
					if (SuccessOutPin && SuccessOutPin->GetName() == TEXT("Success"))
					{
						GraphSchema->TryCreateConnection(SuccessOutPin, SuccessVarPin);
					}

					// add channel variable pins to read node
					for (const FNiagaraDataChannelVariable& Var : Channel->GetVariables())
					{
						if (!VariablesPage->VariablesToProcess.Contains(Var.Version))
						{
							continue;
						}
						
						FNiagaraTypeDefinition SwcType = Var.GetType();
						if (SwcType.IsEnum() == false)
						{
							SwcType = FNiagaraTypeDefinition(FNiagaraTypeHelper::GetSWCStruct(Var.GetType().GetScriptStruct()));
						}
						FNiagaraVariable SWCVar(SwcType, Var.GetName());
						UEdGraphPin* ReadParamPin = ReadFunction->AddParameterPin(SWCVar, EGPD_Output);

						// add matching node on map set and connect them
						UEdGraphPin* SetVarPin = Utilities::AddWriteParameterPin(SwcType, FName(TEXT("Output.Module.") + Var.GetName().ToString()), MapSetNode);
						if (ReadParamPin && SetVarPin)
						{
							GraphSchema->TryCreateConnection(ReadParamPin, SetVarPin);
						}
					}
				}
				
				FNiagaraStackGraphUtilities::RelayoutGraph(*Graph);
				ScratchPadScriptViewModel->ApplyChanges();
			}
		}
		
		virtual bool UpdateModuleInputs(UNiagaraClipboardContent* NewModule, const TArray<const UNiagaraNodeFunctionCall*>& PreviousModules) override
		{
			if (UNiagaraDataChannelAsset* Channel = AssetPage->GetAsset())
			{
				TArray<TObjectPtr<const UNiagaraClipboardFunctionInput>> FunctionInputs = NewModule->FunctionInputs;
				for (const UNiagaraClipboardFunctionInput* FunctionInput : FunctionInputs)
				{
					if (FunctionInput->InputType == FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()))
					{
						// set data interface module input
						if (UNiagaraDataInterfaceDataChannelRead* DataInterface = Cast<UNiagaraDataInterfaceDataChannelRead>(FunctionInput->Data))
						{
							DataInterface->Channel = Channel;
							DataInterface->bReadCurrentFrame = AssetPage->Data->bReadCurrentFrame;
							DataInterface->bUpdateSourceDataEveryTick = AssetPage->Data->bUpdateSourceDataEveryTick;
						}
					}

					// bind index input for particle scripts. System and emitter scripts default to 0
					if (FunctionInput->InputType == FNiagaraTypeDefinition::GetIntDef() && FNiagaraUtilities::ConvertScriptUsageToStaticSwitchContext(TargetUsage) == ENiagaraScriptContextStaticSwitch::Particle)
					{
						UNiagaraClipboardFunctionInput* EditableInput = const_cast<UNiagaraClipboardFunctionInput*>(FunctionInput);
						EditableInput->ValueMode = ENiagaraClipboardFunctionInputValueMode::Linked;
						EditableInput->Linked = SYS_PARAM_PARTICLES_UNIQUE_ID.GetName();
					}
				}
				return true;
			}
			return false;
		}

		TSharedPtr<FSelectAssetPage> AssetPage;
		TSharedPtr<FSelectVariablesPage> VariablesPage;
	};
	
	return MakeShared<FReadNDCModel>();
}

TSharedRef<FModuleWizardModel> DataChannel::CreateWriteNDCModuleWizardModel()
{
	struct FSelectAssetPage : FSelectAssetPageBase
	{
		virtual UNiagaraDataChannelAsset* GetAsset() const override
		{
			if (UNiagaraDataChannelWriteModuleData* ModuleData = Data.Get())
			{
				return ModuleData->DataChannel;
			}
			return nullptr;
		}
		
		virtual TSharedRef<SWidget> GetContent() override
		{
			Data.Reset(NewObject<UNiagaraDataChannelWriteModuleData>());
			return GetDetailsViewContent(Data.Get());
		}

		TStrongObjectPtr<UNiagaraDataChannelWriteModuleData> Data;
	};

	struct FSelectVariablesPage : FSelectVariablesPageBase
	{
		explicit FSelectVariablesPage(FSelectAssetPage* InPreviousPage) : FSelectVariablesPageBase(InPreviousPage)
		{}
		virtual ~FSelectVariablesPage() override = default;

		virtual FText GetHeaderLabel() override
		{
			return LOCTEXT("VariablesWritePageLabel", "Please select which data channel variables to write to");
		}

		virtual FText GetFormattedModuleName(const FText& AssetName) const override
		{
			return FText::Format(LOCTEXT("WriteModuleNameFmt", "Write {0}"), AssetName);
		}
	};
	
	struct FWriteNDCModel : FModuleWizardModel
	{
		FWriteNDCModel()
		{
			AssetPage = MakeShared<FSelectAssetPage>();
			VariablesPage = MakeShared<FSelectVariablesPage>(AssetPage.Get());
			Pages.Add(AssetPage.ToSharedRef());
			Pages.Add(VariablesPage.ToSharedRef());
		}
		virtual ~FWriteNDCModel() override = default;
		
		virtual void GenerateNewModuleContent(TSharedPtr<FNiagaraScratchPadScriptViewModel> ScratchPadScriptViewModel, const TArray<const UNiagaraNodeFunctionCall*>& PreviousModules) override
		{
			FText ScriptName = VariablesPage->ModuleName;
			ScratchPadScriptViewModel->SetScriptName(ScriptName.IsEmptyOrWhitespace() ? VariablesPage->CreateNewModuleName() : ScriptName);
			
			UNiagaraDataChannel* Channel = AssetPage->GetDataChannel();
			UNiagaraGraph* Graph = ScratchPadScriptViewModel->GetGraphViewModel()->GetGraph();
			if (Channel == nullptr || Graph == nullptr)
			{
				return;
			}
			const UEdGraphSchema_Niagara* GraphSchema = Graph->GetNiagaraSchema();
			UNiagaraNodeParameterMapGet* MapGetNode = Utilities::FindSingleNodeChecked<UNiagaraNodeParameterMapGet>(Graph);
			UNiagaraNodeParameterMapSet* MapSetNode = Utilities::FindSingleNodeChecked<UNiagaraNodeParameterMapSet>(Graph);
			UNiagaraNodeInput* InputNode = Utilities::FindSingleNodeChecked<UNiagaraNodeInput>(Graph);
			ENiagaraDataChanneWriteModuleMode WriteMode = AssetPage->Data->WriteMode;
			
			// Add inputs
			UEdGraphPin* DIPin = Utilities::AddReadParameterPin(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelWrite::StaticClass()), FName("Data Channel"), MapGetNode);
			UEdGraphPin* ExecWritePin = Utilities::AddReadParameterPin(FNiagaraTypeDefinition::GetBoolDef(), FName("Execute Write"), MapGetNode);
			SetBoolDefaultValue(Graph, ExecWritePin->PinName, true);
			UEdGraphPin* IndexPin = nullptr;
			if (WriteMode == ENiagaraDataChanneWriteModuleMode::WriteToExistingElement)
			{
				IndexPin = Utilities::AddReadParameterPin(FNiagaraTypeDefinition::GetIntDef(), FName("Write Index"), MapGetNode);
			}

			// Call write function
			FName FunctionName = WriteMode == ENiagaraDataChanneWriteModuleMode::AppendNewElement ? FName("Append") : FName("Write");
			if (UNiagaraNodeFunctionCall* WriteFunction = Utilities::CreateDataInterfaceFunctionNode(UNiagaraDataInterfaceDataChannelWrite::StaticClass(), FunctionName, Graph))
			{
				// connect default function pins
				WriteFunction->AutowireNewNode(DIPin);
				GraphSchema->TryCreateConnection(InputNode->GetOutputPin(0), WriteFunction->GetInputPin(0));
				GraphSchema->TryCreateConnection(WriteFunction->GetOutputPin(0), MapSetNode->GetInputPin(0));
				
				UEdGraphPin* ExecInput = WriteFunction->GetInputPin(2);
				if (ExecInput && ExecInput->GetName() == TEXT("Emit"))
				{
					GraphSchema->TryCreateConnection(ExecWritePin, ExecInput);
				}
				
				UEdGraphPin* IndexInput = WriteFunction->GetInputPin(3);
				if (IndexInput && IndexInput->GetName() == TEXT("Index"))
				{
					GraphSchema->TryCreateConnection(IndexPin, IndexInput);
				}

				// create and connect write success output pin
				UEdGraphPin* SuccessVarPin = Utilities::AddWriteParameterPin(FNiagaraTypeDefinition::GetBoolDef(), FName("Output.Module.WriteSuccess"), MapSetNode);
				UEdGraphPin* SuccessOutPin = WriteFunction->GetOutputPin(1);
				if (SuccessOutPin && SuccessOutPin->GetName() == TEXT("Success"))
				{
					GraphSchema->TryCreateConnection(SuccessOutPin, SuccessVarPin);
				}

				// add channel variable pins to write node
				for (const FNiagaraDataChannelVariable& Var : Channel->GetVariables())
				{
					if (!VariablesPage->VariablesToProcess.Contains(Var.Version))
					{
						continue;
					}
					
					FNiagaraTypeDefinition SwcType = Var.GetType();
					if (SwcType.IsEnum() == false)
					{
						SwcType = FNiagaraTypeDefinition(FNiagaraTypeHelper::GetSWCStruct(Var.GetType().GetScriptStruct()));
					}
					FNiagaraVariable SWCVar(SwcType, Var.GetName());
					UEdGraphPin* WriteParamPin = WriteFunction->AddParameterPin(SWCVar, EGPD_Input);

					// add matching node on map get and connect them
					UEdGraphPin* SetVarPin = Utilities::AddReadParameterPin(SwcType, FName(TEXT("Module.") + Var.GetName().ToString()), MapGetNode);
					if (WriteParamPin && SetVarPin)
					{
						GraphSchema->TryCreateConnection(WriteParamPin, SetVarPin);
					}
				}
				
				FNiagaraStackGraphUtilities::RelayoutGraph(*Graph);
				ScratchPadScriptViewModel->ApplyChanges();
			}
		}
		
		virtual bool UpdateModuleInputs(UNiagaraClipboardContent* NewModule, const TArray<const UNiagaraNodeFunctionCall*>& PreviousModules) override
		{
			if (UNiagaraDataChannelAsset* Channel = AssetPage->GetAsset())
			{
				TArray<TObjectPtr<const UNiagaraClipboardFunctionInput>> FunctionInputs = NewModule->FunctionInputs;
				for (const UNiagaraClipboardFunctionInput* FunctionInput : FunctionInputs)
				{
					if (FunctionInput->InputType == FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelWrite::StaticClass()))
					{
						// set data interface module input
						if (UNiagaraDataInterfaceDataChannelWrite* DataInterface = Cast<UNiagaraDataInterfaceDataChannelWrite>(FunctionInput->Data))
						{
							DataInterface->Channel = Channel;
							DataInterface->bPublishToGame = AssetPage->Data->bPublishToGame;
							DataInterface->bPublishToCPU = AssetPage->Data->bPublishToCPU;
							DataInterface->bPublishToGPU = AssetPage->Data->bPublishToGPU;
							DataInterface->AllocationCount = AssetPage->Data->AllocationCount;
							DataInterface->AllocationMode = AssetPage->Data->AllocationMode;
							DataInterface->bUpdateDestinationDataEveryTick = AssetPage->Data->bUpdateDestinationDataEveryTick;
						}
					}

					// bind index input for particle scripts. System and emitter scripts default to 0
					if (FunctionInput->InputType == FNiagaraTypeDefinition::GetIntDef() && FunctionInput->InputName == FName("Write Index") && FNiagaraUtilities::ConvertScriptUsageToStaticSwitchContext(TargetUsage) == ENiagaraScriptContextStaticSwitch::Particle)
					{
						UNiagaraClipboardFunctionInput* EditableInput = const_cast<UNiagaraClipboardFunctionInput*>(FunctionInput);
						EditableInput->ValueMode = ENiagaraClipboardFunctionInputValueMode::Linked;
						EditableInput->Linked = SYS_PARAM_PARTICLES_UNIQUE_ID.GetName();
					}
				}
				return true;
			}
			return false;
		}

		void SetBoolDefaultValue(UNiagaraGraph* Graph, const FName& VarName, bool Value) const
		{
			if (UNiagaraScriptVariable* ScriptVariable = Graph->GetScriptVariable(VarName))
			{
				FNiagaraVariable Var(FNiagaraTypeDefinition::GetBoolDef(), FName("Var"));
				Var.SetValue(Value);
				ScriptVariable->SetDefaultValueData(Var.GetData());
				Graph->ScriptVariableChanged(ScriptVariable->Variable);
			}
		}

		TSharedPtr<FSelectAssetPage> AssetPage;
		TSharedPtr<FSelectVariablesPage> VariablesPage;
	};
		
	return MakeShared<FWriteNDCModel>();
}

TSharedRef<FModuleWizardGenerator> DataChannel::CreateNDCWizardGenerator()
{
	class NDCWizardGenerator : public FModuleWizardGenerator
	{
	public:
		virtual TArray<FAction> CreateWizardActions(ENiagaraScriptUsage Usage) override
		{
			TArray<FAction> WizardActions;

			FAction& ReadAction = WizardActions.AddDefaulted_GetRef();
			ReadAction.DisplayName = LOCTEXT("NewReadNDCModuleName", "Read From Data Channel...");
			ReadAction.Description = LOCTEXT("NewReadNDCModuleDescription", "Description: Create a new scratch pad module to read attributes from a data channel");
			ReadAction.Keywords = LOCTEXT("NewReadNDCModuleKeywords", "ndc reader datachannel get external");
			ReadAction.bSuggestedAction = true;
			ReadAction.WizardModel = CreateReadNDCModuleWizardModel();

			FAction& WriteAction = WizardActions.AddDefaulted_GetRef();
			WriteAction. DisplayName = LOCTEXT("NewWriteNDCModuleName", "Write To Data Channel...");
			WriteAction.Description = LOCTEXT("NewWriteNDCModuleDescription", "Description: Create a new scratch pad module to write attributes to a data channel");
			WriteAction.Keywords = LOCTEXT("NewWriteNDCModuleKeywords", "ndc writer datachannel save append external");
			WriteAction.bSuggestedAction = true;
			WriteAction.WizardModel = CreateWriteNDCModuleWizardModel();

			if (Usage == ENiagaraScriptUsage::EmitterUpdateScript)
			{
				//TODO: add spawn from ndc wizard
			}
			
			return WizardActions;
		}

		virtual ~NDCWizardGenerator() override = default;
	};
	return MakeShared<NDCWizardGenerator>();
}

#undef LOCTEXT_NAMESPACE
