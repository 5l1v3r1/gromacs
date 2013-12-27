/*
 * $Id: molprop_tables.c,v 1.21 2009/05/29 15:01:18 spoel Exp $
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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "maths.h"
#include "gromacs/fileio/futil.h"
#include "smalloc.h"
#include "string2.h"
#include "vec.h"
#include "xvgr.h"
#include "statutil.h"
#include "copyrite.h"
#include "gmx_statistics.h"
#include "gmx_fatal.h"
#include "gromacs/linearalgebra/matrix.h"
#include "poldata.hpp"
#include "poldata_xml.hpp"
#include "molselect.hpp"
#include "molprop.hpp"
#include "molprop_util.hpp"
#include "molprop_tables.hpp"

static const char *SPOEL = "spoel";

typedef struct 
{
    char *cat;
    bool bPrint;
    int count;
    real rms;
} t_catli;

static int comp_catli(const void *a,const void *b)
{
    t_catli *ca = (t_catli *)a;
    t_catli *cb = (t_catli *)b;
    
    return strcmp(ca->cat,cb->cat);
}

typedef struct 
{
    int ncat;
    t_catli *catli;
} t_cats;

static void add_cat(t_cats *cats,const char *catname)
{
    int i;
  
    for(i=0; (i<cats->ncat); i++) 
        if (strcasecmp(catname,cats->catli[i].cat) == 0) 
        {
            cats->catli[i].count++;
            break;
        }
    if (i == cats->ncat) 
    {
        cats->ncat++;
        srenew(cats->catli,cats->ncat);
        cats->catli[cats->ncat-1].cat = strdup(catname);
        cats->catli[cats->ncat-1].count = 1;
        cats->catli[cats->ncat-1].rms = 0;
    }
}

static void decrease_table_number(FILE *fp)
{
    fprintf(fp,"\\addtocounter{table}{-1}\n");
}

static void stats_footer(FILE *fp,bool bSideways)
{
    fprintf(fp,"\\end{longtable}\n");
    if (bSideways)
    {
        fprintf(fp, "\\end{landscape}\n");
    }
}

static void stats_header(FILE *fp,MolPropObservable mpo,
                         bool bLandscape,
                         t_qmcount *qmc,iMolSelect ims)
{
    char unit[32];
    int  i;
    
    if (bLandscape)
    {
        fprintf(fp, "\\begin{landscape}\n");
    }    
    fprintf(fp,"\\begin{longtable}{l");
    for(i=0; (i<qmc->n); i++)
    {
        fprintf(fp,"c");
    }
    fprintf(fp,"}\n\\hline\n");
    for(int k=0; (k<2); k++)
    {
        if (0 ==k) 
        {
            switch (mpo) 
            {
            case MPO_POLARIZABILITY:
                sprintf(unit,"\\AA$^3$");
                break;
            case MPO_DIPOLE:
                sprintf(unit,"D");
                break;
            case MPO_QUADRUPOLE:
                sprintf(unit,"B");
                break;
            case MPO_ENERGY:
                sprintf(unit,"kJ/mol");
                break;
            default:
                gmx_fatal(FARGS,"Unknown property %s",mpo_name[mpo]);
            }
            fprintf(fp,"\\caption{Performance of the different methods for predicting the molecular %s for molecules containing different chemical groups, given as the RMSD from experimental values (%s), and in brackets the number of molecules in this particular subset. {\\bf Data set: %s.} At the bottom the correlation coefficient R, the regression coefficient a and the intercept b are given as well as the normalized quality of the fit $\\chi^2$.}\n\\label{%s_rmsd}\\\\\n",
                    mpo_name[mpo],unit,ims_names[ims],mpo_name[mpo]);
            fprintf(fp, "\\hline\n");
        }
        else 
        {
            fprintf(fp, "\\hline\n");
        }
        
        fprintf(fp,"Method ");
        for(i=0; (i<qmc->n); i++) 
        {
            fprintf(fp," & %s",qmc->method[i]);
        }
        fprintf(fp,"\\\\\n");
        
        for(i=0; (i<qmc->n); i++) 
            fprintf(fp,"& %s ",qmc->basis[i]);
        fprintf(fp,"\\\\\n");
        
        for(i=0; (i<qmc->n); i++) 
            fprintf(fp,"& %s ",qmc->type[i]);
        fprintf(fp,"\\\\\n\\hline\n");
        if (0 == k)
        {
            fprintf(fp, "\\endfirsthead\n");
        }
        else
        {
            fprintf(fp, "\\endhead\n\\hline\n\\endfoot\n");
        }
    }
}

void gmx_molprop_stats_table(FILE *fp,MolPropObservable mpo,
                             std::vector<alexandria::MolProp> mp,
                             t_qmcount *qmc,char *exp_type,
                             double outlier,gmx_molselect_t gms,iMolSelect ims)
{
    std::vector<alexandria::MolProp>::iterator mpi;
    std::vector<std::string>::iterator si;
    int    i,line,k,m,N,nprint,nqmres,nexpres;
    double exp_val,qm_val;
    real   rms,R,a,da,b,db,chi2;
    char   lbuf[256],tmp[32],catbuf[STRLEN];
    t_cats *cats;
    gmx_stats_t lsq,*lsqtot;
    bool   bSideways = (qmc->n > 3);

    nprint = 0;
    snew(cats,1);
    for(mpi=mp.begin(); (mpi<mp.end()); mpi++) 
    {
        if ((ims == gmx_molselect_status(gms,mpi->GetIupac().c_str())) &&
            mpi->HasComposition(SPOEL))
        {
            nprint++;
            for(si=mpi->BeginCategory(); (si<mpi->EndCategory()); si++) 
            {
                add_cat(cats,si->c_str());
            }
        }
    }
    if (nprint <= 0)
    {
        sfree(cats);
        return;
    }
    
    stats_header(fp,mpo,bSideways,qmc,ims);
    
    qsort(cats->catli,cats->ncat,sizeof(cats->catli[0]),comp_catli);
    line = 0;
    for(i=0; (i<cats->ncat); i++) 
    {
        sprintf(catbuf," %s (%d) ",cats->catli[i].cat,cats->catli[i].count);
        nqmres = 0;
        nexpres = 0;
        for(k=0; (k<qmc->n); k++) 
        {
            sprintf(lbuf,"%s/%s",qmc->method[k],qmc->basis[k]);
            lsq = gmx_stats_init();
            for(mpi=mp.begin(); (mpi<mp.end()); mpi++) 
            {
                if ((gmx_molselect_status(gms,mpi->GetIupac().c_str()) == ims) &&
                    (mpi->HasComposition(SPOEL)))
                {
                    if (mpi->SearchCategory(cats->catli[i].cat) == 1)
                    {
                        if (mpi->GetProp(mpo,iqmExp,NULL,
                                         NULL/*qmc->conf[m]*/,exp_type,&exp_val))
                        {
                            nexpres = 1;
                            if (mpi->GetProp(mpo,iqmQM,lbuf,NULL/*qmc->conf[m]*/,
                                             qmc->type[k],&qm_val))
                            {
                                if (debug)
                                {
                                    fprintf(debug,"%s %s k = %d - TAB4\n",
                                            mpi->GetMolname().c_str(),cats->catli[i].cat,k);
                                }
                                gmx_stats_add_point(lsq,exp_val,qm_val,0,0);
                            }
                        }
                    }
                }
            }
            if (outlier > 0)
            {
                gmx_stats_remove_outliers(lsq,outlier);
            }
            if (gmx_stats_get_rmsd(lsq,&rms) == estatsOK)
            { 
                sprintf(tmp,"& %8.2f",rms);
                strncat(catbuf,tmp,STRLEN-1);
                nqmres++;
            }
            else
            {
                strncat(catbuf,"& -",STRLEN-1);
            }
            
            if (gmx_stats_done(lsq) == estatsOK)
            {
                sfree(lsq);
            }
        }
        cats->catli[i].bPrint = ((nqmres > 0) && (nexpres > 0));
        if (cats->catli[i].bPrint)
        {
            strncat(catbuf,"\\\\\n",STRLEN-1);
            fprintf(fp,"%s",catbuf);
            line++;
        }
    }
    fprintf(fp,"All");
    snew(lsqtot,qmc->n);
    for(k=0; (k<qmc->n); k++) 
    {
        sprintf(lbuf,"%s/%s",qmc->method[k],qmc->basis[k]);
        lsqtot[k] = gmx_stats_init();
        for(mpi=mp.begin(); (mpi<mp.end()); mpi++) 
        {
            if ((gmx_molselect_status(gms,mpi->GetIupac().c_str()) == ims) &&
                (mpi->HasComposition(SPOEL)))
            {
                for(m=0; (m<qmc->nconf); m++) 
                {
                    if ((mpi->GetProp(mpo,iqmExp,NULL,qmc->conf[m],exp_type,&exp_val)) &&
                        (mpi->GetProp(mpo,iqmQM,lbuf,qmc->conf[m],qmc->type[k],&qm_val))) 
                    {
                        gmx_stats_add_point(lsqtot[k],exp_val,qm_val,0,0);
                    }
                }
            }
        }
        if (gmx_stats_get_rmsd(lsqtot[k],&rms) == estatsOK)
        {
            fprintf(fp,"& %8.2f",rms);
        }
        else
        {
            fprintf(fp,"& -");
        }
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"\\hline\n");
    fprintf(fp,"N");
    for(k=0; (k<qmc->n); k++) 
    {
        /* gmx_stats_remove_outliers(lsqtot[k],3); */
        gmx_stats_get_npoints(lsqtot[k],&N);
        fprintf(fp,"& %d",N);
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"a");
    for(k=0; (k<qmc->n); k++) 
    {
        if (gmx_stats_get_ab(lsqtot[k],elsqWEIGHT_NONE,&a,&b,&da,&db,&chi2,&R) == 
            estatsOK) 
            fprintf(fp,"& %8.2f(%4.2f)",a,da);
        
        else
            fprintf(fp,"& -");
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"b");
    for(k=0; (k<qmc->n); k++) 
    {
        if (gmx_stats_get_ab(lsqtot[k],elsqWEIGHT_NONE,&a,&b,&da,&db,&chi2,&R) ==
            estatsOK)
            fprintf(fp,"& %8.2f(%4.2f)",b,db);
        else
            fprintf(fp,"& -");
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"R (\\%%)");
    for(k=0; (k<qmc->n); k++) 
    {
        if (gmx_stats_get_corr_coeff(lsqtot[k],&R) == estatsOK)
            fprintf(fp,"& %8.2f",100*R);
        else
            fprintf(fp,"& -");
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"$\\chi^2$");
    for(k=0; (k<qmc->n); k++) 
    {
        if (gmx_stats_get_ab(lsqtot[k],elsqWEIGHT_NONE,&a,&b,&da,&db,&chi2,&R) ==
            estatsOK)
        {
            fprintf(fp,"& %8.2f",chi2);
        }
        else
        {
            fprintf(fp,"& -");
        }
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"MSE");
    for(k=0; (k<qmc->n); k++) 
    {
        real mse;
        if (gmx_stats_get_mse_mae(lsqtot[k],&mse,NULL) ==
            estatsOK)
        {
            fprintf(fp,"& %8.2f", mse);
        }
        else
        {
            fprintf(fp,"& -");
        }
    }
    fprintf(fp,"\\\\\n");
    fprintf(fp,"MAE");
    for(k=0; (k<qmc->n); k++) 
    {
        real mae;
        if (gmx_stats_get_mse_mae(lsqtot[k],NULL,&mae) ==
            estatsOK)
        {
            fprintf(fp,"& %8.2f", mae);
        }
        else
        {
            fprintf(fp,"& -");
        }
    }
    fprintf(fp,"\\\\\n");
    stats_footer(fp,bSideways);
    
    for(k=0; (k<qmc->n); k++) 
    {
        if (gmx_stats_done(lsqtot[k]) == estatsOK)
        {
            sfree(lsqtot[k]);
        }
    }
    sfree(lsqtot);
}
    
