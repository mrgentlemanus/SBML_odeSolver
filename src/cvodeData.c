/*
  Last changed Time-stamp: <2006-08-30 15:13:32 raim>
  $Id: cvodeData.c,v 1.2 2006/08/30 13:36:27 raimc Exp $
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
 *     Rainer Machne
 *
 * Contributor(s):
 *     Andrew M. Finney
 */

/*! \defgroup cvodeData Integration Results Interface:  x(t)
  \ingroup integration
    
  \brief This module contains the functions to create input data
  for formula evaluation and retrieve results from integration   

*/
/*@{*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libSBML header files */
#include <sbml/SBMLTypes.h>
#include <sbml/Model.h>

/* own header files */


#include "sbmlsolver/cvodeData.h"
#include "sbmlsolver/integratorInstance.h"
#include "sbmlsolver/integratorSettings.h"
#include "sbmlsolver/processAST.h"
#include "sbmlsolver/solverError.h"

#include "sbmlsolver/variableIndex.h"



/* private functions */
static void CvodeData_freeStructures(cvodeData_t *);
static cvodeData_t *CvodeData_allocate(int nvalues, int nevents, int neq);
static int CvodeData_allocateSens(cvodeData_t *, int neq, int nsens);

/* static int CvodeData_allocateAdjSens(cvodeData_t *data, int neq); */



/* Internal Integration Data: The functions allocate and free
   cvodeData and cvodeResults required by the CVODE interface functions
   to read values and store results, respectively. */
