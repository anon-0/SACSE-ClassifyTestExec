#!/usr/bin/env python
import sys
import utils
"""
This class is a parser (or a future compiler at its early stages)
for 'pymodel' files. 'pymodels' describe a network architecture
using the pytorch standards. The file was inspired by the model 'protoxt'
used by Caffe deep learning framework
"""

class model_parser:

	def __init__(this):
		
		this.model = {'name': "", 'layers': []} # Model described as a dictionary
		return

	def print_model(this):

		print("Network name: {}\n\nLayers:\n".format(this.model['name']))
		for layer in this.model['layers']:
			print("Layer name: {}\nLayer params: \n{}\n".format(layer, this.model['layers'][layer]))
		return

	def parse_model(this, model_file):

		mFile = open(model_file, 'r')
		fList = []
		for line in mFile:
			fList.append(line)

		this.model['name'] = this.parse_entry("name", fList[0].split('\n')[0]) # First line should be like: '( )*name( )*:( )*"[*-{"}]"( )*' in the model file
		this.model['layers'] = this.parse_layers(fList[1:]) # Each layer entity should be like: ( )*layer( )*{*}( )* in the model file
		return this.model

	def isType(this, value, ints = False, floats = False, strs = False, bools = False):
		
		try:
			if ints == True:
				int(value)
			if floats == True:
				float(value)
			if strs == True:
				str(value)
			if bools == True:
				bool(value)
			return True
		except ValueError:
			return False

	# Identifies if a line is a comment or not
	def is_comment(this, input_line):

		for character in input_line:
			if character == ' ' or character == '\t':
				continue
			elif character == '#':
				return True
			else:
				return False

	def parse_entry(this, keyword, name_line):

		fields = name_line.split(':')
		name_field = utils.parse_whitespace(fields[0])

		if name_field != keyword:
			assert("Error: Expected field \'{}\' but found \'{}\'".format(keyword, name_field))
		else:
			if len(fields) < 2:
				assert("Error: No {} provided for model\n{}".format(keyword, fields))

			raw_name = fields[1].split('\"')
			if len(raw_name) != 3:
				assert("Wrong format provided for model {}\n{}".format(keyword, raw_name))
			# raw_name should be of type: [*, actual name, *]
			return raw_name[1]

	# This function receives the model file containing all layers (in raw format) and returns a list of layers with their dependencies resolved
	def parse_layers(this, raw_layers):

		layer_block = []
		layers = {} # This list will contain one entry for each layer
		capturing_block = False

		#First iteration creates a list with the layers
		for line in raw_layers:
			stripped_line = utils.parse_whitespace(line.replace("\n", ""))
			if this.is_comment(stripped_line):
				continue
			if "layer{" in stripped_line: # Start counting the layer string block
				pending_brackets = 1
				capturing_block = True
			elif "{" in stripped_line: # Add another open bracket needed to close and add the line to the block
				pending_brackets += 1
				layer_block.append(stripped_line)
			elif "}" in stripped_line: # An internal bracket is closed
				pending_brackets -= 1
				layer_block.append(stripped_line)
				if capturing_block == True and pending_brackets == 0:
					capturing_block = False
					ret_name, ret_params = this.parse_layer_block(layer_block)
					if ret_name in layers:
						assert("Layer \'{}\' already defined!".format(ret_name))
					else:
						layers[ret_name] = ret_params
					layer_block = []
			else: # A random layer line is found
				layer_block.append(stripped_line)

		for layer in layers:
			for hyperparam in layers[layer]['params']:
				if layers[layer]['params'][hyperparam] == "len(input)":
					size = 0
					for input_layer in layers[layer]['input']:
						assert (input_layer in layers), "There is no layer named {}".format(input_layer)
						size += this.get_output_size(layers[input_layer]) # TODO
					layers[layer]['params'][hyperparam] = size
		
		del_1h = []
		for layer in layers:
			if layers[layer]['type'] == "1hot":
				del_1h.append(layer)
				for out_layer in layers:
					for index, inp in enumerate(layers[out_layer]['input']):
						if inp == layer:
							layers[out_layer]['input'][index] = layers[layer]['input'][0]
		for item in del_1h:
			del layers[item]

		return layers

	def parse_layer_input(this, input_line):

		fields = input_line.split(':')
		name_field = utils.parse_whitespace(fields[0])

		if name_field != "input":
			assert("Error: Expected field input but found \'{}\'".format(name_field))
		else:
			if len(fields) < 2:
				assert("Error: No input provided for model\n{}".format(fields))

			raw_input = utils.parse_whitespace(fields[1])
			return raw_input.replace("\"", "").split('+')

	def parse_hyperparam(this, h_line):

		h_line = utils.parse_whitespace(h_line)

		key_value = h_line.split(':')

		if len(key_value) != 2:
			assert("Wrong format specified for keyword\n{}".format(h_line))
		if not this.isType(key_value[0], strs = True):
			assert("String expected for keyword identifier\n{}".format(h_line))

		possible_array = False
		if "[" in key_value[1] and "]" in key_value[1]:
			possible_array = True

		if not this.isType(key_value[1], ints = True, floats = True, bools = True) and key_value[1] != "False" and key_value[1] != True and "len(" not in key_value[1] and possible_array == False:
			assert("Wrong format specified for value of keyword {}\n{}".format(key_value[0], h_line))

		if possible_array == False:

			if this.isType(key_value[1], ints = True):
				return key_value[0], int(key_value[1])
			elif this.isType(key_value[1], floats = True):
				return key_value[0], float(key_value[1])
			elif key_value[1] == True:
				return key_value[0], True
			elif key_value[1] == False:
				return key_value[0], False
			elif "len(" in key_value[1]:
				len_ident = key_value[1].split('len(')[1]
				if ')' not in len_ident:
					assert("Wrong format specified for len() keyword {}\n{}".format(key_value[1], h_line))
				return key_value[0], key_value[1]
		else:
			array_elements = key_value[1].split(']')[0].split('[')[-1].split(',')
			if len(array_elements) == 0:
				assert("Array has zero elements {}\n{}".format(key_value[1], h_line))
			try:
				array_elements = [int(x) for x in array_elements]
				return key_value[0], array_elements
			except ValueError:
				assert("Array elements have invalid format {}\n{}".format(key_value[1], h_line))

	# Returns output size of blob with respect to the layer type
	def get_output_size(this, layer):
		if layer['type'] == "lstm":
			return layer['params']['hidden_size']
		elif layer['type'] == "mlp":
			return layer['params']['out_features'][-1]
		elif layer['type'] == "1hot":
			return layer['params']['output_size']

	# This function receives one layer in raw list format and returns a dictionary with all parameters (without resolved dependencies)
	def parse_layer_block(this, layer_str):

		# layer_str = [utils.parse_whitespace(x) for x in layer_str]
		layer_name = ""
		layer_type = ""
		layer_input = [] # Layer input is a list because multiple inputs can be concatanated into one.
		params = {}
		hyperparams = False

		for index, item in enumerate(layer_str):

			if this.is_comment(item):
				continue
			if "name" in item:
				layer_name = this.parse_entry("name", item)
			elif "type" in item:
				layer_type = this.parse_entry("type", item)
			elif "input" in item and "input_size" not in item:
				layer_input = this.parse_layer_input(item)
			elif (layer_type + "_params") in item:
				hyperparams = True
				if layer_type == "lstm":
					params = this.parse_lstm_params(layer_str[index + 1:])
				elif layer_type == "mlp":
					params = this.parse_mlp_params(layer_str[index + 1:])
				elif layer_type == "1hot":
					params = this.parse_1hot_params(layer_str[index + 1:])
				else:
					assert("Unsupported hyperparameters set for layer of type {}".format(layer_type))
				break

		if layer_name == "":
			assert("Layer name not found")
		if layer_type == "":
			assert("Layer type not found")
		if len(layer_input) == 0:
			assert("Layer input not found")
		if layer_type != "sigmoid" and hyperparams == False:
			assert("Hyperparameters for layer type {} not found".format(layer_type))

		return layer_name, {'type': layer_type, 'input': layer_input, 'params': params}


	# Function to set the lstm hyper parameters
	def parse_lstm_params(this, hyperparam_block):

		lstm_hparams = {'input_size': 0, 'hidden_size': 0, 'num_layers': 1, 'bias': False, 'batch_first': False, 'dropout': 0, 'bidirectional': False, 'output_timestep': -1}

		for line in hyperparam_block:

			if this.is_comment(line) or line == "}" or line == '':
				continue
			key, value = this.parse_hyperparam(line)
			if key not in lstm_hparams:
				assert("Parameter {} not found in lstm params".format(key))
			else:
				lstm_hparams[key] = value

		if lstm_hparams['input_size'] == 0:
			assert("lstm input size is required!")
		if lstm_hparams['hidden_size'] == 0:
			assert("lstm hidden size is required!")

		return lstm_hparams

	# Function to set the mlp hyper parameters
	def parse_mlp_params(this, hyperparam_block):

		mlp_hparams =  {'in_features': 0, 'out_features': 0, 'bias': False}

		for line in hyperparam_block:
			if this.is_comment(line) or line == "}" or line == '':
				continue
			key, value = this.parse_hyperparam(line)
			if key not in mlp_hparams:
				assert("Parameter {} not found in mlp params".format(key))
			else:
				mlp_hparams[key] = value

		if mlp_hparams['in_features'] == 0:
			assert("mlp in features size required!")
		if mlp_hparams['out_features'] == 0:
			assert("mlp out features size required!")

		return mlp_hparams


	# Function to set the 1hot hyper parameters
	def parse_1hot_params(this, hyperparam_block):

		oneHot_hparams = {'output_size' : 0}

		for line in hyperparam_block:
			if this.is_comment(line) or line == "}" or line == '':
				continue
			key, value = this.parse_hyperparam(line)
			if key not in oneHot_hparams:
				assert("Parameter {} not found in 1hot params".format(key))
			else:
				oneHot_hparams[key] = value

		if oneHot_hparams['output_size'] == 0:
			assert("1hot output size is required!")

		return oneHot_hparams


