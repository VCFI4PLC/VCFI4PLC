import json
import re
import argparse
SL0 = 1000
SL1 = 100
SL2 = 10
class Variable:
	def __init__(self, name, cor=0, priority=0):
		self.name = name
		self.cor = cor
		self.priority = priority
class Instruction:
	def __init__(self, name, cor=0, priority=0):
		self.name = name
		self.cor = cor
		self.priority = priority

class Function:
	def __init__(self, name, count=0, cor=0, priority=0):
		self.name = name
		self.count = count
		self.cor = cor
		self.priority = priority	

def count_ss(f):
	total_counts = 0
	for item in f:
		total_counts += item['count']
	return total_counts

def print_priority(variables):
	for var in variables:
		print(f"- {var.name}: {var.cor} : {var.priority}")

def stat_Varis(vari_f):
	variables = []
	for item in vari_f:
		vari = Variable(item['name'])
		if item['var_attr'] == 'VAR':
			vari.priority += SL2
		else:
			vari.priority += SL1
		
		if item['IO_map'] is not None:
			vari.priority += SL0
		
		variables.append(vari)
	return variables


def stat_Insn(insn_f, variables):
	instructions = []
	insn_names = []
	for item in insn_f:
		insn = Instruction(item['name'])
		input_vars = item['input_vars']
		for invar in input_vars:
			for var in variables:
				if var.name == invar:
					var.cor += 1
					break
		output_vars = item['output_vars']
		for ouvar in output_vars:
			for var in variables:
				if var.name == ouvar:
					var.cor += 2
					break
		if item['name'] not in insn_names:
			instructions.append(insn)
	return instructions


def stat_Insn_Prior(insn_f, instructions, variables):
	for item in insn_f:
		input_vars = item['input_vars']
		cur_insn = None
		for insn in instructions:
			if insn.name == item['name']:
				cur_insn = insn
				break
		#if cur_insn is None:
			#print('No find cur_insn: ',item['name'])
		#else:
			#print('current insn is: ',cur_insn.name)		
		for invar in input_vars:
			for var in variables:
				if var.name == invar:
					#print('    input var is:', invar)
					cur_insn.cor += var.cor
					cur_insn.priority += var.cor*var.priority
					break
				
		output_vars = item['output_vars']
		for ouvar in output_vars:
			for var in variables:
				if var.name == ouvar:
					#print('    output var is:', ouvar)
					cur_insn.cor += var.cor
					cur_insn.priority += var.cor*var.priority
					break
	return instructions
	

def parse_INSN_NAME(insn_name):
	if insn_name[-1].isdigit():
		insn_name = re.sub(r'\d+$','',insn_name)
	return insn_name

def stat_Func(func_f, instructions):
	functions = []
	for item in func_f:
		func = Function(item['name'])
		func.count = item['count']
		for insn_name in instructions:
			#print('insn name is:', insn_name.name)
			#print('func name is:', func.name)
			if func.name.startswith(parse_INSN_NAME(insn_name.name)):
				func.cor = insn_name.priority
				func.priority = insn_name.priority
				break
		functions.append(func)
	return functions


def stat_Func_Prior(func_f, functions):
	for item in func_f:
		cur_func = None
		for func in functions:
			if func.name == item['name']:
				cur_func = func
				break
		src_funcs = item['source_funcs']
		for src_func in src_funcs:
			for func in functions:
				if func.name == src_func:
					#print('    source func is:', src_func)
					cur_func.priority += cur_func.cor + func.cor
					break
	return functions

def data_to_dict(data):
    return {
        'name': data.name,
        'count': data.count,
        'priority': data.priority
    }
    
def rewrite_Json(functions, new_file):
	data_json = [data_to_dict(item) for item in functions]

	with open(new_file, 'w+') as file:
		json.dump(data_json, file, indent=4)

def main():
	parser = argparse.ArgumentParser(description='calucate the priority of the functions in st file.')
	parser.add_argument('oriJson', type=str, help='original json from compiled file.')
	parser.add_argument('stInsn', type=str, help='the insn json of st file')
	#parser.add_argument('stVari', type=str, help='the vari json of st file')
	args = parser.parse_args()
	
	
	with open('../json/'+args.oriJson+'.json', 'r') as f2:
		config_data = json.load(f2)
		
	with open('../json/'+args.stInsn.replace('.st','_insn.json'), 'r') as f3:
		st_insn = json.load(f3)

	with open('../json/'+args.stInsn.replace('.st','_vari.json'), 'r') as f4:
		st_vari = json.load(f4)
	variables = stat_Varis(st_vari)
	instructions = stat_Insn(st_insn, variables)
	instructions = stat_Insn_Prior(st_insn, instructions, variables)

	functions = stat_Func(config_data, instructions)	
	functions = stat_Func_Prior(config_data, functions)		
	rewrite_Json(functions, '../json/'+args.oriJson+'_new.json')

	
if __name__ == '__main__':
	main()

#print(count_ss(res_data)) #169
#print(count_ss(config_data)) #148
#print_priority(variables)	

	
#print("---------------------------------")

#print(parse_INSN_NAME("asdasf13"))
#print_priority(instructions)	

#print_priority(functions)
