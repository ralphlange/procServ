
import logging
_log = logging.getLogger(__name__)

import sys, os, errno
from .conf import getrundir

telnet = '/usr/bin/telnet'
socat  = '/usr/bin/socat'

def attach(args):
    rundir = getrundir(user=args.user)

    info = '%s/procserv-%s/info'%(rundir, args.name)
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

            sys.exit("No tool to connect to %s (attach command needs telnet and/or socat)"%args.name)
    except OSError as e:
        if e.errno==errno.ENOENT:
            _log.error('%s is not an active %s procServ', args.name, 'user' if args.user else 'system')
        else:
            _log.exception("Can't open %s"%info)

    sys.exit(1)
