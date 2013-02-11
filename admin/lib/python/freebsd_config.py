#!/usr/bin/env python

# utility to read a config file and return a dict.  Author: linimon.

# note that the config file is shared between /bin/sh scripts and
# Python programs.  If we ever wanted to deal with the {} expansions,
# we would have to include that here.  Note that this is NOT currently
# implemented.

from configobj import ConfigObj
import os

def getConfig( configDir, configSubdir, configFilename ):

    return getConfigFromFile( \
        os.path.join( \
            os.path.join( configDir, configSubdir ),  \
            configFilename ) )


def getConfigFromFile( configFile ):

    config = None
    try:
        config = ConfigObj( configFile )
        return config
    except Exception, e:
        printf( "getConfig: could not read config file %s", configFile )
        return None
