#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/Date.h>
#include <lal/LIGOLwXML.h>
#include <lal/LIGOLwXMLRead.h>
#include <lal/LIGOMetadataUtils.h>
#include <lal/BurstSearch.h>
#include <lalapps.h>

RCSID("$Id$");

#define MAXSTR 2048
#define PLAYGROUND_START_TIME 729273613
#define PLAYGROUND_INTERVAL 6370
#define PLAYGROUND_LENGTH 600

/* Usage format string. */
#define USAGE "Usage: %s --input infile --outinjfile filename \
    --injfile injectionfile --outsnglfile filename [--noplayground] [--help]\n"

#define BINJ_FIND_EARG   1
#define BINJ_FIND_EROW   2
#define BINJ_FIND_EFILE  3

#define BINJ_FIND_MSGEARG   "Error parsing arguments"
#define BINJ_FIND_MSGROW    "Error reading row from XML table"
#define BINJ_FIND_MSGEFILE  "Could not open file"

#define TRUE  1
#define FALSE 0

static int getline(char *line, int max, FILE *fpin)
{
    int i;
    CHAR tmpline[MAXSTR];

    for (i=0;i<MAXSTR;i++) { *(line+i) = '\0' ; }
    if (fgets(tmpline, max, fpin) == NULL){
        return 0;
    }
    else{
        strncpy(line,tmpline,strlen(tmpline)-1);
        return strlen(line);
    }
}


/****************************************************************************
 * 
 * FUNCTION TESTS IF THE FILE CONTAINS ANY PLAY GROUND DATA
 * 
 ***************************************************************************/
static int isPlayground(INT4 gpsStart, INT4 gpsEnd){
    INT4 runStart=729273613;
    INT4 playInterval=6370;
    INT4 playLength=600;
    INT4 segStart,segEnd,segMiddle;

    segStart = (gpsStart - runStart)%playInterval;
    segEnd   = (gpsEnd - runStart)%playInterval;
    segMiddle = gpsStart + (INT4) (0.5 * (gpsEnd - gpsStart));
    segMiddle = (segMiddle - runStart)%playInterval;
    
    if (segStart < playLength || segEnd < playLength || segMiddle < playLength){
        return TRUE;
    }

    return FALSE;
}

/****************************************************************************
 *
 * The main program
 *
 *****************************************************************************/

