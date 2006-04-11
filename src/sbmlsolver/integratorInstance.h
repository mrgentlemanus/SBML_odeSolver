/*
  Last changed Time-stamp: <2006-02-23 17:19:53 raim>
  $Id: integratorInstance.h,v 1.27 2006/04/11 13:10:45 afinney Exp $ 
*/
/* 
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY, WITHOUT EVEN THE IMPLIED WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. The software and
 * documentation provided hereunder is on an "as is" basis, and the
 * authors have no obligations to provide maintenance, support,
 * updates, enhancements or modifications.  In no event shall the
 * authors be liable to any party for direct, indirect, special,
 * incidental or consequential damages, including lost profits, arising
 * out of the use of this software and its documentation, even if the
 * authors have been advised of the possibility of such damage.  See
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * The original code contained here was initially developed by:
 *
 *     Andrew Finney
 *
 * Contributor(s):
 *     Rainer Machne
 */

#ifndef _INTEGRATORINSTANCE_H_
#define _INTEGRATORINSTANCE_H_

#include <time.h>
/* Header Files for CVODE */
#include "nvector_serial.h"

#include "sbmlsolver/exportdefs.h"
#include "sbmlsolver/integratorSettings.h"
#include "sbmlsolver/cvodedatatype.h"
#include "sbmlsolver/odeModel.h"
#include "sbmlsolver/cvodedata.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct cvodeSolver cvodeSolver_t;
  typedef struct integratorInstance integratorInstance_t ;

  /** Solver State Information */
  struct cvodeSolver
  {
    double t, tout, t0;
    int iout, nout;  /**< above data are required by all solvers */    
    realtype reltol, atol1;
    N_Vector y, abstol, senstol;
    void *cvode_mem; /**< above data are used by the CVode Solver */    
    int nsens;
    N_Vector *yS;    /**< sensitivities vector specific */    
    N_Vector dy;     /**< IDA specific data: current ODE values dx/dt */

    N_Vector q; /**< forward sensitivity quadratures of integral functional */ 

    /** adjoint specific */
    void *cvadj_mem;
    N_Vector yA;    
    realtype reltolA, reltolQA;
    N_Vector abstolA, abstolQA; 
    N_Vector qA;

  };


  /** the main structure for numerical integration */
  struct integratorInstance
  {
    /** implies that the 'data' field state is consistant with the 'solver' field */
    int isValid; 

    /** the ODE Model as passed for construction of cvodeData and
	cvodeSolver */
    odeModel_t *om;
    /** the integrator settings as passed for construction
	of cvodeData and cvodeSolver  */
    cvodeSettings_t *opt;
    /** contains current values,
	created with integratorInstance from odeModel and cvodeSettings */
    cvodeData_t *data;
    /** solver structure (CVODES or IDA or other future solvers) */
    cvodeSolver_t *solver;
    /** optional results structure, shared with cvodeData */
    cvodeResults_t *results; 

    /** start time for integration (doesn't include initial solver setup and compilation) */
    clock_t startTime;

    /** indicates whether startTime has a valid value */
    int clockStarted;

    /** indicates that events should be processed at the end of this time step */
    int processEvents;
  };
  
  /* common to all solvers */
  SBML_ODESOLVER_API integratorInstance_t *IntegratorInstance_create(odeModel_t *, cvodeSettings_t *);
  SBML_ODESOLVER_API int IntegratorInstance_set(integratorInstance_t *, cvodeSettings_t *);
  SBML_ODESOLVER_API int IntegratorInstance_reset(integratorInstance_t *);
  SBML_ODESOLVER_API cvodeSettings_t *IntegratorInstance_getSettings(integratorInstance_t *);
  SBML_ODESOLVER_API void IntegratorInstance_copyVariableState(integratorInstance_t *target, integratorInstance_t *source);
  SBML_ODESOLVER_API double IntegratorInstance_getTime(integratorInstance_t *);
  SBML_ODESOLVER_API double IntegratorInstance_getVariableValue(integratorInstance_t *, variableIndex_t *);
  SBML_ODESOLVER_API double IntegratorInstance_getSensitivity(integratorInstance_t *, variableIndex_t *y,  variableIndex_t *p);
  SBML_ODESOLVER_API int IntegratorInstance_setNextTimeStep(integratorInstance_t *, double);
  SBML_ODESOLVER_API void IntegratorInstance_dumpNames(integratorInstance_t *);
  SBML_ODESOLVER_API void IntegratorInstance_dumpData(integratorInstance_t *);

  SBML_ODESOLVER_API void IntegratorInstance_dumpAdjData(integratorInstance_t *);


  SBML_ODESOLVER_API void IntegratorInstance_dumpYSensitivities(integratorInstance_t *, variableIndex_t *);
  SBML_ODESOLVER_API void IntegratorInstance_dumpPSensitivities(integratorInstance_t *, variableIndex_t *);
  SBML_ODESOLVER_API cvodeData_t *IntegratorInstance_getData(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_integrate(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_checkTrigger(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_checkSteadyState(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_timeCourseCompleted(integratorInstance_t *);
  SBML_ODESOLVER_API cvodeResults_t *IntegratorInstance_createResults(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_updateModel(integratorInstance_t*);
  SBML_ODESOLVER_API int IntegratorInstance_simpleOneStep(integratorInstance_t *);
  SBML_ODESOLVER_API double IntegratorInstance_getIntegrationTime(integratorInstance_t *);
  SBML_ODESOLVER_API double *IntegratorInstance_getValues(integratorInstance_t *);
  /* these functions contain solver specific switches and need to be adapted
     for any new solver, and so does the local
     integratorInstance_initialiyeSolverStructures */    
  SBML_ODESOLVER_API void IntegratorInstance_setVariableValue(integratorInstance_t *, variableIndex_t *, double);
  SBML_ODESOLVER_API int IntegratorInstance_integrateOneStep(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_integrateOneStepWithoutEventProcessing(integratorInstance_t *);
  SBML_ODESOLVER_API void IntegratorInstance_dumpSolver(integratorInstance_t *);
  SBML_ODESOLVER_API void IntegratorInstance_free(integratorInstance_t *);
  SBML_ODESOLVER_API int IntegratorInstance_handleError(integratorInstance_t *);
  SBML_ODESOLVER_API void IntegratorInstance_printStatistics(integratorInstance_t *, FILE *f);
  
  
#ifdef __cplusplus
}
#endif

/* default function for data update, event and steady state handling,
   result storage and loop variables; to be used by solver
   specific ...OneStep functions */
int IntegratorInstance_updateData(integratorInstance_t *);

/* default function for adjoint data update, event and steady state handling,
   result storage and loop variables; to be used by solver
   specific ...OneStep functions */
int IntegratorInstance_updateAdjData(integratorInstance_t *);

/* internal function used for optimization of ODEs; will handle the
   case of sensitivity analysis, where ODEs can not be optimized; */
/*!!! will need adaptation to selected sens.analysis !!!*/
void IntegratorInstance_optimizeOdes(integratorInstance_t *);

#endif
