#!/usr/bin/env python
import os
import random

#  Works for two sets
def generate_splitted_exclusive(range_val, p):

	label_sets = [set(), set()]
	range_pool = set(range(1, range_val + 1))
	p_arr = [0] * int(p * range_val) + [1] * (range_val - int(p * range_val))
	random.shuffle(p_arr)

	for num in range_pool:
		label_sets[p_arr[random.randint(0, len(p_arr) - 1)]].add(num)

	return label_sets

def set_trace_path(root_path, folder_lookup, excluded_labels = []):

	trace_paths = []
	label_set = set()
	filenames = os.listdir(root_path)
	for file in filenames:
		if file in excluded_labels:
			continue
		if os.path.isdir(root_path + file):
			if "fail" in file:
				trace_len = len(os.listdir(root_path + file + "/{}/".format(folder_lookup)))
				trace_paths.append({'path': root_path + file + "/{}/".format(folder_lookup), 'extension': ".csv", 'num_traces': trace_len, 'label': file.replace("/", "")})
				label_set.add(file.replace("/", ""))
			elif "pass" in file:
				trace_len = len(os.listdir(root_path + file + "/{}/".format(folder_lookup)))
				trace_paths.append({'path': root_path + file + "/{}/".format(folder_lookup), 'extension': ".csv", 'num_traces': trace_len, 'label': file.replace("/", "")})
				label_set.add(file.replace("/", ""))

	return trace_paths, label_set

## Parses the whitespaces on an input string
def parse_whitespace(inp_str): # Removes whitespaces recursively
	if " " in inp_str or '\t' in inp_str:
		inp_str = inp_str.replace(" ", "").replace('\t', '')
		inp_str = parse_whitespace(inp_str)
	return inp_str

## Create new folders recursively in the directory tree
## base_folder: the directory where the new folder will be created
## new_folder_name: The name of the new folder
## subfolders: list of subfolers to be created inside the new folder
## assert_if_exists: Raise assertion if folder exists
def mkdirs(base_folder, new_folder_name, subfolders = [], assert_if_exists = False):

	try:
		os.mkdir(base_folder + new_folder_name)
	except FileExistsError:
		assert (not assert_if_exists), "Folder {} already exists".format(new_folder_name)

	for subfolder in subfolders:
		try:
			os.mkdir(base_folder + new_folder_name.replace("/", "") + "/" + subfolder)
		except FileExistsError:
			assert (not assert_if_exists), "Folder {} already exists".format(subfolder)
	return

## Delete list of folders and/or files inside base_folder
## base_folder: The base folder where files and folders will be removed
## delete_folders: list of child folders to be deleted
## delete_files: set to True to remove files of the iterated base_folder
def cleardirs(base_folder, delete_folders = [], delete_files = False):

	if base_folder[-1] != "/":
		base_folder += "/"

	files = os.listdir(base_folder)
	for file in files:
		if os.path.isfile(base_folder + file): # File
			if delete_files == True:
				os.remove(base_folder + file)
		else: # Directory
			if file in delete_folders:
				cleardirs(base_folder + file, delete_folders = os.listdir(base_folder + file), delete_files = True)
				os.rmdir(base_folder + file)
			else:
				cleardirs(base_folder + file, delete_folders = delete_folders, delete_files = delete_files )

	return

def add_perc_metrics(results, label_class = "binary", fail_as_positives = True):

	if label_class == "binary":
		pass_match, pass_total, fail_match, fail_total = results['pass']['matches'], results['pass']['total'], 0, 0
		for category in results:
			if "fail" in category:
				fail_match += results[category]['matches']
				fail_total += results[category]['total']

		if fail_as_positives == True:
			try:
				results['precision'] = "{}%".format(round(100*float(fail_match / (fail_match + pass_total - pass_match)), 2))
			except ZeroDivisionError:
				results['precision'] = "-0.0%"
				print("Everything is classified as passing trace")
			try:
				results['recall'] = "{}%".format(round(100*float(fail_match / fail_total), 2))
			except ZeroDivisionError:
				results['recall'] = "-inf"
				assert False, "No failing traces in dataset"
			try:
				results['true_neg_rate'] = "{}%".format(round(100*float(pass_match / pass_total), 2))
			except ZeroDivisionError:
				results['true_neg_rate'] = "-inf"
				assert False, "No passing traces in dataset"
		else:
			try:
				results['precision'] = "{}%".format(round(100*float(pass_match / (pass_match + fail_total - fail_match)), 2))
			except ZeroDivisionError:
				results['precision'] = "-0.0%"
				print("Everything is classified as passing trace")
			try:
				results['recall'] = "{}%".format(round(100*float(pass_match / pass_total), 2))
			except ZeroDivisionError:
				results['recall'] = "-inf"
				assert False, "No failing traces in dataset"
			try:
				results['true_neg_rate'] = "{}%".format(round(100*float(fail_match / fail_total), 2))
			except ZeroDivisionError:
				results['true_neg_rate'] = "-inf"
				assert False, "No passing traces in dataset"

	return results

def help():



	return