/*
 * $Id$
 * 
 *       This source code is part of
 * 
 *        G   R   O   M   A   C   S
 * 
 * GROningen MAchine for Chemical Simulations
 * 
 *               VERSION 2.0
 * 
 * Copyright (c) 1991-1999
 * BIOSON Research Institute, Dept. of Biophysical Chemistry
 * University of Groningen, The Netherlands
 * 
 * Please refer to:
 * GROMACS: A message-passing parallel molecular dynamics implementation
 * H.J.C. Berendsen, D. van der Spoel and R. van Drunen
 * Comp. Phys. Comm. 91, 43-56 (1995)
 * 
 * Also check out our WWW page:
 * http://md.chem.rug.nl/~gmx
 * or e-mail to:
 * gromacs@chem.rug.nl
 * 
 * And Hey:
 * GRowing Old MAkes el Chrono Sweat
 */
static char *SRCID_g_analyze_c = "$Id$";

#include <math.h>
#include <string.h>
#include "statutil.h"
#include "sysstuff.h"
#include "typedefs.h"
#include "smalloc.h"
#include "macros.h"
#include "fatal.h"
#include "vec.h"
#include "copyrite.h"
#include "futil.h"
#include "statutil.h"
#include "txtdump.h"
#include "gstat.h"
#include "xvgr.h"

static real **read_val(char *fn,bool bHaveT,bool bTB,real tb,bool bTE,real te,
		       int nsets_in,int *nset,int *nval,real *t0,real *dt,
		       int linelen)
{
  FILE   *fp;
  static int  llmax=0;
  static char *line0=NULL;
  char   *line;
  int    a,narg,n,sin,set,nchar;
  double dbl,tend=0;
  bool   bEndOfSet,bTimeInRange;
  real   **val;

  if (linelen > llmax) {
    llmax = linelen;
    srenew(line0,llmax);
  }

  val = NULL;
  *t0 = 0;
  fp  = ffopen(fn,"r");
  for(sin=0; sin<nsets_in; sin++) {
    if (nsets_in == 1)
      narg = 0;
    else 
      narg = bHaveT ? 2 : 1;
    n = 0;
    bEndOfSet = FALSE;
    while (!bEndOfSet && fgets(line0,linelen,fp)) {
      line = line0;
      bEndOfSet = (line[0] == '&');
      if ((line[0] != '#') && (line[0] != '@') && !bEndOfSet) {
	a = 0;
	bTimeInRange = TRUE;
	while ((a<narg || (nsets_in==1 && n==0)) && 
	       line[0]!='\n' && sscanf(line,"%lf%n",&dbl,&nchar)
	       && bTimeInRange) {
	  /* Use set=-1 as the time "set" */
	  if (sin) {
	    if (!bHaveT || (a>0))
	      set = sin;
	    else
	      set = -1;
	  } else {
	    if (!bHaveT)
	      set = a;
	    else
	      set = a-1;
	  }
	  if (set==-1 && ((bTB && dbl<tb) || (bTE && dbl>te)))
	    bTimeInRange = FALSE;
	    
	  if (bTimeInRange) {
	    if (n==0) {
	      if (nsets_in == 1)
		narg++;
	      if (set == -1)
		*t0 = dbl;
	      else {
		*nset = set+1;
		srenew(val,*nset);
		val[set] = NULL;
	      }
	    }
	    if (set == -1)
	      tend = dbl;
	    else {
	      if (n % 100 == 0)
	      srenew(val[set],n+100);
	      val[set][n] = (real)dbl;
	    }
	  }
	  a++;
	  line += nchar;
	}
	if (bTimeInRange) {
	  n++;
	  if (a != narg)
	    fprintf(stderr,"Invalid line in %s: '%s'\n",fn,line0);
	}
      }
    }
    if (sin==0) {
      *nval = n;
      if (!bHaveT)
	*dt = 1.0;
      else
	*dt = (real)(tend-*t0)/(n-1.0);
    } else {
      if (n < *nval) {
	fprintf(stderr,"Set %d is shorter (%d) than the previous set (%d)\n",
		sin+1,n,*nval);
	*nval = n;
	fprintf(stderr,"Will use only the first %d points of every set\n",
		*nval);
      }
    }
  }
  fclose(fp);
  
  return val;
}

