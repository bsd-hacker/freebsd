#! /usr/bin/env python
import optparse
import re
import string
import sys

class Line(str):
    """String that carries a line number with it"""
    @property
    def lineNumber(self):
        "Line number where this line first occurred in the file"
        try:
            return self._lineNumber
        except AttributeError:
            return -1

    @lineNumber.setter
    def lineNumber(self, value):
        self._lineNumber = value


class FileScanner(object):
    """Scans a file for a regex"""
    antipattern = ''
    strip_comments = False
    flags = 0
    def __init__(self):
        self._reobj = re.compile(self.pattern, self.flags)
        if self.antipattern:
            self.__antipattern_obj = re.compile(self.antipattern, self.flags)
        self.__comment_reobj = re.compile(
            r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
            re.DOTALL | re.MULTILINE
        )

    def _readlines(self, filename):
        """Return an iterable that will yield each line in file, stripping
        comments if self.strip_comments is set"""
        f = open(filename, 'r')
        if self.strip_comments:
            reader = self._strip_comments(f.read()).split('\n')
        else:
            reader = f
        n = 1
        for l in reader:
            line = Line(l)
            line.lineNumber = n
            yield line
            n += 1

    def __replacer(self, match):
        s = match.group(0)
        if s.startswith('/'):
            return "" + "\n" * s.count('\n')
        else:
            return s

    def _strip_comments(self, text):
        #Thanks to MizardX on stackoverflow.com
        return re.sub(self.__comment_reobj, self.__replacer, text)

    def finditer(self, filename):
        """Returns a generator yielding every matching line in the file"""
        f = self._readlines(filename)
        for line in f:
            mo = self._reobj.search(line)
            if mo:
                if not self.antipattern or not self.__antipattern_obj.search(line):
                    yield line

    def has_match(self, filename):
        """Returns True iff at least one line in the file matches the pattern"""
        f = self._readlines(filename)
        for line in f:
            mo = self._reobj.search(line)
            if mo:
                if not self.antipattern or not self.__antipattern_obj.search(line):
                    return True
                    break
        return False


class Attribute(FileScanner):
    """Use of __attribute__ outside of compiler-specific macro"""
    pattern = '__attribute__'


class BraceSolo(FileScanner):
    """Puts the opening brace of a control statement on its own line"""
    strip_comments = True
    pattern = r'^\s+{\s*$'


class BraceWithoutSpace(FileScanner):
    """Omits a space before the { of a control block"""
    strip_comments = True
    pattern = r'[^a-zA-Z0-9_](if|while|for|switch)(\(|[^a-zA-Z0-9_;(]\S*\s*\().*\){'


class CppComment(FileScanner):
    """C++ style comments"""
    # Assume that if the first non-whitespace character of a line is '*', then
    # the line is probably a continuation of a C-style comment
    pattern = r'//'
    antipattern = r'^\s*/?\*'


class EmptyLoopSemicolon(FileScanner):
    """Puts space before the ; of a loop with no body"""
    strip_comments = True
    pattern = r'[^a-zA-Z0-9_](while|for)\s*\(.*\)\s+;'


class FuncInDeclaration(FileScanner):
    """Calls a function in a variable declaration"""
    strip_comments = True
    pattern = r'^(\s+[a-zA-Z0-9_]+){2,}\s*=.*[a-zA-Z0-9_]\('


class FuncOnSameLine(FileScanner):
    """Function names on the same line as their return types in a definition"""
    strip_comments = True
    flags = re.MULTILINE | re.DOTALL
    pattern = r'^[a-zA-Z0-9_][a-zA-Z0-9_ \t\r\f\v*]+[ \t\r\f\v]+\*?[a-zA-Z0-9_]+\([^)]*\)[{\t\r\f\v]*$'

    def finditer(self, filename):
        """Returns a generator yielding every matching line in the file"""
        file_as_str = self._strip_comments(open(filename, 'r').read())
        for mo in re.finditer(self._reobj, file_as_str):
            line = Line(mo.group(0))
            line.lineNumber = file_as_str[0:mo.start(0)].count('\n') + 1
            yield line

    def has_match(self, filename):
        """Returns True iff at least one line in the file matches the pattern"""
        file_as_str = self._strip_comments(open(filename, 'r').read())
        mo = self._reobj.search(file_as_str)
        if mo:
            return True
        else:
            return False


