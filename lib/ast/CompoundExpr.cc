//! @file ast/CompoundExpr.cc    Definition of @ref fabrique::ast::CompoundExpression
/*
 * Copyright (c) 2013, 2018 Jonathan Anderson
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

#include <fabrique/Bytestream.hh>
#include <fabrique/ast/CompoundExpr.hh>
#include <fabrique/ast/EvalContext.hh>
#include <fabrique/ast/Value.hh>
#include <fabrique/ast/Visitor.hh>

#include <cassert>

using namespace fabrique;
using namespace fabrique::ast;


CompoundExpression::CompoundExpression(UniqPtrVec<Value> values,
                                       UniqPtr<Expression> result,
                                       SourceRange loc)
	: Expression(loc), values_(std::move(values)), result_(std::move(result))
{
	SemaCheck(result_, loc, "invalid CompoundExpression result");
}


void CompoundExpression::PrettyPrint(Bytestream& out, unsigned int indent) const
{
	std::string tabs(indent, '\t');
	std::string intabs(indent + 1, '\t');

	out << Bytestream::Operator << "\n" << tabs << "{\n";
	for (auto& v : values_)
	{
		v->PrettyPrint(out, indent + 1);
		out << "\n";
	}

	out
		<< intabs << *result_
		<< "\n" << Bytestream::Operator << tabs << "}\n"
		<< Bytestream::Reset
		;
}


void CompoundExpression::Accept(Visitor& v) const
{
	if (v.Enter(*this))
	{
		for (auto& val : values_)
			val->Accept(v);

		result_->Accept(v);
	}

	v.Leave(*this);
}

dag::ValuePtr CompoundExpression::evaluate(EvalContext& ctx) const
{
	auto scope(ctx.EnterScope("CompoundExpression"));

	for (auto& v : values_)
		v->evaluate(ctx);

	return result_->evaluate(ctx);
}
