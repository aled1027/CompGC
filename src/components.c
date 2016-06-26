#include "components.h"
#include "utils.h"

#include "garble.h"
#include "circuits.h"
//#include "gates.h"

#include <string.h>
#include <assert.h>
#include <math.h>


static void
bitwiseMUX(garble_circuit *gc, garble_context *gctxt, int the_switch, const int *inps, 
		int ninputs, int *outputs)
{
	//assert(ninputs % 2 == 0 && "ninputs should be even because we are muxing");
	//assert(outputs && "final should already be malloced");
	int split = ninputs / 2;

	for (int i = 0; i < split; i++) {
		//circuit_mux21(gc, gctxt, the_switch, inps[i], inps[i + split], &outputs[i]);
		new_circuit_mux21(gc, gctxt, the_switch, inps[i], inps[i + split], &outputs[i]);
	}
}

void 
circuit_inner_product(garble_circuit *gc, garble_context *ctxt, 
        uint32_t n, uint32_t num_len, int *inputs, int *outputs)
{
    /* Performs inner product over the well integers (well, mod 2^num_len)
     *
     * Inputs
     * n: total number of input bits
     * num_len: length of each number in bits
     *
     * Variables
     * num_numbers: the number of numbers in the input
     * vector_length: the number of numbers in each vector
     * split: index in inputs where second vector begins
     */

    uint32_t num_numbers = n / num_len; // how many numbers in the inputs total
    uint32_t vector_length = num_numbers / 2; // the size of each input vector
    uint32_t split = n / 2;
    int carry = 0;

    // linear version TODO: make tree version
    int running_sum[num_len];
    int initial_mult_in[2 * num_len];
    memcpy(initial_mult_in, inputs, num_len * sizeof(int)); 
    memcpy(&initial_mult_in[num_len], &inputs[split], num_len * sizeof(int));
    circuit_mult_n(gc, ctxt, 2 * num_len, initial_mult_in, running_sum);

    for (uint32_t i = 1; i < vector_length; ++i) {
        uint32_t idx0 = i * num_len; // index of number from vector 0
        uint32_t idx1 = idx0 + split; // index of number from vector 1

        // multiply them
        int mult_in[2 * num_len], mult_out[num_len];
        memcpy(mult_in, &inputs[idx0], num_len * sizeof(int)); 
        memcpy(&mult_in[num_len], &inputs[idx1], num_len * sizeof(int));
        circuit_mult_n(gc, ctxt, 2 * num_len, mult_in, mult_out);

        // add to running sum
        int add_in[2 * num_len], add_out[num_len];
        memcpy(add_in, mult_out, num_len * sizeof(int));
        memcpy(&add_in[num_len], running_sum, num_len * sizeof(int));

		circuit_add(gc, ctxt, 2 * num_len, add_in, add_out, &carry);
        memcpy(running_sum, add_out, num_len * sizeof(int));
    }
    memcpy(outputs, running_sum, num_len * sizeof(int));
}


void
circuit_ak_mux(garble_circuit *circuit, garble_context *context,
             uint32_t n, int the_switch, int *inputs, int *outputs)
{
    /* Slightly altered (didn't change the logic) version of Arkady's mux 
     * that doesn't appear to be a mux.
     * 
     * n = size of inputs array = number of inputs - 1 (ignores switch)
     */

    int internalWire1, internalWire2;
    assert(n%2 == 0);
    int len = n / 2;

    for (int i = 0; i < len; i++) {
        internalWire1 = builder_next_wire(context);
        internalWire2 = builder_next_wire(context);
        outputs[i] = builder_next_wire(context);

        gate_XOR(circuit, context, inputs[i], inputs[len + i], internalWire1);
        gate_AND(circuit, context, internalWire1, the_switch, internalWire2);                                                                                                                          
        gate_XOR(circuit, context, internalWire2, inputs[len + i], outputs[i]);
    }
}

