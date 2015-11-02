#include "2pc_garbled_circuit.h"

#include <malloc.h>
#include <assert.h>
#include <stdint.h>

#include "utils.h"
#include "2pc_common.h"

int 
createGarbledCircuits(ChainedGarbledCircuit* chained_gcs, int n) 
{
    block delta = randomBlock();
    *((uint16_t *) (&delta)) |= 1;

    for (int i = 0; i < n; i++) {
        GarbledCircuit* p_gc = &(chained_gcs[i].gc);
        buildAdderCircuit(p_gc);
        chained_gcs[i].id = i;
        chained_gcs[i].type = ADDER22;
        chained_gcs[i].inputLabels = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.n );
        chained_gcs[i].outputMap = memalign(128, sizeof(block) * 2 * chained_gcs[i].gc.m);
        assert(chained_gcs[i].inputLabels != NULL && chained_gcs[i].outputMap != NULL);
        garbleCircuit(p_gc, chained_gcs[i].inputLabels, chained_gcs[i].outputMap, &delta);
    }
    return 0;
}

int 
freeChainedGarbledCircuit(ChainedGarbledCircuit *chained_gc) 
{
    removeGarbledCircuit(&chained_gc->gc); // frees memory in gc
    free(chained_gc->inputLabels);
    free(chained_gc->outputMap);
    // TODO free other things
    return 0;
}

void
buildAdderCircuit(GarbledCircuit *gc) 
{
    int n = 2; // number of inputs 
    int m = 2; // number of outputs
    int q = 3; // number of gates
	int r = n+q;  // number of wires

	int *inputs = (int *) malloc(sizeof(int) * n);
	countToN(inputs, n);
    int* outputs = (int*) malloc(sizeof(int) * m);
	block *labels = (block*) malloc(sizeof(block) * 2 * n);
	block *outputmap = (block*) malloc(sizeof(block) * 2 * m);

	GarblingContext gc_context;

	createInputLabels(labels, n);
	createEmptyGarbledCircuit(gc, n, m, q, r, labels);
	startBuilding(gc, &gc_context);
	ADD22Circuit(gc, &gc_context, inputs, outputs);
	finishBuilding(gc, &gc_context, outputmap, outputs);

    free(gc_context.fixedWires);
    free(inputs);
    free(outputs);
    free(labels);
    free(outputmap);
}

