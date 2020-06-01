#!/usr/bin/env python
import model_parser as ps
import utils
import os
import torch
import torch.nn as nn
import torch.optim as optim
from random import shuffle
from pathlib import Path

class architecture:

	def __init__(this):
		this.dataset = []
		this.excluded_datapoints = []
		this.loss_weights = {}
		return

	def initialize_architecture(this, model_file="trace_model.pymodel", project_name="SEALEncryptor_traces/traces", base_path = "./", trace_name = "trace",
																													encoded_trace_folder = "encoded_traces",
																													excluded_labels = [],
																													excluded_train_labels = [],
																													split_trace_sets = {},
																													remake_dataset = False,
																													encoding_size = 64, 
																													global_variables = False,
																													mode = "training", 
																													gpu = False,
																													optimizer = "Adam", 
																													learning_rate = 0.00001, 
																													loss_function = "BCELoss", 
																													epochs = 60, 
																													training_length = 0.3, 
																													save_model = False,
																													save_training_log = True,
																													save_model_specs = True,
																													save_trace_distr = False,
																													model_path = "",
																													print_to_out = True,
																													encode_caller = True,
																													encode_callee = True,
																													encode_args = True,
																													encode_ret = True,
																													discard_half = False):

		# open model_file and parse it. Function returns a dictionary describing the model
		parser = ps.model_parser()
		parsed_model = parser.parse_model(model_file)
		parser.print_model()

		#Create the models
		if mode == "inference":
			this.create_network_graph(parsed_model['layers'], base_path + project_name + "/model/" + model_path)	# this.model
		else:
			this.create_network_graph(parsed_model['layers'])	# this.model

		# Set-up trace paths
		if project_name[-1] != "/":
			project_name += "/"
		trace_list, label_set = utils.set_trace_path(base_path + project_name, encoded_trace_folder, excluded_labels)

		# Make sure split sets are correct
		# Format: {'pass': {1, 3, 5}, 'fail': {2, 4, 5, 6}}
		for cat in split_trace_sets:
			assert cat not in excluded_train_labels, "Category {} found in split set but has been excluded from training in the first place!"

		# Load the dataset only the first time.
		# This condition dictates that you will use different objects for different datasets. Any better suggestion ? Maybe push some params to the constructor to make it safer ?
		if (len(this.dataset) == 0) or remake_dataset:
			this.create_dataset(trace_list, trace_name, excluded_train_labels, encoding_size, split_trace_sets, encode_caller, encode_callee, encode_args, encode_ret, discard_half)	

		if mode == "training":

			# Set-up the model path
			if save_model == True or save_training_log == True or save_model_specs == True or save_trace_distr == True:
				if model_path == "":
					utils.mkdirs(base_path + project_name, "model/")
					model_path = base_path + project_name + "model/" # TODO 
				else:
					if model_path[-1] == "/":
						model_path = model_path[:-1]
					while '//' in model_path:
						model_path = model_path.replace('//', '/')
					subfolder = model_path.split('/')
					utils.mkdirs(base_path + project_name + "model/", subfolder[0].replace("/", ""), subfolders = subfolder[1:])
					model_path = base_path + project_name + "model/" + '/'.join(subfolder) + "/"
				print(model_path)

			opt, loss = this.set_train_params(optimizer, learning_rate, loss_function)

			this.train_dispatch(opt, loss, epochs, training_length, save_model, 
										save_training_log, save_model_specs, save_trace_distr, model_path, print_to_out)
		elif mode == "inference":

			this.online_classification()

		else:
			assert False, ("Mode unknown")

		return

	def set_train_params(this, opt, learning_rate, loss):

		param_list = []
		for l in this.model:
			if isinstance(this.model[l]['model'], list):
				for item in this.model[l]['model']:
					param_list += list(item.parameters())
			else:
				param_list += this.model[l]['model'].parameters()

		if opt == "Adam" or opt == "ADAM" or opt == "adam":
			optimizer = optim.Adam(param_list, learning_rate)
		elif opt == "SGD" or opt == "sgd" or opt == "Sgd":
			optimizer = optim.SGD(param_list, learning_rate)
		else:
			assert False, ("Optimizer {} not supported".format(opt))

		if loss == "BCELoss" or loss == "bceloss" or loss == "BCELOSS" or loss == "BCEloss":
			loss = {'function': nn.functional.binary_cross_entropy, 'weight': this.loss_weights}
			if this.model[this.output_layer]['type'] == "sigmoid":
				print("Automatically combining output sigmoid and BCELoss to BCEWithLogitsLoss")
				loss = {'function': nn.functional.binary_cross_entropy_with_logits, 'weight': this.loss_weights}
				prev_out_layer = this.output_layer
				this.output_layer = this.model[this.output_layer]['input'][0]
				del this.model[prev_out_layer]
		else:
			assert False, ("Loss function {} not supported".format(loss))

		return optimizer, loss

	def execute_network(this, input_trace):

		encoded_lines = []
		for func_line in input_trace['func_calls']:
			encoded_ret = this.encode_value(this.model['ret_encoder'], func_line['trace_ret'])
			encoded_arg = this.encode_value(this.model['arg_encoder'], func_line['trace_arg'])
			encoded_lines.append(torch.cat((torch.cat((func_line['trace_func'], encoded_ret.unsqueeze(1)), 2), encoded_arg.unsqueeze(1)), 2).squeeze(0))

		if len(input_trace['trace_globals']) > 0:
			global_tensor = this.encode_value(this.model['global_encoder'], input_trace['trace_globals'])

		encoded_line_tensor = torch.randn(len(encoded_lines), 1, this.tr_encoding_size)
		encoded_line_tensor = encoded_line_tensor

		for index, line in enumerate(encoded_lines):
			encoded_line_tensor[index] = line

		encoded_func = this.encode_value(this.model['line_encoder'], encoded_line_tensor)
		scores = this.mlp_forward(this.model['MLP'], encoded_func)

		return scores

	def encode_value(this, model, inp_tensor):

		this.test_tensor(inp_tensor)

		h0 = torch.randn(1, 1, model['hidden_size'])
		c0 = torch.randn(1, 1, model['hidden_size'])

		out, hn = model['model'](inp_tensor, (h0, c0))
		this.test_tensor(out)

		return out[model['output_timestep']]

	def mlp_forward(this, model, inp_tensor):

		this.test_tensor(inp_tensor)

		out = model['model'][0](inp_tensor)
		this.test_tensor(out)

		for i in range(1, len(model['model'])):
			out = model['model'][i](out)
			this.test_tensor(out)

		return out


	def train_dispatch(this, optimizer, loss_function, epochs, training_length, save_pymodel, 
							save_training_log, save_model_specs, save_trace_distr, model_path, print_to_out):


		# Things to be saved: pytorch models, training log, model specs, trace distribution (this probably means which traces are in train or test set)
		if save_model_specs == True:
			specs_str = "Model Specifications:\n---------------------\noptimizer: {}\nloss function: {}\nepochs: {}\ntraining length: {}\n".format(optimizer, loss_function, epochs, training_length) # TODO save all model specs
			spec_file = open(model_path + "model_specs.log", 'w')
			spec_file.write(specs_str)
			spec_file.close()

		if save_pymodel == True:
			utils.mkdirs(model_path, "pymodel")

		for i in range(200):
			shuffle(this.dataset)
			shuffle(this.excluded_datapoints)
		
		training_set = this.dataset[:int(training_length*len(this.dataset))]
		validation_set = this.dataset[int(training_length*len(this.dataset)):] + this.excluded_datapoints

		# training_set, validation_set = split_exclude_train_val_set(this, training_length, excl_labels_train)

		if save_training_log == True:
			training_log_file = open(model_path + "training_log.log", 'w')
			pass_fail_optimals = {'train': {'pass': [], 'fail': []}, 'validation': {'pass': [], 'fail': []}}


		# tr_pass_total = 0
		# tr_fail_total = 0
		# val_pass_total = 0
		# val_fail_total = 0

		for ep in range(epochs):

			print("Epoch {}".format(ep))
			epoch_stats = "Epoch {}\n----------\n".format(ep)

			for i in range(100):
				shuffle(training_set)

			for tr_index, tr_data in enumerate(training_set):
				out = this.execute_network(tr_data)

				if "pass" in tr_data['label']:
					target = torch.tensor([[1.]])
					weight = loss_function['weight']['pass']
				elif "fail" in tr_data['label']:
					target = torch.tensor([[0.]])
					weight = loss_function['weight']['fail']
				else:
					assert False, "Unrecognized label"

				loss = loss_function['function'](out, target, weight=weight)
				loss.backward()
				optimizer.step()

				### Report Loss every now and then
				if (tr_index % 10) == 0 and print_to_out == True:
					print("{} loss: {:6.4f}, {}".format(tr_data['label'], loss.item(), tr_index))

			#### Training Set inference
			pass_match, pass_total, fail_match, fail_total, _ = this.output_set_labelling(training_set)
			if save_training_log == True:
				pass_fail_optimals['train']['pass'].append(pass_match)
				pass_fail_optimals['train']['fail'].append(fail_match)
			print("\nTraining set:\npass matches: {}\npass total: {}\nfail matches: {}\nfail total: {}\n".format(pass_match, pass_total, fail_match, fail_total))
			epoch_stats += "Train set:\npass matches: {}\npass total: {}\nfail matches: {}\nfail total: {}\n\n".format(pass_match, pass_total, fail_match, fail_total)

			### Validation set inference
			pass_match, pass_total, fail_match, fail_total, _ = this.output_set_labelling(validation_set)
			if save_training_log == True:
				pass_fail_optimals['validation']['pass'].append(pass_match)
				pass_fail_optimals['validation']['fail'].append(fail_match)
			print("\nValidation set:\npass matches: {}\npass total: {}\nfail matches: {}\nfail total: {}\n".format(pass_match, pass_total, fail_match, fail_total))
			epoch_stats += "Validation set:\npass matches: {}\npass total: {}\nfail matches: {}\nfail total: {}\n\n".format(pass_match, pass_total, fail_match, fail_total)


			### Write epoch results to file
			if save_training_log == True:
				training_log_file.write(epoch_stats)
				training_log_file.flush()

			### Save epoch file
			if save_pymodel == True:
				utils.mkdirs(model_path, "pymodel/epoch_{}".format(ep))
				this.save_pymodel(model_path + "pymodel/epoch_{}/".format(ep))

		### Write optimal points to file
		if save_training_log == True:
			training_log_file.close()
			optimal_log_file = open(model_path + "optimal_points.log", 'w')

			tr_pass_opt, tr_fail_opt = this.find_optimals(pass_fail_optimals['train']['pass'], pass_fail_optimals['train']['fail'])
			val_pass_opt, val_fail_opt = this.find_optimals(pass_fail_optimals['validation']['pass'], pass_fail_optimals['validation']['fail'])

			assert (len(tr_pass_opt) == len(tr_fail_opt)) and (len(val_pass_opt) == len(val_fail_opt)), "Optimal lists do not have the same length"

			optimal_log_file.write("Training set optimal points:\n-----------------------\n\n")
			for p, f in zip(tr_pass_opt, tr_fail_opt):
				metrics = utils.add_perc_metrics({'pass': {'matches': p, 'total': pass_total}, 'fail': {'matches': f, 'total': fail_total}}, )
				optimal_log_file.write("pass matches: {}\nfail matches: {}\nprecision: {}\nrecall: {}\nTNR: {}\n\n".format(p, 
																															f, 
																															metrics['precision'], 
																															metrics['recall'],
																															metrics['true_neg_rate']))

			optimal_log_file.write("Validation set optimal points:\n-----------------------\n\n")
			for p, f in zip(val_pass_opt, val_fail_opt):
				metrics = utils.add_perc_metrics({'pass': {'matches': p, 'total': pass_total}, 'fail': {'matches': f, 'total': fail_total}}, )
				optimal_log_file.write("pass matches: {}\nfail matches: {}\nprecision: {}\nrecall: {}\nTNR: {}\n\n".format(p, 
																															f, 
																															metrics['precision'], 
																															metrics['recall'],
																															metrics['true_neg_rate']))

			optimal_log_file.close()

		return

	def online_classification(this):


		print("I'm in online class func")

		print(this.model)

		"""
		TODO
		1. get this.dataset
		2. load network
		3. execute network on set
		4. label the outputs
		5. write the results
		"""

		for i in range(200):
			shuffle(this.dataset)
			shuffle(this.excluded_datapoints)

		classification_set = this.dataset + this.excluded_datapoints

		with torch.no_grad():
			for index, in_data in enumerate(classification_set):
				out = this.execute_network(in_data)

				if "pass" in in_data['label']:
					target = torch.tensor([[1.]])
				elif "fail" in in_data['label']:
					target = torch.tensor([[0.]])
				else:
					assert False, "Unrecognized label"

		pass_match, pass_total, fail_match, fail_total = this.output_set_labelling(classification_set)

		print("\nClassification set:\npass matches: {}\npass total: {}\nfail matches: {}\nfail total: {}\n".format(pass_match, pass_total, fail_match, fail_total))

		return

	def output_set_labelling(this, datapoint_set):

		p_list = []
		with torch.no_grad():
			pass_match, fail_match, pass_total, fail_total = 0, 0, 0, 0
			for d_index, datapoint in enumerate(datapoint_set): 
				d_output = this.execute_network(datapoint)
				if "pass" in datapoint['label']:
					pass_total += 1
					p_list.append("{} 1".format(str(d_output.item())) )
					if d_output.item() >= 0.5:
						pass_match += 1
				elif "fail" in datapoint['label']:
					fail_total += 1
					p_list.append("{} 0".format(str(d_output.item())) )
					if d_output.item() <= 0.5:
						fail_match += 1
				else:
					assert False, "Unrecognized label"

		return pass_match, pass_total, fail_match, fail_total, p_list

	def find_optimals(this, x_list, y_list):

		x, y = (list(t) for t in zip(*sorted(zip(x_list, y_list), reverse = True)))

		print(x)
		print(y)

		x_pareto = []
		y_pareto = []

		x_pareto.append(x[0])
		y_pareto.append(y[0])
		x_reference = x[0]
		y_reference = y[0]

		for i, j in zip(x, y):
			if j > y_reference:
				x_pareto.append(i)
				y_pareto.append(j)
				x_reference = i
				y_reference = j

		return x_pareto, y_pareto

	# TODO insert argument to select only specific folders for training. Done ?
	def create_dataset(this, trace_path_list, trace_name, excluded_train_labels, encoding_size, split_trace_sets, encode_caller, encode_callee, encode_args, encode_ret, discard_half):

		this.loss_weights = {'pass': 0.0, 'fail': 0.0}
		this.dataset = []
		total_size = 0

		for category in trace_path_list:
			print(category['path'] + trace_name)

			if "fail" in category['label']:
				this.loss_weights['pass'] += category['num_traces']
			else:
				this.loss_weights['fail'] += category['num_traces']
			total_size += category['num_traces']

			if category['label'] in split_trace_sets:
				range_set = split_trace_sets[category['label']]
			else:
				range_set = set(range(1, category['num_traces'] + 1)) 

			for tr in range_set:
				datapoint = this.process_trace(category['path'] + trace_name + str(tr) + category['extension'], encoding_size, encode_caller, encode_callee, encode_args, encode_ret, discard_half)
				datapoint['label'] = category['label']
				# This is unused
				datapoint['index'] = tr
				if datapoint['label'] in excluded_train_labels:
					this.excluded_datapoints.append(datapoint)
				else:
					this.dataset.append(datapoint)
		for i in this.loss_weights:
			this.loss_weights[i] = torch.FloatTensor([this.loss_weights[i] / total_size])
		print("Loss weights for classes: " + str(this.loss_weights))
		
		return

	def process_trace(this, trace_path, encoding_size, encode_caller, encode_callee, encode_args, encode_ret, discard_half):

		datapoint = []
		global_values = []

		f = open(trace_path, 'r')
		for line in f:

			line_split = line.replace('\n', '').split(',')
			if len(line_split) > 1:
				if encode_callee == True and encode_caller == True:
					func = this.str_to_tensor(line_split[0].split(' '), 0)
				else:
					func = torch.FloatTensor([0] * 32).unsqueeze(0).unsqueeze(0)
					func = func

				if encode_ret == True:
					ret = this.str_to_tensor(line_split[1].split(' '), encoding_size)
				else:
					ret = torch.FloatTensor([0] * 64).unsqueeze(0).unsqueeze(0)
					ret = ret

				if encode_args == True:
					args = this.str_to_tensor(line_split[2].split(' '), encoding_size)
				else:
					args = torch.FloatTensor([0] * 64).unsqueeze(0).unsqueeze(0)
					args = args
				
				datapoint.append({'trace_func': func, 'trace_ret': ret, 'trace_arg': args})
			else:	#probably globals here
				# global_values.append(this.str_to_tensor(line_split[0].split(' ')[:-1], encoding_size))
				pass

		f.close()
		return {'func_calls': datapoint, 'trace_globals': global_values}

	def str_to_tensor(this, str_list, encoding_size):

		if encoding_size == 0:
			tensor = torch.FloatTensor(list(map(float, str_list))).unsqueeze(0).unsqueeze(0)
			tensor = tensor
			this.test_tensor(tensor)
			return tensor
		else:
			float_list = list(map(float, str_list))
			tensor = torch.randn(int(len(float_list) / encoding_size), encoding_size)
			for index, item in enumerate(float_list):
				tensor[int(index / encoding_size)][index % encoding_size] = item
			tensor = tensor.unsqueeze(1)
			tensor = tensor
			this.test_tensor(tensor)
			return tensor

	def test_tensor(this, tensor):
		assert torch.isnan(tensor).any() == 0
		return

	def create_network_graph(this, parsed_model, load_model = ""): 

		live_depends = this.find_model_inputs(parsed_model)#{"trace_func", "trace_ret", "trace_arg", "trace_globals"}
		this.model = {}
		this.output_layer = ""

		while len(parsed_model) > 0:
			del_keys = []
			for layer in parsed_model:
				if set(parsed_model[layer]['input']).issubset(live_depends):
					if load_model != "":
						if load_model[-1] != "/":
							load_model += "/"
						this.model[layer] = this.create_pylayer(parsed_model[layer], load_model = "{}{}".format(load_model, layer))
					else:
						this.model[layer] = this.create_pylayer(parsed_model[layer])

					live_depends.add(layer)
					del_keys.append(layer)
					this.output_layer = layer
					if len(parsed_model[layer]['input']) > 1 and parsed_model[layer]['type'] == "lstm":
						this.tr_encoding_size = parsed_model[layer]['params']['input_size']
			for i in del_keys:
				del parsed_model[i]

		# for item in this.model:
		# 	print (item)
		# 	print(this.model[item])
		# 	print()

		return

	def find_model_inputs(this, parsed_model):

		live_inputs = set()
		model_outs = set()

		for layer in parsed_model:
			model_outs.add(layer)
			for inp in parsed_model[layer]['input']:
				live_inputs.add(inp)

		return live_inputs - model_outs

	def create_pylayer(this, layer, load_model = ""):

		if layer['type'] == "lstm":

			if load_model != "":
				nn_layer = torch.load("{}.pt".format(load_model))
				nn_layer.eval()
			else:
				nn_layer = nn.LSTM(input_size = layer['params']['input_size'], 
									hidden_size = layer['params']['hidden_size'], 
									num_layers = layer['params']['num_layers'], 
									bias = layer['params']['bias'], 
									batch_first = layer['params']['batch_first'], 
									dropout = layer['params']['dropout'], 
									bidirectional = layer['params']['bidirectional'])

			return {'model': nn_layer,
					'type': "lstm",
					'input': layer['input'],
					'output_timestep': layer['params']['output_timestep'], 
					'hidden_size': layer['params']['hidden_size']}

		elif layer['type'] == "mlp":

			mlp = {'model': [], 'type': "mlp", 'input': layer['input']}
			if load_model != "":
				fc_layer = torch.load("{}_0.pt".format(load_model))
				fc_layer.eval()
				mlp['model'].append(fc_layer)
			else:
				mlp['model'].append(nn.Linear(in_features = layer['params']['in_features'], 
												out_features = layer['params']['out_features'][0], 
												bias = layer['params']['bias']))

			for l in range(1, len(layer['params']['out_features'])):
				
				if load_model != "":
					fc_layer = torch.load("{}_{}.pt".format(load_model, l))
					fc_layer.eval()
					mlp['model'].append(fc_layer)
				else:
					mlp['model'].append(nn.Linear(in_features = layer['params']['out_features'][l - 1], 
													out_features = layer['params']['out_features'][l],
													bias = layer['params']['bias'] ) )

			return mlp
		elif layer['type'] == "sigmoid":
			return {'model': nn.Sigmoid(), 'type': "sigmoid", 'input': layer['input']}

	def save_pymodel(this, pymodel_path):
		
		print("Saving the model....")

		for layer in this.model:
			if this.model[layer]['type'] == "lstm":
				torch.save(this.model[layer]['model'], pymodel_path + layer + ".pt")
			elif this.model[layer]['type'] == "mlp":
				for index, hidden in enumerate(this.model[layer]['model']):
					torch.save(hidden, pymodel_path + layer + "_" + str(index) + ".pt")

		return