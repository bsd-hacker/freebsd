#!/usr/bin/env python
#
# Copyright (c) 2012 The FreeBSD Project. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE PROJECT ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE PROJECT BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

major = 0
minor = 0
micro = 0

import argparse
import logging
import re
import textwrap

def read_db(dbname, language):
    try:
        with open('%s.%s' % (dbname, language)) as f:
            logging.debug('Sucking in %s database' % dbname)
            contents = { }
            for e in f.readlines():
                if not e or e[0] == '#':
                    continue
                e = e.split('	')
                contents[e[0]] = e[1:]
    except:
        logging.error('Cannot open %s database for language %s' % (dbname, language))
        exit()
    return contents

def explain(error):
    if verbosity > 0:
        print textwrap.fill(error[1],
                            initial_indent='==> ', subsequent_indent='    ')

def error(type, line_number, filename):
    logging.error("[%d]%s: %s " % (line_number + 1, filename, errors[type][0]))
    explain(errors[type])

def check_quoted(string):
    return True if string[0] == '"' or string[0] == "'" else False

def mandatory(var):
    mand = ['enable']
    return True if var.split('_')[-1] in mand else False

def get_value(var, line):
    try:
        return re.match('%s=(\S+)$' % var, line).group(1)
    except:
        return False

def get_assignment(line):
    try:
        return re.match('(\S+)=(.+)$', line).groups()
    except:
        return False

def check_header(lines, num, filename):
    # Basic order; shebang, copyright, RCSId, gap, rcorder

    logging.debug('Check shebang')
    if lines[num] != '#!/bin/sh':
        error('shebang', num, filename)

    logging.debug('Skipping license')
    num += 1
    while (not re.match('# \$FreeBSD[:$]', lines[num]) and
          (lines[num] == '' or lines[num][0] == '#')):
        num += 1

    logging.debug('Checking for RCSId')
    if not re.match('# \$FreeBSD[:$]', lines[num]):
        error('rcsid', num, filename)

    num += 1
    while lines[num] == '#' or lines[num] == '': num += 1

    logging.debug('Checking rcorder order and sucking in names')
    if not re.match('# [PRBK]', lines[num]):
        error('rcorder_missing', num, filename)
    orders = ['provide', 'require', 'before', 'keyword']
    index = 0
    rcorder = {o: [] for o in orders}
    while index < 4:
        order = orders[index]
        try:
            for result in re.match('# %s: (.*)' % order.upper(),
                                   lines[num]).group(1).split(' '):
                rcorder[order].append(result)
            num += 1
        except:
            index += 1

    if 'FreeBSD' in rcorder['keyword']:
        error('rcorder_keyword_freebsd', num, filename)

    if re.match('# [PRBK]', lines[num]):
        error('rcorder_order', num, filename)

    return num

def check_intro(lines, num, filename):
    logging.debug('Checking sourcing lines')
    while lines[num] == '' or lines[num][0] == '#':
        num += 1

    if lines[num] != '. /etc/rc.subr':
        error('rc_subr_late', num, filename)

    logging.debug('Checking name assignment')
    while lines[num] == '' or lines[num][0] == '#' or lines[num][0] == '.':
        num += 1

    name = get_value('name', lines[num])
    if not name:
        error('name_missing', num, filename)
    elif check_quoted(name):
        error('name_quoted', num, filename)
    else:
        logging.debug('name discovered as %s' % name)
        num += 1

    logging.debug('Checking rcvar')
    rcvar = get_value('rcvar', lines[num])
    logging.debug('rcvar discovered as %s' % rcvar if rcvar else 'rcvar not discovered')

    if not rcvar:
        error('rcvar_missing', num, filename)
    elif check_quoted(rcvar):
        error('rcvar_quoted', num, filename)
    elif rcvar != '%s_enable' % name:
        error('rcvar_incorrect', num, filename)
    else:
        num += 1

    logging.debug('Checking load_rc_config')
    if lines[num] == '':
        num += 1
    else:
        error('rcvar_extra', num, filename)

    if re.match('load_rc_config (?:\$name|%s)' % name, lines[num]):
        num += 1
    else:
        error('load_rc_config_missing', num, filename)

    if lines[num] == '':
        num += 1
    else:
        error('load_rc_config_extra', num, filename)

    return num

