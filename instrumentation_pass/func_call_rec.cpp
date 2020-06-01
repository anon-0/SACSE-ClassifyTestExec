#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"


#include "llvm/IR/Verifier.h"

using namespace llvm;

#include <unordered_map>
#include <unordered_set>
#include <iostream>

static cl::opt<bool>
    trace_globals("globals", cl::init(true), cl::Hidden,
                     cl::desc("Trace global variables "));

static cl::opt<bool>
   only_main_exit("only_main_exit", cl::init(false), cl::Hidden,
                    cl::desc("Select to instrument only global variables "));

static cl::opt<bool>
    exec_time("exec_time", cl::init(false), cl::Hidden,
                     cl::desc("Benchmark the execution time of the pass "));

static cl::opt<bool>
    only_defined("only_defined", cl::init(false), cl::Hidden,
                     cl::desc("Exclude externally declared funcs "));

static cl::opt<bool>
    no_exit("no_exit", cl::init(true), cl::Hidden,
                     cl::desc("print state only on main's return "));

static cl::opt<bool>
    temp_flag("temp_flag", cl::init(false), cl::Hidden,
                     cl::desc("print state only on main's return "));

namespace {
  struct Func_call : public ModulePass {

    static char ID;
    std::unordered_map<std::string, Value*> func_names_dict;
    std::unordered_map<std::string, GlobalVariable*> undefined_types;
    std::unordered_set<GlobalVariable*> undefined_types_set;
    std::unordered_map<std::string, GlobalVariable*> struct_types; 
    std::unordered_set<Type*> structs_parsed;
    std::unordered_set<std::string> intrinsic_globals = {"llvm.global_ctors", "llvm.global_dtors", "llvm.compiler.used"};
    std::unordered_set<std::string> excluded_callers = {"__cxx_global_var_init", "_GLOBAL__sub_I", "__cxa_atexit", "global_tracing_function"};

    Constant *print_trace, *global_trace=nullptr;
    Constant *record_call, *record_invoke, *record_exit_point;
    Constant *update_void, *update_1, *update_8, *update_16, *update_32, *update_64, *update_128, *update_float, *update_double,
              *update_ptr, *update_array, *update_struct, *update_exception, *update_undefined, *update_unallocated;

    Func_call() : ModulePass(ID) {}

    std::vector<Value*> load_call_names(std::string caller, std::string callee, Instruction* instr_after, LLVMContext &module_context){

        Type *caller_assigned_type = ArrayType::get(Type::getInt8Ty(module_context), caller.size() + 1 );
        Type *callee_assigned_type = ArrayType::get(Type::getInt8Ty(module_context), callee.size() + 1 );

        Instruction *caller_assigned_array = GetElementPtrInst::CreateInBounds(caller_assigned_type,
                                                                                func_names_dict[caller],
                                                                                ConstantInt::get(Type::getInt8Ty(module_context), 0),
                                                                                "caller_name",
                                                                                instr_after  );
        BitCastInst *caller_assigned_cast = new BitCastInst(caller_assigned_array,
                                                            Type::getInt8PtrTy(module_context),
                                                            "casted_caller", instr_after );
        Instruction *callee_assigned_array = GetElementPtrInst::CreateInBounds(callee_assigned_type,
                                                                                func_names_dict[callee],
                                                                                ConstantInt::get(Type::getInt8Ty(module_context), 0),
                                                                                "callee_name",
                                                                                instr_after  ); 

        BitCastInst *callee_assigned_cast = new BitCastInst(callee_assigned_array,
                                                            Type::getInt8PtrTy(module_context),
                                                            "casted_callee", instr_after );

        std::vector<Value*> ret = {caller_assigned_cast, callee_assigned_cast};
        return std::move(ret);
    }

    bool temp_exclude_struct;