int 
saveChainedGC(ChainedGarbledCircuit* chained_gc, bool isGarbler) 
{
    char* buffer;
    size_t buffer_size = 1;

    GarbledCircuit* gc = &chained_gc->gc;

    buffer_size += sizeof(int) * 4;
    buffer_size += sizeof(GarbledGate) * gc->q;
    buffer_size += sizeof(GarbledTable) * gc->q;
    buffer_size += sizeof(Wire) * gc->r; // wires
    buffer_size += sizeof(int) * gc->m; // outputs
    buffer_size += sizeof(block);
    buffer_size += sizeof(int); // id
    buffer_size += sizeof(CircuitType); 
    
    if (isGarbler) {
        buffer_size += sizeof(block)* 2 * gc->n; // inputLabels
        buffer_size += sizeof(block) * 2 *gc->m; // outputMap
    }

    buffer = malloc(buffer_size);

    size_t p = 0;
    memcpy(buffer+p, &(gc->n), sizeof(gc->n));
    p += sizeof(gc->n);
    memcpy(buffer+p, &(gc->m), sizeof(gc->m));
    p += sizeof(gc->m);
    memcpy(buffer+p, &(gc->q), sizeof(gc->q));
    p += sizeof(gc->q);
    memcpy(buffer+p, &(gc->r), sizeof(gc->r));
    p += sizeof(gc->r);

    // save garbled gates
    memcpy(buffer+p, gc->garbledGates, sizeof(GarbledGate)*gc->q);
    p += sizeof(GarbledGate) * gc->q;

    // save garbled table
    memcpy(buffer+p, gc->garbledTable, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable)*gc->q;

    // save wires
    memcpy(buffer+p, gc->wires, sizeof(Wire)*gc->r);
    p += sizeof(Wire)*gc->r;

    // save outputs
    memcpy(buffer+p, gc->outputs, sizeof(int)*gc->m);
    p += sizeof(int) * gc->m; 

    // save globalKey
    memcpy(buffer + p, &(gc->globalKey), sizeof(block));
    p += sizeof(block);

    // id, type, inputLabels, outputmap
    memcpy(buffer + p, &(chained_gc->id), sizeof(int));
    p += sizeof(int);

    memcpy(buffer + p, &(chained_gc->type), sizeof(CircuitType));
    p += sizeof(CircuitType);

    if (isGarbler) {
        memcpy(buffer + p, chained_gc->inputLabels, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;

        memcpy(buffer + p, chained_gc->outputMap, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2 * gc->m;
    }

    if (p >= buffer_size) {
        printf("Buffer overflow error in save.\n"); 
        return FAILURE;
    }

    char fileName[50];

    if (isGarbler) {
        sprintf(fileName, GARBLER_GC_PATH, chained_gc->id);
    } else { 
        sprintf(fileName, EVALUATOR_GC_PATH, chained_gc->id);
    }

    if (writeBufferToFile(buffer, buffer_size, fileName) == FAILURE) {
        return FAILURE;
    }

    return SUCCESS;
}
    
int 
loadChainedGC(ChainedGarbledCircuit* chained_gc, int id, bool isGarbler) 
{
    char* buffer, fileName[50];
    long fs;

    if (isGarbler) {
        sprintf(fileName, GARBLER_GC_PATH, id);
    } else { 
        sprintf(fileName, EVALUATOR_GC_PATH, id);
    }

    fs = filesize(fileName);
    buffer=malloc(fs);

    if (readFileIntoBuffer(buffer, fileName) == FAILURE) {
        return FAILURE;
    }

    GarbledCircuit* gc = &chained_gc->gc;

    size_t p = 0;
    memcpy(&(gc->n), buffer+p, sizeof(gc->n));
    p += sizeof(gc->n);
    memcpy(&(gc->m), buffer+p, sizeof(gc->m));
    p += sizeof(gc->m);
    memcpy(&(gc->q), buffer+p, sizeof(gc->q));
    p += sizeof(gc->q);
    memcpy(&(gc->r), buffer+p, sizeof(gc->r));
    p += sizeof(gc->r);

    // load garbled gates
    gc->garbledGates = malloc(sizeof(GarbledGate) * gc->q);
    memcpy(gc->garbledGates, buffer+p, sizeof(GarbledGate)*gc->q);
    p += sizeof(GarbledGate) * gc->q;

    // load garbled table
    gc->garbledTable = malloc(sizeof(GarbledTable) * gc->q);
    memcpy(gc->garbledTable, buffer+p, sizeof(GarbledTable)*gc->q);
    p += sizeof(GarbledTable)*gc->q;

    // load wires
    gc->wires = malloc(sizeof(Wire) * gc->r);
    memcpy(gc->wires, buffer+p, sizeof(Wire)*gc->r);
    p += sizeof(Wire)*gc->r;

    // load outputs
    gc->outputs = malloc(sizeof(int) * gc->m);
    memcpy(gc->outputs, buffer+p, sizeof(int) * gc->m);
    p += sizeof(int) * gc->m;

    // save globalKey
    memcpy(&(gc->globalKey), buffer+p, sizeof(block));
    p += sizeof(block);

    // id
    memcpy(&(chained_gc->id), buffer+p, sizeof(int));
    p += sizeof(int);

    // type
    memcpy(&(chained_gc->type), buffer+p, sizeof(CircuitType));
    p += sizeof(CircuitType);
    

    if (isGarbler) {
        // inputLabels
        chained_gc->inputLabels = memalign(128, sizeof(block)*2*gc->n);
        memcpy(chained_gc->inputLabels, buffer+p, sizeof(block)*2*gc->n);
        p += sizeof(block) * 2 * gc->n;

        // outputMap
        chained_gc->outputMap = memalign(128, sizeof(block)*2*gc->m);
        memcpy(chained_gc->outputMap, buffer+p, sizeof(block)*2*gc->m);
        p += sizeof(block) * 2*gc->m;
    }

    if (p > fs) {
        printf("Buffer overflow error\n"); 
        return FAILURE;
    }

    return SUCCESS;
}

void
print_block(block blk)
{
    uint64_t *val = (uint64_t *) &blk;
    printf("%016lx%016lx", val[1], val[0]);
}

void
print_garbled_gate(GarbledGate *gg)
{
    printf("%ld %ld %ld %d %d\n", gg->input0, gg->input1, gg->output, gg->id,
           gg->type);
}

void
print_garbled_table(GarbledTable *gt)
{
    print_block(gt->table[0]);
    printf(" ");
    print_block(gt->table[1]);
    printf(" ");
    print_block(gt->table[2]);
    printf(" ");
    print_block(gt->table[3]);
    printf("\n");
}

void
print_wire(Wire *w)
{
    printf("%ld ", w->id);
    print_block(w->label0);
    printf(" ");
    print_block(w->label1);
    printf("\n");
}

void
print_gc(GarbledCircuit *gc)
{
    printf("n = %d\n", gc->n);
    printf("m = %d\n", gc->m);
    printf("q = %d\n", gc->q);
    printf("r = %d\n", gc->r);
    for (int i = 0; i < gc->q; ++i) {
        printf("garbled gate %d: ", i);
        print_garbled_gate(&gc->garbledGates[i]);
        printf("garbled table %d: ", i);
        print_garbled_table(&gc->garbledTable[i]);
    }
    for (int i = 0; i < gc->r; ++i) {
        printf("wire %d: ", i);
        print_wire(&gc->wires[i]);
    }
    for (int i = 0; i < gc->m; ++i) {
        printf("%d\n", gc->outputs[i]);
    }
}

void
print_blocks(const char *str, block *blks, int length)
{
    for (int i = 0; i < length; ++i) {
        printf("%s ", str);
        print_block(blks[i]);
        printf("\n");
    }
}