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
/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#include "gmxpre.h"

#include <ctype.h>
#include <stdlib.h>

#include "gromacs/commandline/filenm.h"
#include "gromacs/commandline/pargs.h"
#include "gromacs/fileio/confio.h"
#include "gromacs/fileio/copyrite.h"
#include "gromacs/fileio/pdbio.h"
#include "gromacs/fileio/strdb.h"
#include "gromacs/fileio/txtdump.h"
#include "gromacs/gmxlib/readinp.h"
#include "gromacs/gmxpreprocess/gpp_atomtype.h"
#include "gromacs/listed-forces/bonded.h"
#include "gromacs/math/vec.h"
#include "gromacs/mdtypes/md_enums.h"
#include "gromacs/mdtypes/state.h"
#include "gromacs/pbcutil/pbc.h"
#include "gromacs/random/random.h"
#include "gromacs/topology/atomprop.h"
#include "gromacs/utility/cstringutil.h"
#include "gromacs/utility/init.h"
#include "gromacs/utility/real.h"
#include "gromacs/utility/smalloc.h"

#include "gauss_io.h"
#include "gentop_core.h"
#include "gentop_qgen.h"
#include "gentop_vsite.h"
#include "gmx_resp.h"
#include "molprop_util.h"
#include "molprop_xml.h"
#include "mymol.h"
#include "poldata.h"
#include "poldata_xml.h"
#include "stringutil.h"

static void clean_pdb_names(t_atoms *atoms, t_symtab *tab)
{
    int   i, changed;
    char *ptr, buf[128];

    for (i = 0; (i < atoms->nr); i++)
    {
        changed = 0;
        strncpy(buf, *(atoms->atomname[i]), sizeof(buf));
        while ((ptr = strchr(buf, ' ')) != NULL)
        {
            *ptr    = '_';
            changed = 1;
        }
        if (changed)
        {
            atoms->atomname[i] = put_symtab(tab, buf);
        }
    }
}

