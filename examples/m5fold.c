/***************************************************************************
 *   Copyright (C) 2010 by Walter Brisken                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: m5spec.c 1989 2010-02-26 17:37:16Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/libraries/mark5access/trunk/mark5access/mark5_stream.c $
// $LastChangedRevision: 1989 $
// $Author: WalterBrisken $
// $LastChangedDate: 2010-02-26 10:37:16 -0700 (Fri, 26 Feb 2010) $
//
//============================================================================

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../mark5access/mark5_stream.h"

const char program[] = "m5fold";
const char author[]  = "Walter Brisken";
const char version[] = "1.0";
const char verdate[] = "2010 Jul 13";

const int ChunkSize = 10000;

int usage(const char *pgm)
{
	printf("\n");

	printf("%s ver. %s   %s  %s\n\n", program, version, author, verdate);
	printf("A Mark5 power folder.  Can use VLBA, Mark3/4, and Mark5B "
		"formats using the\nmark5access library.\n\n");
	printf("Usage: %s <infile> <dataformat> <nbin> <nint> <freq> <outfile> [<offset>]\n\n", program);
	printf("  <infile> is the name of the input file\n\n");
	printf("  <dataformat> should be of the form: "
		"<FORMAT>-<Mbps>-<nchan>-<nbit>, e.g.:\n");
	printf("    VLBA1_2-256-8-2\n");
	printf("    MKIV1_4-128-2-1\n");
	printf("    Mark5B-512-16-2\n\n");
	printf("  <nbin> is the number of bins per if across 1 period\n");
	printf("         if negative, the conversion to true power is not performed\n\n");
	printf("  <nint> is the number of %d sample chunks to work on\n\n", ChunkSize);
	printf("  <freq> [Hz] -- the inverse of the period to be folded\n\n");
	printf("  <outfile> is the name of the output file\n\n");
	printf("  <offset> is number of bytes into file to start decoding\n\n");
	printf("Example: look for the 80 Hz switched power:\n\n");
	printf("  m5fold 2bit.data.vlba VLBA1_1-128-8-2 128 10000 80 switched_power.out\n\n");
	printf("Output: A file witn <nchan>+1 columns.  First column is time [s].\n");
	printf("  Each remaining column is folded power for that baseband channel.\n");
	printf("  If nbin is positive, the scaling is such that <v^2> = 1 yields a\n");
	printf("  power reading of 1.0.  Optimal S/N occurs for power ~= 1.03\n\n");
	printf("Note: This program is useless on 1-bit quantized data\n\n");
	return 0;
}

/* returns theoretical <v^2> for 2 bit samples for a given power level, p */
double powerfunc(double p)
{
	double n1=0.0;

	/* calculate n1 -- the fraction of samples in the "low" state */
	if(p <= 0.0)
	{
		n1 = 1.0;
	}
	else
	{
		n1 = erf(M_SQRT1_2/p);
	}

	return n1 + (1.0-n1)*OPTIMAL_2BIT_HIGH*OPTIMAL_2BIT_HIGH;
}

void plotpower()
{
	FILE *out;
	double p, q;

	out = fopen("powerfunc.txt", "w");

	for(p = 0; p < 5.0; p += 0.01)
	{
		q = powerfunc(p);
		fprintf(out, "%f %f\n", p, q); 
	}

	fclose(out);
}

#if 0
/* series expansion from wikipedia */
double inverseerfbyseries(double x)
{
	return (1.0/M_2_SQRTPI)*
	(
		x +
		(M_PI/12.0)*x*x*x +
		(7.0*M_PI*M_PI/480.0)*x*x*x*x*x +
		(127.0*M_PI*M_PI*M_PI/40320.0)*x*x*x*x*x*x*x +
		(4369.0*M_PI*M_PI*M_PI*M_PI/5806080.0)*x*x*x*x*x*x*x*x*x +
		(34807.0*M_PI*M_PI*M_PI*M_PI*M_PI/182476800.0)*x*x*x*x*x*x*x*x*x*x*x  /* + ... */
	);
}
#endif

/* Approximation by Sergei Winitzki: 
 * http://homepages.physik.uni-muenchen.de/~Winitzki/erf-approx.pdf*/
double inverseerf(double x)
{
	const double a = 8.0/(3.0*M_PI) * (M_PI-3.0)/(4.0-M_PI);
	double b, c;

	if(x == 0.0)
	{
		return 0.0;
	}
	else if(x < 0.0)
	{
		return -inverseerf(-x);
	}
	else
	{
		c = log(1.0-x*x);
		b = 2.0/(M_PI*a) + 0.5*c;

		return sqrt(-b + sqrt(b*b - c/a) );
	}
}

double correctpower(double x)
{
	const double a = OPTIMAL_2BIT_HIGH*OPTIMAL_2BIT_HIGH;

	if(x <= 0.0)
	{
		return -1.0;
	}

	return 1.0/(M_SQRT2*inverseerf( (x-a)/(1.0-a) ) );
}

