#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# Python script to create C code structures in .h files for module
# parameters from JSON definition files
#
# Novidan, Inc. (c) 2023.  May not be used or copied with prior consent
# Bryant Sorensen
# Started 01May2023
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

import os
import json
from datetime import datetime
import time

# Point to different directories; base them all off repository directory
repository_dir = 'C:\Work\RepositoriesCopies'
param_json_dir = os.path.join(repository_dir, 'ParamDefs')
c_shared_dir = os.path.join(repository_dir, 'Shared')
c_struct_outdir = os.path.join(c_shared_dir, 'include')
DirDoesExist = os.path.exists(c_struct_outdir)
if not DirDoesExist:
    os.makedirs(c_struct_outdir)    # Create a new directory because it does not exist

# Get timestamp for file creation start
curtime = datetime.now()
time_string = curtime.strftime("%d-%m-%Y %H:%M:%S")
tz = str(-int(time.timezone*100/(60*60)))    # Convert seconds to hours for time zone offset
timestamp = time_string + ' ' + tz + " UTC"

cwd = os.getcwd()   # Current working directory
# Go to param JSON directory. Loop through all JSON files there
os.chdir(param_json_dir)
for file in os.listdir(param_json_dir):
    param_json_fname = os.fsdecode(file)
    if param_json_fname.endswith(".json"):
    # Booleans indicating existence of each parameter space; default to non-existence
        hasPersist = False
        hasProfile = False
    # Open the file and read the JSON into a dictionary
        fj = open(param_json_fname)
        param_dict = json.load(fj)
        fj.close()
        for module_name in param_dict:      # Loop through each module defined in file
            filestr = '//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n'
            filestr = filestr + '//\n' + '// ' + module_name + ' Parameter Structures\n'
            filestr = filestr + '//   Auto-Created Starting at ' + timestamp + '\n'
            filestr = filestr + '//   Created from ' + os.path.basename(param_json_fname) + '\n//\n'
            filestr = filestr + '// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.\n'
            filestr = filestr + '//\n' + '//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n'
            module = param_dict[module_name]
            for space_name in module:
            # Create a structure for persistent and a structure for profile params
                space = module[space_name]
                if (space_name == 'Persist'):
                    hasPersist = True
                    filestr = filestr + 'struct Persist_' + module_name + '\n{\n'
                elif (space_name == 'Profile'):
                    hasProfile = True
                    filestr = filestr + 'struct Profile_' + module_name + '\n{\n'
                for param in space:
                    pvals = space[param]
                    if (pvals['FractBits'] == 0):
                        ptype = 'int24_t '
                    else:
                        if (pvals['FractBits'] == 23):
                            ptype = 'frac24_t'
                        else:
                            ptype = 'frac' + str(pvals['FractBits']) + '_t'
                    NumElements = pvals['Elements']
                    if NumElements != 1:    # If array, Elements is not 1, so tack onto end of param declaration
                        if isinstance(NumElements, int):    # if element value is integer, convert to string
                            NumElements = str(NumElements)
                            param = param + '[' + NumElements + ']'
                        elif isinstance(NumElements, str):
                            if '[' in NumElements:      # if already have braces
                                param = param + NumElements
                            else:
                                param = param + '[' + NumElements + ']'
                    filestr = filestr + '    ' + ptype + '    ' + param + ';\n'
                filestr = filestr + '};\n\n'
        # Top level structure
            filestr = filestr + 'typedef struct _Params_' + module_name + '\n' + '{\n'
            if (hasPersist):
                filestr = filestr + '    struct Persist_' + module_name + '    Persist;\n'
            if (hasProfile):
                filestr = filestr + '    struct Profile_' + module_name + '    Profile;\n'
            filestr = filestr + '\n} strParams_' + module_name + ';\n'
        # Write out the .h file containing the param structure for this module
            c_struct_hname = module_name + '_ParamStruct.h'
            cstruct_fname = os.path.join(c_struct_outdir, c_struct_hname)
            outf = open(cstruct_fname, 'w')
            outf.write(filestr)
            outf.close()
os.chdir(cwd)       # Return to starting directory