void                                                                                         
circuit_mult_n(garble_circuit *circuit, garble_context *context, uint32_t n,
		int *inputs, int *outputs) {
	/* Multiplies two n-bit unsigned(?) integers            
	 * If signed, depends on add circuit.                                                    
	 * From AK
	 */
	assert(n/2 % 2 == 0);
	uint32_t len = n / 2;
	int zero_wire = wire_zero(circuit);
	int carry = 0;
	int add_in[2*len], add_out[len];

	// put the second input in the second part of add_in
	memcpy(&add_in[len], &inputs[len], len * sizeof(int)); // shouldn't change in for loop
	add_in[0] = zero_wire; // shouldn't change in for loop

	for (int i = 0; i < len; i++) {
		outputs[i] = zero_wire;
	}                                                             

	for (int i = len - 1; i >= 0; i--) {                                         
		memcpy(&add_in[1], outputs, (len - 1) * sizeof(int));
		circuit_add(circuit, context, 2 * len, add_in, add_out, &carry);

		int mux_in[2 * len];
		// first part of input is the output of the add circuit
		memcpy(mux_in, add_out, len * sizeof(int));

		// second part is the output of the previous mux
		memcpy(&mux_in[len], mux_in, len * sizeof(int)); 

		int the_switch = inputs[len + i];
		circuit_ak_mux(circuit, context, 2 * len, the_switch, mux_in, outputs);
	}
}



void circuit_argmax2(garble_circuit *gc, garble_context *ctxt, 
		int *inputs, int *outputs, int num_len) 
{
	/* num_len is the length of each number.
	 *
	 * Inputs should look like idx0 || num0 || idx1 || num1
	 * where the length of idx0, idx1, num0, and num1 is num_len bits
	 */
	assert(inputs && outputs && gc && ctxt);

	// populate idxs and nums by splicing inputs
	int les_ins[2 * num_len];

	memcpy(les_ins, &inputs[num_len], sizeof(int) * num_len); // put num0
	memcpy(&les_ins[num_len], &inputs[3 * num_len], sizeof(int) * num_len); // put num1
	printf("les_ins: %d %d %d %d\n", les_ins[0], les_ins[1], les_ins[2], les_ins[3]);
	int les_out;


	new_circuit_les(gc, ctxt, num_len * 2, les_ins, &les_out);

	// And mux 'em
	int mux_inputs[2 * num_len];
	memcpy(mux_inputs, inputs, sizeof(int) * num_len);
	memcpy(mux_inputs + num_len, &inputs[2 * num_len], sizeof(int) * num_len);
	bitwiseMUX(gc, ctxt, les_out, inputs, 4 * num_len, outputs);
}

void circuit_argmax4(garble_circuit *gc, garble_context *ctxt,
		int *inputs, int *outputs, int num_len) 
{
	assert(inputs && outputs && gc && ctxt);

	// argmax first two numbers
	int out[4 * num_len]; // will hold output of first two argmaxes
	circuit_argmax2(gc, ctxt, inputs, out, num_len);

	// argmax second two
	circuit_argmax2(gc, ctxt, inputs + (4 * num_len), out + (2 * num_len), num_len);

	// argmax their outputs
	circuit_argmax2(gc, ctxt, out, outputs, num_len);
}

void new_circuit_gteq(garble_circuit *circuit, garble_context *context, int n,
		int *inputs, int *output) 
{
	/* Assumes that lsb is on the left. i.e. 011 >= 101 is true.
	 * courtesy of Arkady
	 */
	int split = n / 2;
	int carryWire = wire_one(circuit);
	int internalWire1, internalWire2, preCarryWire;

	for (int i = 0; i < split; i++) {
		internalWire1 = builder_next_wire(context);
		internalWire2 = builder_next_wire(context);
		preCarryWire = builder_next_wire(context);

		gate_XOR(circuit, context, inputs[i], carryWire, internalWire1);
		gate_XOR(circuit, context, inputs[i + split], carryWire, internalWire2);
		gate_AND(circuit, context, internalWire1, internalWire2, preCarryWire);

		carryWire = builder_next_wire(context);
		gate_XOR(circuit, context, inputs[i], preCarryWire, carryWire);
	}
	*output = carryWire;
}

