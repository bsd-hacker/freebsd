# Objects for queue manager

#from traceback import print_stack

from acl import *

from random import shuffle
from heapq import heappush, heappop, heapify
from time import time

from sqlalchemy import Table, Column, Integer, String, ForeignKey, PickleType, Boolean
from sqlalchemy.sql.expression import literal, and_

from sqlalchemy.ext.declarative import declarative_base

Base = declarative_base()

machines = {}		# Machines by name
jobs = {}		# Registered jobs by ID (running/blocked)
acls = {}		# ACL elements by name

from sqlalchemy.orm import sessionmaker
from sqlalchemy import create_engine

from qmanagerhandler import *

DEBUG = False

def startup(filename):
    """ Initialize from backing store on startup """

    global engine, session

    engine = create_engine('sqlite:///' + filename, echo=False)
    Session = sessionmaker(bind=engine)
    session = Session()

    Base.metadata.create_all(engine)

    # Read existing objects and restore local state
    for acl in session.query(QManagerACL):
        acl.setup()
        acls[acl.name] = acl

    for mach in session.query(Machine):
        print "Adding machine %s" % mach.name
        mach.setup()
        machines[mach.name] = mach

    # XXX MCL I don't think this ever works.
#    for job in session.query(Job):
#        job.setup()
#        jobs[job.id] = job
#
#        if job.running:
#            m = machines[job.machines[0]]
#            print "Running job %s on %s" % (job.id, m.name)
#            m.run(job, incr = 1)
#        else:
#            print "Blocking job %s on %s" % (job.id, job.machines)
#            mlist = [machines[m] for m in job.machines]
#            for m in mlist:
#                m.block(job)
#
#    Job.revalidate_blocked()

    for job in session.query(Job):
	try:
            print "Deleting job %s" % job.id
            session.delete(job)
            job.commit()
            print "Deleted job %s" % job.id
	except Exception, e:
            print "Could not delete job:"
            print e

    return (engine, session)


class SQL(object):
    """ Parse a client job description and turn into SQL """

    # XXX should be property of derived table classes?

    @classmethod
    def construct(cls, table, req):
        """ Turn the user input into a sql query

        Raises:
        KeyError if key not in table schema
        ValueError if invalid comparator
        """

        # XXX substring match for pools

        sql = None

        for i in req:
            line = i.split()
            if line[0] not in table.columns:
                raise KeyError
            if line[1] not in ('=', '<=', '>=', '>', '<', '!='):
                raise ValueError
            if sql:
                sql = and_(sql, cls.parse(line, table))
            else:
                sql = cls.parse(line, table)            

        return sql

    @classmethod
    def parse(cls, line, table):
        col = getattr(table.__table__.c, line[0])
        op = line[1]
        val = line[2]
        # XXX check type of val and normalize (e.g. boolean, list)

        if op == "=":
            return col == val
        elif op == "<=":
            return col <= val
        elif op == ">=":
            return col >= val
        elif op == "<":
            return col < val
        elif op == ">":
            return col > val
        elif op == "!=":
            return col != val
        else:
            raise ValueError

    @classmethod
    def getrequest(cls, lines):
        """ Parse a job description into SQL and return the results of the
        query as a list of dicts containing all of the parameters in
        Machine.columns """

        q = cls.construct(Machine, lines)
#        print "q = %s" % q
        res = engine.execute(Machine.__table__.select(q))
        return [cls.to_dict(Machine, i) for i in res]

    @classmethod
    def to_dict(cls, table, tup):
        """ split a query tuple into a dictionary """

        keys = table.columns
        d = dict((key, tup[getattr(table.__table__.c, key)]) for key in keys)
#        print "d = %s" % d
        return d

