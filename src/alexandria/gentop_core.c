/* -*- mode: c; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup"; -*-
 * $Id: gentop_core.c,v 1.12 2009/04/12 21:24:26 spoel Exp $
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

#include <ctype.h>
#include "maths.h"
#include "macros.h"
#include "copyrite.h"
#include "bondf.h"
#include "string2.h"
#include "smalloc.h"
#include "strdb.h"
#include "sysstuff.h"
#include "confio.h"
#include "physics.h"
#include "statutil.h"
#include "vec.h"
#include "random.h"
#include "3dview.h"
#include "txtdump.h"
#include "readinp.h"
#include "names.h"
#include "symtab.h"
#include "vec.h"
#include "atomprop.h"
#include "grompp.h"
#include "toputil.h"
#include "gen_ad.h"
#include "pdbio.h"
#include "gmx_random.h"
#include "gpp_atomtype.h"
#include "poldata.h"
#include "gentop_nm2type.h"
#include "gentop_core.h"
#include "gentop_vsite.h"

gpp_atomtype_t set_atom_type(FILE *fp,const char *molname,
                             t_symtab *tab,t_atoms *atoms,t_params *bonds,
                             int nbonds[],gmx_bool bRing[],double bondorder[],
                             char **smnames,gmx_poldata_t pd,
                             gmx_atomprop_t aps,rvec x[],t_pbc *pbc,real th_toler,
                             real ph_toler,gentop_vsite_t gvt)
{
    gpp_atomtype_t atype;
    int nresolved;
    int i;
    
    atype = init_atomtype();
    snew(atoms->atomtype,atoms->nr);
    nresolved = nm2type(fp,molname,pd,aps,tab,atoms,bRing,bondorder,atype,nbonds,
                        bonds,smnames,x,pbc,th_toler,ph_toler,gvt);
    if (nresolved != atoms->nr) 
        return NULL;
    else if (debug)
        fprintf(debug,"There are %d different atom types in your sample\n",
                get_atomtype_ntypes(atype));
    if (NULL == atoms->atomtype)
        snew(atoms->atomtype,atoms->nr);
    if (NULL == atoms->atomtypeB)
        snew(atoms->atomtypeB,atoms->nr);
    for(i=0; (i<atoms->nr); i++)
    {
        atoms->atomtype[i]  = put_symtab(tab,get_atomtype_name(atoms->atom[i].type,atype));
        atoms->atomtypeB[i] = put_symtab(tab,get_atomtype_name(atoms->atom[i].typeB,atype));
    }
    
    return atype;
}

void calc_angles_dihs(t_params *ang,t_params *dih,rvec x[],gmx_bool bPBC,
                      matrix box)
{
    int    i,ai,aj,ak,al,t1,t2,t3;
    rvec   r_ij,r_kj,r_kl,m,n;
    real   sign,th,costh,ph;
    t_pbc  pbc;

    if (bPBC)
        set_pbc(&pbc,-1,box);
    if (debug)
        pr_rvecs(debug,0,"GENTOP",box,DIM);
    for(i=0; (i<ang->nr); i++) 
    {
        ai = ang->param[i].AI;
        aj = ang->param[i].AJ;
        ak = ang->param[i].AK;
        th = RAD2DEG*bond_angle(x[ai],x[aj],x[ak],bPBC ? &pbc : NULL,
                                r_ij,r_kj,&costh,&t1,&t2);
        if (debug)
            fprintf(debug,"GENTOP: ai=%3d aj=%3d ak=%3d r_ij=%8.3f r_kj=%8.3f th=%8.3f\n",
                    ai,aj,ak,norm(r_ij),norm(r_kj),th);
        ang->param[i].C0 = th;
    }
    for(i=0; (i<dih->nr); i++) 
    {
        ai = dih->param[i].AI;
        aj = dih->param[i].AJ;
        ak = dih->param[i].AK;
        al = dih->param[i].AL;
        ph = RAD2DEG*dih_angle(x[ai],x[aj],x[ak],x[al],bPBC ? & pbc : NULL,
                               r_ij,r_kj,r_kl,m,n,&sign,&t1,&t2,&t3);
        if (debug)
            fprintf(debug,"GENTOP: ai=%3d aj=%3d ak=%3d al=%3d r_ij=%8.3f r_kj=%8.3f r_kl=%8.3f ph=%8.3f\n",
                    ai,aj,ak,al,norm(r_ij),norm(r_kj),norm(r_kl),ph);
        dih->param[i].C0 = ph;
    }
}

void dump_hybridization(FILE *fp,t_atoms *atoms,int nbonds[])
{
    int i;
  
    for(i=0; (i<atoms->nr); i++) 
    {
        fprintf(fp,"Atom %5s has %d bonds\n",*atoms->atomname[i],nbonds[i]);
    }
}

static void print_pl(FILE *fp,t_params plist[],int ftp,const char *name,
                     char ***atomname)
{ 
    int i,j,nral,nrfp;

    if (plist[ftp].nr > 0) 
    {
        fprintf(fp,"\n");
        fprintf(fp,"[ %s ]\n",name);
        nral = interaction_function[ftp].nratoms;
        nrfp = interaction_function[ftp].nrfpA;
        for(i=0; (i<plist[ftp].nr); i++) 
        {
            for(j=0; (j<nral); j++) 
                fprintf(fp,"  %5s",*atomname[plist[ftp].param[i].a[j]]);
            for(j=0; (j<nrfp); j++) 
                fprintf(fp,"  %10.3e",plist[ftp].param[i].c[j]);
            fprintf(fp,"\n");
        }
    }
}

void print_rtp(char *filenm,char *title,t_atoms *atoms,
               t_params plist[],int cgnr[],int nbts,int bts[])
{
    FILE *fp;
    int i;
    
    fp = ffopen(filenm,"w");
    fprintf(fp,"; %s\n",title);
    fprintf(fp,"\n");
    fprintf(fp,"[ %s ]\n",*atoms->resinfo[0].name);
    fprintf(fp,"\n");
    fprintf(fp,"[ atoms ]\n");
    for(i=0; (i<atoms->nr); i++) 
    {
        fprintf(fp,"%-8s  %12s  %8.4f  %5d\n",
                *atoms->atomname[i],*atoms->atomtype[i],
                atoms->atom[i].q,cgnr[i]);
    }
    for(i=0; (i<nbts); i++) {
        print_pl(fp,plist,bts[i],interaction_function[bts[i]].name,atoms->atomname);
    }  
    fclose(fp);
}

static int pcompar(const void *a, const void *b)
{
    t_param *pa,*pb;
    int     d;
    pa=(t_param *)a;
    pb=(t_param *)b;
  
    d = pa->AI - pb->AI;
    if (d == 0) 
        d = pa->AJ - pb->AJ;
    if (d == 0) 
        d = pa->AK - pb->AK;
    if (d == 0) 
        d = pa->AL - pb->AL;
    /*if (d == 0)
      return strlen(pb->s) - strlen(pa->s);
      else*/
    return d;
}

