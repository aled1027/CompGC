#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// #include "gc_comm.h"
// #include "common.h"
#include "2pc_garbler.h" 
#include "2pc_evaluator.h" 
#include "tests.h"

int 
go() 
{
    int num_circs = 3; // can be an upper bound
    int num_maps = 3; // can be an upper bound
    int n = 2; // number of inputs to each gc
    int num_instr = 7;
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_circs);
    Instruction* instructions = malloc(sizeof(Instruction) *  8);
    
    block* receivedOutputMap; // = (block *) memalign(128, sizeof(block) * 2 * 2);
    GarbledCircuit gcs[num_circs];
    InputLabels extractedLabels[num_circs];
    assert(chained_gcs && instructions && receivedOutputMap);

    for (int i=0; i<num_circs; i++) {
        extractedLabels[i] = memalign(128, sizeof(block)*2);
        assert(extractedLabels[i]);
    }

    int* inputs[2];
    inputs[0] = malloc(sizeof(int*)); inputs[1] = malloc(sizeof(int*));

    inputs[0][0] = 1;
    inputs[0][1] = 1;
    inputs[1][0] = 1;
    inputs[1][1] = 1;
    printf("inputs: (%d,%d), (%d,%d)\n", inputs[0][0], inputs[0][1], inputs[1][0], inputs[1][1]);

    createGarbledCircuits(chained_gcs, num_circs);
    createInstructions(instructions, chained_gcs);

    // send garbled circuits over
    for (int i=0; i<num_circs; i++) {
        gcs[i] = chained_gcs[i].gc; // shallow copy; pointers are not copied.
    }

    receivedOutputMap = chained_gcs[2].outputMap; // would be sent over by garbler

	//extractLabels(extractedLabels[0], chained_gcs[0].inputLabels, inputs[0], n);
	//extractLabels(extractedLabels[1], chained_gcs[1].inputLabels, inputs[1], n);
    block** inputLabels = malloc(sizeof(block*) * 2);
    inputLabels[0] = chained_gcs[0].inputLabels;
    inputLabels[1] = chained_gcs[1].inputLabels;

    int* output = malloc(sizeof(int) * chained_gcs[0].gc.m);
    assert(output);
    chainedEvaluate(gcs, 3, instructions, num_instr, inputLabels, receivedOutputMap, inputs, output);
    printf("%d, %d\n", output[0], output[1]);

    for (int i=0; i<num_circs; i++) {
        freeChainedGarbledCircuit(&chained_gcs[i]);
        free(extractedLabels[i]);
    }
    free(chained_gcs);
    free(instructions);
    free(output);
    free(inputLabels);
    free(inputs[0]);
    free(inputs[1]);
    
    return 0;
}

int 
go2() 
{
    FunctionSpec function;

    int num_circs = 3, num_gcs = 3;
    GarbledCircuit gcs[num_circs];
    ChainedGarbledCircuit* chained_gcs = malloc(sizeof(ChainedGarbledCircuit) * num_circs);
    block** inputLabels = malloc(sizeof(block*) * 2);
    block* receivedOutputMap; // = (block *) memalign(128, sizeof(block) * 2 * 2);
    int* inputs[2];
    inputs[0] = malloc(sizeof(int*)); inputs[1] = malloc(sizeof(int*));
    int* output = malloc(sizeof(int) * chained_gcs[0].gc.m);

    
    createGarbledCircuits(chained_gcs, num_circs);
    garbler_init(&function, chained_gcs, num_gcs);

    inputLabels[0] = chained_gcs[0].inputLabels;
    inputLabels[1] = chained_gcs[1].inputLabels;
    receivedOutputMap = chained_gcs[2].outputMap; 

    for (int i=0; i<num_circs; i++) 
        gcs[i] = chained_gcs[i].gc; // shallow copy; pointers are not copied.

    inputs[0][0] = 0;
    inputs[0][1] = 0;
    inputs[1][0] = 1;
    inputs[1][1] = 0;
    printf("inputs: (%d,%d), (%d,%d)\n", inputs[0][0], inputs[0][1], inputs[1][0], inputs[1][1]);

    chainedEvaluate(gcs, num_gcs, function.instructions.instr, function.instructions.size, 
            inputLabels, receivedOutputMap, inputs, output);

    freeFunctionSpec(&function);
    return SUCCESS;
}

int 
main(int argc, char* argv[]) 
{
	srand(time(NULL));
    srand_sse(time(NULL));
    run_all_tests();
    //go();
    //go2();
    //test_saving_reading();
    //json_test();
    return 0;
}


