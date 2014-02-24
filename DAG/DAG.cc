/** @file DAG.cc    Definition of @ref DAG. */
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
#include "AST/Visitor.h"

#include "DAG/Build.h"
#include "DAG/DAG.h"
#include "DAG/File.h"
#include "DAG/List.h"
#include "DAG/Primitive.h"
#include "DAG/Rule.h"
#include "DAG/Target.h"
#include "DAG/UndefinedValueException.h"
#include "DAG/Value.h"

#include "Support/Bytestream.h"
#include "Support/Join.h"
#include "Support/exceptions.h"

#include "Types/FunctionType.h"
#include "Types/Type.h"
#include "Types/TypeError.h"

#include <cassert>
#include <deque>
#include <stack>

using namespace fabrique;
using namespace fabrique::dag;

using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;

template<class T>
using ConstPtr = const std::unique_ptr<T>;


class ImmutableDAG : public DAG
{
public:
	ImmutableDAG(SharedPtrVec<File>& files, SharedPtrVec<Build>& builds,
	             SharedPtrMap<Rule>& rules, SharedPtrMap<Value>& variables,
	             SharedPtrMap<Target>& targets);

	const SharedPtrVec<File>& files() const override { return files_; }
	const SharedPtrVec<Build>& builds() const override { return builds_; }
	const SharedPtrMap<Rule>& rules() const override { return rules_; }
	const SharedPtrMap<Value>& variables() const override { return vars_; }
	const SharedPtrMap<Target>& targets() const override
	{
		return targets_;
	}

private:
	const SharedPtrVec<File> files_;
	const SharedPtrVec<Build> builds_;
	const SharedPtrMap<Rule> rules_;
	const SharedPtrMap<Value> vars_;
	const SharedPtrMap<Target> targets_;
};


//! AST Visitor that flattens the AST into a DAG.
class DAGBuilder : public ast::Visitor
{
public:
	DAGBuilder(FabContext& ctx)
		: fileType(*ctx.fileType()), stringTy(*ctx.type("string"))
	{
	}

	~DAGBuilder() {}

	VISIT(ast::Action)
	VISIT(ast::Argument)
	VISIT(ast::BinaryOperation)
	VISIT(ast::BoolLiteral)
	VISIT(ast::Call)
	VISIT(ast::CompoundExpression)
	VISIT(ast::Conditional)
	VISIT(ast::Filename)
	VISIT(ast::FileList)
	VISIT(ast::ForeachExpr)
	VISIT(ast::Function)
	VISIT(ast::Identifier)
	VISIT(ast::IntLiteral)
	VISIT(ast::List)
	VISIT(ast::Parameter)
	VISIT(ast::Scope)
	VISIT(ast::StringLiteral)
	VISIT(ast::SymbolReference)
	VISIT(ast::UnaryOperation)
	VISIT(ast::Value)

	// The values we're creating:
	SharedPtrVec<File> files_;
	SharedPtrVec<Build> builds_;
	SharedPtrMap<Rule> rules_;
	SharedPtrMap<Value> variables_;
	SharedPtrMap<Target> targets_;

private:
	ValueMap& EnterScope(const string& name);
	ValueMap ExitScope();
	ValueMap& CurrentScope();

	//! Get a named value from the current scope or a parent scope.
	shared_ptr<Value> getNamedValue(const std::string& name);

	shared_ptr<Value> flatten(const ast::Expression&);


	//! The components of the current scope's fully-qualified name.
	std::deque<string> scopeName;

	//! Symbols defined in this scope (or the one up from it, or up...).
	std::deque<ValueMap> scopes;

	//! The type of individual source (or target) files.
	const Type& fileType;

	//! The type of generated strings.
	const Type& stringTy;

	/** The name of the value we are currently processing. */
	std::stack<string> currentValueName;

	/**
	 * The value currently being processed.
	 * Visitor methods should leave a single item on this stack.
	 */
	std::stack<shared_ptr<Value>> currentValue;
};