int main(int argc, char **argv)
{
    static LALStatus         stat;
    FILE                     *fpin=NULL;
    INT4                     fileCounter=0;
    BOOLEAN                  playground=TRUE;
    INT4                     sort=FALSE;
    INT4                     pass=TRUE;
    size_t                   len=0;
    INT4                     ndetected=0;
    INT4                     ninjected=0;
    INT4                     ncheck=0;
    const long S2StartTime   = 729273613;  /* Feb 14 2003 16:00:00 UTC */
    const long S2StopTime    = 734367613;  /* Apr 14 2003 15:00:00 UTC */

       /* filenames */
    CHAR                    *inputFile=NULL;
    CHAR                    *injectionFile=NULL;
    CHAR                    *outputFile=NULL;
    CHAR                    *outSnglFile=NULL;

    /* times of comparison */
    INT4                     gpsStartTime=S2StartTime;
    INT4                     gpsEndTime=S2StopTime;
    INT8                     injPeakTime=0;
    INT8                     burstStartTime=0;

    /* confidence threshold */
    INT4                     maxConfidenceFlag=0;
    REAL4                    maxConfidence=0.0;

    /* duration thresholds */
    INT4                     minDurationFlag=0;
    REAL4                    minDuration=0.0;
    INT4                     maxDurationFlag=0;
    REAL4                    maxDuration=0.0;

    /* central_freq threshold */
    INT4                     maxCentralfreqFlag=0;
    REAL4                    maxCentralfreq=0.0;
    INT4                     minCentralfreqFlag=0;
    REAL4                    minCentralfreq=0.0;

    /* bandwidth threshold */ 
    INT4                     maxBandwidthFlag=0;
    REAL4                    maxBandwidth=0.0;

    /* amplitude threshold */
    INT4                     maxAmplitudeFlag=0;
    REAL4                    maxAmplitude=0.0;
    INT4                     minAmplitudeFlag=0;
    REAL4                    minAmplitude=0.0;

    /* snr threshold */
    INT4                     maxSnrFlag=0;
    REAL4                    maxSnr=0.0;                           
    INT4                     minSnrFlag=0;
    REAL4                    minSnr=0.0;         

    CHAR                     line[MAXSTR];

    /* search summary */
    SearchSummaryTable      *searchSummary=NULL;
    INT4                    timeAnalyzed=0;
    
    /* triggers */
    SnglBurstTable          *tmpEvent=NULL,*currentEvent=NULL,*prevEvent=NULL;
    SnglBurstTable           burstEvent,*burstEventList=NULL,*outEventList=NULL; 
    SnglBurstTable          *detEventList=NULL, *detTrigList=NULL, *prevTrig=NULL;

    /* injections */
    SimBurstTable           *simBurstList=NULL;
    SimBurstTable           *currentSimBurst=NULL, *tmpSimBurst=NULL;
    SimBurstTable           *injSimList=NULL,*prevSimBurst=NULL;

    /* comparison parameters */
    SnglBurstAccuracy        accParams;
    
    /* Table outputs */
    MetadataTable            myTable;
    LIGOLwXMLStream          xmlStream;

    static int		     verbose_flag = 0;

    /*******************************************************************
     * BEGIN PARSE ARGUMENTS (inarg stores the current position)        *
     *******************************************************************/
   int c;
    while (1)
    {
	/* getopt arguments */
	static struct option long_options[] = 
	{
	    /* these options set a flag */
	    {"verbose",         no_argument,	&verbose_flag, 1 },
 	    /* parameters which determine the output xml file */
	    {"input",    	required_argument,  0,	'a'},
	    {"injfile",		required_argument,  0,	'b'},
	    {"outinjfile",      required_argument,  0,	'c'},
	    {"max-confidence",	required_argument,  0,	'd'},
	    {"gps-start-time",	required_argument,  0,	'e'},
	    {"gps-end-time",	required_argument,  0,	'f'},
            {"min-centralfreq", required_argument,  0,  'g'},
	    {"max-centralfreq", required_argument,  0,  'h'},
	    {"noplayground",	required_argument,  0,	'n'},
	    {"help",		no_argument,	    0,	'o'}, 
	    {"sort",		no_argument,	    0,	'p'},
	    {"outsnglfile",     required_argument,  0,  'q'},
	    {0, 0, 0, 0}
	};
	/* getopt_long stores the option index here. */
	int option_index = 0;
 
    	c = getopt_long ( argc, argv, "a:c:d:e:f:g:h:i:", 
				long_options, &option_index );

	/* detect the end of the options */
	if ( c == - 1 )
	    break;

        switch ( c )
	{
	case 0:
	    /* if this option set a flag, do nothing else now */
	    if ( long_options[option_index].flag != 0 )
	    {
		break;
	    }
	    else
	    {
		fprintf( stderr, "error parsing option %s with argument %s\n",
		    long_options[option_index].name, optarg );
		exit( 1 );
	    }
	    break;

	case 'a':
            /* create storage for the input file name */
            len = strlen( optarg ) + 1;
            inputFile = (CHAR *) calloc( len, sizeof(CHAR));
            memcpy( inputFile, optarg, len );
	    break;	
	
	case 'b':
            /* create storage for the injection file name */
            len = strlen( optarg ) + 1;
            injectionFile = (CHAR *) calloc( len, sizeof(CHAR));
            memcpy( injectionFile, optarg, len );
	    break;	
	
	case 'c':
            /* create storage for the output file name */
            len = strlen( optarg ) + 1;
            outputFile = (CHAR *) calloc( len, sizeof(CHAR));
            memcpy( outputFile, optarg, len );
	    break;	

	case 'd':
	    /* the confidence must be smaller than this number */
	    {
              maxConfidenceFlag = 1;
              maxConfidence = atof( optarg );
	    }
	    break;

	case 'e':
	    /* only events with duration greater than this are selected */
	    {
              gpsStartTime = atoi( optarg );
	    }
	    break;
    
	case 'f':
	    /* only events with duration less than this are selected */
	    {
              gpsEndTime = atoi( optarg );
	    }
	    break;

	case 'g':
	     /* only events with centralfreq greater than this are selected */
	    {
              minCentralfreqFlag = 1;
              minCentralfreq = atof( optarg );
	    }
	    break;
    
	case 'h':
	    /* only events with centralfreq less than this are selected */
	    {
              maxCentralfreqFlag = 1;
              maxCentralfreq = atof( optarg );
	    }
	    break;

	case 'n':
	    /* don't restrict to the playground data */
	    {
		playground = FALSE;
	    }
	    break;
    
	case 'o':
	    /* print help */
	    {
		LALPrintError( USAGE, *argv );
		return BINJ_FIND_EARG;
	    }
	    break;

	case 'p':
	    /* sort the events in time */
	    {
		sort = TRUE;
	    }
	    break;
	    

	case 'q':
            /* create storage for the output file name */
            len = strlen( optarg ) + 1;
            outSnglFile = (CHAR *) calloc( len, sizeof(CHAR));
            memcpy( outSnglFile, optarg, len );
	    break;

	default:
	    {
		return BINJ_FIND_EARG;
	    }
	}   
    }

    if ( optind < argc )
    {
	fprintf( stderr, "extraneous command line arguments:\n" );
	    while ( optind < argc )
	{
	    fprintf ( stderr, "%s\n", argv[optind++] );
	}
	exit( 1 );
    }	  

    if ( ! outputFile || ! inputFile || ! injectionFile || ! outSnglFile)
    {
    	LALPrintError( "Input file, injection file, output trig. file and output inj. file 
            names must be specified\n" );
	return BINJ_FIND_EARG;
    }


    /*******************************************************************
    * END PARSE ARGUMENTS                                              *
    *******************************************************************/


    /*******************************************************************
    * initialize things
    *******************************************************************/
    lal_errhandler = LAL_ERR_EXIT;
    set_debug_level( "1" );
    memset( &burstEvent, 0, sizeof(SnglBurstTable) );
    memset( &xmlStream, 0, sizeof(LIGOLwXMLStream) );
    xmlStream.fp = NULL;

    
    /*****************************************************************
     * READ IN THE INJECTIONS
     ****************************************************************/
    if ( verbose_flag )
      fprintf(stdout, "Reading in SimBurst Table\n");

    /*****************************************************************
     * OPEN FILE WITH LIST OF XML FILES (one file per line)
     ****************************************************************/
    if ( !(fpin = fopen(injectionFile,"r")) ){
      LALPrintError("Could not open input file\n");
    }

    /*****************************************************************
     * loop over the xml files
     *****************************************************************/
    currentSimBurst = tmpSimBurst = simBurstList = NULL;
    while ( getline(line, MAXSTR, fpin) ){

      fileCounter++;
      if (verbose_flag)
      {
        fprintf(stderr,"Working on file %s\n", line);
      }

      /* Now the events themselves */
      LAL_CALL( LALSimBurstTableFromLIGOLw ( &stat, &tmpSimBurst, line,
          gpsStartTime, gpsEndTime), &stat );

      /* connect results to linked list */
      if (currentSimBurst == NULL)
      {
        simBurstList = currentSimBurst = tmpSimBurst;
      }
      else
      {
        currentSimBurst->next = tmpSimBurst;
      }

      /* move to the end of the linked list for next input file */
      while (currentSimBurst->next != NULL)
      {
        currentSimBurst = currentSimBurst->next;
      }
      tmpSimBurst = currentSimBurst->next;
    }

    fclose(fpin);



    /*****************************************************************
     * OPEN FILE WITH LIST OF XML FILES (one file per line)
     ****************************************************************/
    if ( !(fpin = fopen(inputFile,"r")) ){
      LALPrintError("Could not open input file\n");
    }

    /*****************************************************************
     * loop over the xml files
     *****************************************************************/
    currentEvent = tmpEvent = burstEventList = NULL;
    while ( getline(line, MAXSTR, fpin) ){
      INT4 tmpStartTime=0,tmpEndTime=0;
      INT4 remainder;

      fileCounter++;
      if (verbose_flag)
      {
        fprintf(stderr,"Working on file %s\n", line);
      }

      /**************************************************************
       * read in search summary information and determine how much
       * data is analyzed: only works for playground right now.
       **************************************************************/
      SearchSummaryTableFromLIGOLw( &searchSummary, line);
      tmpStartTime=searchSummary->out_start_time.gpsSeconds;
      tmpEndTime=searchSummary->out_end_time.gpsSeconds;

      while (tmpStartTime < tmpEndTime)
      {
        remainder = (tmpStartTime - PLAYGROUND_START_TIME)%PLAYGROUND_INTERVAL;
        if (remainder < 600)
        {
          if ( tmpEndTime-tmpStartTime < 600 )
          {
            timeAnalyzed += (tmpEndTime-tmpStartTime);
          }
          else
          {
            timeAnalyzed += (600 - remainder);
          }
        }
        tmpStartTime += (PLAYGROUND_INTERVAL - remainder);
      }
      
      while (searchSummary)
      {
        SearchSummaryTable *thisEvent;
        thisEvent = searchSummary;
        searchSummary = searchSummary->next;
        LALFree( thisEvent );
      }
      searchSummary = NULL; 

      /* Now the events themselves */
      LAL_CALL( LALSnglBurstTableFromLIGOLw (&stat, &tmpEvent, 
            line), &stat);

      /* connect results to linked list */
      if (currentEvent == NULL)
      {
        burstEventList = currentEvent = tmpEvent;
      }
      else
      {
        currentEvent->next = tmpEvent;
      }

      /* move to the end of the linked list for next input file */
      while (currentEvent->next != NULL)
      {
        currentEvent = currentEvent->next;
      }
      tmpEvent = currentEvent->next;
    }

    /****************************************************************
     * do any requested cuts
     ***************************************************************/
    tmpEvent = burstEventList;
    while ( tmpEvent ){

      /* check the confidence */
      if( maxConfidenceFlag && !(tmpEvent->confidence < maxConfidence) )
        pass = FALSE;

      /* check min duration */
      if( minDurationFlag && !(tmpEvent->duration > minDuration) )
        pass = FALSE;

      /* check max duration */
      if( maxDurationFlag && !(tmpEvent->duration < maxDuration) )
        pass = FALSE;

      /* check min centralfreq */
      if( minCentralfreqFlag && !(tmpEvent->central_freq > minCentralfreq) )
        pass = FALSE;

      /* check max centralfreq */
      if( maxCentralfreqFlag && !(tmpEvent->central_freq < maxCentralfreq) )
        pass = FALSE;

      /* check max bandwidth */
      if( maxBandwidthFlag && !(tmpEvent->bandwidth < maxBandwidth) )
        pass = FALSE;

      /* check min amplitude */
      if( minAmplitudeFlag && !(tmpEvent->amplitude > minAmplitude) )
        pass = FALSE;

      /* check max amplitude */
      if( maxAmplitudeFlag && !(tmpEvent->amplitude < maxAmplitude) )
        pass = FALSE;

      /* check min snr */
      if( minSnrFlag && !(tmpEvent->snr > minSnr) )
        pass = FALSE;

      /* check max snr */
      if( maxSnrFlag && !(tmpEvent->snr < maxSnr) )
        pass = FALSE;

      /* check if trigger starts in playground */
      if ( playground && !(isPlayground(tmpEvent->start_time.gpsSeconds,
              tmpEvent->start_time.gpsSeconds)) )
        pass = FALSE;

      /* set it for output if it passes */  
      if ( pass )
      {
        if (outEventList == NULL)
        {
          outEventList = currentEvent = (SnglBurstTable *)
            LALCalloc(1, sizeof(SnglBurstTable) );
          prevEvent = currentEvent;
        }
        else 
        {
          currentEvent = (SnglBurstTable *)
            LALCalloc(1, sizeof(SnglBurstTable) );
          prevEvent->next = currentEvent;
        }
        memcpy( currentEvent, tmpEvent, sizeof(SnglBurstTable));
        prevEvent = currentEvent;
        currentEvent = currentEvent->next = NULL;
      }
      tmpEvent = tmpEvent->next;
      pass = TRUE;
    }

    /*****************************************************************
     * sort the remaining triggers
     *****************************************************************/
    LAL_CALL( LALSortSnglBurst(&stat, &(outEventList), 
                        LALCompareSnglBurstByTime ), &stat);

    
    /*****************************************************************
     * first event in list
     *****************************************************************/

    currentSimBurst = simBurstList;
    currentEvent = detEventList = outEventList;
  
    while ( currentSimBurst != NULL )
      {
      /* check if the injection is made in the playground */
      if ( (playground && !(isPlayground(currentSimBurst->geocent_peak_time.gpsSeconds,
             currentSimBurst->geocent_peak_time.gpsSeconds)))==0 ) 
	{
	  ninjected++;

	  /* write the injected signals to an output file */
	    if ( injSimList == NULL)
	    {
	      injSimList = tmpSimBurst = (SimBurstTable *)
            LALCalloc(1, sizeof(SimBurstTable) );
	      prevSimBurst = tmpSimBurst;
	    }
	  else 
	    {
	      tmpSimBurst = (SimBurstTable *)
		LALCalloc(1, sizeof(SimBurstTable) );
	      prevSimBurst->next = tmpSimBurst;
	    }
	    memcpy( tmpSimBurst, currentSimBurst, sizeof(SimBurstTable) );
	    prevSimBurst = tmpSimBurst;
	    tmpSimBurst = tmpSimBurst->next = NULL;
	  
          /* convert injection time to INT8 */
          LAL_CALL (LALGPStoINT8(&stat, &injPeakTime, 
			     &(currentSimBurst->geocent_peak_time)), &stat);
 

          /* loop over the burst events */
	  while( currentEvent != NULL )
	    {

              /* convert injection time to INT8 */
              LAL_CALL( LALGPStoINT8(&stat, &burstStartTime,
			       &(currentEvent->start_time)), &stat);

	      if( injPeakTime < burstStartTime )
		break;

              LAL_CALL( LALCompareSimBurstAndSnglBurst(&stat,
						 currentSimBurst, currentEvent, &accParams), &stat);

	      if( accParams.match )
		{
		  ndetected++;

		  /*write the detected triggers*/
		  if ( detTrigList == NULL)
		    {
                      detTrigList = tmpEvent = (SnglBurstTable *)
			LALCalloc(1, sizeof(SnglBurstTable) );
		      prevTrig = tmpEvent;
		    }
	          else
		    {
		      tmpEvent = (SnglBurstTable *)
			LALCalloc(1, sizeof(SnglBurstTable) );
		      prevTrig->next = tmpEvent;
		    }
		  memcpy( tmpEvent, currentEvent, sizeof(SnglBurstTable) );
		  prevTrig = tmpEvent;
		  tmpEvent = tmpEvent->next = NULL;

		  break;
		}

	      currentEvent = currentEvent->next;
	    }
	}

      currentSimBurst = currentSimBurst->next;
      }



    /*  fprintf(stdout,"%d sec = %d hours analyzed\n",timeAnalyzed,
	timeAnalyzed/3600);*/
    fprintf(stdout,"Detected %i injections out of %i made\t %i \n",ndetected,ninjected,ncheck);
    fprintf(stdout,"Efficiency is %f \n", ((REAL4)ndetected/(REAL4)ninjected) );

    /*****************************************************************
     * open output xml file
     *****************************************************************/
    LAL_CALL( LALOpenLIGOLwXMLFile(&stat, &xmlStream, outputFile), &stat);
    LAL_CALL( LALBeginLIGOLwXMLTable (&stat, &xmlStream, sim_burst_table), &stat);
    myTable.simBurstTable = injSimList;
    LAL_CALL( LALWriteLIGOLwXMLTable (&stat, &xmlStream, myTable,
                    sim_burst_table), &stat);
    LAL_CALL( LALEndLIGOLwXMLTable (&stat, &xmlStream), &stat);
    LAL_CALL( LALCloseLIGOLwXMLFile(&stat, &xmlStream), &stat);


    LAL_CALL( LALOpenLIGOLwXMLFile(&stat, &xmlStream, outSnglFile), &stat);
    LAL_CALL( LALBeginLIGOLwXMLTable (&stat, &xmlStream, sngl_burst_table), &stat);
    myTable.snglBurstTable = detTrigList;
    LAL_CALL( LALWriteLIGOLwXMLTable (&stat, &xmlStream, myTable,
                    sngl_burst_table), &stat);
    LAL_CALL( LALEndLIGOLwXMLTable (&stat, &xmlStream), &stat);
    LAL_CALL( LALCloseLIGOLwXMLFile(&stat, &xmlStream), &stat);

    return 0;
}
