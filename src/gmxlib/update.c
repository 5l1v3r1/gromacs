/*
 *       $Id$
 *
 *       This source code is part of
 *
 *        G   R   O   M   A   C   S
 *
 * GROningen MAchine for Chemical Simulations
 *
 *            VERSION 2.0
 * 
 * Copyright (c) 1991-1997
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
 * 
 * Please refer to:
 * GROMACS: A message-passing parallel molecular dynamics implementation
 * H.J.C. Berendsen, D. van der Spoel and R. van Drunen
 * Comp. Phys. Comm. 91, 43-56 (1995)
 *
 * Also check out our WWW page:
 * http://rugmd0.chem.rug.nl/~gmx
 * or e-mail to:
 * gromacs@chem.rug.nl
 *
 * And Hey:
 * Gromacs Runs On Most of All Computer Systems
 */
static char *SRCID_update_c = "$Id$";

#include <stdio.h>
#include <math.h>

#include "assert.h"
#include "sysstuff.h"
#include "smalloc.h"
#include "typedefs.h"
#include "nrnb.h"
#include "led.h"
#include "physics.h"
#include "invblock.h"
#include "macros.h"
#include "vveclib.h"
#include "vec.h"
#include "main.h"
#include "confio.h"
#include "update.h"
#include "random.h"
#include "futil.h"
#include "mshift.h"
#include "tgroup.h"
#include "force.h"
#include "names.h"
#include "txtdump.h"
#include "mdrun.h"
#include "copyrite.h"
#include "edsam.h"

void calc_pres(matrix box,tensor ekin,tensor vir,tensor pres)
{
  int  n,m;
  real fac;

  /* Uitzoeken welke ekin hier van toepassing is, zie Evans & Morris - E. */ 
  /* Wrs. moet de druktensor gecorrigeerd worden voor de netto stroom in het */
  /* systeem...       */
  fac=2.0/(PRESFAC*det(box));
  for(n=0; (n<DIM); n++)
    for(m=0; (m<DIM); m++)
      pres[n][m]=(ekin[n][m]-vir[n][m])*fac;
#ifdef DEBUG
  pr_rvecs(stdlog,0,"pres",pres,DIM);
  pr_rvecs(stdlog,0,"ekin",ekin,DIM);
  pr_rvecs(stdlog,0,"vir ",vir, DIM);
#endif
}

real calc_temp(real ekin,int nrdf)
{
  return (2.0*ekin)/(nrdf*BOLTZ);
}

void tcoupl(bool bTC,t_grpopts *opts,t_groups *grps,real dt,real lamb)
{
  int  i;
  real T,reft,lll;

  for(i=0; (i<opts->ngtc); i++) {
    reft=opts->ref_t[i]*lamb;
    if (reft < 0)
      reft=0;
    T=grps->tcstat[i].T;
    if ((bTC) && (T != 0.0)) {
      lll=sqrt(1.0 + (dt/opts->tau_t[i])*(reft/T-1.0));
      grps->tcstat[i].lambda=max(min(lll,1.25),0.8);
    }
    else
      grps->tcstat[i].lambda=1.0;
#ifdef DEBUGTC
    fprintf(stdlog,"group %d: T: %g, Lambda: %g\n",
	    i,T,grps->tcstat[i].lambda);
#endif
  }
}

static void do_pcoupl(t_inputrec *ir,tensor pres,
		      matrix box,int start,int nr_atoms,
		      rvec x[],ushort cFREEZE[],
		      t_nrnb *nrnb,rvec freezefac[])
{
  int    n,d,m,g,ncoupl=0;
  real   scalar_pressure;
  real   X,Y,Z,dx,dy,dz;
  rvec   factor;
  tensor mu;
  real   muxx,muxy,muxz,muyx,muyy,muyz,muzx,muzy,muzz;
  real   fgx,fgy,fgz;
  
  /*
   *  PRESSURE SCALING 
   *  Step (2P)
   */
  scalar_pressure = (trace(pres))/3.0;

  if ((ir->epc != epcNO) && (scalar_pressure != 0.0)) {
    for(m=0; (m<DIM); m++)
      factor[m] = ir->compress[m]*ir->delta_t/ir->tau_p;
    clear_mat(mu);
    switch (ir->epc) {
    case epcISOTROPIC:
      for(m=0; (m<DIM); m++)
	mu[m][m] = 
	  pow(1.0-factor[m]*(ir->ref_p[m]-scalar_pressure),1.0/3.0);
      break;
    case epcANISOTROPIC:
      for (m=0; (m<DIM); m++)
	mu[m][m] = pow(1.0-factor[m]*(ir->ref_p[m] - pres[m][m]),1.0/3.0);
      break;
    case epcTRICLINIC:
    default:
      fprintf(stderr,"Pressure coupling type %s not supported yet\n",
	      EPCOUPLTYPE(ir->epc));
      exit(1);
    }
#ifdef DEBUG
    pr_rvecs(stdlog,0,"mu  ",mu,DIM);
#endif
    /* Scale the positions using matrix operation */
    nr_atoms+=start;
    muxx=mu[XX][XX],muxy=mu[XX][YY],muxz=mu[XX][ZZ];
    muyx=mu[YY][XX],muyy=mu[YY][YY],muyz=mu[YY][ZZ];
    muzx=mu[ZZ][XX],muzy=mu[ZZ][YY],muzz=mu[ZZ][ZZ];
    for (n=start; (n<nr_atoms); n++) {
      g=cFREEZE[n];
      fgx=freezefac[g][XX];
      fgy=freezefac[g][YY];
      fgz=freezefac[g][ZZ];
      
      X=x[n][XX];
      Y=x[n][YY];
      Z=x[n][ZZ];
      dx=muxx*X+muxy*Y+muxz*Z;
      dy=muyx*X+muyy*Y+muyz*Z;
      dz=muzx*X+muzy*Y+muzz*Z;
      x[n][XX]=X+fgx*(dx-X);
      x[n][YY]=Y+fgy*(dy-Y);
      x[n][ZZ]=Z+fgz*(dz-Z);
      
      ncoupl++;
    }
    /* compute final boxlengths */
    for (d=0; (d<DIM); d++)
      for (m=0; (m<DIM); m++)
	box[d][m] *= mu[d][m];
  }
  inc_nrnb(nrnb,eNR_PCOUPL,ncoupl);
}