class Job(Base):
    __tablename__ = "jobs"

    # job ID
    id = Column(Integer, primary_key = True, autoincrement=True)
    # Name of job
    name = Column(String)
    # priority of request (must not be modified when job is blocked or
    # it will corrupt the heapq)
    priority = Column(Integer)
    # job type
    type = Column(String)
    # uid of job owner
    owner = Column(Integer)
    # gids of job owner
    gids = Column(PickleType)
    # machines that satisfied initial query
    machines = Column(PickleType)
    # Time job started/blocked (must not be modified when job is
    # blocked or it will corrupt the heapq)
    starttime = Column(Integer)
    # initial machine description in case we have to revalidate
    mdl = Column(PickleType)
    # True --> job is running; False --> job is blocked
    running = Column(Boolean)

    columns = ('name', 'type', 'priority', 'owner', 'machines', 'starttime',
                    'mdl', 'running')

    # Default value for jobs repopulated after startup
    conn = None
    pending = False

    def __init__(self, name, type, priority, owner, gids, machines, starttime, mdl, running, conn = None):
        self.name = name
        self.type = type
        self.priority = priority
        self.owner = owner
        self.gids = gids
        self.machines = machines
        self.starttime = starttime
        self.mdl = mdl
        self.running = running

        # client connection object for blocked connection
        self.conn = conn
        if conn:
            # Hook up to a pre-existing clientconn
            conn.job = self

        # Add to jobs[] after we commit since we key by ID
        self.pending = True

        session.add(self)

        self.setup()

    def setup(self):
        pass

    # XXX unused
    @classmethod
    def normalize(cls, vars):        
        """ Normalize and validate a dictionary of class variables """

        out = {}

        for i in ['name', 'joblass', 'machines', 'mdl']:
            try:
                out[i] = vars[i].lower()
            except KeyError:
                continue

        for i in ['machines', 'mdl']:
            try:
                out[i] = vars[i].split()
            except KeyError:
                continue

        for i in ['priority', 'owner', 'starttime']:
            try:
                out[i] = int(vars[i])
            except KeyError:
                continue
            if vars < 0:
                raise ValueError
        try:
            out['running'] = bool(int(vars['running']))
        except TypeError:
            raise ValueError
        except KeyError:
            pass

        return out

    @classmethod
    def revalidate_blocked(cls):
        """ Revalidate blocked jobs after a machine spec changed, in case they
        can now run """

        # jobs might change during iteration
        for job in jobs.values():
            if job.running:
                continue
            job.run_or_block()

    def run(self, mach, incr):
        """ Start to run this job.  incr is to specify whether the machine
should increase its running job count """

        # Update job state in DB
        self.machines = [mach.name]
        self.starttime = time()
        self.running = True
        self.commit()

        mach.run(self, incr)

#        print "Finished making runnable"

    def block(self, mlist):
        """ Block this job on a list of machines """

        # XXX don't set these here?  Not appropriate after restart
        self.machines=[m.name for m in mlist]
        self.starttime=time()
        self.running = False

        # Register ourselves with each machine on the list.
        for m in mlist:
#            print "Adding to %s blocklist" % m.name
            m.block(self)

        self.commit()

        # Block until our job slot becomes available
        if not self.conn:
            # Might not be connected yet, after a server restart
            return
        try:
            self.conn.send(203, {"id":self.id}, finish = False)
        except SendError:
            print "Send error, cancelling job %s" % self.id
            self.unblock(None)
            self.finish()

    def commit(self):
        try:
            session.commit()
        except Exception, e:
            print "session.commit failed for %s" % session
            print e

        if self.pending:
            self.pending = False
            jobs[self.id] = self

    def run_or_block(self, block = True):
        """ Revalidate a blocked job after the machine properties change.
        """

        assert self.running == False
        conn = self.conn

        try:
            (runnable, mlist) = self.getrunnable()
        except BodyError:
            if conn:
                conn.send(406)
            print "Body Error for %s" % self.id
            self.finish()
            return
        except NoMachinesError:
            if conn:
                conn.send(402)
            print "No Machines for %s" % self.id
            self.finish()
            return
        except PermissionDeniedError:
            if conn:
                conn.send(408)
            print "Permission denied for %s" % self.id
            self.finish()
            return

        if runnable:
            if self.machines:
                self.unblock(None)
            if not conn:
                # Job became runnable but is disconnected so we cannot
                # tell anyone; cancel job instead
                print "Disconnected job %s became runnable" % self.id
                self.finish()
                return
            self.run(mlist[0], True)
            self.commit()
            if conn:
                conn.send(202, {"machine":mlist[0].name, "id":self.id})
        else:
            if block:
                if DEBUG:
                    print "old_mlist = %s" % self.machines
                old_mlist = set(machines[m] for m in self.machines)
                if old_mlist != set(mlist):
                    if DEBUG:
                        print "Updating blocked list from %s to %s" % \
                            ([m.name for m in old_mlist], [m.name for m in mlist])
                    self.unblock(None)
                    self.block(mlist) # Will commit
            else:
                if conn:
                    conn.send(403)
                print "Job %s would block" % self.id
                self.finish()
        return

    def suitable_machines(self):
        """ Validate a list of suitable machines that can run
        a job.

        Returns: [Machine]
        machines capable of running the job

        Raises:
        BodyError if error in mdl
        NoMachinesError if no machines match constraints
        PermissionDeniedError if machines available but no access permission
        """

        try:
            res = SQL.getrequest(self.mdl)
        except KeyError, ValueError:
            raise BodyError

        if len(res) == 0:
            # No machines match spec
            raise NoMachinesError

        mlist = [machines[m['name']] for m in res]
        # Avoid picking the first machines preferentially
        shuffle(mlist)

        valid_mlist = tuple(m for m in mlist if
                            m.validateuser(self.owner, self.gids))
        if not valid_mlist:
            raise PermissionDeniedError
        return valid_mlist

    def getrunnable(self):
        """ Choose a machine to run this job, or return the list of machines
        it should block on.

        Returns: (isrunnable, mlist)
        isrunnable: bool
        mlist: [Machine]
        """

        try:
            valid_mlist = self.suitable_machines()
        except (BodyError, NoMachinesError, PermissionDeniedError):
            raise
        choice = Machine.pick(valid_mlist)
        if choice:
            return (True, [choice])
        else:
            return (False, valid_mlist)

    def unblock(self, newmachine):
        """ A machine has released a slot and chosen us to run, or we are
        cancelling a blocked job. We need to unblock ourselves from
        the remaining machines we are blocked on.

        args:

        newmachine = Machine (machine the job is now running on, or
        None if the job is canceled)

        Returns:
        True if we were able to make ourselves runnable,
        False otherwise (the socket may have disconnected)

        XXX raise exceptions instead
        """

        assert self.running == False

        # Remove from the machines we were blocked on
        for i in self.machines:
            m = machines[i]