static void composition_header(FILE *fp,int caption,iMolSelect ims)
{
    if (!caption)
        decrease_table_number(fp);
    fprintf(fp,"\\begin{sidewaystable}[H]\n\\centering\n");
    if (caption)
        fprintf(fp,"\\caption{Decomposition of molecules into Alexandria atom types. {\\bf Data set: %s.} Charge is given when not zero, multiplicity is given when not 1.}\n\\label{frag_defs}\n",ims_names[ims]);
    else 
        fprintf(fp,"\\caption{Continued}\n");
    fprintf(fp,"\\begin{tabular}{lll}\n\\hline\n");
    fprintf(fp,"Molecule & Formula  & Types \\\\\n");
    fprintf(fp,"\\hline\n");
}

void gmx_molprop_composition_table(FILE *fp,std::vector<alexandria::MolProp> mp,
                                   gmx_molselect_t gms,iMolSelect ims)
{
    std::vector<alexandria::MolProp>::iterator mpi;
    alexandria::MolecularCompositionIterator mci;
    
    int k,q,m,iline,nprint;
    char qbuf[32];
    const char *comps[2] = { "spoel", "miller" };
    int  kmax = 1;
    
    nprint = 0;
    for(mpi=mp.begin(); (mpi<mp.end()); mpi++)
    {
        if ((ims == gmx_molselect_status(gms,mpi->GetIupac().c_str())) &&
            (mpi->HasComposition(SPOEL)))
            nprint++;
    }
    if (nprint <= 0)
        return;

    composition_header(fp,1,ims);  
    iline = 0;
    for(mpi=mp.begin(); (mpi<mp.end()); mpi++)
    {
        if ((ims == gmx_molselect_status(gms,mpi->GetIupac().c_str())) &&
            (mpi->HasComposition(SPOEL)))
        {
            q = mpi->GetCharge();
            m = mpi->GetMultiplicity();
            if ((q != 0) || (m != 1))
            {
                if ((q != 0) && (m != 1)) 
                    sprintf(qbuf," (q=%c%d, mult=%d)",(q < 0) ? '-' : '+',abs(q),m);
                else if (q != 0)
                    sprintf(qbuf," (q=%c%d)",(q < 0) ? '-' : '+',abs(q));
                else
                    sprintf(qbuf," (mult=%d)",m);
            }
            else
            {
                qbuf[0] = '\0';
            }
            fprintf(fp,"%3d. %s%s & %s &",++iline,
                    mpi->GetIupac().c_str(),qbuf,
                    mpi->GetTexFormula().c_str());
            for(k=0; (k<kmax); k++) 
            {
                mci = mpi->SearchMolecularComposition(comps[k]);
                if (mci != mpi->EndMolecularComposition())
                {
                    alexandria::AtomNumIterator ani;
                    
                    for(ani=mci->BeginAtomNum(); (ani<mci->EndAtomNum()); ani++)
                    {
                        fprintf(fp,"%d%s\t",ani->GetNumber(),ani->GetAtom().c_str());
                    }
                }
                if (k == kmax-1)
                    fprintf(fp," \\\\\n");
                else
                    fprintf(fp," &");
            }
            if (((iline % 28) == 0) && (mpi<mp.end()-1)) 
            {
                fprintf(fp,"\\hline\n\\end{tabular}\n\\end{sidewaystable}\n");
                composition_header(fp,0,ims);
            }
        }
    }
    fprintf(fp,"\\hline\n\\end{tabular}\n\\end{sidewaystable}\n");
}

