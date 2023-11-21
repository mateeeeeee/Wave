#include "LLVMVisitor.h"
#include "Frontend/AST.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"


namespace wave
{
	LLVMVisitor::LLVMVisitor(llvm::LLVMContext& context, llvm::Module& module) : context(context), module(module), builder(context)
	{
		void_type  = llvm::Type::getVoidTy(context);
		bool_type  = llvm::Type::getInt1Ty(context);
		char_type  = llvm::Type::getInt8Ty(context);
		int_type   = llvm::Type::getInt64Ty(context);
		float_type = llvm::Type::getDoubleTy(context);
	}

	void LLVMVisitor::VisitAST(AST const* ast)
	{
		ast->translation_unit->Accept(*this);
	}

	void LLVMVisitor::Visit(NodeAST const&, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(TranslationUnit const& translation_unit, uint32)
	{
		for (auto&& decl : translation_unit.GetDecls()) decl->Accept(*this);
	}

	void LLVMVisitor::Visit(Decl const&, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(FunctionDecl const& function_decl, uint32)
	{
		QualType const& type = function_decl.GetType();
		WAVE_ASSERT(IsFunctionType(type));
		llvm::FunctionType* function_type = llvm::cast<llvm::FunctionType>(ConvertToLLVMType(type));
		llvm::Function::LinkageTypes linkage = function_decl.IsPublic() || function_decl.IsExtern() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
		llvm::Function* llvm_function = llvm::Function::Create(function_type, linkage, function_decl.GetName(), module);

		llvm::Argument* param_arg = llvm_function->arg_begin();
		for (auto& param : function_decl.GetParamDecls())
		{
			llvm::Value* llvm_param = &*param_arg;
			llvm_param->setName(param->GetName());
			llvm_value_map[param.get()] = llvm_param;
			++param_arg;
		}

		if (!function_decl.HasDefinition()) return;

		llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(context, "entry", llvm_function);
		builder.SetInsertPoint(entry_block);

		if(!function_type->getReturnType()->isVoidTy()) return_alloc = builder.CreateAlloca(function_type->getReturnType(), nullptr);
		exit_block = llvm::BasicBlock::Create(context, "exit", llvm_function);

		ConstLabelStmtPtrList labels = function_decl.GetLabels();
		for (LabelStmt const* label : labels)
		{
			std::string block_name = "label."; block_name += label->GetName();
			llvm::BasicBlock* label_block = llvm::BasicBlock::Create(context, block_name, llvm_function, exit_block);
			label_blocks[block_name] = label_block;
		}

		function_decl.GetBodyStmt()->Accept(*this);

		builder.SetInsertPoint(exit_block);
		if (return_alloc) builder.CreateRet(Load(function_type->getReturnType(), return_alloc));
		else builder.CreateRetVoid();

		std::vector<llvm::BasicBlock*> unreachable_blocks{};
		for (auto&& block : *llvm_function) if (block.hasNPredecessors(0) && &block != entry_block) unreachable_blocks.push_back(&block);

		std::vector<llvm::BasicBlock*> empty_blocks{};
		for (auto&& block : *llvm_function) if (block.empty()) empty_blocks.push_back(&block);

		for (auto empty_block : empty_blocks)
		{
			builder.SetInsertPoint(empty_block);
			builder.CreateAlloca(llvm::IntegerType::get(context, 1), nullptr, "nop");
			builder.CreateBr(exit_block);
		}
		if (entry_block->getTerminator() == nullptr)
		{
			builder.SetInsertPoint(entry_block);
			builder.CreateBr(exit_block);
		}

		label_blocks.clear();

		exit_block = nullptr;
		return_alloc = nullptr;

		llvm_value_map[&function_decl] = llvm_function;
	}

	void LLVMVisitor::Visit(MethodDecl const& method_decl, uint32)
	{
		ClassDecl const* class_decl = method_decl.GetParentDecl();
		llvm::Type* class_type = ConvertToLLVMType(ClassType(class_decl));

		QualType const& type = method_decl.GetType();
		WAVE_ASSERT(IsFunctionType(type));
		FunctionType const& function_type = type_cast<FunctionType>(type);

		std::span<FunctionParams const> function_params = function_type.GetParams();
		std::vector<llvm::Type*> param_types; param_types.reserve(function_params.size() + 1);
		param_types.push_back(llvm::PointerType::get(class_type, 0));
		for (auto const& func_param : function_params)
		{
			param_types.push_back(ConvertToLLVMType(func_param.type));
		}

		llvm::Type* return_type = ConvertToLLVMType(function_type.GetReturnType());
		llvm::FunctionType* llvm_function_type = llvm::FunctionType::get(return_type, param_types, false);

		llvm::Function::LinkageTypes linkage = method_decl.IsPublic() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
		std::string name(class_decl->GetName()); name += "::"; name += method_decl.GetName();
		llvm::Function* llvm_function = llvm::Function::Create(llvm_function_type, linkage, name, module);

		llvm::Argument* param_arg = llvm_function->arg_begin();
		llvm::Value* llvm_param = &*param_arg;
		llvm_param->setName("this");
		++param_arg;
		for (auto& param : method_decl.GetParamDecls())
		{
			llvm::Value* llvm_param = &*param_arg;
			llvm_param->setName(param->GetName());
			llvm_value_map[param.get()] = llvm_param;
			++param_arg;
		}

		//todo: move this to common
		llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(context, "entry", llvm_function);
		builder.SetInsertPoint(entry_block);

		this_struct_type = class_type;
		this_value = llvm_function->arg_begin();

		if (!llvm_function_type->getReturnType()->isVoidTy()) return_alloc = builder.CreateAlloca(llvm_function_type->getReturnType(), nullptr);
		exit_block = llvm::BasicBlock::Create(context, "exit", llvm_function);

		ConstLabelStmtPtrList labels = method_decl.GetLabels();
		for (LabelStmt const* label : labels)
		{
			std::string block_name = "label."; block_name += label->GetName();
			llvm::BasicBlock* label_block = llvm::BasicBlock::Create(context, block_name, llvm_function, exit_block);
			label_blocks[block_name] = label_block;
		}

		method_decl.GetBodyStmt()->Accept(*this);

		builder.SetInsertPoint(exit_block);
		if (return_alloc) builder.CreateRet(Load(llvm_function_type->getReturnType(), return_alloc));
		else builder.CreateRetVoid();

		std::vector<llvm::BasicBlock*> unreachable_blocks{};
		for (auto&& block : *llvm_function) if (block.hasNPredecessors(0) && &block != entry_block) unreachable_blocks.push_back(&block);

		std::vector<llvm::BasicBlock*> empty_blocks{};
		for (auto&& block : *llvm_function) if (block.empty()) empty_blocks.push_back(&block);

		for (auto empty_block : empty_blocks)
		{
			builder.SetInsertPoint(empty_block);
			builder.CreateAlloca(llvm::IntegerType::get(context, 1), nullptr, "nop");
			builder.CreateBr(exit_block);
		}
		if (entry_block->getTerminator() == nullptr)
		{
			builder.SetInsertPoint(entry_block);
			builder.CreateBr(exit_block);
		}

		label_blocks.clear();

		exit_block = nullptr;
		return_alloc = nullptr;

		llvm_value_map[&method_decl] = llvm_function;
	}

	void LLVMVisitor::Visit(VarDecl const& var_decl, uint32)
	{
		QualType const& var_type = var_decl.GetType();
		llvm::Type* llvm_type = ConvertToLLVMType(var_type);
		bool const is_array = var_type->Is(TypeKind::Array);

		if (var_decl.IsGlobal())
		{
			if (var_decl.IsExtern())
			{
				llvm::GlobalVariable* global_var = new llvm::GlobalVariable(module, llvm_type, var_type.IsConst(), llvm::GlobalValue::ExternalLinkage, nullptr, var_decl.GetName());
				llvm_value_map[&var_decl] = global_var;
			}
			else if(Expr const* init_expr = var_decl.GetInitExpr())
			{
				if (is_array)
				{
					ArrayType const& array_type = type_cast<ArrayType>(var_type);
					llvm::Type* llvm_element_type = ConvertToLLVMType(array_type.GetBaseType());

					if (InitializerListExpr const* init_list_expr = dynamic_ast_cast<InitializerListExpr>(init_expr))
					{
						WAVE_ASSERT(init_list_expr->IsConstexpr());
						init_list_expr->Accept(*this);
						
						llvm::GlobalValue::LinkageTypes linkage = var_decl.IsPublic() || var_decl.IsExtern() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
						llvm::GlobalVariable* global_array = new llvm::GlobalVariable(module, llvm_type, var_type.IsConst(), linkage, cast<llvm::Constant>(llvm_value_map[init_list_expr]), var_decl.GetName());
						llvm_value_map[&var_decl] = global_array;
					}
					else if (ConstantString const* string = dynamic_ast_cast<ConstantString>(init_expr))
					{
						llvm::Constant* constant = llvm::ConstantDataArray::getString(context, string->GetString());

						llvm::GlobalValue::LinkageTypes linkage = var_decl.IsPublic() || var_decl.IsExtern() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
						llvm::GlobalVariable* global_string = new llvm::GlobalVariable(module, llvm_type, var_type.IsConst(), linkage, constant, var_decl.GetName());
						llvm_value_map[&var_decl] = global_string;
					}
					else WAVE_ASSERT(false);
				}
				else
				{
					WAVE_ASSERT(init_expr->IsConstexpr());
					init_expr->Accept(*this);
					llvm::Value* init_value = llvm_value_map[init_expr];
					llvm::Constant* constant_init_value = llvm::dyn_cast<llvm::Constant>(init_value);
					WAVE_ASSERT(constant_init_value);

					llvm::GlobalValue::LinkageTypes linkage = var_decl.IsPublic() || var_decl.IsExtern() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
					llvm::GlobalVariable* global_var = new llvm::GlobalVariable(module, llvm_type, var_type.IsConst(), linkage, constant_init_value, var_decl.GetName());
					llvm_value_map[&var_decl] = global_var;
				}
			}
			else if (IsClassType(var_type))
			{
				ClassType const& class_type = type_cast<ClassType>(var_type);
				ClassDecl const* class_decl = class_type.GetClassDecl();

				UniqueFieldDeclPtrList const& fields = class_decl->GetFields();
				std::vector<llvm::Constant*> initializers; initializers.reserve(fields.size());
				for (uint64 i = 0; i < fields.size(); ++i)
				{
					fields[i]->Accept(*this);
					initializers.push_back(cast<llvm::Constant>(llvm_value_map[fields[i].get()]));
				}

				llvm::StructType* llvm_struct_type = cast<llvm::StructType>(llvm_type);
				llvm::GlobalVariable* global_var = new llvm::GlobalVariable(module, llvm_type, false, llvm::GlobalValue::ExternalLinkage, 
																		 llvm::ConstantStruct::get(llvm_struct_type, initializers), var_decl.GetName());
				llvm_value_map[&var_decl] = global_var;
			}
			else
			{
				llvm::Constant* constant_init_value = llvm::Constant::getNullValue(llvm_type);
				llvm::GlobalValue::LinkageTypes linkage = var_decl.IsPublic() || var_decl.IsExtern() ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;
				llvm::GlobalVariable* global_var = new llvm::GlobalVariable(module, llvm_type, var_type.IsConst(), linkage, constant_init_value, var_decl.GetName());
				llvm_value_map[&var_decl] = global_var;
			}
		}
		else
		{
			if (Expr const* init_expr = var_decl.GetInitExpr())
			{
				init_expr->Accept(*this);
				if (is_array)
				{
					ArrayType const& array_type = type_cast<ArrayType>(var_type);
					llvm::Type* llvm_element_type = ConvertToLLVMType(array_type.GetBaseType());
					llvm::ConstantInt* zero = llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
					if (InitializerListExpr const* init_list_expr = dynamic_ast_cast<InitializerListExpr>(init_expr))
					{
						llvm::AllocaInst* alloc = builder.CreateAlloca(llvm_type, nullptr);
						UniqueExprPtrList const& init_list = init_list_expr->GetInitList();

						for (uint64 i = 0; i < init_list.size(); ++i)
						{
							llvm::ConstantInt* index = llvm::ConstantInt::get(context, llvm::APInt(64, i, true));
							llvm::Value* ptr = builder.CreateGEP(llvm_type, alloc, { zero, index });
							Store(llvm_value_map[init_list[i].get()], ptr);
						}
						for (uint64 i = init_list.size(); i < array_type.GetArraySize(); ++i)
						{
							llvm::ConstantInt* index = llvm::ConstantInt::get(context, llvm::APInt(64, i, true));
							llvm::Value* ptr = builder.CreateGEP(llvm_type, alloc, { zero, index });
							Store(llvm::Constant::getNullValue(llvm_element_type), ptr);
						}
						llvm_value_map[&var_decl] = alloc;
					}
					else if (ConstantString const* string = dynamic_ast_cast<ConstantString>(init_expr))
					{
						llvm::AllocaInst* alloc = builder.CreateAlloca(llvm_type, nullptr);
						std::string_view str = string->GetString();
						for (uint64 i = 0; i < str.size(); ++i)
						{
							llvm::ConstantInt* index = llvm::ConstantInt::get(context, llvm::APInt(64, i, true));
							llvm::Value* ptr = builder.CreateGEP(llvm_type, alloc, { zero, index });
							Store(llvm::ConstantInt::get(char_type, str[i], true), ptr);
						}
						llvm::ConstantInt* index = llvm::ConstantInt::get(context, llvm::APInt(64, str.size(), true));
						llvm::Value* ptr = builder.CreateGEP(llvm_type, alloc, { zero, index });
						Store(llvm::ConstantInt::get(char_type, '\0', true), ptr);
						llvm_value_map[&var_decl] = alloc;
					}
					else if (init_expr->GetExprKind() == ExprKind::DeclRef || init_expr->GetExprKind() == ExprKind::ArrayAccess)
					{
						WAVE_ASSERT(IsArrayType(init_expr->GetType()));
						llvm::Type* init_expr_type = ConvertToLLVMType(init_expr->GetType());
						llvm::AllocaInst* alloc = builder.CreateAlloca(llvm::PointerType::get(llvm_element_type, 0), nullptr);
						llvm::Value* ptr = nullptr;
						if (init_expr_type->isArrayTy()) ptr = builder.CreateInBoundsGEP(init_expr_type, llvm_value_map[init_expr], { zero, zero });
						else ptr = builder.CreateLoad(init_expr_type, llvm_value_map[init_expr]);
						builder.CreateStore(ptr, alloc);
						llvm_value_map[&var_decl] = alloc;
					}
					else WAVE_ASSERT(false);
				}
				else
				{
					llvm::AllocaInst* alloc = builder.CreateAlloca(llvm_type, nullptr);
					llvm::Value* init_value = llvm_value_map[var_decl.GetInitExpr()];
					Store(init_value, alloc);
					llvm_value_map[&var_decl] = alloc;
				}
			}
			else if (IsClassType(var_type))
			{
				QualType const& var_type = var_decl.GetType();
				ClassType const& class_type = type_cast<ClassType>(var_type);
				ClassDecl const* class_decl = class_type.GetClassDecl();

				llvm::AllocaInst* struct_alloc = builder.CreateAlloca(llvm_type, nullptr);
				llvm_value_map[&var_decl] = struct_alloc;

				UniqueFieldDeclPtrList const& fields = class_decl->GetFields();
				for (uint64 i = 0; i < fields.size(); ++i)
				{
					fields[i]->Accept(*this);
					llvm::Value* field_ptr = builder.CreateStructGEP(llvm_type, struct_alloc, i);
					Store(llvm_value_map[fields[i].get()], field_ptr);
				}
			}
			
		}
	}

	void LLVMVisitor::Visit(FieldDecl const& member_var, uint32)
	{
		QualType const& var_type = member_var.GetType();
		llvm::Type* llvm_type = ConvertToLLVMType(var_type);

		if (Expr const* init_expr = member_var.GetInitExpr())
		{
			init_expr->Accept(*this);
			llvm::Value* init_value = llvm_value_map[init_expr];
			WAVE_ASSERT(isa<llvm::Constant>(init_value));
			llvm_value_map[&member_var] = cast<llvm::Constant>(init_value);
		}
		else
		{
			llvm_value_map[&member_var] = llvm::Constant::getNullValue(llvm_type);
		}
	}

	void LLVMVisitor::Visit(ParamVarDecl const& param_var, uint32)
	{
	}

	void LLVMVisitor::Visit(TagDecl const&, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(EnumDecl const& enum_decl, uint32)
	{
		for (auto const& enum_member : enum_decl.GetEnumMembers()) enum_member->Accept(*this);
	}

	void LLVMVisitor::Visit(EnumMemberDecl const& enum_member, uint32)
	{
		llvm::ConstantInt* constant = llvm::ConstantInt::get(int_type, enum_member.GetValue());
		llvm_value_map[&enum_member] = constant;
	}

	void LLVMVisitor::Visit(AliasDecl const&, uint32)
	{
		//alias declaration doesn't generate any code
	}

	void LLVMVisitor::Visit(ClassDecl const& class_decl, uint32)
	{
		for (auto& method_decl : class_decl.GetMethods()) method_decl->Accept(*this);
	}

	void LLVMVisitor::Visit(Stmt const& stmt, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(CompoundStmt const& compound_stmt, uint32)
	{
		for (auto const& stmt : compound_stmt.GetStmts()) stmt->Accept(*this);
	}

	void LLVMVisitor::Visit(DeclStmt const& decl_stmt, uint32)
	{
		for(auto const& decl : decl_stmt.GetDecls())  decl->Accept(*this);
	}

	void LLVMVisitor::Visit(ExprStmt const& expr_stmt, uint32)
	{
		if (expr_stmt.GetExpr()) expr_stmt.GetExpr()->Accept(*this);
	}

	void LLVMVisitor::Visit(NullStmt const& null_stmt, uint32) {}

	void LLVMVisitor::Visit(ReturnStmt const& return_stmt, uint32)
	{
		if (ExprStmt const* expr_stmt = return_stmt.GetExprStmt()) 
		{
			expr_stmt->GetExpr()->Accept(*this);
			llvm::Value* return_expr_value = llvm_value_map[expr_stmt->GetExpr()];
			WAVE_ASSERT(return_expr_value);
			llvm_value_map[&return_stmt] = Store(return_expr_value, return_alloc);
		}
		else 
		{
			llvm_value_map[&return_stmt] = nullptr;
		}
		builder.CreateBr(exit_block);

		llvm::BasicBlock* currentBlock = builder.GetInsertBlock();
		llvm::Function* currentFunction = currentBlock->getParent();
		llvm::BasicBlock* returnBlock = llvm::BasicBlock::Create(context, "return", currentFunction, currentBlock->getNextNode());
		builder.SetInsertPoint(returnBlock);
	}

	void LLVMVisitor::Visit(IfStmt const& if_stmt, uint32)
	{
		Expr const* cond_expr = if_stmt.GetCondExpr();
		Stmt const* then_stmt = if_stmt.GetThenStmt();
		Stmt const* else_stmt = if_stmt.GetElseStmt();

		llvm::Function* function = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context, "if.then", function, exit_block);
		llvm::BasicBlock* else_block = llvm::BasicBlock::Create(context, "if.else", function, exit_block);
		llvm::BasicBlock* end_block  = llvm::BasicBlock::Create(context, "if.end", function, exit_block);

		cond_expr->Accept(*this);
		llvm::Value* condition_value = llvm_value_map[cond_expr];
		WAVE_ASSERT(condition_value);
		ConditionalBranch(condition_value, then_block, else_stmt ? else_block : end_block);

		builder.SetInsertPoint(then_block);
		then_stmt->Accept(*this);
		if(!then_block->getTerminator()) builder.CreateBr(end_block);

		if (else_stmt)
		{
			builder.SetInsertPoint(else_block);
			else_stmt->Accept(*this);
			if (!else_block->getTerminator()) builder.CreateBr(end_block);
		}
		builder.SetInsertPoint(end_block);
	}

	void LLVMVisitor::Visit(BreakStmt const&, uint32)
	{
		WAVE_ASSERT(!break_blocks.empty());
		builder.CreateBr(break_blocks.back());
		llvm::BasicBlock* break_block = llvm::BasicBlock::Create(context, "break", builder.GetInsertBlock()->getParent(), exit_block);
		builder.SetInsertPoint(break_block);
	}

	void LLVMVisitor::Visit(ContinueStmt const&, uint32)
	{
		WAVE_ASSERT(!continue_blocks.empty());
		builder.CreateBr(continue_blocks.back());
		llvm::BasicBlock* continue_block = llvm::BasicBlock::Create(context, "continue", builder.GetInsertBlock()->getParent(), exit_block);
		builder.SetInsertPoint(continue_block);
	}

	void LLVMVisitor::Visit(ForStmt const& for_stmt, uint32)
	{
		Stmt const* init_stmt = for_stmt.GetInitStmt();
		Expr const* cond_expr = for_stmt.GetCondExpr();
		Expr const* iter_expr = for_stmt.GetIterExpr();
		Stmt const* body_stmt = for_stmt.GetBodyStmt();

		llvm::Function* function = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "for.body", function, exit_block);
		llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "for.cond", function, exit_block);
		llvm::BasicBlock* iter_block = llvm::BasicBlock::Create(context, "for.iter", function, exit_block);
		llvm::BasicBlock* end_block  = llvm::BasicBlock::Create(context, "for.end", function, exit_block);


		if (init_stmt) init_stmt->Accept(*this);
		builder.CreateBr(cond_block);

		builder.SetInsertPoint(cond_block);
		if (cond_expr)
		{
			cond_expr->Accept(*this);
			llvm::Value* condition_value = llvm_value_map[cond_expr];
			WAVE_ASSERT(condition_value);
			ConditionalBranch(condition_value, body_block, end_block);
		}
		else
		{
			builder.CreateBr(body_block);
		}

		builder.SetInsertPoint(body_block);

		continue_blocks.push_back(iter_block);
		break_blocks.push_back(end_block);
		body_stmt->Accept(*this);
		break_blocks.pop_back();
		continue_blocks.pop_back();

		builder.CreateBr(iter_block);

		builder.SetInsertPoint(iter_block);
		if (iter_expr) iter_expr->Accept(*this);
		builder.CreateBr(cond_block);

		builder.SetInsertPoint(end_block);
	}

	void LLVMVisitor::Visit(WhileStmt const& while_stmt, uint32)
	{
		Expr const* cond_expr = while_stmt.GetCondExpr();
		Stmt const* body_stmt = while_stmt.GetBodyStmt();

		llvm::Function* function	 = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "while.cond", function, exit_block);
		llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "while.body", function, exit_block);
		llvm::BasicBlock* end_block  = llvm::BasicBlock::Create(context, "while.end",  function, exit_block);

