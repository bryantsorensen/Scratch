#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# Python script for testing the FBC modules in the C code model
#
# Novidan, Inc. (c) 2023.  May not be used or copied with prior consent
# Bryant Sorensen
# Started 06 Jun 2023
#
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#+++++++++++++++++++
# Test setup, here at top of file.  Everything needed to run this test, differentiating
# if from other tests, should be included here.
#
# Test file names

infile_name = 'whitenoise_m40dBFS_7sec.wav'
outfile_name = 'FBC_test_out.wav'
fbsim_fname = 'lowfreq1_m5dB__dual_bump2_m6dB_FBSIM.txt'      # Set this to '' if FB sim not needed for test
param_fname = 'FBC_Test.json'

#+++++++++++++++++++
#Imports

import sys
import subprocess as subpr
import os
#import numpy
#import matplotlib.pyplot as mplot
#import math
#import soundfile
from csv import reader as csvread
import datetime
#from scipy.signal import freqz

#+++++++++++++++++++
# Set up directories and common names

repo_dir = os.getenv('FW_REPO_DIR')

c_model_name = 'Fxp_C_Model'
build_config = 'Release'    # Alternatives: Release, Debug
build_platform = 'x64'      # Alternatives: Win32, x64

thisdir = os.path.dirname(__file__)
testfilename = os.path.basename(__file__)
testname = os.path.splitext(testfilename)[0]

param_vals_dir = os.path.join(repo_dir, 'ParamVals')
param_defs_dir = os.path.join(repo_dir, 'ParamDefs')
c_code_dir = os.path.join(repo_dir, c_model_name)
exe_dir = os.path.join(c_code_dir, build_config)
scripts_dir = os.path.join(repo_dir, 'Scripts')
test_dir = os.path.join(repo_dir, 'Tests')
test_inputs_dir = os.path.join(test_dir, 'Input_Files')
fbsim_specs_dir = os.path.join(test_inputs_dir, 'FBSimFiles')

sys.path.append(scripts_dir)            # Add scripts to path dynamically
import create_param_init_c_code as ic   # Import custom scripts

#+++++++++++++++++++
# Create the initialization C code from the parameter file

param_val_fname = os.path.join(thisdir, param_fname)
profile_num = '1'
out_fname = os.path.join(c_code_dir, 'FW_Param_Init.cpp')

ic.create_param_init_c_code(param_val_fname, profile_num, param_defs_dir, out_fname)

#+++++++++++++++++++
# Build the C code model, now that we have the init C code in the project directory
# NOTE: MSBuild.exe _must_ be on the system path. 
#   -- User must add the directory (which can differ from user to user) to the 'Path' environment variable

os.chdir(c_code_dir)
c_build = "MSBuild.exe " + c_model_name + ".sln /p:Configuration=" + build_config + " /property:Platform=" + build_platform + " /verbosity:quiet"

rval = subpr.call(c_build, shell=True)
if (rval != 0):
    print ('Error in build call!\n')
    exit(rval)

#+++++++++++++++++++
# Execute the C code model, create the desired outputs

# Go to .exe directory
if build_platform == 'Win32':
    exe_subdir = build_config
else:
    exe_subdir = os.path.join('x64', build_config)
exe_dir = os.path.join(c_code_dir, exe_subdir)

os.chdir(exe_dir)
exefile_name = os.path.join(exe_dir, c_model_name+'.exe')

# Point to the results directory; create it if it doesn't exist
resultpath = os.path.join(thisdir, "Results")
if not os.path.exists(resultpath):
    os.makedirs(resultpath)

# Create command line string
infile_path = os.path.join(test_inputs_dir, infile_name)    # Create full path to input .wav file

c_exe_cmd = exefile_name + " -s " + infile_path + " -r " + resultpath
if fbsim_fname != '':       # Add extra option if this test requires FB simulation file
    fbsim_fpath = os.path.join(fbsim_specs_dir, fbsim_fname)
    c_exe_cmd = c_exe_cmd + " -f " + fbsim_fpath

# Call the C code simulation .exe
rval = subpr.call(c_exe_cmd, shell=True)
if (rval != 0):
    print ('Error in exe call!\n')
    exit (rval)

#+++++++++++++++++++
os.chdir(thisdir)


