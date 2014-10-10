
/*========================================================================

   ConfProc.c
   
   This module contains routines to  update the weather monitoring
     software configuration settings by reading a configuration file

   For an example of the settings and options that are supported, see the
   file rtl-wx.conf
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
   THE COPYRIGHT HOLDERS OR CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include <string.h>
#include <stdio.h>
#include "rtl-wx.h"

// define global configuration settings structure
WX_ConfigSettings WxConfig;

#define READ_BUFSIZE 1000

static int processNumericVar(char *buf,char *matchStr, int *varp);
static int processFloatVar(char *buf,char *matchStr, float *varp);
static int processStringVar(char *buf,char *matchStr, char *destStr);
static int processExtSensorNames(char *buf, WX_ConfigSettings *cVarp);
static int processftpFilename(char *buf, WX_ConfigSettings *cVarp);
static int processTagProcFilename(char *buf, WX_ConfigSettings *cVarp);
static int processMailMsgConfig(char *buf, WX_ConfigSettings *cVarp);
static int processRealtimeCsvFileInfo(char *buf, WX_ConfigSettings *cVarp);
static int processCsvFileInfo(char *buf, WX_ConfigSettings *cVarp);

/*-----------------------------------------------------------------------------------------------------------------------------------------------------
  WX_ProcessConfFile()
 
  This function reads program configuration data from a text file.
  See the file SlugWx.conf for a complete listing of supported configuration options.
------------------------------------------------------------------------------------------------------------------------------------------------------*/

