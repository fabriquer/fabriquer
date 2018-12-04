/** @file Backend/Ninja.cc    Definition of fabrique::backend::NinjaBackend. */
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

#include <fabrique/Bytestream.hh>
#include <fabrique/strings.hh>

#include <fabrique/backend/Ninja.hh>

#include <fabrique/dag/Build.hh>
#include <fabrique/dag/DAG.hh>
#include <fabrique/dag/File.hh>
#include <fabrique/dag/Formatter.hh>
#include <fabrique/dag/List.hh>
#include <fabrique/dag/Parameter.hh>
#include <fabrique/dag/Primitive.hh>
#include <fabrique/dag/Record.hh>
#include <fabrique/dag/Rule.hh>
#include <fabrique/dag/TypeReference.hh>

#include <cassert>

using namespace fabrique::backend;
using namespace fabrique::dag;

using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {

class NinjaFormatter : public Formatter
{
public:
	using Formatter::Format;

	string Format(const Boolean&);
	string Format(const Build&);
	string Format(const File&);
	string Format(const Function&);
	string Format(const Integer&);
	string Format(const List&);
	string Format(const Record&);
	string Format(const Rule&);
	string Format(const String&);
	string Format(const TypeReference&);
};

const char* ReservedRuleArguments[] = { "command", "description" };

} // anonymous namespace


NinjaBackend* NinjaBackend::Create()
{
	return new NinjaBackend;
}


NinjaBackend::NinjaBackend()
	: indent_("    ")
{
}


void NinjaBackend::Process(const dag::DAG& dag, Bytestream& out,
                           ErrorReport::Report ReportError)
{
	NinjaFormatter formatter;

	// Header comment:
	out
		<< Bytestream::Comment
		<< "#\n"
		<< "# Ninja file generated by Fabrique\n"
		<< "#\n"
		<< "\n"
		;


	// Variables:
	out
		<< "#\n"
		<< "# Varables:\n"
		<< "#\n"
		<< Bytestream::Reset
		;

	for (auto& i : dag.variables())
	{
		out
			<< Bytestream::Definition << i.first
			<< Bytestream::Operator << " = "
			<< Bytestream::Literal << formatter.Format(*i.second)
			<< Bytestream::Reset
			<< "\n"
			;
	}


	// Rules:
	out
		<< "\n"
		<< Bytestream::Comment
		<< "#\n"
		<< "# Rules:\n"
		<< "#\n"
		<< Bytestream::Reset
		;

	for (auto& i : dag.rules())
	{
		const dag::Rule& rule = *i.second;

		out
			<< Bytestream::Type << "rule "
			<< Bytestream::Action << i.first
			<< Bytestream::Reset << "\n"

			<< Bytestream::Definition << "  command"
			<< Bytestream::Operator << " = "
			<< Bytestream::Literal << rule.command()
			<< Bytestream::Reset << "\n"

			<< Bytestream::Definition << "  description"
			<< Bytestream::Operator << " = "
			<< Bytestream::Literal << rule.description()
			<< Bytestream::Reset << "\n"
			;

		if (rule.name() == Rule::RegenerationRuleName())
			out
				<< Bytestream::Definition << "  generator"
				<< Bytestream::Operator << " = "
				<< Bytestream::Literal << "true"
				<< Bytestream::Reset << "\n"
				;

		// Check for use of reserved names (e.g., 'command', 'description')
		for (auto& p : rule.parameters())
		{
			for (auto& a : ReservedRuleArguments)
			{
				if (strcmp(p->name().c_str(), a) == 0)
					ReportError("name has reserved meaning in Ninja",
					            p->source(),
					            ErrorReport::Severity::Warning, "");
			}

		}

		for (auto& a : rule.arguments())
			out
				<< Bytestream::Definition << "  " << a.first
				<< Bytestream::Operator << " = "
				<< Bytestream::Literal
				<< formatter.Format(*a.second)
				<< Bytestream::Reset << "\n"
				;

		out << "\n";
	}


	//
	// Build steps, starting with pseudo-targets:
	//
	out
		<< Bytestream::Comment
		<< "#\n"
		<< "# Build steps:\n"
		<< "#\n"
		<< Bytestream::Reset
		;


	// Psuedo-targets:
	for (auto& i : dag.targets())
		out
			<< Bytestream::Type << "build "
			<< Bytestream::Definition << i.first
			<< Bytestream::Operator << " : "
			<< Bytestream::Action << "phony "
			<< Bytestream::Literal << formatter.Format(*i.second)
			<< "\n"
			;

	out << "\n";


	// Build steps:
	for (auto& i : dag.builds())
	{
		const dag::Build& build = *i;

		out << Bytestream::Type << "build" << Bytestream::Filename;
		for (const shared_ptr<File>& f : build.outputs())
			out << " " << formatter.Format(*f);

		out
			<< Bytestream::Operator << " : "
			<< Bytestream::Action << build.buildRule().name()
			<< Bytestream::Filename
			;

		for (const shared_ptr<File>& f : build.inputs())
			out << " " << formatter.Format(*f);

		out << "\n";

		for (auto& a : build.arguments())
			out
				<< Bytestream::Definition << "  " << a.first
				<< Bytestream::Operator << " = "
				<< Bytestream::Literal
				<< formatter.Format(*a.second)
				<< Bytestream::Reset << "\n"
				;
		out << "\n";
	}
}


string NinjaFormatter::Format(const Boolean& b)
{
	return b.value() ? "true" : "false";
}

string NinjaFormatter::Format(const Build& b)
{
	std::vector<string> substrings;

	for (const shared_ptr<File>& f : b.outputs())
		substrings.push_back(Format(*f));

	return fabrique::join(substrings, " ");
}

string NinjaFormatter::Format(const File& f)
{
	if (f.generated())
		return f.relativeName();
	else
		return f.fullName();
}

string NinjaFormatter::Format(const Function&)
{
	return "";
}

string NinjaFormatter::Format(const Integer& i)
{
	return std::to_string(i.value());
}

string NinjaFormatter::Format(const List& l)
{
	std::vector<string> substrings;

	for (const shared_ptr<Value>& element : l)
		substrings.push_back(Format(*element));

	return fabrique::join(substrings, " ");
}

string NinjaFormatter::Format(const Record&)
{
	return "";
}

string NinjaFormatter::Format(const Rule& rule)
{
	return rule.command();
}

string NinjaFormatter::Format(const String& s)
{
	return s.value();
}

string NinjaFormatter::Format(const TypeReference& t)
{
	return t.referencedType().str();
}
