/** @file AST/Action.cc    Definition of @ref fabrique::ast::Action. */
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

#include "ADT/UniqPtr.h"
#include "AST/Action.h"
#include "AST/EvalContext.h"
#include "AST/Visitor.h"
#include "DAG/Parameter.h"
#include "DAG/Primitive.h"
#include "Support/Bytestream.h"
#include "Types/FileType.h"
#include "Types/FunctionType.h"
#include "Types/SequenceType.h"
#include "Types/TypeContext.h"
#include "Types/TypeError.h"

#include <cassert>

using namespace fabrique;
using namespace fabrique::ast;
using std::string;


typedef std::function<bool (const Type&)> TypePredicate;

template<class T>
size_t Count(const UniqPtrVec<T>& values, TypePredicate predicate)
{
	size_t count = 0;

	for (auto& v : values)
	{
		const Type& t = v->type();

		// If we have a list of input files, treat it as "more than one".
		if (t.isOrdered())
		{
			auto& seqType = dynamic_cast<const SequenceType&>(t);
			if (predicate(seqType.elementType()))
				count += 2;
		}
		else if (predicate(v->type()))
		{
			count += 1;
		}
	}

	return count;
}


Action* Action::Create(UniqPtrVec<Argument>& args,
                       UniqPtr<UniqPtrVec<Parameter>>& params,
                       const SourceRange& src, TypeContext& ctx)
{
	UniqPtrVec<Parameter> parameters;
	if (params)
	{
		// Verify that all file parameters are either inputs or outputs.
		for (auto& p : *params)
		{
			const Type& t = p->type();
			if (t.isFile())
			{
				auto& file = dynamic_cast<const FileType&>(t);
				if (not file.isInputFile()
				    and not file.isOutputFile())
					throw WrongTypeException(
						"file[in|out]", t, p->source());
			}
		}

		parameters = std::move(*params);
	}

	const Type& file = ctx.fileType();
	const Type& fileList = ctx.fileListType();

	const FunctionType& type = ctx.functionType(
		Count(parameters, FileType::isInput) == 1 ? file : fileList,
		Count(parameters, FileType::isOutput) == 1 ? file : fileList);

	return new Action(args, parameters, type, src);
}


Action::Action(UniqPtrVec<Argument>& a, UniqPtrVec<Parameter>& params,
               const FunctionType& ty, const SourceRange& loc)
	: Expression(ty, loc), HasParameters(params), args_(std::move(a))
{
}


void Action::PrettyPrint(Bytestream& out, unsigned int /*indent*/) const
{
	out
		<< Bytestream::Action << "action"
		<< Bytestream::Operator << "("
		<< Bytestream::Reset
		;

	for (size_t i = 0; i < args_.size(); )
	{
		assert(args_[i] != NULL);

		out << *args_[i];
		if (++i < args_.size())
			out << Bytestream::Operator << ", ";
	}

	const UniqPtrVec<Parameter>& params = parameters();
	if (not params.empty())
	{
		out << Bytestream::Operator << " <- ";

		for (size_t i = 0; i < params.size(); )
		{
			out << *params[i];
			if (++i < params.size())
				out << Bytestream::Operator << ", ";
		}
	}

	out
		<< Bytestream::Operator << ")"
		<< Bytestream::Reset
		;
}


void Action::Accept(Visitor& v) const
{
	if (v.Enter(*this))
	{
		for (auto& a : args_)
			a->Accept(v);

		for (auto& p : parameters())
			p->Accept(v);
	}

	v.Leave(*this);
}

dag::ValuePtr Action::evaluate(EvalContext& ctx) const
{
	string command;
	dag::ValueMap arguments;

	if (args_.size() < 1)
		throw SemanticException("Missing action arguments", source());

	for (const UniqPtr<Argument>& arg : args_)
	{
		// Evaluate the argument as a string.
		dag::ValuePtr value = arg->getValue().evaluate(ctx);

		// The only keyword-less argument to action() is its command.
		if (not arg->hasName() or arg->getName().name() == "command")
		{
			if (not command.empty())
				throw SemanticException(
					"Duplicate command", arg->source());

			command = value->str();
			continue;
		}

		dag::ValuePtr v(
			new dag::String(value->str(),
			                type().context().stringType(),
			                arg->source())
		);
		arguments.emplace(arg->getName().name(), v);
	}

	SharedPtrVec<dag::Parameter> parameters;
	for (const UniqPtr<Parameter>& p : this->parameters())
	{
		// Ensure that files are properly tagged as input or output.
		FileType::CheckFileTags(p->type(), p->source());

		std::shared_ptr<dag::Parameter> param =
			std::dynamic_pointer_cast<dag::Parameter>(
				p->evaluate(ctx));

		assert(param);
		parameters.emplace_back(param);
	}

	return ctx.builder().Rule(command, arguments, parameters, type(), source());
}