class InlineStatic(FileScanner):
    """Uses "inline static" instead of "static inline" """
    strip_comments = True
    pattern = r'inline static'


class LongLines(FileScanner):
    """Lines over 80 characters"""
    def __init__(self):
        pass

    def has_match(self, filename):
        """Returns True iff at least one line in the file matches the pattern"""
        f = open(filename, 'r')
        for line in f:
            expanded_line = string.expandtabs(line, 8) #re.sub('\t', '        ', line)
            if len(expanded_line) > 81:     #allow one character for the newline
                return True
                break
        return False

    def finditer(self, filename):
        """Returns a generator yielding every matching line in the file"""
        for line in self._readlines(filename):
            expanded_line = string.expandtabs(line, 8) #re.sub('\t', '        ', line)
            if len(expanded_line) > 81:     #allow one character for the newline
                yield line


class MultilineFirst(FileScanner):
    """Multiline comments with text on the first line"""
    pattern = r'/\*.*\S+.*[^/\n]$'


class MultilineLast(FileScanner):
    """Multiline comments with text on the last line"""
    pattern = r'[a-zA-Z0-9_]+.*\*/'
    antipattern = r'/\*'


class ReturnParens(FileScanner):
    """Return statements must enclose their values in parenthesis"""
    strip_comments = True
    pattern = r'[^a-zA-Z0-9_]return\s*[^(; \t\n]'

class RequireBraces(FileScanner):
    """Always uses braces after a control statement"""
    strip_comments = True
    pattern = '^((.*\s+((if|else|elseif|for|do|switch))|([^}]*\s+while))\s*\(.*)$'

    def finditer(self, filename):
        f = self._readlines(filename)
        for line in f:
            ctlStart = re.search(self.pattern, line)
            if ctlStart:
                curLine = ctlStart.group(0)
                lParen = curLine.count('(')
                rParen = curLine.count(')')
                while lParen != rParen:
                    curLine = f.next()
                    lParen += curLine.count('(')
                    rParen += curLine.count(')')
                if not '{' in curLine:
                    yield line

    def has_match(self, filename):
        while self.finditer(filename):
            return True
        return False


class SpaceAfterKeyword(FileScanner):
    """Omit a space after flow control keywords"""
    strip_comments = True
    pattern = r'[^a-zA-Z0-9_](if|while|for|return|switch|case)[^a-zA-Z0-9_ \t\r\f\v;]'


class TrailingWhitespace(FileScanner):
    """Trailing whitespace"""
    pattern = r'[ \t\r\f\v]$'


class UnnecessaryBraces(FileScanner):
    """ Braces around a single line statement following a control block """
    pattern = '\s+(if|else|elseif|for|while|do|switch)\s*\(.*{'
    ENDPATTERN = '^\s+}'

    def __init__(self):
        self.strip_comments = True
        self.ctlBlockLinesStack = []
        self.ctlBlockLines = 0
        super(UnnecessaryBraces, self).__init__()

    def finditer(self, filename):
        f = self._readlines(filename)
        for line in f:
            if re.search(self.pattern, line):
                # Started a new level of control block
                self.ctlBlockLinesStack.append(self.ctlBlockLines)
                self.ctlBlockLines = 0

            elif re.search(self.ENDPATTERN, line):
                # Ended a control block
                if self.ctlBlockLines == 1:
                    yield line
                if self.ctlBlockLinesStack:
                    self.ctlBlockLines += self.ctlBlockLinesStack.pop()
            else:
                # Just a regular line
                self.ctlBlockLines += 1

    def has_match(self, filename):
        while self.finditer(filename):
            return True
        return False

