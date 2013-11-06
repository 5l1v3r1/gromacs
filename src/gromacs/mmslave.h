/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2013, by the GROMACS development team, led by
 * David van der Spoel, Berk Hess, Erik Lindahl, and including many
 * others, as listed in the AUTHORS file in the top-level source
 * directory and at http://www.gromacs.org.
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
/*! \file
 * \brief
 * Declares code for calling GROMACS routines from an external program
 *
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 * \inpublicapi
 * \ingroup module_qmmm
 */
#ifndef GMX_MMSLAVE_H
#define GMX_MMSLAVE_H

#ifdef __cplusplus
extern "C" {
#endif

    //! Abstract type for the mmslave code
    typedef struct gmx_mmslave *gmx_mmslave_t;

    //! Create the data structure and returns it
    extern gmx_mmslave_t mmslave_init(void);
    
    /*! \brief
     * Function to clean up
     * \param[in] gms  the data structure to be freed
     */
    extern void mmslave_done(gmx_mmslave_t gms);

    /*! \brief
     * Function to read a tpr file and store the information in the
     * data structur
     * \param[in] tpr  the file name
     * \param[in] gms  the data structure to be read
     * \return TRUE if successful
     */
    extern gmx_bool mmslave_read_tpr(const char *tpr, 
                                     gmx_mmslave_t gms);

    /*! \brief
     * Function to modify the charge of an atom
     * data structure
     * \param[in] gms  the data structure to be modified
     * \param[in] id   the atom id
     * \param[in] q    the new charge
     * \return TRUE if successful
     */
    extern gmx_bool mmslave_set_q(gmx_mmslave_t gms,
                                  atom_id id,
                                  double q);

    /*! \brief
     * Function to compute the energy and forces
     * \param[in]  gms the data structure to be modified
     * \param[in]  x   the atomic coordinates for the whole system (MM+QM)
     * \param[out] f   the forces on all atoms
     * \param[out] energy the total MM energy
     * \return TRUE if successful
     */
    extern gmx_bool mmslave_calc_energy(gmx_mmslave_t gms, 
                                        const rvec *x,
                                        rvec *f,
                                        double *energy);
    
    
#ifdef __cplusplus
}
#endif

#endif
