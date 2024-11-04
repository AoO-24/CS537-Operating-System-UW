#! /bin/env python

import sys, os, inspect

# Add the tools directory to the PYTHONPATH so we can find useful packages
# we've written there
thisDir = os.path.dirname(inspect.getfile(inspect.currentframe()))
toolsDir = os.path.abspath(os.path.join(thisDir,  '..'))
sys.path.insert(0, toolsDir)