void new_circuit_les(garble_circuit *circuit, garble_context *context, int n,
		int *inputs, int *output) 
{
	int gteq_out;
	new_circuit_gteq(circuit, context, n, inputs, &gteq_out);

	// faking not gate:
	*output = builder_next_wire(context);
	int fixed_wire_one = wire_one(circuit);
	gate_XOR(circuit, context, gteq_out, fixed_wire_one, *output);
	// gate_NOT(gc, ctxt, theSwitch, notSwitch);

}

	void
new_circuit_mux21(garble_circuit *gc, garble_context *ctxt, 
		int theSwitch, int input0, int input1, int *output)
{
	/* MUX21 circuit. 
	 * It takes three inputs, a switch, and two bits. 
	 * If the switch is 0, then it ouptut input0. 
	 * If the switch is 1, then it outputs input1.
	 */
	uint64_t notSwitch = builder_next_wire(ctxt);
	// faking not gate:
	int fixed_wire_one = wire_one(gc);
	gate_XOR(gc, ctxt, theSwitch, fixed_wire_one, notSwitch);
	// gate_NOT(gc, ctxt, theSwitch, notSwitch);

	int and0 = builder_next_wire(ctxt);
	gate_AND(gc, ctxt, notSwitch, input0, and0);
	int and1 = builder_next_wire(ctxt);
	gate_AND(gc, ctxt, theSwitch, input1, and1);
	*output = builder_next_wire(ctxt);
	gate_OR(gc, ctxt, and0, and1, *output);
}

	int
countToN(int *a, int n)
{
	for (int i = 0; i < n; i++)
		a[i] = i;
	return 0;
}

bool isFinalCircuitType(CircuitType type) 
{
	if (type == AES_FINAL_ROUND) 
		return true;
	return false;
}

void buildLinearCircuit(garble_circuit *gc) 
{
    /* Builds a circuit that performs linear classification.
     * That is, it takes the dot product of x and w, where x is in the iput
     * and w is the model, and outputs 1 if <x,w> > 1 and 0 otherwise.
     */
    // TODO
    buildHyperCircuit(gc);
}



void buildHyperCircuit(garble_circuit *gc) 
{
    int n = 8;
    int num_len = 2;
    int m = 2;
    int input_wires[n];
	int output_wires[m];
	garble_context ctxt;

	countToN(input_wires, n);

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &ctxt);

    circuit_inner_product(gc, &ctxt, n, num_len, input_wires, output_wires);


	builder_finish_building(gc, &ctxt, output_wires);
        

}