UniqPtr<DAG> DAG::Flatten(const ast::Scope& s, FabContext& ctx)
{
	DAGBuilder f(ctx);
	s.Accept(f);

	return UniqPtr<DAG>(new ImmutableDAG(
		f.files_, f.builds_, f.rules_, f.variables_, f.targets_));
}


void DAG::PrettyPrint(Bytestream& out, int indent) const
{
	SharedPtrMap<Value> namedValues;
	for (auto& i : rules()) namedValues.emplace(i);
	for (auto& i : targets()) namedValues.emplace(i);
	for (auto& i : variables()) namedValues.emplace(i);

	for (auto& i : namedValues)
	{
		const string& name = i.first;
		const shared_ptr<Value>& v = i.second;

		assert(v);

		out
			<< Bytestream::Type << v->type()
			<< Bytestream::Definition << " " << name
			<< Bytestream::Operator << " = "
			<< *v
			<< Bytestream::Reset << "\n"
			;
	}

	for (const shared_ptr<File>& f : files())
	{
		out
			<< Bytestream::Type << f->type()
			<< Bytestream::Operator << ": "
			<< *f
			<< Bytestream::Reset << "\n"
			;
	}

	for (const shared_ptr<Build>& b : builds())
	{
		out
			<< Bytestream::Type << "build"
			<< Bytestream::Operator << ": "
			<< *b
			<< Bytestream::Reset << "\n"
			;
	}
}


ImmutableDAG::ImmutableDAG(
		SharedPtrVec<File>& files, SharedPtrVec<Build>& builds,
		SharedPtrMap<Rule>& rules, SharedPtrMap<Value>& variables,
		SharedPtrMap<Target>& targets)
	: files_(files), builds_(builds), rules_(rules),
	  vars_(variables), targets_(targets)
{
}


bool DAGBuilder::Enter(const ast::Action& a)
{
	string command;
	ValueMap arguments;

	if (a.arguments().size() < 1)
		throw SemanticException("Missing action arguments", a.source());

	for (ConstPtr<ast::Argument>& arg : a.arguments())
	{
		// Flatten the argument and convert it to a string.
		shared_ptr<Value> value = flatten(arg->getValue());

		// The only keyword-less argument to action() is its command.
		if (not arg->hasName() or arg->getName().name() == "command")
		{
			if (not command.empty())
				throw SemanticException(
					"Duplicate command", arg->source());

			command = value->str();
			continue;
		}

		shared_ptr<Value> v(new String(value->str(), stringTy,
		                               arg->source()));
		arguments.emplace(arg->getName().name(), v);
	}

	currentValue.emplace(Rule::Create(currentValueName.top(),
	                                  command, arguments, a.type()));

	return false;
}

void DAGBuilder::Leave(const ast::Action&) {}


bool DAGBuilder::Enter(const ast::Argument& arg)
{
	currentValue.emplace(flatten(arg.getValue()));
	return false;
}

void DAGBuilder::Leave(const ast::Argument&) {}


bool DAGBuilder::Enter(const ast::BinaryOperation& o)
{
	shared_ptr<Value> lhs = flatten(o.getLHS());
	shared_ptr<Value> rhs = flatten(o.getRHS());

	assert(lhs and rhs);

	shared_ptr<Value> result;
	switch (o.getOp())
	{
		case ast::BinaryOperation::Add:
			result = lhs->Add(rhs);
			break;

		case ast::BinaryOperation::Prefix:
			result = rhs->PrefixWith(lhs);
			break;

		case ast::BinaryOperation::ScalarAdd:
			if (lhs->canScalarAdd(*rhs))
				result = lhs->ScalarAdd(rhs);

			else if (rhs->canScalarAdd(*lhs))
				result = rhs->ScalarAdd(lhs);

			else
				throw SemanticException(
					"invalid types for addition",
					SourceRange::Over(lhs.get(), rhs.get())
				);
			break;

		case ast::BinaryOperation::And:
			result = lhs->And(rhs);
			break;

		case ast::BinaryOperation::Or:
			result = lhs->Or(rhs);
			break;

		case ast::BinaryOperation::Xor:
			result = lhs->Xor(rhs);
			break;

		case ast::BinaryOperation::Invalid:
			break;
	}

	assert(result);
	currentValue.emplace(result);

	return false;
}

