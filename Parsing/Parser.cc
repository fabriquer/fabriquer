/** @file Parsing/Parser.cc    Definition of @ref fabrique::ast::Parser. */
/*
 * Copyright (c) 2013-2014 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "AST/ast.h"
#include "Parsing/Lexer.h"
#include "Parsing/Parser.h"
#include "Parsing/Token.h"
#include "Plugin/Loader.h"
#include "Plugin/Plugin.h"
#include "Plugin/Registry.h"
#include "Support/Bytestream.h"
#include "Support/exceptions.h"
#include "Types/BooleanType.h"
#include "Types/FunctionType.h"
#include "Types/IntegerType.h"
#include "Types/StringType.h"
#include "Types/StructureType.h"
#include "Types/TypeContext.h"
#include "Types/TypeError.h"
#include "Support/os.h"

#include <cassert>
#include <fstream>
#include <sstream>

using namespace fabrique;
using namespace fabrique::ast;

using std::string;
using std::unique_ptr;


//! This is the parsing function generated by yacc.
int yyparse(ast::Parser*);


Parser::Parser(TypeContext& ctx, plugin::Registry& pluginRegistry,
               plugin::Loader& pluginLoader, string srcroot)
	: ctx_(ctx), lexer_(Lexer::instance()),
	  pluginRegistry_(pluginRegistry), pluginLoader_(pluginLoader),
	  srcroot_(srcroot)
{
	currentSubdirectory_.push("");
}


const Type& Parser::ParseDefinitions(const std::vector<string>& definitions)
{
	if (definitions_)
		throw UserError("arguments already defined");

	Type::NamedTypeVec args;
	const Type& nil = ctx_.nilType();

	UniqPtr<Scope> scope { new Scope(nullptr, "definitions", nil, ctx_) };

	for (const string& d : definitions)
	{
		std::istringstream input(d + ";");
		UniqPtr<ast::Scope> definitionTree { ParseFile(input, nil) };
		if (not definitionTree)
			throw UserError("invalid definition '" + d + "'");

		for (UniqPtr<ast::Value>& value : definitionTree->TakeValues())
		{
			const string name = value->name().name();

			if (name == ast::Arguments)
				continue;

			Bytestream::Debug("parser.cli.defines")
				<< Bytestream::Action << "Parsed definition"
				<< Bytestream::Operator << ": "
				<< *value
				<< Bytestream::Reset << "\n"
				;

			args.emplace_back(name, value->type());
			scope->Take(value);
		}

		definitionTree.reset();
	}

	definitions_.swap(scope);

	return ctx_.structureType(args);
}


UniqPtr<Scope> Parser::ParseFile(std::istream& input, const Type& args,
                                 string name, StringMap<string> builtins,
                                 SourceRange openedFrom)
{
	Bytestream& dbg = Bytestream::Debug("parser.file");
	dbg
		<< Bytestream::Action << "Parsing"
		<< Bytestream::Type << " file"
		<< Bytestream::Operator << " '"
		<< Bytestream::Literal << name
		<< Bytestream::Operator << "'"
		;

	if (args.valid())
	{
		dbg
			<< Bytestream::Reset << " with "
			<< Bytestream::Definition << "args"
			<< Bytestream::Operator << ": "
			;

		for (auto& i : args.fields())
			dbg
				<< Bytestream::Definition << i.first
				<< Bytestream::Operator << ":"
				<< i.second
				;
	}

	dbg << Bytestream::Reset << "\n";

	if (not name.empty())
	{
		const string relativeName =
			(name.find(srcroot_) == 0)
			? name.substr(srcroot_.length() + 1)
			: name;

		files_.push_back(relativeName);
	}

	lexer_.PushFile(input, name);
	EnterScope(name, args);

	// Define builtin strings like srcroot and builtroot.
	for (std::pair<string,string> i : builtins)
		if (not Builtin(i.first, i.second, openedFrom))
			throw AssertionFailure("Builtin('" + i.first + "')",
			                       "failed to create builtin");

	int result = yyparse(this);

	lexer_.PopFile();

	if (result != 0)
		return nullptr;

	return ExitScope();
}


//
// AST scopes:
//
Scope& Parser::EnterScope(const string& name, const Type& args, SourceRange src)
{
	Bytestream::Debug("parser.scope")
		<< string(scopes_.size(), ' ')
		<< Bytestream::Operator << " >> "
		<< Bytestream::Type << "scope"
		<< Bytestream::Literal << " '" << name << "'"
		<< Bytestream::Reset << "\n"
		;

	const bool FirstScope = scopes_.empty();

	const ast::Scope *parent = FirstScope ? nullptr : &CurrentScope();
	scopes_.emplace(new Scope(parent, name, args, ctx_));

	if (definitions_)
	{
		Builtin(ast::Arguments, definitions_, src);
		definitions_.release();
	}

	return *scopes_.top();
}

Scope& Parser::EnterScope(const string& name)
{
	return EnterScope(name, ctx_.nilType());
}

Scope& Parser::EnterScope(Scope&& s)
{
	Bytestream::Debug("parser.scope")
		<< string(scopes_.size(), ' ')
		<< Bytestream::Operator << " >> "
		<< Bytestream::Type << "scope"
		<< Bytestream::Literal << " '" << s.name() << "'"
		<< Bytestream::Reset << "\n"
		;

	if (scopes_.empty())
		scopes_.emplace(new Scope(std::move(s)));
	else
		scopes_.emplace(new Scope(std::move(s)));

	return *scopes_.top();
}

unique_ptr<Scope> Parser::ExitScope()
{
	unique_ptr<Scope> scope = std::move(scopes_.top());
	assert(scope and not scopes_.top());
	scopes_.pop();

	Bytestream& dbg = Bytestream::Debug("parser.scope");
	dbg
		<< string(scopes_.size(), ' ')
		<< Bytestream::Operator << " << "
		<< Bytestream::Type << "scope"
		<< Bytestream::Literal << " '" << scope->name() << "'"
		<< Bytestream::Operator << ":"
		;

	for (auto& v : scope->symbols())
		dbg << " " << v.first;

	dbg
		<< Bytestream::Reset << "\n"
		;

	return std::move(scope);
}


const Type& Parser::getType(const string& name,
                            const SourceRange& begin, const SourceRange& end,
                            const PtrVec<Type>& params)
{
	const SourceRange src(begin, end);

	const Type& t = ctx_.find(name, src, params);
	if (not t)
	{
		ReportError("unknown type", src);
		return t;
	}

	return t;
}

const Type& Parser::getType(UniqPtr<Identifier>&& name,
                            UniqPtr<const PtrVec<Type>>&& params)
{
	static const PtrVec<Type>& empty = *new PtrVec<Type>;
	if (not name)
		return ctx_.nilType();

	const SourceRange src = name->source();
	return getType(name->name(), src, src, params ? *params : empty);
}


const FunctionType& Parser::FnType(const PtrVec<Type>& inputs,
                                   const Type& output, SourceRange)
{
	return ctx_.functionType(inputs, output);
}


const StructureType* Parser::StructType(UniqPtr<UniqPtrVec<Identifier>>& f,
                                        SourceRange /*src*/)
{
	if (not f)
		return nullptr;

	Type::NamedTypeVec fields;
	for (UniqPtr<Identifier>& id : *f)
	{
		if (not id->isTyped())
		{
			ReportError("struct fields must have a name and a type",
			            id->source());
			return nullptr;
		}

		fields.emplace_back(id->name(), id->type());
	}

	return &ctx_.structureType(fields);
}


