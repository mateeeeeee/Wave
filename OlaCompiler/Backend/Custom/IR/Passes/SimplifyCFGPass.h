#pragma once
#include "Backend/Custom/IR/FunctionPass.h"
#include "Backend/Custom/IR/PassRegistry.h"

namespace ola
{
	class Instruction;
	class CallInst;

	class SimplifyCFGPass : public FunctionPass
	{
		inline static Char id = 0;
	public:
		SimplifyCFGPass() : FunctionPass(id) {}
		virtual Bool RunOn(Function& F, FunctionAnalysisManager& FAM) override;
		static void const* ID() { return &id; }
	};
	OLA_REGISTER_PASS(SimplifyCFGPass, "Simplify CFG Pass");

	inline FunctionPass* CreateSimplifyCFGPass() { return new SimplifyCFGPass(); }
}