
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
import subprocess as SP

from .conf import getconf, getrundir, getgendir, ConfigParser, addconf, getconffiles

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
journalctl = '/bin/journalctl'

def check_req(conf, args):
    if args.name not in conf.sections():
        _log.error(' "%s" is not an active %s procServ.', args.name, 'user' if args.user else 'system')
        sys.exit(1)

def status(conf, args, fp=None):
    try:
        from tabulate import tabulate
    except ImportError:
        from .fallbacks import tabulate

    try:
        from termcolor import colored
    except ImportError:
        from .fallbacks import colored

    rundir=getrundir(user=args.user)
    fp = fp or sys.stdout

    table = []
    for name in conf.sections():
        if not conf.getboolean(name, 'instance'):
            continue
        instance = ['%s '%name]

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
            instance.append(colored('Running', 'green') if running else colored('Dead', attrs=['bold']))

            if running:
                instance.append(' '.join(ports))
        else:
            instance.append(colored('Stopped', 'red'))

        table.append(instance)

    # Print results table
    headers = ['PROCESS', 'STATUS', 'PORT']
    fp.write(tabulate(sorted(table), headers=headers, tablefmt="github")+ '\n')

def syslist(conf, args):
    SP.check_call([systemctl,
                    '--user' if args.user else '--system',
                    'list-units',
                    '--all' if args.all else '',
                    'procserv-*'])

def startproc(conf, args):
    check_req(conf, args)
    _log.info("Starting service procserv-%s.service", args.name)
    SP.call([systemctl,
            '--user' if args.user else '--system',
            'start', 'procserv-%s.service'%args.name])

def stopproc(conf, args):
    check_req(conf, args)
    _log.info("Stopping service procserv-%s.service", args.name)
    SP.call([systemctl,
            '--user' if args.user else '--system',
            'stop', 'procserv-%s.service'%args.name])

def restartproc(conf, args):
    check_req(conf, args)
    _log.info("Restarting service procserv-%s.service", args.name)
    SP.call([systemctl,
            '--user' if args.user else '--system',
            'restart', 'procserv-%s.service'%args.name])

def showlogs(conf, args):
    check_req(conf, args)
    _log.info("Opening logs of service procserv-%s.service", args.name)
    try:
        SP.call([journalctl,
                '--user-unit' if args.user else '--unit',
                'procserv-%s.service'%args.name] +
                (['-f'] if args.follow else []))
    except KeyboardInterrupt:
        pass

def attachproc(conf, args):
    check_req(conf, args)
    from .attach import attach
    attach(args)

def addproc(conf, args):
    from .generator import run

    # ensure chdir and env_file are absolute paths
    chdir = os.path.abspath(os.path.join(os.getcwd(), args.chdir))
    if args.env_file:
        args.env_file = os.path.abspath(os.path.join(os.getcwd(), args.env_file))
        if not os.path.exists(args.env_file):
            _log.error('File not found: "%s"', args.env_file)
            sys.exit(1)

    # command is relative to chdir
    command = os.path.abspath(os.path.join(args.chdir, args.command))
    command = command + ' ' + ' '.join(map(shlex.quote, args.args))

    # set conf paramers
    new_conf = ConfigParser()
    conf_name = args.name
    new_conf.add_section(conf_name)
    new_conf.set(conf_name, "command", command)
    new_conf.set(conf_name, "chdir", chdir)
    if args.username: new_conf.set(conf_name, "user", args.username)
    if args.group: new_conf.set(conf_name, "group", args.group)
    if args.port: new_conf.set(conf_name, "port", args.port)
    if args.environment:
        new_conf.set(conf_name, "environment", ' '.join("\"%s\""%e for e in args.environment))
    if args.env_file: new_conf.set(conf_name, "env_file", args.env_file)

    # write conf file
    addconf(conf_name, new_conf, args.user, args.force)

    # generate service files
    outdir = getgendir(args.user)
    run(outdir, user=args.user)

    # register systemd service
    argusersys = '--user' if args.user else '--system'
    _log.info('Trigger systemd reload')
    SP.check_call([systemctl,
                   argusersys,
                   'daemon-reload'], shell=False)

    SP.check_call([systemctl,
                   argusersys,
                   'enable',
                   "%s/procserv-%s.service"%(outdir, conf_name)])

    if args.autostart:
        startproc(conf, args)
    else:
        sys.stdout.write("# manage-procs %s start %s\n"%(argusersys, conf_name))

