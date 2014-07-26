
/*========================================================================

	Scheduler.c
	
	This module controls launching of time based actions such as ftp upload,
      tag file processing, conf file re-reading, and email sending.  All of these function happen
      based on a timer or trigger so the action handler should be called frequenty (at least
      once a minute or so) in order to make sure these actions get done when they're supposed to.

      The scheduler makes use of the system time() function in order to detemrine when
      enough time has elapsed based on the configuration.

      Finally, there's some magic in these routines to attempt to align the timing so periodic processing happens at
      even multiples of the frequency (ie. events occurring every 15 mins happen at xx:00, xx:15, xx:30, xx:45.  
      The logic for this is a bit squirrely but it seems to work ok.
      
========================================================================*/

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <stdlib.h>
#include "rtl-wx.h"

static time_t lastConfProcTime;
static time_t lastDataSnapshotTime;
static time_t lastRainDataSnapshotTime;
static time_t lastWebcamSnapshotTime;
static time_t lastTagProcTime;
static time_t lastFtpUploadTime;
static time_t lastTimeoutCheckTime;

static unsigned int configProcCnt;
static unsigned int dataSnapshotCnt;
static unsigned int rainDataSnapshotCnt;
static unsigned int webcamSnapshotCnt;
static unsigned int tagProcCnt;
static unsigned int ftpUploadCnt;

static WX_Data *wxDatap;
static WX_ConfigSettings *configVarp;

static unsigned int getMinutesToWait(unsigned int frequency, time_t currentTime, 
                                                           time_t timeLastDone);
static void checkForSensorTimeouts();
static void updateCurrentTime(WX_Data *weatherDatap);

#define SECS_PER_MIN 60

//--------------------------------------------------------------------------------------------------------------------------------------------
//  Init data structs and timers used by the action scheduler
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_InitActionScheduler(WX_Data *weatherDatap, WX_ConfigSettings *configurationVarp)
{
  wxDatap = weatherDatap;
  configVarp = configurationVarp;

  updateCurrentTime(wxDatap);

  lastConfProcTime = time(NULL);
  lastDataSnapshotTime = time(NULL);
  lastRainDataSnapshotTime = time(NULL);
  lastWebcamSnapshotTime = time(NULL);
  lastTagProcTime = time(NULL);
  lastFtpUploadTime = time(NULL);
  lastTimeoutCheckTime = time(NULL);

  configProcCnt = 0;
  dataSnapshotCnt = 0;
  rainDataSnapshotCnt = 0;
  webcamSnapshotCnt = 0;
  tagProcCnt = 0;
  ftpUploadCnt = 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// MAIN SCHEDULER ROUTINE - see what processing needs to be done this time around.  This routine should get called
//                                                   frequently in order to do its job well.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DoScheduledActions()
{ 
  int i;

  updateCurrentTime(wxDatap);
  if (getMinutesToWait(1, time(NULL), lastTimeoutCheckTime) == 0)
     checkForSensorTimeouts();
  
  // First reread configuration file if more than xx minutes has elapsed since last read
  if (getMinutesToWait(configVarp->configFileReadFrequency, time(NULL), lastConfProcTime) == 0) {
//DPRINTF("Reading rtl-wx.conf at %s", asctime(localtime(&lastConfProcTime)));
    WX_DoConfigFileRead();
  }

  // Next save off data snapshot if it's time 
  if (getMinutesToWait(configVarp->dataSnapshotFrequency, time(NULL), lastDataSnapshotTime) == 0) {
//DPRINTF(" Saving snapshot at %s", asctime(localtime(&lastDataSnapshotTime)));
//printf("saving data snapshot...");fflush(stdout);
    WX_DoDataSnapshotSave();
//printf("done.\n");fflush(stdout);

  }

    // Next save off rain data snapshot if it's time 
  if (getMinutesToWait(configVarp->rainDataSnapshotFrequency, time(NULL), lastRainDataSnapshotTime) == 0) {
//DPRINTF(" Saving rain data snapshot at %s", asctime(localtime(&lastRainDataSnapshotTime)));
//printf("saving rain data snapshot...");fflush(stdout);
    WX_DoRainDataSnapshotSave();
//printf("done.\n");fflush(stdout);
  }
  
  // Next process tag files that need to be processed - Note that if timer is up, but we saved a data snapshot (and zeroed records) within 
  //1 minute we need to wait so there's enough time for new data to make it in.
  if (getMinutesToWait(configVarp->tagFileParseFrequency, time(NULL), lastTagProcTime) == 0) {
//DPRINTF("Parsing Tagfiles at %s", asctime(localtime(&lastTagProcTime)));
//printf("processing tagfiles...");fflush(stdout);
     WX_DoTagFileProcessing();
//printf("done.\n");fflush(stdout);

  }

  // Next process tag files that need to be processed - Note that if timer is up, but we saved a data snapshot (and zeroed records) within 
  //1 minute we need to wait so there's enough time for new data to make it in.
  if (getMinutesToWait(configVarp->webcamSnapshotFrequency, time(NULL), lastWebcamSnapshotTime) == 0) {
//printf("doing webcam snapshot...");fflush(stdout);
     WX_DoWebcamSnapshot();
//printf("done.\n");fflush(stdout);

  }
  // Next,  process ftp uploads that need to be processed
 if (getMinutesToWait(configVarp->ftpUploadFrequency, time(NULL), lastFtpUploadTime) == 0) {
//DPRINTF("Doing FTP upload at %s", asctime(localtime(&lastFtpUploadTime)));
//if (1) {
//printf("Ftp upload...");fflush(stdout);
  WX_DoFtpUpload();
//printf("done.\n");fflush(stdout);
  }
}

int checkSensorForTimeout(WX_Timestamp *ts) {
  long secondsSinceLastMessage = difftime(wxDatap->currentTime.timet, ts->timet);
  
  if ((ts->PktCnt > 0) && (secondsSinceLastMessage > 240))
     return 1;
  else
     return 0;
}

void checkForSensorTimeouts() {

  lastTimeoutCheckTime = time(NULL);
  if (checkSensorForTimeout(&wxDatap->idu.Timestamp))
    wxDatap->idu.DataTimeoutCount++;
  if (checkSensorForTimeout(&wxDatap->odu.Timestamp))
    wxDatap->odu.DataTimeoutCount++;  
  if (checkSensorForTimeout(&wxDatap->rg.Timestamp))
    wxDatap->rg.DataTimeoutCount++;
  if (checkSensorForTimeout(&wxDatap->wg.Timestamp))
    wxDatap->wg.DataTimeoutCount++;
    
  	int sensorIdx;
	for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++)
	  if (checkSensorForTimeout(&wxDatap->ext[sensorIdx].Timestamp))
	    wxDatap->ext[sensorIdx].DataTimeoutCount++;
}

