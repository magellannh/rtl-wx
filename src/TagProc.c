
/*========================================================================

   TagProc.c
   
   This module contains routines to support automatic updates of weather data
   through the use of predefined TAGS which are replaced in a text or html file
   live weather station data.

   For an example of the Tags and options that are supported by the parser, see the
   file wxTagTest.in

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
   THE COPYRIGHT HOLDERS OR CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.
   
========================================================================*/

#include <string.h>
#include <stdio.h>
#include "rtl-wx.h"
#include "TagProc.h"

//*******************************************************************************************************
// This is a generic handler for sensor fields that don't have data because no packets have arrived from
// the weather station for the given sensor type.  
BOOL checkAndHandleNoData(ParserControlVars *pVars)
{
  BOOL retVal;
  char *str;

  // Check the timestamp for the sensor type being processed to see if there's data.  
  // Also, if data for this field wasn't updated in more than 100 packets, consider that data stale.  
  // This check works for historical records as well since we're checking the pkt count when the data was
  // saved off.  If the current data record is a max/min record, the global current time field is not used and the
  // packet count in  that struct will be zero.
  if ((isTimestampPresent(pVars->ts) == FALSE) ||
      ((pVars->ts->PktCnt+100) < pVars->weatherDatap->currentTime.PktCnt)) {
   if (pVars->formatForNoData == 'D')
     str = "--";
   else if (pVars->formatForNoData == '0')
     str = "0";
   else if (pVars->formatForNoData == 'S')
     str = pVars->NoDataStr;
   else
     str = "";
   strcpy(pVars->outputStr,str);
   retVal=TRUE;
  }
 else
   retVal = FALSE;
 
 return(retVal);
}

//*******************************************************************************************************
// 
// These routines take a weather station data field and format it correctly and transfer it into the results buffer.
// The timestamp structure is used in order to verify that the data for the given sensor typeis present (pkts received, etc)
void formatBatteryField(BOOL batteryLow, ParserControlVars *pVars)
{
  if (checkAndHandleNoData(pVars) == FALSE) {
    if (batteryLow == TRUE) {
      if (pVars->formatControlStr[0] == 'S')
        strcpy(pVars->outputStr,&pVars->formatControlStr[1]);
      else if (pVars->formatControlStr[0] == 'B')
        strcpy(pVars->outputStr,"1");
    } else if (pVars->formatControlStr[0] == 'B')
      strcpy(pVars->outputStr,"0");
  }
}
void formatTemperatureField(float temp, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE) {
      if (pVars->formatControlStr[0] != 'C') // C - celcius
         temp = temp*1.8+32; // convert to farenheit
      sprintf(pVars->outputStr,"%4.1f", temp);
   }
}
void formatRelHumField(int relHum, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE)
      sprintf(pVars->outputStr,"%d", relHum);
}
void formatDewpointField(float dewpoint, ParserControlVars *pVars)
{
   char *str = pVars->formatControlStr;
   BOOL raw=FALSE;
   BOOL celcius=FALSE;
   unsigned int i;
   // First see if the Celcius or Raw options have been specified in the optional format control part of the tag.
   for (i=0;i<strlen(str);i++)
     if (str[i] == 'R')
        raw = TRUE;
     else if (str[i] == 'C')
        celcius = TRUE;

   if (checkAndHandleNoData(pVars) == FALSE) {
      if ((dewpoint == 0) && (raw == FALSE))
        sprintf(pVars->outputStr,"LL.L"); // data out of range.
      else {
        if (celcius == FALSE)
         dewpoint = dewpoint*1.8+32; // convert to F
        sprintf(pVars->outputStr,"%4.1f", dewpoint);
      }
   }
}
void formatPressureField(int pressure, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE) {
    if (pVars->formatControlStr[0] == 'I') // Inches of mercury
      sprintf(pVars->outputStr,"%5.2f", pressure*0.02953);
    else if (pVars->formatControlStr[0] == 'M') // mm/Hg
      sprintf(pVars->outputStr,"%4.0f", pressure*0.75006);
    else
      sprintf(pVars->outputStr,"%4d", pressure); // default is mbars and hPa
   }
}
void formatForecastField(char *forecast, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE)
      sprintf(pVars->outputStr,"%s", forecast);
}
void formatRainField(int rainAmount, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE) {
     if (pVars->formatControlStr[0] == 'M') // millimeters
       sprintf(pVars->outputStr,"%d", rainAmount);
     else // convert to inches
       sprintf(pVars->outputStr,"%3.1f", (float) rainAmount * .0393700787 );
   }
}

