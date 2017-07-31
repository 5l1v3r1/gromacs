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
 * \author  Mohammad Mehdi Ghahremanpour <mohammad.ghahremanpour@icm.uu.se>
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */

#include "molprop.h"

#include <cmath>

#include <string>
#include <vector>

#include "gromacs/math/utilities.h"
#include "gromacs/utility/fatalerror.h"

#include "communication.h"
#include "composition.h"
#include "gmx_simple_comm.h"


const char *mpo_name[MPO_NR] =
{
    "potential", "dipole", "quadrupole", "polarizability", "energy", "entropy", "charge"
};

const char *mpo_unit[MPO_NR] =
{
    "e/nm", "D", "B", "\\AA$^3$", "kJ/mol", "J/mol K"
};

namespace alexandria
{

const char *dataSourceName(DataSource ds)
{
    switch (ds)
    {
        case dsExperiment:
            return "Experiment";
        case dsTheory:
            return "Theory";
    }
    return nullptr;
}

DataSource dataSourceFromName(const std::string &name)
{
    if (strcasecmp(dataSourceName(dsExperiment), name.c_str()) == 0)
    {
        return dsExperiment;
    }
    else if (strcasecmp(dataSourceName(dsTheory), name.c_str()) == 0)
    {
        return dsTheory;
    }
    gmx_fatal(FARGS, "No data source corresponding to %s", name.c_str());
}

/*********************** GenericProperty routines **********************/
void GenericProperty::SetType(std::string type)
{
    if ((type_.size() == 0) && (type.size() > 0))
    {
        type_ = type;
    }
    else
    {
        if (type_.size() == 0)
        {
            fprintf(stderr, "Replacing GenericProperty type '%s' by '%s'\n", type_.c_str(), type.c_str());
        }
    }
}

void GenericProperty::SetUnit(std::string unit)
{
    if ((unit_.size() == 0) && (unit.size() > 0))
    {
        unit_ = unit;
    }
    else
    {
        if (unit_.size() == 0)
        {
            fprintf(stderr, "Replacing GenericProperty unit '%s' by '%s'\n", unit_.c_str(), unit.c_str());
        }
    }
}

CommunicationStatus GenericProperty::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_str(cr, dest, type_.c_str());
        gmx_send_str(cr, dest, unit_.c_str());
        gmx_send_double(cr, dest, T_);
        gmx_send_int(cr, dest, (int) eP_);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send GenericProperty, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus GenericProperty::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        gmx_recv_str(cr, src, &type_);
        gmx_recv_str(cr, src, &unit_);
        T_  = gmx_recv_double(cr, src);
        eP_ = (ePhase) gmx_recv_int(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive GenericProperty, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

void CalcAtom::SetUnit(std::string unit)
{
    if ((unit_.size() == 0) && (unit.size() > 0))
    {
        unit_ = unit;
    }
    else
    {
        if (unit_.size() == 0)
        {
            fprintf(stderr, "Replacing CalcAtom unit '%s' by '%s'\n", unit_.c_str(), unit.c_str());
        }
    }
}

void MolecularComposition::AddAtom(AtomNum an)
{
    AtomNumIterator mci = SearchAtom(an.getAtom());
    if (mci == EndAtomNum())
    {
        _atomnum.push_back(an);
    }
    else
    {
        mci->SetNumber(mci->getNumber()+an.getNumber());
    }
}

void MolecularPolarizability::Set(double xx, double yy, double zz,
                                  double xy, double xz, double yz,
                                  double average, double error)
{
    xx_      = xx; yy_ = yy; zz_ = zz;
    xy_      = xy; xz_ = xz; yz_ = yz;
    _average = average; _error = error;
    if (_average == 0)
    {
        // Compute average as the 1/3 the trace of the diagonal
        _average = (xx_ + yy_ + zz_)/3.0;
    }
    else if ((xx_ == 0) && (yy_ == 0) && (zz_ == 0))
    {
        // Estimate tensor as the 1/3 the trace of the diagonal
        xx_ = yy_ = zz_ = _average;
    }
}

int MolProp::NAtom()
{
    if (_mol_comp.size() > 0)
    {
        int nat = _mol_comp[0].CountAtoms();
        return nat;
    }
    return 0;
}

void MolProp::AddBond(Bond b)
{
    BondIterator bi;
    bool         bFound = false;

    for (bi = BeginBond(); !bFound && (bi < EndBond()); bi++)
    {
        bFound = (((bi->getAi() == b.getAi()) && (bi->getAj() == b.getAj())) ||
                  ((bi->getAi() == b.getAj()) && (bi->getAj() == b.getAi())));
        if (bFound)
        {
            break;
        }
    }
    if (!bFound)
    {
        _bond.push_back(b);
    }
    else if ((nullptr != debug) && (bi->getBondOrder() != b.getBondOrder()))
    {
        fprintf(debug, "Different bond orders in molecule %s\n", getMolname().c_str());
        fflush(debug);
    }
}

void MolecularComposition::DeleteAtom(std::string catom)
{
    AtomNumIterator ani;

    if ((ani = SearchAtom(catom)) != EndAtomNum())
    {
        _atomnum.erase(ani);
    }
}

AtomNumIterator MolecularComposition::SearchAtom(std::string an)
{
    AtomNumIterator ani;

    for (ani = BeginAtomNum(); (ani < EndAtomNum()); ani++)
    {
        if (an.compare(ani->getAtom()) == 0)
        {
            return ani;
        }
    }
    return EndAtomNum();
}

void MolecularComposition::ReplaceAtom(std::string oldatom, std::string newatom)
{

    for (auto i = BeginAtomNum(); (i < EndAtomNum()); i++)
    {
        if (oldatom.compare(i->getAtom()) == 0)
        {
            i->SetAtom(newatom);
            break;
        }
    }
}

int MolecularComposition::CountAtoms(std::string atom)
{
    for (auto i = BeginAtomNum(); (i < EndAtomNum()); i++)
    {
        if (atom.compare(i->getAtom()) == 0)
        {
            return i->getNumber();
        }
    }
    return 0;
}

int MolecularComposition::CountAtoms(const char *atom)
{
    std::string str(atom);

    return CountAtoms(str);
}

int MolecularComposition::CountAtoms()
{
    int             nat = 0;

    for (auto i = BeginAtomNum(); (i < EndAtomNum()); i++)
    {
        nat += i->getNumber();
    }
    return nat;
}

void MolProp::CheckConsistency()
{
}

bool MolProp::SearchCategory(const std::string &catname) const
{
    for (auto &i : category_)
    {
        if (strcasecmp(i.c_str(), catname.c_str()) == 0)
        {
            return true;
        }
    }
    return false;
}

void MolProp::DeleteComposition(const std::string &compname)
{
    std::remove_if(_mol_comp.begin(), _mol_comp.end(),
                   [compname](MolecularComposition mc)
        {
            return (compname.compare(mc.getCompName()) == 0);
        });
}

Experiment::Experiment(std::string program, std::string method,
                       std::string basisset, std::string reference,
                       std::string conformation, std::string datafile,
                       jobType jtype)
    :
    dataSource_(dsTheory),
    reference_(reference),
    conformation_(conformation),
    _program(program),
    _method(method),
    _basisset(basisset),
    _datafile(datafile),
    jobtype_(jtype)

{}

void Experiment::Dump(FILE *fp)
{
    if (nullptr != fp)
    {
        fprintf(fp, "Experiment %s %d polar %d dipole\n",
                dataSourceName(dataSource()), NPolar(), NDipole());
        if (dsExperiment == dataSource())
        {
            fprintf(fp, "reference    = %s\n", reference_.c_str());
            fprintf(fp, "conformation = %s\n", conformation_.c_str());
        }
        else
        {
            fprintf(fp, "program    = %s\n", _program.c_str());
            fprintf(fp, "method     = %s\n", _method.c_str());
            fprintf(fp, "basisset   = %s\n", _basisset.c_str());
            fprintf(fp, "datafile   = %s\n", _datafile.c_str());
            for (CalcAtomIterator cai = BeginAtom(); (cai < EndAtom()); ++cai)
            {
                double   x, y, z;
                cai->getCoords(&x, &y, &z);
                fprintf(fp, "%-3s  %-3s  %3d  %10.3f  %10.3f  %10.3f\n",
                        cai->getName().c_str(), cai->getObtype().c_str(),
                        cai->getAtomid(), x, y, z);
            }
        }
    }
}

bool Experiment::getHF(double *value)
{
    double      T    = -1;
    double      error, vec[3];
    bool        done = false;
    tensor      quad;

    if (getVal((char *)"HF", MPO_ENERGY, value, &error, &T, vec, quad))
    {
        done = true;
    }
    return done;
}

int Experiment::Merge(std::vector<Experiment>::iterator src)
{
    int nwarn = 0;

    for (auto mei = src->BeginEnergy(); (mei < src->EndEnergy()); ++mei)
    {
        alexandria::MolecularEnergy me(mei->getType(),
                                       mei->getUnit(),
                                       mei->getTemperature(),
                                       mei->getPhase(),
                                       mei->getValue(),
                                       mei->getError());
        AddEnergy(me);
    }

    for (auto dpi = src->BeginDipole(); (dpi < src->EndDipole()); ++dpi)
    {
        alexandria::MolecularDipole dp(dpi->getType(),
                                       dpi->getUnit(),
                                       dpi->getTemperature(),
                                       dpi->getX(), dpi->getY(), dpi->getZ(),
                                       dpi->getAver(), dpi->getError());
        AddDipole(dp);
    }

    for (auto mpi = src->BeginPolar(); (mpi < src->EndPolar()); ++mpi)
    {
        alexandria::MolecularPolarizability mp(mpi->getType(),
                                               mpi->getUnit(),
                                               mpi->getTemperature(),
                                               mpi->getXX(), mpi->getYY(), mpi->getZZ(),
                                               mpi->getXY(), mpi->getXZ(), mpi->getYZ(),
                                               mpi->getAverage(), mpi->getError());
        AddPolar(mp);
    }

    for (auto mqi = src->BeginQuadrupole(); (mqi < src->EndQuadrupole()); ++mqi)
    {
        alexandria::MolecularQuadrupole mq(mqi->getType(), mqi->getUnit(),
                                           mqi->getTemperature(),
                                           mqi->getXX(), mqi->getYY(), mqi->getZZ(),
                                           mqi->getXY(), mqi->getXZ(), mqi->getYZ());
        AddQuadrupole(mq);
    }

    for (auto cai = src->BeginAtom(); (cai < src->EndAtom()); ++cai)
    {
        double   x, y, z;
        CalcAtom caa(cai->getName(), cai->getObtype(), cai->getAtomid());

        cai->getCoords(&x, &y, &z);
        caa.SetCoords(x, y, z);
        caa.SetUnit(cai->getUnit());

        for (auto aci = cai->BeginQ(); (aci < cai->EndQ()); ++aci)
        {
            AtomicCharge aq(aci->getType(), aci->getUnit(),
                            aci->getTemperature(), aci->getQ());
            caa.AddCharge(aq);
        }
        AddAtom(caa);
    }

    for (auto mep = src->BeginPotential(); (mep < src->EndPotential()); mep++)
    {
        alexandria::ElectrostaticPotential ep(mep->getXYZunit(), mep->getVunit(),
                                              mep->getEspid(),
                                              mep->getX(), mep->getY(), mep->getZ(), mep->getV());
        AddPotential(ep);
    }

    return nwarn;
}

void CalcAtom::AddCharge(AtomicCharge q)
{
    AtomicChargeIterator aci;

    for (aci = BeginQ(); (aci < EndQ()); aci++)
    {
        if ((aci->getType().compare(q.getType()) == 0) &&
            (aci->getUnit().compare(q.getUnit()) == 0) &&
            (aci->getTemperature() == q.getTemperature()) &&
            (aci->getQ() == q.getQ()))
        {
            break;
        }
    }
    if (aci == EndQ())
    {
        q_.push_back(q);
    }
}

bool CalcAtom::Equal(CalcAtom ca)
{
    return !((name_.compare(ca.getName()) != 0) ||
             (obType_.compare(ca.getObtype()) != 0) ||
             (x_ != ca.getX()) ||
             (y_ != ca.getY()) ||
             (z_ != ca.getZ()) ||
             (atomID_ != ca.getAtomid()));
}

CalcAtomIterator Experiment::SearchAtom(CalcAtom ca)
{
    CalcAtomIterator cai;
    for (cai = BeginAtom(); (cai < EndAtom()); ++cai)
    {
        if (cai->Equal(ca))
        {
            break;
        }
    }
    return cai;
}

void Experiment::AddAtom(CalcAtom ca)
{
    CalcAtomIterator cai = SearchAtom(ca);

    if (cai == EndAtom())
    {
        _catom.push_back(ca);
    }
    else
    {
        printf("Trying to add identical atom %s (%s) twice. N = %d\n",
               ca.getName().c_str(), ca.getObtype().c_str(), (int)_catom.size());
    }
}

void MolProp::AddComposition(MolecularComposition mc)
{
    MolecularCompositionIterator mci = SearchMolecularComposition(mc.getCompName());
    if (mci == EndMolecularComposition())
    {
        _mol_comp.push_back(mc);
    }
}

bool MolProp::BondExists(Bond b)
{
    for (auto bi = BeginBond(); (bi < EndBond()); bi++)
    {
        if (((bi->getAi() == b.getAi()) && (bi->getAj() == b.getAj())) ||
            ((bi->getAi() == b.getAj()) && (bi->getAj() == b.getAi())))
        {
            return true;
        }
    }
    return false;
}

int MolProp::Merge(std::vector<MolProp>::iterator src)
{
    double      q = 0, sq = 0;
    std::string stmp;
    int         nwarn = 0;

    for (auto si = src->BeginCategory(); (si < src->EndCategory()); si++)
    {
        AddCategory(*si);
    }
    SetFormula(src->formula());
    SetMass(src->getMass());
    if (getMultiplicity() <= 1)
    {
        SetMultiplicity(src->getMultiplicity());
    }
    else
    {
        int smult = src->getMultiplicity();
        if ((nullptr != debug) && (smult != getMultiplicity()))
        {
            fprintf(debug, "Not overriding multiplicity to %d when merging since it is %d (%s)\n",
                    smult, getMultiplicity(), src->getMolname().c_str());
            fflush(debug);
        }
    }
    q = getCharge();
    if (q == 0)
    {
        SetCharge(src->getCharge());
    }
    else
    {
        sq = src->getCharge();
        if ((nullptr != debug) && (sq != q))
        {
            fprintf(debug, "Not overriding charge to %g when merging since it is %g (%s)\n",
                    sq, q, getMolname().c_str());
            fflush(debug);
        }
    }

    stmp = src->getMolname();
    if ((getMolname().size() == 0) && (stmp.size() != 0))
    {
        SetMolname(stmp);
    }
    stmp = src->getIupac();
    if ((getIupac().size() == 0) && (stmp.size() != 0))
    {
        SetIupac(stmp);
    }
    stmp = src->getCas();
    if ((getCas().size() == 0) && (stmp.size() != 0))
    {
        SetCas(stmp);
    }
    stmp = src->getCid();
    if ((getCid().size() == 0) && (stmp.size() != 0))
    {
        SetCid(stmp);
    }
    stmp = src->getInchi();
    if ((getInchi().size() == 0) && (stmp.size() != 0))
    {
        SetInchi(stmp);
    }
    if (NBond() == 0)
    {
        for (auto bi = src->BeginBond(); (bi < src->EndBond()); bi++)
        {
            alexandria::Bond bb(bi->getAi(), bi->getAj(), bi->getBondOrder());
            AddBond(bb);
        }
    }
    else
    {
        for (auto bi = src->BeginBond(); (bi < src->EndBond()); bi++)
        {
            alexandria::Bond bb(bi->getAi(), bi->getAj(), bi->getBondOrder());
            if (!BondExists(bb))
            {
                fprintf(stderr, "WARNING bond %d-%d not present in %s\n",
                        bi->getAi(), bi->getAj(), getMolname().c_str());
                nwarn++;
            }
        }
    }

    for (auto ei = src->BeginExperiment(); (ei < src->EndExperiment()); ei++)
    {
        if (dsExperiment == ei->dataSource())
        {
            Experiment ex(ei->getReference(), ei->getConformation());
            nwarn += ex.Merge(ei);
            AddExperiment(ex);
        }
        else
        {
            auto jtype = ei->getJobtype();
            if (jtype != JOB_NR)
            {
                Experiment ca(ei->getProgram(), ei->getMethod(),
                              ei->getBasisset(), ei->getReference(),
                              ei->getConformation(), ei->getDatafile(),
                              jtype);
                nwarn += ca.Merge(ei);
                AddExperiment(ca);
            }
        }
    }
    for (auto mci = src->BeginMolecularComposition();
         (mci < src->EndMolecularComposition()); mci++)
    {
        alexandria::MolecularComposition mc(mci->getCompName());

        for (auto ani = mci->BeginAtomNum(); (ani < mci->EndAtomNum()); ani++)
        {
            AtomNum an(ani->getAtom(), ani->getNumber());
            mc.AddAtom(an);
        }
        AddComposition(mc);
    }
    return nwarn;
}

MolecularCompositionIterator MolProp::SearchMolecularComposition(std::string str)
{
    return std::find_if(_mol_comp.begin(), _mol_comp.end(),
                        [str](MolecularComposition const &mc)
        {
            return (str.compare(mc.getCompName()) == 0);
        });
}

void MolProp::Dump(FILE *fp)
{
    std::vector<std::string>::iterator si;
    ExperimentIterator                 ei;

    if (fp)
    {
        fprintf(fp, "formula:      %s\n", formula().c_str());
        fprintf(fp, "molname:      %s\n", getMolname().c_str());
        fprintf(fp, "iupac:        %s\n", getIupac().c_str());
        fprintf(fp, "CAS:          %s\n", getCas().c_str());
        fprintf(fp, "cis:          %s\n", getCid().c_str());
        fprintf(fp, "InChi:        %s\n", getInchi().c_str());
        fprintf(fp, "mass:         %g\n", getMass());
        fprintf(fp, "charge:       %d\n", getCharge());
        fprintf(fp, "multiplicity: %d\n", getMultiplicity());
        fprintf(fp, "category:    ");
        for (si = BeginCategory(); (si < EndCategory()); si++)
        {
            fprintf(fp, " '%s'", si->c_str());
        }
        fprintf(fp, "\n");
        for (ei = BeginExperiment(); (ei < EndExperiment()); ei++)
        {
            ei->Dump(fp);
        }
    }
}

bool MolProp::GenerateComposition(const Poldata &pd)
{
    ExperimentIterator   ci;
    CalcAtomIterator     cai;
    CompositionSpecs     cs;
    MolecularComposition mci_bosque(cs.searchCS(iCbosque)->name());
    MolecularComposition mci_alexandria(cs.searchCS(iCalexandria)->name());
    MolecularComposition mci_miller(cs.searchCS(iCmiller)->name());

    // Why was this again?
    _mol_comp.clear();

    int natoms = 0;
    for (ci = BeginExperiment(); (mci_alexandria.CountAtoms() <= 0) && (ci < EndExperiment()); ci++)
    {
        /* This assumes we have either all atoms or none.
         * A consistency check could be
         * to compare the number of atoms to the formula */
        int nat = 0;
        for (cai = ci->BeginAtom(); (cai < ci->EndAtom()); cai++)
        {
            nat++;
            AtomNum ans(cai->getObtype(), 1);
            mci_alexandria.AddAtom(ans);

            std::string ptype;
            if (pd.atypeToPtype(cai->getObtype(), ptype))
            {
                std::string bos_type;
                if (pd.ptypeToBosque(ptype, bos_type))
                {
                    AtomNum anb(bos_type, 1);
                    mci_bosque.AddAtom(anb);
                }
                std::string mil_type;
                if (pd.ptypeToMiller(ptype, mil_type))
                {
                    AtomNum anm(mil_type.c_str(), 1);
                    mci_miller.AddAtom(anm);
                }
            }
        }
        natoms = std::max(natoms, nat);
    }

    if (natoms == mci_bosque.CountAtoms())
    {
        AddComposition(mci_bosque);
    }
    if (natoms == mci_miller.CountAtoms())
    {
        AddComposition(mci_miller);
    }
    if (natoms == mci_alexandria.CountAtoms())
    {
        AddComposition(mci_alexandria);
        if (nullptr != debug)
        {
            fprintf(debug, "LO_COMP: ");
            for (auto ani = mci_alexandria.BeginAtomNum(); (ani < mci_alexandria.EndAtomNum()); ani++)
            {
                fprintf(debug, " %s:%d", ani->getAtom().c_str(), ani->getNumber());
            }
            fprintf(debug, "\n");
            fflush(debug);
        }
        return true;
    }
    return false;
}

static void add_element_to_formula(const char *elem, int number, char *formula, char *texform)
{
    if (number > 0)
    {
        strcat(formula, elem);
        strcat(texform, elem);
        if (number > 1)
        {
            char cnumber[32];

            sprintf(cnumber, "%d", number);
            strcat(formula, cnumber);
            sprintf(cnumber, "$_{%d}$", number);
            strcat(texform, cnumber);
        }
    }
}

bool MolProp::GenerateFormula(gmx_atomprop_t ap)
{
    char             myform[1280], texform[2560];
    std::vector<int> ncomp;
    alexandria::MolecularCompositionIterator mci;

    ncomp.resize(110, 0);
    myform[0]  = '\0';
    texform[0] = '\0';
    mci        = SearchMolecularComposition("bosque");
    if (mci != EndMolecularComposition())
    {
        for (auto ani = mci->BeginAtomNum(); (ani < mci->EndAtomNum()); ani++)
        {
            int         cnumber, an;
            real        value;
            std::string catom = ani->getAtom();
            cnumber = ani->getNumber();
            if (gmx_atomprop_query(ap, epropElement, "???", catom.c_str(), &value))
            {
                an = std::lround(value);
                range_check(an, 0, 110);
                if (an > 0)
                {
                    ncomp[an] += cnumber;
                }
            }
        }
    }
    add_element_to_formula("C", ncomp[6], myform, texform);
    add_element_to_formula("H", ncomp[1], myform, texform);
    ncomp[6] = ncomp[1] = 0;

    for (int j = 109; (j >= 1); j--)
    {
        add_element_to_formula(gmx_atomprop_element(ap, j), ncomp[j], myform, texform);
    }
    std::string mform = formula();
    if (strlen(myform) > 0)
    {
        if (debug)
        {
            if ((mform.size() > 0) && (strcasecmp(myform, mform.c_str()) != 0))
            {
                fprintf(debug, "Formula '%s' does match '%s' based on composition for %s.\n",
                        mform.c_str(), myform, getMolname().c_str());
                fflush(debug);
            }
        }
        SetFormula(myform);
        SetTexFormula(texform);
    }
    else if ((mform.size() == 0) && debug)
    {
        fprintf(debug, "Empty composition and formula for %s\n",
                getMolname().c_str());
        fflush(debug);
    }

    return (strlen(myform) > 0);
}

bool MolProp::HasComposition(const std::string &composition) const
{
    return std::find_if(BeginMolecularComposition(),
                        EndMolecularComposition(),
                        [composition](const MolecularComposition &mi)
        {
            return mi.getCompName().compare(composition) == 0;
        }) != EndMolecularComposition();
}

bool Experiment::getVal(const std::string  type,
                        MolPropObservable  mpo,
                        double            *value,
                        double            *error,
                        double            *T,
                        double             vec[3],
                        tensor             quad_polar)
{
    bool   done = false;
    double x, y, z;
    double Told = *T;

    switch (mpo)
    {
        case MPO_ENERGY:
        case MPO_ENTROPY:
            for (auto mei = BeginEnergy(); !done && (mei < EndEnergy()); ++mei)
            {
                if (((type.size() == 0) || (type.compare(mei->getType()) == 0)) &&
                    bCheckTemperature(Told, mei->getTemperature()))
                {
                    mei->get(value, error);
                    *T   = mei->getTemperature();
                    done = true;
                }
            }
            break;
        case MPO_DIPOLE:
            for (auto mdp = BeginDipole(); !done && (mdp < EndDipole()); ++mdp)
            {
                if (((type.size() == 0) || (type.compare(mdp->getType()) == 0))  &&
                    bCheckTemperature(Told, mdp->getTemperature()))
                {
                    mdp->get(&x, &y, &z, value, error);
                    vec[XX] = x;
                    vec[YY] = y;
                    vec[ZZ] = z;
                    *T      = mdp->getTemperature();
                    done    = true;
                }
            }
            break;
        case MPO_POLARIZABILITY:
        {
            for (auto mdp = BeginPolar(); !done && (mdp < EndPolar()); ++mdp)
            {
                if (((type.size() == 0) || (type.compare(mdp->getType()) == 0)) &&
                    bCheckTemperature(Told, mdp->getTemperature()))
                {
                    double xx, yy, zz, xy, xz, yz;
                    mdp->get(&xx, &yy, &zz, &xy, &xz, &yz, value, error);
                    quad_polar[XX][XX] = xx;
                    quad_polar[XX][YY] = xy;
                    quad_polar[XX][ZZ] = xz;
                    quad_polar[YY][XX] = 0;
                    quad_polar[YY][YY] = yy;
                    quad_polar[YY][ZZ] = yz;
                    quad_polar[ZZ][XX] = 0;
                    quad_polar[ZZ][YY] = 0;
                    quad_polar[ZZ][ZZ] = zz;
                    *T                 = mdp->getTemperature();
                    done               = true;
                }
            }
        }
            break;
        case MPO_QUADRUPOLE:
            for (auto mqi = BeginQuadrupole(); !done && (mqi < EndQuadrupole()); ++mqi)
            {
                if (((type.size() == 0) || (type.compare(mqi->getType()) == 0)) &&
                    bCheckTemperature(Told, mqi->getTemperature()))
                {
                    double xx, yy, zz, xy, xz, yz;
                    mqi->get(&xx, &yy, &zz, &xy, &xz, &yz);
                    quad_polar[XX][XX] = xx;
                    quad_polar[XX][YY] = xy;
                    quad_polar[XX][ZZ] = xz;
                    quad_polar[YY][XX] = 0;
                    quad_polar[YY][YY] = yy;
                    quad_polar[YY][ZZ] = yz;
                    quad_polar[ZZ][XX] = 0;
                    quad_polar[ZZ][YY] = 0;
                    quad_polar[ZZ][ZZ] = zz;
                    *T                 = mqi->getTemperature();
                    done               = true;
                }
            }
            break;
        case MPO_CHARGE:
        {
            int i = 0;
            for (auto mai = BeginAtom(); mai < EndAtom(); ++mai)
            {
                for (auto q = mai->BeginQ(); q <  mai->EndQ(); ++q)
                {
                    if (((type.size() == 0) || (type.compare(q->getType()) == 0)) &&
                        bCheckTemperature(Told, q->getTemperature()))
                    {
                        vec[i] = q->getQ();
                        i++;
                    }
                }
            }
            if (i == NAtom())
            {
                done = true;
            }
        }
            break;
        default:
            break;
    }
    return done;
}

bool bCheckTemperature(double Tref, double T)
{
    return (Tref < 0) || (fabs(T - Tref) < 0.05);
}

bool MolProp::getPropRef(MolPropObservable mpo, iqmType iQM,
                         const std::string &lot,
                         const std::string &conf,
                         const std::string &type,
                         double *value, double *error, double *T,
                         std::string &ref, std::string &mylot,
                         double vec[3], tensor quad_polar)
{
    bool   done = false;
    double Told = *T;

    if (iQM == iqmBoth)
    {
        for (auto ei = BeginExperiment(); !done && (ei < EndExperiment()); ++ei)
        {
            if ((conf.size() == 0) ||
                (ei->getConformation().compare(conf) == 0))
            {
                if (ei->getVal(type, mpo, value, error, T, vec, quad_polar) &&
                    bCheckTemperature(Told, *T))
                {
                    ref = ei->getReference();
                    mylot.assign("Experiment");
                    done = true;
                    break;
                }
            }
        }
    }
     if (iQM == iqmExp)
    {
        for (auto ei = BeginExperiment(); !done && (ei < EndExperiment()); ++ei)
        {
            if (dsExperiment != ei->dataSource())
            {
                continue;
            }
            if ((conf.size() == 0) ||
                (ei->getConformation().compare(conf) == 0))
            {
                if (ei->getVal(type, mpo, value, error, T, vec, quad_polar) &&
                    bCheckTemperature(Told, *T))
                {
                    ref = ei->getReference();
                    mylot.assign("Experiment");
                    done = true;
                    break;
                }
            }
        }
    }
    else if (iQM == iqmQM)
    {
        for (auto ci = BeginExperiment(); !done && (ci < EndExperiment()); ++ci)
        {
            if (dsExperiment == ci->dataSource())
            {
                continue;
            }
            char buf[256];
            snprintf(buf, sizeof(buf), "%s/%s",
                     ci->getMethod().c_str(), ci->getBasisset().c_str());
            if (((lot.size() == 0) || (lot.compare(buf) == 0))  &&
                ((conf.size() == 0) || (ci->getConformation().compare(conf)) == 0))
            {
                if  (ci->getVal(type.c_str(), mpo, value, error, T, vec, quad_polar) &&
                     bCheckTemperature(Told, *T))
                {
                    ref = ci->getReference();
                    mylot.assign(lot);
                    done = true;
                    break;
                }
            }
        }
    }
    return done;
}

bool MolProp::getOptHF(double *value)
{
    bool done = false;

    for (auto ei = BeginExperiment(); !done && (ei < EndExperiment()); ++ei)
    {
        if (ei->getJobtype() == JOB_OPT)
        {
            if (ei->getHF(value))
            {
                done = true;
            }
        }
    }
    return done;
}

int MolProp::NOptSP()
{
    int n = 0;

    for (auto ei = BeginExperiment(); ei < EndExperiment(); ++ei)
    {
        if (ei->getJobtype() == JOB_OPT ||
            ei->getJobtype() == JOB_SP)
        {
            n++;
        }
    }
    return n;
}

bool MolProp::getProp(MolPropObservable mpo, iqmType iQM,
                      const std::string &lot,
                      const std::string &conf,
                      const std::string &type,
                      double *value, double *error, double *T)
{
    double      myerror, vec[3];
    tensor      quad;
    bool        bReturn;
    std::string myref, mylot;

    bReturn = getPropRef(mpo, iQM, lot, conf, type, value, &myerror, T,
                         myref, mylot, vec, quad);
    if (nullptr != error)
    {
        *error = myerror;
    }
    return bReturn;
}


ExperimentIterator MolProp::getLotPropType(const char       *lot,
                                           MolPropObservable mpo,
                                           const char       *type)
{
    ExperimentIterator       ci;

    std::vector<std::string> ll = split(lot, '/');
    if ((ll[0].length() > 0) && (ll[1].length() > 0))
    {
        for (ci = BeginExperiment(); (ci < EndExperiment()); ci++)
        {
            if ((strcasecmp(ci->getMethod().c_str(), ll[0].c_str()) == 0) &&
                (strcasecmp(ci->getBasisset().c_str(), ll[1].c_str()) == 0))
            {
                bool done = false;
                switch (mpo)
                {
                    case MPO_POTENTIAL:
                        done = ci->NPotential() > 0;
                        break;
                    case MPO_DIPOLE:
                        for (auto mdp = ci->BeginDipole(); !done && (mdp < ci->EndDipole()); mdp++)
                        {
                            done =  ((nullptr == type) ||
                                     (strcasecmp(type, mdp->getType().c_str()) == 0));
                        }
                        break;
                    case MPO_QUADRUPOLE:
                        for (auto mdp = ci->BeginQuadrupole(); !done && (mdp < ci->EndQuadrupole()); mdp++)
                        {
                            done =  ((nullptr == type) ||
                                     (strcasecmp(type, mdp->getType().c_str()) == 0));
                        }
                        break;
                    case MPO_POLARIZABILITY:
                        for (auto mdp = ci->BeginPolar(); !done && (mdp < ci->EndPolar()); mdp++)
                        {
                            done =  ((nullptr == type) ||
                                     (strcasecmp(type, mdp->getType().c_str()) == 0));
                        }
                        break;
                    case MPO_ENERGY:
                    case MPO_ENTROPY:
                        for (auto mdp = ci->BeginEnergy(); !done && (mdp < ci->EndEnergy()); mdp++)
                        {
                            done =  ((nullptr == type) ||
                                     (strcasecmp(type, mdp->getType().c_str()) == 0));
                        }
                        break;
                    default:
                        break;
                }
                if (done)
                {
                    break;
                }
            }
        }
        return ci;
    }
    else
    {
        return EndExperiment();
    }
}

ExperimentIterator MolProp::getLot(const char *lot)
{
    ExperimentIterator       ci;

    std::vector<std::string> ll = split(lot, '/');
    if (ll.size() == 2 && (ll[0].length() > 0) && (ll[1].length() > 0))
    {
        bool done = false;
        for (ci = BeginExperiment(); (!done) && (ci < EndExperiment()); ci++)
        {
            done = ((strcasecmp(ci->getMethod().c_str(), ll[0].c_str()) == 0) &&
                    (strcasecmp(ci->getBasisset().c_str(), ll[1].c_str()) == 0));
            if (done)
            {
                break;
            }
        }
        return ci;
    }
    else
    {
        return EndExperiment();
    }
}

CommunicationStatus MolecularQuadrupole::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = GenericProperty::Send(cr, dest);
    if (CS_OK == cs)
    {
        cs = gmx_send_data(cr, dest);
    }
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, xx_);
        gmx_send_double(cr, dest, yy_);
        gmx_send_double(cr, dest, zz_);
        gmx_send_double(cr, dest, xy_);
        gmx_send_double(cr, dest, xz_);
        gmx_send_double(cr, dest, yz_);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send MolecularQuadrupole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularQuadrupole::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = GenericProperty::Receive(cr, src);
    if (CS_OK == cs)
    {
        cs = gmx_recv_data(cr, src);
    }
    if (CS_OK == cs)
    {
        xx_    = gmx_recv_double(cr, src);
        yy_    = gmx_recv_double(cr, src);
        zz_    = gmx_recv_double(cr, src);
        xy_    = gmx_recv_double(cr, src);
        xz_    = gmx_recv_double(cr, src);
        yz_    = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to received MolecularQuadrupole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularPolarizability::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = GenericProperty::Send(cr, dest);
    if (CS_OK == cs)
    {
        cs = gmx_send_data(cr, dest);
    }
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, xx_);
        gmx_send_double(cr, dest, yy_);
        gmx_send_double(cr, dest, zz_);
        gmx_send_double(cr, dest, xy_);
        gmx_send_double(cr, dest, xz_);
        gmx_send_double(cr, dest, yz_);
        gmx_send_double(cr, dest, _average);
        gmx_send_double(cr, dest, _error);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send MolecularQuadrupole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularPolarizability::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = GenericProperty::Receive(cr, src);
    if (CS_OK == cs)
    {
        cs = gmx_recv_data(cr, src);
    }
    if (CS_OK == cs)
    {
        xx_      = gmx_recv_double(cr, src);
        yy_      = gmx_recv_double(cr, src);
        zz_      = gmx_recv_double(cr, src);
        xy_      = gmx_recv_double(cr, src);
        xz_      = gmx_recv_double(cr, src);
        yz_      = gmx_recv_double(cr, src);
        _average = gmx_recv_double(cr, src);
        _error   = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to received MolecularQuadrupole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularEnergy::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = GenericProperty::Receive(cr, src);
    if (CS_OK == cs)
    {
        cs = gmx_recv_data(cr, src);
    }
    if (CS_OK == cs)
    {
        _value = gmx_recv_double(cr, src);
        _error = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive MolecularEnergy, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularEnergy::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = GenericProperty::Send(cr, dest);
    if (CS_OK == cs)
    {
        cs = gmx_send_data(cr, dest);
    }
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, _value);
        gmx_send_double(cr, dest, _error);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send MolecularEnergy, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus Bond::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_int(cr, dest, _ai);
        gmx_send_int(cr, dest, _aj);
        gmx_send_int(cr, dest, _bondorder);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send Bond, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus Bond::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        _ai        = gmx_recv_int(cr, src);
        _aj        = gmx_recv_int(cr, src);
        _bondorder = gmx_recv_int(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive Bond, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularDipole::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = GenericProperty::Send(cr, dest);
    if (CS_OK == cs)
    {
        cs = gmx_send_data(cr, dest);
    }
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, _x);
        gmx_send_double(cr, dest, _y);
        gmx_send_double(cr, dest, _z);
        gmx_send_double(cr, dest, _aver);
        gmx_send_double(cr, dest, _error);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send MolecularDipole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus MolecularDipole::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = GenericProperty::Receive(cr, src);
    if (CS_OK == cs)
    {
        cs = gmx_recv_data(cr, src);
    }
    if (CS_OK == cs)
    {
        _x     = gmx_recv_double(cr, src);
        _y     = gmx_recv_double(cr, src);
        _z     = gmx_recv_double(cr, src);
        _aver  = gmx_recv_double(cr, src);
        _error = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive MolecularDipole, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus Experiment::Receive(t_commrec *cr, int src)
{
    CalcAtomIterator               cai;
    CommunicationStatus            cs;
    ElectrostaticPotentialIterator epi;
    std::string                    jobtype;
    int                            Npolar, Ndipole, Nenergy, Npotential, Natom;
    
    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        gmx_recv_str(cr, src, &reference_);
        gmx_recv_str(cr, src, &conformation_);
        gmx_recv_str(cr, src, &_program);
        gmx_recv_str(cr, src, &_method);
        gmx_recv_str(cr, src, &_basisset);
        gmx_recv_str(cr, src, &_datafile);
        gmx_recv_str(cr, src, &jobtype);
        jobtype_   = string2jobType(jobtype);
        Npolar     = gmx_recv_int(cr, src);
        Ndipole    = gmx_recv_int(cr, src);
        Nenergy    = gmx_recv_int(cr, src);
        Npotential = gmx_recv_int(cr, src);
        Natom      = gmx_recv_int(cr, src);

        //! Receive Polarizabilities        
        for (int n = 0; (CS_OK == cs) && (n < Npolar); n++)
        {
            MolecularPolarizability mp;
            cs = mp.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddPolar(mp);
            }
        }

        //! Receive Dipoles
        for (int n = 0; (CS_OK == cs) && (n < Ndipole); n++)
        {
            MolecularDipole md;
            cs = md.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddDipole(md);
            }
        }

        //! Receive Energies
        for (int n = 0; (CS_OK == cs) && (n < Nenergy); n++)
        {
            MolecularEnergy me;
            cs = me.Receive(cr, src);
            if  (CS_OK == cs)
            {
                AddEnergy(me);
            }
        }
                
        //! Receive Potentials
        for (int n = 0; (CS_OK == cs) && (n < Npotential); n++)
        {
            ElectrostaticPotential ep;
            cs = ep.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddPotential(ep);
            }
        }
        