static int acomp(const void *a,const void *b)
{
    atom_id *aa = (atom_id *)a;
    atom_id *ab = (atom_id *)b;
  
    return (*aa - *ab);
}

static void my_clean_excls(int nr,t_excls excls[])
{
    int i,j,k;
  
    for(i=0; (i<nr); i++) 
    {
        if ( excls[i].nr > 0) 
        {
            qsort(excls[i].e,excls[i].nr,sizeof(excls[i].e[0]),acomp);
            k=0;
            for(j=0; (j<excls[i].nr); j++) 
            {
                if (excls[i].e[j] != excls[i].e[k]) 
                {
                    excls[i].e[++k] = excls[i].e[j];
                }
            }
            excls[i].nr = ++k;
        }
    }
}

static void clean_thole(t_params *ps)
{
    int     i,j;
    atom_id a,ai,aj,ak,al;
  
    if (ps->nr > 0) 
    {
        /* swap atomnumbers in bond if first larger than second: */
        for(i=0; (i<ps->nr); i++)
            if ( ps->param[i].AK < ps->param[i].AI ) 
            {
                a = ps->param[i].AI;
                ps->param[i].AI = ps->param[i].AK;
                ps->param[i].AK = a;
                a = ps->param[i].AJ;
                ps->param[i].AJ = ps->param[i].AL;
                ps->param[i].AL = a;
            }
    
        /* Sort bonds */
        qsort(ps->param,ps->nr,(size_t)sizeof(ps->param[0]),pcompar);
    
        /* remove doubles, keep the first one always. */
        j = 1;
        for(i=1; (i<ps->nr); i++) 
        {
            if ((ps->param[i].AI != ps->param[j-1].AI) ||
                (ps->param[i].AJ != ps->param[j-1].AJ) ||
                (ps->param[i].AK != ps->param[j-1].AK) ||
                (ps->param[i].AL != ps->param[j-1].AL) ) 
            {
                cp_param(&(ps->param[j]),&(ps->param[i]));
                j++;
            } 
        }
        fprintf(stderr,"Number of Tholes was %d, now %d\n",ps->nr,j);
        ps->nr=j;
    }
    else
        fprintf(stderr,"No Tholes\n");
}

