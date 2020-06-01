#include "ir_type.h"
#include <vector>
#include <string>

class function_call{
public:
	function_call(){}
	virtual ~function_call(){}
	virtual void set_ret_value(ir_type* ret) = 0;
	virtual void set_arg_value(ir_type* ret){ (void) ret; }
	virtual void print() = 0;
	virtual std::string get_caller() = 0;
	virtual std::string get_callee() = 0;

private:
};

class call : public function_call{
public:
	call(const char* clr, const char* cle) : function_call(){
		caller = clr;
		callee = cle;
		return_value = nullptr;
	}
	~call(){
		delete return_value;
		for (unsigned i = 0; i < passed_arguments.size(); i++){
			delete passed_arguments[i];
		}
		passed_arguments.erase(passed_arguments.begin(), passed_arguments.end());
	}
	void set_ret_value(ir_type* ret) override{
		if (return_value != nullptr){
			return_value->push(ret);
		}else{
			return_value = ret;
		}
	}
	void set_arg_value(ir_type* arg) override{
		if ( (passed_arguments.empty()) || (passed_arguments.back()->is_updated())){
			passed_arguments.push_back(arg);
		}else{
			if (passed_arguments.empty()){
			}
			passed_arguments.back()->push(arg);
		}
		return;
	}
	void print() override{

		#ifdef PREDUCE
		std::cout << ( caller.size() > 40 ? caller.substr(0, 49) : caller) << " " << (callee.size() > 40 ? callee.substr(0, 39) : callee);
		#else
		std::cout << caller << " " << callee;
		#endif
		if (return_value != nullptr){
			std::cout << ", ";
			return_value->print();
		}
		std::cout << ", ";
		if (passed_arguments.size() > 0){
			for (std::vector<ir_type*>::iterator it = passed_arguments.begin(); it != passed_arguments.end(); it++){
					(*it)->print();
			}
		}
		std::cout << std::endl;
		return;
	}
	std::string get_caller() override{
		return caller;
	}
	std::string get_callee() override{
		return callee;
	}

private:
	std::string caller;
	std::string callee;
	ir_type *return_value;
	std::vector<ir_type*> passed_arguments;
};

class invoke : public function_call{
public:
	invoke(const char* clr, const char* cle) : function_call(){
		caller = clr;
		callee = cle;
		return_value = nullptr;
	}
	~invoke(){
		delete return_value;
		for (unsigned i = 0; i < passed_arguments.size(); i++){
			delete passed_arguments[i];
		}
		passed_arguments.erase(passed_arguments.begin(), passed_arguments.end());
	}
	void set_ret_value(ir_type* ret) override{
		if (return_value != nullptr){
			return_value->push(ret);
		}else{
			return_value = ret;
		}
	}
	void set_arg_value(ir_type* arg) override{
		if ( (passed_arguments.empty()) || (passed_arguments.back()->is_updated())){
			passed_arguments.push_back(arg);
		}else{
			if (passed_arguments.empty()){
			}
			passed_arguments.back()->push(arg);
		}
		return;
	}
	void print() override{

		#ifdef PREDUCE
		std::cout << ( caller.size() > 40 ? caller.substr(0, 39) : caller) << " " << (callee.size() > 40 ? callee.substr(0, 39) : callee);
		#else
		std::cout << caller << " " << callee;
		#endif
		if (return_value != nullptr){
			std::cout << ", ";
			return_value->print();
		}
		std::cout << ", ";
		if (passed_arguments.size() > 0){
			for (std::vector<ir_type*>::iterator it = passed_arguments.begin(); it != passed_arguments.end(); it++){
					(*it)->print();
			}
		}
		std::cout << std::endl;
		return;
	}
	std::string get_caller() override{
		return caller;
	}
	std::string get_callee() override{
		return callee;
	}

private:
	std::string caller;
	std::string callee;
	ir_type *return_value;
	std::vector<ir_type*> passed_arguments;
};

class exit_point : public function_call{
public:
	exit_point(const char *cl, const char *cle) : function_call(){
		caller = cl;
		callee = cle;
		return_value = nullptr;
	}
	~exit_point(){
		delete return_value;
		for (unsigned i = 0; i < passed_arguments.size(); i++){
			delete passed_arguments[i];
		}
		passed_arguments.erase(passed_arguments.begin(), passed_arguments.end());
	}
	void set_ret_value(ir_type* ret) override{

		if (return_value != nullptr){
			return_value->push(ret);
		}else{
			return_value = ret;
		}
	}
	void print() override{

		#ifdef PREDUCE
		std::cout << ( caller.size() > 40 ? caller.substr(0, 39) : caller) << " " << (callee.size() > 40 ? callee.substr(0, 39) : callee);
		#else
		std::cout << caller << " " << callee;
		#endif
		if (return_value != nullptr){
			std::cout << ", ";
			return_value->print();
			std::cout << ", ";
		}else{
		}
		std::cout << std::endl;
		return;
	}

	std::string get_caller() override{
		return caller;
	}

	std::string get_callee() override{
		return callee;
	}

private:
	std::string caller;
	std::string callee;
	ir_type *return_value;
	std::vector<ir_type*> passed_arguments;
};