int alex_gentop(int argc, char *argv[])
{
    static const char               *desc[] = {
        "gentop generates a topology from molecular coordinates",
        "either from a file, from a database, or from a gaussian log file.",
        "The program assumes all hydrogens are present when defining",
        "the hybridization from the atom name and the number of bonds.[PAR]",
        "If the [TT]-oi[tt] option is set an [TT]itp[tt] file will be generated",
        "instead of a [TT]top[tt] file.",
        "When [TT]-param[tt] is set, equilibrium distances and angles",
        "and force constants will be printed in the topology for all",
        "interactions. The equilibrium distances and angles are taken",
        "from the input coordinates, the force constant are set with",
        "command line options.",
        "With the [TT]-db molecule[tt] option a file is extracted from the",
        "database from one of the specified QM calculations (given with [TT]-lot[tt]).",
        "An alternative to the system-wide database [TT]molprops.dat[tt]",
        "can be passed along using the [TT]-mpdb[tt] flag.[PAR]",
        "If the flag [TT]-qgen[tt] is given, charges will be generated using the",
        "specified algorithm. Without the flag the charges from the QM calculation",
        "will be used.",
        "The only supported force field for this tool is Alexandria.[PAR]",
        "oplsaa OPLS-AA/L all-atom force field (2001 aminoacid dihedrals)[PAR]",
        "The corresponding data files can be found in the library directory",
        "with names like ffXXXX.YYY. Check chapter 5 of the manual for more",
        "information about file formats. The default forcefield is Alexandria",
        "but selection can be made interactive, using the [TT]-ff select[tt] option.",
        "one of the short names above on the command line instead.[PAR]"
    };
    const char                      *bugs[] = {
        "No force constants for impropers are generated"
    };
    gmx_output_env_t                *oenv;
    gmx_atomprop_t                   aps;
    Poldata                         *pd;
    gmx_bool                         bTOP;
    char                             forcefield[STRLEN], ffdir[STRLEN];
    char                             ffname[STRLEN];
    std::vector<alexandria::MolProp> mps;
    alexandria::MolPropIterator      mpi;
    alexandria::MyMol                mymol;
    immStatus                        imm;

    t_filenm                         fnm[] = {
        { efSTX, "-f",    "conf", ffOPTRD },
        { efTOP, "-o",    "out",  ffOPTWR },
        { efITP, "-oi",   "out",  ffOPTWR },
        { efSTO, "-c",    "out",  ffWRITE },
        { efLOG, "-g03",  "gauss",  ffOPTRD },
        { efNDX, "-n",    "renum", ffOPTWR },
        { efDAT, "-q",    "qout", ffOPTWR },
        { efDAT, "-mpdb", "molprops", ffOPTRD },
        { efDAT, "-d",    "gentop", ffOPTRD },
        { efCUB, "-pot",  "potential", ffOPTWR },
        { efCUB, "-ref",  "refpot", ffOPTRD },
        { efCUB, "-diff", "diffpot", ffOPTWR },
        { efCUB, "-rho",  "density", ffOPTWR },
        { efXVG, "-diffhist", "diffpot", ffOPTWR },
        { efXVG, "-his",  "pot-histo", ffOPTWR },
        { efXVG, "-pc",   "pot-comp", ffOPTWR },
        { efPDB, "-pdbdiff", "pdbdiff", ffOPTWR }
    };
#define NFILE sizeof(fnm)/sizeof(fnm[0])
    static real                      kb             = 4e5, kt = 400, kp = 5;
    static real                      btol           = 0.2, qtol = 1e-10, zmin = 5, zmax = 100, delta_z = -1;
    static real                      hfac           = 0, qweight = 1e-3, bhyper = 0.1;
    static real                      th_toler       = 170, ph_toler = 5, watoms = 0, spacing = 0.1;
    static real                      dbox           = 0.370424, penalty_fac = 1, epsr = 1;
    static int                       nexcl          = 2;
    static int                       maxiter        = 25000, maxcycle = 1;
    static int                       nmol           = 1;
    static real                      rDecrZeta      = -1;
    static gmx_bool                  bRemoveDih     = FALSE, bQsym = TRUE, bZatype = TRUE, bFitCube = FALSE;
    static gmx_bool                  bParam         = FALSE, bH14 = TRUE, bRound = TRUE, bITP;
    static const char               *polaropt[]     = { NULL, "No", "AllAtom", "UnitedAtom", NULL };
    static gmx_bool                  bPairs         = FALSE, bPBC = TRUE;
    static gmx_bool                  bUsePDBcharge  = FALSE, bVerbose = TRUE, bAXpRESP = FALSE;
    static gmx_bool                  bCONECT        = FALSE, bRandZeta = FALSE, bRandQ = TRUE, bFitZeta = TRUE, bEntropy = FALSE;
    static gmx_bool                  bGenVSites     = FALSE, bSkipVSites = TRUE;
    static char                     *molnm          = (char *)"", *iupac = (char *)"", *dbname = (char *)"", *symm_string = (char *)"", *conf = (char *)"minimum", *basis = (char *)"";
    static int                       maxpot         = 0;
    static int                       seed           = 0;
    static int                       nsymm          = 0;
    static const char               *cqdist[]       = {
        NULL, "AXp", "AXs", "AXg", "Yang", "Bultinck", "Rappe", NULL
    };
    static const char               *cqgen[]       = {
        NULL, "None", "EEM", "ESP", "RESP", NULL
    };
    static const char               *dihopt[] = { NULL, "No", "Single", "All", NULL };
    static const char               *cgopt[]  = { NULL, "Atom", "Group", "Neutral", NULL };
    static const char               *lot      = "B3LYP/aug-cc-pVTZ";
    static const char               *dzatoms  = "";
    static const char               *ff       = "alexandria";
    t_pargs                          pa[]     = {
        { "-v",      FALSE, etBOOL, {&bVerbose},
          "Generate verbose output in the top file and on terminal." },
        { "-ff",     FALSE, etSTR,  {&ff},
          "Force field, interactive by default. Use -h for information." },
        { "-db",     FALSE, etSTR,  {&dbname},
          "Read a molecule from the database rather than from a file" },
        { "-lot",    FALSE, etSTR,  {&lot},
          "Use this method and level of theory when selecting coordinates and charges" },
        { "-nexcl", FALSE, etINT,  {&nexcl},
          "HIDDENNumber of exclusions. Check consistency of this option with the [TT]-pairs[tt] flag." },
        { "-H14",    FALSE, etBOOL, {&bH14},
          "HIDDENUse 3rd neighbour interactions for hydrogen atoms" },
        { "-dih",    FALSE, etSTR,  {dihopt},
          "Which proper dihedrals to generate: none, one per rotatable bond, or all possible." },
        { "-remdih", FALSE, etBOOL, {&bRemoveDih},
          "HIDDENRemove dihedrals on the same bond as an improper" },
        { "-pairs",  FALSE, etBOOL, {&bPairs},
          "HIDDENOutput 1-4 interactions (pairs) in topology file. Check consistency of your option with the [TT]-nexcl[tt] flag." },
        { "-name",   FALSE, etSTR,  {&molnm},
          "Name of your molecule" },
        { "-iupac",   FALSE, etSTR,  {&iupac},
          "IUPAC Name of your molecule" },
        { "-conf",  FALSE, etSTR, {&conf},
          "Conformation of the molecule" },
        { "-basis",  FALSE, etSTR, {&basis},
          "Basis-set used in this calculation for those case where it is difficult to extract from a Gaussian file" },
        { "-maxpot", FALSE, etINT, {&maxpot},
          "Max number of potential points to add to the molprop file. If 0 all points are registered, else a selection of points evenly spread over the range of values is taken" },
        { "-nsymm", FALSE, etINT, {&nsymm},
          "Symmetry number of the molecule can be supplied here if you know there is an error in the input file" },
        { "-pbc",    FALSE, etBOOL, {&bPBC},
          "Use periodic boundary conditions." },
        { "-seed",   FALSE, etINT,  {&seed},
          "Random number seed. If zero, a seed will be generated." },
        { "-conect", FALSE, etBOOL, {&bCONECT},
          "HIDDENUse CONECT records in an input pdb file to signify bonds" },
        { "-genvsites", FALSE, etBOOL, {&bGenVSites},
          "Generate virtual sites for linear groups. Check and double check." },
        { "-skipvsites", FALSE, etBOOL, {&bSkipVSites},
          "HIDDENSkip virtual sites in the input file" },
        { "-pdbq",  FALSE, etBOOL, {&bUsePDBcharge},
          "HIDDENUse the B-factor supplied in a pdb file for the atomic charges" },
        { "-btol",  FALSE, etREAL, {&btol},
          "HIDDENRelative tolerance for determining whether two atoms are bonded." },
        { "-epsr", FALSE, etREAL, {&epsr},
          "HIDDENRelative dielectric constant to account for intramolecular polarization. Should be >= 1." },
        { "-spacing", FALSE, etREAL, {&spacing},
          "Spacing of grid points for computing the potential (not used when a reference file is read)." },
        { "-dbox", FALSE, etREAL, {&dbox},
          "HIDDENExtra space around the molecule when generating an ESP output file with the [TT]-pot[tt] option. The strange default value corresponds to 0.7 a.u. that is sometimes used in other programs." },
        { "-axpresp", FALSE, etBOOL, {&bAXpRESP},
          "Turn on RESP features for AXp fitting" },
        { "-qweight", FALSE, etREAL, {&qweight},
          "Restraining force constant for the RESP algorithm (AXp only, and with [TT]-axpresp[tt])." },
        { "-bhyper", FALSE, etREAL, {&bhyper},
          "Hyperbolic term for the RESP algorithm (AXp only), and with [TT]-axpresp[tt])." },
        { "-entropy", FALSE, etBOOL, {&bEntropy},
          "HIDDENUse maximum entropy criterion for optimizing to ESP data rather than good ol' RMS" },
        { "-fitcube", FALSE, etBOOL, {&bFitCube},
          "HIDDENFit to the potential in the cube file rather than the log file. This typically gives incorrect results if it converges at all, because points close to the atoms are taken into account on equal footing with points further away." },
        { "-zmin",  FALSE, etREAL, {&zmin},
          "HIDDENMinimum allowed zeta (1/nm) when fitting models containing gaussian or Slater charges to the ESP" },
        { "-zmax",  FALSE, etREAL, {&zmax},
          "HIDDENMaximum allowed zeta (1/nm) when fitting models containing gaussian or Slater charges to the ESP" },
        { "-deltaz", FALSE, etREAL, {&delta_z},
          "HIDDENMaximum allowed deviation from the starting value of zeta. If this option is set then both zmin and zmax will be ignored. A reasonable value would be 10/nm." },
        { "-dzatoms", FALSE, etSTR, {&dzatoms},
          "HIDDENList of atomtypes for which the fitting is restrained by the -deltaz option." },
        { "-zatype", FALSE, etBOOL, {&bZatype},
          "HIDDENUse the same zeta for each atom with the same atomtype in a molecule when fitting gaussian or Slater charges to the ESP" },
        { "-decrzeta", FALSE, etREAL, {&rDecrZeta},
          "HIDDENGenerate decreasing zeta with increasing row numbers for atoms that have multiple distributed charges. In this manner the 1S electrons are closer to the nucleus than 2S electrons and so on. If this number is < 0, nothing is done, otherwise a penalty is imposed in fitting if the Z2-Z1 < this number." },
        { "-randzeta", FALSE, etBOOL, {&bRandZeta},
          "HIDDENUse random zeta values within the zmin zmax interval when optimizing against Gaussian ESP data. If FALSE the initial values from the gentop.dat file will be used." },
        { "-randq", FALSE, etBOOL, {&bRandQ},
          "HIDDENUse random charges to start with when optimizing against Gaussian ESP data. Makes the optimization non-deterministic." },
        { "-fitzeta", FALSE, etBOOL, {&bFitZeta},
          "HIDDENControls whether or not the Gaussian/Slater widths are optimized when fitting to a QM computed ESP" },
        { "-pfac",   FALSE, etREAL, {&penalty_fac},
          "HIDDENFactor for weighing penalty function for e.g. [TT]-decrzeta[tt] option." },
        { "-watoms", FALSE, etREAL, {&watoms},
          "Weight for the atoms when fitting the charges to the electrostatic potential. The potential on atoms is usually two orders of magnitude larger than on other points (and negative). For point charges or single smeared charges use 0. For point+smeared charges 1 is recommended." },
        { "-param", FALSE, etBOOL, {&bParam},
          "Print parameters in the output" },
        { "-round",  FALSE, etBOOL, {&bRound},
          "Round off measured values for distances and angles" },
        { "-qgen",   FALSE, etENUM, {cqgen},
          "Algorithm used for charge generation" },
        { "-qdist",   FALSE, etENUM, {cqdist},
          "Charge distribution used" },
        { "-polar",  FALSE, etENUM, {&polaropt},
          "Add polarizable particles to the topology by specifying AllAtom or UnitedAtom (every atom except H)" },
        { "-qtol",   FALSE, etREAL, {&qtol},
          "Tolerance for assigning charge generation algorithm" },
        { "-maxiter", FALSE, etINT, {&maxiter},
          "Max number of iterations for charge generation algorithm" },
        { "-maxcycle", FALSE, etINT, {&maxcycle},
          "Max number of tries for optimizing the charges. The trial with lowest chi2 will be used for generating a topology. Will be turned off if randzeta is No." },
        { "-hfac",    FALSE, etREAL, {&hfac},
          "HIDDENFudge factor for AXx algorithms that modulates J00 for hydrogen atoms by multiplying it by (1 + hfac*qH). This hack is originally due to Rappe & Goddard." },
        { "-qsymm",  FALSE, etBOOL, {&bQsym},
          "Symmetrize the charges on symmetric groups, e.g. CH3, NH2." },
        { "-symm",   FALSE, etSTR, {&symm_string},
          "Use the order given here for symmetrizing, e.g. when specifying [TT]-symm '0 1 0'[tt] for a water molecule (H-O-H) the hydrogens will have obtain the same charge. For simple groups, like methyl (or water) this is done automatically, but higher symmetry is not detected by the program. The numbers should correspond to atom numbers minus 1, and point to either the atom itself or to a previous atom." },
        { "-cgsort", FALSE, etSTR, {cgopt},
          "HIDDENOption for assembling charge groups: based on Atom (default, does not change the atom order), Group (e.g. CH3 groups are kept together), or Neutral sections (try to find groups that together are neutral). If the order of atoms is changed an index file is written in order to facilitate changing the order in old files." },
        { "-nmolsort", FALSE, etINT, {&nmol},
          "HIDDENNumber of molecules to output to the index file in case of sorting. This is a convenience option to reorder trajectories for use with a new force field." },
        { "-th_toler", FALSE, etREAL, {&th_toler},
          "HIDDENIf bond angles are larger than this value the group will be treated as a linear one and a virtual site will be created to keep the group linear" },
        { "-ph_toler", FALSE, etREAL, {&ph_toler},
          "HIDDENIf dihedral angles are less than this (in absolute value) the atoms will be treated as a planar group with an improper dihedral being added to keep the group planar" },
        { "-kb",    FALSE, etREAL, {&kb},
          "HIDDENBonded force constant (kJ/mol/nm^2)" },
        { "-kt",    FALSE, etREAL, {&kt},
          "HIDDENAngle force constant (kJ/mol/rad^2)" },
        { "-kp",    FALSE, etREAL, {&kp},
          "HIDDENDihedral angle force constant (kJ/mol/rad^2)" }
    };

    if (!parse_common_args(&argc, argv, 0, NFILE, fnm,
                           sizeof(pa)/sizeof(pa[0]), pa,
                           sizeof(desc)/sizeof(desc[0]), desc,
                           sizeof(bugs)/sizeof(bugs[0]), bugs, &oenv))
    {
        return 0;
    }

    /* Force field selection, interactive or direct */
    choose_ff(strcmp(ff, "select") == 0 ? NULL : ff,
              forcefield, sizeof(forcefield),
              ffdir, sizeof(ffdir));

    if (strlen(forcefield) > 0)
    {
        strcpy(ffname, forcefield);
        ffname[0] = toupper(ffname[0]);
    }
    else
    {
        gmx_fatal(FARGS, "Empty forcefield string");
    }

    /* Check the options */
    bITP = opt2bSet("-oi", NFILE, fnm);
    bTOP = TRUE;

    if (!bRandZeta)
    {
        maxcycle = 1;
    }

    if (!bTOP)
    {
        gmx_fatal(FARGS, "Specify at least one output file");
    }

    if ((btol < 0) || (btol > 1))
    {
        gmx_fatal(FARGS, "Bond tolerance should be between 0 and 1 (not %g)", btol);
    }
    if ((qtol < 0) || (qtol > 1))
    {
        gmx_fatal(FARGS, "Charge tolerance should be between 0 and 1 (not %g)", qtol);
    }

    /* Check command line options of type enum */
    eDih                      edih = (eDih) get_option(dihopt);
    eChargeGroup              ecg  = (eChargeGroup) get_option(cgopt);
    ePolar                    epol = (ePolar) get_option(polaropt);
    ChargeGenerationAlgorithm iChargeGenerationAlgorithm = (ChargeGenerationAlgorithm) get_option(cqgen);
    ChargeDistributionModel   iChargeDistributionModel;
    if ((iChargeDistributionModel = Poldata::name2eemtype(cqdist[0])) == eqdNR)
    {
        gmx_fatal(FARGS, "Invalid model %s. How could you!\n", cqdist[0]);
    }

    /* Read standard atom properties */
    aps = gmx_atomprop_init();

    /* Read polarization stuff */
    if ((pd = alexandria::PoldataXml::read(opt2fn_null("-d", NFILE, fnm), aps)) == NULL)
    {
        gmx_fatal(FARGS, "Can not read the force field information. File missing or incorrect.");
    }
    if (bVerbose)
    {
        printf("Reading force field information. There are %d atomtypes.\n",
               pd->getNatypes());
    }

    if (strlen(dbname) > 0)
    {
        if (bVerbose)
        {
            printf("Reading molecule database.\n");
        }
        MolPropRead(opt2fn_null("-mpdb", NFILE, fnm), mps);

        for (mpi = mps.begin(); (mpi < mps.end()); mpi++)
        {
            if (strcasecmp(dbname, mpi->getMolname().c_str()) == 0)
            {
                break;
            }
        }
        if (mpi == mps.end())
        {
            gmx_fatal(FARGS, "Molecule %s not found in database", dbname);
        }
    }
    else
    {
        alexandria::MolProp       mp;
        alexandria::GaussAtomProp gap;
        const char               *fn;

        if (opt2bSet("-g03", NFILE, fnm))
        {
            fn = opt2fn("-g03", NFILE, fnm);
        }
        else
        {
            fn = opt2fn("-f", NFILE, fnm);
        }
        if (bVerbose)
        {
            printf("Will read (gaussian) file %s\n", fn);
        }
        if (strlen(molnm) == 0)
        {
            molnm = (char *)"XXX";
        }
        ReadGauss(fn, mp, molnm, iupac, conf, basis,
                  maxpot, nsymm, pd->getForceField().c_str());
        mps.push_back(mp);
        mpi = mps.begin();
    }

    bQsym = bQsym || (opt2parg_bSet("-symm", sizeof(pa)/sizeof(pa[0]), pa));

    mymol.molProp()->Merge(*mpi);
    mymol.SetForceField(forcefield);

    imm = mymol.GenerateTopology(aps, pd, lot, iChargeDistributionModel,
                                 nexcl,
                                 bGenVSites, bPairs, edih);


    if (immOK == imm)
    {
        mymol.AddShells(pd, epol, iChargeDistributionModel);
    }


    if ((immOK == imm)  && (eqgRESP == iChargeGenerationAlgorithm))
    {
        if (0 == seed)
        {
            seed = gmx_rng_make_seed();
        }
        /* mymol.gr_ = new Resp(iChargeDistributionModel, bAXpRESP, qweight, bhyper, mymol.getCharge(),
                                      zmin, zmax, delta_z,
                                      bZatype, watoms, rDecrZeta, bRandZeta, bRandQ,
                                      penalty_fac, bFitZeta,
                                      bEntropy, dzatoms, seed);*/

        mymol.gr_ = new Resp(iChargeDistributionModel, mymol.molProp()->getCharge());
        mymol.gr_->setBAXpRESP(bAXpRESP);
        if (NULL == mymol.gr_)
        {
            imm = immRespInit;
        }
    }

    if (immOK == imm)
    {
        imm = mymol.GenerateCharges(pd, aps,
                                    iChargeDistributionModel,
                                    iChargeGenerationAlgorithm,
                                    hfac, epsr,
                                    lot, TRUE, symm_string);
    }
    if (immOK == imm)
    {
        fprintf(stderr, "Fix me: GenerateCube is broken\n");
        if (0)
        {
            mymol.GenerateCube(iChargeDistributionModel,
                               pd,
                               spacing,
                               opt2fn_null("-ref", NFILE, fnm),
                               opt2fn_null("-pc", NFILE, fnm),
                               opt2fn_null("-pdbdiff", NFILE, fnm),
                               opt2fn_null("-pot", NFILE, fnm),
                               opt2fn_null("-rho", NFILE, fnm),
                               opt2fn_null("-his", NFILE, fnm),
                               opt2fn_null("-diff", NFILE, fnm),
                               opt2fn_null("-diffhist", NFILE, fnm),
                               oenv);
        }
    }

    if (immOK == imm)
    {
        mymol.GenerateChargeGroups(ecg, bUsePDBcharge);
    }


    if (immOK == imm)
    {
        if (bTOP)
        {
            mymol.PrintTopology(bITP ? ftp2fn(efITP, NFILE, fnm) :
                                ftp2fn(efTOP, NFILE, fnm),
                                iChargeDistributionModel, bVerbose,
                                pd, aps);
        }
        mymol.PrintConformation(opt2fn("-c", NFILE, fnm));
    }
    else
    {
        printf("\nWARNING: alexandria ended prematurely due to \"%s\"\n",
               alexandria::immsg(imm));
    }

    return 0;
}