real calc_dip(t_atoms *atoms,rvec x[])
{
    int i;
    rvec mu,mm;
    real qq;
    
    clear_rvec(mu);
    for(i=0; (i<atoms->nr); i++) 
    {
        qq = atoms->atom[i].q;
        svmul(qq,x[i],mm);
        rvec_inc(mu,mm);
    }
    return norm(mu)*ENM2DEBYE;
}
  
void reset_q(t_atoms *atoms)
{
    int i;
  
    /* Use values from file */
    for(i=0; (i<atoms->nr); i++) 
        atoms->atom[i].qB = atoms->atom[i].q;
}

static void add_excl(t_excls *excls,atom_id e)
{
    int i;
  
    for(i=0; (i<excls->nr); i++)
        if (excls->e[i] == e)
            return;
    srenew(excls->e,excls->nr+1);
    excls->e[excls->nr++] = e;
}

static void remove_excl(t_excls *excls, int remove)
{
    int i;

    for(i=remove+1; i<excls->nr; i++)
        excls->e[i-1] = excls->e[i];
  
    excls->nr--;
}

static void prune_excl(t_excls excls[],t_atoms *atoms,gpp_atomtype_t atype)
{
    int i,k,ak;
  
    for(i=0; (i<atoms->nr); i++) 
    {
        if (get_atomtype_ptype(atoms->atom[i].type,atype) != eptShell)
            for(k=0; (k<excls[i].nr); ) 
            {
                ak = excls[i].e[k];
                if (get_atomtype_ptype(atoms->atom[ak].type,atype) != eptShell)
                    remove_excl(&(excls[i]),k);
                else 
                    k++;
            }
    }
}

void copy_atoms(t_atoms *src,t_atoms *dest)
{
    int i;
    
    if (dest->nr < src->nr)
    {
        srenew(dest->atom,src->nr);
        srenew(dest->atomname,src->nr);
        if (NULL != src->atomtype)
            srenew(dest->atomtype,src->nr);
        else if (NULL != dest->atomtype)
        {
            sfree(dest->atomtype);
            dest->atomtype = NULL;
        }
        if (NULL != src->atomtypeB)
            srenew(dest->atomtypeB,src->nr);
        else if (NULL != dest->atomtypeB)
        {
            sfree(dest->atomtypeB);
            dest->atomtypeB = NULL;
        }
    }
    dest->nr = src->nr;
    for(i=0; (i<src->nr); i++)
    {
        dest->atom[i]      = src->atom[i];
        dest->atomname[i]  = src->atomname[i];
        if (NULL != src->atomtype)
            dest->atomtype[i]  = src->atomtype[i];
        if (NULL != src->atomtypeB)
            dest->atomtypeB[i] = src->atomtypeB[i];
    }
    if (dest->nres < src->nres)
        srenew(dest->resinfo,src->nres);
        
    if (NULL != src->pdbinfo)
        srenew(dest->pdbinfo,src->nres);
    else if (NULL != dest->pdbinfo)
    {
        sfree(dest->pdbinfo);
        dest->pdbinfo = NULL;
    }
    dest->nres = src->nres;
    for(i=0; (i<src->nres); i++)
    {
        dest->resinfo[i] = src->resinfo[i];
        if (NULL != src->pdbinfo)
            dest->pdbinfo[i] = src->pdbinfo[i];
    }
}