Action* Parser::DefineAction(UniqPtr<UniqPtrVec<Argument>>& args,
                             const SourceRange& src,
                             UniqPtr<UniqPtrVec<Parameter>>&& params)
{
	if (not args)
		return nullptr;

	if (not params)
	{
		ReportError("action has no parameters", src);
		return nullptr;
	}

	ExitScope();

	bool hasOutput = false;
	for (const auto& p : *params)
	{
		const Type& t = p->type();
		if (t.isFile() and dynamic_cast<const FileType&>(t).isOutputFile())
		{
			hasOutput = true;
			break;
		}
	}

	if (not hasOutput)
	{
		ReportError("action does not produce any output files", src);
		return nullptr;
	}

	return Action::Create(*args, params, src, ctx_);
}


Argument* Parser::Arg(UniqPtr<Expression>& value, UniqPtr<Identifier>&& name)
{
	if (not value)
		return nullptr;

	return new Argument(name, value);
}


BinaryOperation* Parser::BinaryOp(BinaryOperation::Operator op,
                                  UniqPtr<Expression>& lhs,
                                  UniqPtr<Expression>& rhs)
{
	if (not lhs or not rhs)
		return nullptr;

	return BinaryOperation::Create(std::move(lhs), op, std::move(rhs));
}


Call* Parser::CreateCall(UniqPtr<Expression>& targetExpr,
                         UniqPtr<UniqPtrVec<Argument>>& args,
                         const SourceRange& end)
{
	if (not targetExpr or not args)
		return nullptr;

	SourceRange src(targetExpr->source(), end);

	if (not targetExpr->type().isFunction())
	{
		ReportError("calling an uncallable expression", src);
		return nullptr;
	}

	auto& fnType = dynamic_cast<const FunctionType&>(targetExpr->type());

	// Do some sanity checking on the arguments (the kind that doesn't
	// require deep knowledge of the callee).
	bool seenKeywordArgument = false;
	for (auto& a : *args)
	{
		if (a->hasName())
			seenKeywordArgument = true;

		else if (seenKeywordArgument)
		{
			ReportError(
				"positional argument after keyword argument",
				a->source());
			return nullptr;
		}
	}

	return new Call(targetExpr, *args, fnType.returnType(), src);
}


