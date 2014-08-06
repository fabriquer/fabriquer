/** @file DAG/List.cc    Definition of @ref fabrique::dag::List. */
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

#include "DAG/List.h"
#include "DAG/Target.h"
#include "DAG/Visitor.h"
#include "Support/Bytestream.h"
#include "Support/Join.h"
#include "Support/SourceLocation.h"
#include "Support/exceptions.h"
#include "Types/Type.h"
#include "Types/TypeContext.h"
#include "Types/TypeError.h"

#include <algorithm>
#include <cassert>

using namespace fabrique;
using namespace fabrique::dag;
using std::shared_ptr;
using std::string;
using std::vector;


List* List::of(const SharedPtrVec<Value>& values, const SourceRange& src,
               TypeContext& ctx)
{
	const Type& elementTy =
		values.empty()
			? ctx.nilType()
			: values.front()->type()
		;

	return new List(values, Type::ListOf(elementTy, src), src);
}


List::List(const SharedPtrVec<Value>& v, const Type& t, const SourceRange& src)
	: Value(t, src), elements_(v), elementType_(t[0])
{
#ifndef NDEBUG
	for (auto& value : v)
		assert(value);
#endif
}


const SequenceType& List::type() const
{
	return dynamic_cast<const SequenceType&>(Value::type());
}


List::iterator List::begin() const { return elements_.begin(); }
List::iterator List::end() const { return elements_.end(); }

size_t List::size() const { return elements_.size(); }

const Value& List::operator [] (size_t i) const
{
	return *elements_[i];
}


ValuePtr List::Add(ValuePtr& n) const
{
	SourceRange loc = SourceRange::Over(this, n.get());

	const List *next = n->asList();
	if (not next)
		throw SemanticException(
			"lists can only be concatenated with lists", loc);

	if (not elementType_.isSupertype(next->elementType_)
	    and not next->elementType_.isSupertype(elementType_))
		throw SemanticException(
			"incompatible operands to concatenate (types "
			+ type().str() + ", " + next->type().str() + ")", loc);

	SharedPtrVec<Value> values(elements_);
	auto& nextElem = next->elements_;
	values.insert(values.end(), nextElem.begin(), nextElem.end());

	return ValuePtr(List::of(values, loc, elementType_.context()));
}

ValuePtr List::PrefixWith(ValuePtr& prefix) const
{
	if (prefix->type() != elementType_)
		throw WrongTypeException(elementType_,
		                         prefix->type(), prefix->source());

	SharedPtrVec<Value> values;
	values.push_back(prefix);
	values.insert(values.end(), elements_.begin(), elements_.end());

	return ValuePtr(
		new List(values, type(), SourceRange::Over(prefix.get(), this))
	);
}

ValuePtr List::ScalarAdd(ValuePtr& scalar) const
{
	SharedPtrVec<Value> values;
	for (const ValuePtr& v : this->elements_)
		values.push_back(v->Add(scalar));

	return ValuePtr(
		new List(values, type(), SourceRange::Over(this, scalar.get()))
	);
}

bool List::canScalarAdd(const Value& other) const
{
	const Type& t = type();

	assert(t.typeParamCount() == 1);
	const Type& elementTy = t[0];

	return elementTy.onAddTo(other.type());
}


void List::PrettyPrint(Bytestream& out, size_t indent) const
{
	out
		<< Bytestream::Operator << "[ "
		<< Bytestream::Reset
		;

	for (const ValuePtr& p : elements_)
	{
		p->PrettyPrint(out, indent);
		out << " ";
	}

	out
		<< Bytestream::Operator << "]"
		<< Bytestream::Reset
		;
}


void List::Accept(Visitor& v) const
{
	if (v.Visit(*this))
	{
		for (auto& e : elements_)
			e->Accept(v);
	}
}
