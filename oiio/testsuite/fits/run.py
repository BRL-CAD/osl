#!/usr/bin/python 

import os
import sys

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = [".."] + sys.path
import runtest

# Start off
hi = "echo hi"
command = hi + "> out.txt"


# ../fits-image/pg93:
# tst0001.fits to tst0014.fits
imagedir = "../../../fits-images/pg93"
command = command + "; " + runtest.rw_command (imagedir, "tst0001.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0002.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0003.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0005.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0006.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0007.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0008.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0009.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "tst0013.fits", path)

imagedir = "../../../fits-images/ftt4b"
command = command + "; " + runtest.rw_command (imagedir, "file001.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "file002.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "file003.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "file009.fits", path)
command = command + "; " + runtest.rw_command (imagedir, "file012.fits", path)

# Outputs to check against references
outputs = [ "out.txt" ]

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "tst0001.fits", "tst0002.fits", "tst0003.fits", "tst0005.fits",
               "tst0006.fits", "tst0007.fits", "tst0008.fits", "tst0009.fits",
               "tst0013.fits", "file001.fits", "file002.fits", "file003.fits",
               "file009.fits", "file012.fits" ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
