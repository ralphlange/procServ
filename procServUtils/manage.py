
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
import subprocess as SP

from .conf import getconf, getrundir, getgendir

try:
    import shlex
except ImportError:
    from . import shlex

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
        try:
            with open(os.path.join(rundir, 'procServ', name, 'info')) as F:
                for line in map(str.strip, F):
                    if line.startswith('pid:'):
                        pid = int(line[4:])
                    elif line.startswith('tcp:'):
                        ports.append(line)
                    elif line.startswith('unix:'):
                        ports.append(line)
        except Exception as e:
            if getattr(e, 'errno',0)!=errno.ENOENT:
                _log.exception('oops')

        if pid is not None:
            # Can we say if the process is actually running?
            running = True
            try:
                os.kill(pid, 0)
                # yup
            except OSError as e:
                if e.errno==errno.ESRCH:
                    running = False
                elif e.errno==errno.EPERM:
                    pass # assume it's running as another user
                else:
                    _log.exception("Testing PID %s", pid)
            fp.write('Running' if running else 'Dead')
        else:
            fp.write('Stopped')

        fp.write('\n')

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

    args.command[0] = os.path.abspath(os.path.join(args.chdir, args.chdir))

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


def getargs():
    from argparse import ArgumentParser
    P = ArgumentParser()
    P.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    P.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')

    SP = P.add_subparsers()

    S = SP.add_parser('status', help='List procServ instance state')
    S.set_defaults(func=status)

    S = SP.add_parser('add', help='Create a new procServ instance state')
    S.add_argument('-C','--chdir', default=os.getcwd(), help='Run directory for instance')
    S.add_argument('-P','--port', help='telnet port')
    S.add_argument('-U','--user', dest='username')
    S.add_argument('-G','--group')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('name', help='Instance name')
    S.add_argument('command', nargs='+', help='Command')
    S.set_defaults(func=addproc)

    A = P.parse_args()
    if not hasattr(A, 'func'):
        P.print_help()
        sys.exit(1)
    return A

def main(args):
    logging.basicConfig(level=logging.DEBUG)
    conf = getconf(user=args.user)
    args.func(conf, args)
