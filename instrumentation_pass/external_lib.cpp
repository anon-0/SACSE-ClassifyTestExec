#include <stdio.h>
#include <iostream>
#include "external_lib.h"
#include "function_call.h"

static std::vector<function_call*> func_calls;
static std::vector<ir_type*> global_variables;
static unsigned stop_trace = 2;

void deallocate_memory(){

	for (unsigned i = 0; i < func_calls.size(); i++){
		delete func_calls[i];
	}

	for (unsigned i = 0; i < global_variables.size(); i++){
		delete global_variables[i];
	}
	func_calls.erase(func_calls.begin(), func_calls.end());
	global_variables.erase(global_variables.begin(), global_variables.end());
	return;
}

void print_trace(){

	for (std::vector<function_call*>::iterator it = func_calls.begin(); it != func_calls.end(); it++){
		(*it)->print();
	}
	for (auto &g : global_variables){
			g->print();
			std::cout << std::endl;
	}

	deallocate_memory();

	return;
}

unsigned int record_call(const char* caller, const char* callee){
	if (stop_trace == 2){
		func_calls.push_back(new call(caller, callee));
	}else{
		return 0;
	}
	return func_calls.size() - 1;
}

unsigned int record_invoke(const char* caller, const char* callee){
	if (stop_trace == 2){
		func_calls.push_back(new invoke(caller, callee));
	}else{
		return 0;
	}
	return func_calls.size() - 1;
}

unsigned int record_exit_point(const char *caller, const char *callee){
	if (stop_trace == 2){
		func_calls.push_back(new exit_point(caller, callee));
		stop_trace = 1;
	}else{
		return 0;
	}
	return func_calls.size() - 1;
}

void update_void(unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new void_t());
				}else{
					ir_type *v = new void_t();
					global_variables[index]->push(v);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new void_t());
				break;
			case 2:
				func_calls[index]->set_arg_value(new void_t());
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_1(bool value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i1_t(value));
				}else{
					ir_type *i1 = new i1_t(value);
					global_variables[index]->push(i1);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i1_t(value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i1_t(value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_8(uint8_t value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i8_t(false, value));
				}else{
					ir_type *i8 = new i8_t(false, value);
					global_variables[index]->push(i8);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i8_t(false, value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i8_t(false, value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_16(uint16_t value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i16_t(false, value));
				}else{
					ir_type *i16 = new i16_t(false, value);
					global_variables[index]->push(i16);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i16_t(false, value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i16_t(false, value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_32(uint32_t value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i32_t(false, value));
				}else{
					ir_type *i32 = new i32_t(false, value);
					global_variables[index]->push(i32);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i32_t(false, value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i32_t(false, value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_64(uint64_t value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i64_t(false, value));
				}else{
					ir_type *i64 = new i64_t(false, value);
					global_variables[index]->push(i64);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i64_t(false, value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i64_t(false, value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_128(__uint128_t value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new i128_t(false, value));
				}else{
					ir_type *i128 = new i128_t(false, value);
					global_variables[index]->push(i128);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new i128_t(false, value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new i128_t(false, value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_float(float value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new float_t(value));
				}else{
					ir_type *f = new float_t(value);
					global_variables[index]->push(f);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new float_t(value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new float_t(value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_double(double value, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new double_t(value));
				}else{
					ir_type *d = new double_t(value);
					global_variables[index]->push(d);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new double_t(value));
				break;
			case 2:
				func_calls[index]->set_arg_value(new double_t(value));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_ptr(uint64_t address, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new ptr_t(address));
				}else{
					ir_type *p = new ptr_t(address);
					global_variables[index]->push(p);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new ptr_t(address));
				break;
			case 2:
				func_calls[index]->set_arg_value(new ptr_t(address));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_array(uint64_t size, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new array_t(size));
				}else{
					ir_type *a = new array_t(size);
					global_variables[index]->push(a);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new array_t(size));
				break;
			case 2:
				func_calls[index]->set_arg_value(new array_t(size));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;

	return;
}

 void update_struct(const char *name, uint64_t size, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new struct_t(name, size));
				}else{
					ir_type *a = new struct_t(name, size);
					global_variables[index]->push(a);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new struct_t(name, size));
				break;
			case 2:
				func_calls[index]->set_arg_value(new struct_t(name, size));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_undefined(const char* type_name, unsigned int index, unsigned int type){

	if(stop_trace != 0){
		switch(type){
			case 0:
				if ( (global_variables.size() == 0) || (index > global_variables.size() - 1)){
					global_variables.push_back(new undefined(type_name));
				}else{
					ir_type *u = new undefined(type_name);
					global_variables[index]->push(u);
				}
				break;
			case 1:
				func_calls[index]->set_ret_value(new undefined(type_name));
				break;
			case 2:
				func_calls[index]->set_arg_value(new undefined(type_name));
				break;
			default:
				break;
		}
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}

void update_exception(unsigned int index){

	if(stop_trace != 0){
		func_calls[index]->set_ret_value(new exception());
	}
	stop_trace = stop_trace == 1 ? 0 : stop_trace;
	return;
}