typedef struct {
    char *cat;
    int nmol;
    char **molec;
} t_cat_stat;

static int comp_cats(const void *a,const void *b)
{
    t_cat_stat *ca = (t_cat_stat *)a;
    t_cat_stat *cb = (t_cat_stat *)b;
    
    return strcmp(ca->cat,cb->cat);
}

static void add_cats(int *ncs,t_cat_stat **cs,const char *iupac,const char *mycat)
{
    int j,nmol;

    for(j=0; (j<*ncs); j++)
    {
        if (strcmp((*cs)[j].cat,mycat) == 0)
            break;
    }
    if (j == *ncs) 
    {
        srenew(*cs,++(*ncs));
        (*cs)[j].cat = strdup(mycat);
        (*cs)[j].nmol = 0;
        (*cs)[j].molec = NULL;
    }
    nmol = ++(*cs)[j].nmol;
    srenew((*cs)[j].molec,nmol);
    (*cs)[j].molec[nmol-1] = strdup(iupac);
}

static void category_header(FILE *fp)
{
    fprintf(fp,"\\begin{longtable}{lcc}\n");
    for(int k = 0; (k<2); k++)
    {
        if (0 == k)
        {
            fprintf(fp,"\\caption{Molecules that are part of each category used for statistics.}\\label{stats}\\\\\n");
        }
        else
        {
            fprintf(fp, "\\hline\n");
        }
        fprintf(fp,"Category & N & Molecule(s)\\\\\n");
        fprintf(fp,"\\hline\n");
        if (0 == k)
        {
            fprintf(fp, "\\endfirsthead\n");
        }
        else
        {
            fprintf(fp, "\\endhead\n\\hline\n\\endfoot\n");
        }
    }
}

