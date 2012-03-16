/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 * $Id: mp2csv.c,v 1.3 2009/06/01 06:13:18 spoel Exp $
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
 *
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "statutil.h"
#include "copyrite.h"
#include "smalloc.h"
#include "molprop.h"
#include "molprop_xml.h"
#include "molprop_util.h"
#include "atomprop.h"
#include "poldata_xml.h"

static void gmx_molprop_csv(const char *fn,int np,gmx_molprop_t mp[],
                            char *dip_str,char *pol_str,char *ener_str)
{
    FILE *fp;
    int i,j,k,ll;
    double d,err,vec[3];
    tensor quadrupole;
    char *ref;
#define NEMP 3
    int emp[NEMP] = { empDIPOLE, empPOLARIZABILITY, empENERGY  };
    char *ename[NEMP] = { "Dipole", "Polarizability", "Heat of formation" };
    t_qmcount *qmc[NEMP];
    
    qmc[0] = find_calculations(np,mp,emp[0],dip_str);
    qmc[1] = find_calculations(np,mp,emp[1],pol_str);
    qmc[2] = find_calculations(np,mp,emp[2],ener_str);
    for(k=0; (k<NEMP); k++) 
    {
        printf("--------------------------------------------------\n");
        printf("      Some statistics for %s\n",emp_name[emp[k]]);
        for(i=0; (i<qmc[k]->n); i++) 
            printf("There are %d calculation results using %s/%s type %s\n",
                   qmc[k]->count[i],qmc[k]->method[i],
                   qmc[k]->basis[i],qmc[k]->type[i]);
    }
    printf("--------------------------------------------------\n");

    fp = ffopen(fn,"w");
    fprintf(fp,"\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"",
            "Molecule","Formula","InChi","Charge","Multiplicity","Mass");
    for(k=0; (k<NEMP); k++)
    {
        for(j=0; (j<qmc[k]->n+2); j++) 
        {
            fprintf(fp,",\"%s\"",ename[k]);
        }
    }
    fprintf(fp,"\n");   
    for(ll=0; (ll<NEMP); ll++) 
    {
        fprintf(fp,"\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"","","","","","","");
        for(k=0; (k<3); k++) 
        {
            if (ll == 0)
                fprintf(fp,",\"Experiment\",\"Reference\"");
            else
                fprintf(fp,",\"\",\"\"");
            for(j=0; (j<qmc[k]->n); j++) 
            {
                switch(ll) {
                case 0:
                    fprintf(fp,",\"%s\"",qmc[k]->method[j]);
                    break;
                case 1:
                    fprintf(fp,",\"%s\"",qmc[k]->basis[j]);
                    break;
                case 2:
                    fprintf(fp,",\"%s\"",qmc[k]->type[j]);
                    break;
                default:
                    fprintf(stderr,"BOE\n");
                    exit(1);
                }
            }
        }
        fprintf(fp,"\n");
    }
    for(i=0; (i<np); i++) 
    {
        char *name;
        name = gmx_molprop_get_iupac(mp[i]);
        if (NULL == name)
            name = gmx_molprop_get_molname(mp[i]);
        fprintf(fp,"\"%s\",\"%s\",\"%s\",\"%d\",\"%d\",\"%g\"",
                name,
                gmx_molprop_get_formula(mp[i]),
                gmx_molprop_get_inchi(mp[i]),
                gmx_molprop_get_charge(mp[i]),
                gmx_molprop_get_multiplicity(mp[i]),
                gmx_molprop_get_mass(mp[i]));
        for(k=0; (k<NEMP); k++) 
        {
            if (mp_get_prop_ref(mp[i],emp[k],iqmExp,
                                NULL,NULL,NULL,&d,&err,&ref,NULL,vec,
                                quadrupole) == 1)
            {
                fprintf(fp,",\"%.4f\",\"%s\"",d,ref);
                sfree(ref);
            }
            else
                fprintf(fp,",\"\",\"\"");
            for(j=0; (j<qmc[k]->n); j++) 
            {
                if (mp_get_prop(mp[i],emp[k],iqmQM,qmc[k]->lot[j],NULL,qmc[k]->type[j],&d) == 1)
                    fprintf(fp,",\"%.4f\"",d);
                else
                    fprintf(fp,",\"\"");
            }
        }
        fprintf(fp,"\n");
    }
    fclose(fp);
}

int main(int argc,char*argv[])
{
    static const char *desc[] = {
        "mp2csv converts a molprop database into a spreadsheet"
    };
    t_filenm fnm[] = 
    {
        { efDAT, "-f",  "allmols",  ffREAD },
        { efDAT, "-o",  "csvout",   ffWRITE }
    };
    int NFILE = (sizeof(fnm)/sizeof(fnm[0]));
    static char *sort[] = { NULL, "molname", "formula", "composition", NULL };
    static char *dip_str = "",*pol_str = "",*ener_str = "";
    static gmx_bool bMerge = FALSE;
    t_pargs pa[] = 
    {
        { "-sort",   FALSE, etENUM, {sort},
          "Key to sort the final data file on." },
        { "-merge",  FALSE, etBOOL, {&bMerge},
          "Merge molecule records in the input file" },
        { "-dip_str", FALSE, etSTR, {&dip_str},
          "Selection of the dipole stuff you want in the tables, given as a single string with spaces like: method1/basis1/type1:method2/basis2/type2 (you may have to put quotes around the whole thing in order to prevent the shell from interpreting it)." },
        { "-pol_str", FALSE, etSTR, {&pol_str},
          "Same but for polarizabilities" },
        { "-ener_str", FALSE, etSTR, {&ener_str},
          "Same but for energies" }
    };
    int    i,alg,np,nspoel,nbosque,nhandbook,ntot,nqm,eMP,eprop;
    char   *mpname;
    double *fit2,*test2;
    int    cur = 0;
#define prev (1-cur)
    gmx_molprop_t *mp=NULL;
    gmx_atomprop_t ap;
    gmx_poldata_t  pd;
    output_env_t   oenv;
    
    CopyRight(stdout,argv[0]);
    
    parse_common_args(&argc,argv,PCA_NOEXIT_ON_ARGS,NFILE,fnm,
                      sizeof(pa)/sizeof(pa[0]),pa,
                      sizeof(desc)/sizeof(desc[0]),desc,
                      0,NULL,&oenv);
    mp = gmx_molprops_read(opt2fn("-f",NFILE,fnm),&np);
    ap = gmx_atomprop_init();
    
    gmx_molprop_sort(np,mp,empSORT_Composition,ap,NULL);
    
    gmx_molprop_csv(opt2fn("-o",NFILE,fnm),np,mp,
                    strlen(dip_str) > 0  ? dip_str : NULL,
                    strlen(pol_str) > 0  ? pol_str : NULL,
                    strlen(ener_str) > 0 ? ener_str : NULL);
    
    return 0;
}
