/*
  Last changed Time-stamp: <2007-05-16 19:34:20 raim>
  $Id: odeModel.c,v 1.78 2007/05/16 17:36:59 raimc Exp $ 
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

/*! \defgroup odeModel ODE Model: f(x,p,t) = dx/dt
  \ingroup symbolic
  \brief This module contains all functions to create and interface
  the internal ODE Model it's Jacobian matrix and other derivatives
    
  The internal ODE Model (structure odeModel) can be interfaced for
  analytical purposes. All formulae can be retrieved as libSBML
  Abstract Syntax Trees (AST).
*/
/*@{*/


/* System specific definitions,
   created by configure script */
#ifndef WIN32
#include "config.h"
#endif

#include <string.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include "sbmlsolver/sbml.h"
#include "sbmlsolver/odeConstruct.h"
#include "sbmlsolver/processAST.h"
#include "sbmlsolver/solverError.h"
#include "sbmlsolver/modelSimplify.h"
#include "sbmlsolver/odeModel.h"
#include "sbmlsolver/variableIndex.h"
#include "sbmlsolver/compiler.h"

#define COMPILED_RHS_FUNCTION_NAME "ode_f"
#define COMPILED_JACOBIAN_FUNCTION_NAME "jacobi_f"
#define COMPILED_ADJOINT_JACOBIAN_FUNCTION_NAME "adj_jacobi_f"
#define COMPILED_EVENT_FUNCTION_NAME "event_f"
#define COMPILED_SENSITIVITY_FUNCTION_NAME "sense_f"

static odeModel_t *ODEModel_fillStructures(Model_t *ode);
static odeModel_t *ODEModel_allocate(int neq, int nconst,
				     int nass, int nalg, int nevents);
static void ODEModel_computeAssignmentRuleSets(odeModel_t *om);

typedef struct assignmentStage assignmentStage_t ;

/** represents a step in a function which contains assignments */
struct assignmentStage
{
  List_t *changedSymbols ; /**< set of symbols changed by operation */
  int *assignmentsBeforeChange ; /**< assignments made before
				    operation, this is a boolean array
				    corresponding to the odeMode_t
				    assignment field */
  int timeChanged ; /**< the operation has changed the value of time symbol */
}  ;


/** \brief Create internal model odeModel from a reaction network,
    represented as libSBML's Model_t structure.

    The input model must be of SBML level2!
    The function at first, attempts to construct a simplified SBML
    model, that contains all compartments, species, parameters, events
    and rules of the input model, and constructs new ODEs as SBML
    RateRules from the reaction network of the input model.  The
    function then creates the structure odeModel which contains
    variable, parameter and constant names and all formulas (ODEs and
    assignments) as indexed AST (iAST). This structure can be used to
    initialize and run several integration runs, each associated with
    initial conditions in cvodeData_t. Alternatively I.1a - I.1c
    allow to construct odeModel from higher-level data (a file, an
    SBML document or a reaction network model, respectively).
*/
SBML_ODESOLVER_API odeModel_t *ODEModel_create(Model_t *m)
{
  return ODEModel_createWithObservables(m, NULL);
}

/** \brief Create internal model odeModel from a reaction network,
    represented as libSBML's Model_t structure.

    'observables' is the set of symbols that the user wishes to see
    valid at all times.  A reduced set of observables may result in
    more optimal excution.

    The input model must be of SBML level2!
    The function at first, attempts to construct a simplified SBML
    model, that contains all compartments, species, parameters, events
    and rules of the input model, and constructs new ODEs as SBML
    RateRules from the reaction network of the input model.  The
    function then creates the structure odeModel which contains
    variable, parameter and constant names and all formulas (ODEs and
    assignments) as indexed AST (iAST). This structure can be used to
    initialize and run several integration runs, each associated with
    initial conditions in cvodeData_t. Alternatively I.1a - I.1c
    allow to construct odeModel from higher-level data (a file, an
    SBML document or a reaction network model, respectively).

    observables is a null terminated set of null strings.  This set 
    contains the identiers of all variables that the user wishes to
    observe on the output of the simulator.  If observables is NULL
    the set of species is used instead.
*/

SBML_ODESOLVER_API odeModel_t *ODEModel_createWithObservables(Model_t *m, char **observables)
{
  int i, j;
  Model_t *ode;
  odeModel_t *om;
  
  ode = Model_reduceToOdes(m);
  RETURN_ON_ERRORS_WITH(NULL);

  om = ODEModel_fillStructures(ode);
  /* Errors will cause the program to stop, e.g. when some
     mathematical expressions are missing.  */
  RETURN_ON_ERRORS_WITH(NULL);
  
  om->m = m;
  /*!!! the values array is only used to store initial conditions in
    SBML independent use */
  free(om->values); 
  om->values = NULL;
  om->d = NULL;      /* will be set if created from file */

  om->observables = List_create();

  if ( observables )
  {
    /* make copy */
        
    for ( i = 0; observables[i] != NULL; i++ )
    {
      char *newObservable;

      ASSIGN_NEW_MEMORY_BLOCK(newObservable,
			      strlen(observables[i]) + 1, char, NULL);
      strcpy(newObservable, observables[i]);
      List_add(om->observables, newObservable);
    }
  }
  else
  {
    /* create default list of observables from species */
    /* AMF => if you want to expand the default observables set please
       let me know.  If you include the reaction symbols performance will
       be degraded in models with events */

    for ( i = 0; i != Model_getNumSpecies(m); i++ )
    {
      char *newObservable ;
      Species_t *species = Model_getSpecies(m, i);

      ASSIGN_NEW_MEMORY_BLOCK(newObservable,
			      strlen(Species_getId(species)) + 1,
			      char, NULL);
      strcpy(newObservable, Species_getId(species));
      List_add(om->observables, newObservable);
    }
  }

  for ( i = 0; i != om->neq + om->nass + om->nconst + om->nalg; i++ )
  {        
    om->observablesArray[i] = 0;

    for ( j = 0; j != List_size(om->observables); j++ )
      if ( !strcmp(om->names[i], List_get(om->observables, j)) )
	om->observablesArray[i] = 1;

  }

  ODEModel_computeAssignmentRuleSets(om);
    
  return om;
}

/* creates an assignment stage structure given a set of symbols
   changed by an operation and and a set of assignments made before
   the operation.
   'assignmentsBeforeChange' is a boolean array corresponding
   to the odeMode_t assignment field */ 
assignmentStage_t *AssignmentStage_create(List_t *changedSymbols,
					  int *assignmentsBeforeChange,
					  int timeChanged)
{
  assignmentStage_t *result ;

  ASSIGN_NEW_MEMORY_BLOCK(result, 1, assignmentStage_t, NULL);

  result->changedSymbols = changedSymbols ;
  result->assignmentsBeforeChange = assignmentsBeforeChange;
  result->timeChanged = timeChanged;

  return result;
}

/* adds the contents of 'source' to the end of 'target'.
   List items are only shallow copied. */
void List_append(List_t *target, List_t *source)
{
  int i;

  for (i = 0; i != List_size(source); i++)
    List_add(target, List_get(source, i));
}

/* returns boolean result: whether the given AST is dependant on a given
   set of variables.
*/
int ODEModel_ruleIsDependantOnChangedSymbols(odeModel_t *om,
					     ASTNode_t *rule,
					     List_t *changedSet,
					     int timeChanged)
{
  int i, j;
  List_t *symbols = List_create();
    
  if ( timeChanged && ASTNode_containsTime(rule) )
    return 1;

  ASTNode_getSymbols(rule, symbols);

  for ( j = 0; j != List_size(symbols); j++ )
  {
    char *symbol = List_get(symbols, j);

    for ( i = 0; i != List_size(changedSet); i++ )
    {
      if ( !strcmp(symbol, List_get(changedSet, i)) )
      {
	List_free(symbols);
	return 1;
      }
    }

    for ( i = 0; i != om->nass; i++ )
    {
      if ( !strcmp(symbol, om->names[om->neq + i]) &&
	   ODEModel_ruleIsDependantOnChangedSymbols(om, om->assignment[i],
						    changedSet,
						    timeChanged) )
      {
	List_free(symbols);
	return 1;
      }
    }
  }

  List_free(symbols);

  return 0;
}

/* adds assignment rules from the given model to the set
   'requiredRules' that are not in 'computedRules' that are required
   to compute the given 'targetSymbol' given the set of
   'changedSymbols'.  'requiredRules' and 'computedRules' are boolean
   arrays corresponding to the odeMode_t assignment field */
void ODEModel_computeAssignmentRuleSetForSymbol(odeModel_t *om,
						char *targetSymbol,
						List_t *changedSymbols,
						int *requiredRules,
						int *computedRules,
						int timeChanged)
{
  int i;
     
  for ( i = 0; i != List_size(changedSymbols); i++ )
    if ( !strcmp(targetSymbol, List_get(changedSymbols, i)) )
      return ;

  for ( i = 0; i != om->nass; i++ )
  {
    if ( !computedRules[i] && !requiredRules[i] &&
	 !strcmp(targetSymbol, om->names[om->neq + i]) &&
	 ODEModel_ruleIsDependantOnChangedSymbols(om, om->assignment[i],
						  changedSymbols,
						  timeChanged) )
    {
      int j;
      List_t *symbols = List_create();
            
      ASTNode_getSymbols(om->assignment[i], symbols);
      requiredRules[i] = 1 ;

      for ( j = 0; j != List_size(symbols); j++ )
	ODEModel_computeAssignmentRuleSetForSymbol(om,
						   (char *)
						   List_get(symbols, j),
						   changedSymbols,
						   requiredRules,
						   computedRules,
						   timeChanged);

      List_free(symbols);
    }
  }
}

