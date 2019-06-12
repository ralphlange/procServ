
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
from .conf import getrundir

_levels = [
    logging.WARN,
    logging.INFO,
    logging.DEBUG,
]

telnet = '/usr/bin/telnet'
socat  = '/usr/bin/socat'

def getargs():
    from argparse import ArgumentParser
    P = ArgumentParser()
    P.add_argument('--user', action='store_true', default=os.geteuid()!=0,
                   help='Consider user config')
    P.add_argument('--system', dest='user', action='store_false',
                   help='Consider system config')
    P.add_argument('-v','--verbose', action='count', default=0)
    P.add_argument("proc", help='Name of instance to attach')
    P.add_argument('extra', nargs='*', help='extra args for telnet')
    return P.parse_args()

def main(args):
    lvl = _levels[max(0, min(args.verbose, len(_levels)-1))]
    logging.basicConfig(level=lvl)

    rundir = getrundir(user=args.user)

    info = '%s/procserv-%s/info'%(rundir, args.proc)
    try:
        with open(info) as F:
            for L in map(str.strip, F):

                if L.startswith('tcp:') and os.path.isfile(telnet):
                    _tcp, iface, port = L.split(':', 2)
                    args = [telnet, iface, port]+args.extra
                elif L.startswith('unix:') and os.path.isfile(socat):
                    _unix, socket = L.split(':', 1)
                    args = [socat, '-,raw,echo=0', 'unix-connect:' + socket]
                else:
                    continue

                _log.debug('exec: %s', ' '.join(args))
                os.execv(args[0], args)
                sys.exit(1) # never reached

            sys.exit("No tool to connect to %s"%args.proc)
    except OSError as e:
        if e.errno==errno.ENOENT:
            _log.error('%s is not an active %s procServ', args.proc, 'user' if args.user else 'system')
        else:
            _log.exception("Can't open %s"%info)

    sys.exit(1)
