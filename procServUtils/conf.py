
import logging
_log = logging.getLogger(__name__)

import os
from functools import reduce
from glob import glob

try:
    from ConfigParser import SafeConfigParser as ConfigParser
except ImportError:
    from configparser import ConfigParser

# used by unit tests to redirect config directories
_testprefix = None

def getgendir(user=False):
    if _testprefix:
        return _testprefix+'/procServ.d'
    elif user:
        return os.path.expanduser('~/.config/procServ.d')
    else:
        return '/etc/procServ.d'

def getrundir(user=False):
    if _testprefix:
        return _testprefix+'/run'
    elif user:
        return os.environ['XDG_RUNTIME_DIR']
    else:
        return '/run'

def getconffiles(user=False):
    """Return a list of config file names
    
    Only those which actualy exist
    """
    if _testprefix:
        prefix = _testprefix
    elif user:
        prefix = os.path.expanduser('~/.config')
    else:
        prefix = '/etc'

    files = map(os.path.expanduser, [
        prefix+'/procServ.conf',
        prefix+'/procServ.d/*.conf',
    ])
    _log.debug('Config files %s', files)

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
