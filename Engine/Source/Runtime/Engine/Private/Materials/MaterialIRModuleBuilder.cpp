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

namespace IR = UE::MIR;

struct FMaterialIRModuleBuilder::FPrivate
{
	static void Build_GenerateOutputInstructions(FMaterialIRModuleBuilder& Builder, UE::MIR::FEmitter& Emitter)
	{
		// Prepare the array of FSetMaterialOutputInstr outputs from the material attributes inputs.
		FMaterialInputDescription Input;
		for (int32 Index = 0; UE::Utility::NextMaterialAttributeInput(Builder.BaseMaterial, Index, Input); ++Index)
		{
			EMaterialProperty Property = (EMaterialProperty)Index;

			IR::FSetMaterialOutput* Output = Emitter.EmitSetMaterialOutput(Property, nullptr);

			if (Input.bUseConstant)
			{
				Output->ArgValue = Emitter.EmitConstantFromShaderValue(Input.ConstantValue);
			}
			else if (!Input.Input->IsConnected())
			{
				Output->ArgValue = UE::Utility::CreateMaterialAttributeDefaultValue(Emitter, Builder.BaseMaterial, Property);
			}
			else
			{
				Builder.ExpressionAnalysisStack.Add(Input.Input->Expression);
			}
		}
	}

	static void Build_AnalyzeExpressionGraph(FMaterialIRModuleBuilder& Builder, UE::MIR::FEmitter & Emitter)
	{
		TSet<UMaterialExpression*> BuiltExpressions;

		while (!Builder.ExpressionAnalysisStack.IsEmpty())
		{
			Emitter.Expression = Builder.ExpressionAnalysisStack.Last();

			// If expression is clean, nothing to be done.
			if (BuiltExpressions.Contains(Emitter.Expression))
			{
				Builder.ExpressionAnalysisStack.Pop(EAllowShrinking::No);
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

				Builder.ExpressionAnalysisStack.Push(It->Expression);
			}

			// If on top of the stack there's a different expression, we have a dependency to analyze first.
			if (Builder.ExpressionAnalysisStack.Last() != Emitter.Expression) {
				continue;
			}

			// 
			Builder.ExpressionAnalysisStack.Pop();
			BuiltExpressions.Add(Emitter.Expression);

			//
			for (FExpressionInputIterator It{ Emitter.Expression}; It; ++It)
			{
				if (FExpressionOutput* ConnectedOutput = It->GetConnectedOutput())
				{
					// Fetch the value flowing through connected output.
					IR::FValuePtr* ValuePtr = Builder.OutputValues.Find(ConnectedOutput);
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
		}
	}

	static void Build_LinkMaterialOutputsToIncomingValues(FMaterialIRModuleBuilder& Builder, UE::MIR::FEmitter& Emitter)
	{
		FMaterialInputDescription Input;
		for (IR::FSetMaterialOutput* Output : Builder.Module->Outputs)
		{
			if (Output->ArgValue || !ensure(Builder.BaseMaterial->GetExpressionInputDescription(Output->Property, Input))) {
				continue;
			}

			IR::FValuePtr* ValuePtr = Builder.OutputValues.Find(Input.Input->Expression->GetOutput(0));
			check(ValuePtr);

			Output->ArgValue = *ValuePtr;
		}
	}
};

bool FMaterialIRModuleBuilder::Build(UMaterial* InMaterial, FMaterialIRModule* TargetModule)
{
	BaseMaterial = InMaterial;

	Module = TargetModule;
	Module->Empty();

	IR::FEmitter Emitter{ this, BaseMaterial, Module };

	FPrivate::Build_GenerateOutputInstructions(*this, Emitter);
	FPrivate::Build_AnalyzeExpressionGraph(*this, Emitter);
	FPrivate::Build_LinkMaterialOutputsToIncomingValues(*this, Emitter);

	return true;
}

#endif // #if WITH_EDITOR