static void category_footer(FILE *fp)
{
    fprintf(fp,"\\end{longtable}\n");
}

void gmx_molprop_category_table(FILE *fp,
                                std::vector<alexandria::MolProp> mp,
                                gmx_molselect_t gms,iMolSelect ims)
{
    std::vector<alexandria::MolProp>::iterator mpi;
    std::vector<std::string>::iterator si;
    int i,j,iline,ncs,prmax=50;
    t_cat_stat *cs=NULL;
    const char *iupac;
    
    ncs = 0;
    for(mpi=mp.begin(); (mpi<mp.end()); mpi++)
    {
        iupac = mpi->GetIupac().c_str();
        if (ims == gmx_molselect_status(gms,iupac))
        {
            for(si=mpi->BeginCategory(); (si<mpi->EndCategory()); si++)
            {
                add_cats(&ncs,&cs,iupac,si->c_str());
            }
        }
    }
    
    if (ncs > 0) 
    {
        qsort(cs,ncs,sizeof(cs[0]),comp_cats);
        category_header(fp);  
        iline = 0;
        for(i=0; (i<ncs); i++) 
        {
            fprintf(fp,"%s & %d &",cs[i].cat,cs[i].nmol);
            for(j=0; (j<cs[i].nmol); j++) 
            {
                fprintf(fp," %s",cs[i].molec[j]);
                iline++;
                if (j<cs[i].nmol-1)
                {
                    if (iline >= prmax)
                    {
                        fprintf(fp,"\\\\\n");
                        fprintf(fp,"%s & (ctd) &",cs[i].cat);
                        iline = 0;
                    }
                    else 
                    {
                        fprintf(fp,",");
                    }
                }
            }
            if (iline != 0)
                fprintf(fp,"\\\\\n");
        }
        category_footer(fp);
    }
}

static void atomtype_tab_header(FILE *fp)
{
    fprintf(fp,"\\begin{longtable}{cccccccc}\n");
    for(int k = 0; (k < 2); k++)
    {
        if (0 == k)
        {
            fprintf(fp,"\\caption{Definition of atom types. The number of occurences  of atom N$_{Exp}$ and N$_{QM}$ indicate how many of the polarizability values were fitted to experimental data or quantum chemistry, respectively. The columns Ahc and Ahp contain group polarizabilites computed using Miller's equation~\\protect\\cite{Miller1979a} and parameters~\\protect\\cite{Miller1990a} and Kang and Jhon's method~\\cite{Kang1982a} with the parametrization of Miller~\\protect\\cite{Miller1990a} respectively. The column BS contains the equivalent using the polarizabilities of Bosque and Sales~\\protect\\cite{Bosque2002a}.}\n\\label{fragments}\\\\\n");
        }
        else 
        {
            fprintf(fp, "\\hline\n");
        }
        fprintf(fp,"Name  & N$_{Exp}$ & N$_{QM}$ & \\multicolumn{4}{c}{Polarizability} & Ref.\\\\\n");
        fprintf(fp,"& & & Ahc & Ahp & BS & AX ($\\sigma_{AX}$) & \\\\\n");
        fprintf(fp,"\\hline\n");
        if (0 == k)
        {
            fprintf(fp, "\\endfirsthead\n");
        }
        else
        {
            fprintf(fp, "\\endhead\n");
            fprintf(fp, "\\hline\n");
            fprintf(fp, "\\endfoot\n");
        }
    }
}

static void atomtype_tab_end(FILE *fp)
{
    fprintf(fp,"\\end{longtable}\n\n");
}

typedef struct {
    char *ptype,*miller,*bosque;
    gmx_stats_t lsq,nexp,nqm;
} t_sm_lsq;
    