void
buildLevenshteinCircuit(garble_circuit *gc, int l, int sigma)
{
	/* The goal of the levenshtein algorithm is
	 * populate the l+1 by l+1 D matrix. 
	 * D[i][j] points to an integer address. 
	 * At that address is an DIntSize-bit number representing the distance between
	 * a[0:i] and b[0:j] (python syntax)
	 *
	 * If l = 2
	 * e.g. D[1][2] = 0x6212e0
	 * *0x6212e0 = {0,1} = the wire indices
	 *
	 * For a concrete understanding of the algorithm implemented, 
	 * see extra_scripts/leven.py
	 */

	/* get number of bits needed to represent our numbers.
	 * The input strings can be at most l distance apart,
	 * since they are of length l, so we need floor(log_2(l)+1) bits
	 * to express the number
	 */
	int DIntSize = (int) floor(log2(l)) + 1;
	int inputsDevotedToD = DIntSize * (l+1);

	int n = inputsDevotedToD + (2 * sigma * l);
	int m = DIntSize;
	int core_n = (3 * DIntSize) + (2 * sigma);
	int inputWires[n];
	countToN(inputWires, n);

	garble_context gctxt;
	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &gctxt);

	///* Populate D's 0th row and column with wire indices from inputs*/
	int D[l+1][l+1][DIntSize];
	for (int i = 0; i < l+1; i++) {
		for (int j = 0; j < l+1; j++) {
			//D[i][j] = allocate_ints(DIntSize);
			if (i == 0) {
				memcpy(D[0][j], inputWires + (j*DIntSize), sizeof(int) * DIntSize);
			} else if (j == 0) {
				memcpy(D[i][0], inputWires + (i*DIntSize), sizeof(int) * DIntSize);
			}
		}
	}

	/* Populate a and b (the input strings) wire indices */
	int a[l * sigma];
	int b[l * sigma];
	memcpy(a, inputWires + inputsDevotedToD, l * sigma * sizeof(int));
	memcpy(b, inputWires + inputsDevotedToD + (l * sigma), l * sigma * sizeof(int));

	/* dynamically add levenshtein core circuits */
	for (int i = 1; i < l + 1; i++) {
		for (int j = 1; j < l + 1; j++) {
			int coreInputWires[core_n];
			int p = 0;
			memcpy(coreInputWires + p, D[i-1][j-1], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, D[i][j-1], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, D[i-1][j], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, &a[(i-1)*sigma], sizeof(int) * sigma);
			p += sigma;
			memcpy(coreInputWires + p, &b[(j-1)*sigma], sizeof(int) * sigma);
			p += sigma;
			assert(p == core_n);


			int coreOutputWires[DIntSize];
			addLevenshteinCoreCircuit(gc, &gctxt, l, sigma, coreInputWires, coreOutputWires);
			/*printf("coreInputWires: (i=%d,j=%d) (%d %d) (%d %d) (%d %d) (%d %d) (%d %d) -> (%d %d)\n",*/
			/*i,*/
			/*j,*/
			/*coreInputWires[0],*/
			/*coreInputWires[1],*/
			/*coreInputWires[2],*/
			/*coreInputWires[3],*/
			/*coreInputWires[4],*/
			/*coreInputWires[5],*/
			/*coreInputWires[6],*/
			/*coreInputWires[7],*/
			/*coreInputWires[8],*/
			/*coreInputWires[9],*/
			/*coreOutputWires[0],*/
			/*coreOutputWires[1]);*/

			// Save coreOutputWires to D[i][j] 
			memcpy(D[i][j], coreOutputWires, sizeof(int) * DIntSize);
		}
	}
	int output_wires[m];
	memcpy(output_wires, D[l][l], sizeof(int) * DIntSize);
	builder_finish_building(gc, &gctxt, output_wires);
}

int 
INCCircuitWithSwitch(garble_circuit *gc, garble_context *ctxt,
		int the_switch, int n, int *inputs, int *outputs) {
	/* n does not include the switch. The size of the number */

	for (int i = 0; i < n; i++) {
		outputs[i] = builder_next_wire(ctxt);
	}
	int carry = builder_next_wire(ctxt);
	gate_XOR(gc, ctxt, the_switch, inputs[0], outputs[0]);
	gate_AND(gc, ctxt, the_switch, inputs[0], carry);

	/* do first case */
	int not_switch = builder_next_wire(ctxt);
	gate_NOT(gc, ctxt, the_switch, not_switch);
	for (int i = 1; i < n; i++) {
		/* cout and(xor(x,c),s) */
		int xor_out = builder_next_wire(ctxt);
		int and0 = builder_next_wire(ctxt);
		int and1 = builder_next_wire(ctxt);
		gate_XOR(gc, ctxt, carry, inputs[i], xor_out);
		gate_AND(gc, ctxt, xor_out, the_switch, and0);

		gate_AND(gc, ctxt, not_switch, inputs[i], and1);
		gate_OR(gc, ctxt, and0, and1, outputs[i]);

		/* carry and(nand(c,x),s)*/
		int and_out = builder_next_wire(ctxt);
		gate_AND(gc, ctxt, carry, inputs[i], and_out);
		carry = builder_next_wire(ctxt);
		gate_AND(gc, ctxt, and_out, the_switch, carry);
	}
	return 0;
}

	static int
TCircuit(garble_circuit *gc, garble_context *gctxt, const int *inp0, const int *inp1, int ninputs)
{
	/* Perfroms "T" which equal 1 if and only if inp0 == inp1 */
	/* returns the output wire */
	assert(ninputs % 2 == 0 && "doesnt support other alphabet sizes.. yet");
	int split = ninputs / 2;
	int xor_output[split];

	for (int i = 0; i < split; i++) {
		xor_output[i] = builder_next_wire(gctxt);
		gate_XOR(gc, gctxt, inp0[i], inp1[i], xor_output[i]);
	}
	int T_output;
	circuit_or(gc, gctxt, split, xor_output, &T_output);
	return T_output;
}

	void
