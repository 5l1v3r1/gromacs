/*
 * $Id$
 * 
 *                This source code is part of
 * 
 *                 G   R   O   M   A   C   S
 * 
 *          GROningen MAchine for Chemical Simulations
 * 
 *                        VERSION 3.0
 * 
 * Copyright (c) 1991-2001
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
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
 * Do check out http://www.gromacs.org , or mail us at gromacs@gromacs.org .
 * 
 * And Hey:
 * Gallium Rubidium Oxygen Manganese Argon Carbon Silicon
 */

#include <string.h>
#include <stdlib.h>
#include "sysstuff.h"
#include "princ.h"
#include "futil.h"
#include "statutil.h"
#include "vec.h"
#include "smalloc.h"
#include "typedefs.h"
#include "names.h"
#include "fatal.h"
#include "macros.h"
#include "rdgroup.h"
#include "symtab.h"
#include "index.h"
#include "confio.h"
#include "pull.h"
#include "pull_internal.h"
#include "string.h"


/*  None of the routines in this file check to see if we are
    doing parallel pulling. It is the responsibility of the 
    calling function to ensure that these routines only get
    called by the master node.
*/

#define MAX_PULL_GROUPS 4

void print_afm(t_pull *pull, int step, real t) 
{
  int i,j;
  
  /* Do we need to do output? */
  if (step % pull->nSkip) return;
  
  /* Print time */
  fprintf(pull->out, "%f\t", t);
  
  /* Print COM of reference group */
  for(j=0; j<DIM; ++j) {
    if(pull->dims[j] != 0.0) {
      fprintf(pull->out, "%f\t", pull->ref.x_unc[j]);
    }
  }
  
  /* Print out position of pulled groups */
  for (i=0;i<pull->ngrp;i++) {
    for (j=0;j<3;++j) {
      if (pull->dims[j] != 0.0) {
	fprintf(pull->out,"%f\t%f\t",
		pull->grp[i].x_unc[j],pull->grp[i].spring[j]);
      }
    }
  }
  fprintf(pull->out,"\n");
}

void print_constraint(t_pull *pull, rvec *f, int step, matrix box, int niter) 
{
  int i,ii,m; 
  dvec tmp,tmp2,tmp3;

  if (step % pull->nSkip) return;
  for(i=0;i<pull->ngrp;i++) {
    if (pull->bCyl)
      d_pbc_dx(box,pull->grp[i].x_con,pull->dyna[i].x_con,tmp);
    else
      d_pbc_dx(box,pull->grp[i].x_con,pull->ref.x_con,tmp);
    for(m=0; m<DIM; m++) {
      tmp[m] *= pull->dims[m];
    }
    if (pull->bVerbose)
      fprintf(pull->out,"%d:%d ds:%e f:%e n:%d\n", step,i,dnorm(tmp),
              pull->grp[i].f[ZZ],niter);
    else
      fprintf(pull->out,"%e ",pull->grp[i].f[ZZ]);
  }

  if (!pull->bVerbose)
    fprintf(pull->out,"\n");

  /* DEBUG */ /* this code doesn't correct for pbc, needs improvement */
  if (pull->bVerbose) {
    for(i=0;i<pull->ngrp;i++) {
      if (pull->bCyl)
        fprintf(pull->out,"eConstraint: step %d. Refgroup = dynamic (%f,%f\n"
                "Group %d (%s): ref. dist = %8.3f, unconstr. dist = %8.3f"
                " con. dist = %8.3f f_i = %8.3f\n", step, pull->r,pull->rc,
                i,pull->grp[i].name,
                pull->dyna[i].x_ref[ZZ]-pull->grp[i].x_ref[ZZ],
                pull->dyna[i].x_unc[ZZ]-pull->grp[i].x_unc[ZZ],
                pull->dyna[i].x_con[ZZ]-pull->grp[i].x_con[ZZ],
                pull->grp[i].f[ZZ]);
      else {
        dvec_sub(pull->ref.x_ref,pull->grp[i].x_ref,tmp);
        dvec_sub(pull->ref.x_unc,pull->grp[i].x_unc,tmp2);
        dvec_sub(pull->ref.x_con,pull->grp[i].x_con,tmp3);
        fprintf(stderr,"grp %d:ref (%8.3f,%8.3f,%8.3f) unc(%8.3f%8.3f%8.3f\n"
                "con (%8.3f%8.3f%8.3f)\n",i, tmp[0],tmp[1],tmp[2],
                tmp2[0],tmp2[1],tmp2[2],tmp3[0],tmp3[1],tmp3[2]);
      }
    }
  } /* END DEBUG */

}

