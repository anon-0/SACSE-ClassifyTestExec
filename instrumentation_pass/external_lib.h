#include <iostream>

void deallocate_memory();

void print_trace();
unsigned int record_call(const char* caller, const char* callee);
unsigned int record_invoke(const char* caller, const char* callee);
unsigned int record_exit_point(const char* caller, const char* callee);
void update_8(uint8_t value, unsigned int index, unsigned int type);
void update_16(uint16_t value, unsigned int index, unsigned int type);
void update_32(uint32_t value, unsigned int index, unsigned int type);
void update_64(uint64_t value, unsigned int index, unsigned int type);
void update_float(float value, unsigned int index, unsigned int type);
void update_double(double value, unsigned int index, unsigned int type);
void update_ptr(uint64_t address, unsigned int index, unsigned int type);
void update_array(uint64_t size, unsigned int index, unsigned int type);
void update_struct(const char *name, uint64_t size, unsigned int index, unsigned int type);
void update_exception(unsigned int index);
void update_undefined(const char* type_name, unsigned int index);