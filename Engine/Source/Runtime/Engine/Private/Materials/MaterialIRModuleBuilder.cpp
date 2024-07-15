// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModuleBuilder.h"

#if WITH_EDITOR

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIREmitter.h"
#include "MaterialIRUtility.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "MaterialExpressionIO.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInsights.h"
#include "Async/ParallelFor.h"

namespace IR = UE::MIR;

struct FMaterialIRModuleBuilder::FPrivate
{
	FMaterialIRModuleBuilder& Builder;
	FMaterialIRModuleBuildParams Params;
	FMaterialIRModule& Module;
	IR::FEmitter& Emitter;
	TArray<UMaterialExpression*> ExpressionAnalysisStack;
	TArray<IR::FInstruction*> InstructionStack;

	void Build_GenerateOutputInstructions()
	{
		// Prepare the array of FSetMaterialOutputInstr outputs from the material attributes inputs.
		FMaterialInputDescription Input;
		for (int32 Index = 0; UE::Utility::NextMaterialAttributeInput(Params.Material, Index, Input); ++Index)
		{
			EMaterialProperty Property = (EMaterialProperty)Index;

			IR::FSetMaterialOutput* Output = Emitter.EmitSetMaterialOutput(Property, nullptr);

			if (Input.bUseConstant)
			{
				Output->Arg = Emitter.EmitConstantFromShaderValue(Input.ConstantValue);
			}
			else if (!Input.Input->IsConnected())
			{
				Output->Arg = UE::Utility::CreateMaterialAttributeDefaultValue(Emitter, Params.Material, Property);
			}
			else
			{
				ExpressionAnalysisStack.Add(Input.Input->Expression);
			}
		}
	}

	void Build_AnalyzeExpressionGraph()
	{
		TSet<UMaterialExpression*> BuiltExpressions;

		while (!ExpressionAnalysisStack.IsEmpty())
		{
			Emitter.Expression = ExpressionAnalysisStack.Last();

			// If expression is clean, nothing to be done.
			if (BuiltExpressions.Contains(Emitter.Expression))
			{
				ExpressionAnalysisStack.Pop(EAllowShrinking::No);
				continue;
			}

			// Push to the expression stack all dependencies that still need to be analyzed.
			for (FExpressionInputIterator It{ Emitter.Expression}; It; ++It)
			{
				// Ignore disconnected inputs and connected expressions already built.
				if (!It->IsConnected() || BuiltExpressions.Contains(It->Expression))
				{
					continue;
				}

				ExpressionAnalysisStack.Push(It->Expression);
			}

			// If on top of the stack there's a different expression, we have a dependency to analyze first.
			if (ExpressionAnalysisStack.Last() != Emitter.Expression) {
				continue;
			}

			// 
			ExpressionAnalysisStack.Pop();
			BuiltExpressions.Add(Emitter.Expression);

			//
			for (FExpressionInputIterator It{ Emitter.Expression}; It; ++It)
			{
				if (FExpressionOutput* ConnectedOutput = It->GetConnectedOutput())
				{
					// Fetch the value flowing through connected output.
					IR::FValue** ValuePtr = Builder.OutputValues.Find(ConnectedOutput);
					check(ValuePtr && TEXT("Output value not found."));

					// Set the value flowing into this input.
					Builder.InputValues.Add(It.Input, *ValuePtr);
				}
			}

			// And clear the expression errors.
			Emitter.bHasExprBuildError = false;

			// Invoke the expression build function. This will perform semantic analysis, error reporting and
			// emit IR values for its outputs (which will flow into connected expressions inputs).
			Emitter.Expression->Build(Emitter);

			// Populate the insight information about this expression pins.
			if (Params.TargetInsight)
			{
				AddExpressionConnectionInsights(Emitter.Expression);
			}
		}
	}

	void AddExpressionConnectionInsights(UMaterialExpression* Expression)
	{
		// Update expression inputs insight.
		for (FExpressionInputIterator It{ Expression}; It; ++It)
		{
			if (!It->IsConnected())
			{
				continue;
			}

			IR::FValue** Value = Builder.InputValues.Find(It.Input);
			PushConnectionInsight(Expression, It.Index, It->Expression, It->OutputIndex, Value ? (*Value)->Type : nullptr);
		}
	}
	
	void Build_LinkMaterialOutputsToIncomingValues()
	{
		for (IR::FSetMaterialOutput* Output : Module.Outputs)
		{
			FMaterialInputDescription Input;
			ensure(Params.Material->GetExpressionInputDescription(Output->Property, Input));

			if (!Output->Arg)
			{
				IR::FValue** ValuePtr = Builder.OutputValues.Find(Input.Input->Expression->GetOutput(0));
				check(ValuePtr);

				Builder.InputValues.Add(Input.Input, *ValuePtr);
				Output->Arg = *ValuePtr;
			}

			if (Params.TargetInsight)
			{
				check(Output->Arg);
				PushConnectionInsight(Params.Material, (int)Output->Property, Input.Input->Expression, Input.Input->OutputIndex, Output->Arg->Type);
			}
		}
	}