void add_shells(gmx_poldata_t pd,int maxatom,t_atoms *atoms,
                gpp_atomtype_t atype,t_params plist[],
                rvec *x,t_symtab *symtab,t_excls **excls,
                char **smnames)
{
    int     i,j,k,ai,aj,iat,shell,ns=0;
    int     *renum;
    char    buf[32],**newname,*gt_type;
    t_param p;
    t_atom  *shell_atom;
    t_atoms *newa;
    t_excls *newexcls;
    rvec    *newx;
    double  pol,sigpol;
  
    if (maxatom < atoms->nr*2+2)
    {
        gmx_fatal(FARGS,"Arrays should be preallocated to have enough space for shells.\nAt least %d postions for %d atoms.",atoms->nr*2+2,atoms->nr);
    }
    snew(shell_atom,1);
    shell_atom->ptype = eptShell;
    memset(&p,0,sizeof(p));
    snew(renum,maxatom);
    for(i=0; (i<atoms->nr); i++) 
    {
        renum[i] = i+ns;
        gt_type = gmx_poldata_get_type(pd,smnames[i]);
        if ((NULL != gt_type) &&
            (1 == gmx_poldata_type_polarizability(pd,gt_type,&pol,&sigpol)))
        { 
            ns++;
            p.AI = renum[i];
            p.AJ = renum[i]+1;
            p.C0 = 0.001*pol;
            add_param_to_list(&(plist[F_POLARIZATION]),&p);
        }
    }
    renum[atoms->nr] = atoms->nr + ns;
  
    if (ns > 0) 
    {
        /* Make new atoms and x arrays */
        snew(newa,1);
        init_t_atoms(newa,atoms->nr+ns,TRUE);
        snew(newa->atomtype,atoms->nr+ns);
        snew(newa->atomtypeB,atoms->nr+ns);
        newa->nres = atoms->nres;
        snew(newx,newa->nr);
        snew(newname,newa->nr);
        
        /* Make new exclusion array, and put the shells in it */
        snew(newexcls,newa->nr);
        for(j=0; (j<plist[F_POLARIZATION].nr); j++) 
        {
            ai = plist[F_POLARIZATION].param[j].AI;
            aj = plist[F_POLARIZATION].param[j].AJ;
            add_excl(&newexcls[ai],aj);
            add_excl(&newexcls[aj],ai);
        }
        for(i=0; (i<atoms->nr); i++) 
        {
            newa->atom[renum[i]]     = atoms->atom[i];
            newa->atomname[renum[i]] = put_symtab(symtab,*atoms->atomname[i]);
            newa->atomtype[renum[i]] = put_symtab(symtab,*atoms->atomtype[i]);
            newa->atomtypeB[renum[i]] = put_symtab(symtab,*atoms->atomtypeB[i]);
            copy_rvec(x[i],newx[renum[i]]);
            newname[renum[i]] = smnames[i];
            t_atoms_set_resinfo(newa,renum[i],symtab,
                                *atoms->resinfo[atoms->atom[i].resind].name,
                                atoms->atom[i].resind,' ',1,' ');
        }
        
        for(i=0; (i<atoms->nr); i++) 
        {
            iat = renum[i];
            for(k=0; (k<(*excls)[i].nr); k++)
                add_excl(&(newexcls[iat]),renum[(*excls)[i].e[k]]);
            for(j=iat+1; (j<renum[i+1]); j++) 
            {
                newa->atom[j]       = atoms->atom[i];
                newa->atom[iat].q   = 0;
                newa->atom[iat].qB  = 0;
                newa->atom[j].m     = 0;
                newa->atom[j].mB    = 0;
                newa->atom[j].atomnumber = 0;
                sprintf(buf,"%ss",get_atomtype_name(atoms->atom[i].type,atype));
                newname[j] = strdup(buf);
                shell = add_atomtype(atype,symtab,shell_atom,buf,&p,
                                     0,0,0,0,0,0,0);
                newa->atom[j].type  = shell;
                newa->atom[j].typeB = shell;
                newa->atomtype[j]   = 
                    newa->atomtypeB[j]  = put_symtab(symtab,buf);
                newa->atom[j].ptype = eptShell;
                newa->atom[j].resind = atoms->atom[i].resind;
                sprintf(buf,"%ss",*(atoms->atomname[i]));
                newa->atomname[j] = put_symtab(symtab,buf);
                copy_rvec(x[i],newx[j]);
                for(k=0; (k<(*excls)[i].nr); k++) 
                {
                    ai = j;
                    aj = renum[(*excls)[i].e[k]];
                    if (ai != aj) 
                    {
                        add_excl(&(newexcls[ai]),aj);
                        add_excl(&(newexcls[aj]),ai);
                    }
                }
            }
        }
        for(i=0; (i<atoms->nr); i++) 
        {
            iat = renum[i];
            for(j=iat+1; (j<renum[i+1]); j++) 
            {
                for(k=0; (k<newexcls[iat].nr); k++) 
                {
                    ai = j;
                    aj = newexcls[iat].e[k];
                    if (ai != aj) 
                    {
                        add_excl(&(newexcls[ai]),aj);
                        add_excl(&(newexcls[aj]),ai);
                    }
                }
            }
        }
        prune_excl(newexcls,newa,atype);
        /* Copy newa to atoms */
        copy_atoms(newa,atoms);
        /* Copy coordinates and smnames */
        for(i=0; (i<newa->nr); i++)
        {
            copy_rvec(newx[i],x[i]);
            smnames[i] = newname[i];
        }
        sfree(newx);
        sfree(newname);
        /* Copy exclusions */
        *excls = newexcls;

        for(i=0; (i<F_NRE); i++) 
        {
            if (i != F_POLARIZATION)
                for(j=0; (j<plist[i].nr); j++) 
                    for(k=0; (k<NRAL(i)); k++) 
                        plist[i].param[j].a[k] = renum[plist[i].param[j].a[k]];
        }
    }
    sfree(renum);
    sfree(shell_atom);
}

