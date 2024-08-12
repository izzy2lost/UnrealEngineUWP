// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModuleBuilder.h"

#if WITH_EDITOR

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRDebug.h"
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
#include "Engine/Texture.h"

namespace MIR = UE::MIR;

struct FMaterialIRModuleBuilder::FPrivate
{
	FMaterialIRModuleBuilder& Builder;
	FMaterialIRModuleBuildParams Params;
	FMaterialIRModule& Module;
	MIR::FEmitter& Emitter;
	TArray<UMaterialExpression*> ExpressionAnalysisStack;
	TArray<MIR::FInstruction*> InstructionStack;

	void Build_Initialize()
	{
		Module.Empty();
		Module.ShaderPlatform = Params.ShaderPlatform;

		Emitter.Initialize();
	}

	void Build_GenerateOutputInstructions()
	{
		// Prepare the array of FSetMaterialOutputInstr outputs from the material attributes inputs.
		FMaterialInputDescription Input;
		for (int32 Index = 0; UE::Utility::NextMaterialAttributeInput(Params.Material, Index, Input); ++Index)
		{
			EMaterialProperty Property = (EMaterialProperty)Index;

			MIR::FSetMaterialOutput* Output = Emitter.EmitSetMaterialOutput(Property, nullptr);

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

	void Build_BuildIRGraph()
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
					if (MIR::FValue** ValuePtr = Builder.OutputValues.Find(ConnectedOutput))
					{
						// Set the value flowing into this input.
						Builder.InputValues.Add(It.Input, *ValuePtr);
					}
					else
					{
						Builder.InputValues.Remove(It.Input);
					}
				}
			}

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

	void Build_LinkMaterialOutputsToIncomingValues()
	{
		for (MIR::FSetMaterialOutput* Output : Module.Outputs)
		{
			FMaterialInputDescription Input;
			ensure(Params.Material->GetExpressionInputDescription(Output->Property, Input));

			if (!Output->Arg)
			{
				MIR::FValue** ValuePtr = Builder.OutputValues.Find(Input.Input->GetConnectedOutput());
				check(ValuePtr);

				Builder.InputValues.Add(Input.Input, *ValuePtr);

				MIR::FTypePtr OutputArgType = MIR::FType::FromShaderType(Input.Type);
				Output->Arg = Emitter.TryEmitConstruct(OutputArgType, *ValuePtr);
			}

			if (Params.TargetInsight)
			{
				check(Output->Arg);
				PushConnectionInsight(Params.Material, (int)Output->Property, Input.Input->Expression, Input.Input->OutputIndex, Output->Arg->Type);
			}
		}
	}

	void Build_AnalyzeIRGraph()
	{
		InstructionStack.Reserve(64);

		for (MIR::FSetMaterialOutput* Output : Module.Outputs)
		{
			InstructionStack.Push(Output);
		}

		while (!InstructionStack.IsEmpty())
		{
			MIR::FValue* Instr = InstructionStack.Pop();
			for (MIR::FValue* UseValue : Instr->GetUses())
			{
				if (UseValue && !(UseValue->Flags & MIR::VF_ValueAnalyzed))
				{
					UseValue->SetFlags(MIR::VF_ValueAnalyzed);
					Build_AnalyzeValue(UseValue);
				}

				MIR::FInstruction* Use = UseValue->AsInstruction();
				if (!Use)
				{
					continue;
				}

				Use->NumUsers += 1;

				if (!(Use->Flags & MIR::VF_InstructionAnalyzed))
				{
					Use->SetFlags(MIR::VF_InstructionAnalyzed);
					InstructionStack.Push(Use);
				}
			}
		}
	}

	void Build_AnalyzeValue(MIR::FValue* Value)
	{
		if (auto ExternalInput = Value->As<MIR::FExternalInput>())
		{
			Module.Statistics.ExternalInputUsedMask[SF_Vertex][(int)ExternalInput->Id] = true;
			Module.Statistics.ExternalInputUsedMask[SF_Pixel][(int)ExternalInput->Id] = true;
		}
		else if (auto TextureSample = Value->As<MIR::FTextureSample>())
		{
			EMaterialTextureParameterType ParamType = UE::Utility::TextureMaterialValueTypeToParameterType(TextureSample->Texture->GetMaterialType());

			FMaterialTextureParameterInfo ParamInfo{};
			ParamInfo.ParameterInfo = { "", EMaterialParameterAssociation::GlobalParameter, INDEX_NONE };
			ParamInfo.SamplerSource = SSM_FromTextureAsset; // TODO - Is this needed?

			ParamInfo.TextureIndex = Params.Material->GetReferencedTextures().Find(TextureSample->Texture);
			check(ParamInfo.TextureIndex != INDEX_NONE);

			TextureSample->TextureParameterIndex = Module.CompilationOutput.UniformExpressionSet.FindOrAddTextureParameter(ParamType, ParamInfo);
		}
	}

