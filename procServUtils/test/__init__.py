
import os
from functools import reduce
from glob import glob

from shutil import rmtree
from tempfile import mkdtemp

from .. import conf

class TestDir(object):
    """Patch procServUtils.conf to use files from temp location
    """
    def __init__(self):
        self.dir = None

    def close(self):
        if self.dir is not None:
            rmtree(self.dir, ignore_errors=True)
            self.dir = None

            conf._testprefix = None

    def __enter__(self):
        self.dir = mkdtemp()
        try:
            os.makedirs(self.dir+'/run')
            os.makedirs(self.dir+'/procServ.d')

            conf._testprefix = self.dir

            return self
        except:
            self.close()
            raise

    def __exit__(self,A,B,C):
        self.close()