void histogram(char *distfile,real binwidth,int n, int nset, real **val)
{
  FILE *fp;
  int  i,s;
  real min,max;
  int  nbin;
  real *histo;

  min=val[0][0];
  max=val[0][0];
  for(s=0; s<nset; s++)
    for(i=0; i<n; i++)
      if (val[s][i] < min)
	min = val[s][i];
      else if (val[s][i] > max)
	max = val[s][i];
  
  if (-min > max)
    max = -min;
  nbin = (int)(max/binwidth)+1;
  fprintf(stderr,"Making distributions with %d bins\n",2*nbin+1);
  snew(histo,2*nbin+1);
  fp = xvgropen(distfile,"Distribution","","");
  for(s=0; s<nset; s++) {
    for(i=0; i<2*nbin+1; i++)
      histo[i] = 0;
    for(i=0; i<n; i++)
      histo[nbin+(int)(floor(val[s][i]/binwidth+0.5))]++;
    for(i=0; i<2*nbin+1; i++)
      fprintf(fp," %g  %g\n",(i-nbin)*binwidth,(real)histo[i]/(n*binwidth));
    if (s<nset-1)
      fprintf(fp,"&\n");
  }
  fclose(fp);
}

static int real_comp(const void *a,const void *b)
{
  real dif = *(real *)a - *(real *)b;

  if (dif < 0)
    return -1;
  else if (dif > 0)
    return 1;
  else
    return 0;
}