int *symmetrize_charges(gmx_bool bQsym,t_atoms *atoms,
                        t_params *bonds,gmx_poldata_t pd,
                        gmx_atomprop_t aps,char *symm_string)
{
    char *central,*attached,**ss;
    int nattached,i,j,nh,ai,aj,anri,anrj;
    int is,anr_central,anr_attached,nrq;
    int hs[8];
    double qaver,qsum;
    int *sc;
    
    snew(sc,atoms->nr);
    for(i=0; (i<atoms->nr); i++) 
    {
        sc[i] = i;
    }
    if (bQsym)
    {
        if ((NULL != symm_string) && (strlen(symm_string) > 0))
        {
            ss = split(' ',symm_string);
            is = 0;
            while ((ss[is] != NULL) && (is < atoms->nr))
            {
                sc[is] = atoi(ss[is]);
                is++;
            }
            while (NULL != ss[is])
                is++;
            if (is != atoms->nr)
                gmx_fatal(FARGS,"Wrong number (%d) of atom-numbers in symm_string: expected %d",is,atoms->nr);
        }
        else 
        {
            while (gmx_poldata_get_symcharges(pd,&central,
                                              &attached,&nattached) == 1) 
            {  
                anr_central  = gmx_atomprop_atomnumber(aps,central);
                anr_attached = gmx_atomprop_atomnumber(aps,attached);
                for(i=0; (i<atoms->nr); i++) 
                {
                    if (atoms->atom[i].atomnumber == anr_central) 
                    {
                        nh = 0;
                        for(j=0; (j<bonds->nr); j++) 
                        {
                            ai = bonds->param[j].AI;
                            aj = bonds->param[j].AJ;
                            anri = atoms->atom[ai].atomnumber;
                            anrj = atoms->atom[aj].atomnumber;
                            
                            if ((ai == i) && (anrj == anr_attached))
                                hs[nh++] = aj;
                            else if ((aj == i) && (anri == anr_attached))
                                hs[nh++] = ai; 
                        }
                        if (nh == nattached) 
                        {
                            for(j=0; (j<nattached); j++) 
                            {
                                if (j > 0)
                                    sc[hs[j]] = hs[0];
                            }
                        }
                    }
                }
            }
        }
       
        for(i=0; (i<atoms->nr); i++)
        {
            qsum = 0;
            nrq = 0;
            for(j=0; (j<atoms->nr); j++)
            {
                if (sc[j] == sc[i])
                {
                    qsum += atoms->atom[j].q;
                    nrq++;
                }
            }
            if (0 < nrq)
            {
                qaver = qsum/nrq;
                for(j=0; (j<atoms->nr); j++)
                {
                    if (sc[j] == sc[i])
                    {
                        atoms->atom[j].q = qaver;
                    }
                }    
            }
        }
    }
    return sc;
}

static int *generate_cg_neutral(t_atoms *atoms,gmx_bool bUsePDBcharge)
{
    int    i,n=1;
    int    *cgnr;
    double qt=0,mt=0;
  
    snew(cgnr,atoms->nr);
    for(i=0; (i<atoms->nr); i++) 
    {
        if (atoms->pdbinfo && bUsePDBcharge)
            atoms->atom[i].q = atoms->pdbinfo[i].bfac;
        qt += atoms->atom[i].q;
        cgnr[i] = n;
        if (is_int(qt)) 
        {
            n++;
            qt=0;
        }
    }
    return cgnr;
}