	void PushConnectionInsight(const UObject* InputObject, int InputIndex, const UMaterialExpression* OutputExpression, int OutputIndex, IR::FTypePtr Type)
	{
		FMaterialInsights::FConnectionInsight Insight;
		Insight.InputObject = InputObject,
		Insight.OutputExpression = OutputExpression,
		Insight.InputIndex = InputIndex,
		Insight.OutputIndex = OutputIndex,
		Insight.ValueType = Type ? Type->ToValueType() : UE::Shader::EValueType::Any,
		
		Params.TargetInsight->ConnectionInsights.Push(Insight);
	}

	void Build_FinalizeValueGraph()
	{
		InstructionStack.Reserve(64);

		for (IR::FSetMaterialOutput* Output : Module.Outputs)
		{
			InstructionStack.Push(Output);
		}

		while (!InstructionStack.IsEmpty())
		{
			IR::FValue* Instr = InstructionStack.Pop();
			for (IR::FValue* UseValue : Instr->GetUses())
			{
				IR::FInstruction* Use = UseValue->AsInstruction();
				if (!Use)
				{
					continue;
				}

				Use->NumUsers += 1;

				if (!(Use->Flags & IR::IF_Counted))
				{
					Use->SetFlags(IR::IF_Counted);
					InstructionStack.Push(Use);
				}
			}
		}
	}

	void Build_PopulateBlock()
	{
		// This function walks the instruction graph and puts each instruction into the inner most possible block.
		InstructionStack.Empty(InstructionStack.Max());

		for (IR::FSetMaterialOutput* Output : Module.Outputs)
		{
			Output->Block = Module.RootBlock;
			InstructionStack.Add(Output);
		}

		while (!InstructionStack.IsEmpty()) {
			IR::FInstruction* Instr = InstructionStack.Pop();

			// Push the instruction in its block
			Instr->Next = Instr->Block->Instructions;
			Instr->Block->Instructions = Instr;

			IR::FValue* UseValue;
			IR::FBlock* InnerBlock;
			for (int32 i = 0; Instr->GetInnerBlock(i, UseValue, InnerBlock); ++i)
			{
				IR::FInstruction* Use = UseValue->AsInstruction();
				if (!Use)
				{
					continue;
				}

				// Update dependency's block to be a child of current instruction's block.
				if (InnerBlock != Instr->Block)
				{
					InnerBlock->Parent = Instr->Block;
					InnerBlock->Level = Instr->Block->Level + 1;
				}

				// Set the dependency's block to the common block betwen its current block and this one.
				Use->Block = Use->Block
					? FindCommonParentBlock(Use->Block, InnerBlock)
					: InnerBlock;

				// Increase the number of times this dependency instruction has been considered.
				// When all of its users have processed, we can carry on visiting this instruction.
				++Use->NumProcessedUsers;
				check(Use->NumProcessedUsers <= Use->NumUsers);

				// If all dependants have been processed, we can carry the processing from this dependency.
				if (Use->NumProcessedUsers == Use->NumUsers)
				{
					InstructionStack.Push(Use);
				}
			}
		}
	}

    static IR::FBlock* FindCommonParentBlock(IR::FBlock* A, IR::FBlock* B)
    {
        if (A == B) {
            return A;
        }

        while (A->Level > B->Level) {
            A = A->Parent;
        }

        while (B->Level > A->Level) {
            B = B->Parent;
        }

        while (A != B) {
            A = A->Parent;
            B = B->Parent;
        }

        return A;
    }
};

bool FMaterialIRModuleBuilder::Build(const FMaterialIRModuleBuildParams& Params, FMaterialIRModule* TargetModule)
{
	TargetModule->Empty();
	TargetModule->ShaderPlatform = Params.ShaderPlatform;

	IR::FEmitter Emitter{ this, Params.Material, TargetModule };

	FPrivate Private{ *this, Params, *TargetModule, Emitter };

	Private.Build_GenerateOutputInstructions();
	Private.Build_AnalyzeExpressionGraph();

	if (Private.Emitter.IsInvalid())
	{
		return false;
	}

	Private.Build_LinkMaterialOutputsToIncomingValues();
	Private.Build_FinalizeValueGraph();
	Private.Build_PopulateBlock();

	return true;
}

#endif // #if WITH_EDITOR
