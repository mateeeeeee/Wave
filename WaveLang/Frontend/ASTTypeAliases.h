#pragma once
#include <memory>
#include <vector>
#include "ASTForwardDeclarations.h"

namespace wave
{
	template<typename T>
	using UniquePtr = std::unique_ptr<T>;

	template<typename Type, typename... Args>
	inline UniquePtr<Type> MakeUnique(Args&&... args)
	{
		return std::make_unique<Type>(std::forward<Args>(args)...);
	}

	using UniqueTranslationUnitPtr	= UniquePtr<TranslationUnit>;

	using UniqueDeclPtr				= UniquePtr<Decl>;
	using UniqueFunctionDeclPtr		= UniquePtr<FunctionDecl>;
	using UniqueVariableDeclPtr		= UniquePtr<VariableDecl>;

	using UniqueStmtPtr				= UniquePtr<Stmt>;
	using UniqueCompoundStmtPtr		= UniquePtr<CompoundStmt>;
	using UniqueExprStmtPtr			= UniquePtr<ExprStmt>;
	using UniqueDeclStmtPtr			= UniquePtr<DeclStmt>;
	using UniqueNullStmtPtr			= UniquePtr<NullStmt>;
	using UniqueReturnStmtPtr		= UniquePtr<ReturnStmt>;
	using UniqueIfStmtPtr			= UniquePtr<IfStmt>;
	using UniqueBreakStmtPtr		= UniquePtr<BreakStmt>;
	using UniqueContinueStmtPtr		= UniquePtr<ContinueStmt>;
	using UniqueForStmtPtr			= UniquePtr<ForStmt>;

	using UniqueExprPtr				= UniquePtr<Expr>;
	using UniqueUnaryExprPtr		= UniquePtr<UnaryExpr>;
	using UniqueBinaryExprPtr		= UniquePtr<BinaryExpr>;
	using UniqueTernaryExprPtr		= UniquePtr<TernaryExpr>;
	using UniqueIdentifierExprPtr	= UniquePtr<IdentifierExpr>;
	using UniqueDeclRefExprPtr		= UniquePtr<DeclRefExpr>;
	using UniqueConstantIntPtr		= UniquePtr<ConstantInt>;
	using UniqueConstantStringPtr	= UniquePtr<ConstantString>;
	using UniqueConstantBoolPtr		= UniquePtr<ConstantBool>;
	using UniqueConstantFloatPtr	= UniquePtr<ConstantFloat>;
	using UniqueImplicitCastExprPtr	= UniquePtr<ImplicitCastExpr>;
	using UniqueFunctionCallExprPtr = UniquePtr<FunctionCallExpr>;

	using UniqueVariableDeclPtrList = std::vector<UniqueVariableDeclPtr>;
	using UniqueDeclPtrList			= std::vector<UniqueDeclPtr>;
	using UniqueStmtPtrList			= std::vector<UniqueStmtPtr>;
	using UniqueExprPtrList			= std::vector<UniqueExprPtr>;
	using ExprPtrList				= std::vector<Expr*>;
	using DeclPtrList				= std::vector<Decl*>;
	using StmtPtrList				= std::vector<Stmt*>;
	using VariableDeclPtrList		= std::vector<VariableDecl*>;
	using BreakStmtPtrList			= std::vector<BreakStmt*>;
	using ContinueStmtPtrList		= std::vector<ContinueStmt*>;
}