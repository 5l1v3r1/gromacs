/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gromacs/commandline/pargs.h"
#include "gromacs/fileio/xvgr.h"
#include "gromacs/math/calculate-ewald-splitting-coefficient.h"
#include "gromacs/math/vec.h"
#include "gromacs/mdtypes/md_enums.h"
#include "gromacs/utility/arraysize.h"
#include "gromacs/utility/cstringutil.h"
#include "gromacs/utility/futil.h"
#include "gromacs/utility/smalloc.h"

#include "poldata.h"
#include "poldata_xml.h"
#include "coulombintegrals/coulombintegrals.h"


using namespace alexandria;

enum {
    mGuillot2001a, mAB1, mLjc, mMaaren, mSlater, mGuillot_Maple, mHard_Wall, mDEC, mDEC_pair, mDEC_qd_q, mDEC_qd_qd, mDEC_q_q, mNR
};

static void filter_eemprops(std::vector<Eemprops>    *eemprops,
                            ChargeDistributionModel   iDistributionModel,
                            char                     *atypes)
{
    for (auto eep = eemprops->begin(); eep != eemprops->end();)
    {   
        if (eep->getEqdModel() != iDistributionModel)
        {
            eep = eemprops->erase(eep);
        }
        else
        {
            ++eep;
        }
    }
    if (nullptr != atypes)
    {
        std::vector<std::string> ptr = gmx::splitString(atypes);
        for (auto eep = eemprops->begin(); eep != eemprops->end();)
        {
            if (std::find(ptr.begin(), ptr.end(), eep->getName()) == ptr.end())
            {
                eep = eemprops->erase(eep);
            }
            else
            {
                ++eep;
            }
        }
    }
}

static double Coulomb_PP(double r)
{
    return 1/r;
}

static double DCoulomb_PP(double r)
{
    return -1/gmx::square(r);
}

static int faculty(int n)
{
    if (n <= 0)
    {
        return 1;
    }
    else
    {
        return n*faculty(n-1);
    }
}

static double erf2(double x)
{
    return -(4*x/(sqrt(M_PI)))*exp(-x*x);
}

static double erf1(double x)
{
    return (2/sqrt(M_PI))*exp(-x*x);
}

static void do_hard(FILE *fp, int pts_nm, double efac, double delta)
{
    int    i, imax;
    double x, vr, vr2, vc, vc2;

    if (delta < 0)
    {
        gmx_fatal(FARGS, "Delta should be >= 0 rather than %f\n", delta);
    }

    imax     = std::lround(3.0*pts_nm);
    for (i = 0; (i <= imax); i++)
    {
        x   =  i*(1.0/pts_nm);

        if (x < delta)
        {
            /* Avoid very high numbers */
            vc = vc2 = 1/delta;
        }
        else
        {
            vc  = 1/(x);
            vc2 = 2/pow(x, 3);
        }
        vr  = erfc(efac*(x-delta))/2;
        vr2 = (1-erf2(efac*(x-delta)))/2;
        fprintf(fp, "%15.10e  %15.10e  %15.10e  %15.10e  %15.10e  %15.10e  %15.10e\n",
                x, vr, vr2, 0.0, 0.0, vc, vc2);
    }

}

static void do_AB1(FILE *fp, int pts_nm, int ndisp, int nrep)
{
    int    i, k, imax;
    double myfac[3] = { 1, -1, 1 };
    double myexp[3] = { 1, 6, 0 };
    double x, v, v2;

    myexp[1] = ndisp;
    myexp[2] = nrep;
    imax     = (int) (3.0*pts_nm);
    for (i = 0; (i <= imax); i++)
    {
        x   =  i*(1.0/pts_nm);

        fprintf(fp, "%15.10e", x);

        for (k = 0; (k < 3); k++)
        {
            if (x < 0.04)
            {
                /* Avoid very high numbers */
                v = v2 = 0;
            }
            else
            {
                v  =  myfac[k]*pow(x, -myexp[k]);
                v2 =  (myexp[k])*v/(x);
            }
            fprintf(fp, "   %15.10e %15.10e", v, v2);
        }
        fprintf(fp, "\n");
    }
}

static void lo_do_ljc(double r,
                      double *vc, double *fc,
                      double *vd, double *fd,
                      double *vr, double *fr)
{
    double r2, r_6, r_12;

    r2    = r*r;
    r_6   = 1.0/(r2*r2*r2);
    r_12  = r_6*r_6;

    *vc   = 1.0/r;            /*  f(x)     Coulomb    */
    *fc   = 1.0/(r2);         /* -f'(x)               */

    *vd   = -r_6;             /*  g(c)     Dispersion */
    *fd   =  6.0*(*vd)/r;     /* -g'(x)               */

    *vr   = r_12;             /*  h(x)     Repulsion  */
    *fr   = 12.0*(*vr)/r;     /* -h'(x)               */
}

/* use with coulombtype = user */
static void lo_do_ljc_pme(double r,
                          double rcoulomb, double ewald_rtol,
                          double *vc, double *fc,
                          double *vd, double *fd,
                          double *vr, double *fr)
{
    double r2, r_6, r_12;
    double isp = 0.564189583547756;
    double ewc;

    ewc  = calc_ewaldcoeff_q(rcoulomb, ewald_rtol);

    r2   = r*r;
    r_6  = 1.0/(r2*r2*r2);
    r_12 = r_6*r_6;

    *vc   = erfc(ewc*r)/r;
    /* *vc2  = 2*erfc(ewc*r)/(r*r2)+4*exp(-(ewc*ewc*r2))*ewc*isp/r2+
       4*ewc*ewc*ewc*exp(-(ewc*ewc*r2))*isp;*/
    *fc  = 2*ewc*exp(-ewc*ewc*r2)*isp/r + erfc(ewc*r)/r2;

    *vd  = -r_6;
    *fd  = -6.0*(*vd)/r;

    *vr  = r_12;
    *fr  = 12.0*(*vr)/r;
}