static void average(char *avfile,char **avbar_opt,
		    int n, int nset,real **val,real t0,real dt)
{
  FILE   *fp;
  int    i,s,edge=0;
  double av,var,err;
  real   *tmp=NULL;
  char   c;
  
  c = avbar_opt[0][0];

  fp = ffopen(avfile,"w");
  if ((c == 'e') && (nset == 1))
    c = 'n';
  if (c != 'n') {
    if (c == '9') {
      snew(tmp,nset);
      fprintf(fp,"@TYPE xydydy\n");
      edge = (int)(nset*0.05+0.5);
      fprintf(stdout,"Errorbars: discarding %d points on both sides: %d%%"
	      " interval\n",edge,(int)(100*(nset-2*edge)/nset+0.5));
    } else
      fprintf(fp,"@TYPE xydy\n");
  }
  
  for(i=0; i<n; i++) {
    av = 0;
    for(s=0; s<nset; s++)
      av += val[s][i];
    av /= nset;
    fprintf(fp," %g %g",t0+dt*i,av);
    var = 0;
    if (c != 'n') {
      if (c == '9') {
	for(s=0; s<nset; s++)
	  tmp[s] = val[s][i];
	qsort(tmp,nset,sizeof(tmp[0]),real_comp);
	fprintf(fp," %g %g",tmp[nset-1-edge]-av,av-tmp[edge]);
      } else {
	for(s=0; s<nset; s++)
	  var += sqr(val[s][i]-av);
	if (c == 's')
	  err = sqrt(var/nset);
	else
	  err = sqrt(var/(nset*(nset-1)));
	fprintf(fp," %g",err);
      }
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
  
  if (c == '9')
    sfree(tmp);
}

static void estimate_error(char *eefile,int resol,int n,int nset,
			   double *av,real **val,real dt)
{
  FILE *fp;
  int log2max,rlog2,bs,prev_bs,nb;
  int s,i,j;
  double blav,var;
  char **leg;

  log2max = (int)(log(n)/log(2));

  snew(leg,nset);
  for(s=0; s<nset; s++) {
    snew(leg[s],STRLEN);
    sprintf(leg[s],"av %f",av[s]);
  }

  fp = xvgropen(eefile,"Error estimates","Block size (time)","Error estimate");
  fprintf(fp,
	  "@ subtitle \"using block averaging, total time %g (%d points)\"\n",
	  n*dt,n);
  xvgr_legend(fp,nset,leg);
  for(s=0; s<nset; s++)
    sfree(leg[s]);
  sfree(leg);

  for(s=0; s<nset; s++) {
    prev_bs = 0;
    for(rlog2=resol*log2max; rlog2>=2*resol; rlog2--) {
      bs = n*pow(0.5,(real)rlog2/(real)resol);
      if (bs != prev_bs) {
	nb = 0;
	i = 0;
	var = 0;
	while (i+bs <= n) {
	  blav=0;
	  for (j=0; j<bs; j++) {
	    blav += val[s][i];
	  i++;
	  }
	  var += sqr(av[s] - blav/bs);
	  nb++;
	}
	fprintf(fp," %g %g\n",bs*dt,sqrt(var/(nb*(nb-1.0))));
      }
      prev_bs = bs;
    }
    if (s < nset)
      fprintf(fp,"&\n");
  }

  fclose(fp);
}

int main(int argc,char *argv[])
{
  static char *desc[] = {
    "g_analyze reads an ascii file and analyzes data sets.",
    "A line in the input file may start with a time",
    "(see option [TT]-time[tt]) and any number of y values may follow.",
    "Multiple sets can also be",
    "read when they are seperated by & (option [TT]-n[tt]),",
    "in this case only one y value is read from each line.",
    "All lines starting with # and @ are skipped.",
    "All analyses can also be done for the derivative of a set",
    "(option [TT]-d[tt]).[PAR]",
    "g_analyze always shows the average and standard deviation of each",
    "set. For each set it also shows the relative deviation of the third",
    "and forth cumulant from those of a Gaussian distribution with the same",
    "standard deviation.[PAR]",
    "Option [TT]-ac[tt] produces the autocorrelation function(s).[PAR]",
    "Option [TT]-msd[tt] produces the mean square displacement(s).[PAR]",
    "Option [TT]-dist[tt] produces distribution plot(s).[PAR]",
    "Option [TT]-av[tt] produces the average over the sets.",
    "Error bars can be added with the option [TT]-errbar[tt].",
    "The errorbars can represent the standard deviation, the error",
    "(assuming the points are independent) or the interval containing",
    "90% of the points, by discarding 5% of the points at the top and",
    "the bottom.[PAR]",
    "Option [TT]-ee[tt] produces error estimates using block averaging.",
    "A set is divided in a number of blocks and averages are calculated for",
    "each block. The error for the total average is calculated from",
    "the variance between averages of the m blocks B_i as follows:",
    "error^2 = Sum (B_i - <B>)^2 / (m*(m-1)).",
    "These errors are plotted as a function of the block size.",
    "For a good error estimate the block size should be at least as large",
    "as the correlation time, but possibly much larger.[PAR]"
  };
  static real tb=-1,te=-1,frac=0.5,binwidth=0.1;
  static bool bHaveT=TRUE,bDer=FALSE,bSubAv=FALSE,bAverCorr=FALSE;
  static int  linelen=4096,nsets_in=1,d=1,resol=8;

  static char *avbar_opt[] = { NULL, "none", "stddev", "error", "90", NULL };

  t_pargs pa[] = {
    { "-linelen", FALSE, etINT, {&linelen},
      "HIDDENMaximum input line length" },
    { "-time", FALSE, etBOOL, {&bHaveT},
      "Expect a time in the input" },
    { "-b", FALSE, etREAL, {&tb},
      "First time to read from set" },
    { "-e", FALSE, etREAL, {&te},
      "Last time to read from set" },
    { "-n", FALSE, etINT, {&nsets_in},
      "Read # sets seperated by &" },
    { "-d", FALSE, etBOOL, {&bDer},
	"Use the derivative" },
    { "-dp",  FALSE, etINT, {&d}, 
      "HIDDENThe derivative is the difference over # points" },
    { "-bw", FALSE, etREAL, {&binwidth},
      "Binwidth for the distribution" },
    { "-errbar", FALSE, etENUM, {&avbar_opt},
      "Error bars for -av" },
    { "-resol", FALSE, etINT, {&resol},
      "HIDDENResolution for the block averaging, block size increases with"
    " a factor 2^(1/#)" },
    { "-subav", FALSE, etBOOL, {&bSubAv},
      "Subtract the average before autocorrelating" },
    { "-oneacf", FALSE, etBOOL, {&bAverCorr},
      "Calculate one ACF over all sets" }
  };
#define NPA asize(pa)

  FILE     *out;
  int      n,nlast,s,nset,i,t=0;
  real     **val,t0,dt,tot;
  double   *av,*sig,cum1,cum2,cum3,cum4,db;
  char     *acfile,*msdfile,*distfile,*avfile,*eefile;
  
  t_filenm fnm[] = { 
    { efXVG, "-f",    "graph",    ffREAD   },
    { efXVG, "-ac",   "autocorr", ffOPTWR  },
    { efXVG, "-msd",  "msd",      ffOPTWR  },
    { efXVG, "-dist", "distr",    ffOPTWR  },
    { efXVG, "-av",   "average",  ffOPTWR  },
    { efXVG, "-ee",   "errest",   ffOPTWR  }
  }; 
#define NFILE asize(fnm) 

  int     npargs;
  t_pargs *ppa;

  npargs = asize(pa); 
  ppa    = add_acf_pargs(&npargs,pa);
  
  CopyRight(stderr,argv[0]); 
  parse_common_args(&argc,argv,PCA_CAN_VIEW,TRUE,
		    NFILE,fnm,npargs,ppa,asize(desc),desc,0,NULL); 

  acfile   = opt2fn_null("-ac",NFILE,fnm);
  msdfile  = opt2fn_null("-msd",NFILE,fnm);
  distfile = opt2fn_null("-dist",NFILE,fnm);
  avfile   = opt2fn_null("-av",NFILE,fnm);
  eefile   = opt2fn_null("-ee",NFILE,fnm);

  val=read_val(opt2fn("-f",NFILE,fnm),bHaveT,
	       opt2parg_bSet("-b",npargs,ppa),tb,
	       opt2parg_bSet("-e",npargs,ppa),te,
	       nsets_in,&nset,&n,&t0,&dt,linelen);
  fprintf(stdout,"Read %d sets of %d points, dt = %g\n\n",nset,n,dt);
  if (bDer) {
    fprintf(stdout,"Calculating the derivative as (f[i+%d]-f[i])/(%d*dt)\n\n",
	    d,d);
    n -= d;
    for(s=0; s<nset; s++)
      for(i=0; i<n; i++)
	val[s][i] = (val[s][i+d]-val[s][i])/(d*dt);
  }

  fprintf(stdout,"                                         relative deviation of\n");
  fprintf(stdout,"                           standard     cumulants from those of\n");
  fprintf(stdout,"             average       deviation    a Gaussian distribition\n");
  fprintf(stdout,"                                            cum. 3   cum. 4\n");
  snew(av,nset);
  snew(sig,nset);
  for(s=0; s<nset; s++) {
    cum1 = 0;
    cum2 = 0;
    cum3 = 0;
    cum4 = 0;
    for(i=0; i<n; i++)
      cum1 += val[s][i];
    cum1 /= n;
    for(i=0; i<n; i++) {
      db = val[s][i]-cum1;
      cum2 += db*db;
      cum3 += db*db*db;
      cum4 += db*db*db*db;
    }
    cum2 /= n;
    cum3 /= n;
    cum4 /= n;
    av[s]  = cum1;
    sig[s] = sqrt(cum2);
    /* fprintf(stdout,"Average of set %2d: %13.6e  stddev: %12.6e\n",
	    s+1,av[s],sig); */
    fprintf(stdout,"Set %3d:  %13.6e   %12.6e      %6.3f   %6.3f\n",
	    s+1,av[s],sig[s],
	    sig[s] ? cum3/(sig[s]*sig[s]*sig[s]*sqrt(8/M_PI)) : 0,
	    sig[s] ? cum4/(sig[s]*sig[s]*sig[s]*sig[s]*3)-1 : 0); 
  }
  fprintf(stdout,"\n");

  if (msdfile) {
    out=xvgropen(msdfile,"Mean square displacement",
		 "time (ps)","MSD (nm\\S2\\N)");
    nlast = (int)(n*frac);
    for(s=0; s<nset; s++) {
      for(t=0; t<=nlast; t++) {
	if (t % 100 == 0)
	  fprintf(stderr,"\r%d",t);
	tot=0;
	for(i=0; i<n-t; i++)
	  tot += sqr(val[s][i]-val[s][i+t]); 
	tot /= (real)(n-t);
	fprintf(out," %g %8g\n",dt*t,tot);
      }
      if (s<nset-1)
	fprintf(out,"&\n");
    }
    fclose(out);
    fprintf(stderr,"\r%d, time=%g\n",t-1,(t-1)*dt);
    do_view(msdfile, NULL);
  }
  
  if (distfile) {
    histogram(distfile,binwidth,n,nset,val);
    do_view(distfile, NULL);
  }
  if (avfile) {
    average(avfile,avbar_opt,n,nset,val,t0,dt);
    do_view(avfile, NULL);
  }
  if (eefile) {
    estimate_error(eefile,resol,n,nset,av,val,dt);
    do_view(eefile, NULL);
  }
  if (acfile) {
    if (bSubAv) 
      for(s=0; s<nset; s++)
	for(i=0; i<n; i++)
	  val[s][i] -= av[s];
    do_autocorr(acfile,"Autocorrelation",n,nset,val,dt,
		eacNormal,bAverCorr);
    do_view(acfile, NULL);
  }

  thanx(stderr);

  return 0;
}
  
