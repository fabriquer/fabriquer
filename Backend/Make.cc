/** @file Backend/Make.cc    Definition of fabrique::backend::MakeBackend. */
/*
 * Copyright (c) 2014 Jonathan Anderson
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

#include "Backend/Make.h"

#include "DAG/Build.h"
#include "DAG/DAG.h"
#include "DAG/File.h"
#include "DAG/Formatter.h"
#include "DAG/List.h"
#include "DAG/Primitive.h"
#include "DAG/Rule.h"
#include "DAG/Target.h"
#include "DAG/Value.h"

#include "Support/Bytestream.h"
#include "Support/Join.h"
#include "Support/os.h"

#include <cassert>
#include <set>

using namespace fabrique::backend;
using namespace fabrique::dag;

using fabrique::Bytestream;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::vector;

namespace fabrique
{
static int replaceAll(string& s, const string& pattern, const string&);
static int replaceAll(string& s, const string& pattern, const Build::FileVec&);
}


class MakeFormatter : public Formatter
{
public:
	using Formatter::Format;

	string Format(const Boolean&);
	string Format(const Build&);
	string Format(const File&);
	string Format(const Function&);
	string Format(const Integer&);
	string Format(const List&);
	string Format(const Rule&);
	string Format(const String&);
	string Format(const Structure&);
	string Format(const Target&);
};




MakeBackend* MakeBackend::Create(Flavour flavour)
{
	return new MakeBackend(flavour);
}


MakeBackend::MakeBackend(Flavour flavour)
	: flavour_(flavour), indent_("\t")
{
}


string MakeBackend::DefaultFilename() const
{
	switch (flavour_)
	{
		case Flavour::POSIX:
			return "Makefile";

		case Flavour::BSD:
			return "BSDMakefile";

		case Flavour::GNU:
			return "GNUMakefile";
	}
}


void MakeBackend::Process(const dag::DAG& dag, Bytestream& out, ErrorReport::Report)
{
	MakeFormatter formatter;

	// Header comment:
	out
		<< Bytestream::Comment
		<< "#\n"
		<< "# POSIX makefile generated by Fabrique\n"
		<< "#\n"
		<< Bytestream::Action << ".POSIX:\n"
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
			<< Bytestream::Operator << "=" << indent_
			<< Bytestream::Literal << formatter.Format(*i.second)
			<< Bytestream::Reset
			<< "\n"
			;
	}
	out << "\n";


	//
	// Explicitly-named pseudo-targets, in reverse declaration order.
	//
	// Make treats the first target as the automatic target, whereas fabfiles
	// necessarily define subtargets first and top-level targets last.
	//
	const auto& topLevelTargets = dag.topLevelTargets();
	for (auto i = topLevelTargets.rbegin(); i != topLevelTargets.rend(); i++)
	{
		const string& name = i->first;
		const shared_ptr<Value>& target = i->second;

		if (dynamic_pointer_cast<Rule>(target))
			continue;

		out
			<< Bytestream::Definition << name
			<< Bytestream::Operator << " : "
			<< Bytestream::Literal
			<< formatter.Format(*target)
			<< "\n"
			;
	}

	out << "\n";


	// 'clean' target:
	out
		<< Bytestream::Definition << "clean"
		<< Bytestream::Operator << " :\n"
		<< Bytestream::Action << "\techo \"cleaning...\"\n"
		<< Bytestream::Action << "\trm -rf"
		;

	for (auto& f : dag.files())
		if (f->generated())
			out << " " << formatter.Format(*f);

	out << Bytestream::Reset << "\n\n";


	// Output directories:
	std::set<string> directories;
	for (auto& f : dag.files())
	{
		assert(f);

		if (not f->generated())
			continue;

		const string dir = f->directory();
		if (dir.empty())
			continue;

		directories.insert(dir);

		out
			<< Bytestream::Definition << formatter.Format(*f)
			<< Bytestream::Operator << " : | "
			<< Bytestream::Literal << dir
			<< Bytestream::Reset << "\n"
			;
	}


	// Build steps:
	out
		<< Bytestream::Comment
		<< "#\n"
		<< "# Build steps:\n"
		<< "#\n"
		<< Bytestream::Reset
		;

	for (const string& dir : directories)
	{
		out
			<< Bytestream::Definition << dir
			<< Bytestream::Operator << ":\n"
			<< indent_ << Bytestream::Action << CreateDirCommand(dir)
			<< Bytestream::Reset << "\n"
			;
	}

	StringMap<string> pseudoTargets;
	size_t buildID = 0;
	for (auto& i : dag.builds())
	{
		const dag::Build& build = *i;
		const dag::Rule& rule = build.buildRule();

		//
		// If the build generates multiple outputs, we need to define
		// a pseudo-target that points to all outputs.
		//

		Build::FileVec outputs = build.outputs();
		if (outputs.size() > 1)
		{
			const string pseudoName =
				rule.name() + "_" + std::to_string(++buildID);

			out
				<< Bytestream::Definition << pseudoName
				<< Bytestream::Operator << " :"
				;

			for (const shared_ptr<File>& f : outputs)
			{
				string name = formatter.Format(*f);
				pseudoTargets[name] = pseudoName;
				out << " " << name;
			}

			out << "\n";
		}

		//
		// target1 target2 : input1 input2 | dependency1 dependency 2
		//
		out << Bytestream::Definition;

		for (const shared_ptr<File>& f : outputs)
			out << formatter.Format(*f) << " ";

		out
			<< Bytestream::Operator << ":"
			<< Bytestream::Literal
			;

		for (const shared_ptr<File>& f : build.inputs())
			out << " " << formatter.Format(*f);


		//
		// Build the command to be run (substitute $variables).
		//
		string command = formatter.Format(rule);

		std::function<string (const shared_ptr<File>&)> shortName =
			[](const shared_ptr<File>& f) { return f->relativeName(); };

		string description =
			rule.hasDescription()
			? rule.description()
			: rule.name()
			  + " [ "
			  + join(build.inputs(), shortName, " ")
			  + " ] => [ "
			  + join(build.outputs(), shortName, " ")
			  + " ]"
			;

		for (auto& j : build.arguments())
		{
			const string name = j.first;
			const string str = formatter.Format(*j.second);

			replaceAll(command, "${" + name + "}", str);
			replaceAll(description, "${" + name + "}", str);
		}
		replaceAll(command, "${in}", build.inputs());
		replaceAll(description, "${in}", build.inputs());

		replaceAll(command, "${out}", build.outputs());
		replaceAll(description, "${out}", build.outputs());

		out
			<< "\n"
			<< indent_ << Bytestream::Action
			<< "echo \"" << description << "\"\n"
			<< indent_ << Bytestream::Action << command << "\n"
			;

		out << "\n";
	}
}


static int fabrique::replaceAll(string& haystack, const string& needle,
                                const string& replacement)
{
	int replaced = 0;

	while (true)
	{
		size_t i = haystack.find(needle);
		if (i == string::npos)
			break;

		haystack.replace(i, needle.length(), replacement);
	}

	return replaced;
}

static int fabrique::replaceAll(string& haystack, const string& pattern,
                                const Build::FileVec& files)
{
	std::ostringstream oss;

	for (const shared_ptr<File>& f : files)
		oss << f->fullName() << " ";

	string replacement = oss.str();
	replacement = replacement.substr(0, replacement.length() - 1);

	return replaceAll(haystack, pattern, replacement);
}


string MakeFormatter::Format(const Boolean& b)
{
	return (b.value() ? "true" : "false");
}

string MakeFormatter::Format(const Build&)
{
	assert(false && "called MakeFormatter::Format(Build&)");
}

string MakeFormatter::Format(const File& f)
{
	return f.fullName();
}

string MakeFormatter::Format(const Function&)
{
	return "";
}

string MakeFormatter::Format(const Integer& i)
{
	return std::to_string(i.value());
}

string MakeFormatter::Format(const List& l)
{
	vector<string> substrings;

	for (const shared_ptr<Value>& element : l)
		substrings.push_back(Format(*element));

	return fabrique::join(substrings, " ");
}

string MakeFormatter::Format(const Rule& rule)
{
	return rule.command();
}

string MakeFormatter::Format(const String& s)
{
	return s.value();
}

string MakeFormatter::Format(const Structure&)
{
	return "";
}

string MakeFormatter::Format(const Target& t)
{
	return Format(*t.files());
}