static void lo_do_guillot(double r, double xi, double xir,
                          double *vc, double *fc,
                          double *vd, double *fd,
                          double *vr, double *fr)
{
    double qO     = -0.888;
    double qOd    =  0.226;
    double f0     = qOd/qO;
    double rxi1, rxi2, z;
    double r2, r_6;

    r2   = r*r;
    r_6  = 1.0/(r2*r2*r2);

    rxi1    = r/(2*xi);
    rxi2    = r/(sqrt(2)*xi);
    *vc     = (1 + f0*f0*erf(r/(2*xi)) + 2*f0*erf(r/(sqrt(2)*xi)) )/r;

    *fc   =  f0*f0*erf(r/(2*xi)) + 2*f0*erf(r/(sqrt(2)*xi));
    ;
    /* MuPad: Uc := erf(r/(2*xi))/r +

       Mathematica:
       r1 := r/(2*xi);
       r2 := r/(Sqrt[2] * xi);
       Uc[r_] := (1 + f0 * f0 * Erf[r/(2*xi)] + 2 * f0 * Erf[r/(Sqrt[2]*xi)]) / r;
       -D[Uc[r],r]
       CForm=
       -(((2*f0*Sqrt(2/Pi))/(Power(E,Power(r,2)/(2.*Power(xi,2)))*xi) +
       Power(f0,2)/(Power(E,Power(r,2)/(4.*Power(xi,2)))*Sqrt(Pi)*xi))/r) +
       (1 + Power(f0,2)*Erf(r/(2.*xi)) + 2*f0*Erf(r/(Sqrt(2)*xi)))/Power(r,2)


       Uc1[r_] := 1/r;
       -D[Uc1[r],r]
       -2
       Out[20]= r

       Uc2[r_] := f0^2 * Erf[r1] / r;
       -D[Uc2[r],r]


       Uc3[r_] := 2 * f0 * Erf[r2]/ r;
       -D[Uc3[r],r]

       Uc[r_] := Erf[r/(2*xi)] / r

       D[Uc[r],r]


       D[Erf[r],r]

     */
    *vc   = (1 + gmx::square(f0)*erf(rxi1) + 2*f0*erf(rxi2))/r;
    *fc   =
        (1/r
         + (-f0 * (2 * sqrt(2) + exp(r2/4*xi*xi)*f0)/(exp(r2/(2*xi*xi))*sqrt(M_PI)*xi) + f0*f0*erf(r/(2*xi)) + 2 *f0 * erf(r/(sqrt(2 * xi)))  )/r2)
    ;


    /*  *vc2  = ((2/gmx::square(r))*(*vc -
        gmx::square(f0)*erf1(r1)/(2*xi) -
        4*f0*erf1(r2)/gmx::squaret(2)*xi) +
        (1/r)*(gmx::square(f0/(2.0*xi))*erf2(r1) + (2*f0/gmx::square(xi)))*erf2(r2)); */

    *vd  = -r_6;
    *fd  = -6.0*(*vd)/r;

    z     = r/(2.0*xir);
    *vr   = erfc(z)/z;
    *fr   = 0.0;
    /*  *vr2  = (sqpi*(*vr)/(2.0*z*z)+(1.0/(z*z)+1)*exp(-z*z))/(sqpi*gmx::square(xir)); */
}

void lo_do_guillot_maple(double r, double xi, double xir,
                         double *vc, double *vc2,
                         double *vd, double *vd2,
                         double *vr, double *vr2)
{
    double qO     = -0.888;
    double qOd    = 0.226;
    double f0     = qOd/qO;

    *vc  = pow(-f0/(1.0+f0)+1.0, 2.0)/r+pow(-f0/(1.0+f0)+1.0, 2.0)*f0*f0*erf(r/xi/2.0)/r+2.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0*erf(r*sqrt(2.0)/xi/2.0)/r;
    *vc2 = 2.0*pow(-f0/(1.0+f0)+1.0, 2.0)/(r*r*r)-pow(-f0/(1.0+f0)+1.0, 2.0)*f0*f0/sqrt(M_PI)/(xi*xi*xi)*exp(-r*r/(xi*xi)/4.0)/2.0-2.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0*f0/sqrt(M_PI)*exp(-r*r/(xi*xi)/4.0)/xi/(r*r)+2.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0*f0*erf(r/xi/2.0)/(r*r*r)-2.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0/sqrt(M_PI)/(xi*xi*xi)*exp(-r*r/(xi*xi)/2.0)*sqrt(2.0)-4.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0/sqrt(M_PI)*exp(-r*r/(xi*xi)/2.0)*sqrt(2.0)/xi/(r*r)+4.0*pow(-f0/(1.0+f0)+1.0, 2.0)*f0*erf(r*sqrt(2.0)/xi/2.0)/(r*r*r);

    *vd   = -1.0/(r*r*r*r*r*r);
    *vd2  = -42.0/(r*r*r*r*r*r*r*r);
    *vr   = 2.0*erfc(r/xir/2.0)/r*xir;
    *vr2  = 1.0/sqrt(M_PI)/(xir*xir)*exp(-r*r/(xir*xir)/4.0)+4.0/sqrt(M_PI)*exp(-r*r/(xir*xir)/4.0)/(r*r)+4.0*erfc(r/xir/2.0)/(r*r*r)*xir;
}

