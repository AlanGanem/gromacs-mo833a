/*
 *
 *                This source code is part of
 *
 *                 G   R   O   M   A   C   S
 *
 *          GROningen MAchine for Chemical Simulations
 *
 *                        VERSION 3.2.0
 * Written by David van der Spoel, Erik Lindahl, Berk Hess, and others.
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team,
 * check out http://www.gromacs.org for more information.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * If you want to redistribute modifications, please consider that
 * scientific software is very special. Version control is crucial -
 * bugs must be traceable. We will be happy to consider code for
 * inclusion in the official distribution, but derived work must not
 * be called official GROMACS. Details are found in the README & COPYING
 * files - if they are missing, get the official version at www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the papers on the package - you can find them in the top README file.
 *
 * For more info, check our website at http://www.gromacs.org
 *
 * And Hey:
 * Green Red Orange Magenta Azure Cyan Skyblue
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include "string2.h"
#include "smalloc.h"
#include "sysstuff.h"
#include "confio.h"
#include "statutil.h"
#include "pbc.h"
#include "force.h"
#include "gmx_fatal.h"
#include "futil.h"
#include "maths.h"
#include "macros.h"
#include "vec.h"
#include "tpxio.h"
#include "mdrun.h"
#include "main.h"
#include "random.h"
#include "index.h"
#include "mtop_util.h"
#include "gmx_ana.h"

static int greatest_common_divisor(int p, int q)
{
    int tmp;
    while (q != 0)
    {
        tmp = q;
        q = p % q;
        p = tmp;
    }
    return p;
}

static void insert_ion(int nsa, int *nwater,
                       gmx_bool bSet[], int repl[], atom_id index[],
                       rvec x[], t_pbc *pbc,
                       int sign, int q, const char *ionname,
                       t_atoms *atoms,
                       real rmin, int *seed)
{
    int             i, ei,nw;
    real            rmin2;
    rvec            dx;
    gmx_large_int_t maxrand;

    ei       = -1;
    nw       = *nwater;
    maxrand  = nw;
    maxrand *= 1000;

    do
    {
        ei = nw*rando(seed);
        maxrand--;
    }
    while (bSet[ei] && (maxrand > 0));
    if (bSet[ei])
    {
        gmx_fatal(FARGS, "No more replaceable solvent!");
    }

    fprintf(stderr, "Replacing solvent molecule %d (atom %d) with %s\n",
            ei, index[nsa*ei], ionname);

    /* Replace solvent molecule charges with ion charge */
    bSet[ei] = TRUE;
    repl[ei] = sign;

    atoms->atom[index[nsa*ei]].q = q;
    for (i = 1; i < nsa; i++)
    {
        atoms->atom[index[nsa*ei+i]].q = 0;
    }

    /* Mark all solvent molecules within rmin as unavailable for substitution */
    if (rmin > 0)
    {
        rmin2 = rmin*rmin;
        for (i = 0; (i < nw); i++)
        {
            if (!bSet[i])
            {
                pbc_dx(pbc, x[index[nsa*ei]], x[index[nsa*i]], dx);
                if (iprod(dx, dx) < rmin2)
                {
                    bSet[i] = TRUE;
                }
            }
        }
    }
}


static char *aname(const char *mname)
{
    char *str;
    int   i;

    str = strdup(mname);
    i   = strlen(str)-1;
    while (i > 1 && (isdigit(str[i]) || (str[i] == '+') || (str[i] == '-')))
    {
        str[i] = '\0';
        i--;
    }

    return str;
}

