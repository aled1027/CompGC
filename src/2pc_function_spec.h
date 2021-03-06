#ifndef MPC_FUNCTION_SPEC_H
#define MPC_FUNCTION_SPEC_H

#include <jansson.h>
#include "2pc_garbled_circuit.h"

typedef enum {EVAL, CHAIN, INSTR_ERR} InstructionType;
typedef enum {PERSON_GARBLER = 0, PERSON_EVALUATOR = 1, PERSON_ERR = -1} Person;

typedef struct {
    /* not actually a single component, but a component type 
     * with num components of the type, and ids circuit_ids
     */
    int numComponentTypes;
    int totComponents;
    CircuitType *circuitType;
    int *nCircuits;
    int **circuitIds; 
    // so circuit_type[i] has circuitIds[i], which is an integer array of size nCircuits[i]
} 
FunctionComponent;


typedef struct {
    int input_idx; // so input_idx maps to (gc_id[i], wire_id[i]) for all i in {wire_id,...wire_id + dist}
    int gc_id; 
    int wire_id;
    int dist; // how many wires
    Person inputter; 
} 
InputMappingInstruction;

typedef struct {
    int size;
    InputMappingInstruction *imap_instr;
} 
InputMapping;

typedef struct {
    int size;
    int* input_idx; // so input_idx[i] maps to (gc_id[i], wire_id[i])
    int* gc_id; // each int array should be sizeof(int)*n (where n is number of inputs)
    int* wire_id;
    Person* inputter; // inputter[i] == GARBLER means the ith input should be inputted by the garbler.
}
OldInputMapping;

typedef struct {
    int circId;
} EvalInstruction;

typedef struct {
    int fromCircId;
    int fromWireId;
    int toCircId;
    int toWireId;
    int offsetIdx; 
    int wireDist;
} ChainInstruction;

typedef struct {
    InstructionType type;
    union {
        EvalInstruction ev;
        ChainInstruction ch;
    };
    // TODO not sure if fact below is still true.
    // index for block which is offset. If choffsetidx == -1, then zero_block
} 
Instruction;

typedef struct {
    int size;
    Instruction* instr;
} 
Instructions;

typedef struct {
    block labels[2];
    int gc_id;
    int wire_id;
}
OutputInstruction;

typedef struct {
    OutputInstruction *output_instruction;
    size_t size;
} 
OutputInstructions;

typedef struct {
    /* The specifiction for a function. 
     * That is, the components, instructions for evaluating and chaining components,
     * and everything else necessary for evaluating a function 
     */
    int n, m;
    int num_garb_inputs, num_eval_inputs;
    FunctionComponent components;
    InputMapping input_mapping;
    Instructions instructions;
    OutputInstructions output_instructions;
} 
FunctionSpec;

// json loading functions
// public interface
int load_function_via_json(char* path, FunctionSpec *function, ChainingType chainingType);
void print_function(FunctionSpec* function);
int freeFunctionSpec(FunctionSpec* function);
int writeInputMappingToBuffer(const OldInputMapping* input_mapping, char* buffer);
int readBufferIntoInputMapping(OldInputMapping* input_mapping, const char* buffer);
void deleteInputMapping(InputMapping *map);
void deleteOldInputMapping(OldInputMapping *map);
size_t inputMappingBufferSize(const OldInputMapping *map);

void newInputMapping(InputMapping *map, int size);
void newOldInputMapping(OldInputMapping *map, int num_garb_inputs, int num_eval_inputs);

void print_instructions(const Instructions* instr);

#endif
