#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
# Python script to convert parameter values between units used in
# C code FW, and audiology or fitting units used in GUI
#
# Novidan, Inc. (c) 2023.  May not be used or copied with prior consent
# Bryant Sorensen
# Started 02May2023
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

import math

MAX_FRAC_BITS = 23      # for this system, use 24b params, so 23 fractional bits

# TODO: Coordinate these constants between C code and Python
WHATEVER = 3.0
BB_SAMPLE_RATE = 24000.0
SB_SAMPLE_RATE = (BB_SAMPLE_RATE/8.0)
LOG2_TO_DB20 = math.log10(2.0)*20.0
DB20_TO_LOG2 = 1.0/LOG2_TO_DB20
LOG2_TO_DB10 = math.log2(2.0)*10.0
DB10_TO_LOG2 = 1.0/LOG2_TO_DB10

def convert_param_from_user_to_fw(userval, param_def_info, def_param_name):
# Get the parameter info from the following keys of param_def_info
#    "UserMax" - convert to FW units and limit at top
#    "UserMin" - convert to FW units and limit at bottom
#    "List" - if this is not empty, extract values and check for validity
#    "FractBits" - to scale to integer, or provide default limits; if 0, value is integer
#    "DSPConvert" - either scale factor, or named conversion

# Make return value and checks invalid by default, to ensure they get corrected (or else script will fail)
    fw_value = float('NaN')
    fwmax = float('NaN')
    fwmin = float('NaN')
# Get the # of fractional bits
    fracbits = param_def_info['FractBits']
    if fracbits == '':  # not specified, use 0 --> integer value
        fracbits = 0

# If there is no max or min, use FractBits to set limits in FW directly
    if param_def_info['UserMax'] == '' or param_def_info['UserMin'] == '':
        ibits = MAX_FRAC_BITS - fracbits
        epsbit = 2**(-fracbits)
        fwmax = (2.0**ibits) - epsbit
        fwmin = -(2.0**ibits)
        NeedFwLimits = False
        usermax = 0     # Clear these out to have them defined; should create error downstream
        usermin = 0
    else:
        usermax = param_def_info['UserMax']
        usermin = param_def_info['UserMin']
        NeedFwLimits = True

#Parse the list values; the labels are only used by the fitting SW
# If there is a valid list, make sure the user value matches a valid list entry
    listvals = param_def_info['List']
    validvals = []
    if  listvals != '':
        if isinstance(listvals, int) or isinstance(listvals, float):    # if a number, use list as step between usermin and usermax
            step = listvals
            curval = usermin
            while curval <= usermax:
                validvals.append(curval)
                curval = curval + step
        else:
            for item in listvals:
                lv = int(item.split('=')[0])    # Get the value, convert to int
                validvals.append(lv)
        if userval not in validvals:
            print ('User value not found in list!')
            userval = float('NaN')      # Give it an invalid value

# Get the converter.  If it has braces in it, it needs to be evaluated and converted to a number
    convert_val = param_def_info['DSPConvert']
    if isinstance(convert_val, str) and '(' in convert_val and ')' in convert_val:
        convert_val = eval(convert_val)

# Perform the conversion from User units to FW units
    if convert_val == '':     # no conversion --> user = fw
        fw_value = userval
        if NeedFwLimits:
            fwmax = usermax
            fwmin = usermin
    elif isinstance(convert_val, str):     # if string, use conversion functions
        if convert_val == 'TestConv':
            fw_value = 1.0 - userval
            if NeedFwLimits:
                fwmax = 1.0 - usermax
                fwmin = 1.0 - usermin
        elif convert_val == 'AnsiTC':
            fw_value = 1.0 - math.exp(-1.0/(userval/SB_SAMPLE_RATE))
            if NeedFwLimits:
                fwmax = 1.0 - math.exp(-1.0/(usermax/SB_SAMPLE_RATE))
                fwmin = 1.0 - math.exp(-1.0/(usermin/SB_SAMPLE_RATE))

    # TODO: Add other conversion functions here as needed

    else:   # Scale value
        fw_value = userval * convert_val
        if NeedFwLimits:
            fwmax = usermax * convert_val
            fwmin = usermin * convert_val

# Perform boundary limit checks on the value
    if (fwmax < fwmin):     # Make sure ordering is correct once we convert to FW units
        tmp = fwmax
        fwmax = fwmin
        fwmin = tmp
    if fw_value > fwmax:
        print ('Out of bounds above max! Limiting ' + def_param_name)
        fw_value = fwmax
    elif fw_value < fwmin:
        print ('Out of bounds below min! Limiting ' + def_param_name)
        fw_value = fwmin

# TODO: Use param_def_info['FractBits'] to create scale factor to convert to integer if t
#  the compiler doesn't automatically convert double to fractional / integer

    if fracbits == 0:       # if integer, convert it to such
        fw_value = int(fw_value)
    return fw_value


