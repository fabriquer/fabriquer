//! @file ast/List.hh    Declaration of @ref fabrique::ast::List
/*
 * Copyright (c) 2013 Jonathan Anderson
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

#ifndef LIST_H
#define LIST_H

#include <fabrique/ast/Expression.hh>

namespace fabrique {
namespace ast {

/**
 * A list of same-typed expressions.
 */
class List : public Expression
{
public:
	List(UniqPtrVec<Expression> e, SourceRange loc)
		: Expression(loc), elements_(std::move(e))
	{
	}

	const UniqPtrVec<Expression>& elements() const { return elements_; }

	using ConstIterator = UniqPtrVec<Expression>::const_iterator;
	ConstIterator begin() const { return elements_.begin(); }
	ConstIterator end() const { return elements_.end(); }

	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;
	virtual void Accept(Visitor&) const override;

	virtual dag::ValuePtr evaluate(EvalContext&) const override;

private:
	const UniqPtrVec<Expression> elements_;
};

} // namespace ast
} // namespace fabrique

#endif
