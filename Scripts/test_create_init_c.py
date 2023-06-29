#++++++++++++++++++++++++++++++++++++++
# Python code to test creation of FW param init C code
#++++++++++++++++++++++++++++++++++++++

import os
import create_param_init_c_code as ic

repository_dir = 'C:\Work\RepositoriesCopies'
param_vals_dir = os.path.join(repository_dir, 'ParamVals')
param_defs_dir = os.path.join(repository_dir, 'ParamDefs')
c_code_dir = os.path.join(repository_dir, 'Fxp_C_Model')

#param_val_fname = os.path.join(param_vals_dir, 'FBC_Test.json')
param_val_fname = 'C:\Work\RepositoriesCopies\Tests\WDRC_GainCurve\WDRC_TC_test.json'
profile_num = '1'
out_fname = os.path.join(c_code_dir, 'FW_Param_Init.cpp')

ic.create_param_init_c_code(param_val_fname, profile_num, param_defs_dir, out_fname)
