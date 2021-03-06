//! @file dag/Primitive.hh    Declaration of @ref fabrique::dag::Primitive
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

#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include <fabrique/Bytestream.hh>
#include <fabrique/HasSource.hh>
#include <fabrique/dag/Value.hh>

#include <functional>
#include <string>

namespace fabrique {
namespace dag {


//! The result of evaluating an expression.
template<class T>
class Primitive : public Value
{
public:
	virtual std::string str() const override = 0;
	virtual T value() const { return value_; }

	virtual void
	PrettyPrint(Bytestream& b, unsigned int /*indent*/ = 0) const override
	{
		b << Bytestream::Literal << str() << Bytestream::Reset;
	}

protected:
	Primitive(const Type& t, const T& val, SourceRange src)
		: Value(t, src), value_(val)
	{
	}

	const T value_;
};


//! A boolean (true/false) value.
class Boolean : public Primitive<bool>
{
public:
	Boolean(bool, const Type&, SourceRange src = SourceRange::None());
	std::string str() const override;

	virtual ValuePtr Not(const SourceRange& src) const override;

	virtual ValuePtr And(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Or(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Xor(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Equals(ValuePtr&, SourceRange) const override;

	void Accept(Visitor& v) const override;

private:
	ValuePtr Op(std::string operationName, std::function<bool (bool, bool)>,
	            ValuePtr other, SourceRange) const;
};


//! An integer (of unspecified precision).
class Integer : public Primitive<int>
{
public:
	Integer(int, const Type&, SourceRange src = SourceRange::None());
	std::string str() const override;

	virtual ValuePtr Negate(const SourceRange& src) const override;

	virtual ValuePtr Add(ValuePtr&, SourceRange) const override;
	virtual ValuePtr DivideBy(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Equals(ValuePtr&, SourceRange) const override;
	virtual ValuePtr MultiplyBy(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Subtract(ValuePtr&, SourceRange) const override;

	void Accept(Visitor& v) const override;

private:
	ValuePtr Op(std::string operationName, std::function<int (int, int)>,
	            ValuePtr other, SourceRange) const;
};


//! An ASCII string (for now, we should make this Unicode soon).
class String : public Primitive<std::string>
{
public:
	String(std::string, const Type&, SourceRange src = SourceRange::None());
	std::string str() const override;

	virtual ValuePtr Add(ValuePtr&, SourceRange) const override;
	virtual ValuePtr PrefixWith(ValuePtr&, SourceRange) const override;
	virtual ValuePtr Equals(ValuePtr&, SourceRange) const override;

	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;

	void Accept(Visitor& v) const override;
};

} // namespace dag
} // namespace fabrique

#endif // !PRIMITIVE_H
