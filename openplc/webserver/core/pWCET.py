import json
import re
import numpy as np
from scipy.stats import genextreme
from scipy.stats import genpareto
import argparse
import matplotlib.pyplot as plt
import glob

#print("cycle max is", np.max(cycle_times)) #cycle max is 307873
#print("delay max is", np.max(delay_times)) #delay max is 61

def pWCET_normal_distribution(ex_times):
	time_mean = np.mean(ex_times)
	time_std = np.mean(ex_times)
	pwcet_distri = np.random.normal(time_mean,time_std,size=10000)
	threshold = np.percentile(pwcet_distri, 99)
	pWCET_result = np.mean(pwcet_distri>threshold)
	
	#print("Estimated pWCET using normal distribution:", threshold)
	#print("Probability of exceeding pWCET:", pWCET_result)
	return threshold

#pWCET_normal_distribution(cycle_times)
#pWCET_normal_distribution(delay_times)

#wave2000teeplc
#Estimated pWCET(99th percentile): 898734.5292405431
#Probability of exceeding pWCET: 0.01
#Estimated pWCET(99th percentile): 105.15071891536556
#Probability of exceeding pWCET: 0.01

def pWCET_EVT(ex_times):
	params = genextreme.fit(ex_times)
	shape, loc, scale = params
	pWCET_result = genextreme.ppf(0.99,shape,loc=loc,scale=scale)
	#print("Estimated pWCET using EVT", pWCET_result)
	return pWCET_result


#pWCET_EVT(cycle_times)
#pWCET_EVT(delay_times)
#Estimated pWCET using EVT 1756142406.8305714
#Estimated pWCET using EVT 68.12242582755857


def pWCET_GPD(ex_times, GPD_threshold):
	ex_times = np.array(ex_times)
	if GPD_threshold > np.max(ex_times):
		#print("Error, no extreme value.")
		return 0
	extreme_values = ex_times[ex_times>GPD_threshold]
	params = genpareto.fit(extreme_values)
	shape, loc, scale = params
	pWCET_result = genpareto.ppf(0.99,shape,loc=loc,scale=scale)
	#print("Estimated pWCET using GPD", pWCET_result)
	return pWCET_result
	
#pWCET_GPD(cycle_times, 300000)
#pWCET_GPD(delay_times, 50)
#Estimated pWCET using GPD 307872.9899739805
#Estimated pWCET using GPD 60.98555336953432

def pWCET_MonteCarlo(ex_times):
	execution_times=[]
	for _ in range(len(ex_times)):
		execution_times.append(np.random.choice(ex_times))
	pWCET_result = np.percentile(execution_times,99)
	#print("Estimated pWCET using MonteCarlo", pWCET_result)
	return pWCET_result

#pWCET_MonteCarlo(cycle_times)
#pWCET_MonteCarlo(delay_times)
#Estimated pWCET using MonteCarlo 301278.0
#Estimated pWCET using MonteCarlo 52.0

def pWCET_GEV(ex_times, GEV_threshold):
	ex_times = np.array(ex_times)
	if GEV_threshold > np.max(ex_times):
		#print("Error, no extreme value.")
		return 0
	extreme_values = ex_times[ex_times>GEV_threshold]
	params = genextreme.fit(extreme_values)
	shape, loc, scale = params
	pWCET_result = genpareto.ppf(0.99,shape,loc=loc,scale=scale)
	#print("Estimated pWCET using GEV", pWCET_result)
	return pWCET_result
	
#pWCET_result=pWCET_GEV(cycle_times,300000)
#pWCET_result=pWCET_GEV(delay_times,50)
#Estimated pWCET using GEV 301179.35179916414
#Estimated pWCET using GEV 51.000000000000014

def block_maxima(data, block_size):
	num_blocks = len(data) // block_size
	max_values = []
	for i in range(num_blocks):
		block = data[i*block_size:(i+1)*block_size]
		max_values.append(np.max(block))
	return np.array(max_values)

def estimate_para(data):
	return genextreme.fit(data)
	
def pWCET_GEV_BM(data, block_size):
	max_values = block_maxima(data, block_size)
	params = estimate_para(max_values)
	return params[1]

#block_size=50
#cycle_params = pWCET_GEV_BM(cycle_times, block_size)
#delay_params = pWCET_GEV_BM(delay_times, block_size)
#print("cycle_params is ", cycle_params[1])
#print("delay_params is ", delay_params[1])
#cycle_params is  (-1.4425705157908257, 301249.2148291698, 0.39752124589824944)
#delay_params is  (-0.09111817953443853, 50.881304751417694, 1.4202073990039836)