static int *generate_cg_group(t_atoms *atoms,t_params *bonds,t_params *pols)
{
    int    i,j,k,atn,ai,aj,ncg=1;
    int    *cgnr;
    gmx_bool   bMV;
    int    monovalent[] = { 0, 1, 9, 17, 35, 53, 85 };
#define nmv asize(monovalent)
    double qaver;
    
    /* Assume that shells and masses have atomnumber 0 */
    snew(cgnr,atoms->nr);
    for(i=0; (i<atoms->nr); i++) 
        cgnr[i] = NOTSET;
    
    for(i=0; (i<atoms->nr); i++) 
    {
        atn = atoms->atom[i].atomnumber;
        bMV = FALSE;
        for(j=0; (j<nmv) && !bMV; j++)
            bMV = (atn == monovalent[j]);
        if (!bMV)
            cgnr[i] = ncg++;
    }
    /* Rely on the notion that all H and other monovalent 
       atoms are bound to something */
    for(j=0; (j<bonds->nr); j++) 
    {
        ai  = bonds->param[j].AI;
        aj  = bonds->param[j].AJ;
        bMV = FALSE;
        atn = atoms->atom[ai].atomnumber;
        for(k=0; (k<nmv) && !bMV; k++)
            bMV = (atn == monovalent[k]);
        if (bMV) 
        {
            if (cgnr[aj] != NOTSET)
                cgnr[ai] = cgnr[aj];
            else
                cgnr[ai] = cgnr[aj] = 1;
        }
        else 
        {
            bMV = FALSE;
            atn = atoms->atom[aj].atomnumber;
            for(k=0; (k<nmv) && !bMV; k++)
                bMV = (atn == monovalent[k]);
            if (bMV) 
            {
                cgnr[aj] = cgnr[ai];
            }
        }
    }
    /* Rely on the notion that all shells are bound to something */
    for(j=0; (j<pols->nr); j++) 
    {
        ai = pols->param[j].AI;
        aj = pols->param[j].AJ;
        cgnr[aj] = cgnr[ai];
    }
    for(i=0; (i<atoms->nr); i++) 
        if (cgnr[i] == NOTSET) 
            cgnr[i] = ncg++;
    
    printf("There are %d charge groups\n",ncg-1);

    return cgnr;
}

static int *generate_cg_atom(int natom)
{
    int i,*cgnr;
  
    snew(cgnr,natom);
    for(i=0; (i<natom); i++)
        cgnr[i] = i+1;
    
    return cgnr;
}

int *generate_charge_groups(int cgtp,t_atoms *atoms,
                            t_params *bonds,t_params *pols,
                            gmx_bool bUsePDBcharge,
                            real *qtot,real *mtot)
{
    int i,*cgnr = NULL;
  
    switch (cgtp) 
    {
    case ecgNeutral:
        cgnr = generate_cg_neutral(atoms,bUsePDBcharge);
        break;
    case ecgGroup:
        cgnr = generate_cg_group(atoms,bonds,pols);
        break;
    case ecgAtom:
        cgnr = generate_cg_atom(atoms->nr);
        break;
    default:
        gmx_fatal(FARGS,"Invalid charge group generation type %d",cgtp);
    }
    *qtot = *mtot = 0;
    for(i=0; (i<atoms->nr); i++) 
    {
        *qtot += atoms->atom[i].q;
        *mtot += atoms->atom[i].m;
    }  
    return cgnr;
}

static int *cgnr_copy;
static double *atomnumber;
static int cg_comp(const void *a,const void *b)
{
    int *aa = (int *)a;
    int *bb = (int *)b;
    double c;
    
    int d = cgnr_copy[*aa] - cgnr_copy[*bb];
    if (d == 0)
    {
        c = atomnumber[*aa] - atomnumber[*bb];
        if (c < 0)
            return -1;
        else if (c > 0)
            return 1;
        else 
            return 0;
    }
    else
        return d;
}