CompoundExpression* Parser::CompoundExpr(UniqPtr<Expression>& result,
                                         SourceRange begin, SourceRange end)
{
	if (not result)
		return nullptr;

	SourceRange src = result->source();
	if (begin != SourceRange::None())
	{
		assert(end != SourceRange::None());
		src = SourceRange(begin, end);
	}

	return new CompoundExpression(ExitScope(), result, src);
}



FieldAccess* Parser::FieldAccess(UniqPtr<Expression>& structure,
                                 UniqPtr<Identifier>& field)
{
	if (not structure or not field)
		return nullptr;

	const string& name = field->name();

	const Type& structType = structure->type();
	if (not structType.hasFields())
		throw SemanticException(
			"value of type '" + structType.str() + "' does not have fields",
			structure->source());

	Type::TypeMap fieldTypes { structType.fields() };
	auto i = fieldTypes.find(name);
	if (i == fieldTypes.end())
		throw SemanticException("no such field", field->source());

	const Type& fieldType = i->second;

	if (field->isTyped() and field->type() != fieldType)
		throw WrongTypeException(fieldType, field->type(), field->source());
	else
		field.reset(Id(std::move(field), &fieldType));

	return new class FieldAccess(structure, field);
}


FieldQuery* Parser::FieldQuery(UniqPtr<Expression>& structure,
                               UniqPtr<Identifier>& field, UniqPtr<Expression>& def,
                               SourceRange src)
{
	if (not structure or not field)
		return nullptr;

	const Type& structType = structure->type();
	if (not structType.hasFields())
		throw SemanticException(
			"value of type '" + structType.str() + "' does not have fields",
			structure->source());

	Type::TypeMap structFields = structType.fields();
	auto i = structFields.find(field->name());
	if (i != structFields.end())
	{
		const Type& fieldType = i->second;
		const Type& initializerType = def->type();

		if (not fieldType.isSubtype(initializerType)
		and not initializerType.isSubtype(fieldType))
			throw WrongTypeException(initializerType, fieldType, src);
	}

	const Type& t =
		i == structFields.end()
			? def->type()
			: def->type().supertype(i->second)
			;

	return new class FieldQuery(structure, field, def, t, src);
}


