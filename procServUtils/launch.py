
import sys, os
from .conf import getconf, getrundir

try:
    import shlex
except ImportError:
    from . import shlex

procServ = '/usr/bin/procServ'

def getargs():
    from argparse import ArgumentParser
    A = ArgumentParser()
    A.add_argument('name', help='procServ instance name')
    A.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    A.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')
    return A.parse_args()

def main(args):
    conf = getconf(user=args.user)

    name, user = args.name, args.user

    if not conf.has_section(name):
        sys.stderr.write("Instance '%s' not found"%name)
        sys.exit(1)

    if not conf.getboolean(name, 'instance'):
        sys.stderr.write("'%s' not an instance"%name)
        sys.exit(1)

    if not conf.has_option(name, 'command'):
        sys.stderr.write("instance '%s' missing command="%name)
        sys.exit(1)

    chdir = conf.get(name, 'chdir')
    cmd   = conf.get(name, 'command')
    port  = conf.get(name, 'port')

    rundir = getrundir(user=user)

    env = {
        'PROCSERV_NAME':name,
        'IOCNAME':name,
    }
    env.update(os.environ)

    cmd = [
        procServ,
        '--foreground',
        '--name', name,
        '--ignore','^D^C^]',
        '--chdir',chdir,
        '--info-file',os.path.join(rundir, 'procServ', name, 'info'), #/run/procServ/$NAME/info
        '--port-spec', 'unix:%s/procServ/%s/control'%(rundir,name),
        port,
    ]+shlex.split(cmd)

    os.chdir(chdir)
    os.execve(cmd[0], cmd, env)
    sys.exit(2) # never reached