int WX_processConfigSettingsFile(char *inFname, WX_ConfigSettings *cVarp)
{
 FILE *infd;
 char rdBuf[READ_BUFSIZE];

 // These settings are reset before each read of the configuration file.
 cVarp->tagFileParseFrequency=0; 
 cVarp->configFileReadFrequency=0; // 15 minutes
 cVarp->dataSnapshotFrequency=15;
 cVarp->rainDataSnapshotFrequency=0;

 cVarp->fuelBurnerOnWattageThreshold=0;
 cVarp->fuelBurnerGallonsPerHour=1;
 
 cVarp->webcamSnapshotFrequency=0;
 
 cVarp->realtimeCsvWriteFrequency=0;
 cVarp->realtimeCsvFile[0]=0;
 
 cVarp->numCsvFilesToUpdate=0;
 cVarp->NumTagFilesToParse=0;
 cVarp->ftpUploadFrequency=0;
 cVarp->ftpServerHostname[0]=0;
 cVarp->ftpServerUsername[0]=0;
 cVarp->ftpServerPassword[0]=0;
 cVarp->numFilesToFtp = 0;
 cVarp->mailServerHostname[0]=0;
 cVarp->mailServerUsername[0]=0;
 cVarp->mailServerPassword[0]=0;
 cVarp->numMailMsgsToSend = 0; 
 cVarp->mailSendFrequency = 0; 

 cVarp->iduNameString[0]=0;
 cVarp->oduNameString[0]=0;
 { 
   int i; for (i=0;i<=MAX_SENSOR_CHANNEL_INDEX;i++) cVarp->extNameStrings[i][0]=0; 
 }
  
 if ((infd = fopen(inFname, "r")) == NULL) {
  DPRINTF("Config Processor was unable to open %s for reading.\n",inFname);
  return(1);
 }

 // Read each line in the file and process the tags on that line.
 while (fgets(rdBuf, READ_BUFSIZE, infd) != NULL)
  {
  if ((rdBuf[0] != ';') && (rdBuf[0] != '[')) { 
   if (processNumericVar(rdBuf,"sensorLockingEnabled", &cVarp->sensorLockingEnabled)) {}
   else if (processNumericVar(rdBuf,"altitudeInFeet", &cVarp->altitudeInFeet)) {}
   else if (processNumericVar(rdBuf,"fuelBurnerOnWattageThreshold", &cVarp->fuelBurnerOnWattageThreshold)) {}
   else if (processFloatVar(rdBuf,"fuelBurnerGallonsPerHour", &cVarp->fuelBurnerGallonsPerHour)) {}
   else if (processNumericVar(rdBuf,"dataSnapshotFrequency", &cVarp->dataSnapshotFrequency)) {}
   else if (processNumericVar(rdBuf,"ftpUploadFrequency", &cVarp->ftpUploadFrequency)) {}
   else if (processNumericVar(rdBuf,"tagFileParseFrequency", &cVarp->tagFileParseFrequency)) {}
   else if (processNumericVar(rdBuf,"webcamSnapshotFrequency", &cVarp->webcamSnapshotFrequency)) {}
   else if (processNumericVar(rdBuf,"configFileReadFrequency", &cVarp->configFileReadFrequency)) {}
   else if (processNumericVar(rdBuf,"dataSnapshotFrequency", &cVarp->dataSnapshotFrequency)) {}
   else if (processNumericVar(rdBuf,"rainDataSnapshotFrequency", &cVarp->rainDataSnapshotFrequency)) {}
   else if (processStringVar(rdBuf,"ftpServerHostname", cVarp->ftpServerHostname)) {}
   else if (processStringVar(rdBuf,"ftpServerUsername", cVarp->ftpServerUsername)) {}
   else if (processStringVar(rdBuf,"ftpServerPassword", cVarp->ftpServerPassword)) {}
   else if (processStringVar(rdBuf,"mailServerHostname", cVarp->mailServerHostname)) {}
   else if (processStringVar(rdBuf,"mailServerUsername", cVarp->mailServerUsername)) {}
   else if (processStringVar(rdBuf,"mailServerPassword", cVarp->mailServerPassword)) {}
   else if (processStringVar(rdBuf,"iduSensorName", cVarp->iduNameString)) {}
   else if (processStringVar(rdBuf,"oduSensorName", cVarp->oduNameString)) {}
   else if (processExtSensorNames(rdBuf, cVarp)) {}
   else if (processMailMsgConfig(rdBuf,cVarp)) {}
   else if (processftpFilename(rdBuf,cVarp)) {}
   else if (processTagProcFilename(rdBuf,cVarp)) {}
   else if (processRealtimeCsvFileInfo(rdBuf,cVarp)) {}
   else if (processCsvFileInfo(rdBuf,cVarp)) {}
  }
 }
 fclose(infd);

 return(0);
}

int processExtSensorNames(char *buf, WX_ConfigSettings *cVarp) {
  char tagSearchName[80];
  int sensorIdx;
  
  for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++) {
   sprintf(tagSearchName, "ext%dSensorName", sensorIdx+1);
   if (processStringVar(buf, tagSearchName, cVarp->extNameStrings[sensorIdx]))
     return 1;
  }
  return 0;
}
   
// Process a config file line with a single string var  on it
// Must be of the form 'StringVar=str' with no spaces or special characters before the '='.  After the = whitespace is treated
// as part of the string. 
int processStringVar(char *buf,char *matchStr, char *destStr)
{
  int retVal=0;
  int len= strlen(matchStr);

  if (strncmp(buf, matchStr, len) == 0) {
    int i=len;
    int j=0;

    if (buf[i] != 0) // skip over '='
       i++;
    while (((buf[i] == ' ') || (buf[i] =='\t')) && (i<READ_BUFSIZE))
       i++;
    while ((buf[i] != 0) && (buf[i] != '\r') && (buf[i] != '\n') && (i<READ_BUFSIZE) && (j<(MAX_CONFIG_NAME_SIZE-1)))
      destStr[j++] = buf[i++];
    destStr[j]=0; 
    retVal=1;
  }
  return(retVal);   
}

