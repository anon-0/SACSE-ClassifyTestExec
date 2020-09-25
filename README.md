
# Supervised Learning over Test Executions as a Test Oracle
Code supplementary material. This repository contains execution traces (both in raw and encoded format) to train from scratch a model on SEAL Encryptor.

Run
```
python run.py
```

The default configuration for this run is a training set of 30% of total traces, as per Table 1 in the paper.

## Files

```
1. instrumentation_pass/        (LLVM instrumentation pass folder)
2. SEALEncryptor_traces/        (Folder with the execution traces)
3. run.py                       (Automated script to dispatch training)
4. python files 				(Model parser for pytorch, training and classification routines)
5. trace_model.pymodel 			(The NN model used in the paper)
```
## Usage

The instrumentation pass (``func_call_rec.cpp``) requires ``LLVM 8.0.0`` and ``Clang 8.0.0`` installed.
Inside the ``instrumentation_pass/`` folder, the source code for the external instrumentation library that stores the trace information is also included (``external_lib.cpp``, ``external_lib.h``, ``function_call.h``, ``ir_type.h``) . 
We include the source code and the traces gathered using these tools.

Both Python scripts require ``Python 3.7`` and ``Pytorch`` installed.