void formatWindBearingField(int bearing, ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE)
      sprintf(pVars->outputStr,"%3d", bearing);
}
void formatWindSpeedField(float speed,ParserControlVars *pVars)
{
   if (checkAndHandleNoData(pVars) == FALSE) {
     if (pVars->formatControlStr[0] == 'K')      // Meters/sec
        speed *= 1.609344;
     else if (pVars->formatControlStr[0] == 'M') // Meters/sec
        speed = (speed *1.609344 * 1000) / 3600;
     else if (pVars->formatControlStr[0] == 'N') // knots/hr
        speed *= 0.868976242;
     sprintf(pVars->outputStr,"%4.1f", speed);
   }
} 
void formatWindchillField(int windchill, BOOL chillvalid, ParserControlVars *pVars)
{
  if (checkAndHandleNoData(pVars) == FALSE) {
    char *str = pVars->formatControlStr;
    BOOL raw=FALSE;
    BOOL celcius=FALSE;
    unsigned int i;

    // Check for format options from end of tag
    for (i=0;i<strlen(str);i++)
      if (str[i] == 'R')
        raw = TRUE;
      else if (str[i] == 'C')
        celcius = TRUE;
    
    if ((chillvalid == FALSE) && (raw == FALSE))
      sprintf(pVars->outputStr,"HH.H");
    else
      sprintf(pVars->outputStr,"%3.0f", (celcius == FALSE) ? windchill*1.8+32: windchill);
  }
}
void formatChillValidField(BOOL chillValid,ParserControlVars *pVars)
{
  if (checkAndHandleNoData(pVars) == FALSE) {
     if (chillValid== TRUE)
        sprintf(pVars->outputStr, "1");
     else 
        sprintf(pVars->outputStr, "0");
  }
}

// Some dayofweek and text month support data and functions
char *textDayOfWeek[] = {"Monday", "Tuesday", "Wednesday",
                         "Thursday","Friday","Saturday", "Sunday" };
char *textMonth[] = { "January", "February", "March", "April", "May","June",
                    "July", "August", "September", "October","November","December"};