void print_umbrella(t_pull *pull, int step, real t)
{
  int i,m;

  /* Do we need to do any pulling ? */
  if (step % pull->nSkip) return;

  fprintf(pull->out, "%f\t", t);
  
  /* Print deviation of pulled group from desired position */
  for (i=0; i<pull->ngrp; ++i) {    /* Loop over pulled groups */
    for (m=0; m<DIM; ++m) {             /* Loop over dimensions */
      if (pull->dims[m] != 0.0) {
	fprintf(pull->out,"%f\t",-pull->grp[i].spring[m]);
      }
    }
  }
  fprintf(pull->out,"\n");
}

static void init_pullgrp(t_pullgrp *pg,char *name,char *wbuf,real UmbCons)
{
  double d;
  int n;

  pg->name = strdup(name);
  pg->nweight = 0;
  while (sscanf(wbuf,"%lf %n",&d,&n) == 1) {
    if (pg->nweight == 0) {
      snew(pg->weight,1);
    } else {
      srenew(pg->weight,pg->nweight+1);
    }
    pg->weight[pg->nweight++] = d;
    wbuf += n;
  }
  pg->UmbCons = UmbCons;
}

static void string2dvec(char buf[], dvec nums)
{
  if (sscanf(buf,"%lf%lf%lf",&nums[0],&nums[1],&nums[2]) != 3)
    fatal_error(0,"Expected three numbers at input line %s",buf);
}