static void gmx_molprop_atomtype_polar_table(FILE *fp,int npd,gmx_poldata_t pd[],
                                             gmx_poldata_t pd_aver,
                                             std::vector<alexandria::MolProp> mp,
                                             char *lot,char *exp_type,
                                             output_env_t oenv,const char *histo)
{
    std::vector<alexandria::MolProp>::iterator mpi;
    FILE   *xvg;
    int    i,j,pp,ntab;
    double ahc,ahp,bos_pol,alexandria_pol,sig_pol;
    double exp_val,qm_val;
    real   alexandria_aver,alexandria_sigma,nnn;
    char   *ptype[2] = { NULL, NULL };
    char   *ref;
    int    nsmlsq=0,estats,N,nset;
    t_sm_lsq *smlsq=NULL;
    int    cur = 0;
    MolPropObservable mpo=MPO_POLARIZABILITY;
#define prev (1-cur)

    /* First gather statistics from different input files. Note that the input files
     * do not need to have the same sets of types, as we check for the type name.
     */
    for(pp = 0; (pp<npd); pp++) 
    {
        while(1 == gmx_poldata_get_ptype(pd[pp],
                                         &(ptype[cur]),
                                         NULL,
                                         NULL,
                                         &alexandria_pol,
                                         &sig_pol))
        {
            if (((NULL == ptype[prev]) || (strcmp(ptype[cur],ptype[prev]) != 0)) &&
                (alexandria_pol > 0))
            {
                for(j=0; (j<nsmlsq); j++)
                {
                    if (strcasecmp(ptype[cur],smlsq[j].ptype) == 0)
                    {
                        break;
                    }
                }
                if (j == nsmlsq) 
                {
                    srenew(smlsq,++nsmlsq);
                    smlsq[j].ptype = strdup(ptype[cur]);
                    smlsq[j].lsq = gmx_stats_init();
                    smlsq[j].nexp = gmx_stats_init();
                    smlsq[j].nqm = gmx_stats_init();
                }
                if ((estats = gmx_stats_get_npoints(smlsq[j].lsq,&N)) != estatsOK)
                {
                    gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                }
                if ((estats = gmx_stats_add_point(smlsq[j].lsq,N,alexandria_pol,0,0)) != estatsOK)
                {
                    gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                }
                for(mpi=mp.begin(); (mpi<mp.end()); mpi++) 
                {
                    alexandria::MolecularCompositionIterator mci = mpi->SearchMolecularComposition(SPOEL);
                    
                    if (mci != mpi->EndMolecularComposition())
                    {
                        for(alexandria::AtomNumIterator ani=mci->BeginAtomNum(); (ani<mci->EndAtomNum()); ani++)
                        {
                            const char *pt = gmx_poldata_atype_to_ptype(pd[pp],
                                                                        ani->GetAtom().c_str());
                            if ((NULL != pt) && (strcasecmp(pt,ptype[cur]) == 0)) 
                            {
                                int ngt = ani->GetNumber();
                                if (mpi->GetProp(mpo,iqmExp,lot,NULL,exp_type,&exp_val))
                                {
                                    int N;
                                    if ((estats = gmx_stats_get_npoints(smlsq[j].nexp,&N)) != estatsOK)
                                    {
                                        gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                                    }
                                    if ((estats = gmx_stats_add_point(smlsq[j].nexp,N,ngt,0,0)) != estatsOK)
                                    {
                                        gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                                    }
                                }
                                else if (mpi->GetProp(mpo,iqmQM,lot,NULL,NULL,&qm_val))
                                {
                                    int N;
                                    if ((estats = gmx_stats_get_npoints(smlsq[j].nqm,&N)) != estatsOK)
                                    {
                                        gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                                    }
                                    if ((estats = gmx_stats_add_point(smlsq[j].nqm,N,ngt,0,0)) != estatsOK)
                                    {
                                        gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
                                    }
                                }
                            }
                        }
                    }
                }
                cur = prev;
            }
        }
    }
    if (NULL != histo)
    {
        nset = 0;
        xvg = xvgropen(histo,"Distribution of polarizabilities","Polarizability","a.u.",oenv);
        for(j = 0; (j<nsmlsq); j++) 
        {
            if ((gmx_stats_get_npoints(smlsq[j].lsq,&N) == estatsOK) && (N > 0))
            {
                fprintf(xvg,"@ s%d legend \"%s\"\n",nset++,smlsq[j].ptype);
            }
        }
    }
    else
        xvg = NULL;
        
    /* Now print it! */
    atomtype_tab_header(fp);
    nset = 0;
    ntab = 0;
    ref = gmx_poldata_get_polar_ref(pd[0]);
    for(j = 0; (j<nsmlsq); j++) 
    {
        /* Determine Miller and Bosque polarizabilities for this Spoel element */
        ahc = ahp = bos_pol = 0;
        /* if (1 == gmx_poldata_get_miller_pol(pd[0],smlsq[j].miller,
                                            &atomnumber,&ahc,&ahp)) 
        {
            ahc = (4.0/atomnumber)*sqr(ahc);
        }
        if (0 == gmx_poldata_get_bosque_pol(pd[0],smlsq[j].bosque,&bos_pol))
            bos_pol = 0;
        */     
        /* Compute how many molecules contributed to the optimization 
         * of this polarizability.
         */
        /* Construct group name from element composition */
        /* strncpy(group,smlsq[j].bosque,sizeof(group));*/
        
        if (estatsOK != (estats = gmx_stats_get_npoints(smlsq[j].lsq,&N)))
        {
            gmx_fatal(FARGS,"Statistics problems: %s",gmx_stats_message(estats));
        }
        if (estatsOK != (estats = gmx_stats_get_average(smlsq[j].lsq,&alexandria_aver)))
        {
            gmx_fatal(FARGS,"Statistics problem: %s. gt_type = %s. N = %d.",
                      gmx_stats_message(estats),smlsq[j].ptype,N);
        }
        if (estatsOK != (estats = gmx_stats_get_sigma(smlsq[j].lsq,&alexandria_sigma)))
        {
            gmx_fatal(FARGS,"Statistics problem: %s. gt_type = %s. N = %d.",
                      gmx_stats_message(estats),smlsq[j].ptype,N);
        }
        /* Store average values */
        if (NULL != pd_aver) 
        {
            gmx_poldata_set_ptype_polarizability(pd_aver,smlsq[j].ptype,
                                                 alexandria_aver,alexandria_sigma);
        }
        int nfitexp=0;
        if (estatsOK != (estats = gmx_stats_get_npoints(smlsq[j].nexp,&nfitexp)))
        {
            gmx_fatal(FARGS,"Statistics problem: %s. gt_type = %s. N = %d.",
                      gmx_stats_message(estats),smlsq[j].ptype,nfitexp);
        }
        nfitexp /= npd;
        if ((nfitexp > 0) &&
            (estatsOK != (estats = gmx_stats_get_average(smlsq[j].nexp,&nnn))))
        {
            gmx_fatal(FARGS,"Statistics problem: %s. gt_type = %s. N = %d.",
                      gmx_stats_message(estats),smlsq[j].ptype,nfitexp);
        }
        
        int nfitqm=0;
        if (estatsOK != (estats = gmx_stats_get_npoints(smlsq[j].nqm,&nfitqm)))
        {
            gmx_fatal(FARGS,"Statistics problem: %s. gt_type = %s. N = %d.",
                      gmx_stats_message(estats),smlsq[j].ptype,nfitqm);
        }
        nfitqm /= npd;
        if ((nfitqm > 0) && 
            (estatsOK != (estats = gmx_stats_get_average(smlsq[j].nqm,&nnn))))
        {
            fprintf(stderr,"Statistics problem: %s. gt_type = %s. N = %d\n",
                    gmx_stats_message(estats),smlsq[j].ptype,nfitqm);
        }

        fprintf(fp,"%s & %s & %s & %s & %s & %s & %s (%s)",
                smlsq[j].ptype,
                (nfitexp > 0)     ? gmx_itoa(nfitexp)     : "",
                (nfitqm > 0)      ? gmx_itoa(nfitqm)      : "",
                (ahc > 0)         ? gmx_ftoa(ahc)         : "",
                (ahp > 0)         ? gmx_ftoa(ahp)         : "",
                (bos_pol > 0)     ? gmx_ftoa(bos_pol)     : "",
                (alexandria_aver > 0)  ? gmx_ftoa(alexandria_aver)  : "",
                (alexandria_sigma > 0) ? gmx_ftoa(alexandria_sigma) : "-");
        if (strcasecmp(ref,"Maaren2013a") == 0)
        {
            fprintf(fp,"& (*)\\\\\n");
        }
        else
        {
            fprintf(fp,"& \\cite{%s}\\\\\n",ref);
        }
        ntab++;
        
        if ((NULL != xvg) && (gmx_stats_get_npoints(smlsq[j].lsq,&N) == estatsOK) && (N > 0))
        {
            real *x=NULL,*y=NULL;
            int nbins = 20;
            
            if (gmx_stats_make_histogram(smlsq[j].lsq,0,&nbins,ehistoY,1,&x,&y) == estatsOK)
            {
                fprintf(xvg,"@type xy\n");
                for(i=0; (i<nbins); i++)
                {
                    fprintf(xvg,"%g  %g\n",x[i],y[i]);
                }
                fprintf(xvg,"&\n");
            }
            sfree(x);
            sfree(y);
        }
    }
    if (NULL != xvg)
    {
        fclose(xvg);
    }
    atomtype_tab_end(fp);
    fflush(fp);
}

