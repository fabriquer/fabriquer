/** @file Support/exceptions.h    Declaration of basic Fabrique exceptions. */
/*
 * Copyright (c) 2013, 2018 Jonathan Anderson
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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "Support/Printable.h"
#include "Support/SourceLocation.h"

#include <exception>
#include <memory>
#include <string>

#define FAB_ASSERT(expr, detail) \
	do \
	{ \
		if (not expr) \
		{ \
			throw fabrique::AssertionFailure(#expr, detail); \
		} \
	} \
	while (false)



namespace fabrique {


//! Some code may choose to throw this exception rather than assert() out.
class AssertionFailure : public std::exception
{
public:
	AssertionFailure(const std::string& condition,
	                 const std::string& message = "");

	AssertionFailure(const AssertionFailure&);

	virtual ~AssertionFailure() override;

	const char* what() const noexcept override;
	const std::string& condition() const noexcept { return condition_; }
	const std::string& message() const noexcept { return message_; }

private:
	const std::string condition_;
	const std::string message_;
};


//! An error that has an OS-specific description.
class OSError : public std::exception, public Printable
{
public:
	OSError(const std::string& message, const std::string& description);
	OSError(const OSError&);
	virtual ~OSError() override;

	virtual const std::string& message() const { return message_; }
	virtual const std::string& description() const { return description_; }

	const char* what() const noexcept override { return completeMessage_.c_str(); }
	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;

private:
	const std::string message_;
	const std::string description_;
	const std::string completeMessage_;
};



//! An error in user input.
class UserError : public std::exception, public Printable
{
public:
	UserError(const std::string& message);
	UserError(const UserError&);
	virtual ~UserError() override;

	virtual const std::string& message() const { return message_; }
	const char* what() const noexcept override { return message_.c_str(); }

	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;

private:
	const std::string message_;
};



class ErrorReport;

//! Base class for exceptions related to invalid source code.
class SourceCodeException
	: public std::exception, public HasSource, public Printable
{
public:
	virtual ~SourceCodeException() override;

	const std::string& message() const;
	virtual const char* what() const noexcept override;

	virtual void PrettyPrint(Bytestream&, unsigned int indent = 0) const override;

protected:
	SourceCodeException(const std::string& message, const SourceRange&);
	SourceCodeException(const SourceCodeException&);

private:
	std::shared_ptr<ErrorReport> err_;
};


//! A parser assertion failed.
class ParserError : public SourceCodeException
{
public:
	ParserError(const std::string& message, const SourceRange&);
	ParserError(const ParserError&);
	virtual ~ParserError() override;
};

//! A syntactic error is present in the Fabrique description.
class SyntaxError : public SourceCodeException
{
public:
	SyntaxError(const std::string& message, const SourceRange&);
	SyntaxError(const SyntaxError&);
	virtual ~SyntaxError() override;
};

//! A semantic error is present in the Fabrique description.
class SemanticException : public SourceCodeException
{
public:
	SemanticException(const std::string& message, const SourceRange&);
	SemanticException(const SemanticException&);
	virtual ~SemanticException() override;
};

}

#endif
