//++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Port of Linux getopt() function; command line parsing
// 
// Code from https://stackoverflow.com/questions/10404448/getopt-h-compiling-linux-c-code-in-windows
//
//++++++++++++++++++++++++++++++++++++++++++++++++++


#include "Common.h"

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

// Change from original code: make these static, visible only to this file
static int  opterr = 1;     /* if error message should be printed */
static int  optind = 1;     /* index into parent argv vector */
static int  optreset;       /* reset getopt */
static char *optarg;        /* argument associated with option */

/*
* getopt --
*      Parse argc/argv argument vector.
*/
int getopt(int nargc, char * const nargv[], const char *ostr)
{
int  optopt;         /* character checked for validity */   // CHANGE: moved inside function
static char e[] = EMSG;
static char *place = e;              /* option letter processing */
const char *oli;                        /* option letter list index */

  if (optreset || !*place) {              /* update scanning pointer */
    optreset = 0;
    if (optind >= nargc || *(place = nargv[optind]) != '-') {
      place = e;
      return (-1);
    }
    if (place[1] && *++place == '-') {      /* found "--" */
      ++optind;
      place = e;
      return (-1);
    }
  }                                       /* option letter okay? */
  if ((optopt = (int)*place++) == (int)':' ||
    !(oli = strchr(ostr, optopt))) {
      /*
      * if the user didn't specify '-' as an option,
      * assume it means -1.
      */
      if (optopt == (int)'-')
        return (-1);
      if (!*place)
        ++optind;
      if (opterr && *ostr != ':')
        (void)printf("illegal option -- %c\n", optopt);
      return (BADCH);
  }
  if (*++oli != ':') {                    /* don't need argument */
    optarg = NULL;
    if (!*place)
      ++optind;
  }
  else {                                  /* need an argument */
    if (*place)                     /* no white space */
      optarg = place;
    else if (nargc <= ++optind) {   /* no arg */
      place = e;
      if (*ostr == ':')
        return (BADARG);
      if (opterr)
        (void)printf("option requires an argument -- %c\n", optopt);
      return (BADCH);
    }
    else                            /* white space */
      optarg = nargv[optind];
    place = e;
    ++optind;
  }
  return (optopt);                        /* dump back option letter */
} // getopt


// Change to oiginal code: Helper function to keep optarg a local static, return its value
char* get_optarg()
{
    return optarg;
}