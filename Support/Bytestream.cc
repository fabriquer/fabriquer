/** @file Bytestream.cc    Definition of @ref Bytestream. */
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

#include "Support/Bytestream.h"
#include "Support/Printable.h"

#include <cassert>
#include <fstream>
#include <vector>

#include <fnmatch.h>   // filename-style pattern matching
#include <unistd.h>

using namespace fabrique;
using std::ostream;
using std::string;


class ANSIStream : public Bytestream
{
public:
	ANSIStream(ostream& o) : Bytestream(o) {}

	enum Colour
	{
		Black = 0,
		Red,
		Green,
		Yellow,
		Blue,
		Magenta,
		Cyan,
		White,
		EndOfColours
	};

	enum Modifier
	{
		ResetAll = -1,
		Normal = 0,
		Bold,
		Faint,
		Italic,
		Underline,
		EndOfModifiers,
	};

	Bytestream& operator << (enum Format);

private:
	enum Colour Background(enum Colour c) { return (enum Colour) (c + 10); }

	void set(enum Colour);
	void set(enum Modifier);
};

class PlainStream : public Bytestream
{
public:
	PlainStream(ostream& o) : Bytestream(o) {}
	Bytestream& operator << (enum Format f)
	{
		// Ignore all formatting.
		return *this;
	}
};

class NullStream : public Bytestream
{
public:
	NullStream() : Bytestream(out) {}

	virtual Bytestream& operator << (enum Format) { return *this; }
	virtual Bytestream& operator << (const Printable&) { return *this; }
	virtual Bytestream& operator << (const string&) { return *this; }
	virtual Bytestream& operator << (unsigned long) { return *this; }

private:
	/**
	 * An uninitialised file output stream. If we never initialised the
	 * stream, it will stay in the error state, so formatting operations
	 * will never be performed; this saves us the cost of formatting
	 * characters before throwing them away.
	 */
	std::ofstream out;
};


static class DebugState
{
public:
	Bytestream *out = &Bytestream::Stderr();
	Bytestream *null = &Bytestream::None();
	string pattern;

	bool match(const string& name)
	{
		string longPattern = pattern + ".*";

		return (fnmatch(pattern.c_str(), name.c_str(), 0) == 0)
			or (fnmatch(longPattern.c_str(), name.c_str(), 0) == 0);
	}

	Bytestream& get(const string& name)
	{
		assert(out);
		assert(null);

		return match(name) ? *out : *null;
	}
} debugState;


Bytestream& Bytestream::Stdout()
{
	static ANSIStream ANSIOut(std::cout);
	static PlainStream PlainOut(std::cout);

	if (isatty(fileno(stdin)))
	    return ANSIOut;

	return PlainOut;
}

Bytestream& Bytestream::Stderr()
{
	static ANSIStream ANSIErr(std::cerr);
	static PlainStream PlainErr(std::cerr);

	if (isatty(fileno(stdin)))
		return ANSIErr;

	return PlainErr;
}

Bytestream& Bytestream::Debug(const string& name)
{
	return debugState.get(name);
}

void Bytestream::SetDebugPattern(const string& pattern)
{
	debugState.pattern = pattern;
}

void Bytestream::SetDebugStream(Bytestream& s)
{
	debugState.out = &s;
}

Bytestream& Bytestream::None()
{
	static NullStream stream;
	return stream;
}

Bytestream* Bytestream::Formatted(std::ostream& f)
{
	return new ANSIStream(f);
}

Bytestream* Bytestream::Plain(std::ostream& f)
{
	return new PlainStream(f);
}


Bytestream& Bytestream::operator << (const Printable& p)
{
	p.PrettyPrint(*this);
	return *this;
}

Bytestream& Bytestream::operator << (const std::string& s)
{
	out << s;
	return *this;
}

Bytestream& Bytestream::operator << (unsigned long x)
{
	out << x;
	return *this;
}



Bytestream& ANSIStream::operator << (enum Format f)
{
	switch (f)
	{
		case Action:            set(Red);               break;
		case Comment:           set(Blue);              break;
		case Definition:        set(Green);             break;
		case Filename:          set(Green);             break;
		case Literal:           set(Magenta);           break;
		case Operator:          set(Yellow);            break;
		case Reference:         set(Cyan);              break;
		case Type:              set(Blue);              break;

		case Error:             set(Bold); set(Red);    break;
		case ErrorLoc:          set(Green);             break;

		case Line:              set(Bold); set(Cyan);   break;
		case Column:            set(Blue);              break;

		case Reset:             set(ResetAll);          break;
	}

	// Ignore all formatting.
	return *this;
}

void ANSIStream::set(enum Colour c)
{
	assert((c >= Black and c < EndOfColours)
	       or (c >= Background(Black) and c < Background(EndOfColours)));

	out << "\x1b[" << (30 + (int) c) << "m";
}

void ANSIStream::set(enum Modifier m)
{
	assert(m >= ResetAll and m < EndOfModifiers);

	if (m == ResetAll)
		out << "\x1b[0m";
	else
		out << "\x1b[" << ((int) m) << "m";
}