// function to return the day of the week given the date
// original Turbo C, modified for Pelles C by  vegaseat    8oct2004
// Pelles C free at:  http://smorgasbordet.com/pellesc/index.htm
// given month, day, year, returns day of week, eg. Monday = 0 etc.
char *getDayOfWeek(int month, int day, int year)
{   
  int ix, tx, vx;
  char buf[20];

  switch (month) {
   case 2  :
   case 6  : vx = 0; break;
   case 8  : vx = 4; break;
   case 10 : vx = 8; break;
   case 9  :
   case 12 : vx = 12; break;
   case 3  :
   case 11 : vx = 16; break;
   case 1  :
   case 5  : vx = 20; break;
   case 4  :
   case 7  : vx = 24; break;
  }
  if (year > 1900)  // 1900 was not a leap year
    year -= 1900;
  ix = ((year - 21) % 28) + vx + (month > 2);  // take care of February 
  tx = (ix + (ix / 4)) % 7 + day;              // take care of leap year

  return (textDayOfWeek[(tx % 7)]);
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------
   processTimestampField()

  This function causes some part of a WX_Timestamp structure to be written to the parser output buffer as part of a tag
  replacement.  

  Timestamps are used for vaious purposes including to indicate the time and packet count when the last update from a sensor 
  arrived as well as to store the current date and time or the date and time that the Rain Gauge was last reset.

  The WMR9X8 weather station sends full date and time updates every hour on the hour, while the minute count is updated once
   per minute.  Arrival rates of data from sensors vary.

  For Tag processing, the Timestamp part of the tag may have come from the sensorToGetFrom or from the fieldToGet depending
  on the Tag being processed.  This routine expects that any additional formatting options are stored in the formatCommandStr 
  buffer in the parser control structure.
--------------------------------------------------------------------------------------------------------------------------------------------------------*/
void processTimestampField(char *tsParams, ParserControlVars *pVars)
{
  WX_Timestamp *ts = pVars->ts;
  struct tm *localts;
  
  time_t timeNow = time(NULL);
  if (ts->PktCnt == 0)
     localts = localtime(&timeNow); // When timestamp is invalid, init localts to something
  else
     localts = localtime(&ts->timet);

  if (strcmp(tsParams,"TSDATE") == 0) {
    // TSDATE - Need to handle no data case specially
    if (ts->PktCnt == 0) {
      if (pVars->formatForNoData == 'D')
          sprintf(pVars->outputStr,"--/--/----");
      else if (pVars->formatForNoData == 'S')
         strcpy(pVars->outputStr, pVars->NoDataStr);       
      else
          sprintf(pVars->outputStr,"00/00/0000");
    }
    else {
      sprintf(pVars->outputStr,"%02d/%02d/%04d", localts->tm_mon+1, localts->tm_mday, localts->tm_year+1900);  
    }
  }
  else if (strcmp(tsParams, "TSTIME") == 0) {
   if (ts->PktCnt != 0) {
     if (pVars->formatControlStr[0] != '2')
        sprintf(pVars->outputStr,"%02d:%02d %s",
               (localts->tm_hour>12?localts->tm_hour-12:(localts->tm_hour<1?12:localts->tm_hour)),
                    localts->tm_min,((localts->tm_hour >=12)?"PM":"AM"));
     else
        sprintf(pVars->outputStr,"%02d:%02d",localts->tm_hour, localts->tm_min);
   }
   else {
     if (pVars->formatForNoData == 'D')
       sprintf(pVars->outputStr,"--:--   ");
     else if (pVars->formatForNoData == 'S')
           sprintf(pVars->outputStr,"%s",pVars->NoDataStr);
     else if (pVars->formatForNoData == '0')
       sprintf(pVars->outputStr,"00:00   ");
     else
       sprintf(pVars->outputStr,"  :     ");    
   }
  } 
  else if (strcmp(tsParams, "TSPKTCNT") == 0)
      sprintf(pVars->outputStr,"%d",ts->PktCnt);
  else if (strcmp(tsParams, "TSMINUTE") == 0)
      sprintf(pVars->outputStr,"%02d",localts->tm_min);
  else if (strcmp(tsParams, "TSYEAR") == 0)
      sprintf(pVars->outputStr,"%04d",localts->tm_year+1900);
  else if (strcmp(tsParams, "TSMONTH") == 0) 
        sprintf(pVars->outputStr,"%02d",localts->tm_mon+1);
  else if (strcmp(tsParams, "TSMONTHTEXT") == 0) {
      int maxLen = pVars->formatControlStr[0]- '0';
      if ((localts->tm_mon >=0) && (localts->tm_mon < 12))
        sprintf(pVars->outputStr, "%s",textMonth[localts->tm_mon]);
      if ((maxLen > 0) && (maxLen <= 9))
         pVars->outputStr[maxLen]=0;
    }
  else if (strcmp(tsParams, "TSDAY") == 0) 
      sprintf(pVars->outputStr,"%02d",localts->tm_mday);
  else if (strcmp(tsParams, "TSHOUR") == 0)  {
      if (pVars->formatControlStr[0] != '2')
        sprintf(pVars->outputStr,"%02d",
                  (localts->tm_hour>12? localts->tm_hour-12 : (localts->tm_hour<1?12:localts->tm_hour)));
      else
        sprintf(pVars->outputStr,"%02d",localts->tm_hour);
    }
  else if (strcmp(tsParams, "TSDAYOFWEEK") == 0) {
      int maxLen = pVars->formatControlStr[0]-'0';
      sprintf(pVars->outputStr,"%s", getDayOfWeek(localts->tm_mon+1, localts->tm_mday, localts->tm_year+1900));
      if ((maxLen > 0) && (maxLen <= 9))
         pVars->outputStr[maxLen]=0;
    }
  else if (strcmp(tsParams, "TSAMPM") == 0) {
      if (localts->tm_hour < 12)
        strcpy(pVars->outputStr,"AM");
      else
        strcpy(pVars->outputStr,"PM");
  }
}

/* no longer supported void formatRainResetField(ParserControlVars *pVars)
{
  // Set timestamp to date and time of last rain gauge reset
  pVars->ts = &pVars->weatherDatap->rg.LastReset;

  if (strcmp(pVars->fieldToGet,"RAINRESET") ==0) {
    // If no timestamp formatting is specified, build a generic timestamp out of the reset date and time
    char dateStr[MAX_TAG_SIZE], timeStr[MAX_TAG_SIZE];
    processTimestampField("TSDATE", pVars);
    strcpy(dateStr, pVars->outputStr);
    processTimestampField("TSTIME", pVars);
    strcpy(timeStr, pVars->outputStr);
    sprintf(pVars->outputStr,"%s %s",dateStr,timeStr);
  }
  else {
    // Timestamp tag has RRTSXXX format, need to skip over the RR to get real timestamp format and use it
    char *formatStr= &pVars->fieldToGet[2];
    processTimestampField(formatStr, pVars);
  }
}*/

//*************************************************************************************************************
void processIduTag(ParserControlVars *pVars)
{
  // Save the IDU timestamp object in the parser control structure
  // This gets used in data present checks and timestamp processing later on
  pVars->ts = &pVars->weatherDatap->idu.Timestamp;

  if (strcmp(pVars->fieldToGet,"BATTERY") == 0)
      formatBatteryField(pVars->weatherDatap->idu.BatteryLow, pVars);
  else if (strcmp(pVars->fieldToGet,"TEMP") == 0) {
    pVars->ts = &pVars->weatherDatap->idu.TempTimestamp;
      formatTemperatureField(pVars->weatherDatap->idu.Temp, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"HUMIDITY") == 0) {
    pVars->ts = &pVars->weatherDatap->idu.RelHumTimestamp;
      formatRelHumField(pVars->weatherDatap->idu.RelHum, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"DEWPOINT") == 0) {
    pVars->ts = &pVars->weatherDatap->idu.RelHumTimestamp;
      formatDewpointField(pVars->weatherDatap->idu.Dewpoint, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"PRESSURE") == 0) {
    pVars->ts = &pVars->weatherDatap->idu.PressureTimestamp;
      formatPressureField(pVars->weatherDatap->idu.Pressure, pVars);
  }
  else if (strncmp(pVars->fieldToGet,"TEMP-TS",7) == 0) {
    pVars->ts = &pVars->weatherDatap->idu.TempTimestamp;
      processTimestampField(&pVars->fieldToGet[5], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"HUMIDITY-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->idu.RelHumTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"DEWPOINT-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->idu.RelHumTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"PRESSURE-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->idu.PressureTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strcmp(pVars->fieldToGet,"FORECAST") == 0)
      formatForecastField(pVars->weatherDatap->idu.ForecastStr, pVars);
  else if (strcmp(pVars->fieldToGet,"SEALEVELOFFSET") == 0)
      formatPressureField(pVars->weatherDatap->idu.SeaLevelOffset, pVars);
  else if (strncmp(pVars->fieldToGet,"TS",2) == 0)
      processTimestampField(pVars->fieldToGet, pVars);
  else
    sprintf(pVars->outputStr,"WXERROR_IDUTAG-%s",pVars->fieldToGet);
}

//*************************************************************************************************************
void procesOduTag(ParserControlVars *pVars)
{
  // Save the ODU timestamp object in the parser control structure
  // This gets used in data present checks and timestamp processing later on
  pVars->ts = &pVars->weatherDatap->odu.Timestamp;

  if (strcmp(pVars->fieldToGet,"BATTERY") == 0)
      formatBatteryField(pVars->weatherDatap->odu.BatteryLow, pVars);
  else if (strcmp(pVars->fieldToGet,"TEMP") == 0) {
    pVars->ts = &pVars->weatherDatap->odu.TempTimestamp;
      formatTemperatureField(pVars->weatherDatap->odu.Temp, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"HUMIDITY") == 0) {
    pVars->ts = &pVars->weatherDatap->odu.RelHumTimestamp;
      formatRelHumField(pVars->weatherDatap->odu.RelHum, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"DEWPOINT") == 0) {
    pVars->ts = &pVars->weatherDatap->odu.DewpointTimestamp;
      formatDewpointField(pVars->weatherDatap->odu.Dewpoint, pVars);
  }
  else if (strncmp(pVars->fieldToGet,"TEMP-TS",7) == 0) {
    pVars->ts = &pVars->weatherDatap->odu.TempTimestamp;
      processTimestampField(&pVars->fieldToGet[5], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"HUMIDITY-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->odu.RelHumTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"DEWPOINT-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->odu.DewpointTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  } 
  else if (strncmp(pVars->fieldToGet,"TS",2) == 0)
      processTimestampField(pVars->fieldToGet, pVars);
  else
    sprintf(pVars->outputStr,"WXERROR_ODUTAG-%s",pVars->fieldToGet);
}

//*************************************************************************************************************
void procesExtTag(int sensorIdx, ParserControlVars *pVars)
{
  // Save the appropriate extra sensor timestamp object in the parser control structure
  pVars->ts = &pVars->weatherDatap->ext[sensorIdx].Timestamp;

  if (strcmp(pVars->fieldToGet,"BATTERY") == 0)
      formatBatteryField(pVars->weatherDatap->ext[sensorIdx].BatteryLow, pVars);
  else if (strcmp(pVars->fieldToGet,"TEMP") == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].TempTimestamp;
      formatTemperatureField(pVars->weatherDatap->ext[sensorIdx].Temp, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"HUMIDITY") == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].RelHumTimestamp;
      formatRelHumField(pVars->weatherDatap->ext[sensorIdx].RelHum, pVars);
  }
  else if (strcmp(pVars->fieldToGet,"DEWPOINT") == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].DewpointTimestamp;
      formatDewpointField(pVars->weatherDatap->ext[sensorIdx].Dewpoint, pVars);
  }
  else if (strncmp(pVars->fieldToGet,"TEMP-TS",7) == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].TempTimestamp;
      processTimestampField(&pVars->fieldToGet[5], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"HUMIDITY-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].RelHumTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strncmp(pVars->fieldToGet,"DEWPOINT-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->ext[sensorIdx].DewpointTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  } 
  else if (strncmp(pVars->fieldToGet,"TS",2) == 0)
      processTimestampField(pVars->fieldToGet, pVars);
  else
    sprintf(pVars->outputStr,"WXERROR_EXTTAG-%s",pVars->fieldToGet);
}

//*************************************************************************************************************
void procesRgTag(ParserControlVars *pVars)
{
  // Save the Rain Gauge timestamp object in the parser control structure
  pVars->ts = &pVars->weatherDatap->rg.Timestamp;

  if (strcmp(pVars->fieldToGet,"BATTERY") == 0)
      formatBatteryField(pVars->weatherDatap->rg.BatteryLow, pVars);
  else if (strcmp(pVars->fieldToGet,"RAINRATE") == 0) {
    pVars->ts = &pVars->weatherDatap->rg.RateTimestamp;
      formatRainField(pVars->weatherDatap->rg.Rate, pVars);
  }
  else if (strncmp(pVars->fieldToGet,"RAINRATE-TS",11) == 0) {
    pVars->ts = &pVars->weatherDatap->rg.RateTimestamp;
      processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strcmp(pVars->fieldToGet,"RAINTOTAL") == 0)
      formatRainField(pVars->weatherDatap->rg.Total, pVars);
/* Total yesterday and rain reset are not supported now that we're getting data directly from sensors
  else if (strcmp(pVars->fieldToGet,"RAINYESTERDAY") == 0)
      formatRainField(pVars->weatherDatap->rg.TotalYesterday, pVars);
  else if (strcmp(pVars->fieldToGet,"RAINRESET") == 0)
    formatRainResetField(pVars);
  else if (strncmp(pVars->fieldToGet,"RR",2) == 0)
    formatRainResetField(pVars); */
  else if (strncmp(pVars->fieldToGet,"TS",2) == 0)
      processTimestampField(pVars->fieldToGet, pVars);
  else
    sprintf(pVars->outputStr,"WXERROR_RGTAG-%s",pVars->fieldToGet);
}
int sumRainDataRecords(int startRecord, int endRecord)
{
  int i;
  int sum=0;
  for (i=startRecord;i<=endRecord;i++)
    sum += WX_GetRainDataRecord(i);
  return(sum);
}

//*************************************************************************************************************
void procesRainHistTag(ParserControlVars *pVars)
{
  int recordsToSum=0;

  // Save the Rain Gauge timestamp object in the parser control structure
  pVars->ts = &pVars->weatherDatap->rg.Timestamp;

  if (strcmp(pVars->fieldToGet,"TOTALLASTDAY") == 0)
        recordsToSum=24*1;
  else if (strcmp(pVars->fieldToGet,"TOTALLAST3DAY") == 0)
        recordsToSum=24*3;
  else if (strcmp(pVars->fieldToGet,"TOTALLASTWEEK") == 0)
        recordsToSum=24*7;

  if (recordsToSum != 0)
     formatRainField(sumRainDataRecords(1,recordsToSum), pVars);
  else if (strcmp(pVars->fieldToGet,"TOTALLASTWEEKBYDAY") == 0)
    {
   int day;
   for (day=6;day>=0;day--) {
           int startRecord=1+(day*24);
            int sum=sumRainDataRecords(startRecord,startRecord+23);
         formatRainField(sum, pVars);
            if (day >0) {
           fputs(pVars->outputStr, pVars->outfd);
              fputc(pVars->spacerForMultipleRecords, pVars->outfd);
            }
      }
   }
  else
    sprintf(pVars->outputStr,"WXERROR_RGTAG-%s",pVars->fieldToGet);
}
//*************************************************************************************************************
void procesWgTag(ParserControlVars *pVars)
{
  // Save the Wind  Gauge timestamp object in the parser control structure
  pVars->ts = &pVars->weatherDatap->wg.Timestamp;
  if (strcmp(pVars->fieldToGet,"BATTERY") == 0)
      formatBatteryField(pVars->weatherDatap->wg.BatteryLow, pVars);
  else if (strcmp(pVars->fieldToGet,"BEARING") == 0)
    formatWindBearingField(pVars->weatherDatap->wg.Bearing, pVars);
  else if (strcmp(pVars->fieldToGet,"SPEED") == 0) {
   pVars->ts = &pVars->weatherDatap->wg.SpeedTimestamp;
   formatWindSpeedField(pVars->weatherDatap->wg.Speed,pVars); 
  } 
  else if (strcmp(pVars->fieldToGet,"AVGSPEED") == 0) {
   pVars->ts = &pVars->weatherDatap->wg.AvgSpeedTimestamp;
   formatWindSpeedField(pVars->weatherDatap->wg.AvgSpeed,pVars);
  }
  else if (strncmp(pVars->fieldToGet,"SPEED-TS",8) == 0) {
   pVars->ts = &pVars->weatherDatap->wg.SpeedTimestamp;
    processTimestampField(&pVars->fieldToGet[6], pVars);
  } 
  else if (strncmp(pVars->fieldToGet,"AVGSPEED-TS",11) == 0) {
   pVars->ts = &pVars->weatherDatap->wg.AvgSpeedTimestamp;
    processTimestampField(&pVars->fieldToGet[9], pVars);
  }
  else if (strcmp(pVars->fieldToGet,"WINDCHILL") == 0)
    formatWindchillField(pVars->weatherDatap->wg.WindChill, pVars->weatherDatap->wg.ChillValid, pVars); 
  else if (strcmp(pVars->fieldToGet,"CHILLVALID") == 0)
    formatChillValidField(pVars->weatherDatap->wg.ChillValid,pVars); 
  else if (strncmp(pVars->fieldToGet,"TS",2) == 0)
    processTimestampField(pVars->fieldToGet, pVars);
  else
    sprintf(pVars->outputStr,"WXERROR_WGTAG-%s",pVars->fieldToGet);
}   

//*************************************************************************************************************
// Based on which sensor type was referenced in the Tag, dispatch to the correct processing routine for the specific sensor type
void processTag(ParserControlVars *pVars)
{
  // First see if the tag is a global formatting tag
  if (strcmp(pVars->sensorToGetFrom,"GLOBAL") == 0) {
    if (strcmp(pVars->fieldToGet,"NODATA") == 0) {
       pVars->formatForNoData = pVars->formatControlStr[0];
      if (pVars->formatControlStr[0] == 'S')
        strcpy(pVars->NoDataStr, &pVars->formatControlStr[1]);
    }
    else if (strcmp(pVars->fieldToGet,"MULTISPACER") == 0) {
       pVars->spacerForMultipleRecords = pVars->formatControlStr[0];
    }
    else 
     sprintf(pVars->outputStr,"WXERROR_BADGLOBAL-%s",pVars->fieldToGet);
  }
  else if (strcmp(pVars->sensorToGetFrom,"IDU") == 0)
      processIduTag(pVars);
  else if (strcmp(pVars->sensorToGetFrom,"ODU") == 0)
      procesOduTag(pVars);      
  else if (strcmp(pVars->sensorToGetFrom,"RG") == 0)
      procesRgTag(pVars);
  else if (strcmp(pVars->sensorToGetFrom,"RAINHIST") == 0)
      procesRainHistTag(pVars);  
  else if (strcmp(pVars->sensorToGetFrom,"WG") == 0)
      procesWgTag(pVars);         
  else if (strcmp(pVars->sensorToGetFrom,"EXT1") == 0)
      procesExtTag(0, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT2") == 0)
      procesExtTag(1, pVars);         
  else if (strcmp(pVars->sensorToGetFrom,"EXT3") == 0)
      procesExtTag(2, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT4") == 0)
      procesExtTag(3, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT5") == 0)
      procesExtTag(4, pVars);         
  else if (strcmp(pVars->sensorToGetFrom,"EXT6") == 0)
      procesExtTag(5, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT7") == 0)
      procesExtTag(6, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT8") == 0)
      procesExtTag(7, pVars);         
  else if (strcmp(pVars->sensorToGetFrom,"EXT9") == 0)
      procesExtTag(8, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"EXT10") == 0)
      procesExtTag(9, pVars);
  else if (strcmp(pVars->sensorToGetFrom,"BADPKTCNT") == 0)
      sprintf(pVars->outputStr, "%d", pVars->weatherDatap->BadPktCnt); 
  else if (strcmp(pVars->sensorToGetFrom,"UNSUPPORTEDPKTCNT") == 0)
      sprintf(pVars->outputStr, "%d", pVars->weatherDatap->UnsupportedPktCnt); 
  else if (strncmp(pVars->sensorToGetFrom,"TS",2) == 0) {
      pVars->ts = &pVars->weatherDatap->currentTime;
      strcpy(pVars->formatControlStr, pVars->fieldToGet); // Formatting string is in fieldToGet
      processTimestampField(pVars->sensorToGetFrom, pVars);
  }
  else
         sprintf(pVars->outputStr,"WXERROR_BADSENSOR-%s",pVars->sensorToGetFrom);
}
//*************************************************************************************************************
// Routine to convert the Record Number part of the tag to actual record numbers.
// expected format is -1:20:2 (leading dash is already removed)
//                       1- starting record number (from 1 to max records to store, 1 is the newest record, record 20 has 19 in front of it
//                      20 - ending record number to get
//                      2  - increment when getting records from 1 to 2 (in this case, get every other record)
//                      The parser also supports getting record 20 to record 1 (increment should still be positive)
int processRecordNumberStr(char *recordNumStr, int *recordNumList)
{
  int varCount, i;
  int recordCount=0;
  int start=1; 
  int end=1;
  int increment=1;

  if ((varCount = sscanf(recordNumStr,"%d:%d:%d",&start, &end, &increment)) != 3)
     if ((varCount = sscanf(recordNumStr,"%d:%d",&start, &end)) != 2)
        varCount = sscanf(recordNumStr,"%d",&start);
 
  if ((start < 0) || (start > WX_NUM_RECORDS_TO_STORE))
     start=WX_NUM_RECORDS_TO_STORE;
  if ((end < 0) || (end > WX_NUM_RECORDS_TO_STORE))
     end=WX_NUM_RECORDS_TO_STORE;

  if (varCount == 1)
    end = start;

  if (start <= end)
    for (i=start; i<=end; i+=increment)
       recordNumList[recordCount++] = i;
  else
    for (i=start; i>=end; i-=increment)
       recordNumList[recordCount++] = i;
//printf("VarCount=%d start=%d end=%d inc=%d recordCount\n", varCount, start, end, increment, recordCount);
  return(recordCount);
}
//*************************************************************************************************************
// Routine to extract the next part of the tag.  Each part of the tag is delimited by the "_" character (or - in some cases)
// ie WXTAG_IDU_TEMP_C has three parts (sensorToGetFrom (IDU), fieldToGet(TEMP), formatControlStr("C")
BOOL extractTagParam(char *rdBuf, char *outStr, int *idx)
{
   BOOL retVal=0;
   int j=0;
   int startPosition=*idx;

   outStr[0] = 0;

   // skip over starting '_' if present
   if (rdBuf[*idx] == '_')
       (*idx)++;

   // Copy characters from input buffer into output string until '_', '-', end of tag, end of line, or bfr size exceeded
   while ((rdBuf[*idx] != '_') && (rdBuf[*idx] != '^') && (rdBuf[*idx] != '#') && 
          (rdBuf[*idx] != 0) && (rdBuf[*idx] != '\r') && (rdBuf[*idx] != '\n') && 
          (*idx < READ_BUFSIZE) && (j < MAX_TAG_SIZE))
     outStr[j++] = rdBuf[(*idx)++];
   outStr[j] = 0; // Terminate output string

   // If we didn't end at one of the familiar delimiter characters, the tag is probably not closed correctly.
   if ((rdBuf[*idx] != '_') && (rdBuf[*idx] != '^') && (rdBuf[*idx] != '#')) {
    DPRINTF("Unclosed tag, input line:\n%s\n",rdBuf);
    for (j=0;j<startPosition;j++)
        DPRINTF(" ");
    DPRINTF("^\n");
    retVal = 1; // return error
   }
   // If nothing found, return an error
   //if (j==0)
   //  retVal=1;

   return(retVal);
}


/*-----------------------------------------------------------------------------------------------------------------------
  WX_ReplaceTagsInTextFile()
 
  This function opens two files, one for reading and one for writing, and copies the contents (char by char) from the input file
  to the output file, while searching for special Tags that are replaced with weather station data.

  See the file wxTagTest.in for a complete listing of supported Tags and tag options.

-------------------------------------------------------------------------------------------------------------------------*/

void WX_ReplaceTagsInTextFile(char *inFname, char *outFname)
{
   FILE *infd,*outfd;
  char rdBuf[READ_BUFSIZE];
  char tagBuf[MAX_TAG_SIZE];
  ParserControlVars pVars;

 if ((infd = fopen(inFname, "r")) == NULL) {
     DPRINTF("Tag Processor was unable to open %s for reading.\n", inFname);
 }
 else if ((outfd = fopen(outFname, "w+")) == NULL) {
     DPRINTF("Tag Processor was unable to open %s for writing.\n", outFname);
 }
 else {

 // set default formatting if no data
 pVars.formatForNoData = 'D'; //  use dashes when no data is available
 pVars.spacerForMultipleRecords = ','; // when outputting data from multiple records, separate with comma
 pVars.outfd = outfd;
 // Read each line in the file and process the tags on that line.
 while (fgets(rdBuf, READ_BUFSIZE, infd) != NULL)
  {
  int i=0;
  while ((rdBuf[i] != 0) && (i < READ_BUFSIZE))
   {
   // Init optional vars and strings to null
   int  recordNum = 0;
   char recordNumStr[MAX_TAG_SIZE];
   recordNumStr[0] = 0;
   pVars.formatControlStr[0] = 0;
   pVars.outputStr[0] = 0;

   if (strncmp(&rdBuf[i], "^WXTAG_", 7) == 0) {
    i+=7; // Skip over begining of tag
    if (extractTagParam(rdBuf, pVars.sensorToGetFrom, &i) != 0) { // ie IDU, ODU, WG, RG, etc
       DPRINTF("Unable to get Tag Type on line: \n  %s\n", rdBuf);
       sprintf(pVars.outputStr,"WXERROR_BADTYPE-%s",pVars.sensorToGetFrom);
       fputs(pVars.outputStr,outfd);
    }
    else if (extractTagParam(rdBuf, pVars.fieldToGet, &i) != 0){ // ie TEMP, DEWPOINT, SPEED, etc
       DPRINTF("Unable to get Tag Field on line: \n  %s\n",rdBuf);
       sprintf(pVars.outputStr,"WXERROR_BADFIELD-%s-%s",pVars.sensorToGetFrom,pVars.fieldToGet);
       fputs(pVars.outputStr,outfd);    }
    else {
      // These next two are optional, the format control string has an _ in front of it, while the record number has a -
      if (rdBuf[i] == '_')
         extractTagParam(rdBuf, pVars.formatControlStr, &i); // Optional -   _M, _Y, etc 
      if (rdBuf[i] == '#') {
         i++;
         extractTagParam(rdBuf, recordNumStr, &i); // Optional #1:96:1
      }
      if (rdBuf[i] == '^') { // got to end of tag successfully
        i++; // Skip over end of tag character
        if (recordNumStr[0] == 0)  {
          pVars.weatherDatap= &wxData;  // Use the current dataset (not historical) to process this tag
          processTag(&pVars);
          fputs(pVars.outputStr, outfd);
//DPRINTF(pVars.outputStr); // echo to local console    
        }
        else if (recordNumStr[1] == 'I')  { // Get min historical value
          pVars.weatherDatap = WX_GetMinDataRecord();
          processTag(&pVars);
          fputs(pVars.outputStr, outfd);
//DPRINTF(pVars.outputStr); // echo to local console    
        }
        else if (recordNumStr[1] == 'A')  { // Get max historical record
          pVars.weatherDatap= WX_GetMaxDataRecord();
          processTag(&pVars);
          fputs(pVars.outputStr, outfd);
//DPRINTF(pVars.outputStr); // echo to local console    
        }
        else {
         int recordNumList[WX_NUM_RECORDS_TO_STORE];
         int recordCount, i;
         if ((recordCount = processRecordNumberStr(recordNumStr, recordNumList)) != 0)
          for (i=0;i<recordCount;i++) {
//printf("getting %d of %d: ",recordNumList[i], recordCount);
            pVars.weatherDatap = 
                         WX_GetWeatherDataRecord(recordNumList[i]);
            processTag(&pVars);
            fputs(pVars.outputStr, outfd);
//DPRINTF(pVars.outputStr); // echo to local console    
            if (i < (recordCount-1)) {
              fputc(pVars.spacerForMultipleRecords, outfd);
//DPRINTF("%c",pVars.spacerForMultipleRecords);
            }
//printf("\n");
          }
         else {
          sprintf(pVars.outputStr, "WXERROR_BADRECORDNUM-%s-%s",pVars.sensorToGetFrom,pVars.fieldToGet);
          fputs(pVars.outputStr,outfd);
          DPRINTF("Error processing tag: %s\n",pVars.outputStr);
         }
        }
      }
      else {
        sprintf(pVars.outputStr, "WXERROR_BADTAG-%s-%s",pVars.sensorToGetFrom,pVars.fieldToGet);
        fputs(pVars.outputStr,outfd);
        DPRINTF("Error processing sensor tag: %s\n",pVars.outputStr);
      }
    }
   }
   else {
//DPRINTF("%c",rdBuf[i]); // echo to local console
     fputc(rdBuf[i++], outfd);
     }
   }
  }
 fclose(infd);
 fclose(outfd);
 }
}