Filename* Parser::File(UniqPtr<Expression>& name, const SourceRange& src,
                       UniqPtr<UniqPtrVec<Argument>>&& args)
{
	if (not name)
		return nullptr;

	static UniqPtrVec<Argument>& empty = *new UniqPtrVec<Argument>;

	if (not name->type().isSubtype(StringType::get(ctx_)))
	{
		ReportError("filename should be of type 'string', not '"
		             + name->type().str() + "'", *name);
		return nullptr;
	}

	return Filename::Create(name, args ? *args : empty, ctx_.fileType(), src);
}


FileList* Parser::Files(const SourceRange& begin,
                        UniqPtr<UniqPtrVec<Filename>>& files,
                        UniqPtr<UniqPtrVec<Argument>>&& args)
{
	if (not files)
		return nullptr;

	static UniqPtrVec<Argument>& emptyArgs = *new UniqPtrVec<Argument>;

	const Type& ty = ctx_.fileListType();
	SourceRange src(begin);

	return new FileList(*files, args ? *args : emptyArgs, ty, src);
}


ForeachExpr* Parser::Foreach(UniqPtr<Mapping>& mapping,
                             UniqPtr<Expression>& body,
                             const SourceRange& begin)
{
	if (not mapping or not body)
		return nullptr;

	SourceRange src(begin, body->source());
	ExitScope();

	const Type& resultTy = ctx_.listOf(body->type(), src);
	return new ForeachExpr(mapping, body, resultTy, src);
}


Function* Parser::DefineFunction(const SourceRange& begin,
                                 UniqPtr<UniqPtrVec<Parameter>>& params,
                                 UniqPtr<Expression>& body,
                                 const Type *resultType)
{
	if (not params or not body)
		return nullptr;

	if (resultType and not body->type().isSubtype(*resultType))
	{
		ReportError(
			"wrong return type ("
			+ body->type().str() + " not a subtype of "
			+ resultType->str()
			+ ")", *body);
		return nullptr;
	}

	SourceRange loc(begin, *body);

	PtrVec<Type> parameterTypes;
	for (auto& p : *params)
		parameterTypes.push_back(&p->type());

	ExitScope();

	const Type& retTy = resultType ? *resultType : body->type();
	const FunctionType& ty = ctx_.functionType(parameterTypes, retTy);
	return new Function(*params, ty, body, loc);
}



Identifier* Parser::Id(UniqPtr<fabrique::Token>&& name)
{
	if (not name)
		return nullptr;

	return new Identifier(*name, nullptr, name->source());
}

Identifier* Parser::Id(UniqPtr<Identifier>&& untyped, const Type *ty)
{
	if (not untyped)
		return nullptr;

	SourceRange loc(untyped->source().begin, lexer_.CurrentTokenRange().end);

	assert(ty);
	if (not *ty)
	{
		ReportError("invalid type", loc);
		return nullptr;
	}

	assert(not untyped->isTyped());

	return new Identifier(untyped->name(), ty, loc);
}