static void shake_error(t_mdatoms *md,
			t_atoms *atoms,int start,int homenr,
			rvec x[],rvec xp[],
			rvec v[],rvec f[],
			matrix box)
{
  int  i,rnr;
  rvec fcp;

  clear_rvec(fcp);
  fprintf(stderr,"SHAKE ERROR\n");
  fprintf(stdlog,"Res# Resnm  Anm Atm#%8s%8s%8s%8s%8s%8s%8s%8s%8s%8s%8s%8s\n",
	  "Xold","Yold","Zold","Xnew","Ynew","ZNew",
	  "VX","VY","VZ","FX","FY","FZ");
  for(i=start; (i<start+homenr); i++) {
    rnr=md->resnr[i];
    if (f)
      copy_rvec(f[i],fcp);
    fprintf(stdlog,"%5d%5s%5s%5d%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f%8.3f\n",
	    rnr+1,*atoms->resname[rnr],*atoms->atomname[i],i+1,
	    x[i][XX],x[i][YY],x[i][ZZ],xp[i][XX],xp[i][YY],xp[i][ZZ],
	    v[i][XX],v[i][YY],v[i][ZZ],fcp[XX],fcp[YY],fcp[ZZ]);
  }
  fflush(stdlog);
}

static void calc_g(rvec x_unc,rvec x_cons,rvec g,double mdt_2)
{
  int d;
  
  for(d=0; (d<DIM);d++) 
    g[d]=(x_cons[d]-x_unc[d])*mdt_2;
}

static void do_shake_corr(rvec xold,rvec x,rvec v,double dt_1)
{
  int    d;

  for(d=0; (d<DIM);d++) 
    v[d]=((double) x[d]-(double) xold[d])*dt_1;
}

static void do_both(rvec xold,rvec x_unc,rvec x,rvec g,
		    rvec v,real mdt_2,real dt_1)
{
  real xx,yy,zz;

  xx=x[XX];
  yy=x[YY];
  zz=x[ZZ];
  g[XX]=(xx-x_unc[XX])*mdt_2;
  g[YY]=(yy-x_unc[YY])*mdt_2;
  g[ZZ]=(zz-x_unc[ZZ])*mdt_2;
  v[XX]=(xx-xold [XX])*dt_1;
  v[YY]=(yy-xold [YY])*dt_1;
  v[ZZ]=(zz-xold [ZZ])*dt_1;
}

/* do_elupdate is a function that takes into account electric field
   parameters. Et[] contains the parameters for the time dependent
   part of the field (not yet used). Ex[] contains the parameters for
   the spatial dependent part of the field. You can have cool periodic
   fields in principle, but only a constant field is supported
   now. Only difference with do_update is the electric field, the two
   should probably be merged when its working. Peter Tieleman, 30
   Nov. 1995 
*/

static void do_elupdate(int start,int homenr,double dt,
			rvec lamb[],t_grp_acc gstat[],
			rvec accel[],rvec freezefac[],
			real invmass[],real charge[],
			ushort cFREEZE[],ushort cACC[],
			ushort cTC[],
			rvec x[],rvec xprime[],rvec v[],rvec vold[],
			rvec f[], t_cosines Ex[], t_cosines Et[])
{
  double w_dt;
  int    gf,ga,gt;
  real   vn,vv,va,vb;
  real   qm;             /* qm = charge of atom over mass of atom. */
  real   e_tmp;         /* holds sum of all terms cos expansion for field */
  real   uold,l1,lg;
  real   real1=1.0;
  rvec   adds;          /* contributions from accel and field */
  int    n,d,i;
  
  for (n=start; (n<start+homenr); n++) {  
    w_dt = invmass[n]*dt;
    gf   = cFREEZE[n];
    ga   = cACC[n];
    gt   = cTC[n];
    qm   = invmass[n]  * charge[n] * FIELDFAC;

    for (d=0; d<DIM; d++)
    {
      e_tmp = 0;
      /* this is is silly, since Ex[d].n is either 1 or 0, currently */
      for (i = 0; i < Ex[d].n; i++)
	e_tmp += Ex[d].a[i] * qm;

      adds[d] = dt * (accel[ga][d] + e_tmp);
    }

    for (d=0; (d<DIM); d++) {
      vn             = v[n][d];
      lg             = lamb[gt][d];
      vold[n][d]     = vn;
      vv             = lg*(vn + f[n][d]*w_dt);
 
      /* do not scale the mean velocities u */
      uold           = gstat[ga].uold[d];
      va             = vv + adds[d];
      l1             = (real1-lg);
      vb             = va + l1*uold ;
      v[n][d]        = vb;
      xprime[n][d]   = x[n][d]+vb*dt*freezefac[gf][d];
    }
  }
}