		builder.CreateBr(cond_block);
		builder.SetInsertPoint(cond_block);
		cond_expr->Accept(*this);
		llvm::Value* condition_value = llvm_value_map[cond_expr];
		WAVE_ASSERT(condition_value);
		ConditionalBranch(condition_value, body_block, end_block);

		builder.SetInsertPoint(body_block);

		continue_blocks.push_back(cond_block);
		break_blocks.push_back(end_block);
		body_stmt->Accept(*this);
		break_blocks.pop_back();
		continue_blocks.pop_back();

		builder.CreateBr(cond_block);

		builder.SetInsertPoint(end_block);
	}

	void LLVMVisitor::Visit(DoWhileStmt const& do_while_stmt, uint32)
	{
		Expr const* cond_expr = do_while_stmt.GetCondExpr();
		Stmt const* body_stmt = do_while_stmt.GetBodyStmt();

		llvm::Function* function = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "dowhile.body", function, exit_block);
		llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "dowhile.cond", function, exit_block);
		llvm::BasicBlock* end_block  = llvm::BasicBlock::Create(context, "dowhile.end", function, exit_block);

		builder.CreateBr(body_block);
		builder.SetInsertPoint(body_block);

		continue_blocks.push_back(cond_block);
		break_blocks.push_back(end_block);
		body_stmt->Accept(*this);
		break_blocks.pop_back();
		continue_blocks.pop_back();

		builder.CreateBr(cond_block);

		builder.SetInsertPoint(cond_block);
		cond_expr->Accept(*this);
		llvm::Value* condition_value = llvm_value_map[cond_expr];
		WAVE_ASSERT(condition_value);
		ConditionalBranch(condition_value, body_block, end_block);

		builder.SetInsertPoint(end_block);
	}

	void LLVMVisitor::Visit(CaseStmt const& case_stmt, uint32)
	{
		WAVE_ASSERT(!switch_instructions.empty());
		llvm::SwitchInst* switch_inst = switch_instructions.back();
		if (case_stmt.IsDefault())
		{
			builder.SetInsertPoint(switch_inst->getDefaultDest());
		}
		else
		{
			int64 case_value = case_stmt.GetValue();
			llvm::ConstantInt* llvm_case_value = llvm::ConstantInt::get(int_type, case_value);

			llvm::Function* function = builder.GetInsertBlock()->getParent();
			std::string block_name = "switch.case"; block_name += std::to_string(case_value);
			llvm::BasicBlock* case_block = llvm::BasicBlock::Create(context, block_name, function, exit_block);
			switch_inst->addCase(llvm_case_value, case_block);
			builder.SetInsertPoint(case_block);
		}
	}

	void LLVMVisitor::Visit(SwitchStmt const& switch_stmt, uint32)
	{
		Expr const* cond_expr = switch_stmt.GetCondExpr();
		Stmt const* body_stmt = switch_stmt.GetBodyStmt();

		llvm::Function* function = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* header_block = llvm::BasicBlock::Create(context, "switch.header", function, exit_block);
		llvm::BasicBlock* default_block = llvm::BasicBlock::Create(context, "switch.default", function, exit_block);
		llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "switch.end", function, exit_block);

		builder.CreateBr(header_block);
		builder.SetInsertPoint(header_block);

		cond_expr->Accept(*this);
		llvm::Value* condition_value = llvm_value_map[cond_expr];
		WAVE_ASSERT(condition_value);
		llvm::Value* condition = Load(int_type, condition_value);
		llvm::SwitchInst* switch_inst = builder.CreateSwitch(condition, default_block);

		switch_instructions.push_back(switch_inst);
		break_blocks.push_back(end_block);
		body_stmt->Accept(*this);
		break_blocks.pop_back();
		switch_instructions.pop_back();

		std::vector<llvm::BasicBlock*> case_blocks;
		for (auto& case_stmt : switch_inst->cases()) case_blocks.push_back(case_stmt.getCaseSuccessor());
		for (uint32 i = 0; i < case_blocks.size(); ++i)
		{
			llvm::BasicBlock* case_block = case_blocks[i];
			if (!case_block->getTerminator())
			{
				llvm::BasicBlock* dest_block = i < case_blocks.size() - 1 ? case_blocks[i + 1] : default_block;
				builder.SetInsertPoint(case_block);
				builder.CreateBr(dest_block);
			}
		}
		builder.SetInsertPoint(end_block);
	}

	void LLVMVisitor::Visit(GotoStmt const& goto_stmt, uint32)
	{
		std::string label_name = "label."; label_name += goto_stmt.GetLabelName();
		builder.CreateBr(label_blocks[label_name]);

		llvm::Function* function = builder.GetInsertBlock()->getParent();
		llvm::BasicBlock* goto_block = llvm::BasicBlock::Create(context, "goto", function, exit_block);
		builder.SetInsertPoint(goto_block);
	}

	void LLVMVisitor::Visit(LabelStmt const& label_stmt, uint32)
	{
		std::string block_name = "label."; block_name += label_stmt.GetName();
		llvm::BasicBlock* label_block = label_blocks[block_name];
		builder.CreateBr(label_block);
		builder.SetInsertPoint(label_block);
	}

	void LLVMVisitor::Visit(Expr const&, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(UnaryExpr const& unary_expr, uint32)
	{
		Expr const* operand_expr = unary_expr.GetOperand();
		operand_expr->Accept(*this);
		llvm::Value* operand_value = llvm_value_map[operand_expr];
		WAVE_ASSERT(operand_value);
		llvm::Value* operand = Load(operand_expr->GetType(), operand_value);

		llvm::Value* result = nullptr;
		switch (unary_expr.GetUnaryKind())
		{
		case UnaryExprKind::PreIncrement:
		{
			llvm::Value* incremented_value = builder.CreateAdd(operand, llvm::ConstantInt::get(operand->getType(), 1));
			Store(incremented_value, operand_value);
			result = incremented_value;
		}
		break;
		case UnaryExprKind::PreDecrement:
		{
			llvm::Value* decremented_value = builder.CreateSub(operand, llvm::ConstantInt::get(operand->getType(), 1));
			Store(decremented_value, operand_value);
			result = decremented_value;
		}
		break;
		case UnaryExprKind::PostIncrement:
		{
			result = builder.CreateAlloca(operand_value->getType());
			Store(operand_value, result);
			llvm::Value* incremented_value = builder.CreateAdd(operand, llvm::ConstantInt::get(operand->getType(), 1));
			Store(incremented_value, operand_value);
		}
		break;
		case UnaryExprKind::PostDecrement:
		{
			result = builder.CreateAlloca(operand_value->getType());
			Store(operand_value, result);
			llvm::Value* decremented_value = builder.CreateSub(operand, llvm::ConstantInt::get(operand->getType(), 1));
			Store(decremented_value, operand_value);
		}
		break;
		case UnaryExprKind::Plus:
		{
			result = operand_value;
		}
		break;
		case UnaryExprKind::Minus:
		{
			result = builder.CreateNeg(operand);
		}
		break;
		case UnaryExprKind::BitNot:
		{
			result = builder.CreateNot(operand);
		}
		break;
		case UnaryExprKind::LogicalNot:
		{
			result = builder.CreateICmpEQ(operand, llvm::ConstantInt::get(operand->getType(), 0));
		}
		break;
		default:
			WAVE_ASSERT(false);
		}
		WAVE_ASSERT(result);
		llvm_value_map[&unary_expr] = result;
	}

	void LLVMVisitor::Visit(BinaryExpr const& binary_expr, uint32)
	{
		Expr const* lhs_expr = binary_expr.GetLHS();
		lhs_expr->Accept(*this);
		llvm::Value* lhs_value = llvm_value_map[lhs_expr];
		Expr const* rhs_expr = binary_expr.GetRHS();
		rhs_expr->Accept(*this);
		llvm::Value* rhs_value = llvm_value_map[rhs_expr];
		WAVE_ASSERT(lhs_value && rhs_value);

		llvm::Value* lhs = Load(lhs_expr->GetType(), lhs_value);
		llvm::Value* rhs = Load(rhs_expr->GetType(), rhs_value);
		bool const is_float_expr = IsFloat(lhs->getType()) || IsFloat(rhs->getType());


		llvm::Value* result = nullptr;
		switch (binary_expr.GetBinaryKind())
		{
		case BinaryExprKind::Assign:
		{
			result = Store(rhs_value, lhs_value);
		}
		break;
		case BinaryExprKind::Add:
		{

			result = is_float_expr ? builder.CreateFAdd(lhs, rhs) : builder.CreateAdd(lhs, rhs);
		}
		break;
		case BinaryExprKind::Subtract:
		{
			result = is_float_expr ? builder.CreateFSub(lhs, rhs) : builder.CreateSub(lhs, rhs);
		}
		break;
		case BinaryExprKind::Multiply:
		{
			result = is_float_expr ? builder.CreateFMul(lhs, rhs) : builder.CreateMul(lhs, rhs);
		}
		break;
		case BinaryExprKind::Divide:
		{
			result = is_float_expr ? builder.CreateFDiv(lhs, rhs) : builder.CreateSDiv(lhs, rhs);
		}
		break;
		case BinaryExprKind::Modulo:
		{
			WAVE_ASSERT(!is_float_expr);
			result = builder.CreateSRem(lhs, rhs);
		}
		break;
		case BinaryExprKind::ShiftLeft:
		{
			result = builder.CreateShl(lhs, rhs);
		}
		break;
		case BinaryExprKind::ShiftRight:
		{
			result = builder.CreateAShr(lhs, rhs);
		}
		break;
		case BinaryExprKind::BitAnd:
		{
			result = builder.CreateAnd(lhs, rhs);
		}
		break;
		case BinaryExprKind::BitOr:
		{
			result = builder.CreateOr(lhs, rhs);
		}
		break;
		case BinaryExprKind::BitXor:
		{
			result = builder.CreateXor(lhs, rhs);
		}
		break;
		case BinaryExprKind::LogicalAnd:
		{
			llvm::Value* tmp = builder.CreateAnd(lhs, rhs);
			result = builder.CreateICmpNE(tmp, llvm::ConstantInt::get(tmp->getType(), 0));
		}
		break;
		case BinaryExprKind::LogicalOr:
		{
			llvm::Value* tmp = builder.CreateOr(lhs, rhs);
			result = builder.CreateICmpNE(tmp, llvm::ConstantInt::get(tmp->getType(), 0));
		}
		break;
		case BinaryExprKind::Equal:
		{
			result = is_float_expr ? builder.CreateFCmpOEQ(lhs, rhs) : builder.CreateICmpEQ(lhs, rhs);
		}
		break;
		case BinaryExprKind::NotEqual:
		{
			result = is_float_expr ? builder.CreateFCmpONE(lhs, rhs) : builder.CreateICmpNE(lhs, rhs);
		}
		break;
		case BinaryExprKind::Less:
		{
			result = is_float_expr ? builder.CreateFCmpOLT(lhs, rhs) : builder.CreateICmpSLT(lhs, rhs);
		}
		break;
		case BinaryExprKind::Greater:
		{
			result = is_float_expr ? builder.CreateFCmpOGT(lhs, rhs) : builder.CreateICmpSGT(lhs, rhs);
		}
		break;
		case BinaryExprKind::LessEqual:
		{
			result = is_float_expr ? builder.CreateFCmpOLE(lhs, rhs) : builder.CreateICmpSLE(lhs, rhs);
		}
		break;
		case BinaryExprKind::GreaterEqual:
		{
			result = is_float_expr ? builder.CreateFCmpOGE(lhs, rhs) : builder.CreateICmpSGE(lhs, rhs);
		}
		break;
		case BinaryExprKind::Comma:
		{
			result = rhs;
		}
		break;
		case BinaryExprKind::Invalid:
		default:
			WAVE_ASSERT(false);
		}
		WAVE_ASSERT(result);
		llvm_value_map[&binary_expr] = result;
	}

	void LLVMVisitor::Visit(TernaryExpr const& ternary_expr, uint32)
	{
		Expr const* cond_expr = ternary_expr.GetCondExpr();
		Expr const* true_expr = ternary_expr.GetTrueExpr();
		Expr const* false_expr = ternary_expr.GetFalseExpr();

		cond_expr->Accept(*this);
		llvm::Value* condition_value = llvm_value_map[cond_expr];
		WAVE_ASSERT(condition_value);
		condition_value = Load(bool_type, condition_value);

		true_expr->Accept(*this);
		llvm::Value* true_value = llvm_value_map[true_expr];
		WAVE_ASSERT(true_value);
		true_value = Load(true_expr->GetType(), true_value);

		false_expr->Accept(*this);
		llvm::Value* false_value = llvm_value_map[false_expr];
		WAVE_ASSERT(false_value);
		false_value = Load(false_expr->GetType(), false_value);

		if (condition_value->getType() != llvm::Type::getInt1Ty(context)) WAVE_ASSERT(false);

		llvm_value_map[&ternary_expr] = builder.CreateSelect(condition_value, true_value, false_value);
	}

	void LLVMVisitor::Visit(IdentifierExpr const&, uint32)
	{
		WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(DeclRefExpr const& decl_ref, uint32)
	{
		llvm::Value* value = llvm_value_map[decl_ref.GetDecl()];
		WAVE_ASSERT(value);
		llvm_value_map[&decl_ref] = value;
	}

	void LLVMVisitor::Visit(ConstantInt const& int_constant, uint32)
	{
		llvm::ConstantInt* constant = llvm::ConstantInt::get(int_type, int_constant.GetValue());
		llvm_value_map[&int_constant] = constant;
	}

	void LLVMVisitor::Visit(ConstantChar const& char_constant, uint32)
	{
		llvm::ConstantInt* constant = llvm::ConstantInt::get(char_type, char_constant.GetChar(), true);
		llvm_value_map[&char_constant] = constant;
	}

	void LLVMVisitor::Visit(ConstantString const& string_constant, uint32)
	{
		llvm::Constant* constant = llvm::ConstantDataArray::getString(context, string_constant.GetString());
		
		static uint32 counter = 0;
		std::string name = "__StringLiteral"; name += std::to_string(counter++);

		llvm::GlobalValue::LinkageTypes linkage = llvm::Function::InternalLinkage;
		llvm::GlobalVariable* global_string = new llvm::GlobalVariable(module, ConvertToLLVMType(string_constant.GetType()), true, linkage, constant, name);
		llvm_value_map[&string_constant] = global_string;
	}

	void LLVMVisitor::Visit(ConstantBool const& bool_constant, uint32)
	{
		llvm::ConstantInt* constant = llvm::ConstantInt::get(bool_type, bool_constant.GetValue());
		llvm_value_map[&bool_constant] = constant;
	}

	void LLVMVisitor::Visit(ConstantFloat const& float_constant, uint32)
	{
		llvm::Constant* constant = llvm::ConstantFP::get(float_type, float_constant.GetValue());
		llvm_value_map[&float_constant] = constant;
	}

	void LLVMVisitor::Visit(ImplicitCastExpr const& cast_expr, uint32)
	{
		Expr const* cast_operand_expr = cast_expr.GetOperand();
		cast_operand_expr->Accept(*this);
		llvm::Value* cast_operand_value = llvm_value_map[cast_operand_expr];
		WAVE_ASSERT(cast_operand_value);

		llvm::Type* cast_type = ConvertToLLVMType(cast_expr.GetType());
		llvm::Type* cast_operand_type = ConvertToLLVMType(cast_operand_expr->GetType());

		llvm::Value* cast_operand = Load(cast_operand_type, cast_operand_value);
		if (IsInteger(cast_type))
		{
			if (IsBoolean(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateZExt(cast_operand, int_type);
			}
			else if (IsFloat(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateFPToSI(cast_operand, int_type);
			}
			else WAVE_ASSERT(false);
		}
		else if (IsBoolean(cast_type))
		{
			if (IsInteger(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateICmpNE(cast_operand, llvm::ConstantInt::get(context, llvm::APInt(64, 0)));
			}
			else if (IsFloat(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateFPToUI(cast_operand, bool_type);
			}
			else WAVE_ASSERT(false);
		}
		else if (IsFloat(cast_type))
		{
			if (IsBoolean(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateUIToFP(cast_operand, float_type);
			}
			else if (IsInteger(cast_operand_type))
			{
				llvm_value_map[&cast_expr] = builder.CreateSIToFP(cast_operand, float_type);
			}
			else WAVE_ASSERT(false);
		}
		WAVE_ASSERT(llvm_value_map[&cast_expr] != nullptr);
	}

	void LLVMVisitor::Visit(CallExpr const& call_expr, uint32)
	{
		llvm::Function* called_function = module.getFunction(call_expr.GetFunctionName());
		WAVE_ASSERT(called_function);

		std::vector<llvm::Value*> args;
		uint32 arg_index = 0;
		for (auto const& arg_expr : call_expr.GetArgs())
		{
			arg_expr->Accept(*this);
			llvm::Value* arg_value = llvm_value_map[arg_expr.get()]; 
			WAVE_ASSERT(arg_value);
			args.push_back(Load(called_function->getArg(arg_index++)->getType(), arg_value));
		}

		llvm::Value* call_result = builder.CreateCall(called_function, args);
		llvm_value_map[&call_expr] = call_result;
	}

	void LLVMVisitor::Visit(InitializerListExpr const& initializer_list, uint32)
	{
		UniqueExprPtrList const& init_expr_list = initializer_list.GetInitList();
		for (auto const& element_expr : init_expr_list) element_expr->Accept(*this);
		if (initializer_list.IsConstexpr())
		{
			ArrayType const& array_type = type_cast<ArrayType>(initializer_list.GetType());
			llvm::Type* llvm_element_type = ConvertToLLVMType(array_type.GetBaseType());
			llvm::Type* llvm_array_type = ConvertToLLVMType(array_type);

			std::vector<llvm::Constant*> array_init_list(array_type.GetArraySize());
			for (uint64 i = 0; i < array_type.GetArraySize(); ++i)
			{
				if (i < init_expr_list.size())  array_init_list[i] = llvm::dyn_cast<llvm::Constant>(llvm_value_map[init_expr_list[i].get()]);
				else array_init_list[i] = llvm::Constant::getNullValue(llvm_element_type);
			}
			llvm::Constant* constant_array = llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(llvm_array_type), array_init_list);
			llvm_value_map[&initializer_list] = constant_array;
		}
		
	}

	void LLVMVisitor::Visit(ArrayAccessExpr const& array_access, uint32)
	{
		Expr const* array_expr = array_access.GetArrayExpr();
		Expr const* index_expr = array_access.GetIndexExpr();

		array_expr->Accept(*this);
		index_expr->Accept(*this);

		llvm::Value* array_value = llvm_value_map[array_expr];
		llvm::Value* index_value = llvm_value_map[index_expr];
		index_value = Load(int_type, index_value);

		llvm::ConstantInt* zero = llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
		if (llvm::AllocaInst* alloc = dyn_cast<llvm::AllocaInst>(array_value))
		{
			llvm::Type* alloc_type = alloc->getAllocatedType();
			if (alloc_type->isArrayTy())
			{
				llvm::Value* ptr = builder.CreateGEP(alloc_type, alloc, { zero, index_value });
				llvm_value_map[&array_access] = ptr;
			}
			else if (alloc_type->isPointerTy())
			{
				llvm::Value* ptr = builder.CreateInBoundsGEP(alloc_type, Load(alloc_type, alloc), index_value);
				llvm_value_map[&array_access] = ptr;
			}
			else WAVE_ASSERT(false);
		}
		else 
		{
			QualType const& array_expr_type = array_expr->GetType();
			WAVE_ASSERT(IsArrayType(array_expr_type));
			ArrayType const& array_type = type_cast<ArrayType>(array_expr_type);
			if (IsArrayType(array_type.GetBaseType()))
			{
				uint32 array_size = array_type.GetArraySize();
				index_value = builder.CreateMul(index_value, llvm::ConstantInt::get(int_type, array_size));
			}
			llvm::Value* ptr = builder.CreateInBoundsGEP(array_value->getType(), array_value, index_value);
			llvm_value_map[&array_access] = ptr;
		}
	}

	void LLVMVisitor::Visit(MemberExpr const& member_expr, uint32)
	{
		Expr const* class_expr = member_expr.GetClassExpr();
		Decl const* member_decl = member_expr.GetMemberDecl();
		class_expr->Accept(*this);
		if (member_decl->GetDeclKind() == DeclKind::Field)
		{
			FieldDecl const* field_decl = ast_cast<FieldDecl>(member_decl);
			llvm::Value* field_value = builder.CreateStructGEP(this_struct_type, llvm_value_map[class_expr], field_decl->GetFieldIndex());
			llvm_value_map[&member_expr] = field_value;
		}
		else if (member_decl->GetDeclKind() == DeclKind::Method)
		{
			MethodDecl const* field_decl = ast_cast<MethodDecl>(member_decl);
			WAVE_ASSERT(false);
		}
		else WAVE_ASSERT(false);
	}

	void LLVMVisitor::Visit(MemberCallExpr const& method_call, uint32)
	{
		Expr const* expr = method_call.GetMemberExpr();
		WAVE_ASSERT(expr->GetExprKind() == ExprKind::Member);
		MemberExpr const* member_expr = ast_cast<MemberExpr>(expr);

		Decl const* decl = member_expr->GetMemberDecl();
		WAVE_ASSERT(decl->GetDeclKind() == DeclKind::Method);
		MethodDecl const* method_decl = ast_cast<MethodDecl>(decl);
		ClassDecl const* class_decl = method_decl->GetParentDecl();
		std::string name(class_decl->GetName());
		name += "::";
		name += method_call.GetFunctionName();

		llvm::Function* called_function = module.getFunction(name);
		WAVE_ASSERT(called_function);

		std::vector<llvm::Value*> args;
		uint32 arg_index = 0;

		member_expr->GetClassExpr()->Accept(*this);
		llvm::Value* this_value = llvm_value_map[member_expr->GetClassExpr()];
		args.push_back(this_value);
		for (auto const& arg_expr : method_call.GetArgs())
		{
			arg_expr->Accept(*this);
			llvm::Value* arg_value = llvm_value_map[arg_expr.get()];
			WAVE_ASSERT(arg_value);
			args.push_back(Load(called_function->getArg(arg_index++)->getType(), arg_value));
		}

		llvm::Value* call_result = builder.CreateCall(called_function, args);
		llvm_value_map[&method_call] = call_result;
	}

	void LLVMVisitor::Visit(ThisExpr const& this_expr, uint32)
	{
		llvm_value_map[&this_expr] = this_value;
	}

	void LLVMVisitor::ConditionalBranch(llvm::Value* condition_value, llvm::BasicBlock* true_block, llvm::BasicBlock* false_block)
	{
		if (IsBoolean(condition_value->getType()))
		{
			builder.CreateCondBr(condition_value, true_block, false_block);
		}
		else if (IsInteger(condition_value->getType()))
		{
			llvm::Value* condition = Load(int_type, condition_value);
			llvm::Value* boolean_cond = builder.CreateICmpNE(condition, llvm::ConstantInt::get(context, llvm::APInt(64, 0)));
			builder.CreateCondBr(boolean_cond, true_block, false_block);
		}
		else if (IsFloat(condition_value->getType()))
		{
			llvm::Value* condition = Load(float_type, condition_value);
			llvm::Value* boolean_cond = builder.CreateFCmpONE(condition, llvm::ConstantFP::get(context, llvm::APFloat(0.0)));
			builder.CreateCondBr(boolean_cond, true_block, false_block);
		}
		else WAVE_ASSERT(false);
	}

	llvm::Type* LLVMVisitor::ConvertToLLVMType(QualType const& type)
	{
		switch (type->GetKind())
		{
		case TypeKind::Void:
			return void_type;
		case TypeKind::Bool:
			return bool_type;
		case TypeKind::Char:
			return char_type;
		case TypeKind::Int:
			return int_type;
		case TypeKind::Float:
			return float_type;
		case TypeKind::Array:
		{
			ArrayType const& array_type = type_cast<ArrayType>(type);
			if (array_type.GetArraySize() > 0) return llvm::ArrayType::get(ConvertToLLVMType(array_type.GetBaseType()), array_type.GetArraySize());
			else return llvm::PointerType::get(ConvertToLLVMType(array_type.GetBaseType()), 0);
		}
		case TypeKind::Function:
		{
			FunctionType const& function_type = type_cast<FunctionType>(type);
			std::span<FunctionParams const> function_params = function_type.GetParams();

			llvm::Type* return_type = ConvertToLLVMType(function_type.GetReturnType());
			std::vector<llvm::Type*> param_types; param_types.reserve(function_params.size());
			for (auto const& func_param : function_params)
			{
				param_types.push_back(ConvertToLLVMType(func_param.type));
			}
			return llvm::FunctionType::get(return_type, param_types, false);
		}
		case TypeKind::Class:
		{
			ClassType const& class_type = type_cast<ClassType>(type);
			ClassDecl const* class_decl = class_type.GetClassDecl();
			llvm::StructType* llvm_class_type = llvm::StructType::create(context, class_decl->GetName());

			UniqueFieldDeclPtrList const& member_variables = class_decl->GetFields();
			std::vector<llvm::Type*> llvm_member_types; llvm_member_types.reserve(member_variables.size());
			for (auto const& member_var : member_variables) llvm_member_types.push_back(ConvertToLLVMType(member_var->GetType()));
			llvm_class_type->setBody(llvm_member_types, false);

			return llvm_class_type;
		}
		default:
			WAVE_UNREACHABLE();
		}
		return nullptr;
	}

	llvm::Value* LLVMVisitor::Load(QualType const& type, llvm::Value* ptr)
	{
		llvm::Type* llvm_type = ConvertToLLVMType(type);
		return Load(llvm_type, ptr);
	}

	llvm::Value* LLVMVisitor::Load(llvm::Type* llvm_type, llvm::Value* ptr)
	{
		//#todo: refactor this
		if (ptr->getType()->isPointerTy())
		{
			if (isa<llvm::AllocaInst>(ptr))
			{
				llvm::AllocaInst* alloc = cast<llvm::AllocaInst>(ptr);
				if (alloc->getAllocatedType()->isArrayTy())
				{
					llvm::ConstantInt* zero = llvm::ConstantInt::get(context, llvm::APInt(64, 0, true));
					llvm::ArrayType* array_type = cast<llvm::ArrayType>(alloc->getAllocatedType());
					return builder.CreateInBoundsGEP(array_type, ptr, { zero, zero });
				}
			}
			else if (llvm_type->isPointerTy() && isa<llvm::GlobalVariable>(ptr))
			{
				return ptr;
			}
			return builder.CreateLoad(llvm_type, ptr);
		}
		return ptr;
	}

	llvm::Value* LLVMVisitor::Store(llvm::Value* value, llvm::Value* ptr)
	{
		if (!value->getType()->isPointerTy()) return builder.CreateStore(value, ptr);
		llvm::Value* load = Load(value->getType(), value);
		return builder.CreateStore(load, ptr);
	}

	bool LLVMVisitor::IsBoolean(llvm::Type* type)
	{
		return type->isIntegerTy() && type->getIntegerBitWidth() == 1;
	}

	bool LLVMVisitor::IsInteger(llvm::Type* type)
	{
		return type->isIntegerTy() && type->getIntegerBitWidth() == 64;
	}

	bool LLVMVisitor::IsFloat(llvm::Type* type)
	{
		return type->isDoubleTy();
	}
}


