/** @file literals.h    Declaration of several literal expression types. */
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

#include "AST/literals.h"
#include "AST/Visitor.h"
#include "Support/Bytestream.h"

using namespace fabrique::ast;


std::string BoolLiteral::str() const
{
	return (value() ? "true" : "false");
}

void BoolLiteral::PrettyPrint(Bytestream& out, int indent) const
{
	out
		<< Bytestream::Literal << str()
		<< Bytestream::Reset;
}

void BoolLiteral::Accept(Visitor& v) const { v.Enter(*this); v.Leave(*this); }


std::string IntLiteral::str() const
{
	return std::to_string(value());
}

void IntLiteral::PrettyPrint(Bytestream& out, int indent) const
{
	out << Bytestream::Literal << value() << Bytestream::Reset;
}

void IntLiteral::Accept(Visitor& v) const { v.Enter(*this); v.Leave(*this); }


std::string StringLiteral::str() const { return value(); }

void StringLiteral::PrettyPrint(Bytestream& out, int indent) const
{
	out << Bytestream::Literal << "'";

	std::string s = value();
	size_t i = 0;
	do
	{
		// Highlight variable references within strings.
		size_t dollarSign = s.find("$", i);
		out << s.substr(i, dollarSign - i);

		if (dollarSign == std::string::npos)
			break;

		size_t end;
		if (s[dollarSign + 1] == '{')
			end = s.find("}", dollarSign + 1) + 1;

		else
			end = std::min(
				s.find(" ", dollarSign + 1),
				s.find(".", dollarSign + 1)
			);

		out
			<< Bytestream::Reference
			<< s.substr(dollarSign, end - dollarSign)
			<< Bytestream::Literal
			;

		i = end;

	} while (i < s.length());

	out << "'" << Bytestream::Reset;
}

void StringLiteral::Accept(Visitor& v) const { v.Enter(*this); v.Leave(*this); }