static cvodeData_t *CvodeData_allocate(int nvalues, int nevents, int neq)
{
  cvodeData_t * data;

  ASSIGN_NEW_MEMORY(data, struct cvodeData, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(data->trigger, nevents, int, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(data->value, nvalues, double, NULL);

  data->neq = neq;
  data->opt = NULL;
  
  data->sensitivity = NULL;
  data->p = NULL;
  

  /* Adjoint specific */
  ASSIGN_NEW_MEMORY_BLOCK(data->adjvalue, nvalues, double, NULL);


  return data ;
}

static int CvodeData_allocateSens(cvodeData_t *data, int neq, int nsens)
{
  int i;
  
  ASSIGN_NEW_MEMORY_BLOCK(data->sensitivity, neq, double *, 0);
  for ( i=0; i<neq; i++ )
    ASSIGN_NEW_MEMORY_BLOCK(data->sensitivity[i], nsens, double, 0);

  data->nsens = nsens;
  data->neq = neq;

  return 1;
}



/* Step I.2: */
/** Creates cvodeData from an odeModel and initial values from the
    original SBML model.

    This function is internally used by integratorInstance creation.
    
    It is available as an API function, so users can create this
    structure from an odeModel to evaluate formulae in odeModel
    independent of integratorInstance.
*/
SBML_ODESOLVER_API cvodeData_t *CvodeData_create(odeModel_t *om)
{
  int neq, nconst, nass, nvalues, nevents;
  cvodeData_t *data;

  neq    = om->neq;
  nconst = om->nconst;
  nass   = om->nass;
  nevents = Model_getNumEvents(om->simple);
  nvalues = neq + nconst + nass;

  /* allocate memory for current integration data storage */
  data = CvodeData_allocate(nvalues, nevents, neq);
  RETURN_ON_FATALS_WITH(NULL);

  data->nvalues = nvalues;

  /* set pointer to input model */
  data->model = om ;

  /* set integration run counter to 0,
     used for multiple reruns of integration  */
  data->run = 0;

  /* Adjoint specific stuff */
  data->adjrun = 0;

  /* initialize values */
  CvodeData_initializeValues(data);
  
  return data;
}


/** Writes values from the input SBML model into
    the data structure */

SBML_ODESOLVER_API void CvodeData_initializeValues(cvodeData_t *data)
{
  int i;
  Parameter_t *p;
  Species_t *s;
  Compartment_t *c;
  odeModel_t *om = data->model;
  Model_t *ode = om->simple;

  /* First, fill cvodeData_t  structure with data from
     the derived SBML model  */

  for ( i=0; i<data->nvalues; i++ )
  {
    if ( (s = Model_getSpeciesById(ode, om->names[i])) )
    {
      if ( Species_isSetInitialConcentration(s) )
	data->value[i] = Species_getInitialConcentration(s);
      else if ( i < om->neq || i >= (om->neq+om->nass) ) 
	SolverError_error(FATAL_ERROR_TYPE,
			  SOLVER_ERROR_REQUESTED_PARAMETER_NOT_FOUND,
			  "No value found for species %s",
			  om->names[i]);
    }
    else if ( (c = Model_getCompartmentById(ode, om->names[i])) )
    {
      if ( Compartment_isSetSize(c) )
	data->value[i] = Compartment_getSize(c);
      else if ( i < om->neq || i >= (om->neq+om->nass) ) 
	SolverError_error(FATAL_ERROR_TYPE,
			  SOLVER_ERROR_REQUESTED_PARAMETER_NOT_FOUND,
			  "No value found for compartment %s",
			  om->names[i]);      
    }
    else if ( (p = Model_getParameterById(ode, om->names[i])) )
    {
      if ( Parameter_isSetValue(p) )
	data->value[i] = Parameter_getValue(p);
      else if ( i < om->neq || i >= (om->neq+om->nass) ) 
	SolverError_error(FATAL_ERROR_TYPE,
			  SOLVER_ERROR_REQUESTED_PARAMETER_NOT_FOUND,
			  "No value found for parameter %s",
			  om->names[i]);      	
    }
  }
  /* initialize assigned parameters */
  for ( i=0; i<om->nass; i++ ) 
    data->value[om->neq+i] = evaluateAST(om->assignment[i],data);

  /* set current time to 0 */
  data->currenttime = 0.0;

  /* Zeroing initial adjoint values */  
  for ( i=0; i<data->neq; i++ ) 
    data->adjvalue[i] = 0.0;

}

/** Frees cvodeData
 */

SBML_ODESOLVER_API void CvodeData_free(cvodeData_t * data)
{
  if(data == NULL)
    return;
  CvodeData_freeStructures(data);
  free(data);
}


/** Returns the number of time points for which results exist
 */

SBML_ODESOLVER_API int CvodeResults_getNout(cvodeResults_t *results)     
{
  return results->nout + 1;
}

/** Returns the time point number n, where 0 <= n < CvodeResults_getNout
 */

SBML_ODESOLVER_API double CvodeResults_getTime(cvodeResults_t *results, int n)
{
  return results->time[n];
}


/** Returns the value of a variable or parameter of the odeModel
    at time step timestep via its variableIndex, where
    0 <= timestep < CvodeResults_getNout,
    and the variableIndex can be retrieved from the input odeModel
*/

SBML_ODESOLVER_API double CvodeResults_getValue(cvodeResults_t *results, variableIndex_t *vi, int timestep)
{
  return results->value[vi->index][timestep];
}


/** Returns the sensitivity of ODE variable y to parameter p
    at timestep nr. `timestep, where 0 <= timestep < CvodeResults_getNout.

    Must not be called, if sensitivity wasn't calculated!
*/

SBML_ODESOLVER_API double CvodeResults_getSensitivityByNum(cvodeResults_t *results,  int value, int parameter, int timestep)
{
  return results->sensitivity[value][parameter][timestep];
  /*!!! will need  adaptation to selected sens.analysis !!!*/
}


/** Returns the sensitivity of ODE variable y to parameter p
    at timestep nr. `timestep, where 0 <= timestep < CvodeResults_getNout.

    Must not be called, if sensitivity wasn't calculated!
*/

SBML_ODESOLVER_API double CvodeResults_getSensitivity(cvodeResults_t *results,  variableIndex_t *y,  variableIndex_t *p, int timestep)
{
  return results->sensitivity[y->index][p->type_index][timestep];
  /*!!! will need  adaptation to selected sens.analysis !!!*/
}


/** Computes the directional sensitivity of ODE variable y to parameter
    direction dp, for all time steps.
    
    Must not be called, if sensitivity wasn't calculated!
*/

SBML_ODESOLVER_API void CvodeResults_computeDirectional(cvodeResults_t *results, double *dp)
{
  int i, j, k;
  for(i=0; i<results->neq; i++)
  {   
    for(j=0; j<results->nout+1; j++)
    {
      results->directional[i][j] = 0;
      for(k=0; k<results->nsens; k++)
	results->directional[i][j] += results->sensitivity[i][k][j] * dp[k];
    } 
  }
}


/** Frees results structure cvodeResults filled by the
    CVODE integrator
*/
SBML_ODESOLVER_API void CvodeResults_free(cvodeResults_t *results) {

  int i, j;
  /* free CVODE results if filled */
  if(results != NULL){
    for( i=0; i<results->nvalues; i++ ) 
      free(results->value[i]);
    free(results->time);
    free(results->value);

    if ( results->sensitivity != NULL )
    {
      for ( i=0; i<results->neq; i++ )
      {
	for ( j=0; j<results->nsens; j++ )
	  free(results->sensitivity[i][j]);
	free(results->sensitivity[i]);
      }
      free(results->sensitivity);
    }

    if ( results->directional != NULL )
    {
      for ( i=0; i<results->neq; i++ )
	free(results->directional[i]);
      free(results->directional);
    }

    /* Adjoint free  */
    if ( results->adjvalue != NULL )
    {
      for( i=0; i<results->neq; i++ )
	free(results->adjvalue[i]);
      free(results->adjvalue);
    }

    free(results);	      
  }
}

/*! @} */

/* initialize cvodeData from cvodeSettings and odeModel (could be
   separated in to functions to further support modularity and
   independence of data structures */
int
CvodeData_initialize(cvodeData_t *data, cvodeSettings_t *opt, odeModel_t *om)
{

  int i, j;
  Event_t *e;
  ASTNode_t *trigger;

  if (data->opt != NULL && data->ode && !data->opt->compileFunctions)
  {
    /* free ODEs */
    for ( i=0; i<data->neq; i++ )
      ASTNode_free(data->ode[i]);
    free(data->ode);
  }

  /* data now also depends on cvodeSettings */
  data->opt = opt;

  /* initialize memory for optimized ODEs */
  if ( !data->opt->compileFunctions )
    ASSIGN_NEW_MEMORY_BLOCK(data->ode, data->neq, ASTNode_t *, 0);
  
  /* initialize values */
  CvodeData_initializeValues(data);
     
  /* set current time */
  data->currenttime = opt->TimePoints[0];

  /* update assigned parameters, in case they depend on new time */
  for ( i=0; i<om->nass; i++ ) 
    data->value[om->neq+i] = evaluateAST(om->assignment[i], data);

  /*
    Then, check if formulas can be evaluated, and cvodeData_t *
    contains all necessary variables:
    evaluateAST(ASTNode_t *f, ,data) will
    ask the user for a value, if a a variable is unknown
  */
  for ( i=0; i<om->neq; i++ ) evaluateAST(om->ode[i], data);

  /* evaluate event triggers and set flags with their initial state */
  for ( i=0; i<Model_getNumEvents(om->simple); i++ )
  {
    e = Model_getEvent(om->simple, i);
    trigger = (ASTNode_t *) Event_getTrigger(e);
    data->trigger[i] = evaluateAST(trigger, data);
  }
    
  /* create structures for sensitivity analysis */
  if ( opt->Sensitivity )
  {
    /* the following can later be called with numbers from
       sensitivity Settings inputs */
    if ( data->sensitivity == NULL )
    {
      CvodeData_allocateSens(data, om->neq, om->nconst);
      RETURN_ON_FATALS_WITH(0);
    }
    /* (re)set to 0.0 initial value */
    for ( i=0; i<om->neq; i++ )
      for ( j=0; j<om->nsens; j++ )
	data->sensitivity[i][j] = 0.0;
  }


  /* Now we should have all variables, and can allocate the
     results structure, where the time series will be stored ...  */

  /* allow results only for finite integrations */
  opt->StoreResults = !opt->Indefinitely && opt->StoreResults;

  /* free former results */
  if ( data->results != NULL )
    CvodeResults_free(data->results);

  /* create new results if required */
  if ( opt->StoreResults )
  {
    data->results = CvodeResults_create(data, opt->PrintStep);
    RETURN_ON_FATALS_WITH(0);


    if  ( opt->Sensitivity )
    {
      CvodeResults_allocateSens(data->results, om->neq, om->nconst,
				opt->PrintStep);
      /* write initial values for sensitivity */
      for ( i=0; i<data->results->neq; i++ )
	for ( j=0; j<data->results->nsens; j++ )
	  data->results->sensitivity[i][j][0] = data->sensitivity[i][j];
    }    
      
    /* Adjoint specific  */
    if  ( opt->AdjointPhase )
    {
      CvodeResults_allocateAdjSens(data->results, om->neq,
				   om->n_adj_sens, opt->PrintStep);
      /* write initial values for adj sensitivity */
      for ( i=0; i<data->results->neq; i++ )
	data->results->adjvalue[i][0] = data->adjvalue[i];
    }


    RETURN_ON_FATALS_WITH(0);
  }

  
  return 1;
}



/* frees all internal stuff of cvodeData */
static void CvodeData_freeStructures(cvodeData_t * data)
{
  int i;

  if ( data == NULL ) return;

  /* free sensitivity structure,
     as data->p is required only by the CVODES solver,
     it is freed by IntegratorInstance_freeForwardSensitivity*/
  
  if ( data->sensitivity != NULL )
  {
    for ( i=0; i<data->neq; i++ )
      free(data->sensitivity[i]);
    free(data->sensitivity);
  }
	   
  /* free adjoint structure*/
  if ( data->adjvalue != NULL ) 
    free(data->adjvalue );


  /* free results structure */
  CvodeResults_free(data->results);

  /* free current values array */
  free(data->value);
  
  /* free event trigger flags */
  free(data->trigger);

  if ( data->opt != NULL )
  {  
    if (!data->opt->compileFunctions)
    {
      /* free ODEs */
      for ( i=0; i<data->neq; i++ )
	ASTNode_free(data->ode[i]);
      free(data->ode);
    }
  }
}



/********* cvodeResults will be created by integration runs *********/

int CvodeResults_allocateSens(cvodeResults_t *results, int neq, int nsens, int nout)
{
  int i, j;

  ASSIGN_NEW_MEMORY_BLOCK(results->sensitivity, neq, double **, 0);
  for ( i=0; i<neq; i++ )
  {
    ASSIGN_NEW_MEMORY_BLOCK(results->sensitivity[i], nsens, double*, 0);
    for ( j=0; j<nsens; j++ )
      ASSIGN_NEW_MEMORY_BLOCK(results->sensitivity[i][j], nout+1, double, 0);
  }
  
  results->nsens = nsens;
  results->neq = neq;    

  ASSIGN_NEW_MEMORY_BLOCK(results->directional, neq, double *, 0);
  for ( i=0; i<neq; i++ )  
    ASSIGN_NEW_MEMORY_BLOCK(results->directional[i], nout+1, double, 0);

  return 1;
}



int CvodeResults_allocateAdjSens(cvodeResults_t *results, int neq, int nadjsens, int nout)
{
  int i;

  ASSIGN_NEW_MEMORY_BLOCK(results->adjvalue, neq, double *, 0);

  for ( i=0; i<neq; i++ ) 
    ASSIGN_NEW_MEMORY_BLOCK(results->adjvalue[i], nout+1, double, 0);

  results->nadjeq = neq;
  results->nadjsens = nadjsens;    

  return 1;
}




/* Creates cvodeResults, the structure that stores
   CVODE integration results */
cvodeResults_t *CvodeResults_create(cvodeData_t * data, int nout) {

  int i;
  cvodeResults_t *results;

  ASSIGN_NEW_MEMORY(results, struct cvodeResults, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(results->time, nout+1, double, NULL);
  
  /* The 2-D array `value' contains the time courses, that are
     calculated by ODEs (SBML species, or compartments and parameters
     defined by rate rules.  */
  ASSIGN_NEW_MEMORY_BLOCK(results->value, data->nvalues, double *, NULL);

  results->nvalues = data->nvalues;    

  for ( i=0; i<data->nvalues; i++ )
    ASSIGN_NEW_MEMORY_BLOCK(results->value[i], nout+1, double, NULL);

  results->sensitivity = NULL;

  results->directional = NULL;

  /* adjoint */
  results->adjvalue = NULL;  

  return results;  
}

/* End of file */