static void lo_do_DEC(double r, double xi, double xir,
                      double *vc, double *fc,
                      double *vd, double *fd,
                      double *vr, double *fr)
{
    double qO     = -0.888;
    double qOd    =  0.226;
    double f0     = qOd/qO;
    double r2, xi2;

    r2  = r*r;
    xi2 = xi*xi;

    *vc = 1.0/r + f0*f0*erf(r/(2*xi))/r + 2*f0*erf(r/(sqrt(2)*xi))/r;

    /* -D[1/r,r] -D[f0*f0*Erf[r/(2*xi)]/r,r] -D[2*f0*Erf[r/(Sqrt[2]*xi)]/r,r] */
    *fc  = (
            1.0/r2 +
            f0*f0*(-exp(-r2/(4*xi2))/(sqrt(M_PI) * r * xi) + erf(r/(2*xi))/r2) +
            2*f0*(-sqrt(2.0/M_PI)*exp(-r2/(2*xi2))/ (r*xi) + erf(r/(sqrt(2)*xi))/r2)
            );

    /* -D[1/r^6,r] */
    *vd  = -1.0/(r*r*r*r*r*r);
    *fd  = 6.0*(*vd)/r;

    /*  -D[2*xir*Erfc[r/(2*xir)]/r,r] */
    *vr  = 2.*xir*erfc(r/(2.*xir))/r;
    *fr  = -(-2.*exp(-r2/(4*xir*xir)) / (sqrt(M_PI)*r)  - 2*xir*erfc(r/(2*xir))/r2  );

}

/* Guillot2001 diffuse charge - diffuse charge interaction
   Mathematica

   In[19]:= Uc[r_] := Erf[r/(2*xi)]/r

   In[20]:= -D[Uc[r],r]

   r
   Erf[----]
   1                    2 xi
   Out[20]= -(-------------------------) + ---------
   2      2                       2
   r /(4 xi )                     r
   E           Sqrt[Pi] r xi
 */
void lo_do_DEC_qd_qd(double r, double xi,
                     double *vc, double *fc,
                     double *vd, double *fd,
                     double *vr, double *fr)
{
    double sqpi   = sqrt(M_PI);

    *vc = erf(r/(2*xi))/r;
    *fc = -(1.0/(exp(r*r/(4*xi*xi))*sqpi*r*xi)) + (erf(r/(2*xi))/(r*r));

    *vd  = 0.0;
    *fd  = 0.0;

    *vr  = 0.0;
    *fr  = 0.0;
}

/* Guillot2001 charge - diffuse charge interaction eqn 4 & 5
   Mathematica
   In[17]:= Uc[r_] := Erf[r/(Sqrt[2]*xi)]/r

   In[18]:= -D[Uc[r],r]

   2                  r
   Sqrt[--]        Erf[----------]
   Pi             Sqrt[2] xi
   Out[18]= -(----------------) + ---------------
   2      2                 2
   r /(2 xi )               r
   E           r xi
 */
void lo_do_DEC_q_qd(double r, double xi,
                    double *vc, double *fc,
                    double *vd, double *fd,
                    double *vr, double *fr)
{
    *vc = erf(r/(sqrt(2)*xi)) / r;
    *fc = -(sqrt(2/M_PI)/(exp(r*r/(2*xi*xi))*r*xi)) + (erf(r/(sqrt(2)*xi))/(r*r));

    *vd  = 0.0;
    *fd  = 0.0;

    *vr  = 0.0;
    *fr  = 0.0;
}

/* Guillot2001 charge - charge interaction (normal coulomb), repulsion and dispersion
   Mathematica

   In[6]:= Uc[r_] := 1.0/r

   In[7]:= -D[Uc[r],r]

   1.
   Out[7]= --
   2
   r

   In[8]:= Ud[r_] := -1.0/r^6

   In[9]:= -D[Ud[r],r]

   -6.
   Out[9]= ---
   7
   r

   In[13]:= Ur[r_] := (2*xir)*Erfc[r/(2*xir)]/r

   In[14]:= -D[Ur[r],r]
   r
   2 xir Erfc[-----]
   2                         2 xir
   Out[16]= ----------------------- + -----------------
   2       2                       2
   r /(4 xir )                     r
   E            Sqrt[Pi] r


 */
void lo_do_DEC_q_q(double r, double xir,
                   double *vc, double *fc,
                   double *vd, double *fd,
                   double *vr, double *fr)
{
    double sqpi   = sqrt(M_PI);
    double r2;

    r2  = r*r;

    *vc  = 1.0/r;
    *fc  = 1.0/(r*r);

    *vd  = -1.0/(r*r*r*r*r*r);
    *fd  = -6.0/(r*r*r*r*r*r*r);

    *vr  = (2.0*xir*erfc(r/(2.0*xir)))/r;
    *fr  = 2.0/(exp((r*r)/(4*xir*xir)) * sqpi *r) + (2*xir*erfc((r*xir)/2.0))/(r*r);
    *vr  = 2.*xir*erfc(r/(2.*xir))/r;
    *fr  = -(-2.*exp(-r2/(4*xir*xir)) / (sqrt(M_PI)*r)  - 2*xir*erfc(r/(2*xir))/r2  );
}