def get_file_index(num_list, priority_indice, p_flag):
	#num_list = [(169, 148), (...)]
	if len(num_list) <= 1:
		#print("Error num_list len")
		return -1
	else:
		current_item = num_list[-1]
		#print('current_item: ', current_item)
		last_item = num_list[-2]
		#print('last_item: ',last_item)
		if p_flag == 'in':
			for priori in priority_indice:
				if current_item[priori] != 0:
					if current_item[priori] != last_item[priori]:
						#print('1 priori: ',priori)
						return priori
				else:
					#print('2 priori: ',priori) 
					return priori				
		elif p_flag == 'out':
			for priori in reversed(priority_indice):
				if current_item[priori] != 0:
					#print('3 priori: ',priori) 
					return priori
		else:
			#print("Error p_flag")
			return -1
		return len(priority_indice)

def get_pre_min_or_max(cur_num_list, flag):
	current_item = cur_num_list[-1]
	for item in reversed(cur_num_list[:-1]):
		if flag == 'min': # 'out'
			if item < current_item:
				return item
		if flag == 'max':
			if item > current_item:
				return item
	if flag == 'min':
		return 0
	else:
		return max(cur_num_list)
		

			
def get_dichotomy_value(num_list, num, priority_indice, p_flag, cur_index):
	#cur_index = get_file_index(num_list, priority_indice, p_flag)
	#print("cur_index: ",cur_index)
	if cur_index == -1:
		#print("Error cur_index")
		return -1
	if cur_index == len(priority_indice):
		return num
		
	cur_num_list = []
	for item in num_list:
		cur_num_list.append(item[cur_index])
		
	if p_flag == 'in':
		if num[cur_index] == 0:
			num[cur_index] = max(cur_num_list)
			#print('in-0', max(cur_num_list))
		else:
			find_pre_max = get_pre_min_or_max(cur_num_list, 'max')
			#print('in-else:', find_pre_max)
			num[cur_index] += int((find_pre_max - num[cur_index])/2)
	if p_flag == 'out':
		if num[cur_index] == max(cur_num_list):
			num[cur_index] = int(num[cur_index]/2)
			#print('out-0', num[cur_index])
		else:
			find_pre_min = get_pre_min_or_max(cur_num_list, 'min')
			#print('out-else:', find_pre_min)
			num[cur_index] -= int((num[cur_index] - find_pre_min)/2)
	return num


class Function:
	def __init__(self, source, name, count=None, priority=None):
		self.source = source
		self.name = name
		self.count = count or 0
		self.priority = priority or 0

def searchFunc(item, source_t, function_list):
	for func in function_list:
		if func.source == source_t and func.name == item:
			return func
	return 0

def getIndex(current_item, function_list):
	for j in range(len(function_list)):
			if current_item == function_list[j]:
				return j
# function_list = [f1, f2, f3, f4, f5, f6]
# current_list = [f1, f2, f3, f4, f5, f6]
# current_list = [f1, f2, f3]
# current_list = [f1, f2, f4, f5]
# current_list = [f1, f2, f4, f5, f6]
def getFuncListIndex(current_list, function_list, pflag):
	if pflag == 'in':
		return getIndex(current_list[-1], function_list)
	else:
		temp_j = getIndex(current_list[-1], function_list)
		flag = 0
		for i in range(len(current_list)):
			if current_list[len(current_list) - 1 - i] == function_list[temp_j]:
				temp_j -= 1
			else:
				return len(current_list) - 1 - i
		return 0
				
		
def getMinueFuncs(current_list, final_num):
	count = 0
	final_list = []
	for item in current_list:
		count += item.count
		if count <= final_num:
			final_list.append(item)
			
	return final_list
	
					

def statFuncCount(current_list):
	count = 0
	for item in current_list:
		count+=item.count
	return count			

