#include "PrinterBase.h"
#include "IRModule.h"
#include "GlobalValue.h"

namespace ola
{

	std::string_view NameManager::GetUniqueName(Value const* V)
	{
		if (isa<GlobalValue>(V))
		{
			if (!global_name_map.contains(V)) RegisterValue(V);
			return global_name_map[V];
		}
		else
		{
			if (!local_name_map.contains(V)) RegisterValue(V);
			return local_name_map[V];
		}
	}

	void NameManager::ClearLocals()
	{
		local_name_map.clear();
		local_slot_map.clear();
	}

	void NameManager::RegisterValue(Value const* V)
	{
		if (isa<GlobalValue>(V))
		{
			std::string prefixed_name = GetPrefixedName(V);
			if (global_name_map.contains(V)) return;

			if (global_slot_map.contains(prefixed_name))
			{
				Uint32 slot = global_slot_map[prefixed_name]++;
				global_name_map[V] = prefixed_name + std::to_string(slot);
			}
			else
			{
				global_slot_map[prefixed_name]++;
				global_name_map[V] = prefixed_name;
				if (prefixed_name.size() <= 1) global_name_map[V] += "0";
			}
		}
		else
		{
			std::string prefixed_name = GetPrefixedName(V);
			if (local_name_map.contains(V)) return;

			if (local_slot_map.contains(prefixed_name))
			{
				Uint32 slot = local_slot_map[prefixed_name]++;
				local_name_map[V] = prefixed_name + std::to_string(slot);
			}
			else
			{
				local_slot_map[prefixed_name]++;
				local_name_map[V] = prefixed_name;
				if (prefixed_name.size() <= 1) local_name_map[V] += "0";
			}
		}
	}

	std::string NameManager::GetPrefixedName(Value const* V)
	{
		if (isa<BasicBlock>(V))			return GetPrefixedName(V->GetName(), NamePrefix::Label);
		else if (isa<GlobalValue>(V))   return GetPrefixedName(V->GetName(), NamePrefix::Global);
		else							return GetPrefixedName(V->GetName(), NamePrefix::Local);
	}

	std::string NameManager::GetPrefixedName(std::string_view name, NamePrefix prefix)
	{
		std::string str_prefix = "";
		switch (prefix)
		{
			break;
		case NamePrefix::Global:
			str_prefix = "@";
			break;
		case NamePrefix::Local:
			str_prefix = "%";
			break;
		case NamePrefix::None:
		case NamePrefix::Label:
		default:
			break;
		}
		return str_prefix + std::string(name);
	}

	void PrinterBase::PrintGlobalVariable(GlobalVariable const* GV)
	{
		if (GV->IsDeclaration())
		{
			Emit("declare {} ", GetUniqueName(GV));
			PrintType(GV->GetType());
			EmitNewline();
			return;
		}
		std::string linkage = GV->GetLinkage() == Linkage::External ? "external" : "internal";
		Emit("define {} {} ", linkage, GetUniqueName(GV));
		PrintType(GV->GetType());
		EmitSpace();
		PrintConstant(dyn_cast<Constant>(GV->GetInitValue()));
		EmitNewline();
	}

	void PrinterBase::PrintFunction(Function const* F)
	{
		if (F->IsDeclaration())
		{
			Emit("declare {} ", GetUniqueName(F));
			PrintType(F->GetFunctionType());
			EmitNewline();
			return;
		}
		std::string linkage = F->GetLinkage() == Linkage::External ? "external" : "internal";
		Emit("define {} {} ", linkage, GetUniqueName(F));

		IRFuncType* func_type = F->GetFunctionType();
		{
			PrintType(func_type->GetReturnType());
			Emit("(");
			Uint32 const param_count = func_type->GetParamCount();
			for (Uint32 i = 0; i < param_count; ++i)
			{
				PrintType(func_type->GetParamType(i));
				Emit(" %{}", F->GetArg(i)->GetName());
				Emit(",");
			}
			if (param_count > 0) PopOutput<1>();
			Emit(")");
		}
		EmitLn(" {{");
		for (auto const& BB : F->Blocks())
		{
			PrintBasicBlock(BB);
		}
		EmitLn("}}");
	}

	void PrinterBase::PrintBasicBlock(BasicBlock const& BB)
	{
		EmitLn("{}:", GetUniqueName(&BB));
		for (Instruction const& I : BB)
		{
			PrintInstruction(I);
		}
	}

