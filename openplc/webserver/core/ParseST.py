import re
import copy
import json
import argparse
#the outputs are ../json/xxx_insn.json and xxx_vari.json

TYPE_VALUES = ['BOOL', 'BYTE', 'WORD', 'DWORD', 'LWORD',\
'INT', 'DINT', 'SINT', 'LINT', 'USINT', 'UINT', 'LDINT', 'ULINT',\
'TON', 'TOF', 'REAL', 'LREAL', 'R_TRIG', 'F_TRIG', 'STRING', \
'TIME', 'DATE', 'TIME_OF_DAY', 'DATE_AND_TIME','PID']

BOOL_VALUES = ['TRUE', 'FALSE']

LOGIC_VALUES = ['IF', 'CASE', 'FOR']

INSTR_VALUES = ['ADD', 'SUB', 'MUL', 'DIV','SIN','EQ', 'LT', 'GT', \
'LIMIT', 'BOOL_TO_REAL', 'BOOL_TO_INT', 'INT_TO_STRING','TIME_TO_REAL',\
'AND', 'OR', 'NOT','XOR', 'REAL_TO_DINT', 'UDINT_TO_UINT']

class Variable:
    def __init__(self, var_attr, name=None, data_type=None, initial_value=None, IO_map=None, program=None):
        self.name = name
        self.var_attr = var_attr #"VAR" "VAR_INPUT"
        self.data_type = data_type #"BOOL"
        self.initial_value = initial_value
        self.IO_map = IO_map;
        self.program = program;

class Program:
    def __init__(self, name, variables=None, instructions=None):
        self.name = name
        self.variables = variables or []
        self.instructions = instructions or []

class Configuration:
    def __init__(self, name, resources=None):
        self.name = name
        self.resources = resources or []

class Resource:
    def __init__(self, name, variables=None, tasks=None, programs=None, configuration=None):
        self.name = name
        self.configuration = configuration
        self.variables = variables or []
        self.tasks = tasks or []
        #self.programs = programs or []

class Task:
    def __init__(self, name, interval=None, priority=None, resource = None):
        self.name = name
        self.interval = interval
        self.priority = priority
        self.resource = resource

class Instruction:
    def __init__(self, name, program=None, input_vars=None, output_vars=None):
        self.name = name
        self.program = program or []
        self.input_vars = input_vars or []
        self.output_vars = output_vars or []

# get the ending index of the module with current keyword
# for example: 
# start_i: IF ...
#          ...
# end_i  : END_IF
def record_module(lines, start_i, keyword):
	end_i = 0
	end_keyword = 'END_'+keyword
	stack = 1
	for i in range(start_i+1, len(lines)):
		if lines[i].strip().startswith(keyword):
			stack += 1
		if lines[i].strip().startswith(end_keyword):
			stack -= 1
		if stack == 0:
			end_i = i
			break
	return end_i


def clean_labels(string):
	return string.strip().strip(";").strip(',')

#for VAR definition:
# xxx AT %IO_map : TYPE_VALUES := initial_value
# xxx : TYPE_VALUES := initial_value
def parse_VAR(lines, program_name, var_attr):
	variables = []
	for line in lines:
		line = line.strip()
		linelist = line.split()
		tvar = Variable(var_attr)
		tvar.program = program_name
		if "AT" in linelist:
			tindex = linelist.index("AT")
			varname = linelist[tindex - 1]
			IOmap = linelist[tindex + 1]
			tindex2 = linelist.index(":")
			vartype = clean_labels(linelist[tindex2 + 1])
			tvar.name = varname
			tvar.IO_map = IOmap
			tvar.data_type = vartype
		else:
			tindex = linelist.index(":")
			varname = linelist[tindex - 1]
			vartype = clean_labels(linelist[tindex + 1])
			tvar.name = varname
			tvar.data_type = vartype
			
		if ":=" in linelist:
			tindex = linelist.index(":=")
			varinit = clean_labels(linelist[tindex + 1])
			tvar.initial_value = varinit
		variables.append(tvar)
	return variables