void DAGBuilder::Leave(const ast::BinaryOperation&) {}


bool DAGBuilder::Enter(const ast::BoolLiteral& b)
{
	currentValue.emplace(
		new Boolean(b.value(), b.type(), b.source()));

	return false;
}

void DAGBuilder::Leave(const ast::BoolLiteral&) {}


bool DAGBuilder::Enter(const ast::Call& call) { return false; }
void DAGBuilder::Leave(const ast::Call& call)
{
	const ast::SymbolReference& targetRef = call.target();
	const string name = targetRef.getName().name();
	const ast::Expression& target = targetRef.definition();
	shared_ptr<Value> targetValue = getNamedValue(name);

	//
	// Are we calling a function?
	//
	if (auto *fn = dynamic_cast<const ast::Function*>(&target))
	{
		//
		// We evaluate the function with the given arguments by
		// putting these names into the local scope and then
		// entering the function's CompoundExpr.
		//
		ValueMap& scope = EnterScope("fn eval");

		StringMap<const ast::Parameter&> params;
		std::vector<string> paramNames;
		for (auto& p : fn->parameters())
		{
			const string name(p->getName().name());

			paramNames.push_back(name);
			params.emplace(name, *p);
		}

		//
		// Check the arguments and their types.
		//
		if (params.size() != call.arguments().size())
			throw SyntaxError(
				"expected " + std::to_string(params.size())
				+ " arguments, got "
				+ std::to_string(call.arguments().size()),
				call.source());

		size_t unnamed = 0;
		for (ConstPtr<ast::Argument>& a : call.arguments())
		{
			string name = a->hasName()
				? a->getName().name()
				: paramNames[unnamed++];

			auto p = params.find(name);
			if (p == params.end())
				throw SyntaxError(
					"no such parameter '" + name + "'",
					a->source());

			const ast::Parameter& param = p->second;
			assert(p->first == name);

			if (not a->type().isSubtype(param.type()))
				throw WrongTypeException(param.type(),
					a->type(), a->source());

			shared_ptr<Value> v(flatten(*a));
			scope[name] = v;
		}

		currentValue.emplace(flatten(fn->body()));
		ExitScope();

		return;
	}


	//
	// We can only call functions and build rules. If it's not the former,
	// it must be a rule!
	//
	shared_ptr<Rule> rule = dynamic_pointer_cast<Rule>(targetValue);
	assert(rule);

	shared_ptr<Value> in, out;
	SharedPtrVec<Value> dependencies, extraOutputs;
	ValueMap arguments;

	//
	// Interpret the first two unnamed arguments as 'in' and 'out'.
	//
	for (ConstPtr<ast::Argument>& arg : call)
	{
		shared_ptr<Value> value = flatten(*arg);

		if (arg->hasName())
		{
			const std::string& name = arg->getName().name();

			if (name == "in")
				in = value;

			else if (name == "out")
				out = value;

			else
				arguments[name] = value;
		}
		else
		{
			if (not in)
				in = value;

			else if (not out)
				out = value;
		}
	}

	//
	// Validate against The Rules:
	//  1. there must be input
	//  2. there must be output
	//  3. all other arguments must match explicit parameters
	//
	if (not in)
		throw SemanticException(
			"use of action without input file(s)", call.source());

	if (not out)
		throw SemanticException(
			"use of action without output file(s)", call.source());


	const ast::Action& action =
		dynamic_cast<const ast::Action&>(targetRef.definition());

	auto& params = action.parameters();

	for (auto& i : arguments)
	{
		const string name = i.first;
		shared_ptr<dag::Value> arg = i.second;

		auto j = params.begin();
		for ( ; j != params.end(); j++)
			if ((*j)->getName().name() == name)
				break;

		if (j == params.end())
			throw SemanticException(
				"no such parameter '" + name + "'",
				arg->source());

		//
		// Additionally, if the parameter is a file, add the
		// argument to the dependency graph.
		//
		const ast::Parameter& p = **j;
		const Type& type = p.type();

		if (not type.isFile())
			continue;

		//
		// A file parameter should either be named in/out or have
		// an in/out type parameter (e.g. foo:file[in]).
		//
		if (name == "in" or name == "out")
			continue;

		if (type.typeParamCount() == 0)
			throw SemanticException(
				"file missing [in] or [out] tag", p.source());


		if (type[0].name() == "in")
			dependencies.push_back(arg);

		else if (type[0].name() == "out")
			extraOutputs.push_back(arg);

		else
			throw WrongTypeException("file[in|out]",
				p.type(), p.source());
	}

	shared_ptr<Build> build(
		Build::Create(rule, in, out, dependencies, extraOutputs,
		              arguments, call.source()));

	builds_.push_back(build);
	currentValue.push(build);
}


