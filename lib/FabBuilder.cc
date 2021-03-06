//! @file FabBuilder.cc    Definition of the FabBuilder class
/*
 * Copyright (c) 2018 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme and at Memorial University
 * of Newfoundland under the NSERC Discovery program (RGPIN-2015-06048).
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
#include <fabrique/FabBuilder.hh>
#include <fabrique/platform/files.hh>

using namespace fabrique;


static void DefaultErrorHandler(ErrorReport);


FabBuilder::FabBuilder()
	: err_(DefaultErrorHandler)
{
}


FabBuilder& FabBuilder::backends(std::vector<std::string> backendNames)
{
	for (const std::string& name : backendNames)
	{
		backends_.emplace_back(backend::Backend::Create(name));
	}
	return *this;
}


FabBuilder& FabBuilder::outputDirectory(std::string d)
{
	outputDir_ = platform::AbsoluteDirectory(d, true);
	return *this;
}

Fabrique FabBuilder::build()
{
	return Fabrique(parseOnly_, printASTs_, dumpASTs_, printDAG_, stdout_,
	                std::move(backends_), outputDir_, std::move(pluginPaths_),
	                regenCommand_, err_);
}


static void DefaultErrorHandler(ErrorReport r)
{
	Bytestream::Stderr() << r << "\n";
}