Import* Parser::ImportModule(UniqPtr<StringLiteral>& name, UniqPtrVec<Argument>& args,
                             SourceRange src)
{
	if (not name)
		return nullptr;

	Bytestream& dbg = Bytestream::Debug("parser.import");
	dbg
		<< Bytestream::Action << "importing"
		<< Bytestream::Type << " module"
		<< Bytestream::Operator << " '"
		<< Bytestream::Literal << name->str()
		<< Bytestream::Operator << "'"
		<< Bytestream::Reset << " with "
		;

	if (not args.empty())
	{
		dbg << "args ";
		for (const UniqPtr<Argument>& a : args)
			dbg << " " << *a;

		dbg << Bytestream::Reset << " and ";
	}

	dbg
		<< Bytestream::Reference << "srcroot "
		<< Bytestream::Filename << srcroot_
		<< Bytestream::Reset << "\n"
		;

	if (auto plugin = pluginRegistry_.lookup(name->str()).lock())
	{
		UniqPtr<plugin::Plugin> instance(plugin->Instantiate(ctx_));

		dbg
			<< Bytestream::Action << "found"
			<< Bytestream::Type << " plugin "
			<< Bytestream::Definition << plugin->name()
			<< Bytestream::Reset << " with type "
			<< instance->type()
			<< "\n"
			;

		return new Import(name, args, instance, src);
	}

	if (auto plugin = pluginLoader_.Load(name->str()).lock())
	{
		UniqPtr<plugin::Plugin> instance(plugin->Instantiate(ctx_));

		dbg
			<< Bytestream::Action << "loaded"
			<< Bytestream::Type << " plugin "
			<< Bytestream::Definition << plugin->name()
			<< Bytestream::Reset << " with type "
			<< instance->type()
			<< "\n"
			;

		return new Import(name, args, instance, src);
	}

	const string subdir(currentSubdirectory_.top());
	string filename;
	try { filename = FindModule(srcroot_, subdir, name->str()); }
	catch (const UserError& e)
	{
		ReportError(e.message(), src);
		return nullptr;
	}

	const string directory = DirectoryOf(filename);

	currentSubdirectory_.push(directory);

	const string absolute =
		PathIsAbsolute(filename) ? filename : JoinPath(srcroot_, filename);

	dbg
		<< Bytestream::Action << "found"
		<< Bytestream::Type << " module "
		<< Bytestream::Operator << "'"
		<< Bytestream::Literal << filename
		<< Bytestream::Operator << "'"
		<< Bytestream::Reset << " at "
		<< Bytestream::Operator << "'"
		<< Bytestream::Literal << absolute
		<< Bytestream::Operator << "'"
		<< "\n"
		;

	std::ifstream input(absolute);
	if (not input.good())
		throw UserError("Can't open '" + filename + "'");

	Type::NamedTypeVec argTypes;
	for (UniqPtr<Argument>& a : args)
	{
		if (not a->hasName())
		{
			ReportError("import argument must be named",
			            a->source());
			return nullptr;
		}

		argTypes.emplace_back(a->getName().name(), a->type());
	}

	const StructureType& argsType = ctx_.structureType(argTypes);

	UniqPtr<Scope> module = ParseFile(input, argsType, absolute);
	if (not module)
		return nullptr;

	currentSubdirectory_.pop();

	Type::NamedTypeVec fields;
	for (const UniqPtr<Value>& value : module->values())
	{
		const Identifier& id { value->name() };
		if (not id.reservedName())
			fields.emplace_back(id.name(), value->type());
	}
	const StructureType& ty { ctx_.structureType(fields) };

	return new Import(name, args, directory, module, ty, src);
}


Conditional* Parser::IfElse(const SourceRange& ifLocation,
                            UniqPtr<Expression>& condition,
                            UniqPtr<Expression>& thenResult,
                            UniqPtr<Expression>& elseResult)
{
	if (not condition or not thenResult or not elseResult)
		return nullptr;

	const Type& type = thenResult->type().supertype(elseResult->type());
	if (not type)
	{
		ReportError("incompatible types for conditional clauses: "
		            + thenResult->type().str()
			    + " vs " + elseResult->type().str(),
		            SourceRange::Over(thenResult, elseResult));
		return nullptr;
	}

	return new Conditional(ifLocation, condition, thenResult, elseResult, type);
}


List* Parser::ListOf(UniqPtrVec<Expression>& elements,
                     const SourceRange& src)
{
	const Type& elementType = ctx_.supertype(elements.begin(), elements.end());
	const Type& ty = ctx_.listOf(elementType, src);
	return new List(elements, ty, src);
}