#            print "Unblocking from %s: %s" % (m.name, m.blocked)
            m.unblock(self)

        self.commit()

        if not newmachine:
            # Canceled a job
            return True

        if not self.conn:
            return False

        # Call back to run the job.  incr == False because we didnt
        # decrement the curjobs (XXX not needed in single-threaded
        # server)
        self.run(newmachine, False)

        try:
            self.conn.send(202, args = {"machine":newmachine.name, "id":self.id}, raiseexception = True)
        except IOError:
            # Socket send failure, probably the client disconnected
            return False

#        print "Finished unblock"
        return True

    def finish(self):
        # Delete the job and kick the machine to start a new one if we
        # were running
#        print_stack()
#        print "Deleting job %s" % self.id

        if not self.pending:
            session.delete(self)
            self.commit()
            try:
                del jobs[self.id]
            except Exception, e:
                print "could not delete jobs[ %s ]" % self.id
                print e
        else:
            session.rollback()

        if self.running:
            # Start new job
            m = machines[self.machines[0]]
            m.finish(self)

        del self

class Machine(Base):
    __tablename__ = "machines"

    id = Column(Integer, primary_key = True)
    # Machine name
    name = Column(String)
    # Max number of jobs
    maxjobs = Column(Integer)
    # Running kernel version
    osversion = Column(Integer)
    # Current arch
    arch = Column(Integer)
    # Resource domain (XXX semantics?)
    domain = Column(String)
    # Primary pool (for job priority)
    primarypool = Column(String)
    # Pools we participate in
    pools = Column(PickleType)
    # Number of CPUs
    numcpus = Column(Integer)
    # ZFS available?
    haszfs = Column(Boolean)
    # Access control list
    acl = Column(PickleType)
    # Is the machine online?
    online = Column(Boolean)

    # set of properties we require to configure a new machine
    columns = ('name', 'maxjobs', 'osversion', 'arch', 'domain', 'primarypool', 'pools', 'numcpus', 'haszfs', 'acl', 'online')

    def __init__(self, vars):
        for i in self.columns:
            setattr(self, i, vars[i])
#            print "Setting %s = %s" % (i, getattr(self, i))

        session.add(self)

        self.setup()

    def setup(self):
        # List of jobs blocked on this machine
        self.blocked = []
        # Dictionary of jobs currently running
        self.running = {}
        # Dictionary Cache of ACL validations against this machine
        self.validated={}
        # Current number of running jobs (Don't need to track in DB since
        # we reinitialize at reboot)
        self.curjobs = 0

        # Populate ACL object 
        self.aclobj = ACL([acls[acl].aclelement for acl in self.acl])

    @classmethod
    def normalize(cls, vars):        
        """ Normalize and validate a dictionary of class variables

        args: vars: dictionary of variables

        Returns: normalized copy of vars (strings converted to
        lowercase etc)

        Raises:
        ValueError if value cannot be normalized
        KeyError if value is not an allowed property of this object

        """

        out = {}

        for i in ['name', 'arch', 'domain', 'primarypool']:
            try:
                if "," in vars[i]:
                    raise ValueError
                out[i] = vars[i].lower()
            except KeyError:
                continue

        try:
            out['pools'] = [i.lower() for i in vars['pools'].split(",")]
        except KeyError:
            pass

        for i in ['maxjobs', 'osversion', 'numcpus']:
            try:
                out[i] = int(vars[i])
            except KeyError:
                continue
            except TypeError:
                raise ValueError
            if vars < 0:
                raise ValueError
        for i in ['haszfs', 'online']:
            try:
                out[i] = bool(int(vars[i]))
            except TypeError:
                raise ValueError
            except KeyError:
                continue

        try:
            acllist = vars['acl'].split(",")
            outacl = []
            for acl in acllist:
                acl = acl.lower()
                if acl in acls:
                    outacl.append(acl)
                else:
                    raise ValueError
            out['acl'] = outacl
        except KeyError:
            pass

        return out

    def clear_validated(self):
        """ Clear the cache of validated uid/gids """

        self.validated = {}

    def validateuser(self, uid, gids):
        """ Validate a uid and gidlist against our ACL, memoizing the results """

        try:
            res = self.validated[(uid, gids)]