static void gmx_molprop_atomtype_dip_table(FILE *fp,gmx_poldata_t pd)
{
    int    i,k,m,cur=0;
    char   *elem,*gt_type[2] = { NULL, NULL };
    char   *spref,*desc,*ptype,*btype;
#define prev (1-cur)
#define NEQG 5
    ChargeGenerationModel eqgcol[NEQG] = { eqgAXp, eqgAXs, eqgAXg };
    int    npcol[NEQG]  = { 2, 3, 3 };
    const char   *clab[3] = { "$J_0$", "$\\chi_0$", "$\\zeta$" };
    int    ncol;
    
    ncol = 1;
    for(i=0; (i<NEQG); i++)
    {
        ncol += npcol[i];
    }
    fprintf(fp,"\\begin{sidewaystable}[H]\n\\centering\n");
    fprintf(fp,"\\caption{Electronegativity equalization parameters for Alexandria models. $J_0$ and $\\chi_0$ in eV, $\\zeta$ in 1/nm.}\n");
    fprintf(fp,"\\label{eemparams}\n");
    fprintf(fp,"\\begin{tabular}{l");
    for(i=1; (i<ncol); i++)
        fprintf(fp,"c");
    fprintf(fp,"}\n\\hline\n");
    for(i=0; (i<NEQG); i++) 
        fprintf(fp," & \\multicolumn{%d}{c}{%s}",npcol[i],
                get_eemtype_name(eqgcol[i]));
    fprintf(fp,"\\\\\n");
    for(i=0; (i<NEQG); i++) 
        for(m=0; (m<npcol[i]); m++)
            fprintf(fp," & %s",clab[m]);
    fprintf(fp,"\\\\\n\\hline\n");
    while(1 == gmx_poldata_get_atype(pd,
                                     &elem,
                                     &desc,
                                     &(gt_type[cur]),
                                     &ptype,
                                     &btype,
                                     &spref))
    {
        if (((NULL == gt_type[prev]) || (strcmp(gt_type[cur],gt_type[prev]) != 0)))
        {
            fprintf(fp,"%s\n",gt_type[cur]);
            for(k=0; (k<NEQG); k++)
            {
                if (gmx_poldata_have_eem_support(pd,eqgcol[k],gt_type[cur],false))
                {
                    fprintf(fp," & %.3f",gmx_poldata_get_j00(pd,eqgcol[k],gt_type[cur]));
                    fprintf(fp," & %.3f",gmx_poldata_get_chi0(pd,eqgcol[k],gt_type[cur]));
                    if (npcol[k] == 3)
                        fprintf(fp," & %.3f",gmx_poldata_get_zeta(pd,eqgcol[k],gt_type[cur],1));
                    if (npcol[k] == 4)
                        fprintf(fp," & %.3f",gmx_poldata_get_zeta(pd,eqgcol[k],gt_type[cur],2));
                }
                else 
                {
                    for(m=0; (m<npcol[k]); m++)
                        fprintf(fp," & ");
                }
            }
            fprintf(fp,"\\\\\n");
        }
        cur = prev;
    }
    fprintf(fp,"\\hline\n\\end{tabular}\\end{sidewaystable}\n\n");
}

void gmx_molprop_atomtype_table(FILE *fp,bool bPolar,
                                int npd,gmx_poldata_t pd[],
                                gmx_poldata_t pd_aver,
                                std::vector<alexandria::MolProp> mp,
                                char *lot,char *exp_type,
                                output_env_t oenv,const char *histo)
{
    if (bPolar)
    {
        gmx_molprop_atomtype_polar_table(fp,npd,pd,pd_aver,mp,
                                         lot,exp_type,oenv,histo);
    }
    else
    {
        gmx_molprop_atomtype_dip_table(fp,pd[0]);
    }
}
                               