void sort_on_charge_groups(int *cgnr,t_atoms *atoms,t_params plist[],
                           rvec x[],t_excls excls[],
                           char *smnames[],const char *ndxout,
                           int nmol)
{
    FILE    *fp;
    int     i,j,j0,k,newi,ri,*cg_renum,*ccgg,*inv_renum;
    rvec    *rx;
    t_atom  *ra;
    t_excls *newexcls;
    char    ***an,**smn;
  
    snew(cg_renum,atoms->nr);
    snew(atomnumber,atoms->nr);
    snew(rx,atoms->nr);
    snew(ra,atoms->nr);
    snew(an,atoms->nr);
    for(i=0; (i<atoms->nr); i++)
    {
        cg_renum[i] = i;
        atomnumber[i] = 1+i; /*atoms->atom[i].atomnumber;*/
        if ((atoms->atom[i].ptype == eptShell) && (i > 0))
            atomnumber[i] = atomnumber[i-1]+0.1;
    }
    cgnr_copy = cgnr;
    qsort(cg_renum,atoms->nr,sizeof(cg_renum[0]),cg_comp);
    if (debug)
        for(i=0; (i<atoms->nr); i++)
            fprintf(debug,"cg_renum[%d] = %d\n",i,cg_renum[i]);
    snew(ccgg,atoms->nr);
    for(i=0;(i<atoms->nr); i++) 
    {
        ri = cg_renum[i];
        copy_rvec(x[ri],rx[i]);
        memcpy(&(ra[i]),&(atoms->atom[ri]),sizeof(t_atom));
        an[i] = atoms->atomname[ri];
        ccgg[i] = cgnr[ri];
    }
    snew(inv_renum,atoms->nr);
    snew(smn,atoms->nr);
    for(i=0;(i<atoms->nr); i++) 
    {
        copy_rvec(rx[i],x[i]);
        memcpy(&(atoms->atom[i]),&(ra[i]),sizeof(t_atom));
        atoms->atomname[i] = an[i];
        cgnr[i] = ccgg[i];
        inv_renum[cg_renum[i]] = i;
        smn[i] = strdup(smnames[i]);
    }
    for(i=0;(i<atoms->nr); i++) 
    {
        newi = cg_renum[i];
        sfree(smnames[i]);
        smnames[i] = smn[newi];
    }
    for(i=0; (i<F_NRE); i++) 
    {
        for(j=0; (j<plist[i].nr); j++) 
        {
            for(k=0; (k<NRAL(i)); k++) 
            {
                plist[i].param[j].a[k] = inv_renum[plist[i].param[j].a[k]];
            }
        }
    }
    snew(newexcls,atoms->nr);
    for(i=0; (i<atoms->nr); i++) 
    {
        snew(newexcls[i].e,excls[i].nr);
        newexcls[i].nr = excls[i].nr;
        for(j=0; (j<excls[i].nr); j++)
            newexcls[i].e[j] = excls[i].e[j];
    }
    for(i=0; (i<atoms->nr); i++) 
    {
        newi = inv_renum[i];
        if (newexcls[i].nr > excls[newi].nr)
            srenew(excls[newi].e,newexcls[i].nr);
        for(j=0; (j<newexcls[i].nr); j++)
            excls[newi].e[j] = inv_renum[newexcls[i].e[j]];
        excls[newi].nr = newexcls[i].nr;
    }
    if (NULL != ndxout)
    {
        fp = fopen(ndxout,"w");
        fprintf(fp,"[ number_backward ]\n");
        for(j=0; (j<nmol); j++) 
        {
            j0 = j*atoms->nr;
            for(i=0; (i<atoms->nr); i++)
            {
                if (atoms->atom[inv_renum[i]].ptype == eptShell)
                    k = j0+inv_renum[i-1]+1;
                else
                    k = j0+inv_renum[i]+1;
                fprintf(fp," %d",k);
                if (j == 0)
                    cg_renum[inv_renum[i]] = i;
            }
            fprintf(fp,"\n");
        }
        for(j=0; (j<nmol); j++) 
        {
            j0 = j*atoms->nr;
            fprintf(fp,"[ number_forward ]\n");
            for(i=0; (i<atoms->nr); i++)
            {
                if (atoms->atom[cg_renum[i]].ptype == eptShell)
                    k = j0+cg_renum[i-1]+1;
                else
                    k = j0+cg_renum[i]+1;
                fprintf(fp," %d",k);
            }
            fprintf(fp,"\n");
        }
        fclose(fp);
    }
    sfree(rx);
    sfree(ra);
    sfree(an);
    sfree(cg_renum);
    sfree(inv_renum);
    sfree(ccgg);
}