addLevenshteinCoreCircuit(garble_circuit *gc, garble_context *gctxt, 
		int l, int sigma, int *inputWires, int *outputWires) 
{
	int DIntSize = (int) floor(log2(l)) + 1;
	int D_minus_minus[DIntSize]; /*D[i-1][j-1] */
	int D_minus_same[DIntSize]; /* D[1-1][j] */
	int D_same_minus[DIntSize]; /* D[i][j-1] */
	int symbol0[sigma];
	int symbol1[sigma];

	/* arrayPopulateRange is inclusive on start and exclusive on end */
	memcpy(D_minus_minus, inputWires, sizeof(int) * DIntSize);
	memcpy(D_minus_same, inputWires + DIntSize, sizeof(int) * DIntSize);
	memcpy(D_same_minus, inputWires + 2*DIntSize, sizeof(int) * DIntSize);
	memcpy(symbol0, inputWires + (3*DIntSize), sizeof(int) * sigma);
	memcpy(symbol1, inputWires + (3*DIntSize) + sigma, sizeof(int) * sigma);

	/* First MIN circuit :MIN(D_minus_same, D_same_minus) */
	int min_inputs[2 * DIntSize];
	memcpy(min_inputs + 0, D_minus_same, sizeof(int) * DIntSize);
	memcpy(min_inputs + DIntSize, D_same_minus, sizeof(int) * DIntSize);
	int min_outputs[DIntSize]; 
	circuit_min(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs);

	/* Second MIN Circuit: uses input from first min cricuit and D_minus_minus */
	memcpy(min_inputs, min_outputs, sizeof(int) * DIntSize);
	memcpy(min_inputs + DIntSize,  D_minus_minus, sizeof(int) * DIntSize);
	int min_outputs2[DIntSize + 1]; 
	MINCircuitWithLEQOutput(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs2); 

	int T_output = TCircuit(gc, gctxt, symbol0, symbol1, 2*sigma);

	/* 2-1 MUX(switch = determined by secon min, 1, T)*/
	int mux_switch = min_outputs2[DIntSize];

	int fixed_one_wire = wire_one(gc);

	int mux_output;
	circuit_mux21(gc, gctxt, mux_switch, fixed_one_wire, T_output, &mux_output);

	int final[DIntSize];
	INCCircuitWithSwitch(gc, gctxt, mux_output, DIntSize, min_outputs2, final);

	memcpy(outputWires, final, sizeof(int) * DIntSize);
}

int myLEQCircuit(garble_circuit *gc, garble_context *ctxt, int n,
		int *inputs, int *outputs)
{

	int les, eq;
	int ret = builder_next_wire(ctxt);

	circuit_les(gc, ctxt, n, inputs, &les);
	circuit_equ(gc, ctxt, n, inputs, &eq);
	gate_OR(gc, ctxt, les, eq, ret);
	outputs[0] = ret;
	return 0;
}

int MINCircuitWithLEQOutput(garble_circuit *gc, garble_context *garblingContext, int n,
		int* inputs, int* outputs) 
{
	/* Essentially copied from JustGarble/src/circuits.c.
	 * Different from their MIN circuit because it has output size equal to n/2 + 1
	 * where that last bit indicates the output of the LEQ circuit,
	 * e.g. outputs[2/n + 1] = 1 iff input0 <= input1
	 */
	int i;
	int lesOutput;
	int notOutput = builder_next_wire(garblingContext);
	myLEQCircuit(gc, garblingContext, n, inputs, &lesOutput);
	gate_NOT(gc, garblingContext, lesOutput, notOutput);
	int split = n / 2;
	for (i = 0; i < split; i++) {
		circuit_mux21(gc, garblingContext, lesOutput, inputs[i], inputs[split + i], outputs+i);
	}

	outputs[split] = lesOutput;
	return 0;
}

	void
