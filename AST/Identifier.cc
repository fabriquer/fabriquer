/** @file AST/Identifier.cc    Definition of @ref fabrique::ast::Identifier. */
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

#include "AST/Identifier.h"
#include "AST/Visitor.h"
#include "Support/Bytestream.h"
#include "Types/Type.h"

using namespace fabrique::ast;


Identifier::Identifier(const std::string& name, const Type *type,
                       const SourceRange& src)
	: Node(src), OptionallyTyped(type), name_(name)
{
}

void Identifier::PrettyPrint(Bytestream& out, size_t /*indent*/) const
{
	out << Bytestream::Reference << name_;

	if (isTyped())
		out << Bytestream::Operator << ":"
		<< Bytestream::Type << type()
		;

	out << Bytestream::Reset;
}


void Identifier::Accept(Visitor& v) const { v.Enter(*this); v.Leave(*this); }


bool Identifier::operator == (const Identifier& other) const
{
	return name() == other.name() and type() == other.type();
}

bool Identifier::operator < (const Identifier& other) const
{
	return name() < other.name() or type().isSubtype(other.type());
}