/* returns a boolean array which is the set of assignment rules
   required to compute the set 'targetSymbols' and the set of
   'changes'.
   'targetSymbols' is a list of char *. 'changes' is the
   is a list of assignmentStage_t * and assumed to be in reverse order
   of operation execution */
int *ODEModel_computeAssignmentRuleSet(odeModel_t *om,
				       List_t *targetSymbols,
				       List_t *changes)
{
  int *requiredRules, *computedRules ;
  int i, timeChanged = 0 ;
  List_t *changeSet = List_create();

  ASSIGN_NEW_MEMORY_BLOCK(requiredRules, om->nass, int, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(computedRules, om->nass, int, NULL);

  for ( i = 0; i != om->nass; i++ )
  {
    requiredRules[i] = 0 ;
    computedRules[i] = 0 ;
  }

  /* determine the set of rules that have computed correct values despite
     changes */
  for ( i = 0; i != List_size(changes); i++ )
  {
    assignmentStage_t *stage = List_get(changes, i);

    List_append(changeSet, stage->changedSymbols);   
    timeChanged = timeChanged || stage->timeChanged ;

    if ( stage->assignmentsBeforeChange )
    {
      int j;

      for ( j = 0; j != om->nass; j++ )
      {
	if ( !computedRules[j] &&
	     stage->assignmentsBeforeChange[j] &&
	     !ODEModel_ruleIsDependantOnChangedSymbols(om,
						       om->assignment[j],
						       changeSet,
						       timeChanged))
	  computedRules[j] = 1;
      }
    }
  }

  /* determine the set of rules that have to be computed given the changes */
  for ( i = 0; i != List_size(targetSymbols); i++ )
    ODEModel_computeAssignmentRuleSetForSymbol(om, List_get(targetSymbols, i),
					       changeSet, requiredRules,
					       computedRules, timeChanged);

  free(computedRules);
  List_free(changeSet);

  return requiredRules;
}

/* computes the values for the 'assignmentsBeforeODEs',
   'assignmentsBeforeEvents', 'assignmentsAfterEvents'
   fields on the given 'odeModel_t' structure */
void ODEModel_computeAssignmentRuleSets(odeModel_t *om)
{
  int i ;
  assignmentStage_t *firstAssignmentStage, *secondAssignmentStage;
  List_t *changes, *allVariables, *odeFunctionOfSet;
  List_t *eventExpressionFunctionOfSet;
  List_t *variablesAssignedByEvents ;
    
  changes = List_create();
  odeFunctionOfSet = List_create();
  eventExpressionFunctionOfSet = List_create();
  variablesAssignedByEvents = List_create();
  allVariables = List_create();

  for ( i = 0; i != om->neq; i++ )
  {
    ASTNode_getSymbols(om->ode[i], odeFunctionOfSet);
    List_add(allVariables, om->names[i]);
  }

  if ( om->simple != NULL )
  {
    for ( i = 0; i != Model_getNumEvents(om->simple); i++ )
    {
      int j ;
      Event_t *event = Model_getEvent(om->simple, i);

      ASTNode_getSymbols((ASTNode_t *)Event_getTrigger(event),
			 eventExpressionFunctionOfSet);
      
      for ( j = 0; j != Event_getNumEventAssignments(event); j++ ) 
      {
	EventAssignment_t *assignment = Event_getEventAssignment(event, j);

	ASTNode_getSymbols((ASTNode_t *)EventAssignment_getMath(assignment),
			   eventExpressionFunctionOfSet);
	List_add(variablesAssignedByEvents,
		 (ASTNode_t *) EventAssignment_getVariable(assignment));
      }
    }
  }

  List_append(allVariables, variablesAssignedByEvents);

  /* first assignment stage represents the state before the start of
     the RHS function and and the start of the event function
     in both cases all variables may have changed (including time)
     but no assignments have been made */ 
  firstAssignmentStage = AssignmentStage_create(allVariables,
						NULL,
						1 /* time has changed */);
  List_prepend(changes, firstAssignmentStage);

  /* compute set of rules required at the start of ODE RHS that is the
     set required to compute the variables that are used by the ODEs
     (the set of variables that the ODEs are a function of) */
  om->assignmentsBeforeODEs =
    ODEModel_computeAssignmentRuleSet(om, odeFunctionOfSet, changes);

  /* Note that the RHS doesn't need to compute the observables that's
     left to the event function

     compute set of rules required at the start of event function that
     is the set required to
     compute the variables that are used by all the event expressions
     (triggers and rhs of event assignments) */
  om->assignmentsBeforeEvents =
    ODEModel_computeAssignmentRuleSet(om, eventExpressionFunctionOfSet,
				      changes);

  /* compute set of rules required by the observables 

  this is computed taking into account the rules already computed
  at the beginning of the event function
  only observables that have not already been computed
  at the beginning of the event function
  or are dependant on a event assignment are computed at this point.
  obervables that aren't dependant on variables are not computed at all
  here */
  secondAssignmentStage =
    AssignmentStage_create(variablesAssignedByEvents,
			   om->assignmentsBeforeEvents,
			   0 /* time has not changed */);
  List_prepend(changes, secondAssignmentStage);
  om->assignmentsAfterEvents =
    ODEModel_computeAssignmentRuleSet(om, om->observables, changes);
    
  free(firstAssignmentStage);
  free(secondAssignmentStage);
  List_free(changes);
  List_free(allVariables);
  List_free(odeFunctionOfSet);
  List_free(eventExpressionFunctionOfSet);
  List_free(variablesAssignedByEvents);
}

/* allocates memory for substructures of a new odeModel, writes
   variable and parameter names and returns a pointer to the
   the newly created odeModel. */
static odeModel_t *ODEModel_fillStructures(Model_t *ode)
{
  int i, j, found, neq, nalg, nconst, nass, nevents, nvalues;
  Compartment_t *c;
  Parameter_t *p;
  Species_t *s;
  Rule_t *rl;
  AssignmentRule_t *ar;
  AlgebraicRule_t *alr;
  RateRule_t *rr;
  SBMLTypeCode_t type;  
  ASTNode_t *math;  
  odeModel_t *om;

  neq     = 0;
  nalg    = 0;
  nconst  = 0;
  nass    = 0;
  nvalues = 0;
  found   = 0;

  /*
    counting number of equations (ODEs/rateRules and assignment Rules)
    to initialize cvodeData structure. Any other occuring values are
    stored as parameters.
  */

  for ( j=0; j<Model_getNumRules(ode); j++ )
  {
    rl = Model_getRule(ode,j);
    type = SBase_getTypeCode((SBase_t *)rl);    
    if ( type == SBML_RATE_RULE ) neq++;
    if ( type == SBML_ALGEBRAIC_RULE ) nalg++;
    if ( type == SBML_ASSIGNMENT_RULE ) nass++;
  }
   
  nvalues = Model_getNumCompartments(ode) + Model_getNumSpecies(ode) +
    Model_getNumParameters(ode);

  nconst = nvalues - nass - neq - nalg;

  nevents = Model_getNumEvents(ode);
  
  om = ODEModel_allocate(neq, nconst, nass, nalg, nevents);
  RETURN_ON_FATALS_WITH(NULL);


  /* 
     filling the Ids of all rate rules (ODEs) and assignment rules
     the ODE model
  */

  neq  = 0;
  nass = 0;
  nalg = 0;
  
  for ( j=0; j<Model_getNumRules(ode); j++ )
  {
    rl = Model_getRule(ode,j);
    type = SBase_getTypeCode((SBase_t *)rl);

    if ( type == SBML_RATE_RULE )
    {
      rr = (RateRule_t *)rl;
      ASSIGN_NEW_MEMORY_BLOCK(om->names[neq],
			      strlen(RateRule_getVariable(rr))+1,
			      char, NULL);
      sprintf(om->names[neq],RateRule_getVariable(rr));
      neq++;
    }
    else if ( type == SBML_ASSIGNMENT_RULE )
    {
      
      ar = (AssignmentRule_t *)rl;
      ASSIGN_NEW_MEMORY_BLOCK(om->names[om->neq+nass],
			      strlen(AssignmentRule_getVariable(ar))+1,
			      char, NULL);
      sprintf(om->names[om->neq + nass], AssignmentRule_getVariable(ar));
      nass++;      
    }
    else if ( type == SBML_ALGEBRAIC_RULE )
    {
      
      alr = (AlgebraicRule_t *)rl;
      /* find variables defined by algebraic rules here! */
      ASSIGN_NEW_MEMORY_BLOCK(om->names[nvalues + nalg],
			      strlen("tmp")+3,
			      char, NULL);
      sprintf(om->names[om->neq+om->nass+om->nconst+ nalg], "tmp%d", nalg);
      /* printf("tmp%d \n", nalg); */
      nalg++;
    }
  }

  

  /* filling constants, i.e. all values in the model, that are not
     defined by an assignment or rate rule */
  
  nconst = 0;
  for ( i=0; i<Model_getNumCompartments(ode); i++ )
  {
    found = 0;
    c = Model_getCompartment(ode, i);
    
    for ( j=0; j<neq+nass; j++ ) 
      if ( strcmp(Compartment_getId(c), om->names[j]) == 0 ) 
	found ++;
    
    if ( !found )
    {
      ASSIGN_NEW_MEMORY_BLOCK(om->names[neq+nass+nconst],
			      strlen(Compartment_getId(c))+1, char, NULL);
      sprintf(om->names[neq+nass+nconst], Compartment_getId(c));
      nconst++;      
    }
  }  
  for ( i=0; i<Model_getNumSpecies(ode); i++ )
  {
    found = 0;
    s = Model_getSpecies(ode, i);
    
    for ( j=0; j<neq+nass; j++ ) 
      if ( strcmp(Species_getId(s), om->names[j]) == 0 ) 
	found ++;

    if ( !found )
    {
      ASSIGN_NEW_MEMORY_BLOCK(om->names[neq+nass+nconst],
			      strlen(Species_getId(s))+1, char, NULL);
      sprintf(om->names[neq+nass+nconst], Species_getId(s));
      nconst++;      
    }
  }
  for ( i=0; i<Model_getNumParameters(ode); i++ )
  {
    found = 0;
    p = Model_getParameter(ode, i);
    for ( j=0; j<neq+nass; j++ ) 
      if ( strcmp(Parameter_getId(p), om->names[j]) == 0 ) 
	found ++;

    if ( !found )
    {
      ASSIGN_NEW_MEMORY_BLOCK(om->names[neq+nass+nconst],
			      strlen(Parameter_getId(p))+1, char, NULL);
      sprintf(om->names[neq+nass+nconst], Parameter_getId(p));
      nconst++;      
    }
  }

  /* Writing and Indexing Formulas: Indexing rate rules and assignment
     rules, using the string array created above and writing the
     indexed formulas to the CvodeData structure. These AST are used
     for evaluation during the integration routines!! */
  neq = 0;
  nass = 0;
  nalg = 0;
  
  /*   ODEModel_dumpNames(om); */
  /*   printf("\n\nHallo %d %d %d %d\n\n\n",
       om->neq,om->nass,om->nalg,om->nconst); */
  /*   fflush(stdout); */

  for ( j=0; j<Model_getNumRules(ode); j++ )
  {
    rl = Model_getRule(ode, j);
    type = SBase_getTypeCode((SBase_t *)rl);
  
    if ( type == SBML_RATE_RULE )
    {
      rr = (RateRule_t *)rl;
      math = indexAST(Rule_getMath(rl), nvalues, om->names);
      /*AST_dump("assigning om->ode", math);*/
      om->ode[neq] = math; 
      neq++;      
    }
    else if ( type == SBML_ASSIGNMENT_RULE )
    {
      ar = (AssignmentRule_t *)rl;
      math = indexAST(Rule_getMath(rl), nvalues, om->names); 
      om->assignment[nass] = math;
      nass++;      
    }
    else if ( type == SBML_ALGEBRAIC_RULE )
    {
      alr = (AlgebraicRule_t *)rl;
      math = indexAST(Rule_getMath(rl), nvalues, om->names); 
      om->algebraic[nalg] = math;
      nalg++;
    }
  }  
  
  om->simple = ode;
  /* set jacobian to NULL */
  om->jacob = NULL;

  return om;
}

/* allocates memory for a new odeModel structure and returns
   a pointer to it */ 
static odeModel_t *ODEModel_allocate(int neq, int nconst,
				     int nass, int nalg, int nevents)
{
  odeModel_t *om;
  int nvalues;

  nvalues = neq + nalg + nass + nconst;

  /* names */
  ASSIGN_NEW_MEMORY(om, odeModel_t, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(om->names, nvalues, char *, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(om->observablesArray, nvalues, int, NULL);

  /* values */
  /*!!! om->values currently only required for SBML independent input
    but should be used generally for clarity? */
  ASSIGN_NEW_MEMORY_BLOCK(om->values, nvalues, double, NULL);

  /* equations */
  ASSIGN_NEW_MEMORY_BLOCK(om->ode, neq, ASTNode_t *, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(om->assignment, nass, ASTNode_t *, NULL);
  ASSIGN_NEW_MEMORY_BLOCK(om->algebraic, nalg, ASTNode_t *, NULL);

  om->neq    = neq;
  om->nconst = nconst;
  om->nass   = nass;
  om->nalg   = nalg; /* this causes crash at the moment, because
		      ODEs have been constructed for that
		      should be defined by alg. rules */

  /* set compiled function pointers to NULL */
  om->compiledCVODEFunctionCode = NULL;
  om->compiledCVODEJacobianFunction = NULL;
  om->compiledCVODERhsFunction = NULL;
  om->compiledCVODESenseFunction = NULL;
  
  /* sensitivity structures are filled later */
  om->index_sens = NULL;
  om->index_sensP = NULL;
  om->nsens = 0;      
 
 
  om->vector_v = NULL;
  om->ObjectiveFunction = NULL;

  return om ;
}


/** \brief Create internal model odeModel from an SBML file, that
    contains level 1 or level 2 SBML.

    Conversion of level 1 to level 2 models is done internally.
*/

SBML_ODESOLVER_API odeModel_t *ODEModel_createFromFile(const char *sbmlFileName)
{
  return ODEModel_createFromFileWithObservables(sbmlFileName, NULL);
}

/** \brief Create internal model odeModel from an SBML file, that
    contains level 1 or level 2 SBML.

    'observables' is the set of symbols that the user wishes to see
    valid at all times.  A reduced set of observables may result in
    more optimal excution.
*/
SBML_ODESOLVER_API odeModel_t *ODEModel_createFromFileWithObservables(const char *sbmlFileName, char **observables)
{
  SBMLDocument_t *d;
  odeModel_t *om;

  d =  parseModel((char *)sbmlFileName,
		  0 /* print message */,
		  0 /* don't validate */,
		  0, 0, 0, 0 /* empty validation parameters */);
    
  RETURN_ON_ERRORS_WITH(NULL);
    
  om = ODEModel_createFromSBML2WithObservables(d, observables);
  /* Errors will cause the program to stop, e.g. when some
     mathematical expressions are missing. */
  RETURN_ON_ERRORS_WITH(NULL);
  /* remember for freeing afterwards */
  om->d = d;

  return om;
}


/** \brief Create internal model odeModel_t from SBMLDocument containing
    a level 2 SBML model.
*/

SBML_ODESOLVER_API odeModel_t *ODEModel_createFromSBML2(SBMLDocument_t *d)
{
  return ODEModel_createFromSBML2WithObservables(d, NULL);
}

/** \brief Create internal model odeModel_t from SBMLDocument containing
    a level 2 SBML model.

    'observables' is the set of symbols that the user wishes to see
    valid at all times.  A reduced set of observables may result in
    more optimal excution.

*/
SBML_ODESOLVER_API odeModel_t *ODEModel_createFromSBML2WithObservables(SBMLDocument_t *d, char **observables)
{
  Model_t *m;
  odeModel_t *om;

  if ( SBMLDocument_getLevel(d) == 1 )
  {
    SolverError_error(ERROR_ERROR_TYPE,
		      SOLVER_ERROR_DOCUMENTLEVEL_ONE,
		      "SBML Level %d cannot be processed",
		      SBMLDocument_getLevel(d));
    RETURN_ON_ERRORS_WITH(NULL);
  }
 
  
  m = SBMLDocument_getModel(d);
  
  om = ODEModel_createWithObservables(m, observables);
  /* Errors will cause the program to stop, e.g. when some
     mathematical expressions are missing.  */
  RETURN_ON_ERRORS_WITH(NULL);
  
  return om;
}


/** Create odeModel_t directly:
    This function allows to create the internal odeModel_t structure
    independently from SBML. This structure can then be used to create
    and run integratorInstance_t, including all sensitivity analysis
    features.
    
    The formulae, both ODEs and assignments, can be passed as an array
    `f' of libSBML ASTs. `neq' is the number of ODEs, `nass' is
    the number of assignments and the passed array `f' contains both
    ODEs and assignments in this order. Assignment rules must currently
    occur in correct order, i.e. an assignment rule MUST NOT DEPEND on a
    subsequent assignment rule! See SBML Level 2 Version 1 specification
    for details on this restriction on assignment rules.
    The passed `names' and `values' arrays are of size neq+nass+nconst and
    contain names and values of ODE variables, assigned variables and
    model parameters in this order and in the same order as ASTs in `f'.
*/

SBML_ODESOLVER_API odeModel_t *ODEModel_createFromODEs(ASTNode_t **f, int neq, int nass, int nconst, char **names, double *values, Model_t *events)
{
  int i, j, nvalues;
  odeModel_t *om;

  nvalues = neq + nass + nconst;
  
  /* allocate odeModel structure and set values */
  om = ODEModel_allocate(neq, nconst, nass, 0, 0);

  /* set SBML input to NULL */
  om->d = NULL;
  om->m = NULL;
  /* set optional SBML model containing events */
  om->simple = events;
  
  /* set ODEs with indexed ASTs */
  for ( i=0; i<neq; i++ )    
    om->ode[i] = indexAST(f[i], nvalues, names);

  /* set assignments */
  for ( i=0; i<nass; i++ )
    om->assignment[i] = indexAST(f[neq+i], nvalues, names);
  
  /* set names and values */
  for ( i=0; i<neq+nass+nconst; i++ )
  {
    ASSIGN_NEW_MEMORY_BLOCK(om->names[i], strlen(names[i]) + 1, char, NULL);
    strcpy(om->names[i], names[i]);
  }

  /* set values */
  for ( i=0; i<neq+nass+nconst; i++ )
    om->values[i] = values[i];

  /* create observables list */
  om->observables = List_create();  

    /* create default list of observables */
  for ( i = 0; i <neq+nass+nconst; i++ )
  {
    char *newObservable;
    
    ASSIGN_NEW_MEMORY_BLOCK(newObservable, strlen(names[i]) + 1, char, NULL);
      strcpy(newObservable, names[i]);
      List_add(om->observables, newObservable);
  }

  for ( i = 0; i <neq+nass+nconst; i++ )
  {        
    om->observablesArray[i] = 0;

    for ( j = 0; j != List_size(om->observables); j++ )
      if ( !strcmp(om->names[i], List_get(om->observables, j)) )
	om->observablesArray[i] = 1;
  }

  ODEModel_computeAssignmentRuleSets(om);

  return om;
  
}

/** \brief Frees the odeModel structures
 */

SBML_ODESOLVER_API void ODEModel_free(odeModel_t *om)
{

  int i;

  if(om == NULL) return;

  for ( i=0; i<om->neq+om->nass+om->nconst; i++ )
    free(om->names[i]);
  free(om->names);

  /* free ODEs */
  for ( i=0; i<om->neq; i++ )
    ASTNode_free(om->ode[i]);
  free(om->ode);
  
  /* free assignments */
  for ( i=0; i<om->nass; i++ )
    ASTNode_free(om->assignment[i]);
  free(om->assignment);
  
  /* free algebraic rules */
  for ( i=0; i<om->nalg; i++ )
    ASTNode_free(om->algebraic[i]);
  free(om->algebraic);
  
  /* free Jacobian matrix, if it has been constructed */
  ODEModel_freeJacobian(om);

  /* free objective function AST if it has been constructed */
  if ( om->ObjectiveFunction != NULL )
    ASTNode_free(om->ObjectiveFunction);
 
  /* free linear objective function AST's if constructed */
  if ( om->vector_v != NULL )
   for ( i=0; i<om->neq; i++ )
    ASTNode_free(om->vector_v[i]);
  free(om->vector_v);

  /* free time_series, if present */
  if ( om->time_series != NULL )
    free_data( om->time_series );


  /* free sensitivity structures */
  /* free parameter/variable indices */
  if ( om->index_sens != NULL ) free(om->index_sens);
  if ( om->index_sensP != NULL )  free(om->index_sensP);
  ODEModel_freeSensitivity(om);

  /* free simplified ODE model */
  if ( om->simple != NULL ) Model_free(om->simple); 

  /* free document, if model was constructed from file */
  if ( om->d != NULL ) SBMLDocument_free(om->d);

  /* free values structure from SBML independent odeModel */
  if ( om->values != NULL ) free(om->values);
  
  /* free compiled code */
   if ( om->compiledCVODEFunctionCode != NULL )
   {
     CompiledCode_free(om->compiledCVODEFunctionCode);
     om->compiledCVODEFunctionCode = NULL;
   }

  /* free assignment evaulation rules */
  free(om->assignmentsAfterEvents);
  free(om->assignmentsBeforeEvents);
  free(om->assignmentsBeforeODEs);

  /* free observable lists */
  for (i=0; i != List_size(om->observables); i++)
    free(List_get(om->observables, i));
  List_free(om->observables);
  free(om->observablesArray);

  /* free model structure */
  free(om);
}       

/** \brief Returns 1 if a variable or parameter with the SBML id
    exists in the ODEModel.
*/

SBML_ODESOLVER_API int ODEModel_hasVariable(odeModel_t *model, const char *symbol)
{
  return ODEModel_getVariableIndexFields(model, symbol) != -1;
}


/** \brief Returns the total number of values in oodeModel, equivalent
    to ODEModel_getNeq + ODEModel_getNumAssignments +
    ODEModel_getNumConstants
*/

SBML_ODESOLVER_API int ODEModel_getNumValues(odeModel_t *om)
{
  return om->neq + om->nass + om->nconst + om->nalg ;
}


/** \brief Returns the name of the variable corresponding to passed
    variableIndex. The returned string (const char *) may NOT be
    changed or freed by calling applications.
*/

SBML_ODESOLVER_API const char *ODEModel_getVariableName(odeModel_t *om,
							variableIndex_t *vi)
{
  return (const char*) om->names[vi->index];
}

/** \brief Returns the number of ODEs (number of equations) in the
    odeModel
*/

SBML_ODESOLVER_API int ODEModel_getNeq(odeModel_t *om)
{
  return om->neq;
}


/** \brief Returns the number parameters for which sensitivity
    analysis might be requested

*/

SBML_ODESOLVER_API int ODEModel_getNsens(odeModel_t *om)
{
  return om->nsens;
}


/** \brief Returns the ODE from the odeModel for the variable with
    variableIndex vi.

    The ODE is returned as an `indexed abstract syntax tree' (iAST),
    which is an extension to the usual libSBML AST. Every AST_NAME
    type node in the tree has been replaced by an ASTIndexNameNode,
    that allows a O(1) retrieval of values for this node from an array
    of all values of the odeModel.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getOde(odeModel_t *om, variableIndex_t *vi)
{
  if ( 0 <= vi->index && vi->index < om->neq )
    return (const ASTNode_t *) om->ode[vi->index];
  else return NULL;
}



/** \brief Returns the number of variable assignments in the odeModel
 */

SBML_ODESOLVER_API int ODEModel_getNumAssignments(odeModel_t *om)
{
  return om->nass;
}


/** \brief Returns the assignment formula from the odeModel for the
    variable with variableIndex vi

    The ODE is returned as an `indexed abstract syntax tree' (iAST),
    which is an extension to the usual libSBML AST. Every AST_NAME
    type node in the tree has been replaced by an ASTIndexNameNode,
    that allows a O(1) retrieval of value for this node from an array
    of all values of the odeModel.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getAssignment(odeModel_t *om, variableIndex_t *vi)
{
  if (  0 <= vi->type_index && vi->type_index < om->nass )
    return (const ASTNode_t *) om->assignment[vi->type_index];  
  else return NULL;
}

/** \brief Returns the number of constant parameters of the odeModel
 */

SBML_ODESOLVER_API int ODEModel_getNumConstants(odeModel_t *om)
{
  return om->nconst;
}


/** \brief Returns the number variables that are defined by
    an algebraic rule.

    As SBML Algebraic Rules and ODE models with algebraic constraints (DAE
    models) can currently not be handled, this function is
    of no use.
*/

SBML_ODESOLVER_API int ODEModel_getNalg(odeModel_t *om)
{
  return om->nalg;
}


/** \brief Prints the names (SBML IDs) of all model variables
    and parameters
*/

SBML_ODESOLVER_API void ODEModel_dumpNames(odeModel_t *om)
{
  int i;
  for ( i=0; i<(om->neq+om->nass+om->nconst+om->nalg); i++ )
    printf("%s ", om->names[i]);
  printf("\n");
}


/** \brief Returns the SBML model that has been extracted from the input
    SBML model's reaction network and structures

    The model contains only compartments, species, parameters and SBML
    Rules. Changes to this model will have no effect on odeModel or
    integratorInstance.
*/

SBML_ODESOLVER_API const Model_t *ODEModel_getModel(odeModel_t *om)
{
  return (const Model_t *) om->simple;
}

/** @} */


/*! \defgroup jacobian Jacobian Matrix: J = df(x)/dx
  \ingroup odeModel
  \brief Constructing and Interfacing the Jacobian matrix of an ODE
  system

  as used for CVODES and IDA Dense Solvers
*/
/*@{*/

/** \brief Construct Jacobian Matrix for ODEModel.
    
Once an ODE system has been constructed from an SBML model, this
function calculates the derivative of each species' ODE with respect
to all other species for which an ODE exists, i.e. it constructs the
jacobian matrix of the ODE system. At the moment this matrix is
freed together with the ODE model. A separate function will be available
soon.\n
Returns 1 if successful, 0 otherwise. 
*/

SBML_ODESOLVER_API int ODEModel_constructJacobian(odeModel_t *om)
{  
  int i, j, k, failed, nvalues;
  ASTNode_t *fprime, *simple, *index, *ode;
  List_t *names;

  if ( om == NULL ) return 0;
  
  /******************** Calculate Jacobian ************************/
  
  failed = 0;
  nvalues = om->neq + om->nass + om->nconst;
  
  ASSIGN_NEW_MEMORY_BLOCK(om->jacob, om->neq, ASTNode_t **, 0);
  for ( i=0; i<om->neq; i++ )
    ASSIGN_NEW_MEMORY_BLOCK(om->jacob[i], om->neq, ASTNode_t *, 0);
      
  for ( i=0; i<om->neq; i++ )
  {
    ode = copyAST(om->ode[i]);

    /* assignment rule replacement: reverse to satisfy
       SBML specifications that variables defined by
       an assignment rule can appear in rules declared afterwards */
    for ( j=om->nass-1; j>=0; j-- )
      AST_replaceNameByFormula(ode,
			       om->names[om->neq + j], om->assignment[j]);

    
    for ( j=0; j<om->neq; j++ )
    {
      fprime = differentiateAST(ode, om->names[j]);
      simple =  simplifyAST(fprime);
      ASTNode_free(fprime);
      index = indexAST(simple, nvalues, om->names);
      ASTNode_free(simple);
      om->jacob[i][j] = index;
      /* check if the AST contains a failure notice */
      names = ASTNode_getListOfNodes(index ,
				     (ASTNodePredicate) ASTNode_isName);

      for ( k=0; k<List_size(names); k++ ) 
	if ( strcmp(ASTNode_getName(List_get(names,k)),
		    "differentiation_failed") == 0 ) 
	  failed++;
      List_free(names);
    }
    ASTNode_free(ode);
  }
  if ( failed != 0 ) {
    SolverError_error(WARNING_ERROR_TYPE,
		      SOLVER_ERROR_ENTRIES_OF_THE_JACOBIAN_MATRIX_COULD_NOT_BE_CONSTRUCTED,
		      "%d entries of the Jacobian matrix could not be "
		      "constructed, due to failure of differentiation. "
		      "Cvode will use internal approximation of the "
		      "Jacobian instead.", failed);
    om->jacobian = 0;
  }
  else om->jacobian = 1;
    
  return om->jacobian;
}


/** \brief Free the Jacobian matrix of the ODEModel.    
 */

SBML_ODESOLVER_API void ODEModel_freeJacobian(odeModel_t *om)
{
  int i, j;
  if ( om->jacob != NULL )
  {
    for ( i=0; i<om->neq; i++ )
    {
      for ( j=0; j<om->neq; j++ )
	ASTNode_free(om->jacob[i][j]);
      free(om->jacob[i]);
    }
    free(om->jacob);
    om->jacob = NULL;
  }
}

/**  \brief Returns the ith/jth entry of the jacobian matrix
     
Returns NULL if either the jacobian has not been constructed yet,
or if i or j are >neq. Ownership remains within the odeModel
structure.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getJacobianIJEntry(odeModel_t *om, int i, int j)
{
  if ( om->jacob == NULL ) return NULL;
  if ( i >= om->neq || j >= om->neq ) return NULL;  
  return (const ASTNode_t *) om->jacob[i][j];
}


/** \brief Returns the entry (d(vi1)/dt)/d(vi2) of the jacobian matrix.
    
Returns NULL if either the jacobian has not been constructed yet,
or if the v1 or vi2 are not ODE variables. Ownership remains
within the odeModel structure.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getJacobianEntry(odeModel_t *om, variableIndex_t *vi1, variableIndex_t *vi2)
{
  return ODEModel_getJacobianIJEntry(om, vi1->index, vi2->index);
}


/** \brief Constructs and returns the determinant of the jacobian matrix.
    
The calling application takes ownership of the returned ASTNode_t
and must free it, if not required.
*/

SBML_ODESOLVER_API ASTNode_t *ODEModel_constructDeterminant(odeModel_t *om)
{
  if ( om->jacob != NULL && om->jacobian == 1 )
    return determinantNAST(om->jacob, om->neq);
  else
    return NULL; 
}


/** @} */



/*! \defgroup parametric Sensitivity Matrix: P = df(x)/dp
  \ingroup odeModel
  \brief Constructing and Interfacing the sensitivity matrix of
  an ODE system

  as used for CVODES sensitivity analysis
*/
/*@{*/


/** \brief Construct Sensitivity R.H.S. for ODEModel.
    
Once an ODE system has been constructed from an SBML model, this
function calculates the derivative of each species' ODE with respect
to all global parameters of the SBML model\n
In an upcoming release this will only be done for selected parameters!\n
Returns 1 if successful, 0 otherwise.
   
*/
/*!!! needs repair for analytic construction outside of integrator !!!*/
SBML_ODESOLVER_API int ODEModel_constructSensitivity(odeModel_t *om)
{
  int i, j, k, l, failed, success, nvalues;
  ASTNode_t *ode, *fprime, *simple, *index;
  List_t *names;

  failed = 0;
  success = 0;
  nvalues = om->neq + om->nass + om->nconst;

  /* this might not be necessary ? */
  if ( om->sens != NULL )
    ODEModel_freeSensitivity(om);

  /* fill with default parameters if none specified
     - ? by CvodeData_initializeSensitivities ? for external use only ?) */
  if ( om->index_sens == NULL )
  {
    om->nsens = om->nconst;
    om->nsensP = om->nconst;
    ASSIGN_NEW_MEMORY_BLOCK(om->index_sens, om->nsens, int, 0);
    ASSIGN_NEW_MEMORY_BLOCK(om->index_sensP, om->nsens, int, 0);
    for ( i=0; i<om->nsens; i++ )
    {
      /* index_sens: map between cvodeSettings and om->index_sens */
      om->index_sens[i] = om->neq + om->nass + i;
      /* index_sensP: set index for the optional sensitivity matrix */
      om->index_sensP[i] = i;
    }
  }
  
  ASSIGN_NEW_MEMORY_BLOCK(om->sens, om->neq, ASTNode_t **, 0);
  for ( i=0; i<om->neq; i++ )
    ASSIGN_NEW_MEMORY_BLOCK(om->sens[i], om->nsensP, ASTNode_t *, 0);

  for ( i=0; i<om->neq; i++ )
  {
    ode = copyAST(om->ode[i]);
    /* assignment rule replacement: reverse to satisfy
       SBML specifications that variables defined by
       an assignment rule can appear in rules declared afterwards */
    for ( j=om->nass-1; j>=0; j-- )
      AST_replaceNameByFormula(ode, om->names[om->neq+j], om->assignment[j]);

    l = 0;
    for ( j=0; j<om->nsens; j++ )
    {
      if ( !(om->index_sens[j] < om->neq) )
      {
	/* differentiate d(dYi/dt) / dPj */
	fprime = differentiateAST(ode, om->names[om->index_sens[j]]);
	simple =  simplifyAST(fprime);
	ASTNode_free(fprime);
	index = indexAST(simple, nvalues, om->names);
	ASTNode_free(simple);
	om->sens[i][l] = index;
	/* check if the AST contains a failure notice */
	names = ASTNode_getListOfNodes(index,
				       (ASTNodePredicate) ASTNode_isName);

	for ( k=0; k<List_size(names); k++ ) 
	  if ( strcmp(ASTNode_getName(List_get(names,k)),
		      "differentiation_failed") == 0 ) 
	    failed++;
	List_free(names);
	l++;
      }
    }
    ASTNode_free(ode);
  }

  if ( failed != 0 )
  {
    SolverError_error(WARNING_ERROR_TYPE,
		      SOLVER_ERROR_ENTRIES_OF_THE_PARAMETRIC_MATRIX_COULD_NOT_BE_CONSTRUCTED,
		      "%d entries of the parametric `Jacobian' matrix "
		      "could not be constructed, due to failure of "
		      "differentiation. Cvode will use internal "
		      "approximation instead.",
		      failed);
    ODEModel_freeSensitivity(om);
  }
  else success = 1;

  /* return 1 if sensitivity matrix was constructed successfully,
     and 0 otherwise */
  return success;

}


/** \brief Free Sensitivity R.H.S. for ODEModel.    
 */

SBML_ODESOLVER_API void ODEModel_freeSensitivity(odeModel_t *om)
{
  int i, j;

  /* free parametric matrix, if it has been constructed */
  if ( om->sens != NULL )
  {
    for ( i=0; i<om->neq; i++ )
    {
      for ( j=0; j<om->nsensP; j++ )
	ASTNode_free(om->sens[i][j]);
      free(om->sens[i]);
    }
    free(om->sens);
    om->sens = NULL;
  }
}


/**  \brief Returns an AST of the ith/jth entry of the
     parametric matrix 

     Returns NULL if either the parametric has not been constructed yet,
     or if i > neq or j > nsens. Ownership remains within the odeModel
     structure.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getSensIJEntry(odeModel_t *om, int i, int j)
{
  if ( om->sens == NULL ) return NULL;
  if ( i >= om->neq || j >= om->nsens ) return NULL;  
  return (const ASTNode_t *) om->sens[i][j];
}


/** \brief Returns an AST for the entry (d(vi1)/dt)/d(vi2) of the
    parametric matrix.
    
    Returns NULL if either the parametric matrix has not been constructed
    yet, or if vi1 is not an ODE variable or vi2 is not a parameter or
    variable for which sensitivity analysis was requested.
    Ownership remains within the odeModel structure.
*/

SBML_ODESOLVER_API const ASTNode_t *ODEModel_getSensEntry(odeModel_t *om, variableIndex_t *vi1, variableIndex_t *vi2)
{
  int i;
  /* find sensitivity parameter/variable */
  for ( i=0; i<om->nsens && !(om->index_sens[i] == vi2->index); i++ );

  if ( i == om->nsens ) return NULL;  
  return ODEModel_getSensIJEntry(om, vi1->index, i);
}


/** \brief Returns the variableIndex for the jth parameter for
    which sensitivity analysis was requested, where
    0 < j < ODEModel_getNsens;
    
    Returns NULL if either the parametric matrix has not been constructed
    yet, or if j => ODEModel_getNsens;
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getSensParamIndexByNum(odeModel_t *om, int j)
{
  if ( j < om->nsens )
    return ODEModel_getVariableIndexByNum(om, om->index_sens[j]);
  else
    return NULL;
}


/** @} */

/** returns the number of symbols in the model
    this number is the size of the values array returned by IntegratorInstance_getValues */
int ODEModel_getNumberOfValues(odeModel_t *om)
{
  return (om->neq + om->nconst + om->nass + om->nalg);
}

/*! \defgroup variableIndex Variables + Parameters
  \ingroup odeModel
  \brief Getting the variableIndex structure

  The variableIndex can be used to retrieve formulae from odeModel,
  and to get and set current values in the integratorInstance.
*/
/*@{*/



/* searches for the string "symbol" in the odeModel's names array
   and returns its index number, or -1 if it doesn't exist */
int ODEModel_getVariableIndexFields(odeModel_t *om, const char *symbol)
{
  int i, nvalues;

  nvalues = om->neq + om->nass + om->nconst + om->nalg;
    
  for ( i=0; i<nvalues && strcmp(symbol, om->names[i]); i++ );
  if (i<nvalues)
    return i;
  return -1;
}


/** \brief Creates and returns a variable index for ith variable

Returns NULL if i > nvalues. This functions works for all types of
variables (ODE_VARIABLE, ASSIGNED_VARIABLE, ALGEBRAIC_VARIABLE and
CONSTANT). This variableIndex can be used to get and set values
during an integration run with IntegratorInstance_getVariable and
IntegratorInstance_setVariable, respectively. The variableIndex
must be freed by the calling application.
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getVariableIndexByNum(odeModel_t *om, int i)
{
  variableIndex_t *vi;

  if ( i > ODEModel_getNumValues(om) )
  {
    SolverError_error(ERROR_ERROR_TYPE, SOLVER_ERROR_SYMBOL_IS_NOT_IN_MODEL,
		      "No such variable in the model");
    return NULL;	
  }
  else
  {
    ASSIGN_NEW_MEMORY(vi, variableIndex_t, NULL);
    vi->index = i;

    if ( i<om->neq )
    {
      vi->type = ODE_VARIABLE;
      vi->type_index = vi->index;
    }
    else if ( i < om->neq + om->nass )
    {
      vi->type = ASSIGNMENT_VARIABLE;
      vi->type_index = i - om->neq;
    }
    else if ( i < om->neq + om->nass + om->nconst )
    {
      vi->type = CONSTANT;
      vi->type_index = i - om->neq - om->nass;
    }
    else
    {
      vi->type = ALGEBRAIC_VARIABLE;
      vi->type_index = i - om->neq - om->nass - om->nconst;
    }
  }
  return vi;
}


/** \brief Creates and returns the variableIndex for the string "symbol"

where `symbol' is the ID (corresponding to the SBML ID in the input
model) of one of the models variables (ODE_VARIABLE,
ASSIGNED_VARIABLE, ALGEBRAIC_VARIABLE and CONSTANT) or NULL if the
symbol was not found. The variableIndex must be freed by the
calling application.
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getVariableIndex(odeModel_t *om, const char *symbol)
{

  int index;

  if ( symbol == NULL )
   {
    SolverError_error(ERROR_ERROR_TYPE, SOLVER_ERROR_SYMBOL_IS_NOT_IN_MODEL,
		      "symbol %s is not in the model", symbol);

    return NULL;
  }
 
  index = ODEModel_getVariableIndexFields(om, symbol);

  if ( index == -1 )
  {
    SolverError_error(ERROR_ERROR_TYPE, SOLVER_ERROR_SYMBOL_IS_NOT_IN_MODEL,
		      "symbol %s is not in the model", symbol);

    return NULL;
  }

  return ODEModel_getVariableIndexByNum(om, index);
}



/** \brief  Creates and returns a variable index for ith ODE variable.

Returns NULL if not existing (i > ODEModel_getNeq(om)). The
variableIndex must be freed by the calling application.
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getOdeVariableIndex(odeModel_t *om, int i)
{
  if ( i < om->neq )
    return ODEModel_getVariableIndexByNum(om, i);
  else
    return NULL;
}


/** \brief Creates and returns a variable index for ith assigned variable.

Returns NULL if not existing (i > ODEModel_getNumAssignedVar(om)).
The variableIndex must be freed by the calling application.
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getAssignedVariableIndex(odeModel_t *om, int i)
{
  if ( i < om->nass )
    return ODEModel_getVariableIndexByNum(om, i + om->neq);
  else
    return NULL;  
}

/**\brief  Creates and returns a variable index for ith constant.

Returns NULL if not existing (i > ODEModel_getNumConstants(om)).
The variableIndex must be freed by the calling application.
*/

SBML_ODESOLVER_API variableIndex_t *ODEModel_getConstantIndex(odeModel_t *om, int i)
{
  if ( i < om->nconst )
    return ODEModel_getVariableIndexByNum(om, i + om->neq + om->nass);
  else
    return NULL;
}

/** \brief  Frees a variableIndex structure
 */

SBML_ODESOLVER_API void VariableIndex_free(variableIndex_t *vi)
{
  free(vi);
}

int VariableIndex_getIndex(variableIndex_t *vi)
{
  return vi->index ;
}

/* appends a compilable expression for the given AST to the given buffer
   assuming that the node has not been indexed */
void ODEModel_generateASTWithoutIndex(odeModel_t *om,
				      charBuffer_t *buffer,
				      ASTNode_t *node)
{
  ASTNode_t *index = indexAST(node, om->neq + om->nass +
			      om->nconst, om->names);
  generateAST(buffer, index);
  ASTNode_free(index);
}

/* appends a compilable assignment to the buffer.
   The assignment is made to the 'value' array item indexed by 'index'.
   The value assigned is computed from the given AST. */
void ODEModel_generateAssignmentCode(odeModel_t *om,
				     int index,
				     ASTNode_t *node,
				     charBuffer_t *buffer)
{
  CharBuffer_append(buffer, "value[");
  CharBuffer_appendInt(buffer, index);
  CharBuffer_append(buffer, "] = ");
  ODEModel_generateASTWithoutIndex(om, buffer, node);
  CharBuffer_append(buffer, ";\n");
}

/* appends compiled code for a set of assignment rules to the gievn buffer.
   The assignments generated are taken from the assignment rules in the
   given model however the set generated is determined by the
   given 'requiredAssignments' boolean array which is indexed in the same
   order as the 'assignment' array on the given model. */
void ODEModel_generateAssignmentRuleCode(odeModel_t *om,
					 int *requiredAssignments,
					 charBuffer_t *buffer)
{
  int i ;

  for ( i=0; i<om->nass; i++ )
    if ( !requiredAssignments || requiredAssignments[i] )
      ODEModel_generateAssignmentCode(om,
				      om->neq+i, om->assignment[i], buffer);
}

/** appends compiled code to the given buffer for the function called by
    the value of 'COMPILED_EVENT_FUNCTION_NAME' which implements the
    assignment rules and events required to ensure that observables have
    the correct values and that ODE variables are in the right state
    for the next intergation step.
*/
void ODEModel_generateEventFunction(odeModel_t *om, charBuffer_t *buffer)
{
  int i, j;
  ASTNode_t *trigger, *assignment;
  Event_t *e;
  EventAssignment_t *ea;
  variableIndex_t *vi;

  CharBuffer_append(buffer,"DLL_EXPORT int ");
  CharBuffer_append(buffer,COMPILED_EVENT_FUNCTION_NAME);
  CharBuffer_append(buffer,"(cvodeData_t *data, int *odeVarIsValid)\n"\
		    "{\n"\
		    "    double *value = data->value;\n"\
		    "    int fired = 0;\n"\
		    "    int *trigger = data->trigger;\n");
    
  ODEModel_generateAssignmentRuleCode(om, om->assignmentsBeforeEvents, buffer);

  if ( om->simple != NULL )
  {
    for ( i=0; i<Model_getNumEvents(om->simple); i++ )
    {
      int setIsValidFalse = 0;
      
      e = Model_getEvent(om->simple, i);
      trigger = (ASTNode_t *) Event_getTrigger(e);
      
      CharBuffer_append(buffer, "if ((trigger[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "] == 0) && (");
      ODEModel_generateASTWithoutIndex(om, buffer, trigger);
      CharBuffer_append(buffer, "))\n"\
			"{\n"\
			"    fired++;\n"\
			"    trigger[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "] = 1;\n");
      
      for ( j=0; j<Event_getNumEventAssignments(e); j++ )
      {
	/* generate event assignment */
	ea = Event_getEventAssignment(e, j);
	assignment = (ASTNode_t *) EventAssignment_getMath(ea);           
	vi = ODEModel_getVariableIndex(om, EventAssignment_getVariable(ea));
	CharBuffer_append(buffer, "    ");
	ODEModel_generateAssignmentCode(om, vi->index, assignment, buffer);
	VariableIndex_free(vi);
	
	/* identify cases which modify variables computed by solver which
	   set the solver into an invalid state */
	if ( vi->index < om->neq && !setIsValidFalse )
	{
	  CharBuffer_append(buffer, "    *odeVarIsValid = 0;\n");
	  setIsValidFalse = 1 ;
	}
      }
      
      CharBuffer_append(buffer, "}\n"\
			"else {\n"\
			"    trigger[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "] = 0;\n"\
			"}\n");
    }
  }
  /*!!! check if problem arose through SBML independent code */
  ODEModel_generateAssignmentRuleCode(om, om->assignmentsAfterEvents, buffer);
    
  CharBuffer_append(buffer, "return fired;\n}\n");
}

/* appends compiled code to the given buffer for the function called
   by the value of 'COMPILED_RHS_FUNCTION_NAME' which calculates the
   right hand side ODE values for the set of ODEs being solved. */
void ODEModel_generateCVODERHSFunction(odeModel_t *om, charBuffer_t *buffer)
{
  int i ;

  CharBuffer_append(buffer,"DLL_EXPORT void ");
  CharBuffer_append(buffer,COMPILED_RHS_FUNCTION_NAME);
  CharBuffer_append(buffer,
		    "(realtype t, N_Vector y, N_Vector ydot, void *f_data)\n"\
		    "{\n"\
		    "    int i;\n"\
		    "    realtype *ydata, *dydata;\n"\
		    "    cvodeData_t *data;\n"\
		    "    double *value ;\n"\
		    "    data = (struct cvodeData *) f_data;\n"\
		    "    value = data->value;\n"\
		    "    ydata = NV_DATA_S(y);\n"\
		    "    dydata = NV_DATA_S(ydot);\n");

  /* update parameters: p is modified by CVODES,
     if fS could not be generated  */

  /* update time  */
  CharBuffer_append(buffer, "data->currenttime = t;\n");

  CharBuffer_append(buffer,
		    "if ( data->opt->Sensitivity && " \
		    " !data->model->sensitivity )\n"\
		    "    for ( i=0; i<data->nsens; i++ )\n"\
		    "    data->value[data->model->index_sens[i]] = "\
		    "data->p[i];\n");

  /* update ODE variables from CVODE */
  for ( i=0; i<om->neq; i++ ) 
  {
    CharBuffer_append(buffer, "value[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = ydata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "];\n");
  }

  /* update assignment rules */
  ODEModel_generateAssignmentRuleCode(om, om->assignmentsBeforeODEs, buffer);

  /* evaluate ODEs f(x,p,t) = dx/dt */
  for ( i=0; i<om->neq; i++ ) 
  {
    CharBuffer_append(buffer, "dydata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = ");
    generateAST(buffer, om->ode[i]);
    CharBuffer_append(buffer, ";\n");
  }
 /*  CharBuffer_append(buffer, "printf(\"F\");");*/
  CharBuffer_append(buffer, "}\n");
}

/* appends compiled code to the given buffer for the function called by
   the value of 'COMPILED_JACOBIAN_FUNCTION_NAME' which
   calculates the Jacobian for the set of ODEs being solved. */
void ODEModel_generateCVODEJacobianFunction(odeModel_t *om,
					    charBuffer_t *buffer)
{
  int i, j ;

  CharBuffer_append(buffer,"DLL_EXPORT void ");
  CharBuffer_append(buffer,COMPILED_JACOBIAN_FUNCTION_NAME);
  CharBuffer_append(buffer,
		    "(long int N, DenseMat J, realtype t,\n"\
		    "    N_Vector y, N_Vector fy, void *jac_data,\n"\
		    "    N_Vector vtemp1, N_Vector vtemp2, N_Vector vtemp3)\n"\
		    "{\n"\
		    "  \n"\
		    "int i;\n"\
		    "realtype *ydata;\n"\
		    "cvodeData_t *data;\n"\
		    "double *value;\n"\
		    "data  = (cvodeData_t *) jac_data;\n"\
		    "value = data->value ;\n"\
		    "ydata = NV_DATA_S(y);\n"\
		    "data->currenttime = t;\n"\
		    "\n"\
		    "if ( data->p != NULL && data->opt->Sensitivity )\n"\
		    "    for ( i=0; i<data->nsens; i++ )\n"\
		    "        value[data->model->index_sens[i]] = "\
		    "data->p[i];\n\n");

  /** update ODE variables from CVODE */
  for ( i=0; i<om->neq; i++ ) 
  {
    CharBuffer_append(buffer, "value[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = ydata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "];\n");
  }

  /** evaluate Jacobian J = df/dx */
  for ( i=0; i<om->neq; i++ )
  {
    for ( j=0; j<om->neq; j++ ) 
    {
      CharBuffer_append(buffer, "DENSE_ELEM(J,");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, ",");
      CharBuffer_appendInt(buffer, j);
      CharBuffer_append(buffer, ") = ");
      generateAST(buffer, om->jacob[i][j]);
      CharBuffer_append(buffer, ";\n");
    }
  }
  /* CharBuffer_append(buffer, "printf(\"J\");"); */
  CharBuffer_append(buffer, "}\n");
}

/* appends compiled code to the given buffer for the function called by
   the value of 'COMPILED_JACOBIAN_FUNCTION_NAME' which
   calculates the Jacobian for the set of ODEs being solved. */
void ODEModel_generateCVODEAdjointJacobianFunction(odeModel_t *om,
						   charBuffer_t *buffer)
{
  int i, j ;

  CharBuffer_append(buffer,"DLL_EXPORT void ");
  CharBuffer_append(buffer,COMPILED_ADJOINT_JACOBIAN_FUNCTION_NAME);
  CharBuffer_append(buffer,
		    "(long int NB, DenseMat JB, realtype t,\n"\
		    "    N_Vector y, N_Vector yB, void *jac_dataB,\n"\
		    "    N_Vector tmpB, N_Vector tmp2B, N_Vector tmp3B)\n"\
		    "{\n"\
		    "  \n"\
		    "int i;\n"\
		    "realtype *ydata;\n"\
		    "cvodeData_t *data;\n"\
		    "double *value;\n"\
		    "data  = (cvodeData_t *) jac_dataB;\n"\
		    "value = data->value ;\n"\
		    "ydata = NV_DATA_S(y);\n"\
		    "data->currenttime = t;\n"\
		    "\n"\
		    "if ( data->p != NULL && data->opt->Sensitivity )\n"\
		    "    for ( i=0; i<data->nsens; i++ )\n"\
		    "        value[data->model->index_sens[i]] = "\
		    "data->p[i];\n\n");

  /** update ODE variables from CVODE */
  for ( i=0; i<om->neq; i++ ) 
  {
    CharBuffer_append(buffer, "value[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = ydata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "];\n");
  }

  /** evaluate Jacobian J = df/dx */
  for ( i=0; i<om->neq; i++ )
  {
    for ( j=0; j<om->neq; j++ ) 
    {
      CharBuffer_append(buffer, "DENSE_ELEM(JB,");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, ",");
      CharBuffer_appendInt(buffer, j);
      CharBuffer_append(buffer, ") = - ");
      generateAST(buffer, om->jacob[i][j]);
      CharBuffer_append(buffer, ";\n");
    }
  }
  /* CharBuffer_append(buffer, "printf(\"JA\");"); */
  CharBuffer_append(buffer, "}\n");
}

/* appends compiled code to the given buffer for the function called
   by the value of 'COMPILED_SENSITIVITY_FUNCTION_NAME' which
   calculates the sensitivities (derived from Jacobian and parametrix
   matrices) for the set of ODEs being solved. */
void ODEModel_generateCVODESensitivityFunction(odeModel_t *om,
					       charBuffer_t *buffer)
{
  int i, j, k;

  CharBuffer_append(buffer,"DLL_EXPORT void ");
  CharBuffer_append(buffer,COMPILED_SENSITIVITY_FUNCTION_NAME);
  CharBuffer_append(buffer,
		    "(int Ns, realtype t, N_Vector y, N_Vector ydot,\n"\
		    " int iS, N_Vector yS, N_Vector ySdot, \n"
		    " void *fs_data, N_Vector tmp1, N_Vector tmp2)\n"\
		    "{\n"\
		    "  \n"\
		    "realtype *ydata, *ySdata, *dySdata;\n"\
		    "cvodeData_t *data;\n"\
		    "double *value;\n"\
		    "data = (cvodeData_t *) fs_data;\n"\
		    "value = data->value ;\n"\
		    "ydata = NV_DATA_S(y);\n"\
		    "ySdata = NV_DATA_S(yS);\n"\
		    "dySdata = NV_DATA_S(ySdot);\n"\
		    "data->currenttime = t;\n");

  /** update ODE variables from CVODE */
  for ( i=0; i<om->neq; i++ )
  {
    CharBuffer_append(buffer, "value[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = ydata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "];\n\n");
  }
  
  /** evaluate sensitivity RHS: df/dx * s + df/dp for one p */
  for ( i=0; i<om->neq; i++ )
  {
    CharBuffer_append(buffer, "dySdata[");
    CharBuffer_appendInt(buffer, i);
    CharBuffer_append(buffer, "] = 0.0;\n");
    for (j=0; j<om->neq; j++)
    {
      CharBuffer_append(buffer, "dySdata[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "] +=  ");
      generateAST(buffer, om->jacob[i][j]);
      CharBuffer_append(buffer, " * ySdata[");
      CharBuffer_appendInt(buffer, j);
      CharBuffer_append(buffer, "]; ");
      CharBuffer_append(buffer, " /* om->jacob[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "][");
      CharBuffer_appendInt(buffer, j);
      CharBuffer_append(buffer, "]  */ \n");
    }
 
    for ( k=0; k<om->nsens; k++ )
    {
      CharBuffer_append(buffer, "if ( ");
      CharBuffer_appendInt(buffer, k);
      CharBuffer_append(buffer, " == iS ) ");
      CharBuffer_append(buffer, "dySdata[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "] += ");
      if ( om->index_sensP[k] == -1 )
	CharBuffer_appendInt(buffer, 0);
      else 
	generateAST(buffer, om->sens[i][om->index_sensP[k]]);
      CharBuffer_append(buffer, "; ");
      CharBuffer_append(buffer, " /* om->sens[");
      CharBuffer_appendInt(buffer, i);
      CharBuffer_append(buffer, "][");
      CharBuffer_appendInt(buffer,om->index_sensP[k]);
      CharBuffer_append(buffer, "]  */ \n");
    }
  }
  /* CharBuffer_append(buffer, "printf(\"S\");"); */
  CharBuffer_append(buffer, "}");
}

/* dynamically generates and complies the ODE RHS, Jacobian and
   Events handling functions for the given model.
   The jacobian function is not generated if the jacobian AST
   expressions have not been generated.
*/
void ODEModel_compileCVODEFunctions(odeModel_t *om)
{
  charBuffer_t *buffer = CharBuffer_create();

  /* if available, the whole code needs recompilation, can happen
     for subsequent runs with new sensitivity settings */
  if ( om->compiledCVODEFunctionCode != NULL )
  {
    CompiledCode_free(om->compiledCVODEFunctionCode);
    om->compiledCVODEFunctionCode = NULL;
  }

#ifdef WIN32        
  CharBuffer_append(buffer,
		    "#include <windows.h>\n"\
		    "#include <math.h>\n"\
		    "#include <sbmlsolver/sundialstypes.h>\n"\
		    "#include <sbmlsolver/nvector.h>\n"\
		    "#include <sbmlsolver/nvector_serial.h>\n"\
		    "#include <sbmlsolver/dense.h>\n"\
		    "#include <sbmlsolver/cvodes.h>\n"\
		    "#include <sbmlsolver/cvodea.h>\n"\
		    "#include <sbmlsolver/cvdense.h>\n"\
		    "#include <sbmlsolver/cvodeData.h>\n"\
		    "#include <sbmlsolver/cvodeSettings.h>\n"\
		    "#include <sbmlsolver/odeModel.h>\n"\
		    "#define DLL_EXPORT __declspec(dllexport)\n");
#elif USE_TCC == 1
  CharBuffer_append(buffer,
		    "#include <math.h>\n"\
		    "#include <nvector_serial.h>\n"\
		    "#include <dense.h>\n" 
		    "#include <cvodes.h>\n"\
		    "#include <cvodea.h>\n"\
		    "#include <cvdense.h>\n"\
		    "#include <sbmlsolver/cvodeData.h>\n"\
		    "#include <sbmlsolver/integratorSettings.h>\n"\
		    "#include <sbmlsolver/odeModel.h>\n"\
		    "#define DLL_EXPORT\n");
#endif

  generateMacros(buffer);
  
  if ( om->jacobian )
  {
    ODEModel_generateCVODEJacobianFunction(om, buffer);
    /* this could later be made optional and compiled into
       a separate structure when TCC allows */
    ODEModel_generateCVODEAdjointJacobianFunction(om, buffer);
  }
  ODEModel_generateEventFunction(om, buffer);

  ODEModel_generateCVODERHSFunction(om, buffer);

  /* including sensitivity matrix here implies that re-compilation of
   all functions is required when sensitivities change, could later
   be compiled into a separate structure when TCC allows */
  if ( om->sensitivity )
    ODEModel_generateCVODESensitivityFunction(om, buffer);

#ifdef _DEBUG /* write out source file for debugging*/
  FILE *src;
  char *srcname =  "rhsfunctions.c";
  src = fopen(srcname, "w");
  fprintf(src, CharBuffer_getBuffer(buffer));
  fclose(src);
#endif

  /* now all required sourcecode is in `buffer' and can be sent
     to the compiler */
  om->compiledCVODEFunctionCode =
    Compiler_compile(CharBuffer_getBuffer(buffer));


  if ( SolverError_getNum(ERROR_ERROR_TYPE) ||
       SolverError_getNum(FATAL_ERROR_TYPE) )
  {
    CharBuffer_free(buffer);
    return ;
  }

  CharBuffer_free(buffer);


  om->compiledCVODERhsFunction =
    CompiledCode_getFunction(om->compiledCVODEFunctionCode,
			     COMPILED_RHS_FUNCTION_NAME);
  if ( om->jacobian )
  {
    om->compiledCVODEJacobianFunction =
      CompiledCode_getFunction(om->compiledCVODEFunctionCode,
			       COMPILED_JACOBIAN_FUNCTION_NAME);

    if ( SolverError_getNum(ERROR_ERROR_TYPE) ||
	 SolverError_getNum(FATAL_ERROR_TYPE) )
      return;

    om->compiledCVODEAdjointJacobianFunction =
      CompiledCode_getFunction(om->compiledCVODEFunctionCode,
			       COMPILED_ADJOINT_JACOBIAN_FUNCTION_NAME);

    if ( SolverError_getNum(ERROR_ERROR_TYPE) ||
	 SolverError_getNum(FATAL_ERROR_TYPE) )
      return;
  }

  om->compiledEventFunction =
    CompiledCode_getFunction(om->compiledCVODEFunctionCode,
			     COMPILED_EVENT_FUNCTION_NAME);
  
  if ( SolverError_getNum(ERROR_ERROR_TYPE) ||
       SolverError_getNum(FATAL_ERROR_TYPE) )
    return;

  if ( om->sensitivity )
     om->compiledCVODESenseFunction =
       CompiledCode_getFunction(om->compiledCVODEFunctionCode,
				COMPILED_SENSITIVITY_FUNCTION_NAME);
}


/* dynamically generates and compiles the ODE Sensitivity RHS
   for the given model */
/*!!! this function is currently not called, because TCC is not
  thread-safe and can only have one TCCState at a time, thus all
  functions need to be re-compiled when sensitivity changes */
void ODEModel_compileCVODESenseFunctions(odeModel_t *om)
{
  charBuffer_t *buffer = CharBuffer_create();

#ifdef WIN32        
  CharBuffer_append(buffer,
		    "#include <windows.h>\n"\
		    "#include <math.h>\n"\
		    "#include <sbmlsolver/sundialstypes.h>\n"\
		    "#include <sbmlsolver/nvector.h>\n"\
		    "#include <sbmlsolver/nvector_serial.h>\n"\
		    "#include <sbmlsolver/dense.h>\n" 
		    "#include <sbmlsolver/cvodes.h>\n"\
		    "#include <sbmlsolver/cvodea.h>\n"\
		    "#include <sbmlsolver/cvdense.h>\n"\
		    "#include <sbmlsolver/cvodeData.h>\n"\
		    "#include <sbmlsolver/cvodeSettings.h>\n"\
		    "#include <sbmlsolver/processAST.h>\n"\
		    "#include <sbmlsolver/odeModel.h>\n"\
		    "#define DLL_EXPORT __declspec(dllexport)\n");
#elif USE_TCC == 1
  CharBuffer_append(buffer,
		    "#include <math.h>\n"\
		    "#include <nvector_serial.h>\n"\
		    "#include <dense.h>\n" 
		    "#include <cvodes.h>\n"\
		    "#include <cvdense.h>\n"\
		    "#include <cvodea.h>\n"\
		    "#include <sbmlsolver/cvodeData.h>\n"\
		    "#include <sbmlsolver/integratorSettings.h>\n"\
		    "#include <sbmlsolver/processAST.h>\n"\
		    "#include <sbmlsolver/odeModel.h>\n"\
		    "#define DLL_EXPORT\n");
#endif

  generateMacros(buffer);

  if ( om->sensitivity )
    ODEModel_generateCVODESensitivityFunction(om, buffer);

#ifdef _DEBUG /* write out source file for debugging*/
  FILE *src;
  char *srcname =  "sensfunctions.c";
  src = fopen(srcname, "w");
  fprintf(src, CharBuffer_getBuffer(buffer));
  fclose(src);
#endif
  
  /* now all required sourcecode is in `buffer' and can be sent
     to the compiler */
  om->compiledCVODESensitivityCode =
    Compiler_compile(CharBuffer_getBuffer(buffer));

  if ( SolverError_getNum(ERROR_ERROR_TYPE) ||
       SolverError_getNum(FATAL_ERROR_TYPE) )
  {
    CharBuffer_free(buffer);
    return ;
  }

  CharBuffer_free(buffer);

  om->compiledCVODESenseFunction =
    CompiledCode_getFunction(om->compiledCVODESensitivityCode,
			     COMPILED_SENSITIVITY_FUNCTION_NAME);
}


/** returns the compiled RHS ODE function for the given model */
SBML_ODESOLVER_API CVRhsFn ODEModel_getCompiledCVODERHSFunction(odeModel_t *om)
{
  if ( !om->compiledCVODERhsFunction )
  {
    ODEModel_compileCVODEFunctions(om);
    RETURN_ON_ERRORS_WITH(NULL);
  }

  return om->compiledCVODERhsFunction;
}

/** returns the compiled Jacobian function for the given model */
SBML_ODESOLVER_API CVDenseJacFn ODEModel_getCompiledCVODEJacobianFunction(odeModel_t *om)
{
  if ( !om->jacobian )
  {
    SolverError_error(ERROR_ERROR_TYPE,
		      SOLVER_ERROR_CANNOT_COMPILE_JACOBIAN_NOT_COMPUTED,
		      "Attempting to compile jacobian before the jacobian "\
		      "is computed\n"\
		      "Call ODEModel_constructJacobian before calling\n"\
		      "ODEModel_getCompiledCVODEJacobianFunction or "\
		      "ODEModel_getCompiledCVODERHSFunction\n");
    return NULL;
  }

  if ( !om->compiledCVODEJacobianFunction )
  {
    /* only for calling independent of solver!!
       function should have been compiled already */
    ODEModel_compileCVODEFunctions(om);
    RETURN_ON_ERRORS_WITH(NULL);
  }

  return om->compiledCVODEJacobianFunction;
}

/** returns the compiled Sensitivity function for the given model */
SBML_ODESOLVER_API CVSensRhs1Fn ODEModel_getCompiledCVODESenseFunction(odeModel_t *om)
{
  if ( !om->sensitivity )
  {
    SolverError_error(ERROR_ERROR_TYPE,
		      SOLVER_ERROR_CANNOT_COMPILE_SENSITIVTY_NOT_COMPUTED,
		      "Attempting to compile sensitivity matrix before "\
		      "the matrix is computed\n"\
		      "Call ODEModel_constructSensitivity before calling\n"\
		      "ODEModel_getCompiledCVODESenseFunction\n");
    return NULL;
  }

  if ( !om->compiledCVODESenseFunction )
  {
    /*!!! currently not used: if TCC multiple states become possible,
      until then this must have been compiled already within the main
      compiled code structure */
    ODEModel_compileCVODESenseFunctions(om);
    RETURN_ON_ERRORS_WITH(NULL);
  }

  return om->compiledCVODESenseFunction;
}

/** returns the compiled adjoint jacobian function for the given model */
SBML_ODESOLVER_API CVDenseJacFnB ODEModel_getCompiledCVODEAdjointJacobianFunction(odeModel_t *om)
{
  if ( !om->jacobian )
  {
    SolverError_error(ERROR_ERROR_TYPE,
		      SOLVER_ERROR_CANNOT_COMPILE_JACOBIAN_NOT_COMPUTED,
		      "Attempting to compile adjoint jacobian before "\
		      "the jacobian is computed\n"\
		      "Call ODEModel_constructJacobian before calling\n"\
		      "ODEModel_getCompiledCVODEAdjointJacobianFunction or "\
		      "ODEModel_getCompiledCVODERHSFunction\n");
    return NULL;
  }

  if ( !om->compiledCVODEAdjointJacobianFunction )
  {
    /* only for calling independent of solver!!
       function should have been compiled already */
    ODEModel_compileCVODEFunctions(om);
    RETURN_ON_ERRORS_WITH(NULL);
  }

  return om->compiledCVODEAdjointJacobianFunction;
}

/** @} */
/* End of file */
