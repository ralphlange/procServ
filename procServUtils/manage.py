
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
import subprocess as SP

from .conf import getconf, getrundir, getgendir

try:
    import shlex
except ImportError:
    from . import shlex

_levels = [
    logging.WARN,
    logging.INFO,
    logging.DEBUG,
]

systemctl = '/bin/systemctl'

def status(conf, args, fp=None):
    rundir=getrundir(user=args.user)
    fp = fp or sys.stdout

    for name in conf.sections():
        if not conf.getboolean(name, 'instance'):
            continue
        fp.write('%s '%name)

        pid = None
        ports = []
        infoname = os.path.join(rundir, 'procserv-%s'%name, 'info')
        try:
            with open(infoname) as F:
                _log.debug('Read %s', F.name)
                for line in map(str.strip, F):
                    if line.startswith('pid:'):
                        pid = int(line[4:])
                    elif line.startswith('tcp:'):
                        ports.append(line[4:])
                    elif line.startswith('unix:'):
                        ports.append(line[5:])
        except Exception as e:
            _log.debug('No info file %s', infoname)
            if getattr(e, 'errno',0)!=errno.ENOENT:
                _log.exception('oops')

        if pid is not None:
            _log.debug('Test PID %s', pid)
            # Can we say if the process is actually running?
            running = True
            try:
                os.kill(pid, 0)
                _log.debug('PID exists')
            except OSError as e:
                if e.errno==errno.ESRCH:
                    running = False
                    _log.debug('PID does not exist')
                elif e.errno==errno.EPERM:
                    _log.debug("Can't say if PID exists or not")
                else:
                    _log.exception("Testing PID %s", pid)
            fp.write('Running' if running else 'Dead')

            if running:
                fp.write('\t'+' '.join(ports))
        else:
            fp.write('Stopped')

        fp.write('\n')

def syslist(conf, args):
    SP.check_call([systemctl,
                    '--user' if args.user else '--system',
                    'list-units', 'procserv-*'])

def addproc(conf, args):
    outdir = getgendir(user=args.user)
    cfile = os.path.join(outdir, '%s.conf'%args.name)

    if os.path.exists(cfile) and not args.force:
        _log.error("Instance already exists @ %s", cfile)
        sys.exit(1)

    #if conf.has_section(args.name):
    #    _log.error("Instance already exists")
    #    sys.exit(1)

    try:
        os.makedirs(outdir)
    except OSError as e:
        if e.errno!=errno.EEXIST:
            _log.exception('Creating directory "%s"', outdir)
            raise

    _log.info("Writing: %s", cfile)

    # ensure chdir is an absolute path
    args.chdir = os.path.abspath(os.path.join(os.getcwd(), args.chdir))

    args.command[0] = os.path.abspath(os.path.join(args.chdir, args.command[0]))

    opts = {
        'name':args.name,
        'command': ' '.join(map(shlex.quote, args.command)),
        'chdir':args.chdir,
    }

    with open(cfile+'.tmp', 'w') as F:
        F.write("""
[%(name)s]
command = %(command)s
chdir = %(chdir)s
"""%opts)

        if args.username: F.write("user = %s\n"%args.username)
        if args.group: F.write("group = %s\n"%args.group)
        if args.port: F.write("port = %s\n"%args.port)

    os.rename(cfile+'.tmp', cfile)

    _log.info('Trigger systemd reload')
    SP.check_call([systemctl,
                   '--user' if args.user else '--system',
                   'daemon-reload'], shell=False)

    if args.autostart:
        SP.check_call([systemctl,
                       '--user' if args.user else '--system',
                       'start', 'procserv-%s.service'%args.name])
    else:
        sys.stdout.write("# systemctl start procserv-%s.service\n"%args.name)

def delproc(conf, args):
    from .conf import getconffiles, ConfigParser
    for cfile in getconffiles(user=args.user):
        _log.debug('Process %s', cfile)

        with open(cfile) as F:
            C = ConfigParser({'instance':'1'})
            C.readfp(F)

        if not C.has_section(args.name):
            continue
        if not C.getboolean(args.name, 'instance'):
            continue

        if not args.force and sys.stdin.isatty():
            while True:
                sys.stdout.write("Remove section '%s' from %s ? [yN]"%(args.name, cfile))
                sys.stdout.flush()
                L = sys.stdin.readline().strip().upper()
                if L=='Y':
                    break
                elif L in ('N',''):
                    sys.exit(1)
                else:
                    sys.stdout.write('\n')

        if len(C.defaults())==1 and len(C.sections())==1:
            _log.info('Removing empty file %s', cfile)
            os.remove(cfile)
        else:
            C.remove_section(args.name)
            C.remove_option('DEFAULT', 'instance')
            _log.info("Removing section '%s' from file %s", args.name, cfile)
            with open(cfile+'.tmp', 'w') as F:
                C.write(F)
            os.rename(cfile+'.tmp', cfile)

    _log.info('Trigger systemd reload')
    SP.check_call([systemctl,
                   '--user' if args.user else '--system',
                   'daemon-reload'], shell=False)

    sys.stdout.write("# systemctl stop procserv-%s.service\n"%args.name)

def writeprocs(conf, args):
    opts = {
        'rundir':getrundir(user=args.user),
    }
    _log.debug('Writing %s', args.out)
    with open(args.out+'.tmp', 'w') as F:
        for name in conf.sections():
            opts['name'] = name
            F.write("""
console %(name)s {
    master localhost;
    type uds;
    uds %(rundir)s/procserv-%(name)s/control;
}
"""%opts)

    os.rename(args.out+'.tmp', args.out)

    if args.reload:
        _log.debug('Reloading conserver-server')
        SP.check_call([systemctl,
                    '--user' if args.user else '--system',
                    'reload', 'conserver-server.service'], shell=False)
    else:
        sys.stdout.write('# systemctl reload conserver-server.service\n')

def getargs():
    from argparse import ArgumentParser
    P = ArgumentParser()
    P.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    P.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')
    P.add_argument('-v','--verbose', action='count', default=0)

    SP = P.add_subparsers()

    S = SP.add_parser('status', help='List procServ instance state')
    S.set_defaults(func=status)

    S = SP.add_parser('list', help='List procServ instances')
    S.set_defaults(func=syslist)

    S = SP.add_parser('add', help='Create a new procServ instance')
    S.add_argument('-C','--chdir', default=os.getcwd(), help='Run directory for instance')
    S.add_argument('-P','--port', help='telnet port')
    S.add_argument('-U','--user', dest='username')
    S.add_argument('-G','--group')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('-A','--autostart',action='store_true', default=False,
                   help='Automatically start after adding')
    S.add_argument('name', help='Instance name')
    S.add_argument('command', nargs='+', help='Command')
    S.set_defaults(func=addproc)

    S = SP.add_parser('remove', help='Remove a procServ instance')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('name', help='Instance name')
    S.set_defaults(func=delproc)

    S = SP.add_parser('write-procs-cf', help='Write conserver config')
    S.add_argument('-f','--out',default='/etc/conserver/procs.cf') 
    S.add_argument('-R','--reload', action='store_true', default=False)
    S.set_defaults(func=writeprocs)

    A = P.parse_args()
    if not hasattr(A, 'func'):
        P.print_help()
        sys.exit(1)
    return A

def main(args):
    lvl = _levels[max(0, min(args.verbose, len(_levels)-1))]
    logging.basicConfig(level=lvl)
    conf = getconf(user=args.user)
    args.func(conf, args)
