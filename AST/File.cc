/** @file AST/File.cc    Definition of @ref fabrique::ast::File. */
/*
 * Copyright (c) 2013, 2016 Jonathan Anderson
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

#include "AST/Argument.h"
#include "AST/Builtins.h"
#include "AST/EvalContext.h"
#include "AST/File.h"
#include "AST/Visitor.h"
#include "DAG/File.h"
#include "Support/Bytestream.h"
#include "Support/exceptions.h"
#include "Types/FileType.h"

#include <cassert>
#include <set>

using namespace fabrique;
using namespace fabrique::ast;
using std::string;


File::Parser::~Parser()
{
}

File* File::Parser::Build(const Scope& scope, TypeContext& types, Err& err)
{
	assert(name_);

	UniqPtr<Expression> name(name_->Build(scope, types, err));

	UniqPtrVec<Argument> arguments;
	for (auto& a : arguments_)
	{
		arguments.emplace_back(a->Build(scope, types, err));
		if (not arguments.back())
		{
			return nullptr;
		}
	}

	const FileType& type = types.fileType();

	return new File(std::move(name), std::move(arguments), type, source());
}


File* File::Create(UniqPtr<Expression>& name, UniqPtrVec<Argument>& args,
                   const FileType& t, const SourceRange& src)
{
	if (args.empty())
		return new File(std::move(name), std::move(args), t, src);

	Type::TypeMap argTypes;
	for (const UniqPtr<Argument>& a : args)
		argTypes.emplace(a->getName().name(), a->type());

	const FileType& incomplete = dynamic_cast<const FileType&>(t);
	const FileType& complete = incomplete.WithArguments(argTypes);

	return new File(std::move(name), std::move(args), complete, src);
}


File::File(UniqPtr<Expression> name, UniqPtrVec<Argument> args,
           const FileType& t, const SourceRange& src)
	: Expression(t, src), unqualName_(std::move(name)),
	  args_(std::move(args))
{
}


const fabrique::FileType& File::type() const
{
	return dynamic_cast<const FileType&>(Expression::type());
}


void File::PrettyPrint(Bytestream& out, size_t /*indent*/) const
{
	out
		<< Bytestream::Action << "file"
		<< Bytestream::Operator << "(";

	out << Bytestream::Filename << *unqualName_ << Bytestream::Reset;

	for (auto& a : args_)
		out
			<< Bytestream::Operator << ", "
			<< Bytestream::Reset << *a;

	out
		<< Bytestream::Operator << ")"
		<< Bytestream::Reset;
}


void File::Accept(Visitor& v) const
{
	if (v.Enter(*this))
	{
		unqualName_->Accept(v);
		for (auto& a : args_)
			a->Accept(v);
	}

	v.Leave(*this);
}

dag::ValuePtr File::evaluate(EvalContext& ctx) const
{
	const string filename = unqualName_->evaluate(ctx)->str();

	assert(ctx.Lookup(ast::Subdirectory));
	string subdirectory = ctx.Lookup(ast::Subdirectory)->str();

	dag::ValueMap attributes;
	for (const UniqPtr<ast::Argument>& a : args_)
	{
		if (not a->hasName())
			throw SemanticException("file arguments must have names",
			                        a->source());

		const string& name = a->getName().name();
		dag::ValuePtr value = a->getValue().evaluate(ctx);

		if (name == ast::Subdirectory)
			subdirectory = value->str();

		else
			attributes[name] = value;
	}

	return ctx.builder().File(
		subdirectory, filename, attributes, type(), source());
}