static void do_update(int start,int homenr,double dt,
		      rvec lamb[],t_grp_acc gstat[],
		      rvec accel[],rvec freezefac[],
		      real invmass[],
		      ushort ptype[],
		      ushort cFREEZE[],ushort cACC[],
		      ushort cTC[],
		      rvec x[],rvec xprime[],rvec v[],rvec vold[],rvec f[])
{
  double w_dt;
  int    gf,ga,gt;
  real   vn,vv,va,vb;
  real   uold,l1,lg;
  real   real1=1.0;
  int    n,d;
  
  for (n=start; (n<start+homenr); n++) {  
    w_dt = invmass[n]*dt;
    gf   = cFREEZE[n];
    ga   = cACC[n];
    gt   = cTC[n];
    
    for (d=0; (d<DIM); d++) {
      vn             = v[n][d];
      lg             = lamb[gt][d];
      vold[n][d]     = vn;
      vv             = lg*(vn + f[n][d]*w_dt);
      
      /* do not scale the mean velocities u */
      uold           = gstat[ga].uold[d];
      va             = vv + accel[ga][d]*dt;
      l1             = (real1-lg);
      vb             = va + l1*uold ;
      v[n][d]        = vb;
      xprime[n][d]   = x[n][d]+vb*dt*freezefac[gf][d];
    }
  }
}

static void do_update_lang(int start,int homenr,double dt,
		      rvec x[],rvec xprime[],rvec v[],rvec vold[],rvec f[],
		      real temp, real fr, int *seed)
{
  const unsigned long im = 0xffff;
  const unsigned long ia = 1093;
  const unsigned long ic = 18257;
  real   vn,vv;
  real   rfac,invfr,rhalf,jr;
  int    n,d;
  ulong  jran;

  /* (r-0.5) n times:  var_n = n * var_1 = n/12
     n=4:  var_n = 1/3, so multiply with 3 */
  
  rfac  = sqrt(3.0 * 2.0*BOLTZ*temp/(fr*dt));
  rhalf = 2.0*rfac; 
  rfac  = rfac/(real)im;
  invfr = 1.0/fr;
  
  jran = (unsigned long)((real)im*rando(seed));

  for (n=start; (n<start+homenr); n++) {  
    for (d=0; (d<DIM); d++) {
      vn             = v[n][d];
      vold[n][d]     = vn;
      jran = (jran*ia+ic) & im;
      jr = (real)jran;
      jran = (jran*ia+ic) & im;
      jr += (real)jran;
      jran = (jran*ia+ic) & im;
      jr += (real)jran;
      jran = (jran*ia+ic) & im;
      jr += (real)jran;
      vv             = invfr*f[n][d] + rfac * jr - rhalf;
      v[n][d]        = vv;
      xprime[n][d]   = x[n][d]+v[n][d]*dt;
    }
  }
}

static void shake_calc_vir(FILE *log,int nxf,rvec x[],rvec f[],tensor vir,
                           t_commrec *cr)
{
  int    i,m,n;
  matrix dvir;
  
  clear_mat(dvir);
  for(i=0; (i<nxf); i++) {
    for(m=0; (m<DIM); m++)
      for(n=0; (n<DIM); n++)
        dvir[m][n]+=x[i][m]*f[i][n];
  }
  
  for(m=0; (m<DIM); m++)
    for(n=0; (n<DIM); n++)
      vir[m][n]-=0.5*dvir[m][n];
}

void dump_it_all(FILE *fp,char *title,
		 int natoms,rvec x[],rvec xp[],rvec v[],
		 rvec vold[],rvec f[])
{
#ifdef DEBUG
  fprintf(fp,"%s\n",title);
  pr_rvecs(fp,0,"x",x,natoms);
  pr_rvecs(fp,0,"xp",xp,natoms);
  pr_rvecs(fp,0,"v",v,natoms);
  pr_rvecs(fp,0,"vold",vold,natoms);
  pr_rvecs(fp,0,"f",f,natoms);
#endif
}