void sort_ions(int nsa, int nw, int repl[], atom_id index[],
               t_atoms *atoms, rvec x[],
               const char *p_name, const char *n_name)
{
    int    i, j, k, r, np, nn, starta, startr, npi, nni;
    rvec  *xt;
    char **pptr = NULL, **nptr = NULL, **paptr = NULL, **naptr = NULL;

    snew(xt, atoms->nr);

    /* Put all the solvent in front and count the added ions */
    np = 0;
    nn = 0;
    j  = index[0];
    for (i = 0; i < nw; i++)
    {
        r = repl[i];
        if (r == 0)
        {
            for (k = 0; k < nsa; k++)
            {
                copy_rvec(x[index[nsa*i+k]], xt[j++]);
            }
        }
        else if (r > 0)
        {
            np++;
        }
        else if (r < 0)
        {
            nn++;
        }
    }

    if (np+nn > 0)
    {
        /* Put the positive and negative ions at the end */
        starta = index[nsa*(nw - np - nn)];
        startr = atoms->atom[starta].resind;

        if (np)
        {
            snew(pptr, 1);
            pptr[0] = strdup(p_name);
            snew(paptr, 1);
            paptr[0] = aname(p_name);
        }
        if (nn)
        {
            snew(nptr, 1);
            nptr[0] = strdup(n_name);
            snew(naptr, 1);
            naptr[0] = aname(n_name);
        }
        npi = 0;
        nni = 0;
        for (i = 0; i < nw; i++)
        {
            r = repl[i];
            if (r > 0)
            {
                j = starta+npi;
                k = startr+npi;
                copy_rvec(x[index[nsa*i]], xt[j]);
                atoms->atomname[j]     = paptr;
                atoms->atom[j].resind  = k;
                atoms->resinfo[k].name = pptr;
                npi++;
            }
            else if (r < 0)
            {
                j = starta+np+nni;
                k = startr+np+nni;
                copy_rvec(x[index[nsa*i]], xt[j]);
                atoms->atomname[j]     = naptr;
                atoms->atom[j].resind  = k;
                atoms->resinfo[k].name = nptr;
                nni++;
            }
        }
        for (i = index[nsa*nw-1]+1; i < atoms->nr; i++)
        {
            j                  = i-(nsa-1)*(np+nn);
            atoms->atomname[j] = atoms->atomname[i];
            atoms->atom[j]     = atoms->atom[i];
            copy_rvec(x[i], xt[j]);
        }
        atoms->nr -= (nsa-1)*(np+nn);

        /* Copy the new positions back */
        for (i = index[0]; i < atoms->nr; i++)
        {
            copy_rvec(xt[i], x[i]);
        }
        sfree(xt);
    }
}

static void update_topol(const char *topinout, int p_num, int n_num,
                         const char *p_name, const char *n_name, char *grpname)
{
#define TEMP_FILENM "temp.top"
    FILE    *fpin, *fpout;
    char     buf[STRLEN], buf2[STRLEN], *temp, **mol_line = NULL;
    int      line, i, nsol, nmol_line, sol_line, nsol_last;
    gmx_bool bMolecules;

    printf("\nProcessing topology\n");
    fpin  = ffopen(topinout, "r");
    fpout = ffopen(TEMP_FILENM, "w");

    line       = 0;
    bMolecules = FALSE;
    nmol_line  = 0;
    sol_line   = -1;
    nsol_last  = -1;
    while (fgets(buf, STRLEN, fpin))
    {
        line++;
        strcpy(buf2, buf);
        if ((temp = strchr(buf2, '\n')) != NULL)
        {
            temp[0] = '\0';
        }
        ltrim(buf2);
        if (buf2[0] == '[')
        {
            buf2[0] = ' ';
            if ((temp = strchr(buf2, '\n')) != NULL)
            {
                temp[0] = '\0';
            }
            rtrim(buf2);
            if (buf2[strlen(buf2)-1] == ']')
            {
                buf2[strlen(buf2)-1] = '\0';
                ltrim(buf2);
                rtrim(buf2);
                bMolecules = (gmx_strcasecmp(buf2, "molecules") == 0);
            }
            fprintf(fpout, "%s", buf);
        }
        else if (!bMolecules)
        {
            fprintf(fpout, "%s", buf);
        }
        else
        {
            /* Check if this is a line with solvent molecules */
            sscanf(buf, "%s", buf2);
            if (gmx_strcasecmp(buf2, grpname) == 0)
            {
                sol_line = nmol_line;
                sscanf(buf, "%*s %d", &nsol_last);
            }
            /* Store this molecules section line */
            srenew(mol_line, nmol_line+1);
            mol_line[nmol_line] = strdup(buf);
            nmol_line++;
        }
    }
    ffclose(fpin);

    if (sol_line == -1)
    {
        ffclose(fpout);
        gmx_fatal(FARGS, "No line with moleculetype '%s' found the [ molecules ] section of file '%s'", grpname, topinout);
    }
    if (nsol_last < p_num+n_num)
    {
        ffclose(fpout);
        gmx_fatal(FARGS, "The last entry for moleculetype '%s' in the [ molecules ] section of file '%s' has less solvent molecules (%d) than were replaced (%d)", grpname, topinout, nsol_last, p_num+n_num);
    }

    /* Print all the molecule entries */
    for (i = 0; i < nmol_line; i++)
    {
        if (i != sol_line)
        {
            fprintf(fpout, "%s", mol_line[i]);
        }
        else
        {
            printf("Replacing %d solute molecules in topology file (%s) "
                   " by %d %s and %d %s ions.\n",
                   p_num+n_num, topinout, p_num, p_name, n_num, n_name);
            nsol_last -= p_num + n_num;
            if (nsol_last > 0)
            {
                fprintf(fpout, "%-10s  %d\n", grpname, nsol_last);
            }
            if (p_num > 0)
            {
                fprintf(fpout, "%-15s  %d\n", p_name, p_num);
            }
            if (n_num > 0)
            {
                fprintf(fpout, "%-15s  %d\n", n_name, n_num);
            }
        }
    }
    ffclose(fpout);
    /* use ffopen to generate backup of topinout */
    fpout = ffopen(topinout, "w");
    ffclose(fpout);
    rename(TEMP_FILENM, topinout);
#undef TEMP_FILENM
}

