#ifndef MPC_EVALUATOR_H
#define MPC_EVALUATOR_H

#include "2pc_function_spec.h"

void
evaluator_classic_2pc(int *input, int *output,
                      int num_garb_inputs, int num_eval_inputs,
                      unsigned long *tot_time);

void
evaluator_offline(char *dir, int num_eval_inputs, int nchains);

void
evaluator_online(char *dir, int *eval_inputs, int num_eval_inputs,
                 int num_chained_gcs, unsigned long *tot_time);

#endif