	void PrinterBase::PrintInstruction(Instruction const& I)
	{
		IRType* type = I.GetType();
		if (!type->IsVoid())
		{
			Emit("{} = ", GetUniqueName(&I));
		}
		Emit("{} ", I.GetOpcodeName());

		if (isa<AllocaInst>(&I))
		{
			AllocaInst const* AI = cast<AllocaInst>(&I);
			PrintType(AI->GetAllocatedType());
		}
		else if (isa<BranchInst>(&I))
		{
			BranchInst const* BI = cast<BranchInst>(&I);
			if (BI->IsConditional())
			{
				PrintOperand(BI->GetCondition());
				Emit(", ");
				PrintOperand(BI->GetTrueTarget());
				Emit(", ");
				PrintOperand(BI->GetFalseTarget());
			}
			else
			{
				PrintOperand(BI->GetTrueTarget());
			}
		}
		else if (isa<PhiInst>(&I))
		{
			PhiInst const* Phi = cast<PhiInst>(&I);
			Uint OpCount = Phi->GetNumOperands() / 2;
			PrintType(Phi->GetType());
			EmitSpace();
			for (Uint i = 0; i < OpCount; ++i)
			{
				Emit("[ ");
				PrintOperand(Phi->GetIncomingValue(i), false);
				Emit(", ");
				PrintOperand(Phi->GetIncomingBlock(i), false);
				Emit(" ], ");
			}
			if (I.GetNumOperands() > 0) PopOutput<2>();

		}
		else if (I.GetNumOperands() > 0)
		{
			IRType* type = I.GetOperand(0)->GetType(); 
			if (LoadInst const* LI = dyn_cast<LoadInst>(&I))
			{
				PrintType(LI->GetType());
				Emit(", ");
			}

			Bool print_all_types = false;
			if (isa<SelectInst>(&I) || isa<StoreInst>(&I)) 
			{
				print_all_types = true;
			}

			if (!print_all_types)
			{
				PrintType(type);
				EmitSpace();
			}
			for (auto& Op : I.Operands())
			{
				PrintOperand(Op.GetValue(), print_all_types);
				Emit(", ");
			}
			if(I.GetNumOperands() > 0) PopOutput<2>();
		}
		EmitNewline();
	}

	void PrinterBase::PrintOperand(Value const* V, Bool print_type)
	{
		if (!V)
		{
			Emit("null operand");
			return;
		}

		Constant const* C = dyn_cast<Constant>(V);
		if (C && !isa<GlobalValue>(C))
		{
			PrintConstant(C);
			return;
		}

		if (print_type && V->GetType())
		{
			PrintType(V->GetType());
			EmitSpace();
		}
		Emit("{}", GetUniqueName(V));
	}

	void PrinterBase::PrintConstant(Constant const* V)
	{
		if (!V) return;
		OLA_ASSERT(isa<Constant>(V));
		Constant const* C = cast<Constant>(V);
		ConstantID const_id = C->GetConstantID();
		switch (const_id)
		{
		case ConstantID::Float:		 Emit("{}", cast<ConstantFloat>(C)->GetValue()); break;
		case ConstantID::Integer:	 Emit("{}", cast<ConstantInt>(C)->GetValue()); break;
		case ConstantID::String:	 Emit("{}", cast<ConstantString>(C)->GetValue()); break;
		case ConstantID::Array:
		{
			ConstantArray const* CA = cast<ConstantArray>(C);
			Emit("[");
			for (Constant* E : CA->Values())
			{
				PrintConstant(E);
				Emit(",");
			}
			Emit("]");
		}
		break;
		default: OLA_ASSERT_MSG(false, "not yet implemented");
		}
	}

	void PrinterBase::PrintType(IRType* type)
	{
		std::string type_string;
		switch (type->GetKind())
		{
		case IRTypeKind::Void:		Emit("void"); break;
		case IRTypeKind::Float:		Emit("f64");  break;
		case IRTypeKind::Pointer:	Emit("ptr");  break;
		case IRTypeKind::Label:		Emit("label");  break;
		case IRTypeKind::Integer:
		{
			IRIntType* int_type = cast<IRIntType>(type);
			switch (int_type->GetWidth())
			{
			case 1: Emit("i8");  break;
			case 8: Emit("i64"); break;
			}
		}
		break;
		case IRTypeKind::Array:
		{
			IRArrayType* array_type = cast<IRArrayType>(type);
			PrintType(array_type->GetElementType());
			Emit("[{}]", array_type->GetArraySize());
		}
		break;
		case IRTypeKind::Function:
		{
			IRFuncType* func_type = cast<IRFuncType>(type);
			PrintType(func_type->GetReturnType());
			Emit("(");
			Uint32 const param_count = func_type->GetParamCount();
			for (Uint32 i = 0; i < param_count; ++i)
			{
				PrintType(func_type->GetParamType(i));
				Emit(",");
			}
			if(param_count > 0) PopOutput<1>();
			Emit(")");
		}
		break;
		case IRTypeKind::Struct: OLA_ASSERT_MSG(false, "Not implemented yet"); break;
		}
	}
}

