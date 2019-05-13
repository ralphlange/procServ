
import os
from functools import reduce
from glob import glob

try:
    from ConfigParser import SafeConfigParser as ConfigParser
except ImportError:
    from configparser import ConfigParser

def getgendir(user=False):
    if user:
        return os.path.expanduser('~/.config/procServ.d')
    else:
        return '/etc/procServ.d'

def getrundir(user=False):
    if user:
        return os.environ['XDG_RUNTIME_DIR']
    else:
        return '/run'

def getconffiles(user=False):
    """Return a list of config file names
    
    Only those which actualy exist
    """
    if user:
        files = map(os.path.expanduser, [
            '~/.config/procServ.conf',
            '~/.config/procServ.d/*.conf',
        ])
    else:
        files = [
            '/etc/procServ.conf',
            '/etc/procServ.d/*.conf',
        ]

    # glob('') -> ['',]
    # map(glob) produces a list of lists
    # reduce by concatination into a single list
    return reduce(list.__add__, map(glob, files), [])

_defaults = {
    'user':'nobody',
    'group':'nogroup',
    'chdir':'/',
    'port':'0',
    'instance':'1',
}

def getconf(user=False):
    """Return a ConfigParser with one section per procServ instance
    """
    from glob import glob

    C = ConfigParser(_defaults)

    C.read(getconffiles(user=user))

    return C
