/*
  Last changed Time-stamp: <2007-09-20 03:10:44 raim>
  $Id: variableIndex.h,v 1.10 2007/09/20 01:16:14 raimc Exp $ 
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

#ifndef _VARIABLEINDEX_H_
#define _VARIABLEINDEX_H_

/** variable types */
typedef enum variableType
  {
    ODE_VARIABLE,
    ASSIGNMENT_VARIABLE,
    CONSTANT,
    ALGEBRAIC_VARIABLE
  } variableType_t; 	 

/** The variableIndex is the main interface to model
    variables and constants.

    It can be retrieved from an odeModel, and used to get its name
    or associated symbolic constructs from the odeModel structure
    AND to get and set the values of all model constants and variables
    via the integratorInstance during any time of an integration run.
    Please see documentation of these modules to learn about the full
    use of variableIndex */
struct variableIndex
{
  variableType_t type;
  int type_index;
  int index ;
} ;

#endif
/* _VARIABLEINDEX_H_ */
