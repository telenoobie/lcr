/*****************************************************************************\
**                                                                           **
** PBX4Linux                                                                 **
**                                                                           **
**---------------------------------------------------------------------------**
** Copyright: Andreas Eversberg                                              **
**                                                                           **
** options header file                                                       **
**                                                                           **
\*****************************************************************************/ 

struct options {
	char	log[128];		/* location of log file */
	int	deb;			/* debugging */
	char	law;			/* 'a' or 'u' law */

	char	national[10];		/* prefix for national calls */
	char	international[10];	/* prefix for international calls */

	char	tones_dir[64];		/* directory of all tones/patterns */
	char	fetch_tones[256];	/* directories of tones to fetch */
	char	extensions_dir[64];	/* directory of extensions */
	char	dummyid[32];		/* caller id for external calls if not available */
	int	dsptones;		/* tones will be generated via dsp.o 1=american 2=ger */
	int	schedule;		/* run process in realtime @ given priority */
	char	email[128];		/* source email address */
};	

extern struct options options;

extern char options_error[256];

int read_options(void);


