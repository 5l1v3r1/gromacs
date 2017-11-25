/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2014,2015, by the GROMACS development team, led by
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
/*! \internal \file
 * \brief
 * Implements test of autocorrelation function routines
 *
 * \author Anders G&auml;rden&auml;s <anders.gardenas@gmail.com>
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 * \ingroup module_correlationfunctions
 */
#include <math.h>

#include <gtest/gtest.h>

#include "programs/alexandria/plistwrapper.h"
#include "programs/alexandria/poldata.h"
#include "programs/alexandria/poldata_low.h"
#include "programs/alexandria/poldata_xml.h"

#include "testutils/refdata.h"
#include "testutils/testasserts.h"
#include "testutils/testfilemanager.h"

namespace alexandria
{

namespace
{

class PoldataTest : public ::testing::Test
{
    protected:
        static  alexandria::Poldata                      pd_;
        gmx::test::TestReferenceData                     refData_;
        gmx::test::TestReferenceChecker                  checker_;
        static   std::vector<std::string>                atomNames;
        static std::string atomName;

        PoldataTest ( )
            : refData_(gmx::test::erefdataCreateMissing), checker_(refData_.rootChecker())
        {
        }

        // Static initiation, only run once every test.
        static void SetUpTestCase()
        {
            gmx_atomprop_t aps = gmx_atomprop_init();

            // Reads the file, the file only supports 3 chargedistributionModels
            // eqdAXp,eqdAXg,  eqdAXs,  23/07/15
            std::string dataName = gmx::test::TestFileManager::getInputFilePath("gentop.dat");
            try
            {
                alexandria::readPoldata(dataName.c_str(), pd_, aps);
            }
            GMX_CATCH_ALL_AND_EXIT_WITH_FATAL_ERROR;

            atomName = pd_.findAtype("ha")->getType();
            for (auto iter = pd_.getAtypeBegin(); iter != pd_.getAtypeEnd(); iter++)
            {
                atomNames.push_back(iter->getType());
            }
        }

        static void TearDownTestCase()
        {
        }


};

alexandria::Poldata      PoldataTest::pd_;
std::vector<std::string> PoldataTest::atomNames;
std::string              PoldataTest::atomName;

TEST_F (PoldataTest, getAtype){
    alexandria::FfatypeIterator aType =  pd_.findAtype("h1");

    checker_.checkString(aType->getElem(), "elem");
    checker_.checkString(aType->getDesc(), "desc");
    checker_.checkString(aType->getType(), "type");
    checker_.checkString(aType->getPtype(), "ptype");
    checker_.checkString(aType->getBtype(), "btype");
    checker_.checkString(aType->getVdwparams(), "vdwparams");
    checker_.checkString(aType->getRefEnthalpy(), "refEnthalpy");
}

TEST_F(PoldataTest, addAtype){
    const std::string        elem         = "elem";
    const std::string        desc         = "temporary test atom";
    const std::string        atype        = "aType";
    const std::string        ptype        = "Type";
    const std::string        btype        = "bType";
          std::string        vdwparams    = "vdwparams";
    const std::string        ref_enthalpy = "1000";

    pd_.addAtype(elem,
                 desc,
                 atype,
                 ptype,
                 btype,
                 vdwparams,
                 ref_enthalpy);

    auto fa = pd_.findAtype(atype);
    if (fa != pd_.getAtypeEnd())
    {
        // Test if the extractions where correct
        checker_.checkString(fa->getElem(), elem.c_str());
        checker_.checkString(fa->getDesc(), desc.c_str());
        checker_.checkString(fa->getType(), atype.c_str());
        checker_.checkString(fa->getPtype(), ptype.c_str());
        checker_.checkString(fa->getBtype(), btype.c_str());
        checker_.checkString(fa->getVdwparams(), vdwparams.c_str());
    }
}



TEST_F (PoldataTest, Ptype)
{
    auto ptype = pd_.findPtype("p_ha");
    if (ptype != pd_.getPtypeEnd())
    {
        checker_.checkString(ptype->getType(), "type");
        checker_.checkString(ptype->getMiller(), "miller");
        checker_.checkString(ptype->getBosque(), "bosque");
        checker_.checkDouble(ptype->getPolarizability(), "polarizability");
        checker_.checkDouble(ptype->getSigPol(), "sigPol");
    }
}

TEST_F (PoldataTest, Miller)
{
    alexandria::MillerIterator miller = pd_.getMillerBegin();
    checker_.checkInteger(miller->getAtomnumber(), "atomnumber");
    checker_.checkDouble(miller->getTauAhc(), "tauAhc");
    checker_.checkDouble(miller->getAlphaAhp(), "alphaAhp");
}


TEST_F (PoldataTest, Bosque)
{
    alexandria::BosqueIterator bosque = pd_.getBosqueBegin();
    checker_.checkString(bosque->getBosque(), "bosque");
    checker_.checkDouble(bosque->getPolarizability(), "polarizability");
}

TEST_F (PoldataTest, chi)
{
    std::vector<double>      chi0s;
    std::vector<ChargeDistributionModel> eqd;
    eqd.push_back(eqdAXpp);
    eqd.push_back(eqdAXpg);
    eqd.push_back(eqdAXps);

    for (auto model : eqd)
    {
        chi0s.push_back(pd_.getChi0(model, atomName));
    }
    checker_.checkSequence(chi0s.begin(), chi0s.end(), "chi");
}

TEST_F (PoldataTest, row){
    std::vector<double>      rows;
    int numAtoms = 3;
    int numModels = 3;

    for (int atomNr = 0; atomNr < numAtoms; atomNr++)
    {
        for (int model = 0; model <  numModels; model++)
        {
            rows.push_back(pd_.getRow((ChargeDistributionModel)model, atomName, 0));
        }
    }
    checker_.checkSequence(rows.begin(), rows.end(), "row");
}


TEST_F (PoldataTest, zeta)
{
    std::vector<double>      zetas;
    int numAtoms = 3;
    int numModels = 3;
    
    for (int atomNr = 0; atomNr < numAtoms; atomNr++)
    {
        for (int model = 0; model <  numModels; model++)
        {
            for(int z = 0; z < pd_.getNzeta((ChargeDistributionModel)model, atomName); z++)
            {
                zetas.push_back(pd_.getZeta((ChargeDistributionModel)model, atomName, z));
            }
        }
    }
    checker_.checkSequence(zetas.begin(), zetas.end(), "zeta");
}

TEST_F (PoldataTest, forceField)
{
    std::string force =  pd_.getForceField( );
    checker_.checkString(force, "forceField");
}


TEST_F (PoldataTest, lenghtUnit)
{
    auto fs = pd_.findForces(alexandria::eitBONDS);
    std::string length =  fs->unit();
    checker_.checkString(length, "lenghtUnit");
}

TEST_F (PoldataTest, polarUnit)
{
    std::string polarUnit = pd_.getPolarUnit( );
    checker_.checkString(polarUnit, "polarUnit");
}


TEST_F (PoldataTest, polarRef)
{
    std::string polarRef =  pd_.getPolarRef( );
    checker_.checkString(polarRef, "polarRef");
}

}

}