static void prop_header(FILE *fp,const char *property,real rel_toler,real abs_toler,
                        t_qmcount *qmc,iMolSelect ims,bool bPrintConf,
                        bool bPrintBasis,bool bPrintMultQ,bool bLandscape)
{
    int  i, k;
    
    if (bLandscape)
    {
        fprintf(fp, "\\begin{landscape}\n");
    }
    fprintf(fp,"\\begin{longtable}{p{75mm}cc");
    if (bPrintMultQ)
        fprintf(fp,"cc");
    if (bPrintConf)
        fprintf(fp,"c");
    for(i=0; (i<qmc->n); i++)
        fprintf(fp,"c");
    fprintf(fp,"}\n\\hline\n");

    for(k = 0; (k < 2); k++)
    {
        if (0 == k)
        {
            fprintf(fp, "\\caption{Comparison of experimental %s to calculated values. {\\bf Data set: %s}. Calculated numbers that are more than %.0f%s off the experimental values are printed in bold, more than %.0f%s off in bold red.}\\label{%s}\\\\\n",
                    property,ims_names[ims],
                    (abs_toler > 0) ? abs_toler   : 100*rel_toler,(abs_toler > 0) ? "" : "\\%",
                    (abs_toler > 0) ? 2*abs_toler : 200*rel_toler,(abs_toler > 0) ? "" : "\\%",
                    ims_names[ims]);
        }
        else
        {
            fprintf(fp, "\\hline\n");
        }
        
        fprintf(fp,"Molecule & Form. %s %s & Exper. ",
                bPrintMultQ ? "& q & mult" : "",
                bPrintConf  ? "& Conf." : "");
        for(i=0; (i<qmc->n); i++)
            fprintf(fp,"& %s",qmc->method[i]);
        fprintf(fp,"\\\\\n");
        if (bPrintBasis) 
        {
            fprintf(fp," & & %s %s",
                    bPrintMultQ ? "& &" : "",
                    bPrintConf ? "&" : "");
            for(i=0; (i<qmc->n); i++)
                fprintf(fp,"& %s",qmc->basis[i]);
            fprintf(fp,"\\\\\n");
        }
        fprintf(fp,"Type & &%s %s",
                bPrintMultQ ? "& &" : "",
                bPrintConf ? "&" : "");
        for(i=0; (i<qmc->n); i++)
            fprintf(fp,"& %s",qmc->type[i]);
        fprintf(fp,"\\\\\n\\hline\n");
        if (0 == k)
        {
            fprintf(fp, "\\endfirsthead\n");
        }
        else
        {
            fprintf(fp, "\\endhead\n");
            fprintf(fp, "\\hline\n");
            fprintf(fp, "\\endfoot\n");
        }
    }
}

static void prop_end(FILE *fp,bool bLandscape)
{
    fprintf(fp,"\\end{longtable}\n");
    if (bLandscape)
    {
        fprintf(fp, "\\end{landscape}\n");
    }
}

static int outside(real vexp,real vcalc,real rel_toler,real abs_toler)
{
    real rdv,adv = fabs(vexp-vcalc);
    if (abs_toler > 0) 
    {
        if (adv > 2*abs_toler)
            return 2;
        else if (adv > abs_toler)
            return 1;
        return 0;
    }
    else 
    {
        if (vexp == 0)
            return 0;
        rdv = adv/vexp;
        
        if (rdv > 2*rel_toler)
            return 2;
        else if (rdv > rel_toler)
            return 1;
        return 0;
    }
}

class ExpData 
{
public:
    double val_,err_;
    std::string ref_,conf_,type_;
    ExpData(double val,double err,std::string ref,std::string conf,std::string type)
    { val_ = val; err_ = err; ref_ = ref; conf_ = conf; type_ = type; }
}; 

class CalcData 
{
public:
    double val_,err_;
    int found_;
    CalcData(double val,double err,int found)
    { val_ = val; err_ = err; found_ = found; }
}; 