// Process a config file line with a numeric var  on it
// Must be of the form 'numericVar=n' with no spaces or special characters.  
int processNumericVar(char *buf,char *matchStr, int *varp)
{
  int retVal=0;
  int len= strlen(matchStr);
  int varCount, temp=0;

  if (strncmp(buf, matchStr, len) == 0) {
    varCount = sscanf(&buf[len+1],"%d",&temp);
    if (varCount == 1) 
       *varp = temp;
    retVal=1;
  }

  return(retVal);   
}
// Process a config file line with a float var  on it
// Must be of the form 'floatVar=n.n' with no spaces or special characters.  
int processFloatVar(char *buf,char *matchStr, float *varp)
{
  int retVal=0;
  int len= strlen(matchStr);
  int varCount;
  float temp=0;

  if (strncmp(buf, matchStr, len) == 0) {
    varCount = sscanf(&buf[len+1],"%f",&temp);
    if (varCount == 1) 
       *varp = temp;
    retVal=1;
  }
  return(retVal);   
}
// process config file line for FTP filename.  Format is "localFilename remotePathAndFilename"
// the ftpFile text is r
int processftpFilename(char *buf, WX_ConfigSettings *cVarp)
{
  int i;
  int j;
  int retVal=0;
  char *filename = cVarp->ftpFiles[cVarp->numFilesToFtp].filename;
  char *destpath = cVarp->ftpFiles[cVarp->numFilesToFtp].destpath;

  if (strncmp("ftpFile",buf, 7) == 0) {
   i=7;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy filename string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != 0) && (i < READ_BUFSIZE))
     filename[j++] = buf[i++];
   filename[j] = 0;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy destpath string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != '\r') &&
          (buf[i] != '\n') && (buf[i] != 0) && (i < READ_BUFSIZE))
     destpath[j++] = buf[i++];
   destpath[j] = 0;

   if ((filename[0] != 0) && (destpath[0] != 0)) {
    cVarp->numFilesToFtp++;
    retVal = 1;
   }
   else
    retVal=0;
  }
  return(retVal);
}
int processTagProcFilename(char *buf, WX_ConfigSettings *cVarp) 
{
  int i;
  int j;
  int retVal=0;
  char *filename = cVarp->tagFiles[cVarp->NumTagFilesToParse].inFile;
  char *destpath = cVarp->tagFiles[cVarp->NumTagFilesToParse].outFile;

  if (strncmp("tagFile",buf, 7) == 0) {
   i=7;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy filename string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != 0) && (i < READ_BUFSIZE))
     filename[j++] = buf[i++];
   filename[j] = 0;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy destpath string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != '\r') &&
          (buf[i] != '\n') && (buf[i] != 0) && (i < READ_BUFSIZE))
     destpath[j++] = buf[i++];
   destpath[j] = 0;

   if ((filename[0] != 0) && (destpath[0] != 0)) {
    cVarp->NumTagFilesToParse++;
    retVal = 1;
   }
   else
    retVal=0;
  }
  return(retVal);
}
int processRealtimeCsvFileInfo(char *buf, WX_ConfigSettings *cVarp) 
{
  int i;
  int j;
  int retVal=0;
  char *fname = cVarp->realtimeCsvFile;
  
  if (strncmp("realtimeCsvFile",buf, 15) == 0) {
   i=15;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy filename string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != 0) && (i < READ_BUFSIZE))
     fname[j++] = buf[i++];
   fname[j] = 0;
   
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
    
   // copy snapshots string until whitespace is encountered
   j=0;
   char minutesStr[MAX_CONFIG_NAME_SIZE];
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != '\r') &&
          (buf[i] != '\n') && (buf[i] != 0) && (i < READ_BUFSIZE))
     minutesStr[j++] = buf[i++];
   minutesStr[j] = 0;
   
   int minutesBetweenUpdates = atoi(minutesStr);
   if (minutesBetweenUpdates < 0)
      minutesBetweenUpdates = 0;
   
   if ((fname[0] != 0) && (minutesBetweenUpdates != 0)) {
    cVarp->realtimeCsvWriteFrequency = minutesBetweenUpdates;
    retVal = 1;
   }
   else
    retVal=0;
  }
  return(retVal);
}
int processCsvFileInfo(char *buf, WX_ConfigSettings *cVarp) 
{
  int i;
  int j;
  int retVal=0;
  char *fname = cVarp->csvFiles[cVarp->numCsvFilesToUpdate].fname;
  char snapshotStr[MAX_CONFIG_NAME_SIZE];
  
  if (strncmp("csvFile",buf, 7) == 0) {
   i=7;
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
   // copy filename string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != 0) && (i < READ_BUFSIZE))
     fname[j++] = buf[i++];
   fname[j] = 0;
   
   // skip over any whitespace
   while (((buf[i] == ' ') || (buf[i] == '\t')) && (i < READ_BUFSIZE))
    i++;
    
   // copy snapshots string until whitespace is encountered
   j=0;
   while ((buf[i] != ' ') && (buf[i] != '\t') && (buf[i] != '\r') &&
          (buf[i] != '\n') && (buf[i] != 0) && (i < READ_BUFSIZE))
     snapshotStr[j++] = buf[i++];
   snapshotStr[j] = 0;
   int snapshotsBetweenUpdates = atoi(snapshotStr);
   if (snapshotsBetweenUpdates > 96)
      snapshotsBetweenUpdates = 0;
   else if (snapshotsBetweenUpdates < 1)
      snapshotsBetweenUpdates = 0;
   
   if ((fname[0] != 0) && (snapshotsBetweenUpdates != 0)) {
    cVarp->csvFiles[cVarp->numCsvFilesToUpdate].snapshotsBetweenUpdates = snapshotsBetweenUpdates;
    cVarp->numCsvFilesToUpdate++;
    retVal = 1;
   }
   else
    retVal=0;
  }
  return(retVal);
}
static int extractQuotedString(char *buf, int *iptr, char *destString)
{
   int j;
   while ((buf[*iptr] != '"') && (buf[*iptr] != 0) && (*iptr < READ_BUFSIZE))
     (*iptr)++;
   j=0;
   if (buf[*iptr] == '"') {
      (*iptr)++;
      while ((buf[*iptr] != '"') && (buf[*iptr] != 0) && (*iptr < READ_BUFSIZE))
         destString[j++] = buf[(*iptr)++];
   }
   destString[j] = 0;

   if (buf[*iptr] == '"')
       (*iptr)++;
   if (destString[0] != 0)
      return(1);
   else
      return(0);
}

