#
# try doing some SQL reads as a test
#

import sys, os, threading, socket, Queue 

from signal import *
from sys import exc_info
from itertools import chain

sys.path.insert(0, '/var/portbuild/lib/python')

from freebsd_config import *

from qmanagerobj import *

CONFIG_DIR="/var/portbuild"
CONFIG_SUBDIR="conf"
CONFIG_FILENAME="server.conf"

# pieces of qmanagerobj.startup
def obj_startup(filename):

    engine = create_engine('sqlite:///' + filename, echo=True)
    Session = sessionmaker(bind=engine)
    session = Session()

    Base.metadata.create_all(engine)

    return (engine, session)


def show_acl( session ):

    acls = session.query(QManagerACL)
    acls = acls.order_by('name')

    print
    print 'starting dump of acl table:'
    print

    for acl in acls:

        print
        print "name: %s" % acl.name
        # list
        print "uidlist: " + str( acl.uidlist )
        # list
        print "gidlist: " + str( acl.gidlist )
        print "sense: " + str( acl.sense )


def show_jobs( session ):

    jobs = session.query(Job)
    jobs = jobs.order_by('id')

    print
    print 'starting dump of Job table:'
    print

    for job in jobs:

        print
        # job ID
        print "job id: " + `job.id`
        # Name of job
        print "name: " + job.name
        # priority of request
        print "priority: " + `job.priority`
        # job type
        print "type: " + job.type
        # uid of job owner
        print "owner: " + `job.owner`
        # gids of job owner (tuple)
        #print str( type( job.gids ) )
        print "gids: " + str( job.gids )
        # machines that satisfied initial query (list)
        #print str( type( job.machines ) )
        print "machines: " + str( job.machines )
        # Time job started/blocked (must not be modified when job is
        # blocked or it will corrupt the heapq)
        print "startttime: " + `job.starttime`
        # initial machine description in case we have to revalidate (list)
        # print str( type( job.mdl ) )
        print "mdl: " + str( job.mdl )
        # True --> job is running; False --> job is blocked
        print "running: " + str( job.running )


def show_machines( session ):

    machines = session.query(Machine)
    machines = machines.order_by('name')

    print
    print 'starting dump of Machines table:'
    print

    for machine in machines:

        print
        print "name: %s" % machine.name
        # list
        print "acl: " + str( machine.acl )
        # boolean
        print "haszfs: " + str( machine.haszfs )
        # boolean
        print "online: " + str( machine.online )


def show_machines_for_arch( engine, arch ):

    mdl = ["arch = %s" % arch]

    q = SQL.construct(Machine, mdl)
    res = engine.execute(Machine.__table__.select(q))
    result = [SQL.to_dict(Machine, i) for i in res]

    print
    for machine in result:
        print "machine for %s : %s " % ( arch, machine[ 'name' ] )


# main

if __name__ == '__main__':

    print "acquiring engine and session"
    config = getConfig( CONFIG_DIR, CONFIG_SUBDIR, CONFIG_FILENAME )
    QMANAGER_PATH = config.get( 'QMANAGER_PATH' )
    QMANAGER_DATABASE_FILE = config.get( 'QMANAGER_DATABASE_FILE' )
    (engine, session) = obj_startup( \
        os.path.join( QMANAGER_PATH, QMANAGER_DATABASE_FILE ) )
    print "acquired engine and session"
    # print "engine = '" + str( engine ) + "', session = '" + str( session ) + "'"

    show_acl( session )
    show_machines( session )
    show_jobs( session )

    show_machines_for_arch( engine, 'i386' )
    show_machines_for_arch( engine, 'amd64' )