def main():
	parser = argparse.ArgumentParser(description='calculate pwcet')
	parser.add_argument('timeJson', type=str, help='object json')
	parser.add_argument('--option_init', type=int, default=0, help='for the original code')
	parser.add_argument('--option_para', type=int, default=0, help='threshold for GEV/GPD or block size for BM')#just for cycle_times
	parser.add_argument('--option_type', type=str, default='GEV_BM', help='choose th pWCET alogrithm')
	args = parser.parse_args()
	
	object_st = args.timeJson.replace('.st','')
	
	
	## -----open current time recod-----
	with open('../json/time_record.json', 'r') as f1:
		time_data = json.load(f1)

	cycle_times = []
	delay_times = []
	ticktime = 0
	for item in time_data:
		cycle_times.append(item['cycle_t'])
		delay_times.append(item['delay_t'])
	for item in time_data:
		ticktime = item['ticktime']
		break
	
	## -----pWCET type and para-----
	option_type = args.option_type
	
	option_para = args.option_para
	GEV_threshold = option_para
	GPD_threshold = option_para
	block_size = option_para
	if option_para == 0:
		block_size = 1	
	cycle_result = 0
	delay_result = 0
	if option_type == 'GEV_BM':
		cycle_result = pWCET_GEV_BM(cycle_times, block_size)
		delay_result = pWCET_GEV_BM(delay_times, block_size)
	elif option_type == 'GEV':
		cycle_result = pWCET_GEV(cycle_times, GEV_threshold)
		delay_result = pWCET_GEV(delay_times, 2/3*sum(delay_times))
	elif option_type == 'MonteCarlo':
		cycle_result = pWCET_MonteCarlo(cycle_times)
		delay_result = pWCET_MonteCarlo(delay_times)
	elif option_type == 'GPD':
		cycle_result = pWCET_GPD(cycle_times, GPD_threshold)
		delay_result = pWCET_GPD(delay_times, 2/3*sum(delay_times))
	elif option_type == 'EVT':
		cycle_result = pWCET_EVT(cycle_times)
		delay_result = pWCET_EVT(delay_times)
	elif option_type == 'ND':
		cycle_result = pWCET_normal_distribution(cycle_times)
		delay_result = pWCET_normal_distribution(delay_times)
	else:
		#print('error option type.')
		return 0
	
	## -----get current and historical granularity-----	
	txt_files = sorted(glob.glob('../json/'+object_st+'_*_gran.txt'))

	num = []
	ori_num = []
	num_list = []
	for fi in txt_files:
		#print('txt_file is ', fi)
		num_list_t = []
		try:
			with open(fi, 'r') as ff:
				lines = ff.readlines()
				if lines:
					for j in range(len(lines)):
						num_list_t.append(int(lines[j].strip()))
					num.append(int(lines[-1].strip()))
					ori_num.append(int(lines[-1].strip()))
				else:
					return "File is empty"
			num_list.append(num_list_t)
		except FileNotFoundError:
			return "File not found or cannot be opened"
		except Exception as e:
			return "An error: " + str(e)
	num_list = list(zip(*num_list))
	#print("num:",num)
	#print("ori_num:",ori_num)
	#print("num_lsit:",num_list)
	
	
	## -----get priority-----
	json_files = sorted(glob.glob('../json/'+object_st+'_*_new.json'))
	priority_list = []
	function_list = []
	for fi in json_files:
		max_priority = 0
		source_t = fi.replace('../json/','').replace('_new.json','')
		with open(fi, 'r') as fj:
			jsondata = json.load(fj)
		for jd in jsondata:
			name_t = jd['name']
			count_t = jd['count']
			priori_t = jd['priority']
			_function = Function(source_t, name_t, count_t, priori_t)
			function_list.append(_function)
			if priori_t > max_priority:
				max_priority = priori_t
		priority_list.append(max_priority)	
		
	# get indice of num
	#priority_indice = [index for index,_ in sorted(enumerate(priority_list), key=lambda x: x[0], reverse=False)]
	function_sorted = sorted(function_list, key=lambda x: (x.priority, x.source, x.name), reverse=True)
	#print("priority_indice: ",priority_indice)
	#print("function sorted: ", [fun.name for fun in function_sorted])
	
	
	funcs_txts = sorted(glob.glob('../json/'+object_st+'_*_funcs.txt'))
	dichotomy_flag = 'Final'
	## -----get cur funcs-----
	'''
	cur_unsorted = []
	funcs_txts = glob.glob('../json/'+object_st+'_*_funcs.txt')
	for fi in funcs_txts:
		source_t = fi.replace('../json/','').replace('_funcs.txt','')
		with open(fi, 'r') as fc:
			funcs_lines = fc.readlines()
			if funcs_lines:
				tline = funcs_lines[-1].strip().split(' ')
				for item in tline:
					itemfunc = searchFunc(item, source_t, function_sorted)
					if itemfunc!= 0:
						cur_unsorted.append(itemfunc)
					else:
						print('Cannot find item: ',item)
	if len(cur_unsorted) == len(function_sorted):
		cur_unsorted = function_sorted
	cur_sorted = sorted(cur_unsorted, key=lambda x: (x.priority, x.source, x.name), reverse=True)
	print('cur_unsorted: ', [cur_s.name for cur_s in cur_unsorted])
	print('cur_sorted: ', [cur_s.name for cur_s in cur_sorted])
	
	histor_txts = []#[[[func1, func2], [func1], [func2, func3], ...], [[func4, func5],[func6],[]]]
	for fi in funcs_txts:
		source_t = fi.replace('../json/','').replace('_funcs.txt','')
		histor_lines_in_txt = []
		with open(fi, 'r') as fc:
			funcs_lines = fc.readlines()
			if funcs_lines:
				cur_funcs_in_line = []
				for line in funcs_lines:
					tline = line.strip().split(' ')
					for item in tline:
						itemfunc = searchFunc(item, source_t, function_sorted)
						if itemfunc!= 0:
							cur_funcs_in_line.append(itemfunc)
					histor_lines_in_txt.append(cur_funcs_in_line)
			histor_txts.append(histor_lines_in_txt)
		
	histor_funcs = [] #[[func1, func2, func4, func5], [func1, func6], ...]
	for i in range(len(histor_txts[0])):
		t_cur_funcs = []
		for j in range(len(histor_txts)):
			t_cur_funcs += histor_txts[j][i]
		histor_funcs.append(t_cur_funcs)
		
	cur_unsorted = histor_funcs[-1] #cur_unsorted
	for his_f in histor_funcs:
		his_f = sorted(his_f, key=lambda x: (x.priority, x.name), reverse=True)
	cur_sorted= histor_funcs[-1] 
	for histor_f in histor_funcs:
		print('---histor_funcs: ', [his_f.name for his_f in histor_f])
	print('cur_unsorted: ', cur_unsorted)
	print('cur_sorted: ', cur_sorted)
	#print('cur_sorted: ', [cur_s.name for cur_s in cur_sorted]) 
	'''
	
	## -----first time running-----
	if args.option_init == 0:
		with open('../json/'+object_st+'_init_time.txt', 'w+') as f2:
			f2.write(f"{cycle_result}\n{delay_result}")
		with open('../json/'+object_st+'_log.txt','a') as f3:
			f3.write(f"ticktime: {ticktime}, instru_point: 0, max/avg/min of cycle_times: {np.max(cycle_times)}/{np.mean(cycle_times)}/{np.min(cycle_times)}, cycle_result: {cycle_result}\
			 max/avg/min of delay_times: {np.max(delay_times)}/{np.mean(delay_times)}/{np.min(delay_times)}, delay_result: {delay_result}\n \
			 ----cycle_times: {cycle_times}\n \
			 ----delay_times: {delay_times}\n \
			 instru_point: {num}, total points:{sum(num)}, ")  #[169, 148], 317
		ob_funcs = [tuple([func.source, func.name]) for func in function_sorted]
		with open('../json/'+object_st+'_funcs.txt','a') as f4:
			#print("ob_funcs: ",ob_funcs)
			f4.write(f"Ticktime: {ticktime}\n")
			f4.write("Original: "+ " ".join([f"({x},{y})" for x,y in ob_funcs]) + "\n")
		return 0
	## -----second and flowing times running-----
	else:
		## -----get histor and cur funcs-----
		cur_unsorted = []
		histor_line_out = []
		histor_line_in = []
		with open('../json/'+object_st+'_funcs.txt','r') as fl:
			lines = fl.readlines()
			index = 0
			for i in range(len(lines)):
				if lines[i].strip().split(':')[0] == "Ticktime":
					if int(lines[i].strip().split(':')[1].strip()) == ticktime:
						index = i
						break
			lines = lines[index+1:]
			if lines:
				ori_line = lines
				for i in range(len(lines)):
					cur_line_t = lines[i].strip().split(':')
					if cur_line_t[0] == 'out':
						cur_line_last = lines[i-1].strip().split(':')
						tmp_line = cur_line_last[1].strip().split(' ')
						tmp_tuples = [tuple(x.strip("()").split(",")) for x in tmp_line]
						histor_line_out_t = []
						for item in tmp_tuples:
							tmp_func = searchFunc(item[1], item[0], function_sorted)
							if tmp_func!= 0:
								histor_line_out_t.append(tmp_func)
						histor_line_out_t = sorted(histor_line_out_t, key=lambda x: (x.priority, x.source, x.name), reverse=True)
						histor_line_out.append(histor_line_out_t)
					if cur_line_t[0] == 'in':
						cur_line_last = lines[i-1].strip().split(':')
						tmp_line = cur_line_last[1].strip().split(' ')
						tmp_tuples = [tuple(x.strip("()").split(",")) for x in tmp_line]
						histor_line_in_t = []
						for item in tmp_tuples:
							tmp_func = searchFunc(item[1], item[0], function_sorted)
							if tmp_func!= 0:
								histor_line_in_t.append(tmp_func)
						histor_line_in_t = sorted(histor_line_in_t, key=lambda x: (x.priority, x.source, x.name), reverse=True)
						histor_line_in.append(histor_line_in_t)
				last_line_t = lines[-1].strip().split(':')
				last_line = last_line_t[1].strip().split(' ')		
				itemtuple = [tuple(x.strip("()").split(",")) for x in last_line]
				#print("itemtuple: ",itemtuple)
				for item in itemtuple:
					itemfunc = searchFunc(item[1], item[0], function_sorted)
					if itemfunc!= 0:
						cur_unsorted.append(itemfunc)
					#else:
						#print('Cannot find item: ',item)
		cur_sorted = sorted(cur_unsorted, key=lambda x: (x.priority, x.source, x.name), reverse=True)
		#print('cur_unsorted: ', [cur_s.name for cur_s in cur_unsorted])
		#print('cur_sorted: ', [cur_s.name for cur_s in cur_sorted])				
		
		if sum(num) == 0:
			return 1
		flag = 0
		cycle_init, delay_init = 0, 0
		with open('../json/'+object_st+'_init_time.txt', 'r') as f2:
			lines = f2.readlines()
			cycle_init = int(float(lines[0].strip()))
			delay_init = int(float(lines[1].strip()))
		cycle_instr = cycle_result - cycle_init
		delay_instr = delay_result - delay_init
		
		instr_avg = cycle_instr / sum(num) ##avg time of one cfi check point
		
		#extra_time = cycle_result - ticktime #extra_time<0
		max_extra_time = max(max(cycle_times), cycle_result) - ticktime
		max_delay_time = max(max(delay_times), delay_result) - delay_init
		
		redun_time = abs(max_extra_time) - ticktime * 0.1
		redun_num = int(redun_time / instr_avg)					
		final_list = cur_sorted
		if max_delay_time <= 200 and abs(max_extra_time) < ticktime * 0.1:
			flag=1
		elif max_delay_time <= 200 and abs(max_extra_time) >= ticktime * 0.1: #max_extra_time must < 0
			''' # dichotomy for each file
			if len(num_list) == 1:
				flag=1
			else:
				cur_index = get_file_index(num_list, priority_indice, 'in')
				if cur_index == len(priority_indice):
					flag=1
				else:
					num = get_dichotomy_value(num_list, num, priority_indice, 'in', cur_index)
			'''
			dichotomy_flag = 'in'
			index = getFuncListIndex(cur_sorted, function_sorted, dichotomy_flag)
			#print('add index: ',index)
			#print('in cur_sorted: ',[cur_s.name for cur_s in cur_sorted])
			#print('in cur_unsorted: ',[cur_s.name for cur_s in cur_unsorted])
			final_list = []
			if index == len(function_sorted) - 1: #full
				flag = 1
				final_list = cur_sorted
				#print('in-1 cur_sorted: ',cur_sorted)
			else:
				
				if index == 0:
					for item in function_sorted[index+2:]:
						final_list.append(item)
				else:
					final_list += cur_sorted[1:] + [cur_sorted[0]]
					for item in function_sorted[index+1:]:
						final_list.append(item)
				#temp_count = 0
				#for item in function_sorted[index+2:]:
					#if temp_count + item.count <= redun_num:
						#temp_count += item.count
						#final_list.append(item)
				final_sorted = sorted(final_list, key=lambda x: (x.priority, x.source, x.name), reverse=True)
				final_sorted_times = 0
				for his_line in histor_line_in:
					if final_sorted == his_line:
						final_sorted_times += 1
				if final_sorted_times >= 1:
					flag = 1

				#final_list += function_sorted[index+2:]
				#print('in-2 final_list: ',[item.name for item in final_list])

		else: #max_delay_time > 200
			''' # dichotomy for each file
			if len(num_list) == 1:
				for priori in priority_indice[1:]:
					num[priori] = 0
			else:
				cur_index = get_file_index(num_list, priority_indice, 'out')
				if cur_index == len(priority_indice):
					flag=1
				else:
					num = get_dichotomy_value(num_list, num, priority_indice, 'out',cur_index)
			'''
			dichotomy_flag = 'out'
			#print("current gran is ", num)
			index = getFuncListIndex(cur_unsorted, function_sorted, dichotomy_flag)
			#print('minue index: ',index)
			final_list = []
			if index == 0: #means never 'in'
				#final_num = sum(num) - redun_num
				#if final_num <=0:
					#final_num = 1
				final_funcnum = int(len(cur_unsorted)/2)
				#print('out-1 final_funcnum: ',final_funcnum)
				if final_funcnum == 0: #len(cur_unsorted)=1 means the first one should be out
					t_index = getIndex(cur_unsorted[0], function_sorted)
					#print('out-1 t_index: ', t_index)
					if t_index == len(function_sorted) - 1:
						flag = 1
						final_list = []
					else:
						final_list = function_sorted[t_index+1:]
				else: 
					final_list = cur_unsorted[:final_funcnum]
				#if final_list == cur_sorted:
					#final_list = final_list[:-1]
				#print('out-1 final_list: ', [item.name for item in final_list])
			else:
				#enable_count = statFuncCount(cur_sorted[:index+1])
				#print("enable_count: ", enable_count)
				#final_num = sum(num) - enable_count  - redun_num
				#if final_num <= 0:
					#final_num = 1
				final_num = int(len(cur_unsorted[index+1:])/2)
				#print('out-2 final_num: ',final_num)
				if final_num == 0:#len(cur_unsorted[index+1:]=1
					t_index = getIndex(cur_unsorted[index+1], function_sorted)
					#print('out-2 t_index: ', t_index)
					if t_index == len(function_sorted) - 1:
						flag = 1
						final_list = cur_unsorted[:index+1]
					else:
						final_list = cur_unsorted[:index+1] + function_sorted[t_index+1:]
				else:
					final_list = cur_unsorted[:index+1] + cur_sorted[index+1:index+1+final_num]
				#if final_list == cur_sorted:
					#final_list = final_list[:-1]
				#print('out-2 final_lsit: ', [item.name for item in final_list])
				
		for i in range(len(funcs_txts)):
			source_t = funcs_txts[i].replace('../json/','').replace('_funcs.txt','')
			#print('source_t: ', source_t)
			num_t=0
			for item in final_list:
				if item.source == source_t:
					#print('item.source: ', item.source)
					num_t += item.count
			num[i] = num_t
		#print('final num ',num)

		for i in range(len(txt_files)):
			with open(txt_files[i], 'a+') as ff:
				ff.write(f"{num[i]}\n")
		
		for fi in funcs_txts:
			source_t = fi.replace('../json/','').replace('_funcs.txt','')
			with open(fi, 'a+') as ff:
				cur_items = []
				for item in final_list:
					if item.source == source_t:
						cur_items.append(item.name)
				cur_line = ' '.join(cur_items)
				#print("cur_line: ",cur_line)
				ff.write(f"{cur_line}\n")
					

		with open('../json/'+object_st+'_log.txt','a') as f3:
			f3.write(f"max/avg/min of cycle_times: {np.max(cycle_times)}/{np.mean(cycle_times)}/{np.min(cycle_times)}, cycle_result: {cycle_result}\n \
			max/avg/min of delay_times: {np.max(delay_times)}/{np.mean(delay_times)}/{np.min(delay_times)}, delay_result: {delay_result}\n \
			----cycle_times: {cycle_times}\n \
			----delay_times: {delay_times}\n \
			instru_point: {num}, total points:{sum(num)}, ")
			
		fin_funcs = [tuple([func.source, func.name]) for func in final_list]
		with open('../json/'+object_st+'_funcs.txt','a') as f5:
			f5.write(dichotomy_flag + ": " + " ".join([f"({x},{y})" for x,y in fin_funcs]) + "\n")
					 
		if flag==1:
			return 1
		return 0
	
	
if __name__ == '__main__':
	output = main()
	print(output)
