#include "workload.h"
#include <xparameters.h>
#include <stdint.h>
#include <xtime_l.h>
#include <string.h>
#include <math.h>
#include <xil_printf.h>

#include "variant.h"
#include "shared_state.h"

SharedState_Variables s_variables;


#define T0_WORK_DIFFICULTY 8
#define T1_WORK_DIFFICULTY 200
#define T2_WORK_DIFFICULTY 400

void WORKLOAD_Init(void)
{
    memset(&s_variables, 0, sizeof(s_variables));
}

void WORKLOAD_T0(void)
{
    /* This is just random calculations to simulate real workload */
    
    static double s_counter = 1.41;
    
    for (int j = 0; j < T0_WORK_DIFFICULTY; ++j) {
        for (int i = 0; i < SHAREDSTATE_VARIABLE_COUNT; ++i) {
            s_variables.vd[i] += 2.1 * s_variables.vd[i] + s_counter + 231.4;
            s_counter += 0.14;
            
            if (s_counter > 50.0) {
                s_counter = -23.4;
            }
            
            if (s_variables.vd[i] > 10000.0 || s_variables.vd[i] < -10000.0) {
                s_variables.vd[i] = sin(s_variables.vd[i] / 10000.0);
                
                
                s_variables.vs[i] += 1;
            }
            
            s_variables.vb[(s_variables.vs[i] * 123 + 13) % SHAREDSTATE_VARIABLE_COUNT] += 1;
            s_variables.vf[i] += s_variables.vb[(int)(s_counter * s_variables.vf[s_variables.vb[i]]) % SHAREDSTATE_VARIABLE_COUNT] * s_counter;
            
            if (s_variables.vf[i] > 2143515.0f || s_variables.vf[i] < -125412.0f) {
                s_variables.vf[i] = 123.452 * s_counter;
            }
        }
    }
}

void WORKLOAD_T1(void)
{
    static volatile double s_workStuff = 1.0;
    
    double var = s_workStuff;
    for (int i = 0; i < T1_WORK_DIFFICULTY; ++i) {
        var += 123.0;
        

        if (var > 1512436.0) {
            var = 123.0 - i;
        }
        
        var = cos(var) * 0.65 * s_workStuff;
        
        
    }
    
    if (var > -32165132.0 && var < 1234239051597.0) {
        s_workStuff = var;
    }
    else {
        s_workStuff = 634.0;
    }
}

void WORKLOAD_T2(void)
{
    static volatile double s_workStuff = 1.0;
    
    double var = s_workStuff;
    for (int i = 0; i < T2_WORK_DIFFICULTY; ++i) {
        var += 123.0;

        if (var > 1512436.0) {
            var = 123.0 - i;
        }
        
        var = cos(var) * 0.65 * s_workStuff;
        
        
    }
    
    if (var > -32165132.0 && var < 1234239051597.0) {
        s_workStuff = var;
    }
    else {
        s_workStuff = 634.0;
    }
}

void WORKLOAD_BG(void)
{
    
}