Mapping* Parser::Map(UniqPtr<Expression>& source, UniqPtr<Identifier>& target)
{
	if (not source or not target)
		return nullptr;

	UniqPtr<Identifier> id(std::move(target));

	Bytestream& dbg = Bytestream::Debug("parser.map");
	dbg
		<< "mapping: " << *id
		<< Bytestream::Operator << " <- "
		<< *source
		<< Bytestream::Operator << ":"
		<< source->type()
		<< "\n"
		;

	assert(source->type());
	assert(source->type().typeParamCount() == 1);
	const Type& elementType = source->type()[0];

	if (id->isTyped())
	{
		if (not id->type().isSupertype(elementType))
		{
			ReportError("incompatible types for map: "
			            + id->type().str() + " vs "
			            + elementType.str(),
			            SourceRange::Over(source, target));
			return nullptr;
		}
	}
	else
		id.reset(Id(std::move(id), &elementType));


	UniqPtr<Parameter> parameter(Param(std::move(id)));

	SourceRange src(*parameter, *source);
	return new Mapping(parameter, source, src);
}

SomeValue* Parser::Some(UniqPtr<Expression>& initializer, SourceRange src)
{
	if (not initializer)
		return nullptr;

	const Type& type = ctx_.maybe(initializer->type(), src);
	return new SomeValue(type, initializer, src);
}

StructInstantiation* Parser::StructInstantiation(SourceRange src)
{
	UniqPtr<Scope> scope(ExitScope());

	Type::NamedTypeVec fields;
	for (const UniqPtr<Value>& v : scope->values())
		fields.emplace_back(v->name().name(), v->type());

	const StructureType& t = ctx_.structureType(fields);

	return new ast::StructInstantiation(scope, t, src);
}

BoolLiteral* Parser::True()
{
	return new BoolLiteral(true, BooleanType::get(ctx_),
	                       lexer_.CurrentTokenRange());
}

BoolLiteral* Parser::False()
{
	return new BoolLiteral(false, BooleanType::get(ctx_),
	                       lexer_.CurrentTokenRange());
}

IntLiteral* Parser::ParseInt(int value)
{
	return new IntLiteral(value, IntegerType::get(ctx_),
	                      lexer_.CurrentTokenRange());
}

StringLiteral* Parser::ParseString(UniqPtr<fabrique::Token>&& t)
{
	if (not t)
		return nullptr;

	return new StringLiteral(*t, StringType::get(ctx_), t->source());
}


Parameter* Parser::Param(UniqPtr<Identifier>&& name,
                         UniqPtr<Expression>&& defaultValue)
{
	if (not name)
		return nullptr;

	if (not (name->isTyped() or (defaultValue and defaultValue->type())))
	{
		ReportError("expected type or default value", *name);
		return nullptr;
	}

	if (name->isTyped() and defaultValue
	    and not defaultValue->type().isSubtype(name->type()))
	{
		ReportError("expected type " + name->type().str()
		            + ", got " + defaultValue->type().str(),
		            *defaultValue);
		return nullptr;
	}

	const Type& resultType =
		name->isTyped() ? name->type() : defaultValue->type();
	assert(resultType);

	auto *p = new Parameter(name, resultType, std::move(defaultValue));
	CurrentScope().Register(p);

	return p;
}


SymbolReference* Parser::Reference(UniqPtr<Identifier>&& name)
{
	if (not name)
		return nullptr;

	const Type& t = CurrentScope().Lookup(*name);
	if (not t.valid())
	{
		ReportError("reference to undefined value", *name);
		return nullptr;
	}

	return new SymbolReference(std::move(name), t);
}


SymbolReference* Parser::Reference(UniqPtr<class FieldAccess>&& access)
{
	if (not access)
		return nullptr;

	const Type& fieldType = access->type();
	return new SymbolReference(std::move(access), fieldType);
}


DebugTracePoint* Parser::TracePoint(UniqPtr<Expression>& e, SourceRange src)
{
	return new DebugTracePoint(e, src);
}


UnaryOperation* Parser::UnaryOp(UnaryOperation::Operator op,
                                const SourceRange& opSrc,
                                UniqPtr<Expression>& e)
{
	if (not e)
		return nullptr;

	return UnaryOperation::Create(op, opSrc, e);
}