int gmx_genion(int argc, char *argv[])
{
    const char        *desc[] = {
        "[TT]genion[tt] randomly replaces solvent molecules with monoatomic ions.",
        "The group of solvent molecules should be continuous and all molecules",
        "should have the same number of atoms.",
        "The user should add the ion molecules to the topology file or use",
        "the [TT]-p[tt] option to automatically modify the topology.[PAR]",
        "The ion molecule type, residue and atom names in all force fields",
        "are the capitalized element names without sign. This molecule name",
        "should be given with [TT]-pname[tt] or [TT]-nname[tt], and the",
        "[TT][molecules][tt] section of your topology updated accordingly,",
        "either by hand or with [TT]-p[tt]. Do not use an atom name instead!",
        "[PAR]Ions which can have multiple charge states get the multiplicity",
        "added, without sign, for the uncommon states only.[PAR]",
        "For larger ions, e.g. sulfate we recommended using [TT]genbox[tt]."
    };
    const char        *bugs[] = {
        "If you specify a salt concentration existing ions are not taken into "
        "account. In effect you therefore specify the amount of salt to be added.",
    };
    static int         p_num   = 0, n_num = 0, p_q = 1, n_q = -1;
    static const char *p_name  = "NA", *n_name = "CL";
    static real        rmin    = 0.6, conc = 0;
    static int         seed    = 1993;
    static gmx_bool    bNeutral = FALSE;
    static t_pargs     pa[]    = {
        { "-np",    FALSE, etINT,  {&p_num}, "Number of positive ions"       },
        { "-pname", FALSE, etSTR,  {&p_name}, "Name of the positive ion"      },
        { "-pq",    FALSE, etINT,  {&p_q},   "Charge of the positive ion"    },
        { "-nn",    FALSE, etINT,  {&n_num}, "Number of negative ions"       },
        { "-nname", FALSE, etSTR,  {&n_name}, "Name of the negative ion"      },
        { "-nq",    FALSE, etINT,  {&n_q},   "Charge of the negative ion"    },
        { "-rmin",  FALSE, etREAL, {&rmin},  "Minimum distance between ions" },
        { "-seed",  FALSE, etINT,  {&seed},  "Seed for random number generator" },
        { "-conc",  FALSE, etREAL, {&conc},
          "Specify salt concentration (mol/liter). This will add sufficient ions to reach up to the specified concentration as computed from the volume of the cell in the input [TT].tpr[tt] file. Overrides the [TT]-np[tt] and [TT]-nn[tt] options." },
        { "-neutral", FALSE, etBOOL, {&bNeutral}, "This option will add enough ions to neutralize the system. These ions are added on top of those specified with [TT]-np[tt]/[TT]-nn[tt] or [TT]-conc[tt]. "}
    };
    t_topology        top;
    rvec              *x, *v;
    real               vol, qtot;
    matrix             box;
    t_atoms            atoms;
    t_pbc              pbc;
    int               *repl, ePBC;
    atom_id           *index;
    char              *grpname, title[STRLEN];
    gmx_bool          *bSet;
    int                i, nw, nwa, nsa, nsalt, iqtot;
    output_env_t       oenv;
    t_filenm           fnm[] = {
        { efTPX, NULL,  NULL,      ffREAD  },
        { efNDX, NULL,  NULL,      ffOPTRD },
        { efSTO, "-o",  NULL,      ffWRITE },
        { efTOP, "-p",  "topol",   ffOPTRW }
    };
#define NFILE asize(fnm)

    parse_common_args(&argc, argv, PCA_BE_NICE, NFILE, fnm, asize(pa), pa,
                      asize(desc), desc, asize(bugs), bugs, &oenv);

    /* Check input for something sensible */
    if ((p_num < 0) || (n_num < 0))
    {
        gmx_fatal(FARGS, "Negative number of ions to add?");
    }

    if (conc > 0 && (p_num > 0 || n_num > 0))
    {
        fprintf(stderr, "WARNING: -conc specified, overriding -nn and -np.\n");
    }

    /* Read atom positions and charges */
    read_tps_conf(ftp2fn(efTPX, NFILE, fnm), title, &top, &ePBC, &x, &v, box, FALSE);
    atoms = top.atoms;

    /* Compute total charge */
    qtot = 0;
    for (i = 0; (i < atoms.nr); i++)
    {
        qtot += atoms.atom[i].q;
    }
    iqtot = gmx_nint(qtot);

    
    if (conc > 0)
    {
        /* Compute number of ions to be added */
        vol = det(box);
        nsalt = gmx_nint(conc*vol*AVOGADRO/1e24);
        p_num = abs(nsalt*n_q);
        n_num = abs(nsalt*p_q);
    }
    if (bNeutral)
    {
        int qdelta = p_num*p_q + n_num*n_q + iqtot;

        /* Check if the system is neutralizable
         * is (qdelta == p_q*p_num + n_q*n_num) solvable for p_num and n_num? */
        int gcd = greatest_common_divisor(n_q, p_q);
        if ((qdelta % gcd) != 0)
        {
            gmx_fatal(FARGS, "Can't neutralize this system using -nq %d and"
                    " -pq %d.\n", n_q, p_q);
        }
        
        while (qdelta != 0)
        {
            while (qdelta < 0)
            {
                p_num++;
                qdelta += p_q;
            }
            while (qdelta > 0)
            {
                n_num++;
                qdelta += n_q;
            }
        }
    }

    if ((p_num == 0) && (n_num == 0))
    {
        fprintf(stderr, "No ions to add.\n");
        exit(0);
    }
    else
    {
        printf("Will try to add %d %s ions and %d %s ions.\n",
               p_num, p_name, n_num, n_name);
        printf("Select a continuous group of solvent molecules\n");
        get_index(&atoms, ftp2fn_null(efNDX, NFILE, fnm), 1, &nwa, &index, &grpname);
        for (i = 1; i < nwa; i++)
        {
            if (index[i] != index[i-1]+1)
            {
                gmx_fatal(FARGS, "The solvent group %s is not continuous: "
                          "index[%d]=%d, index[%d]=%d",
                          grpname, i, index[i-1]+1, i+1, index[i]+1);
            }
        }
        nsa = 1;
        while ((nsa < nwa) &&
               (atoms.atom[index[nsa]].resind ==
                atoms.atom[index[nsa-1]].resind))
        {
            nsa++;
        }
        if (nwa % nsa)
        {
            gmx_fatal(FARGS, "Your solvent group size (%d) is not a multiple of %d",
                      nwa, nsa);
        }
        nw = nwa/nsa;
        fprintf(stderr, "Number of (%d-atomic) solvent molecules: %d\n", nsa, nw);
        if (p_num+n_num > nw)
        {
            gmx_fatal(FARGS, "Not enough solvent for adding ions");
        }
    }

    if (opt2bSet("-p", NFILE, fnm))
    {
        update_topol(opt2fn("-p", NFILE, fnm), p_num, n_num, p_name, n_name, grpname);
    }

    snew(bSet, nw);
    snew(repl, nw);

    snew(v, atoms.nr);
    snew(atoms.pdbinfo, atoms.nr);

    set_pbc(&pbc, ePBC, box);

    /* Now loop over the ions that have to be placed */
    while (p_num-- > 0)
    {
        insert_ion(nsa, &nw, bSet, repl, index, x, &pbc,
                   1, p_q, p_name, &atoms, rmin, &seed);
    }
    while (n_num-- > 0)
    {
        insert_ion(nsa, &nw, bSet, repl, index, x, &pbc,
                   -1, n_q, n_name, &atoms, rmin, &seed);
    }
    fprintf(stderr, "\n");

    if (nw)
    {
        sort_ions(nsa, nw, repl, index, &atoms, x, p_name, n_name);
    }

    sfree(atoms.pdbinfo);
    atoms.pdbinfo = NULL;
    write_sto_conf(ftp2fn(efSTO, NFILE, fnm), *top.name, &atoms, x, NULL, ePBC,
                   box);

    return 0;
}
