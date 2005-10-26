/*
  Last changed Time-stamp: <2005-10-26 17:39:03 raim>
  $Id: printModel.h,v 1.2 2005/10/26 15:41:56 raimc Exp $
*/
/* 
 *
 * This application is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * any later version.
 *
 * This application  is distributed in the hope that it will be useful, but
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
 *
 */

#ifndef _PRINTMODEL_H_
#define _PRINTMODEL_H_

/* print model structures to file */
void printModel(Model_t *m, FILE *f);
void printSpecies(Model_t *m, FILE *f);
void printReactions(Model_t *m, FILE *f);
void printODEs(odeModel_t *om, FILE *f);
void printODEsToSBML(Model_t *ode, FILE *f);
void printJacobian(odeModel_t *om, FILE *f);
/* print results of simulation to file */
void printConcentrationTimeCourse(cvodeData_t *data, FILE *f);
void printOdeTimeCourse(cvodeData_t *data, FILE *f);
void printReactionTimeCourse(cvodeData_t *data, Model_t *m, FILE *f);
void printJacobianTimeCourse(cvodeData_t *data, FILE *f);
void printDeterminantTimeCourse(cvodeData_t *data, ASTNode_t* det, FILE *f);

void
printPhase(cvodeData_t *data);

#endif
/* End of file */