void gmx_molprop_prop_table(FILE *fp,MolPropObservable mpo,real rel_toler,real abs_toler,
                            std::vector<alexandria::MolProp> mp,
                            t_qmcount *qmc,bool bPrintAll,
                            bool bPrintBasis,bool bPrintMultQ,
                            gmx_molselect_t gms,iMolSelect ims)
{
    alexandria::MolecularQuadrupoleIterator qi;
    alexandria::MolecularEnergyIterator mei;
    
    int    j,iprint=0,nexp,max_nexp;
#define BLEN 1024
    char   lbuf[BLEN],myline[BLEN],mylbuf[BLEN],vbuf[BLEN];
    double calc_val,calc_err,vc;
    int    nprint;
    double dvec[DIM];
    tensor quadrupole;
    bool   bSideways, bPrintConf;
  
    bSideways = (qmc->n > 1);
    
    nprint = 0;
    for(alexandria::MolPropIterator mpi=mp.begin(); (mpi<mp.end()); mpi++)
    {
        if ((ims == gmx_molselect_status(gms,mpi->GetIupac().c_str())) &&
            (mpi->HasComposition(SPOEL)))
        {
            nprint++;
        }
    }
    if (nprint <= 0)
    {
        return;
    }
    bPrintConf = false; //(mpo == MPO_DIPOLE);
    prop_header(fp,mpo_name[mpo],rel_toler,abs_toler,qmc,
                ims,bPrintConf,bPrintBasis,bPrintMultQ,bSideways);  
    for(alexandria::MolPropIterator mpi=mp.begin(); (mpi<mp.end()); mpi++)
    {
        if ((ims == gmx_molselect_status(gms,mpi->GetIupac().c_str())) && 
            (mpi->HasComposition(SPOEL)))
        {
            std::vector<ExpData> ed;
            std::vector<CalcData> cd;
            for(alexandria::ExperimentIterator ei=mpi->BeginExperiment(); (ei<mpi->EndExperiment()); ei++)
            {
                switch (mpo) 
                {
                case MPO_DIPOLE:
                    for(alexandria::MolecularDipPolarIterator mdi=ei->BeginDipole(); (mdi<ei->EndDipole()); mdi++)
                    {
                        ed.push_back(ExpData(mdi->GetAver(),
                                             mdi->GetError(),
                                             ei->GetReference(),
                                             ei->GetConformation(),
                                             mdi->GetType()));
                    }
                    break;
                case MPO_POLARIZABILITY:
                    for(alexandria::MolecularDipPolarIterator mdi=ei->BeginPolar(); (mdi<ei->EndPolar()); mdi++)
                    {
                        ed.push_back(ExpData(mdi->GetAver(),
                                             mdi->GetError(),
                                             ei->GetReference(),
                                             ei->GetConformation(),
                                             mdi->GetType()));
                    }
                    break;
                case MPO_ENERGY:
                    for(mei=ei->BeginEnergy(); (mei<ei->EndEnergy()); mei++)
                    {
                        ed.push_back(ExpData(mei->GetValue(),
                                             mei->GetError(),
                                             ei->GetReference(),
                                             ei->GetConformation(),
                                             mei->GetType()));
                    }
                    break;
                default:
                    gmx_fatal(FARGS,"No support for for mpo %d",mpo);
                    break;
                }
            }
            max_nexp = ed.size();
            if (max_nexp <= 0) 
                max_nexp = 1;
            for(nexp=0; (nexp < max_nexp); nexp++)
            {
                for(j=0; (j<qmc->n); j++) 
                {
                    sprintf(lbuf,"%s/%s",qmc->method[j],qmc->basis[j]);
                    if (mpi->GetPropRef(mpo,iqmQM,lbuf,NULL,//conf_exp[nexp].c_str(),
                                        qmc->type[j],&calc_val,&calc_err,
                                        NULL,NULL,dvec,quadrupole)) 
                    {
                        cd.push_back(CalcData(calc_val,calc_err,1));
                    }
                    else 
                    {
                        cd.push_back(CalcData(0,0,0));
                    }
                }
                if ((bPrintAll || (ed.size() > 0))  && (cd.size() > 0))
                {
                    if (0 == nexp)
                    {
                        iprint++;
                    }
                    myline[0] = '\0';
                    if (nexp == 0)
                    {
                        if (bPrintMultQ)
                        {
                            sprintf(myline,"%d. %-15s & %s & %d & %d",
                                    iprint,mpi->GetIupac().c_str(),
                                    mpi->GetTexFormula().c_str(),
                                    mpi->GetCharge(),
                                    mpi->GetMultiplicity());
                        }
                        else
                        {
                            sprintf(myline,"%d. %-15s & %s",
                                    iprint,mpi->GetIupac().c_str(),
                                    mpi->GetTexFormula().c_str());
                        }
                    }
                    else 
                    {
                        sprintf(myline," & ");
                    }
                    if (bPrintConf)
                    {
                        sprintf(mylbuf,"      & %s ",((ed[nexp].conf_.size() > 0) ? 
                                                      ed[nexp].conf_.c_str() : "-"));
                        strncat(myline,mylbuf,BLEN-strlen(myline));
                    }
                    if (ed.size() > 0) 
                    {
                        sprintf(mylbuf,"& %8.3f",ed[nexp].val_);
                        strncat(myline,mylbuf,BLEN-strlen(myline));
                        if (strcmp(ed[nexp].ref_.c_str(),"Maaren2014a") == 0)
                            sprintf(mylbuf," (*)");
                        else
                            sprintf(mylbuf,"~\\cite{%s} ",ed[nexp].ref_.c_str());
                        strncat(myline,mylbuf,BLEN-strlen(myline));
                    }
                    else 
                    {
                        sprintf(mylbuf,"& - ");
                        strncat(myline,mylbuf,BLEN-strlen(myline));
                    }
                    for(j=0; (j<qmc->n); j++) 
                    { 
                        if (cd[j].found_ > 0) 
                        {
                            vc = cd[j].val_;
                            if (cd[j].err_ > 0) 
                            {
                                sprintf(vbuf,"%8.2f(%.2f)",vc,cd[j].err_);
                            }
                            else
                            {
                                sprintf(vbuf,"%8.2f",vc);
                            }
                            if (ed.size() > 0) 
                            {
                                int oo = outside(ed[nexp].val_,vc,rel_toler,abs_toler);
                                switch(oo) {
                                case 2:
                                    sprintf(mylbuf,"& \\textcolor{Red}{\\bf %s} ",vbuf);
                                    break;
                                case 1:
                                    sprintf(mylbuf,"& {\\bf %s} ",vbuf);
                                    break;
                                default:
                                    sprintf(mylbuf,"& %s ",vbuf);
                                }
                            }
                            else
                            {
                                sprintf(mylbuf,"& %s ",vbuf);
                            }
                            strncat(myline,mylbuf,BLEN-strlen(myline));
                        }
                        else
                        {
                            sprintf(mylbuf,"& ");
                            strncat(myline,mylbuf,BLEN-strlen(myline));
                        }
                    }
                    sprintf(mylbuf,"\\\\\n");
                    strncat(myline,mylbuf,BLEN-strlen(myline));
                    fprintf(fp,"%s",myline);
                }
            }
        }
    }
    prop_end(fp,bSideways);
}