# IF aaa THEN
#    bbb := ccc
#    IF ddd THEN
#       eee := fff
#    END_IF
# END_IF
# so that there are two instructions:
# one is the first IF, with the inputs are aaa and ccc; the output is bbb
# the other is the second IF, with the inputs are aaa, ddd, and fff; the output is eee
def parse_LOGIC(lines, program_name, lg, variables):
	instructions = []
	exinputs = []
	excount = []
	logics = []
	varis = [vari.name for vari in variables]
	curins = None
	for line in lines:
		tline = re.findall(r'\b\w+(?:#\w+)*\b|\b\w+(?=\.\D)\b', line)
		if tline[0] in varis: 
			#A:= MUL(B,C) or A(B,C), so the first variable must be the output
			#the other variables must be the inputs
			tins = Instruction(lg)
			tins.program = program_name
			tins.output_vars.append(tline[0])
			tins.input_vars += exinputs
			for item in tline[1:]:
				if item in varis:
					tins.input_vars.append(item)
			instructions.append(tins)
		# inside LOGICs
		elif tline[0] in LOGIC_VALUES:
			logics.append(tline[0])
			ecount = 0
			for item in tline[1:]:
				if item in varis:
					exinputs.append(item)
					ecount += 1
			excount.append(ecount)
		elif tline[0] == 'END_' + logics[-1]:
			for i in range(excount[-1]):
				exinputs.pop()
			excount.pop()
			logics.pop()
		else:
			for item in tline:
				if item in varis:
					exinputs.append(item)
					excount[-1] += 1
			
	return instructions
			
				
# find the modules of VARs or LOGIC INSTRUCTIONs
def parse_program(lines, program_name):
	variables = []
	instructions = []
	i=0
	while(i < len(lines)):
		tline = re.findall(r'\b\w+(?:#\w+)*\b|\b\w+(?=\.\D)\b', lines[i])
		if len(tline) > 0 and tline[0].startswith("VAR"):
			end_i = record_module(lines, i, "VAR")
			#print("curlines for var: ", lines[i+1:end_i])
			curvars = parse_VAR(lines[i+1:end_i], program_name, lines[i].strip())
			variables += curvars
			#for curvar in curvars:
				#print(curvar.name)
				#print(curvar.data_type)
				#print(curvar.initial_value)
			i = end_i + 1
			continue
		if len(tline) > 0 and tline[0] in LOGIC_VALUES:
			end_i = record_module(lines, i, tline[0])
			curins = parse_LOGIC(lines[i:end_i], program_name, tline[0], variables)
			instructions += curins
			#print("curlines for cur ", tline[0], ": ", lines[i:end_i])
			#for crins in curins:
				#print(crins.name)
				#print(crins.input_vars)
				#print(crins.output_vars)
			i = end_i + 1
			continue
		#print("curlines in program: ",lines[i])
		
		if len(tline) > 1:
			
			varis = [vari.name for vari in variables]
			tinsname = None
			for item in INSTR_VALUES:
				if item in tline:
					tinsname = item
			if tinsname is None:
				tinsname = 'EQUAL'
			tins = Instruction(tinsname)
			tins.program = program_name
			tins.output_vars.append(tline[0])
			for item in tline[1:]:
				if item in varis:
					tins.input_vars.append(item)
			instructions.append(tins)		
			#print(tins.name)
			#print(tins.input_vars)
			#print(tins.output_vars)
		i+=1
	return variables, instructions

# find the modules of VARs and TASKs in resource
def parse_resource(lines, resource_name):
	variables = []
	tasks = []
	i = 0
	
	while(i < len(lines)):
		if lines[i].strip().startswith("VAR"):
			end_i = record_module(lines, i, "VAR")
			#print("curlines for var: ", lines[i+1:end_i])
			curvars = parse_VAR(lines[i+1:end_i], resource_name, lines[i].strip())
			variables += curvars
			#for curvar in curvars:
				#print(curvar.name)
				#print(curvar.data_type)
				#print(curvar.initial_value)
			i = end_i + 1
			continue
		if lines[i].strip().startswith("TASK"):
			tline = re.findall(r'\b\w+(?:[#.]\w+)*\b', lines[i])
			curTask = Task(tline[1])
			curTask.interval = tline[3]
			curTask.priority = tline[5]
			curTask.resource = resource_name
			tasks.append(curTask)
		i+=1
	return variables, tasks

def parse_configuration(lines, config_name):
	current_resource = None
	variables = []
	tasks = []
	i=0
	while(i<len(lines)):
		if lines[i].strip().startswith("RESOURCE"):
			resource_name = re.search(r'RESOURCE (\w+)', lines[i]).group(1)
			current_resource = Resource(resource_name)
			current_resource.configuration = config_name
			end_i = record_module(lines, i, "RESOURCE")
			curvars, curtasks = parse_resource(lines[i+1:end_i], resource_name)
			current_resource.variables = curvars
			current_resource.tasks = curtasks
			variables += curvars
			tasks += curtasks
			#print(resource_name)
			#print(end_i)
			i = end_i + 1
			continue
		i+=1
	return current_resource, variables, tasks

