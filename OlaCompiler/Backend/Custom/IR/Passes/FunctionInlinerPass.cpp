#include <unordered_map>
#include "FunctionInlinerPass.h"
#include "Backend/Custom/IR/GlobalValue.h"
#include "Backend/Custom/IR/IRBuilder.h"
#include "Backend/Custom/IR/IRContext.h"

namespace ola
{

	Bool FunctionInlinerPass::RunOn(Function& F, FunctionAnalysisManager& FAM)
	{
		Bool Changed = false;
		std::vector<CallInst*> CallsToInline;
		for (BasicBlock& BB : F)
		{
			for (Instruction& I : BB.Instructions())
			{
				if (CallInst* CI = dyn_cast<CallInst>(&I))
				{
					if (ShouldInline(CI)) CallsToInline.push_back(CI);
				}
			}
		}
		for (CallInst* CI : CallsToInline)
		{
			if (InlineFunction(CI))
			{
				Changed = true;
			}
		}
		return Changed;
	}

	Bool FunctionInlinerPass::ShouldInline(CallInst* CI)
	{
		Function* Callee = CI->GetCalleeAsFunction(); 
		if (!Callee || Callee->IsDeclaration())
		{
			return false;
		}
		if (CI->GetBasicBlock()->GetFunction() == Callee)
		{
			return false;
		}
		return Callee->Blocks().Size() <= 5;
	}

	Bool FunctionInlinerPass::InlineFunction(CallInst* CI)
	{
		IRContext& Ctx = CI->GetContext();
		IRBuilder Builder(Ctx);
		Function* Callee = CI->GetCalleeAsFunction();
		BasicBlock* CallBlock = CI->GetBasicBlock();
		Function* Caller = CI->GetCaller();

		std::unordered_map<Value*, Value*> ValueMap;

		auto ArgIt = Callee->ArgBegin();
		for (Uint32 i = 0; i < CI->GetNumOperands() - 1; ++i, ++ArgIt)
		{
			ValueMap[*ArgIt] = CI->GetOperand(i);
		}

		std::unordered_map<BasicBlock*, BasicBlock*> BBMap;
		for (BasicBlock& BB : *Callee)
		{
			std::string name(BB.GetName());
			name += ".inlined";
			BasicBlock* InlinedBB = Builder.AddBlock(Caller, CallBlock->GetNextNode(), name);
			BBMap[&BB] = InlinedBB;
		}

		for (auto& BBPair : BBMap)
		{
			BasicBlock* OrigBB = BBPair.first;
			BasicBlock* NewBB  = BBPair.second;

			for (Instruction& I : OrigBB->Instructions())
			{
				if (isa<ReturnInst>(&I))
				{
					if (I.GetNumOperands() > 0)
					{
						Value* RetVal = I.GetOperand(0);
						Value* MappedVal = ValueMap[RetVal];
						CI->ReplaceAllUsesWith(MappedVal);
					}
					continue;
				}

				Builder.SetCurrentBlock(NewBB);
				Instruction* NewInst = Builder.CloneInst(&I);

				for (Uint32 i = 0; i < NewInst->GetNumOperands(); ++i)
				{
					Value* Op = NewInst->GetOperand(i);
					if (Value* MappedOp = ValueMap[Op])
					{
						NewInst->SetOperand(i, MappedOp);
					}
				}
				ValueMap[&I] = NewInst;
			}
		}

		BasicBlock* InlinedEntry = BBMap[&Callee->GetEntryBlock()];
		BasicBlock* CallBlockRemainder = CallBlock->SplitBasicBlock(CI);
		CallBlock->GetTerminator()->EraseFromParent();

		Builder.SetCurrentBlock(CallBlock);
		Builder.MakeInst<BranchInst>(Ctx, InlinedEntry);

		for (Instruction& I : CallBlockRemainder->Instructions())
		{
			if (PhiInst* Phi = dyn_cast<PhiInst>(&I))
			{
				for (Uint32 i = 0; i < Phi->GetNumIncomingValues(); ++i)
				{
					if (Phi->GetIncomingBlock(i) == CallBlock)
					{
						for (auto& BBPair : BBMap)
						{
							if (ReturnInst* RI = dyn_cast<ReturnInst>(BBPair.first->GetTerminator()))
							{
								Phi->AddIncoming(Phi->GetIncomingValue(i), BBPair.second);
							}
						}
						Phi->RemoveIncomingValue(i);
						break;
					}
				}
			}
		}
		CI->EraseFromParent();
		return true;
	}

}
