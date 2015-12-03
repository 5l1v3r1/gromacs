/*
 * This source file is part of the Alexandria project.
 *
 * Copyright (C) 2014 David van der Spoel and Paul J. van Maaren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#ifndef GMX_RESP_H
#define GMX_RESP_H

#include <stdio.h>
#include "gromacs/statistics/statistics.h"
#include "gromacs/topology/atomprop.h"
#include "poldata.h"
#include "molprop.h"
#include "gmx_ra.h"

struct gmx_output_env_t;
struct t_symtab;

enum eParm {
    eparmQ, eparmZ, eparmNR
};


namespace alexandria
{
class Resp
{
    public:
        Resp(){}

        Resp(ChargeDistributionModel iDistributionModel, real qtot);
        /*
           Resp(ChargeDistributionModel iDistributionModel,
           bool bAXpRESP, real qfac, real b_hyper, real qtot,
           real zmin, real zmax, real deltaZ, bool bZatyp,
           real watoms, real rDecrZeta,
           bool bRandZeta, bool bRandQ, real penaltyFac, bool bFitZeta,
           bool bEntropy, const std::string dzatoms,
           unsigned int seed);


           //Constructor with defult values
           Resp(ChargeDistributionModel iDistributionModel,real qtot){


           //Defult values from gentop.ccp 31-07-2015
           return Resp(ChargeDistributionModel iDistributionModel,
              FALSE, 1e-3, 0.1, qtot,
              5, 100, -1, TRUE,
              0, -1,
              FALSE, TRUE,1, TRUE,
              FALSE, "",
              0);
           }

         */
        ~Resp();


        int atomicnumber2row(int elem);



        void statistics( int len, char buf[]);

        void summary(FILE             *gp,
                     std::vector<int> &symmetricAtoms);

        void updateAtomtypes( t_atoms *atoms);

        //void fillZeta( alexandria::Poldata * pd);

	void fillZeta();

        void fillQ( t_atoms *atoms);

        void addAtomCoords( rvec *x);

        bool addAtomInfo( t_atoms              *atoms,
                          alexandria::Poldata * pd);

        void getAtomInfo( t_atoms *atoms,
                          t_symtab *symtab, rvec **x);

        const std::string getStoichiometry();

        void addAtomSymmetry(std::vector<int> &symmetricAtoms);

        void addPoint( double x, double y,
                       double z, double V);

        void makeGrid( real spacing, matrix box, rvec x[]);

        void copyGrid(Resp * src);

        Resp * copy();

        void calcRms();

        double getRms( real *wtot);

        void potLsq( gmx_stats_t lsq);

        void calcRho();

        void calcPot();

        void readCube( const std::string fn, bool bESPonly);

        void writeCube( const std::string fn, std::string title);

        void writeRho( const std::string fn, std::string  title);

        void writeDiffCube(Resp * src,
                           const std::string cubeFn, const std::string histFn,
                           std::string title, const gmx_output_env_t *oenv, int rho);

        void writeHisto(const std::string fn,
                        std::string title, const gmx_output_env_t *oenv);

        int  optimizeCharges(FILE *fp,  int maxiter,
                             real toler, real *rms);

        void potcomp(const std::string potcomp,
                     const std::string pdbdiff, const gmx_output_env_t *oenv);

        double getQtot( int atom);

        double getQ( int atom, int zz);

        double getZeta( int atom, int zz);

        void setQ( int atom, int zz, double q);

        void setZeta( int atom, int zz, double zeta);

        void setBAXpRESP(bool bAXpRESP)
        {
            _bAXpRESP = bAXpRESP;
        }

        void setZmin(real zmin)
        {
            _zmin = zmin;
        }

        void setDeltaZ(real detlaZ)
        {
            _deltaZ = detlaZ;
        }

        void setWatoms(double watoms)
        {
            _watoms = watoms;
        }
        void setRDecrZeta(real rDecrZeta)
        {
            _rDecrZeta = rDecrZeta;
        }

        void setBRandZeta(bool bRandZeta)
        {
            _bRandZeta = bRandZeta;
        }

        void setSeed(unsigned int seed)
        {
            _seed = seed;
        }

        void setBEntropy(bool bEntropy)
        {
            _bEntropy  = bEntropy;
        }
        void calcPenalty();
        unsigned int seed() { return _seed; }
        void getSetVector(bool         bSet,
                          bool         bRandQ,
                          bool         bRandZeta,
                          unsigned int seed,
                          double    *  nmx);
    private:
        ChargeDistributionModel                   _iDistributionModel;
        int                                       _nesp, _natom, _natype;
        double                                    _qtot, _qsum, _watoms;
        double                                    _rms, _rrms, _penalty, _pfac, _entropy, _wtot;
        dvec                                      _origin, _space;
        bool                                      _bZatype, _bFitZeta, _bEntropy;
        bool                                      _bRandZeta, _bRandQ;
        bool                                      _bAXpRESP;
        ivec                                      _nxyz;
        real                                      _qfac, _bHyper, _zmin, _zmax, _deltaZ, _qmin, _qmax, _rDecrZeta;
        unsigned int                              _seed;
        int                                       _nparam; /* Total number of parameters */
        std::vector<Ra *>                         _ra;
        std::vector<std::string>                  _dzatoms;
        const std::string                         _stoichiometry;
        std::vector<double>                       _pot, _potCalc, _rho;
        rvec                                     *_x, *_esp;


        void warning(const std::string fn, int line);


        real myWeight(int iatom);


        void addParam( int atom, eParm eparm, int zz);

        //        static double chargeFunction(void * gr, double v[]);

};
}
#endif
