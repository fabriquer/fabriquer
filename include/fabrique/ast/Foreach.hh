//! @file ast/Foreach.hh    Declaration of @ref fabrique::ast::ForeachExpr
/*
 * Copyright (c) 2013-2014, 2018 Jonathan Anderson
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

#ifndef FOREACH_H
#define FOREACH_H

#include <fabrique/ast/CompoundExpr.hh>
#include <fabrique/ast/TypeReference.hh>

namespace fabrique {
namespace ast {

class Parameter;
class Value;


/**
 * An expression that maps list elements into another list.
 */
class ForeachExpr : public Expression
{
public:
	ForeachExpr(UniqPtr<Identifier> loopVarName, UniqPtr<TypeReference> explicitType,
	            UniqPtr<Expression> inputValue, UniqPtr<Expression> body,
	            SourceRange);

	const Expression& sourceSequence() const { return *inputValue_; }
	const Expression& loopBody() const { return *body_; }

	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;
	virtual void Accept(Visitor&) const override;

	virtual dag::ValuePtr evaluate(EvalContext&) const override;

private:
	const UniqPtr<Identifier> loopVarName_;
	const UniqPtr<TypeReference> explicitType_;
	const UniqPtr<Expression> inputValue_;
	const UniqPtr<Expression> body_;
};

} // namespace ast
} // namespace fabrique

#endif
