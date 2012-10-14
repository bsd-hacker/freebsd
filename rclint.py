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

def error_explain(error):
	if verbosity > 0: print error['explanation']

def error(type, line_number, filename):
	logging.error("[%d]%s: %s " % (line_number, filename, errors[type]['message']))
	error_explain(errors[type])

def check_quoted(string):
	return True if string[0] == '"' or string[0] == "'" else False

def get_value(var, line):
	n = re.match('%s=(\S*)$' % var, line)
	if n:
		value=n.group(1)
		return value
	else:
		return False

def do_rclint(filename):
	logging.debug('Suck in file %s' % filename)
	lines=[line.rstrip('\n') for line in open(filename)]
	num = 0

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
	rcorder = { o: [] for o in orders }
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

	documentation_line = num

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
	

parser = argparse.ArgumentParser()
parser.add_argument('filenames', nargs = '+')
parser.add_argument('--language', nargs = 1, type=str, default = ['en'], help = 'sets the language that errors are reported in')
parser.add_argument('-v', action='count', help='raises debug level; provides detailed explanations of errors')

args = parser.parse_args()

verbosity = args.v
errordb = 'errors.%s' % args.language[0]

logging.basicConfig(level=(logging.DEBUG if verbosity > 1 else logging.ERROR))

try:
	with open(errordb) as f:
		logging.debug('Sucking in error database')
		errors = {}
		for e in f.readlines():
			if e[0] == '#' or len(e) == 1:
				continue
			e = e.split(':')
			errors[e[0]] = { 'message': e[1], 'explanation': e[2] }
except:
	logging.error('Cannot open database for language %s' % errordb)
	exit()

for file in args.filenames:
	do_rclint(file)
