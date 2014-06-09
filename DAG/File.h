/** @file DAG/File.h    Declaration of @ref fabrique::dag::File. */
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

#ifndef DAG_FILE_H
#define DAG_FILE_H

#include "DAG/Value.h"

#include <string>


namespace fabrique {
namespace dag {

/**
 * A reference to a file on disk (source or target).
 */
class File : public Value
{
public:
	static File* Create(std::string fullPath, const Type&, SourceRange);
	static File* Create(std::string directory, std::string filename,
	                    const Type&, SourceRange);
	virtual ~File() {}

	virtual std::string filename() const;
	virtual std::string relativeName() const;
	virtual std::string fullName() const;
	virtual std::string str() const { return filename_; }

	//! This file refers to an absolute path.
	bool absolute() const { return absolute_; }

	bool generated() const { return generated_; }
	void setGenerated(bool);

	//! Absolute path to the directory this file is in.
	std::string directory() const;
	std::string subdirectory() const { return subdirectory_; }
	void setSubdirectory(std::string subdir) { subdirectory_ = subdir; }

	//! Name a file with my name + a suffix.
	virtual ValuePtr Add(ValuePtr&) const override;

	//! Name a file with a prefix + my name.
	virtual ValuePtr PrefixWith(ValuePtr&) const override;

	virtual void PrettyPrint(Bytestream&, size_t indent = 0) const override;
	void Accept(Visitor&) const override;

private:
	File(std::string name, std::string subdirectory, bool absolute,
	     const Type&, SourceRange);

	const std::string filename_;
	std::string subdirectory_;
	const bool absolute_;
	bool generated_;
};

} // namespace dag
} // namespace fabrique

#endif