buildANDCircuit(garble_circuit *gc, int n, int nlayers)
{
	garble_context ctxt;
	int wire;
	int wires[n];

	countToN(wires, n);

	garble_new(gc, n, n, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &ctxt);

	for (int i = 0; i < nlayers; ++i) {
		for (int j = 0; j < n; j += 2) {
			wire = builder_next_wire(&ctxt);
			gate_AND(gc, &ctxt, wires[j], wires[j+1], wire);
			wires[j] = wires[j+1] = wire;
		}
	}

	builder_finish_building(gc, &ctxt, wires);
}

void AddAESCircuit(garble_circuit *gc, garble_context *garblingContext, int numAESRounds, 
		int *inputWires, int *outputWires) 
{
	/* Adds AES to the circuit
	 * :param inputWires has size 128 * numAESRounds + 128
	 */

	int addKeyInputs[256];
	int addKeyOutputs[128];
	int subBytesOutputs[128];
	int shiftRowsOutputs[128];
	int mixColumnOutputs[128];

	memcpy(addKeyInputs, inputWires, sizeof(addKeyInputs));

	for (int round = 0; round < numAESRounds; round++) {
		aescircuit_add_round_key(gc, garblingContext, addKeyInputs, addKeyOutputs);

		for (int j = 0; j < 16; j++) {
			aescircuit_sub_bytes(gc, garblingContext, addKeyOutputs + (8*j), subBytesOutputs + (8*j));
		}

		aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

		if (round != numAESRounds-1) { /*if not final round */
			for (int j = 0; j < 4; j++) {
				aescircuit_mix_columns(gc, garblingContext, shiftRowsOutputs + j * 32, mixColumnOutputs + 32 * j);
			}

			memcpy(addKeyInputs, mixColumnOutputs, sizeof(mixColumnOutputs));
			memcpy(addKeyInputs + 128, inputWires + (round + 2) * 128, sizeof(int)*128);
		} 
	}

	for (int i = 0; i < 128; i++) {
		outputWires[i] = shiftRowsOutputs[i];
	}
}

	void 
buildCBCFullCircuit (garble_circuit *gc, int num_message_blocks, int num_aes_rounds, block *delta) 
{
	int n = (num_message_blocks * 128) + (num_message_blocks * num_aes_rounds * 128) + 128;
	int m = num_message_blocks*128;
	// TODO figure out a rough estimate for q based on num_aes_rounds and num_message_blocks 
	int num_evaluator_inputs = 128 * num_message_blocks;
	int num_garbler_inputs = n - num_evaluator_inputs;

	/* garbler_inputs and evaluator_inputs are the wires to 
	 * which the garbler's and evaluators inputs should go */
	int *garbler_inputs = (int*) malloc(sizeof(int) * num_garbler_inputs);
	int *evaluator_inputs = (int*) malloc(sizeof(int) * num_evaluator_inputs);

	countToN(evaluator_inputs, num_evaluator_inputs);
	for (int i = 0; i < num_garbler_inputs ; i++)
		garbler_inputs[i] = i + num_evaluator_inputs;

	int xorIn[256] = {0};
	int xorOut[128] = {0};
	int aesOut[128] = {0};
	int *aesIn = (int*) malloc(sizeof(int) * (128 * (num_aes_rounds + 1)));
	int *outputWires = (int*) malloc(sizeof(int) * m);
	assert(aesIn && outputWires);

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	garble_context gc_context;
	builder_start_building(gc, &gc_context);

	countToN(xorIn, 256);
	circuit_xor(gc, &gc_context, 256, xorIn, xorOut);

	int output_idx = 0;
	int garbler_input_idx = 128;
	int evaluator_input_idx = 128;

	memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
	memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
	garbler_input_idx += 128*num_aes_rounds;

	AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);
	memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
	output_idx += 128;

	for (int i = 0; i < num_message_blocks - 1; i++) {
		/* xor previous round output with new message block */
		memcpy(xorIn, aesOut, sizeof(aesOut));
		memcpy(xorIn + 128, evaluator_inputs + evaluator_input_idx, 128);
		evaluator_input_idx += 128;

		circuit_xor(gc, &gc_context, 256, xorIn, xorOut);

		/* AES the output of the xor, and add in the key */
		memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
		memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
		garbler_input_idx += 128 * num_aes_rounds;

		AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);

		memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
		output_idx += 128;
	}
	assert(garbler_input_idx == num_garbler_inputs);
	assert(evaluator_input_idx == num_evaluator_inputs);
	assert(output_idx == m);
	builder_finish_building(gc, &gc_context, outputWires);
}

	void
