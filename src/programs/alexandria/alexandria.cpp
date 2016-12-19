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
 * Implements the alexandria wrapper binary.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#include "gmxpre.h"

#include "gromacs/commandline/cmdlineinit.h"
#include "gromacs/commandline/cmdlinemodulemanager.h"
#include "gromacs/gmxlib/network.h"
#include "gromacs/mdtypes/commrec.h"
#include "gromacs/selection/selhelp.h"
#include "gromacs/utility/exceptions.h"

#include "alex_modules.h"

int
main(int argc, char *argv[])
{
    gmx::CommandLineProgramContext &context = gmx::initForCommandLine(&argc, &argv);
    try
    {
        t_commrec *cr = init_commrec();
        gmx::CommandLineModuleManager manager("alexandria", &context);
        registerAlexandriaModules(&manager);
        manager.addHelpTopic(gmx::createSelectionHelpTopic());
        manager.setQuiet(true);
        if (MASTER(cr))
        {
            printf("\n                   Welcome to Alexandria\n\n");
            printf("Copyright (c) 2014-2016 David van der Spoel and Paul J. van Maaren\n");
            printf("See http://folding.bmc.uu.se/ for details.\n\n");
            printf("Alexandria is free software under the Gnu Public License v 2.\n");
            printf("Read more at http://www.gnu.org/licenses/gpl-2.0.html\n\n");
        }
        int rc = manager.run(argc, argv);
        gmx::finalizeForCommandLine();
        if (MASTER(cr))
        {
            printf("\nThanks for using Alexandria\n");
        }
        return rc;
    }
    catch (const std::exception &ex)
    {
        gmx::printFatalErrorMessage(stderr, ex);
        return gmx::processExceptionAtExit(ex);
    }
}