static void do_guillot(FILE *fp, int eel, int pts_nm, double xi, double xir)
{
    int    i, imax;
    double r, vc, fc, vd, fd, vr, fr;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = fc = vd = fd = vr = fr = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_guillot(r, xi, xir, &vc, &fc, &vd, &fd, &vr, &fr);
        }
        fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                r, vc, fc, vd, fd, vr, fr);

    }
}

/* TODO:
   PvM: Everything is hardcoded, we should fix that. How?
 */
static void do_guillot2001a(int eel, int pts_nm, double xi, double xir)
{
    FILE       *fp = NULL;
    char        buf[256];
    const char *atype[]   = { "HW", "OW", "HWd", "OWd", NULL };
    int         i, j, k, imax, atypemax = 4;
    double      r, vc, fc, vd, fd, vr, fr;

    /* For Guillot2001a we have four types: HW, OW, HWd and OWd. */

    for (j = 0; (j < atypemax); j++)             /* loops over types */
    {
        for (k = 0; (k <= j); k++)
        {
            sprintf(buf, "table_%s_%s.xvg", atype[k], atype[j]);

            printf("%d %d %s\n", j, k, buf);
            /* Guillot2001a eqn 2, 6 and 7 */
            if (((strcmp(atype[j], "HW") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                ((strcmp(atype[j], "OW") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                ((strcmp(atype[j], "OW") == 0) && (strcmp(atype[k], "OW") == 0)))
            {

                fp = gmx_ffopen(buf, "w");

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_q_q(r, xir, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

                /* Guillot eqn 4 and 5 */
            }
            else if (((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                     ((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "OW") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "OW") == 0)))
            {

                fp = gmx_ffopen(buf, "w");

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_q_qd(r, xi, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

                /* Guillot2001a eqn 3 */
            }
            else if (((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "HWd") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "HWd") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "OWd") == 0)))
            {

                fp = gmx_ffopen(buf, "w");

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_qd_qd(r, xi, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

            }
            else
            {
                gmx_fatal(FARGS, "Invalid atom type: %s %s", atype[j], atype[k]);
            }


        }
    }
}

/* TODO:
   PvM: Everything is hardcoded, we should fix that. How?
 */
static void do_DEC_pair(const char *file, int eel, int pts_nm, double rc, double rtol, double xi, double xir)
{
    FILE       *fp = NULL;
    char        buf[256];
    const char *atype[]   = { "OW", "HW", "OWd", "HWd", NULL };
    int         i, j, k, imax, atypemax = 4;
    double      r, vc, fc, vd, fd, vr, fr;
    char        fbuf[256];

    strncpy(fbuf, file, 255);
    fbuf[strlen(fbuf)-4] = '\0';
    printf("%d %s\n", (int)strlen(fbuf), fbuf);

    /* For Guillot2001a we have four types: HW, OW, HWd and OWd. */
    for (j = 0; (j < atypemax); j++)             /* loops over types */
    {
        for (k = 0; (k <= j); k++)
        {
            sprintf(buf, "%s_%s_%s.xvg", fbuf, atype[k], atype[j]);

            printf("%d %d %s\n", j, k, buf);
            /* Guillot2001a eqn 2, 6 and 7 */
            if (((strcmp(atype[j], "HW") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                ((strcmp(atype[j], "HW") == 0) && (strcmp(atype[k], "OW") == 0)) ||
                ((strcmp(atype[j], "OW") == 0) && (strcmp(atype[k], "OW") == 0)))
            {

                fp = gmx_ffopen(buf, "w");
                fprintf(fp, "#\n# Table %s %s DEC(Guillot2001a): rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", atype[k], atype[j], rc, rtol, xi, xir);

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_q_q(r, xir, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

                /* Guillot2001a eqn 4 and 5 */
            }
            else if (((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                     ((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "OW") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "HW") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "OW") == 0)))
            {

                fp = gmx_ffopen(buf, "w");
                fprintf(fp, "#\n# Table %s %s DEC(Guillot2001a): rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", atype[k], atype[j], rc, rtol, xi, xir);

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_q_qd(r, xi, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

                /* Guillot2001a eqn 3 */
            }
            else if (((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "HWd") == 0)) ||
                     ((strcmp(atype[j], "HWd") == 0) && (strcmp(atype[k], "OWd") == 0)) ||
                     ((strcmp(atype[j], "OWd") == 0) && (strcmp(atype[k], "OWd") == 0)))
            {

                fp = gmx_ffopen(buf, "w");
                fprintf(fp, "#\n# Table %s %s DEC(Guillot2001a): rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", atype[k], atype[j], rc, rtol, xi, xir);

                imax = 3*pts_nm;
                for (i = 0; (i <= imax); i++)
                {
                    r     = i*(1.0/pts_nm);
                    /* Avoid very large numbers */
                    if (r < 0.04)
                    {
                        vc = fc = vd = fd = vr = fr = 0;
                    }
                    else
                    if (eel == eelPME || eel == eelRF)
                    {
                        fprintf(stderr, "Not implemented\n");
                        return;
                    }
                    else if (eel == eelCUT)
                    {
                        lo_do_DEC_qd_qd(r, xi, &vc, &fc, &vd, &fd, &vr, &fr);
                    }
                    fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                            r, vc, fc, vd, fd, vr, fr);

                }
                fclose(fp);

            }
            else
            {
                gmx_fatal(FARGS, "Invalid atom type: %s %s", atype[j], atype[k]);
            }


        }
    }
}


static void do_Slater(int pts_nm,
                      int nrow1, int nrow2,
                      double w1, double w2)
{
    FILE  *fp = NULL;
    char   buf[256];
    int    i, imax;
    double r, vc, fc, vd, fd, vr, fr;

    sprintf(buf, "table_%d-%g_%d-%g.xvg", nrow1, w1, nrow2, w2);
    printf("Writing %s\n", buf);
    fp = gmx_ffopen(buf, "w");

    vd   = fd = vr = fr = 0;
    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r  = i*(1.0/pts_nm);
        vc = fc = 0;
        if ((w1 == 0) && (w2 == 0))
        {
            if (r > 0)
            {
                vc = 1/r;
                fc = 1/gmx::square(r);
            }
        }
        else if ((w1 == 0) && (w2 != 0))
        {
            vc = Nuclear_SS(r, nrow2, w2);
            fc = DNuclear_SS(r, nrow2, w2);
        }
        else if ((w2 == 0) && (w1 != 0))
        {
            vc = Nuclear_SS(r, nrow1, w1);
            fc = DNuclear_SS(r, nrow1, w1);
        }
        else
        {
            vc = Coulomb_SS(r, nrow1, nrow2, w1, w2);
            fc = DCoulomb_SS(r, nrow1, nrow2, w1, w2);
        }

        fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                r, vc, fc, vd, fd, vr, fr);
    }
    fclose(fp);
}

static void do_ljc(FILE *fp, int eel, int pts_nm, real rc, real rtol)
{
    int    i, imax;
    double r, vc, fc, vd, fd, vr, fr;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = fc = vd = fd = vr = fr = 0;
        }
        else
        {
            if (eel == eelPME)
            {
                lo_do_ljc_pme(r, rc, rtol, &vc, &fc, &vd, &fd, &vr, &fr);
            }
            else if (eel == eelCUT)
            {
                lo_do_ljc(r, &vc, &fc, &vd, &fd, &vr, &fr);
            }
        }
        fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                r, vc, fc, vd, fd, vr, fr);
    }
}

