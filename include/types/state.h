/*
 * $Id$
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.1
 * Copyright (c) 1991-2001, University of Groningen, The Netherlands
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 * 
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 * 
 * For more info, check our website at http://www.gromacs.org
 * 
 * And Hey:
 * Gnomes, ROck Monsters And Chili Sauce
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
 * The t_state struct should contain all the (possibly) non-static
 * information required to define the state of the system.
 * Currently the random seeds for SD and BD are missing.
 */

typedef struct
{
  int           natoms;
  int           ngtc;
  real          lambda; /* the free energy switching parameter          */
  matrix 	box;    /* box vector coordinates                      	*/
  matrix 	boxv;   /* box velocitites for Parrinello-Rahman pcoupl */
  matrix        pcoupl_mu; /* for Berendsen pcoupl                      */
  real          *nosehoover_xi; /* for Nose-Hoover tcoupl (ngtc)        */
  real          *tcoupl_lambda; /* for Berendsen tcoupl (ngtc)          */
  rvec          *x;     /* the coordinates (natoms)                     */
  rvec          *v;     /* the velocities (natoms)                      */
} t_state;
