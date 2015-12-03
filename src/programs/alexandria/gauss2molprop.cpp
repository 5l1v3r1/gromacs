/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#include "gmxpre.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gromacs/commandline/filenm.h"
#include "gromacs/commandline/pargs.h"
#include "gromacs/fileio/confio.h"
#include "gromacs/fileio/copyrite.h"
#include "gromacs/fileio/pdbio.h"
#include "gromacs/fileio/txtdump.h"
#include "gromacs/gmxlib/readinp.h"
#include "gromacs/listed-forces/bonded.h"
#include "gromacs/math/units.h"
#include "gromacs/math/vec.h"
#include "gromacs/mdtypes/md_enums.h"
#include "gromacs/pbcutil/pbc.h"
#include "gromacs/topology/atomprop.h"
#include "gromacs/utility/cstringutil.h"
#include "gromacs/utility/real.h"
#include "gromacs/utility/smalloc.h"

#include "gauss_io.h"
#include "molprop.h"
#include "molprop_util.h"
#include "molprop_xml.h"
#include "poldata.h"
#include "poldata_xml.h"

int alex_gauss2molprop(int argc, char *argv[])
{
    static const char               *desc[] = {
        "gauss2molprop reads a series of Gaussian output files, and collects",
        "useful information, and saves it to molprop file."
    };

    t_filenm                         fnm[] = {
        { efLOG, "-g03",  "gauss",  ffRDMULT },
        { efDAT, "-o",    "molprop", ffWRITE }
    };
#define NFILE sizeof(fnm)/sizeof(fnm[0])
    static gmx_bool                  bVerbose   = FALSE;
    static char                     *molnm      = NULL, *iupac = NULL, *conf = (char *)"minimum", *basis = NULL;
    static const char               *forcefield = "GAFF";
    static int                       maxpot     = 0;
    static int                       nsymm      = 0;
    static gmx_bool                  compress   = FALSE;
    t_pargs                          pa[]       = {
        { "-v",      FALSE, etBOOL, {&bVerbose},
          "Generate verbose terminal output." },
        { "-compress", FALSE, etBOOL, {&compress},
          "Compress output XML files" },
        { "-molnm", FALSE, etSTR, {&molnm},
          "Name of the molecule in *all* input files. Do not use if you have different molecules in the input files." },
        { "-nsymm", FALSE, etINT, {&nsymm},
          "Symmetry number of the molecule can be supplied here if you know there is an error in the input file" },
        { "-iupac", FALSE, etSTR, {&iupac},
          "IUPAC name of the molecule in *all* input files. Do not use if you have different molecules in the input files." },
        { "-conf",  FALSE, etSTR, {&conf},
          "Conformation of the molecule" },
        { "-basis",  FALSE, etSTR, {&basis},
          "Basis-set used in this calculation for those case where it is difficult to extract from a Gaussian file" },
        { "-ff", FALSE, etSTR, {&forcefield},
          "Force field for basic atom typing available in OpenBabel" },
        { "-maxpot", FALSE, etINT, {&maxpot},
          "Max number of potential points to add to the molprop file. If 0 all points are registered, else a selection of points evenly spread over the range of values is taken" }
    };
    gmx_output_env_t                *oenv;
    gmx_atomprop_t                   aps;
    Poldata                         *pd;
    std::vector<alexandria::MolProp> mp;
    alexandria::GaussAtomProp        gap;
    char **fns = NULL;
    int    i, nfn;

    if (!parse_common_args(&argc, argv, 0, NFILE, fnm,
                           sizeof(pa)/sizeof(pa[0]), pa,
                           sizeof(desc)/sizeof(desc[0]), desc, 0, NULL, &oenv))
    {
        return 0;
    }

    /* Read standard atom properties */
    aps = gmx_atomprop_init();

    /* Read polarization stuff */
    if ((pd = alexandria::PoldataXml::read("", aps)) == NULL)
    {
        gmx_fatal(FARGS, "Can not read the force field information. File missing or incorrect.");
    }

    nfn = ftp2fns(&fns, efLOG, NFILE, fnm);
    for (i = 0; (i < nfn); i++)
    {
        alexandria::MolProp mmm;

        ReadGauss(fns[i], mmm, molnm, iupac, conf, basis,
                  maxpot, nsymm, pd->getForceField().c_str());
        mp.push_back(mmm);
    }

    printf("Succesfully read %d molprops from %d Gaussian files.\n",
           (int)mp.size(), nfn);
    MolPropSort(mp, MPSA_MOLNAME, NULL, NULL);
    merge_doubles(mp, NULL, TRUE);
    if (mp.size() > 0)
    {
        MolPropWrite(opt2fn("-o", NFILE, fnm), mp, (int)compress);
    }

    return 0;
}
