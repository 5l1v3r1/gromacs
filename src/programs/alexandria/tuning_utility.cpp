/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2016, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \brief
 * Implements part of the alexandria program.
 * \author Mohammad Mehdi Ghahremanpour <m.ghahremanpour@hotmail.com>
 */

#include "tuning_utility.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
 
namespace alexandria
{

static bool check_polarizability(tensor alpha)
{
    for (int mm = 0; mm < DIM; mm++)
    {
        if (alpha[mm][mm] == 0)
        {
            return false;
        }
    }    
    return true;
}

void print_stats(FILE        *fp,        
                 const char  *prop, 
                 gmx_stats_t  lsq, 
                 gmx_bool     bHeader,
                 char        *xaxis,     
                 char        *yaxis)
{
    real a    = 0, da  = 0, b    = 0, db   = 0;
    real mse  = 0, mae = 0, chi2 = 0, rmsd = 0;
    real Rfit = 0;
    int  n;

    if (bHeader)
    {
        fprintf(fp, "Fitting data to y = ax + b, where x = %s and y = %s\n", xaxis, yaxis);
        fprintf(fp, "%-12s %5s %13s %13s %8s %8s %8s %8s\n",
                "Property", "N", "a", "b", "R(%)", "RMSD", "MSE", "MAE");
        fprintf(fp, "---------------------------------------------------------------\n");
    }
    gmx_stats_get_ab(lsq, elsqWEIGHT_NONE, &a, &b, &da, &db, &chi2, &Rfit);
    gmx_stats_get_rmsd(lsq,    &rmsd);
    gmx_stats_get_mse_mae(lsq, &mse, &mae);
    gmx_stats_get_npoints(lsq, &n);
    fprintf(fp, "%-12s %5d %6.3f(%5.3f) %6.3f(%5.3f) %7.2f %8.4f %8.4f %8.4f\n",
            prop, n, a, da, b, db, Rfit*100, rmsd, mse, mae);
}

void print_lsq_set(FILE *fp, gmx_stats_t lsq)
{
    real   x, y;

    fprintf(fp, "@type xy\n");
    while (gmx_stats_get_point(lsq, &x, &y, nullptr, nullptr, 0) == estatsOK)
    {
        fprintf(fp, "%10g  %10g\n", x, y);
    }
    fprintf(fp, "&\n");
}

void xvgr_symbolize(FILE                   *xvgf, 
                    int                     nsym, 
                    const char             *leg[],
                    const gmx_output_env_t *oenv)
{
    int i;

    xvgr_legend(xvgf, nsym, leg, oenv);
    for (i = 0; (i < nsym); i++)
    {
        xvgr_line_props(xvgf, i, elNone, ecBlack+i, oenv);
        fprintf(xvgf, "@ s%d symbol %d\n", i, i+1);
    }
}

void print_polarizability(FILE              *fp, 
                          alexandria::MyMol *mol,
                          char              *calc_name,
                          real               q_toler)
{
    tensor dalpha;
    real   delta = 0;
    
    if (nullptr != calc_name)
    {
        if (strcmp(calc_name, (char *)"Calc") == 0)
        {
            m_sub(mol->alpha_elec_, mol->alpha_calc_, dalpha);
            delta = sqrt(gmx::square(dalpha[XX][XX])+gmx::square(dalpha[XX][YY])+gmx::square(dalpha[XX][ZZ])+
                         gmx::square(dalpha[YY][YY])+gmx::square(dalpha[YY][ZZ]));
            fprintf(fp,
                    "%-4s (%6.2f %6.2f %6.2f) Dev: (%6.2f %6.2f %6.2f) Delta: %6.2f %s\n"
                    "     (%6s %6.2f %6.2f)      (%6s %6.2f %6.2f)\n"
                    "     (%6s %6s %6.2f)      (%6s %6s %6.2f)\n",
                    calc_name,
                    mol->alpha_calc_[XX][XX], mol->alpha_calc_[XX][YY], mol->alpha_calc_[XX][ZZ],
                    dalpha[XX][XX], dalpha[XX][YY], dalpha[XX][ZZ], delta, (delta > q_toler) ? "YYY" : "",
                    "", mol->alpha_calc_[YY][YY], mol->alpha_calc_[YY][ZZ],
                    "", dalpha[YY][YY], dalpha[YY][ZZ],
                    "", "", mol->alpha_calc_[ZZ][ZZ],
                    "", "", dalpha[ZZ][ZZ]);
        }
        else
        {
            fprintf(fp, "Polarizability analysis\n");
            fprintf(fp,
                    "Electronic   (%6.2f %6.2f %6.2f)\n"
                    "             (%6s %6.2f %6.2f)\n"
                    "             (%6s %6s %6.2f)\n",
                    mol->alpha_elec_[XX][XX], mol->alpha_elec_[XX][YY], mol->alpha_elec_[XX][ZZ],
                    "", mol->alpha_elec_[YY][YY], mol->alpha_elec_[YY][ZZ],
                    "", "", mol->alpha_elec_[ZZ][ZZ]);
        }
    }
}

void print_quadrapole(FILE              *fp, 
                      alexandria::MyMol *mol,
                      qType              qt,
                      real               q_toler)
{
    const tensor &qelec = mol->QQM(qtElec);
    
    if (qt != qtElec)
    {
        const tensor &qcalc = mol->QQM(qt);
        tensor        dQ;
        real          delta = 0;
        
        m_sub(qelec, qcalc, dQ);
        delta = sqrt(gmx::square(dQ[XX][XX])+gmx::square(dQ[XX][YY])+gmx::square(dQ[XX][ZZ])+
                     gmx::square(dQ[YY][YY])+gmx::square(dQ[YY][ZZ]));
        fprintf(fp,
                "%-4s (%6.2f %6.2f %6.2f) Dev: (%6.2f %6.2f %6.2f) Delta: %6.2f %s\n"
                "     (%6s %6.2f %6.2f)      (%6s %6.2f %6.2f)\n"
                "     (%6s %6s %6.2f)      (%6s %6s %6.2f)\n",
                qTypeName(qt),
                qcalc[XX][XX], qcalc[XX][YY], qcalc[XX][ZZ],
                dQ[XX][XX], dQ[XX][YY], dQ[XX][ZZ], delta, (delta > q_toler) ? "YYY" : "",
                "", qcalc[YY][YY], qcalc[YY][ZZ],
                "", dQ[YY][YY], dQ[YY][ZZ],
                "", "", qcalc[ZZ][ZZ],
                "", "", dQ[ZZ][ZZ]);
    }
    else
    {
        fprintf(fp, "Quadrupole analysis (6 independent components only)\n");
        fprintf(fp,
                "Electronic   (%6.2f %6.2f %6.2f)\n"
                "             (%6s %6.2f %6.2f)\n"
                "             (%6s %6s %6.2f)\n",
                qelec[XX][XX], qelec[XX][YY], qelec[XX][ZZ],
                "", qelec[YY][YY], qelec[YY][ZZ],
                "", "", qelec[ZZ][ZZ]);
    }
}

void print_dipole(FILE              *fp, 
                  alexandria::MyMol *mol, 
                  qType              qt,
                  real               toler)
{
    rvec dmu;
    real ndmu, cosa;
    char ebuf[32];

    rvec_sub(mol->muQM(qtElec), mol->muQM(qt), dmu);
    ndmu = norm(dmu);
    cosa = cos_angle(mol->muQM(qtElec), mol->muQM(qtCalc));
    if (ndmu > toler)
    {
        sprintf(ebuf, "XXX");
    }
    else if (fabs(cosa) < 0.1)
    {
        sprintf(ebuf, "YYY");
    }
    else
    {
        ebuf[0] = '\0';
    }
    fprintf(fp, "%-10s (%6.2f,%6.2f,%6.2f) |Mu| = %5.2f Dev: (%6.2f,%6.2f,%6.2f) |%5.2f|%s\n",
            qTypeName(qt), mol->muQM(qtCalc)[XX], mol->muQM(qtCalc)[YY], mol->muQM(qtCalc)[ZZ], 
            mol->dipQM(qtCalc), dmu[XX], dmu[YY], dmu[ZZ], ndmu, ebuf);
}

void print_electric_props(FILE                           *fp, 
                          std::vector<alexandria::MyMol>  mymol,
                          const Poldata                  &pd,
                          const char                     *qhisto,
                          const char                     *DipCorr,
                          const char                     *MuCorr, 
                          const char                     *Qcorr,
                          const char                     *EspCorr,
                          const char                     *alphaCorr,
                          const char                     *isopolCorr,
                          const char                     *anisopolCorr, 
                          real                            dip_toler, 
                          real                            quad_toler,
                          real                            alpha_toler, 
                          const gmx_output_env_t         *oenv,
                          bool                            bPolar,
                          bool                            bDipole,
                          bool                            bQuadrupole,
                          bool                            bfullTensor,
                          IndexCount                     *indexCount,
                          real                            hfac,
                          t_commrec                      *cr,
                          real                            efield)
{
    int           i    = 0, j     = 0, n     = 0;
    int           nout = 0, mm    = 0, nn    = 0;
    real          sse  = 0, rms   = 0, sigma = 0;
    real          aver = 0, error = 0, qCalc = 0;
    
    FILE          *dipc, *muc,  *Qc;
    FILE          *hh,   *espc, *alphac, *isopolc, *anisopolc;   
        
    struct ZetaTypeLsq {
        std::string ztype;
        gmx_stats_t lsq;
    };    
    
    gmx_stats_t               lsq_mu[qtNR], lsq_dip[qtNR], lsq_quad[qtNR];
    gmx_stats_t               lsq_esp, lsq_alpha, lsq_isoPol, lsq_anisoPol;
    const char               *eprnm[qtNR] = {"Calc", "ESP", "MPA", "HPA", "CM5"};
    std::vector<ZetaTypeLsq>  lsqt;

    for (int i = 0; i < qtNR; i++)
    {
        lsq_quad[i] = gmx_stats_init();
        lsq_dip[i]  = gmx_stats_init();
        lsq_mu[i]   = gmx_stats_init();
    }
    lsq_esp      = gmx_stats_init();
    lsq_alpha    = gmx_stats_init();
    lsq_isoPol   = gmx_stats_init();
    lsq_anisoPol = gmx_stats_init();
    n            = 0;
    
    for (auto ai = indexCount->beginIndex(); ai < indexCount->endIndex(); ++ai)
    {
        ZetaTypeLsq k;
        k.ztype.assign(ai->name());
        k.lsq = gmx_stats_init();
        lsqt.push_back(std::move(k));
    }       
    for (auto &mol: mymol)
    {
        if (mol.eSupp_ != eSupportNo)
        {
            fprintf(fp, "Molecule %d: %s Qtot: %d, Multiplicity %d\n", n+1,
                    mol.molProp()->getMolname().c_str(),
                    mol.molProp()->getCharge(),
                    mol.molProp()->getMultiplicity());
                        
            mol.CalcDipole();
            for(int j = 0; j < qtElec; j++)
            { 
                qType qt = static_cast<qType>(j);
                print_dipole(fp, &mol, qt,   dip_toler);
                
                gmx_stats_add_point(lsq_dip[j], mol.dipQM(qtElec), mol.dipQM(qt), 0, 0);
            }

            sse += gmx::square(mol.dipQM(qtElec) - mol.dipQM(qtCalc));
            
            mol.CalcQuadrupole();
            print_quadrapole(fp, &mol, qtElec, quad_toler);
            
            for (int j = 0; j <= qtCalc; j++)
            {
                qType qt = static_cast<qType>(j);
                print_quadrapole(fp, &mol, qt, quad_toler);
                for (mm = 0; mm < DIM; mm++)
                {
                    gmx_stats_add_point(lsq_mu[j], mol.muQM(qtElec)[mm], mol.muQM(qt)[mm], 0, 0);
                
                    for (nn = 0; nn < DIM; nn++)
                    {
                        if (bfullTensor || (mm == nn))
                        {
                            gmx_stats_add_point(lsq_quad[j], mol.QQM(qtElec)[mm][nn], mol.QQM(qt)[mm][nn], 0, 0);
                        }
                    }
                }
            }
            
            if(bPolar)
            {
                mol.CalcPolarizability(efield, cr, nullptr);
                if (check_polarizability(mol.alpha_calc_))
                {
                    print_polarizability(fp, &mol, (char *)"Electronic", alpha_toler);
                    print_polarizability(fp, &mol, (char *)"Calc",       alpha_toler);
                    gmx_stats_add_point(lsq_isoPol, mol.isoPol_elec_, mol.isoPol_calc_,       0, 0);
                    gmx_stats_add_point(lsq_anisoPol, mol.anisoPol_elec_, mol.anisoPol_calc_, 0, 0);
                    for (mm = 0; mm < DIM; mm++)
                    {
                        gmx_stats_add_point(lsq_alpha, mol.alpha_elec_[mm][mm], mol.alpha_calc_[mm][mm], 0, 0);
                    }
                }
            }
            
            rms = mol.espRms();
            fprintf(fp,   "ESP rms: %g (Hartree/e)\n", rms);                                  
            auto nEsp     = mol.Qgresp_.nEsp();
            auto EspPoint = mol.Qgresp_.espPoint();
            for (size_t i = 0; i < nEsp; i++)
            {
                gmx_stats_add_point(lsq_esp, gmx2convert(EspPoint[i].v(),eg2cHartree_e), gmx2convert(EspPoint[i].vCalc(), eg2cHartree_e), 0, 0);
            }
            
            fprintf(fp, "Atom   Type      q_Calc     q_ESP     q_MPA     q_HPA     q_CM5       x       y       z\n");
            auto qesp = mol.chargeQM(qtESP);
            auto x    = mol.x();
            for (j = i = 0; j < mol.topology_->atoms.nr; j++)
            {
                // TODO Or virtual site?
                if (mol.topology_->atoms.atom[j].ptype == eptAtom)
                {               
                    auto fa = pd.findAtype(*(mol.topology_->atoms.atomtype[j]));
                    auto at = fa->getZtype();
                    if(indexCount->isOptimized(at))
                    {
                        auto        k  = std::find_if(lsqt.begin(), lsqt.end(),
                                                      [at](const ZetaTypeLsq &atlsq)
                                                      {
                                                          return atlsq.ztype.compare(at) == 0;
                                                      });                                                 
                        if (k != lsqt.end())
                        {
                            qCalc = mol.topology_->atoms.atom[j].q;
                            if(nullptr != mol.shellfc_)
                            {
                                qCalc += mol.topology_->atoms.atom[j+1].q;
                            }
                            gmx_stats_add_point(k->lsq, qesp[i], qCalc, 0, 0);
                        }                        
                        fprintf(fp, "%-2d%3d  %-5s  %8.4f  %8.4f  %8.4f  %8.4f  %8.4f%8.3f%8.3f%8.3f\n",
                                mol.topology_->atoms.atom[j].atomnumber,
                                j+1,
                                *(mol.topology_->atoms.atomtype[j]),
                                qCalc, 
                                qesp[i],
                                mol.chargeQM(qtMulliken)[i],
                                mol.chargeQM(qtHirshfeld)[i],
                                mol.chargeQM(qtCM5)[i],
                                x[j][XX], 
                                x[j][YY], 
                                x[j][ZZ]); 
                    }
                    i++;
                }
            }
            fprintf(fp, "\n");
            n++;
        }
    }

    fprintf(fp, "Dipoles are %s in Calc Parametrization.\n",     (bDipole ?     "used" : "not used"));
    fprintf(fp, "Quadrupoles are %s in Calc Parametrization.\n", (bQuadrupole ? "used" : "not used"));
    fprintf(fp, "\n");

    print_stats(fp, (char *)"ESP           (Hartree/e)",  lsq_esp,           true,  (char *)"Electronic", (char *)"Calc");
    print_stats(fp, (char *)"Dipoles       (Debye)",      lsq_mu[qtCalc],   false, (char *)"Electronic", (char *)"Calc");
    print_stats(fp, (char *)"Dipole Moment (Debye)",      lsq_dip[qtCalc],  false, (char *)"Electronic", (char *)"Calc");
    print_stats(fp, (char *)"Quadrupoles   (Buckingham)", lsq_quad[qtCalc], false, (char *)"Electronic", (char *)"Calc");
    if (bPolar)
    {
        print_stats(fp, (char *)"Principal Components of Polarizability (A^3)",  lsq_alpha, false,  (char *)"Electronic", (char *)"Calc");
        print_stats(fp, (char *)"Isotropic Polarizability (A^3)",    lsq_isoPol,   false,  (char *)"Electronic", (char *)"Calc");
        print_stats(fp, (char *)"Anisotropic Polarizability (A^3)",  lsq_anisoPol, false,  (char *)"Electronic", (char *)"Calc");
    }
    fprintf(fp, "\n");

    print_stats(fp, (char *)"Dipoles",       lsq_mu[qtESP],   true,  (char *)"Electronic", (char *)"ESP");
    print_stats(fp, (char *)"Dipole Moment", lsq_dip[qtESP],  false, (char *)"Electronic", (char *)"ESP");
    print_stats(fp, (char *)"Quadrupoles",   lsq_quad[qtESP], false, (char *)"Electronic", (char *)"ESP");       
    fprintf(fp, "\n");

    print_stats(fp, (char *)"Dipoles",       lsq_mu[qtMulliken],   true,  (char *)"Electronic", (char *)"MPA");
    print_stats(fp, (char *)"Dipole Moment", lsq_dip[qtMulliken],  false, (char *)"Electronic", (char *)"MPA");
    print_stats(fp, (char *)"Quadrupoles",   lsq_quad[qtMulliken], false, (char *)"Electronic", (char *)"MPA");   
    fprintf(fp, "\n");

    print_stats(fp, (char *)"Dipoles",       lsq_mu[qtHirshfeld],   true,  (char *)"Electronic", (char *)"HPA");
    print_stats(fp, (char *)"Dipole Moment", lsq_dip[qtHirshfeld],  false, (char *)"Electronic", (char *)"HPA");
    print_stats(fp, (char *)"Quadrupoles",   lsq_quad[qtHirshfeld], false, (char *)"Electronic", (char *)"HPA");   
    fprintf(fp, "\n");

    print_stats(fp, (char *)"Dipoles",       lsq_mu[qtCM5],   true,  (char *)"Electronic", (char *)"CM5");
    print_stats(fp, (char *)"Dipole Moment", lsq_dip[qtCM5],  false, (char *)"Electronic", (char *)"CM5");
    print_stats(fp, (char *)"Quadrupoles",   lsq_quad[qtCM5], false, (char *)"Electronic", (char *)"CM5");   
    fprintf(fp, "\n");
    
    std::vector<const char*> atypes;
    for (const auto &k : lsqt)
    {
        atypes.push_back(k.ztype.c_str());
    }
    
    hh = xvgropen(qhisto, "Histogram for charges", "q (e)", "a.u.", oenv);
    xvgr_legend(hh, atypes.size(), atypes.data(), oenv);
    
    fprintf(fp, "\nParameters are optimized for %zu atom types:\n", atypes.size());    
    for (auto k = lsqt.begin(); k < lsqt.end(); ++k)
    {
        int   nbins;
        if (gmx_stats_get_npoints(k->lsq, &nbins) == estatsOK)
        {
            real *x, *y;
            fprintf(fp, "%-4d copies for %4s\n", nbins, k->ztype.c_str());
            if (gmx_stats_make_histogram(k->lsq, 0, &nbins, ehistoY, 1, &x, &y) == estatsOK)
            {
                fprintf(hh, "@type xy\n");
                for (int i = 0; i < nbins; i++)
                {
                    fprintf(hh, "%10g  %10g\n", x[i], y[i]);
                }
                fprintf(hh, "&\n");
                
                free(x);
                free(y);
            }
        }
        gmx_stats_free(k->lsq);
    }
    fclose(hh);
    fprintf(fp, "\n");
    
    dipc = xvgropen(DipCorr, "Dipole Moment (Debye)", "Electronic", "Empirical", oenv);
    xvgr_symbolize(dipc, 5, eprnm, oenv);
    print_lsq_set(dipc, lsq_dip[qtCalc]);
    print_lsq_set(dipc, lsq_dip[qtESP]);
    print_lsq_set(dipc, lsq_dip[qtMulliken]);
    print_lsq_set(dipc, lsq_dip[qtHirshfeld]);
    print_lsq_set(dipc, lsq_dip[qtCM5]);
    fclose(dipc);
    
    muc = xvgropen(MuCorr, "Dipoles (Debye)", "Electronic", "Empirical", oenv);
    xvgr_symbolize(muc, 5, eprnm, oenv);
    print_lsq_set(muc, lsq_mu[qtCalc]);
    print_lsq_set(muc, lsq_mu[qtESP]);
    print_lsq_set(muc, lsq_mu[qtMulliken]);
    print_lsq_set(muc, lsq_mu[qtHirshfeld]);
    print_lsq_set(muc, lsq_mu[qtCM5]);
    fclose(muc);

    Qc = xvgropen(Qcorr, "Quadrupoles (Buckingham)", "Electronic", "Empirical", oenv);
    xvgr_symbolize(Qc, 5, eprnm, oenv);
    print_lsq_set(Qc, lsq_quad[qtCalc]);
    print_lsq_set(Qc, lsq_quad[qtESP]);
    print_lsq_set(Qc, lsq_quad[qtMulliken]);
    print_lsq_set(Qc, lsq_quad[qtHirshfeld]);
    print_lsq_set(Qc, lsq_quad[qtCM5]);
    fclose(Qc);
    
    espc = xvgropen(EspCorr, "Electrostatic Potential (Hartree/e)", "Electronic", "Calc", oenv);
    xvgr_symbolize(espc, 1, eprnm, oenv);
    print_lsq_set(espc, lsq_esp);
    fclose(espc);
    
    if (bPolar)
    {
        alphac = xvgropen(alphaCorr, "Pricipal Components of Polarizability Tensor (A\\S3\\N)", "Electronic", "Calc", oenv);
        xvgr_symbolize(alphac, 1, eprnm, oenv);
        print_lsq_set(alphac, lsq_alpha);
        fclose(alphac);
        
        isopolc = xvgropen(isopolCorr, "Isotropic Polarizability (A\\S3\\N)", "Electronic", "Calc", oenv);
        xvgr_symbolize(isopolc, 1, eprnm, oenv);
        print_lsq_set(isopolc, lsq_isoPol);
        fclose(isopolc);
        
        anisopolc = xvgropen(anisopolCorr, "Anisotropic Polarizability (A\\S3\\N)", "Electronic", "Calc", oenv);
        xvgr_symbolize(anisopolc, 1, eprnm, oenv);
        print_lsq_set(anisopolc, lsq_anisoPol);
        fclose(anisopolc);
    }

    fprintf(fp, "hfac = %g\n", hfac);
    gmx_stats_get_ase(lsq_mu[qtCalc], &aver, &sigma, &error);
    sigma = sqrt(sse/n);
    nout  = 0;
    fprintf(fp, "Overview of dipole moment outliers (> %.3f off)\n", 2*sigma);
    fprintf(fp, "----------------------------------\n");
    fprintf(fp, "%-20s  %12s  %12s  %12s\n", "Name", "Calc", "Electronic", "Deviation (Debye)");
            
    for (auto &mol : mymol)
    {
        auto deviation = std::abs(mol.dipQM(qtCalc) - mol.dipQM(qtElec));
        if ((mol.eSupp_ != eSupportNo) &&
            (mol.dipQM(qtElec) > sigma) &&
            (deviation > 2*sigma))
        {
            fprintf(fp, "%-20s  %12.3f  %12.3f  %12.3f\n",
                    mol.molProp()->getMolname().c_str(),
                    mol.dipQM(qtCalc), mol.dipQM(qtElec), deviation);
            nout++;
        }
    }
    if (nout)
    {
        printf("There were %d outliers. See at the very bottom of the log file\n", nout);
    }
    else
    {
        printf("No outliers! Well done.\n");
    }
    for (int i = 0; i < qtNR; i++)
    {
        gmx_stats_free(lsq_quad[i]);
        gmx_stats_free(lsq_mu[i]);
        gmx_stats_free(lsq_dip[i]);    
    }
    gmx_stats_free(lsq_esp);
    gmx_stats_free(lsq_alpha);
    gmx_stats_free(lsq_isoPol);
    gmx_stats_free(lsq_anisoPol);
}
}