def check_defaults(lines, num, filename):
    logging.debug('Checking defaults set')

    default = { }
    try:
        while lines[num]:
            while lines[num][0] == '#' or lines[num][:4] == 'eval':
                num += 1
            if lines[num][0] == ':':
                # Shorthand set self-default assignment
                (target, operator, value) = re.match(': \${([^:=]+)(:?=)([^}]+)}', lines[num]).groups()
                if operator == ':=' and not mandatory(target):
                    error('defaults_non_mandatory_colon', num, filename)
                elif operator == '=' and mandatory(target):
                    error('defaults_mandatory_colon', num, filename)

            else:
                # Longhand set default assignment
                (target, source, operator, value) = re.match('([^=]+)=\${([^:-]+)(:?-)([^}]+)}', lines[num]).groups()
                if target == source:
                    error('defaults_old_style', num, filename)

                if operator == ':-' and not mandatory(target):
                    error('defaults_non_mandatory_colon', num, filename)
                elif operator == '-' and mandatory(target):
                    error('defaults_mandatory_colon', num, filename)

            if check_quoted(value):
                error('defaults_value_quoted', num, filename)

            num += 1
    except:
        error('defaults_invalid', num, filename)

    # Allow line breaks in the middle; if we put
    # gaps in that's usually good style.  Lookahead!
    if lines[num+1] and (':' in lines[num+1] or '-' in lines[num+1]):
        return check_defaults(lines, num, filename)
    else:
        return num

def check_definitions(lines, num, filename):
    logging.debug('Checking the basic definitions')

    num += 1

    while lines[num]:
        while lines[num][0] == '#' or lines[num][:4] == 'eval': num += 1
        try:
            (var, value) = get_assignment(lines[num])
        except:
            error('definitions_missing', num, filename)
            return num

        if check_quoted(value):
            if ' ' not in lines[num] and '	' not in lines[num]:
                error('definitions_quoted', num, filename)
        num += 1

    # As in check_defaults, allow line breaks in the middle; if we put
    # gaps in that's usually good style.  Lookahead!
    if lines[num+1] and '=' in lines[num+1]:
        return check_definitions(lines, num, filename)
    else:
        return num

def check_functions(lines, num, filename):
    logging.debug('Now checking functions')

    num += 1
    func = { }

    while lines[num]:
        while not lines[num] or lines[num][0] == '#':
            num += 1

        if lines[num][-2:] != '()':
            if lines[num][-1] == '{':
                error('functions_brace_inline', num, filename)
            else:
                logging.debug('No functions left!')
                return num
        if ' ' in lines[num]:
            error('functions_spaces', num, filename)
        func['name'] = lines[num][:-2]
        func['length'] = 0

        num += 1
        if lines[num] != '{':
            error('functions_brace_missing', num, filename)

        num += 1

        try:
            while lines[num] != '}':
                print lines[num]
                num += 1
                if lines[num] and lines[num][0] != '#':
                    func['length'] += 1 
        except:
            error('functions_neverending', num, filename)
            return num

        if func['length'] == 1:
            error('functions_short', num, filename)

    print lines[num]
    print lines[num+1]
    print lines[num+2]

    if lines[num+2] and '{' in lines[num+2]:
        return check_functions(lines, num, filename)
    else:
        return num

def check_run_rc(lines, num, filename):
    logging.debug('Checking the last line of the file contains run_rc_command')

    while not lines[num] or lines[num][0] == '#':
        num += 1

    try:
        arg = re.match('run_rc_command (.*)$', lines[num]).group(1)
        if check_quoted(arg):
            error('run_rc_quoted', num, filename)
        elif arg != r'$1':
            error('run_rc_argument', num, filename)

        if num < len(lines) - 1:
            error('run_rc_followed', num, filename)
    except:
        error('run_rc_cruft', num, filename)

def general_checks(lines, filename):
    logging.debug('Checking for unrecommended sequences and orphan commands')
    # Don't use regex on last line, it must contain run_rc_command, which fails
    # unassociated shell command test.  We already hacked for load_rc_config
    for num in range(0, len(lines) - 1):
        for regex in problems.keys():
            if lines[num] and re.search(regex, lines[num]):
                logging.warn("[%d]%s: %s " % (num + 1, filename,
                             problems[regex][0]))
                explain(problems[regex])

def do_rclint(filename):
    logging.debug('Suck in file %s' % filename)
    try:
        lines=[line.rstrip('\n') for line in open(filename)]
    except: 
        logging.error('Cannot open %s for testing' % filename)
        return
    lineno = {'begin': 0}

    lineno['header'] = check_header(lines, lineno['begin'], filename)
    lineno['intro'] = check_intro(lines, lineno['header'], filename)
    lineno['defaults'] = check_defaults(lines, lineno['intro'], filename)
    lineno['definitions'] = check_definitions(lines, lineno['defaults'], filename)
    lineno['functions'] = check_functions(lines, lineno['definitions'], filename)
    check_run_rc(lines, lineno['functions'], filename)

    general_checks(lines, filename)

parser = argparse.ArgumentParser()
parser.add_argument('filenames', nargs = '+')
parser.add_argument('--language', nargs = 1, type=str, default = ['en'], help = 'sets the language that errors are reported in')
parser.add_argument('-v', action='count', help='raises debug level; provides detailed explanations of errors')

args = parser.parse_args()

verbosity = args.v
logging.basicConfig(level=logging.DEBUG if verbosity > 1 else logging.WARN)

errors = read_db('errors', args.language[0])
problems = read_db('problems', args.language[0])

for file in args.filenames:
    do_rclint(file)