def delproc(conf, args):
    check_req(conf, args)

    for cfile in getconffiles(user=args.user):
        _log.debug('delproc processing %s', cfile)

        with open(cfile) as F:
            C = ConfigParser({'instance':'1'})
            C.readfp(F)

        if not C.has_section(args.name):
            continue
        if not C.getboolean(args.name, 'instance'):
            continue

        if not args.force and sys.stdin.isatty():
            while True:
                sys.stdout.write("Remove section '%s' from %s ? [yN] "%(args.name, cfile))
                sys.stdout.flush()
                L = sys.stdin.readline().strip().upper()
                if L=='Y':
                    break
                elif L in ('N',''):
                    sys.exit(1)
                else:
                    sys.stdout.write('\n')

        if len(C.defaults())==1 and len(C.sections())==1:
            _log.info('Emptying and removing file %s', cfile)
            os.remove(cfile)
        else:
            C.remove_section(args.name)
            C.remove_option('DEFAULT', 'instance')
            _log.info("Removing section '%s' from file %s", args.name, cfile)
            with open(cfile+'.tmp', 'w') as F:
                C.write(F)
            os.rename(cfile+'.tmp', cfile)

    stopproc(conf, args)

    _log.info("Disabling service procserv-%s.service", args.name)
    SP.check_call([systemctl,
                   '--user' if args.user else '--system',
                   'disable',
                   "procserv-%s.service"%args.name])

    _log.info("Resetting service procserv-%s.service", args.name)
    with open(os.devnull, 'w') as devnull:
        SP.call([systemctl,
                    '--user' if args.user else '--system',
                    'reset-failed',
                    'procserv-%s.service'%args.name], stderr=devnull)

    _log.info('Triggering systemd reload')
    SP.check_call([systemctl,
                   '--user' if args.user else '--system',
                   'daemon-reload'], shell=False)
    outdir = getgendir(user=args.user)

    _log.info('Removing service file %s/procserv-%s.service', outdir, args.name)
    try:
        os.remove("%s/procserv-%s.service"%(outdir,args.name))
    except OSError:
        pass

    #sys.stdout.write("# systemctl stop procserv-%s.service\n"%args.name)

def renameproc(conf, args):
    check_req(conf, args)
    
    if not args.force and sys.stdin.isatty():
        while True:
            sys.stdout.write("This will stop the service '%s' if it's running. Continue? [yN] "%(args.name))
            sys.stdout.flush()
            L = sys.stdin.readline().strip().upper()
            if L=='Y':
                break
            elif L in ('N',''):
                sys.exit(1)
            else:
                sys.stdout.write('\n')

    from .generator import run

    # copy settings from previous conf
    items = conf.items(args.name)
    new_conf = ConfigParser()
    new_conf.add_section(args.new_name)
    for item in items:
        new_conf.set(args.new_name, item[0], item[1])

    # create new conf file with old settings
    addconf(args.new_name, new_conf, args.user, args.force)

    # delete previous proc
    args.force = True
    delproc(conf, args)

    # generate service files
    outdir = getgendir(args.user)
    run(outdir, user=args.user)

    # register systemd service
    argusersys = '--user' if args.user else '--system'
    _log.info('Trigger systemd reload')
    SP.check_call([systemctl,
                   argusersys,
                   'daemon-reload'], shell=False)

    SP.check_call([systemctl,
                   argusersys,
                   'enable',
                   "%s/procserv-%s.service"%(outdir, args.new_name)])

    if args.autostart:
        args.name = args.new_name
        startproc(conf, args)
    else:
        sys.stdout.write("# manage-procs %s start %s\n"%(argusersys,args.new_name))

