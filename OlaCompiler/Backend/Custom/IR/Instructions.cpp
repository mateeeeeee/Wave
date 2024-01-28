#include "Instructions.h"
#include "GlobalValue.h"
#include "BasicBlock.h"

namespace ola
{

	Function* CallInst::GetCalledFunction() const
	{
		if (Function* F = dyn_cast<Function>(GetCalledOperand()))
			if (F->GetType() == GetFunctionType()) return F;
		return nullptr;
	}

	Function* CallInst::GetCaller()
	{
		return GetParent()->GetParent();
	}

	BranchInst::BranchInst(BasicBlock* if_true, BasicBlock* bb) : Instruction(ValueKind_Branch, VoidType::Get(if_true->GetContext()), 1, bb)
	{
		OLA_ASSERT(if_true);
		Op<0>() = if_true;
		Assert();
	}
	BranchInst::BranchInst(BasicBlock* if_true, Instruction* position) : Instruction(ValueKind_Branch, VoidType::Get(if_true->GetContext()), 1, position)
	{
		OLA_ASSERT(if_true);
		Op<0>() = if_true;
		Assert();
	}
	BranchInst::BranchInst(BasicBlock* if_true, BasicBlock* if_false, Value* cond, BasicBlock* bb) : Instruction(ValueKind_Branch, VoidType::Get(if_true->GetContext()), 3, bb)
	{
		Op<0>() = if_true;
		Op<1>() = if_false;
		Op<2>() = cond;
		Assert();
	}
	BranchInst::BranchInst(BasicBlock* if_true, BasicBlock* if_false, Value* cond, Instruction* position) : Instruction(ValueKind_Branch, VoidType::Get(if_true->GetContext()), 3, position)
	{
		Op<0>() = if_true;
		Op<1>() = if_false;
		Op<2>() = cond;
		Assert();
	}
	BasicBlock* BranchInst::GetSuccessor(uint32 i) const
	{
		OLA_ASSERT_MSG(i < GetNumSuccessors(), "Successor # out of range for Branch!");
		OLA_ASSERT(isa<BasicBlock>(GetOperand(i)));
		return cast<BasicBlock>(GetOperand(i));
	}
	void BranchInst::SetSuccessor(uint32 idx, BasicBlock* successor)
	{
		OLA_ASSERT_MSG(idx < GetNumSuccessors(), "Successor # out of range for Branch!");
		SetOperand(idx, successor);
	}

	SwitchInst::SwitchInst(Value* Value, BasicBlock* Default, unsigned NumCases, Instruction* InsertBefore) : Instruction(ValueKind_Switch, VoidType::Get(Value->GetContext()), 2 + NumCases * 2, InsertBefore)
	{
		Op<0>() = Value;
		Op<1>() = Default;
	}

	SwitchInst::SwitchInst(Value* Value, BasicBlock* Default, unsigned NumCases, BasicBlock* InsertAtEnd) : Instruction(ValueKind_Switch, VoidType::Get(Value->GetContext()), 2 + NumCases * 2, InsertAtEnd)
	{
		Op<0>() = Value;
		Op<1>() = Default;
	}

	BasicBlock* SwitchInst::GetDefaultDest() const
	{
		return cast<BasicBlock>(GetOperand(1));
	}

	void SwitchInst::SetDefaultDest(BasicBlock* DefaultCase)
	{
		SetOperand(1, DefaultCase);
	}

	BasicBlock* SwitchInst::GetSuccessor(uint32 idx) const
	{
		OLA_ASSERT_MSG(idx < GetNumSuccessors(), "Successor idx out of range for switch!");
		return cast<BasicBlock>(GetOperand(idx * 2 + 1));
	}

	void SwitchInst::SetSuccessor(uint32 idx, BasicBlock* successor)
	{
		OLA_ASSERT_MSG(idx < GetNumSuccessors(), "Successor # out of range for switch!");
		SetOperand(idx * 2 + 1, successor);
	}

}