def merge_variables(vari1,vari2):
	vari2names = [ var.name for var in vari2]
	newvaris = []
	for var in vari1:
		if var.name in vari2names:
			newvaris.append(vari2[vari2names.index(var.name)])
		else:
			newvaris.append(var)
	return newvaris
		

def parse_ST_file(ST_file):
	programs = []
	variables = []
	configurations = []
	instructions = []
	resources = []
	tasks = []
	current_program = None
	current_configuration = None
	with open(ST_file, 'r') as f:
		lines = f.readlines()
	
	i=0
	while(i < len(lines)):
		if lines[i].strip().startswith("PROGRAM"):
			program_name = re.search(r'PROGRAM (\w+)', lines[i]).group(1)
			current_program = Program(program_name)
			end_i = record_module(lines, i, "PROGRAM")
			#print('curindex for program:',end_i)
			#print('curlines for program:',lines[i+1:end_i])
			curvariables, curinstructions = parse_program(lines[i+1:end_i], program_name)
			current_program.variables = curvariables
			current_program.instructions = curinstructions
			variables += curvariables
			instructions += curinstructions 
			programs.append(current_program)
			i = end_i + 1
			continue
		if lines[i].strip().startswith("CONFIGURATION"):
			configuration_name = re.search(r'CONFIGURATION (\w+)', lines[i]).group(1)
			current_configuration = Configuration(configuration_name)
			end_i = record_module(lines, i,"CONFIGURATION")
			curresource, curvaris, curtasks = parse_configuration(lines[i+1:end_i], configuration_name);
			#print('curindex for config:',end_i)
			#print('curlines for config:',lines[i+1:end_i])
			current_configuration.resource = curresource
			configurations.append(current_configuration)
			tasks += curtasks
			resources.append(curresource)
			variables = merge_variables(variables, curvaris)
			i = end_i + 1
			continue
		i+=1
	return programs, configurations, variables, instructions, resources, tasks

def insn_data_to_dict(data, i):
    return {
        'insn_id': i,
        'type': 'Instruction',
        'name': data.name,
        'input_vars': data.input_vars,
        'output_vars': data.output_vars,
        'program': data.program
    }
def vari_data_to_dict(data, i):
    return {
        'insn_id': i,
        'type': 'Variable',
        'name': data.name,
        'var_attr': data.var_attr,
        'data_type': data.data_type,
        'initial_value': data.initial_value,
        'IO_map': data.IO_map,
        'program': data.program
    }

def main():
    parser = argparse.ArgumentParser(description='parse st file to json.')
    parser.add_argument('STfile', type=str, help='st file')
    parser.add_argument('STname', type=str, help='st file name')
    args = parser.parse_args()
    
    programs, configurations, variables, instructions, resources, tasks = parse_ST_file(args.STfile)
    #printall(programs, configurations, variables, instructions, resources, tasks)
    
    data_dict_insn = []
    for i in range(len(instructions)):
        if instructions[i]:
            data_dict_insn.append(insn_data_to_dict(instructions[i],i))
    data_dict_vari = []
    for i in range(len(variables)):
        if variables[i]:
            data_dict_vari.append(vari_data_to_dict(variables[i],i))
    insnjson = args.STname.replace(".st","_insn.json")
    varijson = args.STname.replace(".st","_vari.json") #wave_generator_vari.json
    with open("../json/"+insnjson,"w+") as f1:
        json.dump(data_dict_insn, f1, indent=4)
    with open("../json/"+varijson,"w+") as f2:
        json.dump(data_dict_vari, f2, indent=4)
        


if __name__ == '__main__':
    main()
    
# Example printing all programs, variables, and configurations
def printall(programs, configurations, variables, instructions, resources, tasks):
	for program in programs:
		print("Program:", program.name)
		print("Variables:")
		for var in program.variables:
			print(f"- {var.name}")
		for insn in program.instructions:
			print(f"- {insn.name}")


	print("Variables:")
	for var in variables:
		print(f"- {var.name}: {var.data_type} = {var.initial_value} which is {var.var_attr} and its IO map is {var.IO_map}")
	print()

	print("Instructions:")
	for insn in instructions:
		print(f"- {insn.name}: {insn.output_vars} = {insn.input_vars}")
	print()



	for config in configurations:
		print("Configuration:", config.name)
		print("Resources:")
		for resource in config.resources:
			print("- Resource:", resource.name)
			print("Global Variables:")
			for var in resource.variables:
				print(f"  - {var.name}: {var.data_type} = {var.initial_value} which is {var.var_attr} and its IO map is {var.IO_map}")
			for task in resource.tasks:
				print(f"  - {task.name}: {task.interval}: {task.priority} ")
	  

