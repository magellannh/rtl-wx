/*========================================================================
   Utils.c
   
   Utility functions used by the SlugWx weather monitor.  Mostly routines to dump 
   various data structures for debug or status reporting.

   This module also contains a generic routine that checks to see if a weather data field
   timestamp is present (ie - there's data in the field, not just 0's)

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

#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/signal.h>
#include <sys/types.h>

#include "rtl-wx.h"

static void printTimeDateAndUptime(FILE *fd);

//--------------------------------------------------------------------------------------------------------------------------------------------
// Dump the current weather station data to the file specified in a human readable format.  This is called based on
// user input when running in interactive mode (either standalone or server with a client attached) or by remote
// command mode when used to produce output for the embedded web pages
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DumpInfo(FILE *fd) { 
   int sensorIdx;
  
   printTimeDateAndUptime(fd);
   
   // --------------------------- Outdoor Unit info ---------------------------------------------------
   if (isTimestampPresent(&wxData.odu.Timestamp))  {
      if (WxConfig.oduNameString[0] != 0)
         fprintf(fd, "   %s (ODU)  Temp: ",WxConfig.oduNameString);
      else
         fprintf(fd, "   Outdoor Temp: ");
      fprintf(fd, "%5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.odu.Temp*1.8 + 32, wxData.odu.RelHum);
      fprintf(fd,"%2.1f ",wxData.odu.Dewpoint*1.8+32);
      if (wxData.odu.BatteryLow == TRUE)
      fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n\n"); 
   }

   // --------------------------- Extra Sensor info ---------------------------------------------------
   for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++) {
      if (isTimestampPresent(&wxData.ext[sensorIdx].Timestamp)) {
         char label[80];
         if (WxConfig.extNameStrings[sensorIdx][0] != 0)
            fprintf(fd, "   %s (Ch%2d) ",WxConfig.extNameStrings[sensorIdx], sensorIdx+1);
         else 
            fprintf(fd, "   Extra Sensor%2d ", sensorIdx+1);
         fprintf(fd, "Temp: %5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.ext[sensorIdx].Temp*1.8 + 32, wxData.ext[sensorIdx].RelHum);
         fprintf(fd,"%2.1f ",wxData.ext[sensorIdx].Dewpoint*1.8+32);
         if (wxData.ext[sensorIdx].BatteryLow == TRUE)
            fprintf(fd, "(** Sensor Battery Low)");
            fprintf(fd,"\n");
      }
   }
   // --------------------------- Indoor Unit info ---------------------------------------------------
   if (isTimestampPresent(&wxData.idu.Timestamp))  {
      if (WxConfig.iduNameString[0] != 0)
         fprintf(fd, "\n   %s (IDU)  Temp: ",WxConfig.iduNameString);
      else
         fprintf(fd, "\n   Indoor  Temp: ");
      fprintf(fd, "%5.1f  Relative Humidity: %d%%  Dewpoint: ",
               wxData.idu.Temp*1.8 + 32, wxData.idu.RelHum);
      fprintf(fd,"%2.1f ",wxData.idu.Dewpoint*1.8+32);
      if (wxData.idu.BatteryLow == TRUE)
         fprintf(fd, "(** Low Sensor Battery)");
      fprintf(fd,"\n");

      if (WxConfig.iduNameString[0] != 0)
         fprintf(fd, "   %s (IDU)  Pressure: ",WxConfig.iduNameString);
      else
         fprintf(fd, "   Indoor  Pressure: ");      
      fprintf(fd, "%d mbar   Sealevel: %4d Forecast: %s\n",
                    wxData.idu.Pressure, wxData.idu.Pressure+wxData.idu.SeaLevelOffset, wxData.idu.ForecastStr);
   }
   
   fprintf(fd, "\n");
   // --------------------------- Wind Gauge info ---------------------------------------------------
   if (isTimestampPresent(&wxData.wg.Timestamp)) {
      fprintf(fd, "   Wind Speed: %4.1f  Avg Speed: %4.1f  Direction: %d",
               wxData.wg.Speed, wxData.wg.AvgSpeed, wxData.wg.Bearing);
      if (wxData.wg.ChillValid)
         fprintf(fd," Wind Chill: %2.0f ",wxData.wg.WindChill*1.8+32);
      else 
         fprintf(fd," Wind Chill: --\n");
      if (wxData.wg.BatteryLow == TRUE)
         fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n");   ;
   }
   // --------------------------- Energy Sensor info ---------------------------------------------------
   if (isTimestampPresent(&wxData.energy.Timestamp))
      fprintf(fd, "   Energy Usage (Efergy): %4d watts  Avg Last Hr: %4d  Avg Last Day: %4d\n", 
		wxData.energy.Watts, getWattsAvgAvg(1, 4), getWattsAvgAvg(1, 24*4));  
   if (isTimestampPresent(&wxData.owl.Timestamp)) {
      float fuelBurnedLastHour = (float) getBurnerRunSecondsTotal(0, 4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
      float fuelBurnedLastDay = (float) getBurnerRunSecondsTotal(0, 24*4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
      float fuelBurnedTotal = (float) WX_totalBurnerRunSeconds/(60*60) * WxConfig.fuelBurnerGallonsPerHour;
      if (fuelBurnedLastDay != 0)
         fprintf(fd, "     Oil Usage (Gallons): %4.2f (last hr)  %4.2f (last day)  %4.2f (Total)\n", 
		fuelBurnedLastHour, fuelBurnedLastDay, fuelBurnedTotal);
      else
         fprintf(fd, "   Energy Usage (OWL119): %4d watts  Avg Last Hr: %4d  Avg Last Day: %4d\n", 
		wxData.owl.Watts, getWattsAvgAvg(0, 4), getWattsAvgAvg(0, 24*4));
   }
   if ((isTimestampPresent(&wxData.energy.Timestamp)) || (isTimestampPresent(&wxData.owl.Timestamp)))
      fprintf(fd, "\n");

   // --------------------------- Rain Gauge info ---------------------------------------------------
   if (isTimestampPresent(&wxData.rg.Timestamp)) {
      fprintf(fd, "   Rainfall: %dmm/hr  Total Rainfall: %dmm ", wxData.rg.Rate, wxData.rg.Total);
      if (wxData.wg.BatteryLow == TRUE)
         fprintf(fd, "(**Low Sensor Battery)");
      fprintf(fd,"\n");   
   }
  fprintf(fd,"\n");
}

int getWattsAvgAvg(int use_efergy_sensor, int numSnapshotsToAverage) {
	int i;
	int sumWattsAvg=0;
	int wattsAvgCount=0;
	for (i=1;i<=numSnapshotsToAverage;i++) {
		WX_Data *wxDatap = WX_GetWeatherDataRecord(i);
		if (wxDatap != NULL) {
			if (use_efergy_sensor) {
			   if (isTimestampPresent(&wxDatap->energy.Timestamp)) {
				sumWattsAvg += wxDatap->energy.WattsAvg;
				wattsAvgCount++;
			   }
			} else {
			   if (isTimestampPresent(&wxDatap->owl.Timestamp)) {
				sumWattsAvg += wxDatap->owl.WattsAvg;
				wattsAvgCount++;
			   }
			}
		}
	}
	if (wattsAvgCount != 0)
		return (sumWattsAvg/wattsAvgCount);
	else
		return (0);
}
int getBurnerRunSecondsTotal(int use_efergy_sensor, int numSnapshotsToSum) {
	int i;
	int runSecondsTotal=0;
	for (i=1;i<=numSnapshotsToSum;i++) {
		WX_Data *wxDatap = WX_GetWeatherDataRecord(i);
		if (wxDatap != NULL) {
			if (use_efergy_sensor) {
			   if (isTimestampPresent(&wxDatap->energy.Timestamp))
				runSecondsTotal += wxDatap->energy.BurnerRuntimeSeconds;
			} else {
			   if (isTimestampPresent(&wxDatap->owl.Timestamp))
				runSecondsTotal += wxDatap->owl.BurnerRuntimeSeconds;
			}
		}
	}
	return (runSecondsTotal);
}
int getEnergyHistoryIndex(int minute, int second, int samples_per_minute) {
  // For any minute,second pair, return the index into the energy history array that
  // corresponds to that  time.   Energy history is indexed by time and sample number.
  // It's assumed that there are x samples per minute and that the history stores
  // y minutes of sample data.  So given any minute/second pair, the
  // appropriate index into the sample array for that time can be computed.
  // if second=0, the first sample for the given minute is returned
  int minutesOffset =  minute % NUM_MINUTES_PER_SNAPSHOT;
  int secondsOffset = second/(60 / samples_per_minute);
  int index = (minutesOffset*samples_per_minute) + secondsOffset;
  if ((index<0) || (index >= (samples_per_minute*NUM_MINUTES_PER_SNAPSHOT))) {
    DPRINTF("Energy history index out of range (%d). Minute=%d Second=%d samples_per_Min=%d\n", index, minute, second, samples_per_minute);
    index=0;
  }
  return index;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DumpEnergyHistoryInfo(FILE *fd, char *sensor_name, WX_EnergySensorData *energyp, int samples_per_minute) { 
      
   int dumping_efergy_sensor = (energyp == &wxData.energy);
   
   if (isTimestampPresent(&energyp->Timestamp))  { 
        float fuelBurnedLastHour = (float) getBurnerRunSecondsTotal(dumping_efergy_sensor, 4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
        float fuelBurnedLastDay = (float) getBurnerRunSecondsTotal(dumping_efergy_sensor, 24*4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
	float fuelBurnedTotal = (float) WX_totalBurnerRunSeconds/(60*60) * WxConfig.fuelBurnerGallonsPerHour;

	if (fuelBurnedLastDay != 0)
          fprintf(fd, "   Current %s Energy Use: %d watts  Oil Used Last Hr: %4.2f  Last Day: %4.2f Cumulative: %4.2f (gals)\n\n", 
		 sensor_name, energyp->Watts, fuelBurnedLastHour, fuelBurnedLastDay, fuelBurnedTotal);
	else
          fprintf(fd, "   Current %s Energy Use: %d watts  Avg Last Hr: %d  Avg Last Day: %d\n\n", 
		sensor_name, energyp->Watts, getWattsAvgAvg(dumping_efergy_sensor, 4), getWattsAvgAvg(dumping_efergy_sensor,24*4));	struct tm *localTime = localtime(&wxData.currentTime.timet);

	fprintf(fd,"   Most Recent Sample Data (%d samples per minute)\n    ", samples_per_minute);
	int min;
	for (min=0;min<15;min++) {
		int displayMinute = localTime->tm_min - min;
		int displayHour = localTime->tm_hour;
		if (displayMinute < 0) {
			displayMinute = 60 + displayMinute;
			if (displayHour == 0)
				displayHour = 23;
			else 
				displayHour -= 1;
		}
		fprintf(fd,"%02d:%02d ",displayHour, displayMinute);
	}
	fprintf(fd,"\n    ");
	int minutesSinceSnapshot = localTime->tm_min % 15;
	int sample;
	for (sample=0;sample<samples_per_minute; sample++) {
		for (min=0;min<15;min++) {
			// Use current data for all minutes since the last snapshot. 
			int minuteToGet = localTime->tm_min - min;
			if (minuteToGet < 0)
				minuteToGet = 15 + minuteToGet;
			minuteToGet = minuteToGet % 15;
			int idx = getEnergyHistoryIndex(minuteToGet,0, samples_per_minute) + sample;
			int watts=0;
			if (min <= minutesSinceSnapshot)
				watts = energyp->WattsHistory[idx];
			else {
				WX_Data *wxDatap = WX_GetWeatherDataRecord(1);
				if (wxDatap != NULL) {
				   if (dumping_efergy_sensor)
				      watts = wxDatap->energy.WattsHistory[idx];
				   else
				      watts = wxDatap->owl.WattsHistory[idx];
				}
			}
			if (watts == 0)
				fprintf(fd,"      ");
			else
				fprintf(fd,"%5d ", watts);
		}
		fprintf(fd,"\n    ");
	}

	if ((dumping_efergy_sensor == 1) || (WxConfig.fuelBurnerOnWattageThreshold == 0)) {
	// Dump average wattage efergy energy sensor and owl energy sensor, when owl is not being used for buner on time tracking...
	fprintf(fd,"\n   Average energy usage (watts) over last 24 hours in 15 minute intervals\n    ");
	int col, row;
	for (row=0;row<4; row++) {
		for (col=1;col<=16; col++) {
			WX_Data *wxDatap = WX_GetWeatherDataRecord((row*16)+col);
			if (isTimestampPresent(&wxDatap->currentTime)) {
				struct tm *localTime = localtime(&wxDatap->currentTime.timet);
				fprintf(fd,"%02d:%02d ", localTime->tm_hour, localTime->tm_min);
			} else
				fprintf(fd,"      ");
		}
		fprintf(fd,"\n    ");
		for (col=1;col<=16;col++) {
			int record = (row*16) + col;
			int watts = 0;
			WX_Data *wxDatap = WX_GetWeatherDataRecord(record);
			if (wxDatap != NULL) {
				if (dumping_efergy_sensor)
				      watts = wxDatap->energy.WattsAvg;
				else
				      watts = wxDatap->owl.WattsAvg;
			}
			if (watts != 0)
				fprintf(fd,"%5d ", watts);
			else
				fprintf(fd,"      ");
		}
		fprintf(fd,"\n\n    ");
	}
	} else { 
	fprintf(fd,"\n   Burner run time in seconds during last 24 hours (15 minute intervals)\n    ");
	int col, row;
	for (row=0;row<4; row++) {
		for (col=1;col<=16; col++) {
			WX_Data *wxDatap = WX_GetWeatherDataRecord((row*16)+col);
			if (isTimestampPresent(&wxDatap->currentTime)) {
				struct tm *localTime = localtime(&wxDatap->currentTime.timet);
				fprintf(fd,"%02d:%02d ", localTime->tm_hour, localTime->tm_min);
			} else
				fprintf(fd,"      ");
		}
		fprintf(fd,"\n    ");
		for (col=1;col<=16;col++) {
			int record = (row*16) + col;
			int seconds = 0;
			WX_Data *wxDatap = WX_GetWeatherDataRecord(record);
			if (wxDatap != NULL)
				seconds = wxDatap->owl.BurnerRuntimeSeconds;
			if (seconds != 0)
				fprintf(fd,"%5d ", seconds);
			else
				fprintf(fd,"      ");
		}
		fprintf(fd,"\n\n    ");
	} }
   fprintf(fd,"\n");
   }
}

static void printMaxMinTemp(FILE *fd, char *label,float maxTemp, WX_Timestamp *maxTs, float minTemp, WX_Timestamp *minTs);
static void printMaxMinRelHum(FILE *fd, char *label, int maxHum, WX_Timestamp *maxTs,int minHum, WX_Timestamp *minTs);
static void printMaxMinDewpoint(FILE *fd,char *label,float maxDewpoint,WX_Timestamp *maxTs,float minDewpoint,WX_Timestamp *minTs);
static void printMaxMinWindSpeed(FILE *fd, char *label,float maxSpeed, WX_Timestamp *maxTs,float minSpeed, WX_Timestamp *minTs);
static void printTimestamp(FILE *fd, WX_Timestamp *ts);

//--------------------------------------------------------------------------------------------------------------------------------------------
// Dump  max/min weather station data to the file specified in a human readable format.  This is called based on
// user input when running in interactive mode (either standalone or server with a client attached) or by the remote
// command mode (to produce web output)
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_DumpMaxMinInfo(FILE *fd)
{ 
  int i;
  WX_Data *maxDatap = WX_GetMaxDataRecord();
  WX_Data *minDatap = WX_GetMinDataRecord();
  char label[100];
   
  printTimeDateAndUptime(fd);
   
  fprintf(fd,"                   Min           Time                  Max            Time\n\n");
         
  if (WxConfig.iduNameString[0] != 0)
      sprintf(label, "\n   %s (IDU)\n     Temperature",WxConfig.iduNameString);
  else
      sprintf(label, "\n   Indoor\n     Temperature");
  printMaxMinTemp(fd, label,
                  maxDatap->idu.Temp, &maxDatap->idu.TempTimestamp, 
                  minDatap->idu.Temp, &minDatap->idu.TempTimestamp);
  printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->idu.Dewpoint, &maxDatap->idu.DewpointTimestamp, 
                  minDatap->idu.Dewpoint, &minDatap->idu.DewpointTimestamp);
  printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->idu.RelHum, &maxDatap->idu.RelHumTimestamp, 
                  minDatap->idu.RelHum, &minDatap->idu.RelHumTimestamp);
  
  fprintf(fd, "\n");
  if (WxConfig.oduNameString[0] != 0)
      sprintf(label, "\n   %s (ODU)\n     Temperature",WxConfig.oduNameString);
  else
      sprintf(label, "\n   Outdoor\n     Temperature");
  printMaxMinTemp(fd, label,
                  maxDatap->odu.Temp, &maxDatap->odu.TempTimestamp, 
                  minDatap->odu.Temp, &minDatap->odu.TempTimestamp);
  printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->odu.Dewpoint, &maxDatap->odu.DewpointTimestamp, 
                  minDatap->idu.Dewpoint, &minDatap->odu.DewpointTimestamp);
  printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->odu.RelHum, &maxDatap->odu.RelHumTimestamp, 
                  minDatap->odu.RelHum, &minDatap->odu.RelHumTimestamp);

  for(i=0;i<=MAX_SENSOR_CHANNEL_INDEX;i++) {
      if (WxConfig.extNameStrings[i][0] != 0)
        sprintf(label, "\n   %s (Ch %d)\n     Temperature",WxConfig.extNameStrings[i], i+1);
      else
        sprintf(label, "\n   Extra Sensor (Ch %d)\n     Temperature", i+1);
      printMaxMinTemp(fd, label,
                  maxDatap->ext[i].Temp, &maxDatap->ext[i].TempTimestamp, 
                  minDatap->ext[i].Temp, &minDatap->ext[i].TempTimestamp);
      printMaxMinDewpoint(fd, "        Dewpoint",
                  maxDatap->ext[i].Dewpoint, &maxDatap->ext[i].DewpointTimestamp, 
                  minDatap->ext[i].Dewpoint, &minDatap->ext[i].DewpointTimestamp);
      printMaxMinRelHum(fd, "        Humidity",
                  maxDatap->ext[i].RelHum, &maxDatap->ext[i].RelHumTimestamp, 
                  minDatap->ext[i].RelHum, &minDatap->ext[i].RelHumTimestamp);
  }

  printMaxMinWindSpeed(fd, "\n   Wind Gauge\n      Wind Speed",
                  maxDatap->wg.Speed, &maxDatap->wg.SpeedTimestamp, 
                  minDatap->wg.Speed, &minDatap->wg.SpeedTimestamp);
  printMaxMinWindSpeed(fd,"      Avg. Speed",
                  maxDatap->wg.AvgSpeed, &maxDatap->wg.AvgSpeedTimestamp, 
                  minDatap->wg.AvgSpeed, &minDatap->wg.AvgSpeedTimestamp);
  if (isTimestampPresent(&maxDatap->energy.Timestamp) || 
      isTimestampPresent(&minDatap->energy.Timestamp)) {
     fprintf(fd, "\n   Power Consumption\n    Efergy Watts");
     fprintf(fd, " %5d  ", minDatap->energy.Watts);
     printTimestamp(fd, &minDatap->energy.Timestamp); 
     fprintf(fd, "  %5d  ", maxDatap->energy.Watts);
     printTimestamp(fd, &maxDatap->energy.Timestamp);
     fprintf(fd, "\n"); 
    }
  if (isTimestampPresent(&maxDatap->owl.Timestamp) || 
      isTimestampPresent(&minDatap->owl.Timestamp)) {
     fprintf(fd, "    OWL119 Watts");
     fprintf(fd, " %5d  ", minDatap->owl.Watts);
     printTimestamp(fd, &minDatap->owl.Timestamp); 
     fprintf(fd, "  %5d  ", maxDatap->owl.Watts);
     printTimestamp(fd, &maxDatap->owl.Timestamp);
     fprintf(fd, "\n");
     fprintf(fd, "  Burner On Secs");
     fprintf(fd, " %5d  ", minDatap->owl.BurnerRuntimeSeconds);
     printTimestamp(fd, &minDatap->owl.Timestamp); 
     fprintf(fd, "  %5d  ", maxDatap->owl.BurnerRuntimeSeconds);
     printTimestamp(fd, &maxDatap->owl.Timestamp);
     fprintf(fd, "\n");
    }
  if (isTimestampPresent(&maxDatap->rg.RateTimestamp) || 
      isTimestampPresent(&minDatap->rg.RateTimestamp)) {
     fprintf(fd, "\n   Rain Gauge\n       Rain Rate");
     fprintf(fd, "  %4d  ", minDatap->rg.Rate);
     printTimestamp(fd, &minDatap->rg.RateTimestamp); 
     fprintf(fd, "   %4d  ", maxDatap->rg.Rate);
     printTimestamp(fd, &maxDatap->rg.RateTimestamp);
     fprintf(fd, "\n"); 
    }
  fprintf(fd, "\n");
 }

void printMaxMinTemp(FILE *fd, char *label, 
                     float maxTemp, WX_Timestamp *maxTs,
                     float minTemp, WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minTemp*1.8+32);
     printTimestamp(fd, minTs);
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxTemp*1.8+32);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printMaxMinRelHum(FILE *fd,char *label,int maxHum,WX_Timestamp *maxTs,int minHum,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "   %d%%  ", minHum);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "    %d%%  ", maxHum);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printMaxMinDewpoint(FILE *fd, char *label,float maxDewpoint,WX_Timestamp *maxTs,float minDewpoint,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minDewpoint*1.8+32);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxDewpoint*1.8+32);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}

void printMaxMinWindSpeed(FILE *fd,char *label,float maxSpeed,WX_Timestamp *maxTs,float minSpeed,WX_Timestamp *minTs)
{
  if (isTimestampPresent(maxTs) || isTimestampPresent(minTs)) {
    fprintf(fd, "%s",label);
    if (isTimestampPresent(minTs)) {
     fprintf(fd, "  %4.1f  ", minSpeed);
     printTimestamp(fd, minTs); 
    }
    else
     fprintf(fd, "                        ");
    if (isTimestampPresent(maxTs)) {
     fprintf(fd, "   %4.1f  ", maxSpeed);
     printTimestamp(fd, maxTs); 
    }
    fprintf(fd, "\n");
  }
}
void printTimestamp(FILE *fd, WX_Timestamp *ts) {
  struct tm *localtm = localtime(&ts->timet);
  fprintf(fd, "Date: %02d/%02d/%04d Time: %02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min);      
}

static void printSensorStatus(FILE *fd,char *str, int lock_code, int lock_code_change_count, int no_data_for_180_secs, int no_data_between_snapshots, WX_Timestamp *ts);
static void printExtraSensorStatus(FILE *fd, int sensorIdx);

void WX_DumpSensorInfo(FILE *fd)
{ 
   printTimeDateAndUptime(fd);

   fprintf(fd, "   Messages Processed: %d     Messages With Errors: %d\n",wxData.currentTime.PktCnt, wxData.BadPktCnt);
   
   if (wxData.UnsupportedPktCnt > 0)
     fprintf(fd, "   Count of snapshots without new data: %d\n",wxData.noDataBetweenSnapshots);
   if (wxData.UnsupportedPktCnt > 0)
     fprintf(fd, "   Unsupported Packets: %d\n",wxData.UnsupportedPktCnt);
              
   fprintf(fd,"\n                   Lock  Lock  Code  300 sec.  Snapshot\n");
     fprintf(fd,"   Sensor Name     Code  Mismatches  Timeouts  Timeouts  Last Valid Message Received\n\n");

   char label[80];
   if (WxConfig.oduNameString[0] != 0)
      sprintf(label,"   %s (ODU) ",WxConfig.oduNameString);
   else
      sprintf(label,"   Outdoor Unit  ");
   printSensorStatus(fd, label, wxData.odu.LockCode, wxData.odu.LockCodeMismatchCount, 
             wxData.odu.noDataFor300Seconds, wxData.odu.noDataBetweenSnapshots, &wxData.odu.Timestamp);
   
   if (WxConfig.iduNameString[0] != 0)
      sprintf(label,"   %s (IDU) ",WxConfig.iduNameString);
   else
      sprintf(label,"   Indoor Unit   ");
   printSensorStatus(fd,label, wxData.idu.LockCode, wxData.idu.LockCodeMismatchCount, 
             wxData.idu.noDataFor300Seconds, wxData.idu.noDataBetweenSnapshots, &wxData.idu.Timestamp);
   
   int sensorIdx;
   for (sensorIdx=0;sensorIdx<=MAX_SENSOR_CHANNEL_INDEX;sensorIdx++)
      printExtraSensorStatus(fd, sensorIdx);
        
   printSensorStatus(fd,"   Wind Gauge    ", wxData.wg.LockCode,  wxData.wg.LockCodeMismatchCount,  
          wxData.wg.noDataFor300Seconds, wxData.wg.noDataBetweenSnapshots, &wxData.wg.Timestamp);
   printSensorStatus(fd,"   Rain Gauge    ", wxData.rg.LockCode,  wxData.rg.LockCodeMismatchCount,  
          wxData.rg.noDataFor300Seconds, wxData.rg.noDataBetweenSnapshots, &wxData.rg.Timestamp);
   printSensorStatus(fd,"   Efergy Sensor ", wxData.energy.LockCode,  wxData.energy.LockCodeMismatchCount,  
          wxData.energy.noDataFor300Seconds, wxData.energy.noDataBetweenSnapshots, &wxData.energy.Timestamp);     
   printSensorStatus(fd,"   OWL119 Sensor ", wxData.owl.LockCode,  wxData.owl.LockCodeMismatchCount,  
          wxData.owl.noDataFor300Seconds, wxData.owl.noDataBetweenSnapshots, &wxData.owl.Timestamp);      
   if (WxConfig.sensorLockingEnabled)
     fprintf(fd, "\n   Sensor Locking is ENABLED (edit rtl-wx.conf to change)\n\n");
   else
     fprintf(fd, "\n   Sensor Locking is DISABLED (edit rtl-wx.conf to change)\n\n");
}

void printSensorStatus(FILE *fd,char *str, int lock_code, int lock_code_change_count, int no_data_for_180_secs, int no_data_between_snapshots, WX_Timestamp *ts)
{
  if (ts->PktCnt != 0) {
    fprintf(fd,"%s  ", str);
   if (lock_code == -1)
      fprintf(fd,"                                      ");
   else
      fprintf(fd,"0x%02x     %3d        %3d       %3d     ", lock_code, lock_code_change_count, 
                                                         no_data_for_180_secs, no_data_between_snapshots);
 
   struct tm *localtm = localtime(&ts->timet);
   fprintf(fd, "%02d/%02d/%04d at %02d:%02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min, localtm->tm_sec);      
   fprintf(fd," (Msg# %d)\n", ts->PktCnt);
   }
}

void printExtraSensorStatus(FILE *fd, int sensorIdx) {
  char label[80];
  if (WxConfig.extNameStrings[sensorIdx][0] != 0)
      sprintf(label,"   %s (Ch%2d)",WxConfig.extNameStrings[sensorIdx], sensorIdx+1);
  else
      sprintf(label,"   Ext Sensor %2d ", sensorIdx+1);
  printSensorStatus(fd,label, wxData.ext[sensorIdx].LockCode, wxData.ext[sensorIdx].LockCodeMismatchCount, 
      wxData.ext[sensorIdx].noDataFor300Seconds, wxData.ext[sensorIdx].noDataBetweenSnapshots, &wxData.ext[sensorIdx].Timestamp);
}

void WX_DumpConfigInfo(FILE *fd)
{
 int i;
 char passwdStr[21];
 int configFileReadFrequency;

 fprintf(fd, "    sensorLockingEnabled: %d\n",WxConfig.sensorLockingEnabled);
 fprintf(fd, "          altitudeInFeet: %d\n",WxConfig.altitudeInFeet);
 fprintf(fd, "     fuelBurnerOnWattage: %d\n",WxConfig.fuelBurnerOnWattageThreshold);
 fprintf(fd, "    burnerGallonsPerHour: %4.2f\n",WxConfig.fuelBurnerGallonsPerHour);
 fprintf(fd, " configFileReadFrequency: %d\n",WxConfig.configFileReadFrequency);
 fprintf(fd, "   dataSnapshotFrequency: %d\n",WxConfig.dataSnapshotFrequency);
 fprintf(fd, " webcamSnapshotFrequency: %d\n\n",WxConfig.webcamSnapshotFrequency);
 fprintf(fd, "   tagFileParseFrequency: %d\n",WxConfig.tagFileParseFrequency);
 fprintf(fd, "      NumTagFilesToParse: %d\n",WxConfig.NumTagFilesToParse);
 for (i=0;i<WxConfig.NumTagFilesToParse;i++)
    fprintf(fd, "                    %-15s -> %s\n",WxConfig.tagFiles[i].inFile, WxConfig.tagFiles[i].outFile);
 fprintf(fd, "         realtimeCsvFile: Rewrite %s every %d minute(s)\n",
               WxConfig.realtimeCsvFile,WxConfig.realtimeCsvWriteFrequency);
 fprintf(fd, "     numCsvFilesToUpdate: %d\n",WxConfig.numCsvFilesToUpdate);
 for (i=0;i<WxConfig.numCsvFilesToUpdate;i++)
    fprintf(fd, "                    update %s every %d snapshot(s)\n",WxConfig.csvFiles[i].fname, WxConfig.csvFiles[i].snapshotsBetweenUpdates);
 fprintf(fd, "\n");
 fprintf(fd, "      ftpUploadFrequency: %d\n",WxConfig.ftpUploadFrequency);
 fprintf(fd, "       ftpServerHostname: %s\n",WxConfig.ftpServerHostname);
 fprintf(fd, "       ftpServerUsername: %s\n",WxConfig.ftpServerUsername);
 strncpy(passwdStr, WxConfig.ftpServerPassword,20);
 passwdStr[20]=0;
 for (i=2;i<(int) strlen(passwdStr);i++)
    passwdStr[i] = '*';
 fprintf(fd, "       ftpServerPassword: %s\n", passwdStr);
 fprintf(fd, "           numFilesToFtp: %d\n",WxConfig.numFilesToFtp);
 for (i=0;i<WxConfig.numFilesToFtp;i++) 
    fprintf(fd, "                    %-15s -> %s\n",WxConfig.ftpFiles[i].filename, WxConfig.ftpFiles[i].destpath);
 fprintf(fd, "\n");
 /*
 fprintf(fd, "      mailSendFrequency: %d\n",WxConfig.mailSendFrequency);
 fprintf(fd, "     mailServerHostname: %s\n",WxConfig.mailServerHostname);
 fprintf(fd, "     mailServerUsername: %s\n",WxConfig.mailServerUsername);
 fprintf(fd, "     mailServerPassword: %s\n",WxConfig.mailServerPassword);
 fprintf(fd, "      numMailMsgsToSend: %d\n",WxConfig.numMailMsgsToSend);
 for (i=0;i<WxConfig.numMailMsgsToSend;i++)
    fprintf(fd, "                         --%s-%s-%s--\n",WxConfig.mailMsgList[i].subject, 
                                       WxConfig.mailMsgList[i].recipients, WxConfig.mailMsgList[i].bodyFilename);
 fprintf(fd, "\n");
 */
}

void printTimeDateAndUptime(FILE *fd) {
   struct tm *localtm = localtime(&wxData.currentTime.timet);
   fprintf(fd, "   Date: %02d/%02d/%04d    Time: %02d:%02d:%02d",
      localtm->tm_mon+1, localtm->tm_mday, localtm->tm_year+1900, localtm->tm_hour, localtm->tm_min, localtm->tm_sec);   

   time_t timeNow;
   time(&timeNow);
   long int seconds = difftime(timeNow, WX_programStartTime);
      
   int days = seconds / (60*60*24);
   seconds = seconds % (60*60*24);
   
   int hours = seconds / (60*60);
   seconds = seconds % (60*60);
   
   int minutes = seconds / 60;
   seconds = seconds % 60;
   
   if (days > 0)
     fprintf(fd,"    Uptime: %d days, %d hours, %d Minutes\n\n", days, hours, minutes);
   else
     fprintf(fd,"    Uptime: %d hours, %d Minutes\n\n", hours, minutes);
}

BOOL isTimestampPresent(WX_Timestamp *ts) {
  BOOL retVal=1;
  if (ts->PktCnt == 0) // If pkt count is non-zero - data has been stored
      retVal=0;
  return(retVal);
}