bool DAGBuilder::Enter(const ast::CompoundExpression& e)
{
	Enter(static_cast<const ast::Scope&>(e));
	return true;
}

void DAGBuilder::Leave(const ast::CompoundExpression& e)
{
	Leave(static_cast<const ast::Scope&>(e));
	assert(not currentValue.empty());
}


bool DAGBuilder::Enter(const ast::Conditional& c)
{
	shared_ptr<Boolean> cond =
		dynamic_pointer_cast<Boolean>(flatten(c.condition()));

	if (not cond)
		throw WrongTypeException("bool", c.type(), c.source());

	// Flatten either the "then" or the "else" clause.
	currentValue.emplace(
		flatten(cond->value() ? c.thenClause() : c.elseClause())
	);

	return false;
}

void DAGBuilder::Leave(const ast::Conditional&) {}


bool DAGBuilder::Enter(const ast::Filename& f)
{
	string name = flatten(f.name())->str();

	if (shared_ptr<Value> subdir = getNamedValue(ast::Subdirectory))
		name = join(subdir->str(), name, "/");

	shared_ptr<File> file(new File(name, f.type(), f.source()));
	files_.push_back(file);
	currentValue.push(file);

	return false;
}
void DAGBuilder::Leave(const ast::Filename&) {}


bool DAGBuilder::Enter(const ast::FileList& l)
{
	SharedPtrVec<Value> files;
	ValueMap listScope;

	for (ConstPtr<ast::Argument>& arg : l.arguments())
	{
		const string name = arg->getName().name();
		shared_ptr<Value> value = flatten(arg->getValue());

		if (name == ast::Subdirectory)
			if (shared_ptr<Value> existing = getNamedValue(name))
			{
				string base = existing->str();
				string subdir = join(base, value->str(), "/");
				SourceRange loc = arg->source();

				value.reset(new String(subdir, stringTy, loc));
			}

		listScope[name] = value;
	}

	EnterScope("file list");

	for (ConstPtr<ast::Filename>& file : l)
	{
		shared_ptr<Value> f = flatten(*file);
		files.push_back(f);

		assert(f == dynamic_pointer_cast<File>(f));
	}

	ExitScope();

	currentValue.emplace(new List(files, l.type(), l.source()));

	return false;
}
void DAGBuilder::Leave(const ast::FileList&) {}