int processMailMsgConfig(char *buf, WX_ConfigSettings *cVarp)
{
  int i;
  int j;
  int retVal=0;
  char *subject = cVarp->mailMsgList[cVarp->numMailMsgsToSend].subject;
  char *recipients = cVarp->mailMsgList[cVarp->numMailMsgsToSend].recipients;
  char *bodyFilename = cVarp->mailMsgList[cVarp->numMailMsgsToSend].bodyFilename;

  if (strncmp("mailMessage",buf, 11) == 0) {
   i=11;
   // First try to extract subject and recipient list which are each delimited by quotes
   if (extractQuotedString(buf, &i, subject) ==0) {}
   else if (extractQuotedString(buf, &i, recipients) ==0) {}
   else {
    // Extract filename (which isn't in quotes)
    while (((buf[i] == ' ') || (buf[i] =='\t')) && (i<READ_BUFSIZE))
       i++;
    j=0;
    while ((buf[i] != 0) && (buf[i] != '\r') && (buf[i] != '\n') && (i<READ_BUFSIZE))
      bodyFilename[j++] = buf[i++];
    bodyFilename[j]=0;
   }
   if ((recipients[0] != 0) && (subject[0] != 0) && (bodyFilename[0] != 0)) {
    cVarp->numMailMsgsToSend++;
    retVal = 1;
   } else
    retVal=0;
  }
  return(retVal);
}