/*
void testierf()
{
	double x, y, z;

	for(x = 0.0; x < 10.0; x += 0.2)
	{
		y = erf(x);
		z = inverseerf(y);
		printf("%f %f %f\n", x, y, z);
	}
}
*/

int fold(const char *filename, const char *formatname, int nbin, int nint,
	double freq, const char *outfile, long long offset)
{
	struct mark5_stream *ms;
	double **data, **bins;
	int **weight;
	int c, i, j, k, status;
	int nif, bin;
	long long total, unpacked;
	FILE *out;
	double R;
	long long sampnum;
	int docorrection = 1;

	if(nbin < 0)
	{
		nbin = -nbin;
		docorrection = 0;
	}

	total = unpacked = 0;

	ms = new_mark5_stream(
		new_mark5_stream_file(filename, offset),
		new_mark5_format_generic_from_string(formatname) );

	if(!ms)
	{
		printf("problem opening %s\n", filename);
		return 0;
	}

	if(ms->nbit < 2)
	{
		fprintf(stderr, "Warning: 1-bit data supplied.  Results will be\n");
		fprintf(stderr, "useless.  Proceeding anyway!\n\n");
	}

	if(ms->nbit > 2)
	{
		fprintf(stderr, "More than 2 bits: power not being corrected!\n");
		docorrection = 0;
	}

	mark5_stream_print(ms);

	sampnum = (int)((double)ms->ns*(double)ms->samprate*1.0e-9 + 0.5);

	out = fopen(outfile, "w");
	if(!out)
	{
		fprintf(stderr, "Error -- cannot open %s for write\n", outfile);
		delete_mark5_stream(ms);

		return 0;
	}

	R = nbin*freq/ms->samprate;

	nif = ms->nchan;

	data = (double **)malloc(nif*sizeof(double *));
	bins = (double **)malloc(nif*sizeof(double *));
	weight = (int **)malloc(nif*sizeof(double *));
	for(i = 0; i < nif; i++)
	{
		data[i] = (double *)malloc(ChunkSize*sizeof(double));
		bins[i] = (double *)calloc(nbin, sizeof(double));
		weight[i] = (int *)calloc(nbin, sizeof(int));
	}

	for(j = 0; j < nint; j++)
	{
		status = mark5_stream_decode_double(ms, ChunkSize, data);
		
		if(status < 0)
		{
			break;
		}
		else
		{
			total += ChunkSize;
			unpacked += status;
		}

		if(ms->consecutivefails > 5)
		{
			printf("Too many failures.  consecutive, total fails = %d %d\n", ms->consecutivefails, ms->nvalidatefail);
			break;
		}

		for(k = 0; k < ChunkSize; k++)
		{
			if(data[0][k] != 0.0)
			{
				bin = (int)(sampnum*R) % nbin;
				for(i = 0; i < nif; i++)
				{
					bins[i][bin] += data[i][k]*data[i][k];
					weight[i][bin]++;
				}
			}
			sampnum++;
		}
	}

	fprintf(stderr, "%Ld / %Ld samples unpacked\n", unpacked, total);

	/* normalize */
	for(k = 0; k < nbin; k++)
	{
		for(i = 0; i < nif; i++)
		{
			if(weight[i][k]) 
			{
				bins[i][k] /= weight[i][k];
			}
		}
	}

	/* convert the mean quantized voltage squared to nominal power */
	if(docorrection)
	{
		for(k = 0; k < nbin; k++)
		{
			for(i = 0; i < nif; i++)
			{
				bins[i][k] = correctpower(bins[i][k]);
			}
		}
	}

	for(c = 0; c < nbin; c++)
	{
		fprintf(out, "%11.9f ", c/(freq*nbin));
		for(i = 0; i < nif; i++)
		{
			fprintf(out, " %f", bins[i][c]);
		}
		fprintf(out, "\n");
	}

	fclose(out);

	for(i = 0; i < nif; i++)
	{
		free(data[i]);
		free(bins[i]);
		free(weight[i]);
	}
	free(data);
	free(bins);
	free(weight);

	delete_mark5_stream(ms);

	return 0;
}

int main(int argc, char **argv)
{
	long long offset = 0;
	int nbin, nint;
	double freq;

	if(argc < 6)
	{
		return usage(argv[0]);
	}

	nbin = atol(argv[3]);
	nint = atol(argv[4]);
	freq = atof(argv[5]);

	/* if supplied nint is non-sensical, assume whole file */
	if(nint <= 0)
	{
		nint = 2000000000L;
	}

	if(argc > 7)
	{
		offset=atoll(argv[7]);
	}

	fold(argv[1], argv[2], nbin, nint, freq, argv[6], offset);

	return 0;
}