bool DAGBuilder::Enter(const ast::ForeachExpr& f)
{
	SharedPtrVec<Value> values;

	auto target = flatten(f.targetSequence());
	assert(target->type().name() == "list");
	shared_ptr<List> input = dynamic_pointer_cast<List>(target);
	const ast::Parameter& loopParam = f.loopParameter();

	//
	// For each input element, put its value in scope as the loop parameter
	// and then evaluate the CompoundExpression.
	//
	for (const shared_ptr<Value>& element : *input)
	{
		ValueMap& scope = EnterScope("foreach body");

		scope[loopParam.getName().name()] = element;
		values.push_back(flatten(f.loopBody()));

		ExitScope();
	}

	currentValue.emplace(new List(values, f.type(), f.source()));
	return false;
}

void DAGBuilder::Leave(const ast::ForeachExpr&) {}


bool DAGBuilder::Enter(const ast::Function&) { return false; }
void DAGBuilder::Leave(const ast::Function&) {}


bool DAGBuilder::Enter(const ast::Identifier&) { return false; }
void DAGBuilder::Leave(const ast::Identifier&) {}


bool DAGBuilder::Enter(const ast::IntLiteral& i)
{
	currentValue.emplace(
		new Integer(i.value(), i.type(), i.source()));

	return false;
}

void DAGBuilder::Leave(const ast::IntLiteral&) {}


bool DAGBuilder::Enter(const ast::List& l)
{
	assert(l.type().name() == "list");
	assert(l.type().typeParamCount() == 1);
	const Type& subtype = l.type()[0];

	SharedPtrVec<Value> values;

	for (ConstPtr<ast::Expression>& e : l)
	{
		if (e->type() != subtype)
			throw WrongTypeException(subtype,
			                         e->type(), e->source());

		values.push_back(flatten(*e));
	}

	currentValue.emplace(new List(values, l.type(), l.source()));

	return false;
}

void DAGBuilder::Leave(const ast::List&) {}


bool DAGBuilder::Enter(const ast::Parameter&) { return false; }
void DAGBuilder::Leave(const ast::Parameter&) {}


bool DAGBuilder::Enter(const ast::Scope&)
{
	EnterScope("AST scope");
	return false;
}

void DAGBuilder::Leave(const ast::Scope&)
{
	ValueMap scopedSymbols = ExitScope();

	// We only save top-level values if we are, in fact, at the top level.
	if (not scopes.empty())
		return;

	const string scopeName = join(this->scopeName, ".");

	for (auto symbol : scopedSymbols)
	{
		const string name = join(scopeName, symbol.first, ".");
		shared_ptr<Value>& v = symbol.second;

		if (dynamic_pointer_cast<File>(v)
		    or dynamic_pointer_cast<Build>(v))
		{
			// Do nothing: files and builds have already been
			// added to files_ or builds_, respectively.
		}
		else if (auto rule = dynamic_pointer_cast<Rule>(v))
			rules_[name] = rule;

		else if (auto target = dynamic_pointer_cast<Target>(v))
			targets_[name] = target;

		else
			variables_[name] = v;
	}
}


bool DAGBuilder::Enter(const ast::StringLiteral& s)
{
	currentValue.emplace(new String(s.str(), s.type(), s.source()));
	return false;
}

void DAGBuilder::Leave(const ast::StringLiteral&) {}


bool DAGBuilder::Enter(const ast::SymbolReference& r)
{
	const string& name = r.getName().name();

	shared_ptr<Value> value = getNamedValue(name);
	if (not value)
		throw UndefinedValueException(name, r.source());

	currentValue.emplace(value);

	return false;
}

void DAGBuilder::Leave(const ast::SymbolReference&) {}


bool DAGBuilder::Enter(const ast::UnaryOperation& o)
{
	shared_ptr<Value> subexpr = flatten(o.getSubExpr());
	assert(subexpr);

	shared_ptr<Value> result;
	switch (o.getOp())
	{
		case ast::UnaryOperation::Negate:
			result = subexpr->Negate(o.source());
			break;

		case ast::UnaryOperation::Invalid:
			break;
	}

	assert(result);
	currentValue.emplace(result);

	return false;
}

