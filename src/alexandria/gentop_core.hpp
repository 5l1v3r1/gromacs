/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 * $Id: gentop_core.h,v 1.8 2009/02/02 21:11:11 spoel Exp $
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 4.0.99
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2008, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

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
 * Groningen Machine for Chemical Simulation
 */

#ifndef GENTOP_CORE_HPP
#define GENTOP_CORE_HPP

#include <stdio.h>
#include "gromacs/legacyheaders/typedefs.h"
#include "gromacs/fileio/pdbio.h"
#include "gromacs/gmxpreprocess/gpp_nextnb.h"
#include "gromacs/gmxpreprocess/gpp_atomtype.h"
#include "gentop_nm2type.hpp"
#include "poldata.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void calc_angles_dihs(t_params *ang,t_params *dih,rvec x[],gmx_bool bPBC,matrix box);
			     
real calc_dip(t_atoms *atoms,rvec x[]);

void dump_hybridization(FILE *fp,t_atoms *atoms,int nbonds[]);

void reset_q(t_atoms *atoms);

void print_rtp(const char *filenm,const char *title,t_atoms *atoms,
               t_params plist[],int cgnr[],int nbts,int bts[]);

void add_shells(gmx_poldata_t pd,int maxatom,t_atoms *atoms,
                gpp_atomtype_t atype,t_params plist[],
                rvec *x,t_symtab *symtab,t_excls **excls);
		       
int *symmetrize_charges(gmx_bool bQsym,
                        t_atoms *atoms,t_params *bonds,gmx_poldata_t pd,
                        gmx_atomprop_t aps,const char *symm_string);
    
enum eChargeGroup { ecgAtom, ecgGroup, ecgNeutral, ecgNR };

int *generate_charge_groups(eChargeGroup cgtp,t_atoms *atoms,
                            t_params *bonds,t_params *pols,
                            bool bUsePDBcharge,
                            real *qtot,real *mtot);

void sort_on_charge_groups(int *cgnr,t_atoms *atoms,t_params plist[],
                           rvec x[],t_excls excls[],
                           const char *ndxout,
                           int nmol);
#ifdef __cplusplus
}
#endif

#endif