	void Build_PopulateBlocks()
	{
		// This function walks the instruction graph and puts each instruction into the inner most possible block.
		InstructionStack.Empty(InstructionStack.Max());

		for (MIR::FSetMaterialOutput* Output : Module.Outputs)
		{
			Output->Block = Module.RootBlock;
			InstructionStack.Add(Output);
		}

		while (!InstructionStack.IsEmpty())
		{
			MIR::FInstruction* Instr = InstructionStack.Pop();
			if (MIR::FSetMaterialOutput* Output = Instr->As<MIR::FSetMaterialOutput>())
			{
				if (Output->Property == EMaterialProperty::MP_BaseColor)
				{
					static int l = 0;
					++l;
				}
			}

			// Push the instruction to its block in reverse order (push front)
			Instr->Next = Instr->Block->Instructions;
			Instr->Block->Instructions = Instr;

			TArrayView<MIR::FValue*> Uses = Instr->GetUses();
			for (int32 UseIndex = 0; UseIndex < Uses.Num(); ++UseIndex)
			{
				MIR::FValue* Use = Uses[UseIndex];
				MIR::FInstruction* UseInstr = Use->AsInstruction();
				if (!UseInstr)
				{
					continue;
				}

				// Get the block into which the dependency instruction should go.
				MIR::FBlock* TargetBlock = Instr->GetDesiredBlockForUse(UseIndex);

				// Update dependency's block to be a child of current instruction's block.
				if (TargetBlock != Instr->Block)
				{
					TargetBlock->Parent = Instr->Block;
					TargetBlock->Level = Instr->Block->Level + 1;
				}

				// Set the dependency's block to the common block betwen its current block and this one.
				UseInstr->Block = UseInstr->Block
					? UseInstr->Block->FindCommonParentWith(TargetBlock)
					: TargetBlock;

				// Increase the number of times this dependency instruction has been considered.
				// When all of its users have processed, we can carry on visiting this instruction.
				++UseInstr->NumProcessedUsers;
				check(UseInstr->NumProcessedUsers <= UseInstr->NumUsers);

				// If all dependants have been processed, we can carry the processing from this dependency.
				if (UseInstr->NumProcessedUsers == UseInstr->NumUsers)
				{
					InstructionStack.Push(UseInstr);
				}
			}
		}
	}

	void Build_Finalize()
	{
		/* Produce the module statistics */
		for (int TexCoordIndex = 0; TexCoordIndex < MIR::TexCoordMaxNum; ++TexCoordIndex)
		{
			MIR::EExternalInput TexCoordInput = MIR::TexCoordIndexToExternalInput(TexCoordIndex);
			if (Module.Statistics.ExternalInputUsedMask[SF_Vertex][(int)TexCoordInput])
			{
				Module.Statistics.NumVertexTexCoords = TexCoordIndex + 1;
			}
			if (Module.Statistics.ExternalInputUsedMask[SF_Pixel][(int)TexCoordInput])
			{
				Module.Statistics.NumPixelTexCoords = TexCoordIndex + 1;
			}
		}

		/* Configure the compilation output */
		FMaterialCompilationOutput& CompilationOutput = Module.CompilationOutput;
		CompilationOutput.NumUsedUVScalars = Module.Statistics.NumPixelTexCoords * 2;
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

			MIR::FValue** Value = Builder.InputValues.Find(It.Input);
			PushConnectionInsight(Expression, It.Index, It->Expression, It->OutputIndex, Value ? (*Value)->Type : nullptr);
		}
	}
	
	void PushConnectionInsight(const UObject* InputObject, int InputIndex, const UMaterialExpression* OutputExpression, int OutputIndex, MIR::FTypePtr Type)
	{
		FMaterialInsights::FConnectionInsight Insight;
		Insight.InputObject = InputObject,
		Insight.OutputExpression = OutputExpression,
		Insight.InputIndex = InputIndex,
		Insight.OutputIndex = OutputIndex,
		Insight.ValueType = Type ? Type->ToValueType() : UE::Shader::EValueType::Any,
		
		Params.TargetInsight->ConnectionInsights.Push(Insight);
	}
};

bool FMaterialIRModuleBuilder::Build(const FMaterialIRModuleBuildParams& Params, FMaterialIRModule* TargetModule)
{
	MIR::FEmitter Emitter{ this, Params.Material, TargetModule };
	FPrivate Private{ *this, Params, *TargetModule, Emitter };

	Private.Build_Initialize();
	Private.Build_GenerateOutputInstructions();
	Private.Build_BuildIRGraph();

	if (Private.Emitter.IsInvalid())
	{
		return false;
	}

	Private.Build_LinkMaterialOutputsToIncomingValues();
	Private.Build_AnalyzeIRGraph();
	Private.Build_PopulateBlocks();
	Private.Build_Finalize();

	UE::MIR::DebugDumpIRUseGraph(*TargetModule);

	return true;
}

#endif // #if WITH_EDITOR