void WX_DoConfigFileRead()
{
  WX_processConfigSettingsFile(CONFIG_FILE_PATH, configVarp);
  lastConfProcTime = time(NULL);    
  configProcCnt++;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Time to save a data snapshot by copying the contents of the wxData structure into the historical storage structure
// By default this is done every 15 minutes.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DoDataSnapshotSave()
{
  WX_SaveWeatherDataRecord(wxDatap);
  lastDataSnapshotTime = time(NULL);
  dataSnapshotCnt++;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Time to save a data snapshot by copying the contents of the wxData structure into the historical storage structure
// By default this is done every 15 minutes.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DoRainDataSnapshotSave()
{
  WX_SaveRainDataRecord(wxDatap);
  lastRainDataSnapshotTime = time(NULL);
  rainDataSnapshotCnt++;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Time to read in each of the input tag files specified in the .conf file and copy the contents to an output file with
// the tags removed and replaced with weather station data.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DoTagFileProcessing()
{
  int i;

  for(i=0;i<configVarp->NumTagFilesToParse;i++)
      WX_ReplaceTagsInTextFile(configVarp->tagFiles[i].inFile,
                                   configVarp->tagFiles[i].outFile);
  lastTagProcTime = time(NULL);
  tagProcCnt++;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Time to snap a webcam image and save the file in the public directory.  This assumes that the camera is set up
// correctly .  This call can be turned off by setting the webcam snapshot frequency to 0
// in the SlugWx.conf file.  
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DoWebcamSnapshot()
{
  // First set the framerate down to 5 so we can grab a larger resolution capture
  // this was only needed for NSLU2 -> system("/opt/bin/setpwc -f 5 > /dev/null");

  // Now do the capture
  // old NSLU2 way was -> system("/opt/bin/vidcat -m -d /dev/video0 -s 320x240 -p y -o webcam.jpg > /dev/null");
  system("fswebcam -r 640x480 web/webcam.jpg > /dev/null");

  lastWebcamSnapshotTime = time(NULL);
  webcamSnapshotCnt++;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Time to upload each file specified in the .conf file to the FTP server that was specified in the conf file.
//--------------------------------------------------------------------------------------------------------------------------------------------
int WX_DoFtpUpload(void)
{ 
  int i, ftpResult, retVal, xferCount = 0;
  char str[250];
  
/* ncftpput -unnnnnnnnnnnn -pxxxxxx ftp.server.com Weather tstfile"); */
			for(i=0;i<configVarp->numFilesToFtp;i++) {
			sprintf(str, "ncftpput -t30 -V -u%s -p%s %s %s %s",  
			   configVarp->ftpServerUsername,configVarp->ftpServerPassword,  configVarp->ftpServerHostname, 
			   configVarp->ftpFiles[i].destpath,configVarp->ftpFiles[i].filename);
			if ((ftpResult = system(str))) {
			  DPRINTF("NCFTPPUT: Error %d putting %s to %s\n",ftpResult, configVarp->ftpFiles[i].filename, configVarp->ftpFiles[i].destpath);
              break;
			}
			else
		      xferCount++; // successful
           }
  if (xferCount == configVarp->numFilesToFtp)
     retVal=1;
  else
     retVal=0;

  lastFtpUploadTime = time(NULL);
  ftpUploadCnt++;

	return(retVal);
}

void updateCurrentTime(WX_Data *weatherDatap) {
   time_t timeNow = time(NULL);
	weatherDatap->currentTime.timet = timeNow;
}

// Determine the remaining wait time before an action should be done.  This routine tries to sync up occurances so they fall on the
// hour and at multiples of the hour where possible.  For example a frequency of 15 minutes should happen at on the hour and at
// hour plus 15, 30, and 45.  Also, frequencies greater than 59 minutes are synced up to happen at the top of the hour.
// Finally, there's some magic to delay the action by 30 seconds in order to deal with likely inconsistencies between the weather station
// time and the slug time
unsigned int getMinutesToWait(unsigned int frequency, time_t currentTime, time_t timeLastDone)
{
  int minutesLeft = 0;
  int minsSinceLastDone = difftime(currentTime, timeLastDone)/SECS_PER_MIN;
  struct tm *localTime = localtime(&currentTime);
  int currentMinute = localTime->tm_min;
  int currentSecond = localTime->tm_sec;

  if (frequency == 0)
    minutesLeft = 9999;
  else if (frequency < 60) {
    if (((60 % frequency) == 0) && ((currentMinute % frequency) != 0))
      minutesLeft = frequency - (currentMinute % frequency);
    else if (((60 % frequency) == 0) && (minsSinceLastDone != 0))
      if (currentSecond < 30)
         minutesLeft = 1;
      else
         minutesLeft = 0;
    else
      minutesLeft = frequency - minsSinceLastDone;
  }
  else {
    if (((frequency % 60) == 0) && (minsSinceLastDone != currentMinute))
      minutesLeft = (60 - currentMinute) % 60;
    else
      minutesLeft = frequency - minsSinceLastDone;
  }
  if (minutesLeft < 0)
     minutesLeft = 0;
  return(minutesLeft);  
}

void printSchedulerAction(FILE *fd, char *label, time_t *lastOccurancep, 
            unsigned int count, unsigned int frequency)
{
  unsigned int remaining;
  char *timeStr;
  time_t currentTime = time(NULL);

	remaining = getMinutesToWait(frequency, currentTime, *lastOccurancep);

  timeStr = asctime(localtime(lastOccurancep));
 
   if (strlen(timeStr) != 0)
      timeStr[strlen(timeStr)-1] = 0;

   fprintf(fd,"%14s  %s   %4d        %3d      ",label, timeStr, count, frequency);
   if (frequency != 0)
     fprintf(fd,"%02d:%02d\n", remaining/60 , remaining % 60);
   else
     fprintf(fd,"--:--\n");

}

void WX_DumpSchedulerInfo(FILE *fd)
{
  time_t timeNow = time(NULL);

fprintf(fd, "\nCurrent System Time: %s", asctime(localtime(&timeNow)));
fprintf(fd, "\n");
fprintf(fd, "                                            Total   Frequency Remaining\n");
fprintf(fd, "Action          Last Occurance           Occurances   (min)    (hh:mm)\n");
fprintf(fd, "--------------  ------------------------ ---------- --------- ---------\n");
  printSchedulerAction(fd, "Read Conf File",  
     &lastConfProcTime, configProcCnt,configVarp->configFileReadFrequency);
  printSchedulerAction(fd, "Save  Snapshot",  
     &lastDataSnapshotTime, dataSnapshotCnt,configVarp->dataSnapshotFrequency);
  printSchedulerAction(fd, "Read Tag Files",  
     &lastTagProcTime, tagProcCnt,configVarp->tagFileParseFrequency);
  printSchedulerAction(fd, "Webcam    Save",  
     &lastWebcamSnapshotTime, webcamSnapshotCnt,configVarp->webcamSnapshotFrequency);  
  printSchedulerAction(fd, "Do  FTP Upload",  
     &lastFtpUploadTime, ftpUploadCnt,configVarp->ftpUploadFrequency);
  fprintf(fd,"\n");
  
  fflush(fd);
}

