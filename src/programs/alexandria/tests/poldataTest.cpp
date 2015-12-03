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

#include "programs/alexandria/poldata.h"
#include "programs/alexandria/poldata_xml.h"

#include "testutils/refdata.h"
#include "testutils/testasserts.h"
#include "testutils/testfilemanager.h"

class PoldataTest : public ::testing::Test
{
    protected:
        static  alexandria::Poldata                    * pd;
        gmx::test::TestReferenceData                     refData_;
        gmx::test::TestReferenceChecker                  checker_;
        static const  int numModels = 3;
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
            pd = alexandria::PoldataXml::read(dataName.c_str(), aps);

            alexandria::FfatypeIterator iter = pd->getAtypeBegin();
            atomName = iter->getType();
            for (; iter != pd->getAtypeEnd(); iter++)
            {
                atomNames.push_back(iter->getType());
            }
        }

        static void TearDownTestCase()
        {
        }


};

alexandria::Poldata    * PoldataTest::pd;
const int                PoldataTest::numModels;
std::vector<std::string> PoldataTest::atomNames;
std::string              PoldataTest::atomName;

TEST_F (PoldataTest, getAtype){
    alexandria::FfatypeIterator aType =  pd->getAtypeBegin();

    checker_.checkString(aType->getElem(), "elem");
    checker_.checkString(aType->getDesc(), "desc");
    checker_.checkString(aType->getType(), "type");
    checker_.checkString(aType->getPtype(), "ptype");
    checker_.checkString(aType->getBtype(), "btype");
    checker_.checkString(aType->getVdwparams(), "vdwparams");
    checker_.checkDouble(aType->getRefEnthalpy(), "refEnthalpy");
}

TEST_F(PoldataTest, searchAtype){
    alexandria::Ffatype  type;
    pd->searchAtype(atomName, &type);

    checker_.checkString(type.getElem(), "elem");
    checker_.checkString(type.getDesc(), "desc");
    checker_.checkString(type.getPtype(), "ptype");
    checker_.checkString(type.getBtype(), "btype");
    checker_.checkString(type.getVdwparams(), "vdwparams");
    checker_.checkBoolean(type.getType().compare(atomName) == 0, "Same type");
}

TEST_F(PoldataTest, addAtype){
    const std::string        elem         = "elm";
    const std::string        desc         = "temproary test atom";
    const std::string        atype        = "aType";
    const std::string        ptype        = "Type";
    const std::string        btype        = "bType";
    const std::string        vdwparams    = "vdwparams";
    const double             ref_enthalpy = 1000;

    std::string              newElem;
    std::string              newDesc;
    std::string              newAtype;
    std::string              newPtype;
    std::string              newBtype;
    std::string              newVdwparams;
    alexandria::Ffatype      fatype;


    pd->addAtype( elem,
                  desc,
                  atype,
                  ptype,
                  btype,
                  vdwparams,
                  ref_enthalpy);

    int valid = pd->searchAtype(atype, &fatype);
    //will faill if the seartch failed
    checker_.checkBoolean(valid == 1, "search valid");

    //test if the extraction where corect
    checker_.checkBoolean(fatype.getElem().compare(elem) == 0, "elem");
    checker_.checkBoolean(fatype.getDesc().compare(desc) == 0, "desc");
    checker_.checkBoolean(fatype.getType().compare(atype) == 0, "atype");
    checker_.checkBoolean(fatype.getPtype().compare(ptype) == 0, "ptype");
    checker_.checkBoolean(fatype.getBtype().compare(btype) == 0, "btype");
    checker_.checkBoolean(fatype.getVdwparams().compare(vdwparams) == 0, "vdwparams" );
}



TEST_F (PoldataTest, Ptype)
{
    alexandria::PtypeIterator ptype = pd->getPtypeBegin();
    checker_.checkString(ptype->getType(), "type");
    checker_.checkString(ptype->getMiller(), "miller");
    checker_.checkString(ptype->getBosque(), "bosque");
    checker_.checkDouble(ptype->getPolarizability(), "polarizability");
    checker_.checkDouble(ptype->getSigPol(), "sigPol");
}

TEST_F (PoldataTest, Miller)
{
    alexandria::MillerIterator miller = pd->getMillerBegin();
    checker_.checkInteger(miller->getAtomnumber(), "atomnumber");
    checker_.checkDouble(miller->getTauAhc(), "tauAhc");
    checker_.checkDouble(miller->getAlphaAhp(), "alphaAhp");
}


TEST_F (PoldataTest, Bosque)
{
    alexandria::BosqueIterator bosque = pd->getBosqueBegin();
    checker_.checkString(bosque->getBosque(), "bosque");
    checker_.checkDouble(bosque->getPolarizability(), "polarizability");
}

TEST_F (PoldataTest, chi)
{
    std::vector<double>      chi0s;


    for (int model = 0; model < numModels; model++)
    {
        chi0s.push_back(pd->getChi0((ChargeDistributionModel)model, atomName));
    }
    checker_.checkSequence(chi0s.begin(), chi0s.end(), "chi");
}

TEST_F (PoldataTest, row){
    std::vector<double>      rows;
    int numAtoms = 3;

    for (int atomNr = 0; atomNr < numAtoms; atomNr++)
    {
        for (int model = 0; model <  numModels; model++)
        {
            rows.push_back(pd->getRow((ChargeDistributionModel)model, atomName, atomNr));
        }
    }
    checker_.checkSequence(rows.begin(), rows.end(), "row");
}


TEST_F (PoldataTest, zeta)
{
    std::vector<double>      zetas;
    int numAtoms = 3;

    for (int atomNr = 0; atomNr < numAtoms; atomNr++)
    {
        for (int model = 0; model <  numModels; model++)
        {
            zetas.push_back(pd->getZeta((ChargeDistributionModel)model, atomName, atomNr));
        }
    }
    checker_.checkSequence(zetas.begin(), zetas.end(), "zeta");
}

TEST_F (PoldataTest, forceField)
{
    std::string force =  pd->getForceField( );
    checker_.checkString(force, "forceField");
}

TEST_F (PoldataTest, lenghtUnit)
{
    std::string lenght =  pd->getLengthUnit( );
    checker_.checkString(lenght, "lenghtUnit");
}


TEST_F (PoldataTest, polarUnit)
{
    std::string polarUnit = pd->getPolarUnit( );
    checker_.checkString(polarUnit, "polarUnit");
}


TEST_F (PoldataTest, polarRef)
{
    std::string polarRef =  pd->getPolarRef( );
    checker_.checkString(polarRef, "polarRef");
}