#            print "Found cached answer for (%s, %s) of %s: %s" % (uid, gids, self.acl, res)
#            print self.validated
            return res
        except KeyError:
#            print "Validating (%s, %s) against %s" % (uid, gids, self.acl)
            res = self.aclobj.validate(uid, gids)
            self.validated[(uid, gids)]=res
#            print "--> %s" % res
            return res

    @classmethod
    def pick(cls, mlist):
        """ Choose the least loaded machine from a list

        args: mlist = [Machine]

        Returns: Machine
        """

        choice = None
        min = 999
        for m in mlist:
            if not m.online:
                continue
                
            load = float(m.curjobs) / m.maxjobs
            if load >= 1:
                continue

            if load < min:
                min = load
                choice = m
#                print "%s : %f" % (m.name, load)

        return choice

    def run(self, job, incr):
        """ Add job to the running queue on this machine """

        self.running[job.id] = job
        if incr:
            self.curjobs += 1
        #assert self.curjobs <= self.maxjobs

    def block(self, job):
        """ Add a job to our blocked queue """

        assert job not in [j[2] for j in self.blocked]
        heappush(self.blocked, (job.priority, job.starttime, job))

#        print "blocked now %s" % self.blocked

    def unblock(self, job):
        """ Remove a job from the blocked list.        
        XXX: O(N) and might be called on many machines. """

        if DEBUG:
            print "job = %s self.blocked = %s" % (job, self.blocked)
        try:
            self.blocked.remove((job.priority, job.starttime, job))
        except Exception, e:
            # XXX MCL 20100312
            print "------------------------------------"
            print "Exception in Machine.unblock:"
            print str( e )
            print
            try:
                print "job:"
                print str( job )
                print job.id
                print job.name
                print job.priority
                print job.type
                print job.owner
                print str( job.gids )
                print str( job.machines )
                print job.starttime
                print str( job.mdl )
                print job.running
            except Exception, eprime:
                print "Exception in Machine.unblock:"
                print str( eprime )
            print
            print "self:"
            print str( self )
            print "------------------------------------"
        # Restore heap invariant
        heapify(self.blocked)

        if DEBUG:
            print "blocked now %s" % self.blocked

    def finish(self, job):
        """ A job finished running, remove it from running list and start
	another one """

        try:
            del self.running[job.id]
        except KeyError:
            print "job %d was not running!", job.id
            raise KeyError

        while True:
            # Peek at next blocked job on this machine
            try:
                (prio, starttime, next) = self.blocked[0]
            except IndexError:
                # No more jobs to run
                self.curjobs -= 1
                return
            
            # Try to make the job runabble
            if DEBUG:
                print "Making next job %s runnable" % next.id
            if next.unblock(self):
                break
            else:
                # Oops, something went wrong (e.g. socket
                # disconnection; clean up & try another one)
                print "Unblocking %s failed" % next.id
                next.finish()

class QManagerACL(Base):
    __tablename__ = "acl"

    name = Column(String, primary_key = True)
    uidlist = Column(PickleType)
    gidlist = Column(PickleType)
    sense = Column(Boolean)

    columns=('name', 'uidlist', 'gidlist', 'sense')

    def __init__(self, vars):
        for i in self.columns:
            setattr(self, i, vars[i])
#            print "Setting %s = %s" % (i, getattr(self, i))

        session.add(self)

        self.setup()

    @classmethod
    def normalize(cls, vars):
        out = {}

        try:
            if "," in vars['name']:
                raise ValueError
            out['name'] = vars['name'].lower()
        except KeyError:
            pass

        try:
            if vars['uidlist'] == "":
                out['uidlist'] = []
            else:
                out['uidlist'] = vars['uidlist'].split(",")
        except KeyError:
            pass

        try:
            if vars['gidlist'] == "":
                out['gidlist'] = []
            else:
                out['gidlist'] = vars['gidlist'].split(",")
        except KeyError:
            pass        

        try:
            out['sense'] = bool(int(vars['sense']))
        except TypeError:
            raise ValueError
        except KeyError:
            pass

        return out

    def setup(self):
        self.aclelement = ACLElement(self.name, self.uidlist,
                                     self.gidlist, self.sense)

        return
