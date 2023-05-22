#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# Python script to create C code that initializes parameters as
# specified in a JSON file
#
# Novidan, Inc. (c) 2023.  May not be used or copied with prior consent
# Bryant Sorensen
# Started 02May2023
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

import os
import json
from datetime import datetime
import time
import math

from param_conversions import convert_param_from_user_to_fw

def create_param_init_c_code(val_inp_fname, profile_num, param_defs_dir, out_fname):
# Create file header
    # Get timestamp for file creation start
    curtime = datetime.now()
    time_string = curtime.strftime("%d-%m-%Y %H:%M:%S")
    tz = str(-int(time.timezone*100/(60*60)))    # Convert seconds to hours for time zone offset, convert to military time
    timestamp = time_string + ' ' + tz + " UTC"
    fstr = '//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n//\n'
    fstr = fstr + '// Parameter initialization code\n'
    fstr = fstr + '//   Auto-Created Starting at ' + timestamp + '\n'
    fstr = fstr + '//   Created from input file: ' + os.path.basename(val_inp_fname) + '\n'
    fstr = fstr + '//   Uses Profile ' + profile_num + '\n//\n'
    fstr = fstr + '// Novidan, Inc. (c) 2023.  May not be used or copied with prior consent.\n//\n'
    fstr = fstr + '//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n'
    fstr = fstr + 'void FW_Param_Init()\n{\n'
# Read the parameter value JSON
    fv = open(val_inp_fname)
    param_vals = json.load(fv)
    fv.close()
# Loop through each module in the file
    for module_name in param_vals:      # Loop through each module defined in file
        fstr = fstr + '// ' + module_name + ' Parameters\n'
    # Open and read the param defintion JSON file for this module
        module_def_fname = module_name + "_ParamDef.json"
        module_def_path = os.path.join(param_defs_dir, module_def_fname)
        fm = open(module_def_path)
        param_def = json.load(fm)
        fm.close()
        param_def = param_def[module_name]      # TODO: Some redundancy in file name and having module name in param def file
    # Read the parameters from the param value file
        for memory in param_vals[module_name]:      # Loop through 0 (persist), then 1-4 (profiles)
            mem_list = None     # Default the list of params in the memory to be None
            if memory == '0':
                val_bname = module_name + '_Params.Persist.'      # Create value base name
                mem_list = param_vals[module_name][memory]      # Mark as valid memory list
                def_list = param_def['Persist']
            else:
                if memory == profile_num:     # Extract only the profile specified
                    val_bname = module_name + '_Params.Profile.'
                    mem_list = param_vals[module_name][memory]      # Mark as valid memory list
                    def_list = param_def['Profile']
            if mem_list is not None:  # If we've found a valid memory
                fstr = fstr + '\n'
                for param_name in mem_list:      # Loop over all parameters found in memory list
                    if '[' in param_name:
                        def_param_name = param_name.split('[')[0]   # Remove [] from param name for lookup in definition dict
                    else:
                        def_param_name = param_name
                    user_value = mem_list[param_name]
                # Convert the parameter from user units to FW units
                    fw_value = convert_param_from_user_to_fw(user_value, def_list[def_param_name], def_param_name)
                    if math.isnan(fw_value):
                        print ('FW value returned is NaN! ' + module_name + ':' + param_name)
                    val_name = val_bname + param_name
                    fbits = def_list[def_param_name]['FractBits']
                    cast_str = ''   # default to no cast string
                    endbracket = '' # default to no end bracket
                    if fbits != '' and fbits != 0:
                        if fbits == 23:
                            cast_str = 'to_frac24('
                            endbracket = ')'
                        elif fbits == 16:
                            cast_str = 'to_frac16('
                            endbracket = ')'
                    fstr = fstr + '    ' + val_name + ' = ' + cast_str + str(fw_value) + endbracket + ';\n'
    # Go to next module with space between each modules' params
        fstr = fstr + '\n'
# Write to and close the output file
    fstr = fstr + '}\n'
    fo = open(out_fname, 'w')
    fo.write(fstr)
    fo.close()