void calc_ke_part(bool bFirstStep,int start,int homenr,
		  rvec vold[],rvec v[],rvec vt[],
		  t_grpopts *opts,t_mdatoms *md,t_groups *grps,
		  t_nrnb *nrnb,real lambda,real *dvdlambda)
{
  int          g,d,n,ga,gt;
  rvec         v_corrt;
  real         hm,vvt,vct;
  t_grp_tcstat *tcstat=grps->tcstat;
  t_grp_acc    *grpstat=grps->grpstat;
  real         dvdl;

  /* group velocities are calculated in update_grps and
   * accumulated in acumulate_groups.
   * Now the partial global and groups ekin.
   */
  for (g=0; (g<opts->ngtc); g++)
    clear_mat(grps->tcstat[g].ekin); 
    
  if (bFirstStep) {
    for(n=start; (n<start+homenr); n++) {
      copy_rvec(v[n],vold[n]);
    }
    for (g=0; (g<opts->ngacc); g++) {
      for(d=0; (d<DIM); d++)
	grps->grpstat[g].ut[d]=grps->grpstat[g].u[d];
    }
  }
  else {
    for (g=0; (g<opts->ngacc); g++) { 
      for(d=0; (d<DIM); d++)
	grps->grpstat[g].ut[d]=0.5*(grps->grpstat[g].u[d]+
				    grps->grpstat[g].uold[d]);
    }
  }

  dvdl = 0;
  for(n=start; (n<start+homenr); n++) {  
    ga   = md->cACC[n];
    gt   = md->cTC[n];
    hm   = 0.5*md->massT[n];
    
    for(d=0; (d<DIM); d++) {
      vvt        = 0.5*(v[n][d]+vold[n][d]);
      vt[n][d]   = vvt;
      vct        = vvt - grpstat[ga].ut[d];
      v_corrt[d] = vct;
    }
    for(d=0; (d<DIM); d++) {
      tcstat[gt].ekin[XX][d]+=hm*v_corrt[XX]*v_corrt[d];
      tcstat[gt].ekin[YY][d]+=hm*v_corrt[YY]*v_corrt[d];
      tcstat[gt].ekin[ZZ][d]+=hm*v_corrt[ZZ]*v_corrt[d];
    }
    if (md->bPerturbed[n]) {
      dvdl+=0.5*(md->massB[n]-md->massA[n])*iprod(v_corrt,v_corrt);
    }
  }
  *dvdlambda += dvdl;
  
#ifdef DEBUG
  fprintf(stdlog,"ekin: U=(%12e,%12e,%12e)\n",
	  grpstat[0].ut[XX],grpstat[0].ut[YY],grpstat[0].ut[ZZ]);
  fprintf(stdlog,"ekin: %12e\n",trace(tcstat[0].ekin));
#endif

  inc_nrnb(nrnb,eNR_EKIN,homenr);
}

typedef struct {
  atom_id iatom[3];
  atom_id blocknr;
} t_sortblock;

#ifdef DEBUG
static void pr_sortblock(FILE *fp,char *title,int nsb,t_sortblock sb[])
{
  int i;
  
  fprintf(fp,"%s\n",title);
  for(i=0; (i<nsb); i++)
    fprintf(fp,"i: %5d, iatom: (%5d %5d %5d), blocknr: %5d\n",
	    i,sb[i].iatom[0],sb[i].iatom[1],sb[i].iatom[2],
	    sb[i].blocknr);
}
static int pcount=0;
#endif

int pcomp(const void *p1, const void *p2)
{
  int     db;
  atom_id min1,min2,max1,max2;
  t_sortblock *a1=(t_sortblock *)p1;
  t_sortblock *a2=(t_sortblock *)p2;
#ifdef DEBUG
  pcount++;
#endif
  
  db=a1->blocknr-a2->blocknr;
  
  if (db != 0)
    return db;
    
  min1=min(a1->iatom[1],a1->iatom[2]);
  max1=max(a1->iatom[1],a1->iatom[2]);
  min2=min(a2->iatom[1],a2->iatom[2]);
  max2=max(a2->iatom[1],a2->iatom[2]);
  
  if (min1 == min2)
    return max1-max2;
  else
    return min1-min2;
}

static int icomp(const void *p1, const void *p2)
{
  atom_id *a1=(atom_id *)p1;
  atom_id *a2=(atom_id *)p2;

  return (*a1)-(*a2);
}