bool Parser::DefineValue(UniqPtr<Identifier>& id, UniqPtr<Expression>& e, bool builtin)
{
	if (not id or not e)
		return false;

	if (not builtin and id->reservedName())
	{
		ReportError("defining value with reserved name", *id);
		return false;
	}

	auto& scope(CurrentScope());

	if (scope.contains(*id))
	{
		ReportError("redefining value", *id);
		return false;
	}

	SourceRange range = SourceRange::Over(id, e);

	if (id->isTyped() and !e->type().isSubtype(id->type()))
	{
		ReportError("assigning " + e->type().str()
		             + " to identifier of type " + id->type().str(),
		            range);
		return false;
	}

	const Type& t = id->isTyped() ? id->type() : e->type();
	if ((id->name() == ast::Subdirectory) and not t.isFile())
	{
		ReportError(string(ast::Subdirectory) + " must be a file (not '"
		             + t.str() + "')", range);
		return false;
	}

	scope.Take(new Value(id, e, t));
	return true;
}


bool Parser::DefineValue(UniqPtr<Expression>& value)
{
	if (not value)
		return false;

	auto& scope(CurrentScope());

	const size_t symbols = scope.symbols().size();
	UniqPtr<Identifier> id(new Identifier(ast::Unnamed + std::to_string(symbols)));

	scope.Take(new Value(id, value, value->type()));

	return true;
}


Scope& Parser::CurrentScope()
{
	// We must always have at least a top-level scope on the stack.
	assert(scopes_.size() > 0);
	return *scopes_.top();
}


bool Parser::Builtin(string name, UniqPtr<Expression>& e, SourceRange src)
{
	UniqPtr<Token> token(new Token(name, src));
	UniqPtr<Identifier> id(Id(std::move(token)));

	return DefineValue(id, e, true);
}


bool Parser::Builtin(string name, int value, SourceRange src)
{
	UniqPtr<Expression> val(ParseInt(value));

	return Builtin(name, val, src);
}


bool Parser::Builtin(string name, string value, SourceRange src)
{
	UniqPtr<Token> token(new Token(value, src));
	UniqPtr<Expression> val(ParseString(std::move(token)));

	return Builtin(name, val, src);
}


bool Parser::Builtin(string name, UniqPtr<Scope>& scope, SourceRange src)
{
	EnterScope(std::move(*scope));
	UniqPtr<Expression> value(StructInstantiation(src));

	return Builtin(name, value, src);
}


void Parser::AddToScope(const PtrVec<Argument>& args)
{
	auto& scope(CurrentScope());

	for (auto *arg : args)
		if (arg->hasName())
			scope.Register(arg);
}


fabrique::Token* Parser::ParseToken(YYSTYPE& yyunion)
{
	assert(yyunion.token);
	assert(dynamic_cast<fabrique::Token*>(yyunion.token));

	return yyunion.token;
}


bool Parser::Set(YYSTYPE& yyunion, Node *e)
{
	if (not e)
		return false;

	Bytestream& dbg = Bytestream::Debug("parser.node");
	dbg
		<< Bytestream::Action << "parsed "
		<< Bytestream::Type << "AST node"
		<< Bytestream::Reset
		;

	if (auto *typed = dynamic_cast<const Typed*>(e))
		dbg << " of type " << typed->type();

	else if (auto *ot = dynamic_cast<const OptionallyTyped*>(e))
	{
		if (ot->isTyped())
			dbg << " with type " << ot->type();
	}

	dbg
		<< Bytestream::Operator << ": "
		<< Bytestream::Reset << *e
		<< Bytestream::Operator << " @ " << e->source()
		<< "\n"
		;

	yyunion.node = e;
	return true;
}


const ErrorReport& Parser::ReportError(const string& msg, const HasSource& s,
                                       ErrorReport::Severity severity)
{
	return ReportError(msg, s.source(), severity);
}

const ErrorReport& Parser::ReportError(const string& message,
                                       const SourceRange& location,
                                       ErrorReport::Severity severity)
{
	errs_.push_back(
		unique_ptr<ErrorReport>(ErrorReport::Create(message, location, severity))
	);

	return *errs_.back();
}
