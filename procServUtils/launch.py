
import sys, os
from .conf import getconf, getrundir

try:
    import shlex
except ImportError:
    from . import shlex

def getargs():
    from argparse import ArgumentParser
    A = ArgumentParser()
    A.add_argument('name', help='procServ instance name')
    A.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    A.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')
    A.add_argument('-d','--debug', action='count', default=0)
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

    toexec = [
        'procServ',
        '--foreground',
        '--logfile', '-',
        '--name', name,
        '--ignore','^D^C^]',
        '--logoutcmd', '^D',
        '--chdir',chdir,
        '--info-file',os.path.join(rundir, 'procserv-%s'%name, 'info'), #/run/procserv-$NAME/info
        '--port', port if port != "0" else 'unix:%s/procserv-%s/control'%(rundir,name),
    ]

    if args.debug>1:
        toexec.append('--debug')

    #toexec.append(port)
    toexec.extend(shlex.split(cmd))

    if args.debug>0:
        sys.stderr.write('in %s exec: %s\n'%(chdir, ' '.join(map(shlex.quote, toexec))))

    os.chdir(chdir)
    os.execvpe(toexec[0], toexec, env)
    sys.exit(2) # never reached