void init_update(FILE *log,t_topology *top,t_inputrec *ir,
		 t_mdatoms *md,
		 int start,int homenr,
		 int *nbl,int **sbl,
		 int *nset,int **owp,int *settle_tp)
{
  t_sortblock *sb;
  t_block     *blocks=&(top->blocks[ebSBLOCKS]);
  t_idef      *idef=&(top->idef);
  t_iatom     *iatom;
  atom_id     *inv_sblock;
  int         i,j,m,bnr;
  int         ncons,bstart;
  int         settle_type;
  
  /* Output variables, initiate them right away */
  int         nblocks=0;
  int         *sblock=NULL;
  int         nsettle=0;
  int         *owptr=NULL;

  if ((ir->btc) || (ir->epc != epcNO))
    please_cite(log,"Berendsen84a");
  
  /* Put the oxygen atoms in the owptr array */
  nsettle=idef->il[F_SETTLE].nr/2;
  if (nsettle > 0) {
    snew(owptr,nsettle);
    settle_type=idef->il[F_SETTLE].iatoms[0];
    *settle_tp=settle_type;
    for (j=0; (j<idef->il[F_SETTLE].nr); j+=2) {
      if (idef->il[F_SETTLE].iatoms[j] != settle_type)
	fatal_error(0,"More than one settle type (%d and %d)",
		    settle_type,idef->il[F_SETTLE].iatoms[j]);
      owptr[j/2]=idef->il[F_SETTLE].iatoms[j+1];
#ifdef DEBUG
      fprintf(log,"owptr[%d]=%d\n",j/2,owptr[j/2]);
#endif
    }
    /* We used to free this memory, but ED sampling needs it later on 
     *  sfree(idef->il[F_SETTLE].iatoms);
     */
    
    please_cite(log,"Miyamoto92a");
  }
  
  ncons=idef->il[F_SHAKE].nr/3;
  if (ncons > 0) {
    bstart=(idef->pid > 0) ? blocks->multinr[idef->pid-1] : 0;
    nblocks=blocks->multinr[idef->pid] - bstart;
#ifdef DEBUGIDEF
    fprintf(stdlog,"ncons: %d, bstart: %d, nblocks: %d\n",
	    ncons,bstart,nblocks);
    fflush(stdlog);
#endif
    
    /* Calculate block number for each atom */
    inv_sblock=make_invblock(blocks,md->nr);

    /* Store the block number in temp array and
     * sort the constraints in order of the sblock number 
     * and the atom numbers, really sorting a segment of the array!
     */
#ifdef DEBUGIDEF 
    pr_idef(stdlog,0,"Before Sort",idef);
#endif
    iatom=idef->il[F_SHAKE].iatoms;
    snew(sb,ncons);
    for(i=0; (i<ncons); i++,iatom+=3) {
      for(m=0; (m<3); m++)
	sb[i].iatom[m]=iatom[m];
      sb[i].blocknr=inv_sblock[iatom[1]];
    }

    /* Now sort the blocks */
#ifdef DEBUG
    pr_sortblock(log,"Before sorting",ncons,sb);
    fprintf(log,"Going to sort constraints\n");
#endif

    qsort(sb,ncons,(size_t)sizeof(*sb),pcomp);
    
#ifdef DEBUG
    fprintf(log,"I used %d calls to pcomp\n",pcount);
    pr_sortblock(log,"After sorting",ncons,sb);
#endif

    iatom=idef->il[F_SHAKE].iatoms;
    for(i=0; (i<ncons); i++,iatom+=3) 
      for(m=0; (m<DIM); m++)
	iatom[m]=sb[i].iatom[m];
#ifdef DEBUGIDEF
    pr_idef(stdlog,0,"After Sort",idef);
#endif
    
    j=0;
    snew(sblock,nblocks+1);
    bnr=-2;
    for(i=0; (i<ncons); i++) {
      if (sb[i].blocknr != bnr) {
	bnr=sb[i].blocknr;
	sblock[j++]=3*i;
      }
    }
    /* Last block... */
    sblock[j++]=3*ncons;
    
    if (j != (nblocks+1)) {
      fprintf(stdlog,"bstart: %d\n",bstart);
      fprintf(log,"j: %d, nblocks: %d, ncons: %d\n",
	      j,nblocks,ncons);
      for(i=0; (i<ncons); i++)
	fprintf(log,"i: %5d  sb[i].blocknr: %5u\n",i,sb[i].blocknr);
      for(j=0; (j<=nblocks); j++)
	fprintf(log,"sblock[%3d]=%5d\n",j,(int) sblock[j]);
      exit(1);
    }
    sfree(sb);
    sfree(inv_sblock);
    
    please_cite(log,"Ryckaert77a");
  }
  
  /* Copy pointers */
  *nbl=nblocks;
  *sbl=sblock;
  *nset=nsettle;
  *owp=owptr;
}


void init_project(FILE *log,t_topology *top,t_inputrec *ir,
		  t_mdatoms *md,int start,int homenr,
		  int *nbl,int **sbl,
		  int *nset,int **owp,int *settle_tp,
		  int *ncm,int *cmax,
		  rvec **r,int **bla1,int **bla2,int **blnr,int **blbnb,
		  real **bllen,real **blc,real **blcc,real **blm,
		  real **tmp1,real **tmp2,real **tmp3,
		  real **lincslam,real **bllen0,real **ddist)
{
  t_idef      *idef=&(top->idef);
  t_iatom     *iatom;
  int         i,j,k,n,b1,b,cen;
  int         ncons;
  int         type,a1,a2,b2,nr,n1,n2,nc4;
  real        len,len1,sign;
  real        im1,im2,imcen;
  
  ncons=idef->il[F_SHAKE].nr/3;

  if (ncons > 0) {

    /* Make constraint-neighbour list */

    snew(*r,ncons);
    snew(*bla1,ncons);
    snew(*bla2,ncons);
    snew(*blnr,ncons);
    snew(*bllen,ncons);
    snew(*blc,ncons);
    snew(*tmp1,ncons);
    snew(*tmp2,ncons);
    snew(*tmp3,ncons);
    snew(*lincslam,ncons);
    snew(*bllen0,ncons);
    snew(*ddist,ncons);
    
    iatom=idef->il[F_SHAKE].iatoms;

    /* Number of coupling of a bond is defined as the number of
       bonds directly connected to that bond (not to an atom!).
       The constraint are divided into two groups, the first
       group consists of bonds with 4 or less couplings, the
       second group consists of bonds with more than 4 couplings
       (in proteins most of the bonds have 2 or 4 couplings). 
       
       cmax: maximum number of bonds coupled to one bond 
       ncm:  number of bonds with mor than 4 couplings 
       */

    *cmax=0;
    n1=0;
    n2=ncons-1;

    for(i=0; (i<ncons); i++) {
      j=3*i;
      a1=iatom[j+1];
      a2=iatom[j+2];
      nr=0;
      for(k=0; (k<ncons); k++) {
	b1=iatom[3*k+1];
	b2=iatom[3*k+2];
	if ((a1==b1 || a1==b2) || (a2==b1 || a2==b2)) 
	  if (i != k) nr++;
      }
      if (nr > *cmax) *cmax=nr;
      type=iatom[j];
      len =idef->iparams[type].shake.dA;
      len1=idef->iparams[type].shake.dB;
      if (nr <=4) {
	(*bla1)[n1]=a1;
	(*bla2)[n1]=a2;
	(*bllen)[n1]=len;
	(*bllen0)[n1]=len;
	(*ddist)[n1]=len1-len;
	n1++;
      }
      else {
	(*bla1)[n2]=a1;
	(*bla2)[n2]=a2;
	(*bllen)[n2]=len;
	(*bllen0)[n2]=len;
	(*ddist)[n2]=len1-len;
	n2--;
      }
    }
    
    *ncm=ncons-n1;
    nc4=(*cmax-4)*n1;

    i=4*n1+(*cmax)*(*ncm);
    snew(*blbnb,i); 
    snew(*blcc,i);
    snew(*blm,i); 

    for(i=0; (i<ncons); i++) {
      a1=(*bla1)[i];
      a2=(*bla2)[i];
      im1=md->invmass[a1];
      im2=md->invmass[a2];
      (*blc)[i]=invsqrt(im1+im2);
      /* printf("%d %d %f\n",a1,a2,blist[i].len); */
      nr=0;
      /* printf("\n%d",i); */
      for(k=0; (k<ncons); k++) {
	b1=(*bla1)[k];
	b2=(*bla2)[k];
	if ((a1==b1 || a1==b2) || (a2==b1 || a2==b2)) 
	  if (i != k) {
	    if (i<n1)
	      (*blbnb)[4*i+nr]=k;
	    else
	      (*blbnb)[(*cmax)*i-nc4+nr]=k;
	    nr++;
	    /* printf(" %d",k); */
	  }
      }
      (*blnr)[i]=nr;
      /* printf("\n"); */
    }
    fprintf(stdlog,"\nInitializing LINear Constraint Solver\n");
    fprintf(stdlog,"%d constraints\nof which %d with more than 4 neighbours\n",ncons,*ncm);
    fprintf(stdlog,"maximum number of bonds coupled to one bond is %d\n\n",*cmax);
    fflush(stdlog);
 
    for(b=0; (b<ncons); b++) {
      i=(*bla1)[b];
      j=(*bla2)[b];
      nr=(*blnr)[b];
      if (nr) 
	for(n=0; (n<nr);n++) {
	  if (b < n1) 
	    k=(*blbnb)[4*b+n];
	  else
	    k=(*blbnb)[(*cmax)*b-nc4+n];
	  if (i==(*bla1)[k] || j==(*bla2)[k])
	    sign=-1;
	    else
	    sign=1;
	  if (i==(*bla1)[k] || i==(*bla2)[k])
	    cen=i;
	  else
	    cen=j;
	  if (ir->eI==eiMD) {
	    imcen=md->invmass[cen];
	    len=sign*imcen*(*blc)[b]*(*blc)[k];
	  }
	  if (ir->eI==eiLD) 
	    len=sign*0.5;
	  if (b<n1) 
	    (*blcc)[4*b+n]=len;
	  else
	    (*blcc)[(*cmax)*b-nc4+n]=len;
	}
    }
    
  }
}

