/*! \internal \brief
 * Implements part of the alexandria program.
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmx_simple_comm.h"

#include <string.h>

#include "gromacs/utility/fatalerror.h"
#include <gromacs/utility/gmxmpi.h>
#include "gromacs/utility/smalloc.h"

void gmx_send(const t_commrec *cr, int dest, void *buf, int bufsize)
{
#ifndef GMX_MPI
    gmx_call("gmx_send");
#else
    int         tag = 0;
    MPI_Status  status;
    MPI_Request req;

    if (MPI_Isend(buf, bufsize, MPI_BYTE, RANK(cr, dest), tag,
                  cr->mpi_comm_mygroup, &req) != MPI_SUCCESS)
    {
        gmx_comm("MPI_Isend Failed");
    }
    if (MPI_Wait(&req, &status) != MPI_SUCCESS)
    {
        gmx_comm("MPI_Wait failed");
    }
#endif
}

void gmx_recv(const t_commrec *cr, int src, void *buf, int bufsize)
{
#ifndef GMX_MPI
    gmx_call("gmx_recv");
#else
    int         tag = 0;
    MPI_Status  status;
    MPI_Request req;

    if (MPI_Irecv(buf, bufsize, MPI_BYTE, src, tag,
                  cr->mpi_comm_mygroup, &req) != MPI_SUCCESS)
    {
        gmx_comm("MPI_Irecv Failed");
    }
    if (MPI_Wait(&req, &status) != MPI_SUCCESS)
    {
        gmx_comm("MPI_Wait failed");
    }
#endif
}

void gmx_send_str(t_commrec *cr, int dest, const char *ptr)
{
    int len;

    if (nullptr == ptr)
    {
        len = 0;
    }
    else
    {
        len = strlen(ptr)+1;
    }
    if (nullptr != debug)
    {
        fprintf(debug, "Sending string '%s' to %d\n", ptr, dest);
    }
    gmx_send(cr, dest, &len, sizeof(len));
    if (nullptr != ptr)
    {
        gmx_send(cr, dest, (void *)ptr, len);
    }
}

void gmx_recv_str(t_commrec *cr, int src, std::string *str)
{
    int   len;
    
    gmx_recv(cr, src, &len, sizeof(len));
    if (len == 0)
    {
        str->clear();
    }
    else
    {
        str->resize(len-1);
        gmx_recv(cr, src, (void*)str->data(), len);
    }
}

void gmx_send_double(t_commrec *cr, int dest, double d)
{
    if (nullptr != debug)
    {
        fprintf(debug, "Sending double '%g' to %d\n", d, dest);
    }
    gmx_send(cr, dest, &d, sizeof(d));
}

double gmx_recv_double(t_commrec *cr, int src)
{
    double d;

    gmx_recv(cr, src, &d, sizeof(d));

    return d;
}

void gmx_send_int(t_commrec *cr, int dest, int d)
{
    if (nullptr != debug)
    {
        fprintf(debug, "Sending int '%d' to %d\n", d, dest);
    }
    gmx_send(cr, dest, &d, sizeof(d));
}

int gmx_recv_int(t_commrec *cr, int src)
{
    int d;

    gmx_recv(cr, src, &d, sizeof(d));

    return d;
}
