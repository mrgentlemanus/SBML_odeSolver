#ifndef _SOLVERERROR_H_
#define _SOLVERERROR_H_

#include <stdarg.h>
#include "sbmlsolver/exportdefs.h"

/* error codes */

typedef enum errorCode
{
    /* codes < 0 reserved for CVODE */
    /* codes 0 throu 9999 reserved for LibSBML */

    /* 1XXXX - conversion to ode model failures */
    SOLVER_ERROR_ODE_COULD_NOT_BE_CONSTRUCTED_FOR_SPECIES  = 10000,
    SOLVER_ERROR_THE_MODEL_CONTAINS_EVENTS = 10001,
    SOLVER_ERROR_THE_MODEL_CONTAINS_ALGEBRAIC_RULES = 10002,
    SOLVER_ERROR_ODE_MODEL_COULD_NOT_BE_CONSTRUCTED = 10003,
    SOLVER_ERROR_NO_KINETIC_LAW_FOUND_FOR_REACTION = 10004,
    SOLVER_ERROR_ENTRIES_OF_THE_JACOBIAN_MATRIX_COULD_NOT_BE_CONSTRUCTED = 10005,
    SOLVER_ERROR_MODEL_NOT_SIMPLIFIED = 10006,
    SOLVER_ERROR_MAKE_SURE_SCHEMA_IS_ON_PATH = 10007,
    SOLVER_ERROR_CANNOT_PARSE_MODEL = 10008,
    
    /* 2XXXX - Integration failures */
    SOLVER_ERROR_INTEGRATION_NOT_SUCCESSFUL = 20000,
    SOLVER_ERROR_EVENT_TRIGGER_FIRED = 20001,
    SOLVER_ERROR_CVODE_MALLOC_FAILED = 20002
} errorCode_t;


typedef enum errorType
{
    FATAL_ERROR_TYPE = 0,
    ERROR_ERROR_TYPE = 1,
    WARNING_ERROR_TYPE = 2,
    NUMBER_OF_ERROR_TYPES = 3
} errorType_t;

#define RETURN_ON_ERRORS_WITH(x) \
{if (SolverError_getNum(ERROR_ERROR_TYPE) || SolverError_getNum(FATAL_ERROR_TYPE)) return (x); }

#define RETURN_ON_FATALS_WITH(x) \
{if (SolverError_getNum(FATAL_ERROR_TYPE)) return (x); }

/* get number of stored errors  of given type */
SBML_ODESOLVER_API int
SolverError_getNum(errorType_t); 

/* get a stored error message */
SBML_ODESOLVER_API char *
SolverError_getMessage(errorType_t, int errorNum);

/* get error code */
SBML_ODESOLVER_API errorCode_t
SolverError_getCode(errorType_t, int errorNum);

/* get error code of last error stored of given type */
SBML_ODESOLVER_API errorCode_t
SolverError_getLastCode(errorType_t);

/* empty error store */
SBML_ODESOLVER_API void
SolverError_clear();

/* create an error */
SBML_ODESOLVER_API void
SolverError_error(errorType_t, errorCode_t, char *format, ...);

/* exit the program if errors or fatals have been created. */
SBML_ODESOLVER_API void
SolverError_haltOnErrors();

/* write all errors and warnings to standard error */
SBML_ODESOLVER_API void
SolverError_dump();

/* write all errors and warnings to standard error and then empty error store*/
SBML_ODESOLVER_API void
SolverError_dumpAndClearErrors();

#endif 
/* _SOLVERERROR_H_ */