        //! Receive Atoms
        for (int n = 0; (CS_OK == cs) && (n < Natom); n++)
        {
            CalcAtom ca;
            cs = ca.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddAtom(ca);
            }
        }      
    }

    if ((CS_OK != cs) && (nullptr != debug))
    {
        fprintf(debug, "Trying to receive Experiment, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus Experiment::Send(t_commrec *cr, int dest)
{
    CalcAtomIterator               cai;
    CommunicationStatus            cs;
    ElectrostaticPotentialIterator epi;
   
    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_str(cr, dest, reference_.c_str());
        gmx_send_str(cr, dest, conformation_.c_str());      
        gmx_send_str(cr, dest, _program.c_str());
        gmx_send_str(cr, dest, _method.c_str());
        gmx_send_str(cr, dest, _basisset.c_str());
        gmx_send_str(cr, dest, _datafile.c_str());
        gmx_send_str(cr, dest, jobType2string(jobtype_));        
        gmx_send_int(cr, dest, polar_.size());
        gmx_send_int(cr, dest, dipole_.size());
        gmx_send_int(cr, dest, energy_.size());
        gmx_send_int(cr, dest, _potential.size());
        gmx_send_int(cr, dest, _catom.size());
        

        //! Send Polarizabilities
        for (auto dpi = BeginPolar(); (CS_OK == cs) && (dpi < EndPolar()); dpi++)
        {
            cs = dpi->Send(cr, dest);
        }

        //! Send Dipoles
        for (auto dpi = BeginDipole(); (CS_OK == cs) && (dpi < EndDipole()); dpi++)
        {
            cs = dpi->Send(cr, dest);
        }

        //! Send Energies
        for (auto mei = BeginEnergy(); (CS_OK == cs) && (mei < EndEnergy()); mei++)
        {
            cs = mei->Send(cr, dest);
        }
        
        //! Send Potentials
        for (epi = BeginPotential(); (CS_OK == cs) && (epi < EndPotential()); epi++)
        {
            cs = epi->Send(cr, dest);
        }

        //! Send Atoms
        for (cai = BeginAtom(); (CS_OK == cs) && (cai < EndAtom()); cai++)
        {
            cs = cai->Send(cr, dest);
        }
    }  
     
    if ((CS_OK != cs) && (nullptr != debug))
    {
        fprintf(debug, "Trying to send Experiment, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus ElectrostaticPotential::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        gmx_recv_str(cr, src, &xyzUnit_);
        gmx_recv_str(cr, src, &vUnit_);
        espID_ = gmx_recv_int(cr, src);
        x_     = gmx_recv_double(cr, src);
        y_     = gmx_recv_double(cr, src);
        z_     = gmx_recv_double(cr, src);
        V_     = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive ElectrostaticPotential, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus ElectrostaticPotential::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_str(cr, dest, xyzUnit_.c_str());
        gmx_send_str(cr, dest, vUnit_.c_str());
        gmx_send_int(cr, dest, espID_);
        gmx_send_double(cr, dest, x_);
        gmx_send_double(cr, dest, y_);
        gmx_send_double(cr, dest, z_);
        gmx_send_double(cr, dest, V_);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send ElectrostaticPotential, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus AtomicCharge::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = GenericProperty::Receive(cr, src);

    if (CS_OK == cs)
    {
        cs = gmx_recv_data(cr, src);
    }
    if (CS_OK == cs)
    {
        _q = gmx_recv_double(cr, src);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to receive AtomicCharge, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus AtomicCharge::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = GenericProperty::Send(cr, dest);

    if (CS_OK == cs)
    {
        cs = gmx_send_data(cr, dest);
    }
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, _q);
    }
    else if (nullptr != debug)
    {
        fprintf(debug, "Trying to send AtomicCharge, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus CalcAtom::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;
    int Ncharge;
    
    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        gmx_recv_str(cr, src, &name_);
        gmx_recv_str(cr, src, &obType_);
        atomID_ = gmx_recv_int(cr, src);
        gmx_recv_str(cr, src, &unit_);
        x_ = gmx_recv_double(cr, src);
        y_ = gmx_recv_double(cr, src);
        z_ = gmx_recv_double(cr, src);
        Ncharge = gmx_recv_int(cr, src);
        
        for (int n = 0; (CS_OK == cs) && (n < Ncharge); n++)
        {
            AtomicCharge aq;
            cs = aq.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddCharge(aq);
            }
        }
    }
    if (nullptr != debug)
    {
        fprintf(debug, "Received CalcAtom, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus CalcAtom::Send(t_commrec *cr, int dest)
{
    CommunicationStatus  cs;
    AtomicChargeIterator qi;

    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_str(cr, dest, name_.c_str());
        gmx_send_str(cr, dest, obType_.c_str());
        gmx_send_int(cr, dest, atomID_);
        gmx_send_str(cr, dest, unit_.c_str());
        gmx_send_double(cr, dest, x_);
        gmx_send_double(cr, dest, y_);
        gmx_send_double(cr, dest, z_);
        gmx_send_int(cr, dest, q_.size());

        for (qi = BeginQ(); (CS_OK == cs) && (qi < EndQ()); qi++)
        {
            cs = qi->Send(cr, dest);
        }
    }
    if (nullptr != debug)
    {
        fprintf(debug, "Sent CalcAtom, status %s\n", cs_name(cs));
        fflush(debug);
    }
    return cs;
}

CommunicationStatus AtomNum::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs;

    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_str(cr, dest, _catom.c_str());
        gmx_send_int(cr, dest, _cnumber);
        if (nullptr != debug)
        {
            fprintf(debug, "Sent AtomNum %s %d, status %s\n",
                    _catom.c_str(), _cnumber, cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

CommunicationStatus AtomNum::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;

    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        gmx_recv_str(cr, src, &_catom);
        _cnumber = gmx_recv_int(cr, src);
        if (nullptr != debug)
        {
            fprintf(debug, "Received AtomNum %s %d, status %s\n",
                    _catom.c_str(), _cnumber, cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

CommunicationStatus MolecularComposition::Send(t_commrec *cr, int dest)
{
    CommunicationStatus cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_int(cr, dest, _atomnum.size());
        gmx_send_str(cr, dest, _compname.c_str());
        for (auto ani = BeginAtomNum(); (CS_OK == cs) && (ani < EndAtomNum()); ani++)
        {
            cs = ani->Send(cr, dest);
        }
        if (nullptr != debug)
        {
            fprintf(debug, "Sent MolecularComposition %s, status %s\n",
                    _compname.c_str(), cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

CommunicationStatus MolecularComposition::Receive(t_commrec *cr, int src)
{
    int Natomnum;
    CommunicationStatus cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        Natomnum = gmx_recv_int(cr, src); 
        gmx_recv_str(cr, src, &_compname);      
        CommunicationStatus cs2;
        for(int n = 0; n < Natomnum; n++)
        {
            AtomNum an;
            cs2 = an.Receive(cr, src);
            if (CS_OK == cs2)
            {
                AddAtom(an);
            }
        }
        if (nullptr != debug)
        {
            fprintf(debug, "Received MolecularComposition %s, status %s\n",
                    _compname.c_str(), cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

CommunicationStatus MolProp::Send(t_commrec *cr, int dest)
{
    CommunicationStatus                cs;
    BondIterator                       bi;
    MolecularCompositionIterator       mci;
    std::vector<std::string>::iterator si;
    ExperimentIterator                 ei;

    /* Generic stuff */
    cs = gmx_send_data(cr, dest);
    if (CS_OK == cs)
    {
        gmx_send_double(cr, dest, _mass);
        gmx_send_int(cr, dest, _charge);
        gmx_send_int(cr, dest, _multiplicity);
        gmx_send_str(cr, dest, _formula.c_str());
        gmx_send_str(cr, dest, _molname.c_str());
        gmx_send_str(cr, dest, _iupac.c_str());
        gmx_send_str(cr, dest, _cas.c_str());
        gmx_send_str(cr, dest, _cid.c_str());
        gmx_send_str(cr, dest, _inchi.c_str());
        gmx_send_int(cr, dest, _bond.size());
        gmx_send_int(cr, dest, _mol_comp.size());
        gmx_send_int(cr, dest, category_.size());
        gmx_send_int(cr, dest, _exper.size());

        /* Send Bonds */
        for (bi = BeginBond(); (CS_OK == cs) && (bi < EndBond()); bi++)
        {
            cs = bi->Send(cr, dest);
        }

        /* Send Composition */
        for (mci = BeginMolecularComposition(); (CS_OK == cs) && (mci < EndMolecularComposition()); mci++)
        {
            cs = mci->Send(cr, dest);
        }

        /* send Categories */
        for (si = BeginCategory(); (CS_OK == cs) && (si < EndCategory()); si++)
        {
            cs = gmx_send_data(cr, dest);
            if (CS_OK == cs)
            {
                gmx_send_str(cr, dest, si->c_str());
                if (nullptr != debug)
                {
                    fprintf(debug, "Sent category %s\n", si->c_str());
                    fflush(debug);
                }
            }
        }

        /* Send Experiments */
        for (ei = BeginExperiment(); (CS_OK == cs) && (ei < EndExperiment()); ei++)
        {
            cs = ei->Send(cr, dest);
        }
        
        if (nullptr != debug)
        {
            fprintf(debug, "Sent MolProp %s, status %s\n",
                    getMolname().c_str(), cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

CommunicationStatus MolProp::Receive(t_commrec *cr, int src)
{
    CommunicationStatus cs;
    int Nbond, Nmol_comp, Ncategory, Nexper; 

    /* Generic stuff */
    cs = gmx_recv_data(cr, src);
    if (CS_OK == cs)
    {
        //! Receive mass and more
        _mass         = gmx_recv_double(cr, src);
        _charge       = gmx_recv_int(cr, src);
        _multiplicity = gmx_recv_int(cr, src);
        gmx_recv_str(cr, src, &_formula);
        gmx_recv_str(cr, src, &_molname);
        gmx_recv_str(cr, src, &_iupac);
        gmx_recv_str(cr, src, &_cas);
        gmx_recv_str(cr, src, &_cid);
        gmx_recv_str(cr, src, &_inchi);
        Nbond     = gmx_recv_int(cr, src);
        Nmol_comp = gmx_recv_int(cr, src);
        Ncategory = gmx_recv_int(cr, src);
        Nexper    = gmx_recv_int(cr, src);
        
        if (nullptr != debug)
        {
            fprintf(debug, "Got molname %s\n", getMolname().c_str());
        }
        //! Receive Bonds       
        for(int n = 0; (CS_OK == cs) && (n < Nbond); n++)
        {
            Bond b;
            cs = b.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddBond(b);
            }
        }

        //! Receive Compositions       
        for(int n = 0; (CS_OK == cs) && (n < Nmol_comp); n++)
        {
            MolecularComposition mc;
            cs = mc.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddComposition(mc);
            }
        }

        //! Receive Categories      
        for(int n = 0; (CS_OK == cs) && (n < Ncategory); n++)
        {
            cs = gmx_recv_data(cr, src);
            if (CS_OK == cs)
            {
                std::string str;
                gmx_recv_str(cr, src, &str);
                if (!str.empty())
                {
                    AddCategory(str);
                    if (nullptr != debug)
                    {
                      fprintf(debug, "Received a category %s\n", str.c_str());
                        fflush(debug);
                    }
                }
                else
                {
                    gmx_fatal(FARGS, "A category was promised but I got a NULL pointer");
                }
            }
        }

        //! Receive Experiments    
        for(int n = 0; (CS_OK == cs) && (n < Nexper); n++)
        {
            Experiment ex;
            cs = ex.Receive(cr, src);
            if (CS_OK == cs)
            {
                AddExperiment(ex);
            }
        }

        if (nullptr != debug)
        {
            fprintf(debug, "Reveived %d experiments from %d for mol %s\n",
                    NExperiment(), src, getMolname().c_str());
            fprintf(debug, "Received MolProp %s, status %s\n",
                    getMolname().c_str(), cs_name(cs));
            fflush(debug);
        }
    }
    return cs;
}

const std::string &MolProp::getTexFormula() const
{
    if (_texform.size() > 0)
    {
        return _texform;
    }
    else
    {
        return _formula;
    }
}

}
