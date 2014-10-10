/** @file DAG/Function.h    Declaration of @ref fabrique::dag::Function. */
/*
 * Copyright (c) 2014 Jonathan Anderson
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

#ifndef DAG_FUNCTION_H
#define DAG_FUNCTION_H

#include "ADT/PtrVec.h"
#include "DAG/Callable.h"
#include "DAG/Value.h"

#include <functional>


namespace fabrique {

class FunctionType;

namespace dag {

class EvalContext;

/**
 * A reference to a user- or plugin-defined function.
 */
class Function : public Callable, public Value
{
public:
	typedef std::function<ValuePtr (const ValueMap&, const ValueMap&)>
		Evaluator;

	Function(Evaluator, ValueMap&& scope, const SharedPtrVec<Parameter>&,
	         const FunctionType&, SourceRange source = SourceRange::None());
	virtual ~Function();

	//! Call this function with (named) arguments.
	virtual ValuePtr Call(const ValueMap& arguments) const;

	//! A copy of the scope containing the function (at definition).
	const ValueMap& scope() const { return containingScope_; }

	virtual void PrettyPrint(Bytestream&, size_t indent = 0) const override;
	void Accept(Visitor&) const override;

private:
	const Evaluator evaluator_;
	const ValueMap containingScope_;
};

} // namespace dag
} // namespace fabrique

#endif