void read_pullparams(t_pull *pull, char *infile, char *outfile) 
{
  t_inpfile *inp;
  int ninp,i,nchar;
  char *tmp,*ptr;
  char dummy[STRLEN];
  char refbuf[STRLEN],grpbuf[MAX_PULL_GROUPS][STRLEN],
    refwbuf[STRLEN],wbuf[MAX_PULL_GROUPS][STRLEN],
    bf[MAX_PULL_GROUPS][STRLEN],
    pos[MAX_PULL_GROUPS][STRLEN],
    pulldim[STRLEN],pulldim1[STRLEN],condir[STRLEN];
  char DirTemp[MAX_PULL_GROUPS][STRLEN], InitTemp[MAX_PULL_GROUPS][STRLEN];
  real AfmRate[MAX_PULL_GROUPS],AfmK[MAX_PULL_GROUPS],UmbCons[MAX_PULL_GROUPS];

  int bReverse; int tmpref; int tmprun; 

  static const char *runtypes[ePullruntypeNR+1] = { 
    "afm", "constraint", "umbrella", NULL
  };
  enum {
    erefCom, erefComT0, erefDyn, erefDynT0, erefNR
  };
  static const char *reftypes[erefNR+1] = {
    "com", "com_t0", "dynamic", "dynamic_t0", NULL
  };
  enum {
    ereverseTO_REF, ereverseFROM_REF, ereverseNR
  };
  static const char *reversetypes[ereverseNR+1] = {
    "from_reference", "to_reference", NULL
  };
  enum {
    everboseYES, everboseNO, everboseNR
  };
  static const char *verbosetypes[erefNR+1] = {
    "no", "yes", NULL
  };
  int nerror = 0;

  /* read input parameter file */
  fprintf(stderr,"Reading parameter file %s\n",infile);
  inp=read_inpfile(infile,&ninp);

  /* general options */
  CTYPE("GENERAL");
  EETYPE("verbose",         pull->bVerbose, verbosetypes, &nerror, TRUE);
  ITYPE("Skip steps",       pull->nSkip,1);
  CTYPE("Runtype: afm, constraint, umbrella");
  EETYPE("runtype",         tmprun, runtypes, &nerror, TRUE);
  CTYPE("Groups to be pulled and the weights (default all 1)");
  STYPE("group_1",          grpbuf[0], "");
  STYPE("weights_1",        wbuf[0],   "");
  STYPE("group_2",          grpbuf[1], "");
  STYPE("weights_2",        wbuf[1],   "");
  STYPE("group_3",          grpbuf[2], "");
  STYPE("weights_3",        wbuf[2],   "");
  STYPE("group_4",          grpbuf[3], "");
  STYPE("weights_4",        wbuf[3],   "");
  CTYPE("The group for the reaction force.");
  STYPE("reference_group",  refbuf, "");
  STYPE("reference_weights", refwbuf, "");
  CTYPE("Ref. type: com, com_t0, dynamic, dynamic_t0");
  EETYPE("reftype",         tmpref, reftypes, &nerror, TRUE);
  CTYPE("Use running average for reflag steps for com calculation");
  ITYPE("reflag",           pull->reflag, 1);
  CTYPE("Select components for the pull vector. default: Y Y Y");
  STYPE("pulldim",          pulldim, "Y Y Y");

  /* options for dynamic reference groups */
  CTYPE("DYNAMIC REFERENCE GROUP OPTIONS");
  CTYPE("Cylinder radius for dynamic reaction force groups (nm)");
  RTYPE("r",                pull->r, 0.0);
  CTYPE("Switch from r to rc in case of dynamic reaction force");
  RTYPE("rc",   pull->rc,   0.0);
  CTYPE("Update frequency for dynamic reference groups (steps)");
  ITYPE("update",           pull->update, 1);
    

  /* constraint run options */
  CCTYPE("CONSTRAINT RUN OPTIONS");
  CTYPE("Direction, default: 0 0 0, no direction");
  STYPE("constraint_direction",        condir, "0.0 0.0 0");
  CTYPE("Rate of chance of the constraint length, in nm/ps");
  RTYPE("constraint_rate",    pull->constr_rate, 0);
  CTYPE("Tolerance of constraints, in nm");
  RTYPE("constraint_tolerance",            pull->constr_tol, 1E-6);

   /* options for AFM type pulling simulations */
  CCTYPE("AFM OPTIONS");
  CTYPE("Pull rates in nm/ps");
  RTYPE("afm_rate1",         AfmRate[0],    0.0);
  RTYPE("afm_rate2",         AfmRate[1],    0.0);
  RTYPE("afm_rate3",         AfmRate[2],    0.0);
  RTYPE("afm_rate4",         AfmRate[3],    0.0);
  CTYPE("Force constants in kJ/(mol*nm^2)");
  RTYPE("afm_k1",            AfmK[0], 0.0);
  RTYPE("afm_k2",            AfmK[1], 0.0);
  RTYPE("afm_k3",            AfmK[2], 0.0);
  RTYPE("afm_k4",            AfmK[3], 0.0);
  CTYPE("Directions");
  STYPE("afm_dir1",              DirTemp[0], "0.0 0.0 1.0");
  STYPE("afm_dir2",              DirTemp[1], "0.0 0.0 1.0");
  STYPE("afm_dir3",              DirTemp[2], "0.0 0.0 1.0");
  STYPE("afm_dir4",              DirTemp[3], "0.0 0.0 1.0");
  CTYPE("Initial spring positions");
  STYPE("afm_init1",              InitTemp[0], "0.0 0.0 0.0");
  STYPE("afm_init2",              InitTemp[1], "0.0 0.0 0.0");
  STYPE("afm_init3",              InitTemp[2], "0.0 0.0 0.0");
  STYPE("afm_init4",              InitTemp[3], "0.0 0.0 0.0");
  
  /* umbrella sampling options */
  CCTYPE("UMBRELLA SAMPLING OPTIONS");
  CTYPE("Force constants for umbrella sampling in kJ/(mol*nm^2)");
  CTYPE("Centers of umbrella potentials with respect to reference:");
  CTYPE("Ref - Pull.");
  RTYPE("K1",            UmbCons[0], 0.0);
  STYPE("Pos1",pos[0],"0.0 0.0 0.0");
  RTYPE("K2",            UmbCons[1], 0.0);
  STYPE("Pos2",pos[1],"0.0 0.0 0.0");
  RTYPE("K3",            UmbCons[2], 0.0);
  STYPE("Pos3",pos[2],"0.0 0.0 0.0");
  RTYPE("K4",            UmbCons[3], 0.0);
  STYPE("Pos4",pos[3],"0.0 0.0 0.0");

  write_inpfile(outfile,ninp,inp);
  for(i=0; (i<ninp); i++) {
    sfree(inp[i].name);
    sfree(inp[i].value);
  }
  sfree(inp);

  pull->runtype = (t_pullruntype)tmprun;
  pull->reftype = (t_pullreftype)tmpref;

  /* sort out the groups */
  fprintf(stderr,"Groups: %s %s %s %s %s\n",
          grpbuf[0],grpbuf[1],grpbuf[2],grpbuf[3],refbuf);

  if (!strcmp(refbuf, "")) {
    if (pull->runtype == eConstraint)
      fatal_error(0, "Constraint forces require a reference group to be specified.\n");
    pull->AbsoluteRef = TRUE;
    fprintf(stderr, "Pull code using absolute reference.\n");
  }
  
  if (!strcmp(grpbuf[0],""))
    fatal_error(0,"Need to specify at least group_1.");
  pull->ngrp = 1;
  while (i<MAX_PULL_GROUPS && strcmp(grpbuf[pull->ngrp],""))
    pull->ngrp++;
  
  fprintf(stderr,"Using %d pull groups\n",pull->ngrp);
  
  /* Make the pull groups */
  init_pullgrp(&pull->ref,refbuf,refwbuf,0);
  snew(pull->grp,pull->ngrp);
  for(i=0; i<pull->ngrp; i++)
    init_pullgrp(&pull->grp[i],grpbuf[i],wbuf[i],UmbCons[i]);
  
  if (pull->runtype == eAfm) {
    for (i=0; i<pull->ngrp; ++i) {
      pull->grp[i].AfmRate = AfmRate[i];
      pull->grp[i].AfmK = AfmK[i];
      string2dvec(DirTemp[i], pull->grp[i].AfmVec);
      string2dvec(InitTemp[i], pull->grp[i].AfmInit);
    }
  }

  if(pull->runtype == eUmbrella) {
    if(! (pull->reftype==erefCom || pull->reftype==erefComT0))
      fatal_error(0,"Umbrella sampling currently only works with COM and COM_T0 reftypes");
    for(i=0;i<pull->ngrp;++i) {
      string2dvec(pos[i],pull->grp[i].UmbPos);
    }
  }
  /* End Modification */

  ptr = pulldim;
  for(i=0; i<DIM; i++) {
    if (sscanf(ptr,"%s%n",pulldim1,&nchar) != 1)
      fatal_error(0,"Less than 3 pull dimensions given in pulldim: '%s'",
		  pulldim);
    
    if (strncasecmp(pulldim1,"N",1) == 0) {
      pull->dims[i] = 0;
    } else if (strncasecmp(pulldim1,"Y",1) == 0) {
      pull->dims[i] = 1;
    } else {
      fatal_error(0,"Please use Y(ES) or N(O) for pulldim only (not %s)",
		  pulldim1);
    }
    ptr += nchar;
  }

  fprintf(stderr,"Using distance components %d %d %d\n",
	  pull->dims[0],pull->dims[1],pull->dims[2]);

  string2dvec(condir,pull->dir);
  if (pull->runtype != eConstraint ||
      (pull->dir[XX] == 0 && pull->dir[YY] == 0 && pull->dir[ZZ] == 0)) {
    pull->bDir = FALSE;
    if (pull->runtype == eConstraint)
      fprintf(stderr,"Not using directional constraining");
  } else {
    pull->bDir = TRUE;
    dsvmul(1/dnorm(pull->dir),pull->dir,pull->dir);
    fprintf(stderr,"Using directional constraining %5.2f %5.2f %5.2f\n",
    pull->dir[XX],pull->dir[YY],pull->dir[ZZ]);
  }

  if(pull->r > 0.001)
    pull->bCyl = TRUE;
  else
    pull->bCyl = FALSE;
}