    void parse_value(Value* call_rec, Value* &value, Type* &val_type, Instruction *insert_point, unsigned mode, Module &M, LLVMContext& module_context){

        if (val_type->isPointerTy()){

            PtrToIntInst *ptr_address = new PtrToIntInst(value, Type::getInt64Ty(module_context), "ptr_address", insert_point);
            Instruction *cmp_zero = new ICmpInst(insert_point, CmpInst::Predicate::ICMP_EQ, ptr_address, ConstantInt::get(Type::getInt64Ty(module_context), 0), "cmp_ptr_address");
            TerminatorInst *if_term, *else_term;

            SplitBlockAndInsertIfThenElse(cmp_zero, insert_point, &if_term, &else_term, nullptr);

            CallInst *update_val_ptr_null = CallInst::Create(update_ptr, {ptr_address, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", if_term);	(void) update_val_ptr_null;

            if (!val_type->getPointerElementType()->isFunctionTy() && ( val_type->getPointerElementType()->isStructTy() ? !dyn_cast<StructType>(val_type->getPointerElementType())->isOpaque() : true) ){
                LoadInst *ptr_deref = new LoadInst(value, "ptr_deref", else_term);
                value = ptr_deref;
            }

            CallInst *update_val_ptr_normal = CallInst::Create(update_ptr, {ptr_address, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", else_term);	(void) update_val_ptr_normal;
            val_type = val_type->getPointerElementType();
            parse_value(call_rec, value, val_type, else_term, mode, M, module_context);

        }else if (val_type->isArrayTy()){

        	CallInst *update_val_array = CallInst::Create(update_array, {ConstantInt::get(Type::getInt64Ty(module_context), val_type->getArrayNumElements()), call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);	(void) update_val_array;

        	for (uint64_t arr_i = 0; arr_i < val_type->getArrayNumElements(); arr_i++){

	        	Instruction *arr_elem = ExtractValueInst::Create(value, arr_i, "array_access", insert_point);

	        	Value *arr_val = arr_elem;
		        Type *arr_elem_type = arr_elem->getType();
		        parse_value(call_rec, arr_val, arr_elem_type, insert_point, mode, M, module_context);
        	}
        }else if (val_type == Type::getVoidTy(module_context)){
            CallInst *update_val_void = CallInst::Create(update_void, {call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);  (void) update_val_void;
        }else if (val_type == Type::getInt1Ty(module_context)){
            CallInst *update_val_1 = CallInst::Create(update_1, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_1;
        }else if (val_type == Type::getInt8Ty(module_context)){
            CallInst *update_val_8 = CallInst::Create(update_8, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_8;
        }else if (val_type == Type::getInt16Ty(module_context)){
            CallInst *update_val_16 = CallInst::Create(update_16, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);   (void) update_val_16;
        }else if (val_type == Type::getInt32Ty(module_context)){
            CallInst *update_val_32 = CallInst::Create(update_32, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);   (void) update_val_32;
        }else if (val_type == Type::getInt64Ty(module_context)){
            CallInst *update_val_64 = CallInst::Create(update_64, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);   (void) update_val_64;
        }else if (val_type == Type::getInt128Ty(module_context)){
            CallInst *update_val_128 = CallInst::Create(update_128, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_128;
        }else if (val_type == Type::getFloatTy(module_context)){
            CallInst *update_val_float = CallInst::Create(update_float, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_float;
        }else if (val_type == Type::getDoubleTy(module_context)){
            CallInst *update_val_double = CallInst::Create(update_double, {value, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point);   (void) update_val_double;
        }else if (val_type->isStructTy() && (exclude_structs(val_type) || temp_exclude_struct) ){

            bool was_true = temp_exclude_struct;
            temp_exclude_struct = true;
            std::string type_str;
            if (!dyn_cast<StructType>(val_type)->isLiteral()){
                type_str = val_type->getStructName(); 
            }else{
                type_str = "unnamed";
            }

            if (struct_types.find(type_str) == struct_types.end()){

                std::vector<Constant*> letters;
                for (const char& c : type_str){
                    letters.push_back(ConstantInt::get(Type::getInt8Ty(module_context), c));
                }
                letters.push_back(ConstantInt::get(Type::getInt8Ty(module_context), '\00'));

                GlobalVariable *type_global = new GlobalVariable(M,
                                        ArrayType::get(Type::getInt8Ty(module_context), type_str.size() + 1),
                                        true,
                                        GlobalValue::PrivateLinkage,
                                        ConstantArray::get(ArrayType::get(Type::getInt8Ty(module_context),
                                                                            type_str.size() + 1),
                                                            letters),
                                        type_str);
                struct_types.insert(std::make_pair(type_str, type_global));
            }

            Type *type_arraytype = ArrayType::get(Type::getInt8Ty(module_context), type_str.size() + 1 );
            Instruction *type_array = GetElementPtrInst::CreateInBounds(type_arraytype,
                                                                        struct_types[type_str],
                                                                        ConstantInt::get(Type::getInt8Ty(module_context), 0),
                                                                        "type_name",
                                                                        insert_point);
            BitCastInst *type_cast = new BitCastInst(type_array,
                                                    Type::getInt8PtrTy(module_context),
                                                    "casted_type_name", insert_point );

            if (structs_parsed.find(val_type) != structs_parsed.end()){
              CallInst *update_val_struct = CallInst::Create(update_struct, {type_cast, ConstantInt::get(Type::getInt64Ty(module_context), 0), call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_struct;
            }else{
              structs_parsed.insert(val_type);
              CallInst *update_val_struct = CallInst::Create(update_struct, {type_cast, ConstantInt::get(Type::getInt64Ty(module_context), val_type->getStructNumElements()), call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_struct;

              for (unsigned i = 0; i < val_type->getStructNumElements(); i++){

              	if (! is_func_ptr(val_type->getStructElementType(i))){
					Instruction *struct_elem = ExtractValueInst::Create(value, i, "struct_access", insert_point);
					Value *struct_val = struct_elem;
					Type *struct_elem_type = val_type->getStructElementType(i);
					parse_value(call_rec, struct_val, struct_elem_type, insert_point, mode, M, module_context);
				}else{
                    Type *temp_type = ArrayType::get(Type::getInt8Ty(module_context), type_str.size() + 1 );
                    Instruction *temp_array = GetElementPtrInst::CreateInBounds(temp_type,
                                                                                struct_types[type_str],
                                                                                ConstantInt::get(Type::getInt8Ty(module_context), 0),
                                                                                "type_name",
                                                                                insert_point);
                    BitCastInst *temp_cast = new BitCastInst(temp_array,
                                                            Type::getInt8PtrTy(module_context),
                                                            "casted_type_name", insert_point );
                    CallInst *update_val_undefined = CallInst::Create(update_undefined, {temp_cast, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_undefined;
                }
              }
				structs_parsed.erase(val_type);
              if (!was_true){
                temp_exclude_struct = false;
              }
            }
        }else{

            std::string type_str;
            if (val_type->isStructTy()){
                if (!dyn_cast<StructType>(val_type)->isLiteral()){
                    type_str = val_type->getStructName();
                }else{
                    type_str = "unnamed";
                }                
            }else{
                raw_string_ostream type_stream(type_str);
                val_type->print(type_stream);
            }

            if (undefined_types.find(type_str) == undefined_types.end()){

                std::vector<Constant*> letters;
                for (const char& c : type_str){
                    letters.push_back(ConstantInt::get(Type::getInt8Ty(module_context), c));
                }
                letters.push_back(ConstantInt::get(Type::getInt8Ty(module_context), '\00'));

                GlobalVariable *type_global = new GlobalVariable(M,
                                        ArrayType::get(Type::getInt8Ty(module_context), type_str.size() + 1),
                                        true,
                                        GlobalValue::PrivateLinkage,
                                        ConstantArray::get(ArrayType::get(Type::getInt8Ty(module_context),
                                                                            type_str.size() + 1),
                                                            letters),
                                        type_str);
                undefined_types.insert(std::make_pair(type_str, type_global));
                undefined_types_set.insert(type_global);
            }

            Type *type_arraytype = ArrayType::get(Type::getInt8Ty(module_context), type_str.size() + 1 );
            Instruction *type_array = GetElementPtrInst::CreateInBounds(type_arraytype,
                                                                        undefined_types[type_str],
                                                                        ConstantInt::get(Type::getInt8Ty(module_context), 0),
                                                                        "type_name",
                                                                        insert_point);
            BitCastInst *type_cast = new BitCastInst(type_array,
                                                    Type::getInt8PtrTy(module_context),
                                                    "casted_type_name", insert_point );
            CallInst *update_val_undefined = CallInst::Create(update_undefined, {type_cast, call_rec, ConstantInt::get(Type::getInt32Ty(module_context), mode)}, "", insert_point); (void) update_val_undefined;
        }
    	return;
    }

    void trace_global_variables(Module &M){

    	unsigned var_index = 0;
        LLVMContext &module_context = M.getContext();
        Function *global_tracing_func = Function::Create(FunctionType::get(Type::getVoidTy(module_context), false),
                                        GlobalValue::PrivateLinkage,
                                        "global_tracing_function",
                                        M);
        global_tracing_func->addFnAttr(Attribute::NoUnwind);
        global_tracing_func->addFnAttr(Attribute::NoInline);
        global_tracing_func->addFnAttr(Attribute::OptimizeNone);
        global_trace = cast<Constant>(global_tracing_func);
		BasicBlock *first_block = BasicBlock::Create(M.getContext(), "entrypoint", global_tracing_func);
		Instruction *insert_point = ReturnInst::Create(M.getContext(), nullptr, first_block);

    	for (SymbolTableList<GlobalVariable>::iterator gl = M.global_begin(); gl != M.global_end(); gl++){

            if (undefined_types_set.find(&*gl) != undefined_types_set.end()
                ||  intrinsic_globals.find(gl->getName()) != intrinsic_globals.end()
                || gl->isConstant() ){
                continue;
            }

    		Value *global_value = dyn_cast<Value>(&*gl);
    		Type *global_val_type = gl->getType();

        temp_exclude_struct = false;
    		parse_value(ConstantInt::get(Type::getInt32Ty(module_context), var_index), global_value, global_val_type, insert_point, 0, M, module_context);
    		var_index++;
    	}

    	return;
    }

    void declare_extern_funcs(Module &M, LLVMContext &module_context){

        print_trace = M.getOrInsertFunction("_Z11print_tracev", Type::getVoidTy(module_context));

        record_call = M.getOrInsertFunction("_Z11record_callPKcS0_", Type::getInt32Ty(module_context), Type::getInt8PtrTy(module_context), Type::getInt8PtrTy(module_context));
        record_invoke = M.getOrInsertFunction("_Z13record_invokePKcS0_", Type::getInt32Ty(module_context), Type::getInt8PtrTy(module_context), Type::getInt8PtrTy(module_context));
        record_exit_point = M.getOrInsertFunction("_Z17record_exit_pointPKcS0_", Type::getInt32Ty(module_context), Type::getInt8PtrTy(module_context), Type::getInt8PtrTy(module_context));

        update_void = M.getOrInsertFunction("_Z11update_voidjj", Type::getVoidTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_1 = M.getOrInsertFunction("_Z8update_1bjj", Type::getVoidTy(module_context), Type::getInt1Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_8 = M.getOrInsertFunction("_Z8update_8hjj", Type::getVoidTy(module_context), Type::getInt8Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_16 = M.getOrInsertFunction("_Z9update_16tjj", Type::getVoidTy(module_context), Type::getInt16Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_32 = M.getOrInsertFunction("_Z9update_32jjj", Type::getVoidTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_64 = M.getOrInsertFunction("_Z9update_64mjj", Type::getVoidTy(module_context), Type::getInt64Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_128 = M.getOrInsertFunction("_Z10update_128ojj", Type::getVoidTy(module_context), Type::getInt128Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_float = M.getOrInsertFunction("_Z12update_floatfjj", Type::getVoidTy(module_context), Type::getFloatTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_double = M.getOrInsertFunction("_Z13update_doubledjj", Type::getVoidTy(module_context), Type::getDoubleTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_ptr = M.getOrInsertFunction("_Z10update_ptrmjj", Type::getVoidTy(module_context), Type::getInt64Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_array = M.getOrInsertFunction("_Z12update_arraymjj", Type::getVoidTy(module_context), Type::getInt64Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_struct = M.getOrInsertFunction("_Z13update_structPKcmjj", Type::getVoidTy(module_context), Type::getInt8PtrTy(module_context), Type::getInt64Ty(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_exception = M.getOrInsertFunction("_Z16update_exceptionj", Type::getVoidTy(module_context), Type::getInt32Ty(module_context));
        update_undefined = M.getOrInsertFunction("_Z16update_undefinedPKcjj", Type::getVoidTy(module_context), Type::getInt8PtrTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        update_unallocated = M.getOrInsertFunction("_Z11update_unallocatedjj", Type::getVoidTy(module_context), Type::getInt32Ty(module_context), Type::getInt32Ty(module_context));
        return;
    }

    void funcs_to_vec(std::vector<std::string> &name_vec, Module &M){

        for (SymbolTableList<Function>::iterator F = M.begin(); F != M.end(); F++){
            if (!F->isIntrinsic()){
                name_vec.push_back(F->getName());
            }
        }
        name_vec.push_back("ret");
		name_vec.push_back("resume");
        return;
    }

    void func_vec_to_globals(std::vector<std::string> &func_names, Module &M, LLVMContext &module_context){

        for (auto &f_name : func_names){

            std::vector<Constant*> name;
            for (const char& c : f_name){
                name.push_back(ConstantInt::get(Type::getInt8Ty(module_context), c));
            }
            name.push_back(ConstantInt::get(Type::getInt8Ty(module_context), '\00'));
            GlobalVariable *name_global = new GlobalVariable(M,
                                                            ArrayType::get(Type::getInt8Ty(module_context), f_name.size() + 1),
                                                            true,
                                                            GlobalValue::PrivateLinkage,
                                                            ConstantArray::get(ArrayType::get(Type::getInt8Ty(module_context),
                                                                                                f_name.size() + 1),
                                                                                name),
                                                            f_name);
            name_global->setAlignment(1);
            func_names_dict.insert(std::make_pair(f_name, name_global));
            intrinsic_globals.insert(f_name);
        }

        return;
    }

    void setup_exit_point(Value *call_rec, Instruction *insert_point, Module &M, LLVMContext& module_context){

		CallInst *update_void_exit = CallInst::Create(update_void, {call_rec, ConstantInt::get(Type::getInt32Ty(module_context), 1)}, "", insert_point);  (void) update_void_exit;
    if (trace_globals){
      CallInst *gl_trace = CallInst::Create(global_trace, "", insert_point);	(void) gl_trace;
    }else{
      outs() << "Skipping CallInst to global_trace\n";
    }
		CallInst *print_rec = CallInst::Create(print_trace, "", insert_point);	(void) print_rec;
    	return;
    }

    bool setup_main_exit(Module &M, LLVMContext& module_context){

      Function *main = M.getFunction("main");

      if (main != nullptr){
      for (SymbolTableList<BasicBlock>::iterator BB = main->begin(); BB != main->end(); BB++){
              for (SymbolTableList<Instruction>::iterator I = BB->begin(); I != BB->end(); I++){
                  if (isa<ReturnInst>(&*I)){

                      if (trace_globals){
                        CallInst *gl_trace = CallInst::Create(global_trace, "", &*I);
                        (void) gl_trace;
                      }else{
                        outs() << "Skipping CallInst to global_trace\n";
                      }
                      CallInst *call_rec = CallInst::Create(record_exit_point, load_call_names(std::string("main"), std::string("ret"), &*I, module_context), "call_index", &*I);
                      Value *main_ret_val = dyn_cast<ReturnInst>(&*I)->getReturnValue();
                      Type *main_ret_type = main->getReturnType();

                      temp_exclude_struct = false;
                      parse_value(call_rec, main_ret_val, main_ret_type, &*I, 1, M, module_context);
                      while ( &*BB != I->getParent()){
                          BB++;
                      }

                      CallInst *print_rec = CallInst::Create(print_trace, "", &*I);   (void) print_rec;

                  }else if (isa<ResumeInst>(&*I)){

                      if (trace_globals){
                        CallInst *gl_trace = CallInst::Create(global_trace, "", &*I);
                        (void) gl_trace;
                      }else{
                        outs() << "Skipping CallInst to global_trace\n";
                      }
                      CallInst *call_rec = CallInst::Create(record_exit_point, load_call_names(std::string("main"), std::string("resume"), &*I, module_context), "call_index", &*I);
                      CallInst *update_main_exception = CallInst::Create(update_exception, {call_rec}, "", &*I);
                      CallInst *print_rec = CallInst::Create(print_trace, "", &*I);
                      (void) call_rec; (void) update_main_exception; (void) print_rec;
                  }
              }
          }
      }else{
        return false; 
      }
      return true;
    }

    void update_unwind_block(BasicBlock *unw, BasicBlock *source, Value *cur_index, LLVMContext& module_context){

    	static std::unordered_map<BasicBlock*, PHINode*> accessed_unwind_blocks;

    	if (accessed_unwind_blocks.find(unw) == accessed_unwind_blocks.end()){
    		PHINode *invoke_index = PHINode::Create(Type::getInt32Ty(module_context), 0, "invoke_index", &*unw->begin());
    		invoke_index->addIncoming(cur_index, source);
    		accessed_unwind_blocks.insert(std::make_pair(unw, invoke_index));
			CallInst *exception_call = CallInst::Create(update_exception, {dyn_cast<Value>(invoke_index)}, "", &*unw->rbegin());	(void) exception_call;
    	}else{
    		accessed_unwind_blocks[unw]->addIncoming(cur_index, source);
    	}

    	return;
    }

    bool is_func_ptr(Type *t){

    	while(isa<PointerType>(t)){
    		t = t->getContainedType(0);
    	}

    	return (isa<FunctionType>(t) ? true : false);
    }

    bool exclude_structs(Type *t){

      if(t->isStructTy()){
        if (!dyn_cast<StructType>(t)->isLiteral()){
          if (
            t->getStructName().find("class.dev::eth::BlockHeader") != std::string::npos ||
            t->getStructName().find("struct.dev::eth::ChainOperationParams") != std::string::npos ||
            t->getStructName().find("class.boost::multiprecision::number") != std::string::npos ||
            t->getStructName().find("class.std::__cxx11::basic_string") != std::string::npos ){


            return true;
          }else{
            return false;
          }
        }else{
          return false;
        }
      }

      return true;
    }

    bool runOnModule(Module &M) override{


        LLVMContext &module_context = M.getContext();
        std::vector<std::string> func_names;  

        funcs_to_vec(func_names, M);
        declare_extern_funcs(M, module_context);

        if (trace_globals){
          outs() << "Globals\n";
          trace_global_variables(M);
        }
        func_vec_to_globals(func_names, M, module_context);

        if (only_main_exit){
          outs() << "Setting up only main exit\n";
          return setup_main_exit(M, module_context);
        }

        for (SymbolTableList<Function>::iterator F = M.begin(); F != M.end(); F++){

            bool skip = false;
            for (auto &forb_str : excluded_callers){
            	if (F->getName().find(forb_str) != std::string::npos){
                    skip = true;
                    break;
                }
            }
            if (skip){  continue;   }

        for (SymbolTableList<BasicBlock>::iterator BB = F->begin(); BB != F->end(); BB++){
        for (SymbolTableList<Instruction>::iterator I = BB->begin(); I != BB->end(); I++){

            if (isa<CallInst>(&*I)){

                CallInst *func_call = dyn_cast<CallInst>(&*I);
                if ( func_call->getCalledFunction() == nullptr || (!isa<Function>(func_call->getCalledFunction())) || ( only_defined && func_call->getCalledFunction()->getInstructionCount() == 0 && func_call->getCalledFunction()->getName() != "exit") ){
                	continue;
                }

                Function *called_func = func_call->getCalledFunction();
                CallInst *call_rec;
                Type *func_ret_type = called_func->getReturnType();
                Value *func_ret_val = &*I;

                if ( func_names_dict.find(called_func->getName()) == func_names_dict.end()){
                    continue;
                }

                Instruction *next = I->getNextNode();

                if (called_func->getName() == "exit" && !no_exit){
                	call_rec = CallInst::Create(record_exit_point, load_call_names(F->getName(), called_func->getName(), &*I, module_context), "call_index", &*I);
                }else{
                	call_rec = CallInst::Create(record_call, load_call_names(F->getName(), called_func->getName(), I->getNextNode(), module_context), "call_index", I->getNextNode());
                }

                unsigned arg_iter = 0;
                if (called_func->arg_size() > 0){
                	if (called_func->arg_begin()->hasStructRetAttr()){
                		func_ret_type = called_func->arg_begin()->getType();
                		func_ret_val = func_call->getArgOperand(0);
						arg_iter = 1;
                	}
                }

                for (unsigned arg = arg_iter; arg != func_call->getNumArgOperands(); arg++){

                    Value *argument = func_call->getArgOperand(arg);
                    Type *arg_type = argument->getType();
                    temp_exclude_struct = false;
                    parse_value(call_rec, argument, arg_type, next, 2, M, module_context);
                }
                temp_exclude_struct = false;
                parse_value(call_rec, func_ret_val, func_ret_type, next, 1, M, module_context);
                if (called_func->getName() == "exit" && !no_exit){	setup_exit_point(call_rec, next, M, module_context); }
                while ( &*BB != I->getParent()){ BB++; }

            } else if (isa<InvokeInst>(&*I)){

                InvokeInst *invoke_instr = dyn_cast<InvokeInst>(&*I);

                if ( invoke_instr->getCalledFunction() == nullptr || (!isa<Function>(invoke_instr->getCalledFunction())) || ( only_defined && invoke_instr->getCalledFunction()->getInstructionCount() == 0 )){
                	// update_unwind_block(invoke_instr->getUnwindDest(), &*BB, ConstantInt::get(Type::getInt32Ty(module_context), 0), module_context);
                	continue;
                }

                Instruction *insert_point_n = &*(invoke_instr->getNormalDest()->rbegin());

                for (auto rev_it = invoke_instr->getNormalDest()->rbegin(); rev_it != invoke_instr->getNormalDest()->rend(); rev_it++){
                	if (isa<CallInst>(&*rev_it)){
                		insert_point_n = &*rev_it;
                	}
                }

                if (invoke_instr->getNormalDest()->getSinglePredecessor() == nullptr){
                    BasicBlock *mid_inv_block = BasicBlock::Create(module_context, "mid_invoke", &*F, invoke_instr->getNormalDest());
                    Instruction *jump_normal_dest = BranchInst::Create(invoke_instr->getNormalDest(), mid_inv_block);
                    insert_point_n = jump_normal_dest;

                    Instruction *t = &*invoke_instr->getNormalDest()->begin();
                    while (isa<PHINode>(t)){
                        for (unsigned b = 0; b < dyn_cast<PHINode>(t)->getNumIncomingValues(); b++){
                            if (dyn_cast<PHINode>(t)->getIncomingBlock(b) == invoke_instr->getParent()){
                                dyn_cast<PHINode>(t)->setIncomingBlock(b, mid_inv_block);

                            }
                        }
                        t = t->getNextNode();
                    }
                    invoke_instr->setNormalDest(mid_inv_block);
                }
                Function *invoked_func = invoke_instr->getCalledFunction();
                CallInst *call_rec = CallInst::Create(record_invoke, load_call_names(F->getName(), invoked_func->getName(), &*I, module_context), "call_index", insert_point_n);
                Type *func_ret_type = invoked_func->getReturnType();
                Value *func_ret_val = &*I;
                unsigned arg_iter = 0;

                if (invoked_func->arg_size() > 0){
                    if (invoked_func->arg_begin()->hasStructRetAttr()){
                        func_ret_type = invoked_func->arg_begin()->getType();
                        func_ret_val = invoke_instr->getArgOperand(0);
                    arg_iter = 1;
                    }
                }

                for (unsigned arg = arg_iter; arg != invoke_instr->getNumArgOperands(); arg++){

                    Value *argument = invoke_instr->getArgOperand(arg);
                    Type *arg_type = argument->getType();
                    temp_exclude_struct = false;
                    parse_value(call_rec, argument, arg_type, insert_point_n, 2, M, module_context);
                }
                temp_exclude_struct = false;
                parse_value(call_rec, func_ret_val, func_ret_type, insert_point_n, 1, M, module_context);
                while ( &*BB != I->getParent()){ 	BB++; 	}
                // update_unwind_block(invoke_instr->getUnwindDest(), &*BB, call_rec, module_context);
            }
        }
        }
        }

        bool main_was_edited = setup_main_exit(M, module_context);
        return main_was_edited;
    }

  };
}

char Func_call::ID = 0;
static RegisterPass<Func_call> X("func_call", "", false, false);