buildAdderCircuit(garble_circuit *gc) 
{
	int n = 2; // number of inputs 
	int m = 2; // number of outputs

	int *inputs = (int *) malloc(sizeof(int) * n);
	countToN(inputs, n);
	int* outputs = (int*) malloc(sizeof(int) * m);

	garble_context gc_context;

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &gc_context);
	circuit_add22(gc, &gc_context, inputs, outputs);
	builder_finish_building(gc, &gc_context, outputs);

	free(inputs);
	free(outputs);
}

	void 
buildXORCircuit(garble_circuit *gc, block *delta) 
{
	garble_context garblingContext;
	int n = 256;
	int m = 128;
	int inp[n];
	int outs[m];
	countToN(inp, n);

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &garblingContext);
	circuit_xor(gc, &garblingContext, 256, inp, outs);
	builder_finish_building(gc, &garblingContext, outs);
}

	void
buildAESRoundComponentCircuit(garble_circuit *gc, bool isFinalRound, block* delta) 
{
	garble_context garblingContext;
	int n1 = 128; // size of key
	int n = 256; // tot size of input: prev value is 128 bits + key size 128 bits
	int m = 128;
	int inp[n];
	countToN(inp, n);
	int prevAndKey[n];
	int keyOutputs[n1];
	int subBytesOutputs[n1];
	int shiftRowsOutputs[n1];
	int mixColumnOutputs[n1];

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &garblingContext);
	countToN(prevAndKey, 256); 

	// first 128 bits of prevAndKey are prev value
	// second 128 bits of prevAndKey are the new key
	// addRoundKey xors them together into keyOutputs
	aescircuit_add_round_key(gc, &garblingContext, prevAndKey, keyOutputs);

	for (int i = 0; i < 16; i++) {
		aescircuit_sub_bytes(gc, &garblingContext, keyOutputs + (8*i), subBytesOutputs + (8*i));
	}

	aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

	if (!isFinalRound) { 
		for (int i = 0; i < 4; i++) { 
			// fixed justGarble bug in their construction
			aescircuit_mix_columns(gc, &garblingContext, shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
		}
		// output wires are stored in mixColumnOutputs
		builder_finish_building(gc, &garblingContext, mixColumnOutputs);
	} else {
		builder_finish_building(gc, &garblingContext, shiftRowsOutputs);
	}
}

void buildAESCircuit(garble_circuit *gc)
{
	garble_context garblingContext;

	int roundLimit = 10;
	int n = 128 * (roundLimit + 1);
	int m = 128;
	int* final;
	int inp[n];
	countToN(inp, n);
	int addKeyInputs[n * (roundLimit + 1)];
	int addKeyOutputs[n];
	int subBytesOutputs[n];
	int shiftRowsOutputs[n];
	int mixColumnOutputs[n];
	int i;

	garble_new(gc, n, m, GARBLE_TYPE_STANDARD);
	builder_start_building(gc, &garblingContext);

	countToN(addKeyInputs, 256);

	for (int round = 0; round < roundLimit; round++) {

		aescircuit_add_round_key(gc, &garblingContext, addKeyInputs,
				addKeyOutputs);

		for (i = 0; i < 16; i++) {
			aescircuit_sub_bytes(gc, &garblingContext, addKeyOutputs + 8 * i,
					subBytesOutputs + 8 * i);
		}

		aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

		if (round != roundLimit - 1) {
			for (i = 0; i < 4; i++) {
				aescircuit_mix_columns(gc, &garblingContext,
						shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
			}
		}

		for (i = 0; i < 128; i++) {
			addKeyInputs[i] = mixColumnOutputs[i];
			addKeyInputs[i + 128] = (round + 2) * 128 + i;
		}
	}
	final = shiftRowsOutputs;
	builder_finish_building(gc, &garblingContext, final);
}