void DAGBuilder::Leave(const ast::UnaryOperation&) {}


bool DAGBuilder::Enter(const ast::Value& v)
{
	currentValueName.push(v.getName().name());
	return true;
}

void DAGBuilder::Leave(const ast::Value& v)
{
	// Things like function definitions will never be flattened.
	if (currentValue.empty())
		return;

	ValueMap& currentScope = CurrentScope();

	Bytestream& dbg = Bytestream::Debug("dag.scope");
	dbg
		<< Bytestream::Action << "defining "
		<< Bytestream::Literal << "'" << currentValueName.top() << "'"
		<< Bytestream::Operator << ":"
		;

	shared_ptr<Value> val = std::move(currentValue.top());
	currentValue.pop();

	const string name = currentValueName.top();
	currentValueName.pop();


	//
	// If the right-hand side is a build, file or list of files,
	// convert to a named target (files and builds are already in the DAG).
	//
	if (auto build = dynamic_pointer_cast<Build>(val))
		val.reset(Target::Create(name, build));

	else if (auto file = dynamic_pointer_cast<File>(val))
		val.reset(Target::Create(name, file));

	else if (auto list = dynamic_pointer_cast<List>(val))
	{
		if (list->type().isListOf(fileType))
			val.reset(Target::Create(name, list));
	}


	currentScope.emplace(name, std::move(val));

	for (auto& i : currentScope)
		dbg << Bytestream::Definition << " " << i.first;

	dbg << Bytestream::Reset << "\n";
}


ValueMap& DAGBuilder::EnterScope(const string& name)
{
	Bytestream::Debug("dag.scope")
		<< string(scopes.size(), ' ')
		<< Bytestream::Operator << " >> "
		<< Bytestream::Type << "scope"
		<< Bytestream::Literal << " '" << name << "'"
		<< Bytestream::Reset << "\n"
		;

	scopes.push_back(ValueMap());
	return CurrentScope();
}

ValueMap DAGBuilder::ExitScope()
{
	ValueMap values = std::move(CurrentScope());
	scopes.pop_back();

	Bytestream& dbg = Bytestream::Debug("parser.scope");
	dbg
		<< string(scopes.size(), ' ')
		<< Bytestream::Operator << " << "
		<< Bytestream::Type << "scope"
		<< Bytestream::Operator << ":"
		;

	for (auto& i : values)
		dbg << " " << i.first;

	dbg << Bytestream::Reset << "\n";

	return std::move(values);
}

ValueMap& DAGBuilder::CurrentScope()
{
	return scopes.back();
}


shared_ptr<Value> DAGBuilder::getNamedValue(const string& name)
{
	Bytestream& dbg = Bytestream::Debug("dag.lookup");
	dbg
		<< Bytestream::Action << "lookup "
		<< Bytestream::Literal << "'" << name << "'"
		<< Bytestream::Reset << "\n"
		;

	for (auto i = scopes.rbegin(); i != scopes.rend(); i++)
	{
		const ValueMap& scope = *i;

		auto value = scope.find(name);
		if (value != scope.end())
		{
			dbg
				<< Bytestream::Action << "  found "
				<< Bytestream::Literal << "'" << name << "'"
				<< Bytestream::Operator << ": "
				<< *value->second
				<< Bytestream::Reset << "\n"
				;
			return value->second;
		}

		dbg
			<< "  no "
			<< Bytestream::Literal << "'" << name << "'"
			<< Bytestream::Operator << ":"
			;

		for (auto& j : scope)
			dbg << " " << Bytestream::Definition << j.first;

		dbg << Bytestream::Reset << "\n";
	}

	return NULL;
}

shared_ptr<Value> DAGBuilder::flatten(const ast::Expression& e)
{
	e.Accept(*this);

	assert(not currentValue.empty());
	shared_ptr<Value> v(currentValue.top());
	currentValue.pop();

	return v;
}
