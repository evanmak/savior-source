#!/usr/bin/env python
import ConfigParser
import sys
import utils
from klee_explorer import *

def get_explorer_factory(config, proj_dir):
    """parse config to determine which explorer to initialize"""
    cfg = ConfigParser.ConfigParser()
    cfg.read(config)
    sections = cfg.sections()

    for _s in sections:
        if "klee" in _s:
            return KleeExplorers(config, proj_dir)
        if "s2e" in _s:
            pass
        if "angr" in _s:
            pass

    utils.error_msg("Can't find explorer options in config")
    sys.exit(-1)