void update(int          natoms, 	/* number of atoms in simulation */
	    int      	 start,
	    int          homenr,	/* number of home particles 	*/
	    int          step,
	    real         lambda,
	    real         *dvdlambda, /* FEP stuff */
	    t_inputrec   *ir,           /* input record with constants 	*/
	    bool         bFirstStep,   
	    t_mdatoms    *md,
	    rvec         x[],	/* coordinates of home particles */
	    t_graph      *graph,
	    rvec         shift_vec[],	
	    rvec         force[], 	/* forces on home particles 	*/
	    rvec         delta_f[],
	    rvec         vold[],	/* Old velocities		   */
	    rvec         v[], 		/* velocities of home particles */
	    rvec         vt[],  	/* velocity at time t 		*/
	    tensor       pressure, 	/* instantaneous pressure tensor */
	    tensor       box,  		/* instantaneous box lengths 	*/
	    t_topology   *top,
	    t_groups     *grps,
	    tensor       vir_part,
	    t_commrec    *cr,
	    t_nrnb       *nrnb,
	    bool         bTYZ,
	    bool         bDoUpdate,
	    t_edsamyn    *edyn)
{
  static char      buf[256];
  static bool      bFirst=TRUE;
  static int       nblocks=0;
  static int       *sblock=NULL;
  static int       nsettle,settle_type;
  static int       *owptr;
  static bool      bField=FALSE;  /* true if field is used */
  static rvec      *xprime,*x_unc=NULL;
  static int       ngtc,ngacc,ngfrz;
  static rvec      *lamb,*freezefac;
  static int       *bla1,*bla2,*blnr,*blbnb;
  static rvec      *r;
  static real      *bllen,*blc,*blcc,*blm,*tmp1,*tmp2,*tmp3,*lincslam,
                   *bllen0,*ddist;
  static int       ncm,cmax;
  static t_edpar   edpar;

  t_idef           *idef=&(top->idef);
  double           dt;
  real             dt_1,dt_2;
  int              i,n,m,g;
  int              ncons=0,nc;
  int              warn,p_imax;
  real             wang,p_max,p_rms;

  set_led(UPDATE_LED);

  if (bFirst) {
    init_update(stdlog,top,ir,md,
		start,homenr,
		&nblocks,&sblock,
		&nsettle,&owptr,&settle_type);
    if (ir->eShakeType == estLINCS) {
      please_cite(stdlog,"Hess97a");
      init_project(stdlog,top,ir,md,
		   start,homenr,
		   &nblocks,&sblock,
		   &nsettle,&owptr,&settle_type,
		   &ncm,&cmax,
		   &r,&bla1,&bla2,&blnr,&blbnb,
		   &bllen,&blc,&blcc,&blm,&tmp1,&tmp2,&tmp3,&lincslam,
		   &bllen0,&ddist);
    }
    if (edyn->bEdsam) 
      init_edsam(stdlog,top,md,start,homenr,&nblocks,&sblock,x,box,
		 edyn,&edpar);

    /* Allocate memory for xold, original atomic positions
     * and for xprime.
     */
    snew(xprime,natoms);
    snew(x_unc,homenr);

    /* Freeze Factor: If a dimension of a group has to be frozen,
     * the corresponding freeze fac will be 0.0 otherwise 1.0
     * This is implemented by multiplying the CHANGE in position
     * by freeze fac (also in do_pcoupl)
     *
     * Coordinates in shake can be frozen by setting the invmass
     * of a particle to 0.0 (===> Infinite mass!)
     */
    ngfrz=ir->opts.ngfrz;
    snew(freezefac,ngfrz);
    for(n=0; (n<ngfrz); n++)
      for(m=0; (m<DIM); m++) {
	freezefac[n][m]=(ir->opts.nFreeze[n][m]==0) ? 1.0 : 0.0;
      }
    /* Copy the pointer to the external acceleration in the opts */
    ngacc=ir->opts.ngacc;
    
    ngtc=ir->opts.ngtc;
    snew(lamb,ir->opts.ngtc);
    
    /* find out if we have to take into account electric fields */
    if (ir->ex[0].n || ir->ex[1].n || ir->ex[2].n) {
      bField = TRUE;
      fprintf(stdlog,"Using field version of update\n");
    }
    /* done with initializing */
    bFirst=FALSE;
  }
  
  dt   = ir->delta_t;
  dt_1 = 1.0/dt;
  dt_2 = 1.0/(dt*dt);
  
  for(i=0; (i<ngtc); i++) {
    real l=grps->tcstat[i].lambda;
    
    if (bTYZ)
      lamb[i][XX]=1;
    else
      lamb[i][XX]=l;
    lamb[i][YY]=l;
    lamb[i][ZZ]=l;
  }

  if (bDoUpdate) {  
    /* update mean velocities */
    for (g=0; (g<ngacc); g++) {
      copy_rvec(grps->grpstat[g].u,grps->grpstat[g].uold);
      clear_rvec(grps->grpstat[g].u);
    }
    
    /* Now do the actual update of velocities and positions */
    where();
    dump_it_all(stdlog,"Before update",natoms,x,xprime,v,vold,force);
    if (bField)  
      /* use field version of update */
      do_elupdate(start,homenr,dt, 
		  lamb, grps->grpstat,
		  ir->opts.acc,freezefac,
		  md->invmass,md->chargeA,
		  md->cFREEZE,md->cACC,md->cTC,
		  x,xprime,v,vold,force,ir->ex,ir->et);
    else {
      /* use normal version of update */
      
      if (ir->eI==eiMD)
	do_update(start,homenr,dt,
		lamb,grps->grpstat,
		ir->opts.acc,freezefac,
		md->invmass,md->ptype,
		md->cFREEZE,md->cACC,md->cTC,
		x,xprime,v,vold,force);
      if (ir->eI==eiLD) 
	do_update_lang(start,homenr,dt,
		x,xprime,v,vold,force,
		ir->ld_temp,ir->ld_fric,&ir->ld_seed);
    }

    where();
    inc_nrnb(nrnb,eNR_UPDATE,homenr);
    dump_it_all(stdlog,"After update",natoms,x,xprime,v,vold,force);
  }
  else {
    /* If we're not updating we're doing shakefirst!
     * In this case the extra coordinates are passed in v array
     */
    for(n=start; (n<start+homenr); n++) {
      copy_rvec(v[n],xprime[n]);
    }
  }

  /* 
   *  Steps (7C, 8C)
   *  APPLY CONSTRAINTS:
   *  BLOCK SHAKE 
   */
 
  if ((nblocks > 0) || (nsettle > 0)) {
    /* Copy Unconstrained X to temp array */
    for(n=start; (n<start+homenr); n++)
      copy_rvec(xprime[n],x_unc[n-start]);

    
    if (nblocks > 0) {
      where();

      nc=idef->il[F_SHAKE].nr/3;

      if (ir->eShakeType == estSHAKE)
	ncons=bshakef(stdlog,natoms,md->invmass,nblocks,sblock,idef,
		      ir,box,x,xprime,nrnb);
      
      if (ir->eShakeType == estLINCS) {
	
	if (ir->bPert) {
	  for(i=0;i<nc;i++)
	    bllen[i]=bllen0[i]+lambda*ddist[i];
	}  

	wang=ir->LincsWarnAngle;

	if (do_per_step(step,ir->nstLincsout)) {
#ifdef USEF77
	  CALLF77(fconerr)(&p_max,&p_rms,&p_imax,xprime,&nc,bla1,bla2,bllen);
#else
	  cconerr(&p_max,&p_rms,&p_imax,xprime,nc,bla1,bla2,bllen);
#endif
	}

	if (ir->eI==eiMD) {
#ifdef USEF77
	  CALLF77(flincs) (x[0],xprime[0],&nc,&ncm,&cmax,bla1,bla2,blnr,blbnb,
			   bllen,blc,blcc,blm,&ir->nProjOrder,
			   md->invmass,r[0],tmp1,tmp2,tmp3,&wang,&warn,
			   lincslam);
#else
	  clincs(x,xprime,nc,ncm,cmax,bla1,bla2,blnr,blbnb,
		 bllen,blc,blcc,blm,ir->nProjOrder,
		 md->invmass,r,tmp1,tmp2,tmp3,wang,&warn,lincslam);
#endif
	  if (ir->bPert) {
	    real dvdl=0;
	    
	    for(i=0; (i<nc); i++)
	      dvdl+=lincslam[i]*dt_2*ddist[i];
	    *dvdlambda+=dvdl;
	  }
	}

	if (ir->eI==eiLD) {
#ifdef USEF77
	  CALLF77(flincsld) (x[0],xprime[0],&nc,&ncm,&cmax,bla1,bla2,blnr,
			     blbnb,bllen,blcc,blm,&ir->nProjOrder,
			     r[0],tmp1,tmp2,tmp3,&wang,&warn);
#else
	  clincsld(x,xprime,nc,ncm,cmax,bla1,bla2,blnr,
		   blbnb,bllen,blcc,blm,ir->nProjOrder,
		   r,tmp1,tmp2,tmp3,wang,&warn);
#endif
	}

	if (do_per_step(step,ir->nstLincsout)) {
	  fprintf(stdlog,"Step %d\nRel. Constraint Deviation:  Max    between atoms     RMS\n",step);
	  fprintf(stdlog,"    Before LINCS         %.6f %6d %6d   %.6f\n",p_max,bla1[p_imax],bla2[p_imax],p_rms);
#ifdef USEF77
	  CALLF77(fconerr)(&p_max,&p_rms,&p_imax,xprime,&nc,bla1,bla2,bllen);
#else
	  cconerr(&p_max,&p_rms,&p_imax,xprime,nc,bla1,bla2,bllen);
#endif
	  fprintf(stdlog,"     After LINCS         %.6f %6d %6d   %.6f\n\n",
		    p_max,bla1[p_imax],bla2[p_imax],p_rms);
	}

	if (warn > 0) {
#ifdef USEF77
	  CALLF77(fconerr)(&p_max,&p_rms,&p_imax,xprime,&nc,bla1,bla2,bllen);
#else
	  cconerr(&p_max,&p_rms,&p_imax,xprime,nc,bla1,bla2,bllen);
#endif
	  sprintf(buf,"Step %d  WARNING\n"
		  "bond between atoms %d and %d rotated more than %.1f degrees\n"
		  "relative constraint deviation after LINCS:\n"
		  "max %.6f (between atoms %d and %d) rms %.6f\n\n",
		  step,
		  bla1[warn-1],bla2[warn-1],wang,
		  p_max,bla1[p_imax],bla2[p_imax],p_rms);
	  fprintf(stdlog,"%s",buf);
	  fprintf(stderr,"%s",buf);
	}
      }

      where();

      
      if (ncons == -1) {
	shake_error(md,&(top->atoms),start,homenr,x,xprime,v,force,box);
	exit(1);
      }
      
      dump_it_all(stdlog,"After Shake",natoms,x,xprime,v,vold,force);
    }

    /* apply Essetial Dynamics constraints when required */
    if (edyn->bEdsam)
      do_edsam(stdlog,top,ir,step,md,start,homenr,&nblocks,&sblock,xprime,x,
	       x_unc,force,box,edyn,&edpar,bDoUpdate);
    
    if (nsettle > 0) {
      int  ow1;
      real mO,mH,dOH,dHH;
      
      ow1  = owptr[0];
      mO   = md->massA[ow1];
      mH   = md->massA[ow1+1];
      dOH  = top->idef.iparams[settle_type].settle.doh;
      dHH  = top->idef.iparams[settle_type].settle.dhh;
#ifdef USEF77
      CALLF77(fsettle) (&nsettle,owptr,x[0],xprime[0],&dOH,&dHH,&mO,&mH);
#else
      csettle(stdlog,nsettle,owptr,x[0],xprime[0],dOH,dHH,mO,mH);
#endif
      inc_nrnb(nrnb,eNR_SETTLE,nsettle);
      where();
    }
    if (bDoUpdate) {
      real mdt_2;
      
      for(n=start; (n<start+homenr); n++) {
	mdt_2 = dt_2*md->massT[n];
	do_both(x[n],x_unc[n-start],xprime[n],delta_f[n],
		v[n],mdt_2,dt_1);
      }
      where();

      inc_nrnb(nrnb,eNR_SHAKE_V,homenr);
      dump_it_all(stdlog,"After Shake-V",natoms,x,xprime,v,vold,force);
      where();
      
      /* Calculate virial due to shake (for this proc) */
      calc_vir(stdlog,homenr,&(x[start]),&(delta_f[start]),vir_part,cr);
      inc_nrnb(nrnb,eNR_SHAKE_VIR,homenr);
      where();
    }
  }

  
  /* We must always unshift here, also if we did not shake */
  where();
  if ((graph->nnodes > 0) && bDoUpdate) {
    unshift_x(graph,shift_vec,x,xprime);
    inc_nrnb(nrnb,eNR_SHIFTX,graph->nnodes);
    for(n=start; (n<graph->start); n++)
      copy_rvec(xprime[n],x[n]);
    for(n=graph->start+graph->nnodes; (n<start+homenr); n++)
      copy_rvec(xprime[n],x[n]);
  }
  else {
    for(n=start; (n<start+homenr); n++)
      copy_rvec(xprime[n],x[n]);
  }
  dump_it_all(stdlog,"After unshift",natoms,x,xprime,v,vold,force);
  where();
  
  if (bDoUpdate) {  
    update_grps(start,homenr,grps,&(ir->opts),v,md);
    do_pcoupl(ir,pressure,box,start,homenr,x,md->cFREEZE,nrnb,
	      freezefac);
    where();
  }

  clr_led(UPDATE_LED);
}
