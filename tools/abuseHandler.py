"""Go through a set of submitted PRs in a directory and rename
   them for manual handling if they meet certain spammy criteria.
   Original author: Mark Linimon, Lonesome Dove Computing Services,
   www.lonesome.com.  License: BSD license.
   $FreeBSD$
"""

__version__ = "0.02"

__all__ = ["abuseHandler"]

import os
import string
import sys

QUEUE_DIR = "/home/gnats/gnats-queue"

HEADER_SYNOPSIS  = ">Synopsis:"
HEADER_ORGINATOR = ">Orginator:"
HEADER_RELEASE   = ">Release:"

INTERESTING_HEADERS = ( \
    HEADER_SYNOPSIS, \
    HEADER_ORGINATOR, \
    HEADER_RELEASE
)

ABUSED_HEADERS = ( \
    HEADER_SYNOPSIS, \
    HEADER_ORGINATOR, \
    HEADER_RELEASE
)

ABUSIVE_ORIGINATORS = ( \
    "Perry Keller", \
)

IGNORE_FILE_PREFIX = "."

LINELEN = 1024        # try to prevent buffer overruns when reading files

DEBUG_LINE         = 0
DEBUG_VERBOSE      = 0
DEBUG_VERY_VERBOSE = 0


# look for all N headers in ABUSED_HEADERS existing, and having the
# same value.
def checkForAbusiveHeaders( headers ):

    compare = None
    for header in ABUSED_HEADERS:
        try:
            if DEBUG_VERY_VERBOSE:
                print "examining header %s" % header
            value = headers[ header ]
            if compare == None:
                compare = value
            else:
                if compare != value:
                    if DEBUG_VERBOSE:
                        print "file is ok: %s != %s" % ( compare, value )
                    return False
        except:
            if DEBUG_VERBOSE:
                print "file is ok: no value for %s" % header
            return False

    return True


def checkForAbusiveOriginators( headers ):

    for originator in ABUSIVE_ORIGINATORS:
        try:
            if orginator == headers[ HEADER_ORGINATOR ]:
                return True
        except:
            return False

    return False


def renamingScheme( infile ):

    # hard-coded by GNATS
    return IGNORE_FILE_PREFIX + infile


def renameFile( filename ):

    newfilename = renamingScheme( filename )
    try:
        os.rename( filename, renamingScheme( filename ) )
        print "suspicious queued PR %s" % filename + \
            " renamed to %s for manual inspection" % newfilename
    except Exception, e:
        print "could not rename suspected spam PR %s:" % filename
        print str( e )
        pass


def handleFiles( dir ):

    try:
        # dunno why I have to do this, but rename fails with a full pathname
        os.chdir( dir )
        filenames = os.listdir( "." )
        for filename in filenames:
            if string.find( filename, IGNORE_FILE_PREFIX ) == 0:
                if DEBUG_VERBOSE:
                    print "skippping file %s" % filename
            else:
                if DEBUG_VERBOSE:
                    print "handling file %s" % filename
                try:
                    handleFile( filename )
                except Exception, e:
                    print "could not handle file %s:" % filename
                    print str( e )
    except Exception, e:
        print "could not list directory %s:" % dir
        print str( e )


def handleFile( filename ):

    infile = None
    try:
        infile = file( filename )
    except Exception, e:
        print "could not open %s:" % filename
        print str( e )
        return

    headers = {}

    while 1:
        try:
            line = infile.readline( LINELEN )
            if len( line ) == 0:
                break
            else:
                if DEBUG_LINE:
                    print 'line: ' + line

                tokens = string.split( line )
                if len( tokens ) > 1:
                    # TODO add more analysis in the future?
                    header = tokens[ 0 ]
                    if header in INTERESTING_HEADERS:
                        headers[ header ] = tokens[ 1 : ]

            # endif len( line ) == 0

        except EOFError:
            break
        except IOError, e:
            print 'handleFile: IOError:'
            print e
            break
    # end while 1 (read line)

    try:
        infile.close()
    except:
        pass

    # run algorithms and use results to determine whether to rename file
    hasAbusiveHeaders = checkForAbusiveHeaders( headers )
    hasAbusiveOriginator = checkForAbusiveOriginators( headers )

    # TODO add more tests as appropriate in the future

    if hasAbusiveHeaders or hasAbusiveOriginator:
        renameFile( filename )


# main

if __name__ == '__main__':

    dir = QUEUE_DIR

    if len( sys.argv ) > 1:
        dir = sys.argv[ 1 ]

    handleFiles( dir )