static void do_guillot_maple(FILE *fp, int eel, int pts_nm, double xi, double xir)
{
    int    i, imax;
    /*  double xi     = 0.15;*/
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_guillot_maple(r, xi, xir, &vc, &vc2, &vd, &vd2, &vr, &vr2);
        }
        fprintf(fp, "%15.10e  %15.10e  %15.10e   %15.10e  %15.10e  %15.10e  %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void do_DEC(FILE *fp, int eel, int pts_nm, double xi, double xir)
{
    int    i, imax;
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_DEC(r, xi, xir, &vc, &vc2, &vd, &vd2, &vr, &vr2);
        }
        fprintf(fp, "%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void do_DEC_q_q(FILE *fp, int eel, int pts_nm, double xir)
{
    int    i, imax;
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_DEC_q_q(r, xir, &vc, &vc2, &vd, &vd2, &vr, &vr2);
        }
        fprintf(fp, "%15.10e  %15.10e  %15.10e   %15.10e  %15.10e  %15.10e  %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void do_DEC_q_qd(FILE *fp, int eel, int pts_nm, double xi)
{
    int    i, imax;
    /*  double xi     = 0.15;*/
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_DEC_q_qd(r, xi, &vc, &vc2, &vd, &vd2, &vr, &vr2);
        }
        fprintf(fp, "%15.10e  %15.10e  %15.10e   %15.10e  %15.10e  %15.10e  %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void do_DEC_qd_qd(FILE *fp, int eel, int pts_nm, double xi)
{
    int    i, imax;
    /*  double xi     = 0.15;*/
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        if (eel == eelPME)
        {
            fprintf(fp, "Not implemented\n");
        }
        else if (eel == eelCUT)
        {
            lo_do_DEC_qd_qd(r, xi, &vc, &vc2, &vd, &vd2, &vr, &vr2);
        }
        fprintf(fp, "%15.10e  %15.10e  %15.10e   %15.10e  %15.10e  %15.10e  %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void do_maaren(FILE *fp, int pts_nm, int npow)
{
    int    i, imax;
    double xi      = 0.05;
    double xir     = 0.0615;
    double r, vc, vc2, vd, vd2, vr, vr2;

    imax = 3*pts_nm;
    for (i = 0; (i <= imax); i++)
    {
        r     = i*(1.0/pts_nm);
        /* Avoid very large numbers */
        if (r < 0.04)
        {
            vc = vc2 = vd = vd2 = vr = vr2 = 0;
        }
        else
        {
            lo_do_guillot_maple(r, xi, xir, &vc, &vc2, &vd, &vd2, &vr, &vr2);
            vr  =  pow(r, -1.0*npow);
            vr2 = (npow+1.0)*(npow)*vr/gmx::square(r);
        }
        fprintf(fp, "%15.10e  %15.10e  %15.10e   %15.10e  %15.10e  %15.10e  %15.10e\n",
                r, vc, vc2, vd, vd2, vr, vr2);
    }
}

static void gen_alexandria_rho(Poldata                 &pd,
                               const char             *fn,
                               ChargeDistributionModel iDistributionModel,
                               real                    rcut,
                               real                    spacing,
                               const gmx_output_env_t *oenv,
                               std::vector<Eemprops>  *eemprops)
{
    FILE                         *fp;
    int                           j     = 0;
    int                           n     = 0;
    int                           nmax  = 0;
    int                           nzeta = 0;
    int                          *row;
    std::string                   name;
    double                        rho  = 0;
    double                        rr   = 0;
    double                        qtot = 0;
    double                       *A;
    double                       *zeta;
    double                       *q;
    
    char                          buf[STRLEN];

    nmax = 1+(int)(rcut/spacing);
    for (auto eep = eemprops->begin(); eep != eemprops->end(); eep++)
    {
        name      = eep->getName();        
        nzeta = pd.getNzeta( iDistributionModel, name);
        snew(zeta, nzeta);
        snew(q, nzeta);
        snew(row, nzeta);
        snew(A, nzeta);
        qtot = 0;
        for (j = 0; j < nzeta; j++)
        {
            zeta[j] = pd.getZeta(iDistributionModel, name, j);
            q[j]    = pd.getQ(iDistributionModel, name, j);
            qtot   += q[j];
            row[j]  = pd.getRow(iDistributionModel, name, j);
            switch (iDistributionModel)
            {
                case eqdAXg:
                case eqdAXpg:
                    A[j] = pow(zeta[j]*zeta[j]/M_PI, 1.5);
                    break;
                case eqdAXs:
                case eqdAXps:
                    A[j] = pow(2*zeta[j], 2*row[j]+1)/(4*M_PI*faculty(2*row[j]));
                    break;
                default:
                    gmx_fatal(FARGS, "Don't know how to handle model %s",
                              getEemtypeName(iDistributionModel));
            }
        }
        if (q[nzeta-1] == 0)
        {
            q[nzeta-1] = -qtot;
        }
        sprintf(buf, "%s_%s", name.c_str(), fn);
        fp = xvgropen(buf, "Rho", "r (nm)", "rho(r)", oenv);
        for (n = 0; n <= nmax; n++)
        {
            rr  = n*spacing;
            rho = 0;
            for (j = 0; j < nzeta; j++)
            {
                if (zeta[j] > 0)
                {
                    switch (iDistributionModel)
                    {
                        case eqdAXg:
                        case eqdAXpg:
                            rho += A[j]*exp(-gmx::square(rr*zeta[j]));
                            break;
                        case eqdAXs:
                        case eqdAXps:
                            rho += A[j]*pow(rr, 2*row[j]-2)*exp(-2*zeta[j]*rr);
                            break;
                        default:
                            gmx_fatal(FARGS, "Don't know how to handle model %s",
                                      getEemtypeName(iDistributionModel));
                    }
                }
            }
            fprintf(fp, "%10.5e  %10.5e\n", rr, rho);
        }
        fclose(fp);
        sfree(q);
        sfree(zeta);
        sfree(row);
        sfree(A);
    }
}

static void gen_alexandria_tables(Poldata                 &pd,
                                  const char              *fn,
                                  ChargeDistributionModel  iDistributionModel,
                                  real                     rcut,
                                  real                     spacing,
                                  const gmx_output_env_t  *oenv,
                                  char                    *atypes)
{
    double       cv = 0;
    double       cf = 0;
    double       rr = 0;
    double       vc = 0;
    double       vd = 0;
    double       fd = 0;
    double       fc = 0;
    double       vr = 0;
    double       fr = 0;
    
    const char  *ns[2]  = {"", "_s"};
    
    char         buf1[STRLEN];
    char         buf2[STRLEN];
    char         fnbuf[STRLEN];
    FILE        *fp1;
    FILE        *fp2;

    auto eemprops = pd.getEemprops();
    filter_eemprops(&eemprops, iDistributionModel, atypes);
    
    gen_alexandria_rho(pd, "rho.xvg", iDistributionModel, rcut, spacing, oenv, &eemprops);
    
    auto nmax = 1+(int)(rcut/spacing);
    for (auto eei = eemprops.begin(); eei != eemprops.end(); ++eei)
    {
        auto nzetaI =  pd.getNzeta(iDistributionModel, eei->getName());
        for (auto eej = eemprops.begin(); eej != eemprops.end(); ++eej)
        {
            auto nzetaJ =  pd.getNzeta( iDistributionModel, eej->getName());
            for (int i = 0; i < nzetaI; i++)
            {
                auto zetaI = pd.getZeta(iDistributionModel, eei->getName(), i);
                auto rowI  = pd.getRow(iDistributionModel,  eei->getName(), i);
                for (int j = 0; j < nzetaJ; j++)
                {
                    auto zetaJ = pd.getZeta(iDistributionModel, eej->getName(), j);
                    auto rowJ  = pd.getRow(iDistributionModel,  eej->getName(), j);

                    strncpy(fnbuf, fn, strlen(fn)-4);
                    fnbuf[strlen(fn)-4] = '\0';
                    sprintf(buf1, "%s_%s%s_%s%s.xvg", fnbuf, eei->getName(), ns[i], eej->getName(), ns[j]);
                    sprintf(buf2, "%s_%s%s_%s%s.xvg", fnbuf, eej->getName(), ns[j], eei->getName(), ns[i]);
                    fp1 = xvgropen(buf1, buf1, "r (nm)", "V (kJ/mol e)", oenv);
                    fp2 = xvgropen(buf2, buf2, "r (nm)", "V (kJ/mol e)", oenv);

                    for (int n = 0; n <= nmax; n++)
                    {
                        rr = n*spacing;
                        switch (iDistributionModel)
                        {
                            case eqdAXp:
                            case eqdAXpp:
                                cv = Coulomb_PP(rr);
                                cf = DCoulomb_PP(rr);
                                break;
                            case eqdAXg:
                            case eqdAXpg:
                                cv = Coulomb_GG(rr, zetaI, zetaJ);
                                cf = DCoulomb_GG(rr, zetaI, zetaJ);
                                break;
                            case eqdAXs:
                            case eqdAXps:
                            case eqdRappe:
                            case eqdYang:
                                cv = Coulomb_SS(rr, rowI, rowJ, zetaI, zetaJ);
                                cf = DCoulomb_SS(rr, rowI, rowJ, zetaI, zetaJ);
                                break;
                            default:
                                gmx_fatal(FARGS, "Don't know how to handle model %s",
                                          getEemtypeName(iDistributionModel));
                        }
                        if (rr > 0)
                        {
                            lo_do_ljc(rr, &vc, &fc, &vd, &fd, &vr, &fr);
                        }
                        else
                        {
                            vc = fc = vd = fd = vr = fr = 0;
                        }
                        fprintf(fp1, "%10.5e  %10.5e  %10.5e %10.5e %10.5e %10.5e %10.5e\n", rr, cv, cf, vd, fd, vr, fr);
                        fprintf(fp2, "%10.5e  %10.5e  %10.5e %10.5e %10.5e %10.5e %10.5e\n", rr, cv, cf, vd, fd, vr, fr);
                    }
                    fclose(fp1);
                    fclose(fp2);
                }
            }
        }
    }
}

int alex_gen_table(int argc, char *argv[])
{
    static const char            *desc[] = {
        "gen_table generates tables for mdrun for use with the USER defined",
        "potentials. Note that the format has been update for higher",
        "accuracy in the forces starting with version 4.0. Using older",
        "tables with 4.0 will silently crash your simulations, as will",
        "using new tables with an older GROMACS version. This is because in the",
        "old version the second derevative of the potential was specified",
        "whereas in the new version the first derivative of the potential",
        "is used instead.[PAR]",
        "The program can read the [TT]gentop.dat[tt] file (or otherwise as",
        "specified with the [TT]-di[tt] option) and generate tables for all",
        "possible interactions for a given charge model (as specified with",
        "the [TT]-qdist[tt] option).[PAR]",
        "For Slater interactions four parameters must be passed: the 1/Width",
        "and the row number of the element. The interactions are computed analytically",
        "which may be slow due to the fact that arbitraray precision arithmetic is",
        "needed. If the width of one of the Slater is zero a Nucleus-Slater interaction",
        "will be generated."
    };

    t_filenm                      fnm[] = {
        { efXVG, "-o", "table",  ffWRITE },
        { efDAT, "-d", "gentop", ffOPTRD }
    };
    
    const  int                    NFILE      = asize(fnm);
    
    static int                    nrow1      = 1;
    static int                    nrow2      = 1;
    static int                    nrep       = 12;
    static int                    ndisp      = 6;
    static int                    pts_nm     = 500;
    static double                 delta      = 0;
    static double                 efac       = 500;
    static double                 rc         = 0.9;
    static double                 rtol       = 1e-05;
    static double                 xi         = 0.15;
    static double                 xir        = 0.0615;
    static double                 w1         = 20;
    static double                 w2         = 20;
    static char                  *atypes     = nullptr;
    static const char            *cqdist[]   = {nullptr, "None", "Yang", "Rappe", "AXp", "AXpp", "AXg", "AXpg", "AXs", "AXps", nullptr};
    static const char            *opt[]      = {nullptr, "cut", "rf", "pme", nullptr};
    static const char            *model[]    = {nullptr, "ljc", "dec", "dec-pair", "guillot2001a", "slater", "AB1", nullptr};
    
    t_pargs                       pa[]       = {
        { "-qdist",  FALSE, etENUM, {cqdist},
          "Algorithm used for charge distribution" },
        { "-el",     FALSE, etENUM, {opt},
          "Electrostatics type: cut, rf or pme" },
        { "-rc",     FALSE, etREAL, {&rc},
          "Cut-off required for rf or pme" },
        { "-rtol",   FALSE, etREAL, {&rtol},
          "Ewald tolerance required for pme" },
        { "-xi",   FALSE, etREAL, {&xi},
          "Width of the Gaussian diffuse charge of the G&G model" },
        { "-xir",   FALSE, etREAL, {&xir},
          "Width of erfc(z)/z repulsion of the G&G model (z=0.5 rOO/xir)" },
        { "-z1",   FALSE, etREAL, {&w1},
          "1/Width of the first Slater charge (unit 1/nm)" },
        { "-z2",   FALSE, etREAL, {&w2},
          "1/Width of the second Slater charge (unit 1/nm)" },
        { "-nrow1",   FALSE, etINT, {&nrow1},
          "Row number for the first Slater charge" },
        { "-nrow2",   FALSE, etINT, {&nrow2},
          "Row number for the first Slater charge" },
        { "-m",      FALSE, etENUM, {model},
          "Model for the tables" },
        { "-resol",  FALSE, etINT,  {&pts_nm},
          "Resolution of the table (points per nm)" },
        { "-delta",  FALSE, etREAL, {&delta},
          "Displacement in the Coulomb functions (nm), used as 1/(r+delta). Only for hard wall potential." },
        { "-efac",   FALSE, etREAL, {&efac},
          "Number indicating the steepness of the hardwall potential." },
        { "-nrep",   FALSE, etINT,  {&nrep},
          "Power for the repulsion potential (with model AB1 or maaren)" },
        { "-ndisp",   FALSE, etINT,  {&ndisp},
          "Power for the dispersion potential (with model AB1 or maaren)" },
        { "-atypes",  FALSE, etSTR, {&atypes},
          "List of atom types for which you want to generate table." }
    };
    
    
    FILE                         *fp;
    const char                   *fn;
    int                           eel = 0, m = 0;
    ChargeDistributionModel       iDistributionModel;
    gmx_output_env_t             *oenv;

    if (!parse_common_args(&argc, argv, PCA_CAN_VIEW | PCA_CAN_TIME,
                           NFILE, fnm, asize(pa), pa,
                           asize(desc), desc, 0, nullptr, &oenv))
    {
        return 0;
    }
    if (strcmp(opt[0], "cut") == 0)
    {
        eel = eelCUT;
    }
    else if (strcmp(opt[0], "rf") == 0)
    {
        eel = eelRF;
    }
    else if (strcmp(opt[0], "pme") == 0)
    {
        eel = eelPME;
    }
    else
    {
        gmx_fatal(FARGS, "Invalid argument %s for option -e", opt[0]);
    }
    if (strcmp(model[0], "maaren") == 0)
    {
        m = mMaaren;
    }
    else if (strcmp(model[0], "AB1") == 0)
    {
        m = mAB1;
    }
    else if (strcmp(model[0], "ljc") == 0)
    {
        m = mLjc;
    }
    else if (strcmp(model[0], "guillot2001a") == 0)
    {
        m = mGuillot2001a;
    }
    else if (strcmp(model[0], "guillot_maple") == 0)
    {
        m = mGuillot_Maple;
    }
    else if (strcmp(model[0], "slater") == 0)
    {
        m = mSlater;
    }
    else if (strcmp(model[0], "hard_wall") == 0)
    {
        m = mHard_Wall;
    }
    else if (strcmp(model[0], "dec") == 0)
    {
        m = mDEC;
    }
    else if (strcmp(model[0], "dec-pair") == 0)
    {
        m = mDEC_pair;
    }
    else if (strcmp(model[0], "dec_qd_q") == 0)
    {
        m = mDEC_qd_q;
    }
    else if (strcmp(model[0], "dec_qd_qd") == 0)
    {
        m = mDEC_qd_qd;
    }
    else if (strcmp(model[0], "dec_q_q") == 0)
    {
        m = mDEC_q_q;
    }
    else
    {
        gmx_fatal(FARGS, "Invalid argument %s for option -m", opt[0]);
    }

    if ((iDistributionModel = name2eemtype(cqdist[0])) == eqdNR)
    {
        fprintf(stderr, "Running in old mode!\n");
        fn = opt2fn("-o", NFILE, fnm);
        if ((m != mGuillot2001a) && (m != mSlater))
        {
            fp = gmx_ffopen(fn, "w");
        }
        switch (m)
        {
            case mGuillot2001a:
                do_guillot2001a(eel, pts_nm, xi, xir);
                break;
            case mSlater:
                do_Slater(pts_nm, nrow1, nrow2, w1, w2);
                break;
            case mGuillot_Maple:
                fprintf(fp, "#\n# Table Guillot_Maple: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", rc, rtol, xi, xir);
                do_guillot_maple(fp, eel, pts_nm, xi, xir);
                break;
            case mDEC_q_q:
                fprintf(fp, "#\n# Table DEC_q_q: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", rc, rtol, xi, xir);
                do_DEC_q_q(fp, eel, pts_nm, xir);
                break;
            case mDEC:
                fprintf(fp, "#\n# Table DEC: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", rc, rtol, xi, xir);
                do_DEC(fp, eel, pts_nm, xi, xir);
                break;
            case mDEC_pair:
                do_DEC_pair(fn, eel, pts_nm, rc, rtol, xi, xir);
                break;
            case mDEC_qd_q:
                fprintf(stdout, "case mDEC_qd_q");
                fprintf(fp, "#\n# Table DEC_qd_q: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", rc, rtol, xi, xir);
                do_DEC_q_qd(fp, eel, pts_nm, xi);
                break;
            case mDEC_qd_qd:
                fprintf(stdout, "case mDEC_qd_qd");
                fprintf(fp, "#\n# Table DEC_qd_qd: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n", rc, rtol, xi, xir);
                do_DEC_qd_qd(fp, eel, pts_nm, xi);
                break;
            case mMaaren:
                do_maaren(fp, pts_nm, nrep);
                break;
            case mAB1:
                fprintf(fp, "#\n# Table AB1: ndisp=%d nrep=%d\n#\n", ndisp, nrep);
                do_AB1(fp, pts_nm, ndisp, nrep);
                break;
            case mLjc:
                fprintf(fp, "#\n# Table LJC(12-6-1): rc=%g, rtol=%g\n#\n", rc, rtol);
                do_ljc(fp, eel, pts_nm, rc, rtol);
                break;
            case mHard_Wall:
                do_hard(fp, pts_nm, efac, delta);
                break;
            default:
                gmx_fatal(FARGS, "Model %s not supported yet", model[0]);
        }
        if ((m != mGuillot2001a) && (m != mSlater))
        {
            fclose(fp);
        }
    }
    else
    {
        gmx_atomprop_t aps = gmx_atomprop_init();
        Poldata        pd;
        std::string    gentop;
        const char    *ptr = opt2fn_null("-d", NFILE, fnm);
        if (nullptr != ptr)
        {
            gentop.assign(ptr);
        }
        try
        {
            readPoldata(gentop, pd, aps);
        }
        GMX_CATCH_ALL_AND_EXIT_WITH_FATAL_ERROR;
        gen_alexandria_tables(pd, opt2fn("-o", NFILE, fnm), iDistributionModel, rc, 1.0/pts_nm, oenv, atypes);
    }

    return 0;
}
