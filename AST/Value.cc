/** @file AST/Value.cc    Definition of @ref fabrique::ast::Value. */
/*
 * Copyright (c) 2013-2015 Jonathan Anderson
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

#include "AST/Builtins.h"
#include "AST/EvalContext.h"
#include "AST/Value.h"
#include "AST/Visitor.h"
#include "DAG/Build.h"
#include "DAG/File.h"
#include "DAG/List.h"
#include "Parsing/ErrorReporter.h"
#include "Support/Bytestream.h"
#include "Types/Type.h"
#include "Types/TypeError.h"

#include <cassert>
#include <iomanip>

using namespace fabrique;
using namespace fabrique::ast;
using std::string;


Value::Parser::~Parser()
{
}


string Value::Parser::name(const Scope& scope, Err& err) const
{
	TypeContext none;

	UniqPtr<Identifier> id(name_->Build(scope, none, err));
	assert(id && "Identifier should not encounter parsing errors");

	return id->name();
}


Value* Value::Parser::Build(const Scope& scope, TypeContext& types, Err& err)
{
	UniqPtr<Identifier> name(name_->Build(scope, types, err));
	UniqPtr<TypeReference> explicitType;
	if (type_)
		explicitType.reset(type_->Build(scope, types, err));

	UniqPtr<Expression> value(value_->Build(scope, types, err));
	if (not name or not value)
		return nullptr;

	SourceRange src = SourceRange::Over(name, value);

	if (name->reservedName())
	{
		err.ReportError("defining value with reserved name", *name);
		return nullptr;
	}

	const Type& type = explicitType ? explicitType->referencedType() : value->type();

	if (explicitType and not value->type().isSubtype(type))
	{
		err.ReportError("assigning " + value->type().str()
		                + " to value of type " + type.str(), src);
		return nullptr;
	}

	if ((name->name() == ast::Subdirectory) and not type.isFile())
	{
		err.ReportError(string(ast::Subdirectory) + " must be a file (not '"
		                + type.str() + "')", src);
		return nullptr;
	}

	return new Value(std::move(name), std::move(explicitType), std::move(value),
	                 type);
}


Value::Value(UniqPtr<Identifier> id, UniqPtr<TypeReference> explicitType,
             UniqPtr<Expression> value, const Type& t)
	: Expression(t, SourceRange::Over(id, value)),
	  name_(std::move(id)), explicitType_(std::move(explicitType)),
	  value_(std::move(value))
{
	assert(not name_->isTyped() or t.isSubtype(name_->type()));
	assert(value_->type().isSubtype(t));

	assert(name_);
	assert(value_);
}


void Value::PrettyPrint(Bytestream& out, size_t indent) const
{
	string tabs(indent, '\t');

	out
		<< tabs
		<< Bytestream::Definition << name_->name()
		<< Bytestream::Operator << ":"
		<< Bytestream::Type << type()
		<< Bytestream::Operator << " = "
		<< Bytestream::Reset << *value_
		<< Bytestream::Operator << ";"
		<< Bytestream::Reset
		;
}

void Value::Accept(Visitor& v) const
{
	if (v.Enter(*this))
	{
		name_->Accept(v);
		value_->Accept(v);
	}

	v.Leave(*this);
}


dag::ValuePtr Value::evaluate(EvalContext& ctx) const
{
	const string name(name_->name());
	Bytestream& dbg = Bytestream::Debug("eval.value");
	dbg
		<< Bytestream::Action << "Evaluating "
		<< *name_
		<< Bytestream::Operator << "..."
		<< "\n"
		;

	auto valueName(ctx.evaluating(name));
	dag::ValuePtr val(value_->evaluate(ctx));
	assert(val);

	dbg
		<< *name_
		<< Bytestream::Operator << " == "
		<< *val
		<< "\n"
		;

	val->type().CheckSubtype(type(), val->source());

	ctx.Define(valueName, val);

	return val;
}