void print_pull_header(t_pull * pull)
{
  /* print header information */
  int i,j;

  if(pull->runtype==eUmbrella)
    fprintf(pull->out,"# UMBRELLA\t3.0\n");
  else if(pull->runtype==eAfm)
    fprintf(pull->out,"# AFM\t3.0\n");
  else if(pull->runtype==eConstraint)
    fprintf(pull->out,"# CONSTRAINT\t3.0\n");

  fprintf(pull->out,"# Component selection:");
  for(i=0;i<3;++i) {
    fprintf(pull->out," %d",pull->dims[i]);
  }
  fprintf(pull->out,"\n");

  fprintf(pull->out,"# nSkip %d\n",pull->nSkip);

  fprintf(pull->out,"# Ref. Group '%s'\n",pull->ref.name);

  fprintf(pull->out,"# Nr. of pull groups %d\n",pull->ngrp);
  for(i=0;i<pull->ngrp;i++) {
    fprintf(pull->out,"# Group %d '%s'",i+1,pull->grp[i].name);

    if (pull->runtype == eAfm) {
      fprintf(pull->out, "  afmVec %f %f %f  AfmRate %f  AfmK %f",
	      pull->grp[i].AfmVec[XX],
	      pull->grp[i].AfmVec[YY],
	      pull->grp[i].AfmVec[ZZ],
	      pull->grp[i].AfmRate,
	      pull->grp[i].AfmK);
    } else if (pull->runtype == eUmbrella) {
      fprintf(pull->out,"  Umb. Pos.");
      for (j=0;j<3;++j) {
	if (pull->dims[j] != 0)
	  fprintf(pull->out," %f",pull->grp[i].UmbPos[j]);
      }
      fprintf(pull->out,"  Umb. Cons.");
      for (j=0;j<3;++j) {
	if (pull->dims[j] != 0)
	  fprintf(pull->out," %f",pull->grp[i].UmbCons);
      }
    } else if (pull->runtype == eConstraint) {
      fprintf(pull->out,"  Pos.");
      for (j=0;j<3;++j) {
	if (pull->dims[j] != 0)
	  fprintf(pull->out," %f",pull->grp[i].x_unc[j]);
      }
    }
    fprintf(pull->out,"\n");
  }
  fprintf(pull->out,"#####\n");

}
