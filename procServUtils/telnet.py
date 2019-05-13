
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
                if not L.startswith('tcp:'):
                    continue

                _tcp, iface, port = L.split(':', 2)

                args = [telnet, iface, port]+args.extra
                _log.debug('exec: %s', ' '.join(args))

                os.execv(telnet, args)
                sys.exit(1) # never reached

            sys.error('%s has no tcp control port')
    except OSError as e:
        if e.errno==errno.ENOENT:
            _log.error('%s is not an active %s procServ', args.name, 'user' if args.user else 'system')
        else:
            _log.exception("Can't open %s"%info)

    sys.exit(1)