def writeprocs(conf, args):
    argusersys = '--user' if args.user else '--system'
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
                    argusersys,
                    'reload', 'conserver-server.service'], shell=False)
    else:
        sys.stdout.write('# systemctl %s reload conserver-server.service\n'%argusersys)

def instances_completer(**kwargs):
    user = True
    if 'parsed_args' in kwargs:
        user = kwargs['parsed_args'].user
    return getconf(user=user).sections()

def getargs(args=None):
    from argparse import ArgumentParser, REMAINDER

    P = ArgumentParser()
    P.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    P.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')
    P.add_argument('-v','--verbose', action='count', default=0)

    SP = P.add_subparsers()

    S = SP.add_parser('status', help='Report state of procServ instances')
    S.set_defaults(func=status)

    S = SP.add_parser('list', help='List procServ instances')
    S.add_argument('--all', action='store_true', default=False)
    S.set_defaults(func=syslist)

    S = SP.add_parser('add', help='Create a new procServ instance')
    S.add_argument('-C','--chdir', default=os.getcwd(), help='Run directory for instance')
    S.add_argument('-P','--port', help='telnet port')
    S.add_argument('-U','--user', dest='username')
    S.add_argument('-G','--group')
    S.add_argument('-e','--environment', action='append', help='Add an environment variable')
    S.add_argument('-E','--env-file', help='Environment file path')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('-A','--autostart',action='store_true', default=False,
                   help='Automatically start after adding')
    S.add_argument('name', help='Instance name')
    S.add_argument('command', help='Command')
    S.add_argument('args', nargs=REMAINDER)
    S.set_defaults(func=addproc)

    S = SP.add_parser('remove', help='Remove a procServ instance')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('name', help='Instance name').completer = instances_completer
    S.set_defaults(func=delproc)

    S = SP.add_parser('rename', help='Rename a procServ instance. The instance will be stopped.')
    S.add_argument('-f','--force', action='store_true', default=False)
    S.add_argument('-A','--autostart',action='store_true', default=False,
                   help='Automatically start after renaming')
    S.add_argument('name', help='Current instance name').completer = instances_completer
    S.add_argument('new_name', help='Desired instance name')
    S.set_defaults(func=renameproc)

    S = SP.add_parser('write-procs-cf', help='Write conserver config')
    S.add_argument('-f','--out',default='/etc/conserver/procs.cf')
    S.add_argument('-R','--reload', action='store_true', default=False)
    S.set_defaults(func=writeprocs)

    S = SP.add_parser('start', help='Start a procServ instance')
    S.add_argument('name', help='Instance name').completer = instances_completer
    S.set_defaults(func=startproc)

    S = SP.add_parser('stop', help='Stop a procServ instance')
    S.add_argument('name', help='Instance name').completer = instances_completer
    S.set_defaults(func=stopproc)

    S = SP.add_parser('restart', help='Restart a procServ instance')
    S.add_argument('name', help='Instance name').completer = instances_completer
    S.set_defaults(func=restartproc)

    S = SP.add_parser('logs', help='Open logs of a procServ instance')
    S.add_argument('-f','--follow', action='store_true', default=False)
    S.add_argument('name', help='Instance name').completer = instances_completer
    S.set_defaults(func=showlogs)

    S = SP.add_parser('attach', help='Attach to a procServ instance')
    S.add_argument("name", help='Instance name').completer = instances_completer
    S.add_argument('extra', nargs=REMAINDER, help='extra args for telnet')
    S.set_defaults(func=attachproc)

    try:
        from argcomplete import autocomplete
        autocomplete(P)
    except ImportError:
        pass

    A = P.parse_args(args=args)
    if not hasattr(A, 'func'):
        P.print_help()
        sys.exit(1)
    return A

def main(args, test=False):
    lvl = _levels[max(0, min(args.verbose, len(_levels)-1))]
    if not test:
        logging.basicConfig(level=lvl)
    conf = getconf(user=args.user)
    args.func(conf, args)
