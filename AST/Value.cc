/** @file AST/Value.cc    Definition of @ref fabrique::ast::Value. */
/*
 * Copyright (c) 2013-2015, 2018 Jonathan Anderson
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

#include "AST/EvalContext.h"
#include "AST/Value.h"
#include "AST/Visitor.h"
#include "DAG/Build.h"
#include "DAG/File.h"
#include "DAG/List.h"
#include "DAG/TypeReference.h"
#include "Support/Bytestream.h"
#include "Types/Type.h"
#include "Types/TypeError.h"

#include <cassert>
#include <iomanip>

using namespace fabrique;
using namespace fabrique::ast;
using std::string;


Value::Value(UniqPtr<Identifier> id, UniqPtr<TypeReference> explicitType,
             UniqPtr<Expression> value)
	: Expression(SourceRange::Over(id, value)),
	  name_(std::move(id)), explicitType_(std::move(explicitType)),
	  value_(std::move(value))
{
	assert(not explicitType_ or name_ && "having an explicit type => having a name");
	assert(value_);
}


void Value::PrettyPrint(Bytestream& out, unsigned int indent) const
{
	string tabs(indent, '\t');

	out << tabs;

	if (name_)
	{
		out << Bytestream::Definition << name_->name();

		if (explicitType_)
		{
			out << Bytestream::Operator << ":" << Bytestream::Reset;
			explicitType_->PrettyPrint(out, indent);
		}

		out << Bytestream::Operator << " = ";
	}

	value_->PrettyPrint(out, indent);
	out
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
	Bytestream& dbg = Bytestream::Debug("eval.value");
	dbg
		<< Bytestream::Action << "Evaluating "
		<< *this
		<< Bytestream::Operator << "..."
		<< "\n"
		;

	const string name = name_ ? name_->name() : "";
	auto valueName(ctx.evaluating(name));

	dag::ValuePtr val(value_->evaluate(ctx));
	assert(val);

	dbg
		<< name
		<< Bytestream::Operator << " == "
		<< *val
		<< "\n"
		;

	if (explicitType_)
	{
		auto typeRef = explicitType_->evaluateAs<dag::TypeReference>(ctx);
		val->type().CheckSubtype(typeRef->referencedType(), val->source());
	}

	if (name != "")
	{
		ctx.Define(valueName, val, source());
	}

	return val;
}