def main():
    usage = "usage: %prog [OPTIONS] FILE [FILE ...]"
    parser = optparse.OptionParser(usage)
    parser.add_option('-a', '--all', action="store_true", default=False,
        help="Run all checks");
    parser.add_option('-v', '--verbose', action="store_true", default=False,
        help="Print matching lines as well as filenames");

    parser.add_option('--attribute', action="store_true", default=False,
            help="Check for " + Attribute.__doc__);
    parser.add_option('--brace-solo', action="store_true", default=False,
            help="Check for " + BraceSolo.__doc__);
    parser.add_option('--brace-without-space', action="store_true",
            default=False, help="Check for " + BraceWithoutSpace.__doc__);
    parser.add_option('--cpp-comments', action="store_true", default=False,
            help="Check for " + CppComment.__doc__);
    parser.add_option('--empty-loop-semicolon', action="store_true",
            default=False, help="Check for " + EmptyLoopSemicolon.__doc__);
    parser.add_option('--func-in-declaration', action="store_true",
            default=False, help="Check for " + FuncInDeclaration.__doc__);
    parser.add_option('--func-on-same-line', action="store_true",
            default=False, help="Check for " + FuncOnSameLine.__doc__);
    parser.add_option('--inline-static', action="store_true", default=False,
            help="Check for " + InlineStatic.__doc__);
    parser.add_option('--long-lines', action="store_true", default=False,
            help="Check for " + LongLines.__doc__);
    parser.add_option('--multiline-first', action="store_true", default=False,
            help="Check for " + MultilineFirst.__doc__);
    parser.add_option('--multiline-last', action="store_true", default=False,
            help="Check for " + MultilineLast.__doc__);
    parser.add_option('--return-parens', action="store_true",
            default=False, help="Check for " + ReturnParens.__doc__);
    parser.add_option('--space-after-keyword', action="store_true",
            default=False, help="Check for " + SpaceAfterKeyword.__doc__);
    parser.add_option('--trailing-whitespace', action="store_true",
            default=False, help="Check for " + TrailingWhitespace.__doc__);
    parser.add_option('--unnecessary-braces', action="store_true",
            default=False, help="Check for " + UnnecessaryBraces.__doc__);
    parser.add_option('--require-braces', action="store_true",
            default=False, help="Check for " + RequireBraces.__doc__);
    
    (options, args) = parser.parse_args()
    if len(args) < 1:
        parser.error("Incorrect number of arguments")

    checks = []
    if options.all or options.attribute:
        checks.append(Attribute())
    if options.all or options.brace_solo:
        checks.append(BraceSolo())
    if options.all or options.brace_without_space:
        checks.append(BraceWithoutSpace())
    if options.all or options.cpp_comments:
        checks.append(CppComment())
    if options.all or options.empty_loop_semicolon:
        checks.append(EmptyLoopSemicolon())
    if options.all or options.func_in_declaration:
        checks.append(FuncInDeclaration())
    if options.all or options.inline_static:
        checks.append(FuncOnSameLine())
    if options.all or options.inline_static:
        checks.append(InlineStatic())
    if options.all or options.long_lines:
        checks.append(LongLines())
    if options.all or options.multiline_first:
        checks.append(MultilineFirst())
    if options.all or options.multiline_last:
        checks.append(MultilineLast())
    if options.all or options.return_parens:
        checks.append(ReturnParens())
    if options.all or options.space_after_keyword:
        checks.append(SpaceAfterKeyword())
    if options.all or options.trailing_whitespace:
        checks.append(TrailingWhitespace())
    if (options.all or options.unnecessary_braces) and not options.require_braces:
        checks.append(UnnecessaryBraces())
    if options.require_braces:
        checks.append(RequireBraces())

    for check in checks:
        #TODO: don't print check.desc if it has no matches
        print "*** " + check.__doc__ + " ***"
        for filename in args:
            if options.verbose:
                for line in check.finditer(filename):
                    print "%s:%d:%s" % (filename, line.lineNumber, line)
            else:
                if check.has_match(filename):
                    print filename


if __name__ == '__main__':
    main()
