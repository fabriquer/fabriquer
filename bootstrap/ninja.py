#!/usr/bin/env python3
#
# Copyright (c) 2014-2016, 2018-2019 Jonathan Anderson
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
# ("CTSRD"), as part of the DARPA CRASH research programme and at Memorial University
# of Newfoundland under the NSERC Discovery program (RGPIN-2015-06048).
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#
import itertools
import os

from build import BootstrapBuild


class NinjaBuild(BootstrapBuild):
    # Build rules: how we actually build things.
    rules = {
        'bin': {
            'command': '$cxx $ldflags -o $out $in',
            'description': 'Linking $out',
        },

        'cc': {
            'command': '$cc -c $cflags -MMD -MT $out -MF $out.d -o $out $in',
            'description': 'Compiling $in',
            'depfile': '$out.d',
        },

        'cxx': {
            'command': '$cxx -c $cxxflags -MMD -MT $out -MF $out.d -o $out $in',
            'description': 'Compiling $in',
            'depfile': '$out.d',
        },

        'lib': {
            'command': '$cxx -shared -o $out $ldflags $in',
            'description': 'Linking library $out',
        },

        'rebuild': {
            'command': '$python $in $args',
            'description': 'Regenerating $out',
            'generator': '',
        },
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def write(self, out):
        out.write('# Generated by Fabrique bootstrap tool\n\n')

        # Ninja variables look a lot like variables in any other language: key = val
        for (name, tool) in self.tools.items():
            out.write(f'{name} = {tool}\n')

        out.write(f'cxxflags = {" ".join(self.all_cxxflags())}\n')
        out.write('\n')

        # Describe how to build things (cc, cxx, etc.)
        for (name, variables) in self.rules.items():
            out.write(f'rule {name}\n')
            for (key, val) in variables.items():
                out.write(f'  {key} = {val}\n')
            out.write('\n')

        # Ninja file regeneration
        if self.regen:
            (regen_script, regen_args) = self.regen
            out.write(f'''
build build.ninja: rebuild {regen_script}
    args = {' '.join(regen_args)}

''')

        # Finally, describe what to build
        objs = [(src, src+'.o') for src in self.sources]

        # The main executable
        binary = self.dirs['bin'] + 'fab' + self.suffixes['exe']
        out.write(f'''
build fab: phony {binary}
build {binary}: bin {" ".join((o[1] for o in objs))}
default fab

''')

        # Object files built as part of the main executable
        src_root = self.dirs['src']
        for src, obj in objs:
            out.write(f'build {obj}: cxx {os.path.join(src_root, src)}\n')

        # Libraries that aren't part of the main executable
        for (name, sources) in self.libraries:
            library_objs = [(src, src+'.o') for src in sources]

            out.write(f'build {name}: lib {" ".join((o[1] for o in library_objs))}\n')
            out.write(f'default {name}\n')

            for src, obj in library_objs:
                out.write(f'build {obj}: cxx {os.path.join(src_root, src)}\n')
