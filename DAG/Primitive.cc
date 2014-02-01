/** @file Primitive.cc    Definition of @ref Primitive. */
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

#include "DAG/Primitive.h"
#include "Support/Bytestream.h"
#include "Support/exceptions.h"

using namespace fabrique::dag;
using std::shared_ptr;
using std::string;


Primitive::Primitive(const Type& t, SourceRange loc)
	: Value(t, loc)
{
}

void Primitive::PrettyPrint(Bytestream& b, int indent) const
{
	b << Bytestream::Literal << str() << Bytestream::Reset;
}


Boolean::Boolean(bool b, const Type& t, SourceRange loc)
	: Primitive(t, loc), value(b)
{
	// TODO: assert(t is a subtype of bool?)
}

string Boolean::str() const { return value ? "true" : "false"; }


Integer::Integer(int i, const Type& t, SourceRange loc)
	: Primitive(t, loc), value(i)
{
}

string Integer::str() const { return std::to_string(value); }

shared_ptr<Value> Integer::Add(shared_ptr<Value>& v)
{
	SourceRange loc = SourceRange::Over(this, v.get());

	shared_ptr<Integer> other = std::dynamic_pointer_cast<Integer>(v);
	if (not other)
		throw SemanticException(
			"integers can only add with integers", loc);

	return shared_ptr<Value>(
		new Integer(this->value + other->value, type(), loc));
}


String::String(string s, const Type& t, SourceRange loc)
	: Primitive(t, loc), value(s)
{
}

string String::str() const { return value; }

shared_ptr<Value> String::Add(shared_ptr<Value>& v)
{
	SourceRange loc = SourceRange::Over(this, v.get());

	shared_ptr<String> other = std::dynamic_pointer_cast<String>(v);
	if (not other)
		throw SemanticException(
			"strings can only concatenate with strings", loc);

	return shared_ptr<Value>(
		new String(this->value + other->value, type(), loc));
}

void String::PrettyPrint(Bytestream& b, int indent) const
{
	b << Bytestream::Literal << "'" << str() << "'" << Bytestream::Reset;
}
