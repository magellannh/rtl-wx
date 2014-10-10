
/*========================================================================

   DataStore.c
   
   This module contains routines to support storage of historical weather station data
   The functionality includes saving a preset number of complete weather station records
   as well as support for saving min and max values for selected data fields

   Data records are saved using a first in never out ring buffer.  Once the max number of
   records to store is reached, saving additional records causes the oldest record to be 
   discarded.  Data is never pulled out of the ring, but rather it is replaced by newer data.

   Data inside the ring can be referenced by requesting a specific historical record number.
   historical record 1 returns the newest record.  The larger the record number requested, the
   older the data.  The largest record number that can be requested is maxRecordCount

   There's also a separate rain data ring buffer.  This ring buffer works essentially the
   same way as the main data store ring buffer, but stores a simple int in each record
   which represents the rainfall (in mm) since the last record was captured.  This data
   is stored seperately to support different save frequency and storage amounts for
   this data (eg. able to store 1 week of data instead of just 1 day).
   
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
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include "rtl-wx.h"

// This ring buffer is a bit strange since the insert operation behaves exactly like a ring buffer, but there is no remove
// operation.  Once data goes in it stays, or is over-written when the ring fills around again.  Any data in the ring can be
// referenced, but the data can't be removed.
//
// Another unusual feature of this buffer is that it doesn't matter if there's already data there and
// there's no need to keep track of how much data is in the buffer.   Because of the unique records being stored, 
// all code processing those records must be able to deal with NULL or empty records already, so this ring buffer
// code doesn't need to check for data underflow or a reference to uninitialized data (all buffer data is preset to 0).

static WX_Data *ringBuffer = (WX_Data *) 0;
static WX_Data minData, maxData;
static unsigned int maxRecordCount=0;
static unsigned int inIndex=0;
static unsigned int pktCntAtLastSnapshot=0;

static void updateMinData(WX_Data *weatherDatap);
static void updateMaxData(WX_Data *weatherDatap);

// A separate ring buffer is used for Rain Data storage  may be done at different frequency and
// have different record count than other weather data storage
static unsigned int *rainRingBuffer = (unsigned int *) 0;
static unsigned int maxRainRecordCount=0;
static unsigned int rainTotalAtLastSave=0;
static unsigned int rainInitComplete=0;
static unsigned int rainInIndex=0;

//--------------------------------------------------------------------------------------------------------------------------------------------
// Init vars used to track historical weather station information.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_InitHistoricalWeatherData(int numberOfRecordsToStore)
{
  char *str;
  if (ringBuffer != (WX_Data *) 0) // free up any old storage
     free(ringBuffer);

  ringBuffer = (WX_Data *)  malloc(numberOfRecordsToStore * sizeof(WX_Data));

  memset(ringBuffer,0, numberOfRecordsToStore * sizeof(WX_Data));
  memset(&minData, 0, sizeof(WX_Data));
  memset(&maxData, 0, sizeof(WX_Data));
  
  pktCntAtLastSnapshot = 0;

  maxRecordCount = numberOfRecordsToStore;
  inIndex=0;
}

void WX_InitHistoricalMaxMinData(void)
{
  memset(&minData, 0, sizeof(WX_Data));
  memset(&maxData, 0, sizeof(WX_Data));
  
  // This is a bit of a kludge, but reset max/min also resets the cumulative burner time time counter
  WX_totalBurnerRunSeconds=0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Init vars used to track historical weather station information.
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_InitHistoricalRainData(int numberOfRainRecordsToStore)
{
  if (rainRingBuffer != (unsigned int *) 0) // free up any old storage
     free(rainRingBuffer);

  rainRingBuffer = (unsigned int *)  malloc(numberOfRainRecordsToStore * sizeof(unsigned int));

  memset(rainRingBuffer,0, numberOfRainRecordsToStore * sizeof(unsigned int));
  
  maxRainRecordCount = numberOfRainRecordsToStore;
  rainTotalAtLastSave=0;
  rainInitComplete=0;
  rainInIndex=0;
}

static int checkSensorForSnaphotTimeout(WX_Data *weatherDatap, WX_Timestamp *ts, int minutesPerSnapshot) {
  long secondsSinceLastMessage = difftime(weatherDatap->currentTime.timet, ts->timet);
  
  // Only record timeout if sensor is active
  if ((ts->PktCnt > 0) && (secondsSinceLastMessage >= (60*minutesPerSnapshot)))
     return 1;
  else
     return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Save a weather station dataset to the datastore by copying the contents into the ring buffer
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_SaveWeatherDataRecord(WX_Data *weatherDatap, WX_ConfigSettings *cVarp, int minutesPerSnapshot)
{
  if (ringBuffer == (WX_Data *) 0) {
   DPRINTF("WX_SaveWeatherData called before initialization\n");
   return;
  }

  updateMinData(weatherDatap);
  updateMaxData(weatherDatap);
  
  if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->idu.Timestamp, minutesPerSnapshot))
      weatherDatap->idu.noDataBetweenSnapshots++;
  if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->odu.Timestamp, minutesPerSnapshot))
      weatherDatap->odu.noDataBetweenSnapshots++;
  if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->rg.Timestamp, minutesPerSnapshot))
      weatherDatap->rg.noDataBetweenSnapshots++;
  if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->wg.Timestamp, minutesPerSnapshot))
      weatherDatap->wg.noDataBetweenSnapshots++;
   
  int i;
  for(i=0;i<=MAX_SENSOR_CHANNEL_INDEX;i++) {
    if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->ext[i].Timestamp, minutesPerSnapshot))
        weatherDatap->ext[i].noDataBetweenSnapshots++;
  }  
 
  if (weatherDatap->energy.Timestamp.PktCnt != 0) {
      if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->energy.Timestamp, minutesPerSnapshot)) {
          weatherDatap->energy.noDataBetweenSnapshots++;
	  weatherDatap->energy.WattsAvg = 0;
	  weatherDatap->energy.BurnerRuntimeSeconds = 0;
      } else {
	  // Compute wattsAvg for this sample before saving off snapshot
          int i;
	  int wattsSum=0; int wattsCount=0;
	  for (i=0;i<ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++) {
	    if (weatherDatap->energy.WattsHistory[i] != 0) {
		wattsSum += weatherDatap->energy.WattsHistory[i];
		wattsCount++;
	    }
	  }
	  if (wattsCount != 0)
	     weatherDatap->energy.WattsAvg = wattsSum/wattsCount;
	  else
	     weatherDatap->energy.WattsAvg = 0;
      }
  }
  if (weatherDatap->owl.Timestamp.PktCnt != 0) {
      if (checkSensorForSnaphotTimeout(weatherDatap, &weatherDatap->owl.Timestamp, minutesPerSnapshot)) {
          weatherDatap->owl.noDataBetweenSnapshots++;
	  weatherDatap->owl.WattsAvg = 0;
	  weatherDatap->owl.BurnerRuntimeSeconds = 0;
      } else {
	  // Compute wattsAvg and possibly "burner on time" for this sample before saving off the snapshot data
	  // Burner on time is an optional feature that works by having a dedicated OWL energy sensor on the oil/gas burner electric circuit
	  // If the wattage reported on that circuit is above a user configurable threshold, it is assumed that the burner is running and if the wattage
	  // is below that threshold it is assumed that the burner is off.  The logic below attempts to determine the total amount of time (in seconds)
	  // that the burner was running during the snapshot. 
          int i;
	  int wattsSum=0; int wattsCount=0;
	  int burnerRuntime=0; int burnerOnStartTime=0;
	  int secondsPerSample=(60/OWL_ENERGY_HISTORY_SAMPLES_PER_MINUTE);
	  int burnerOnThreshold=cVarp->fuelBurnerOnWattageThreshold;
	  if (burnerOnThreshold==0) 
		burnerOnThreshold=INT_MAX;
	  int foundSampleWithBurnerOff=0;
	  for (i=0;i<ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++) {
	    if (weatherDatap->owl.WattsHistory[i] != 0) {
		wattsSum += weatherDatap->owl.WattsHistory[i];
		wattsCount++;
		if (weatherDatap->owl.WattsHistory[i] <= burnerOnThreshold)
		   foundSampleWithBurnerOff=1;
		if (weatherDatap->owl.WattsHistory[i] > burnerOnThreshold) {
		   if (burnerOnStartTime == 0)
		      if (foundSampleWithBurnerOff == 1)
		         burnerOnStartTime = (i * secondsPerSample)+1;
		      else
		         burnerOnStartTime = 1;
		} else if (burnerOnStartTime != 0) {
		   burnerRuntime += ((i*secondsPerSample) - burnerOnStartTime);
		   burnerOnStartTime = 0;
		} 
	    }
	  }
	  if (burnerOnStartTime != 0)
	     burnerRuntime += ((ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT*secondsPerSample) - burnerOnStartTime);
	  if (wattsCount != 0)
	     weatherDatap->owl.WattsAvg = wattsSum/wattsCount;
	  else
	     weatherDatap->owl.WattsAvg = 0;
	  weatherDatap->owl.BurnerRuntimeSeconds = burnerRuntime;
	  WX_totalBurnerRunSeconds += burnerRuntime;

      }
  }
  
  // If current record has no new data at all (no pkts from any sensor), save an empty data record instead
  if (weatherDatap->currentTime.PktCnt == pktCntAtLastSnapshot) {
    memset(&ringBuffer[inIndex], 0, sizeof(WX_Data));
    ringBuffer[inIndex].currentTime = weatherDatap->currentTime;
    DPRINTF("Warning: No sensor messages were received between data snapshots\n");
    weatherDatap->noDataBetweenSnapshots++;
  }
  else {
	ringBuffer[inIndex] = *weatherDatap; // Copy the entire weather data structure into the buffer
	// Check if timestamp  wrapped into the next 15 minute interval before getting saved and if so, back it into the previous time interval
	// This allows the sample start time to be reliably determined by using (localTime->tm_min % 15)
	if (isTimestampPresent(&ringBuffer[inIndex].currentTime)) {
		struct tm *localTime = localtime(&ringBuffer[inIndex].currentTime.timet);
		if ((localTime->tm_min % 15) == 0) // If min is 00, 15, 30, or 45 
			ringBuffer[inIndex].currentTime.timet -= 60; // subtract 60 seconds from timestamp
	}
  }
  inIndex++;
  if ( inIndex >= maxRecordCount )
     inIndex = 0;

  // Keep track of packet counter at time of snapshot
  pktCntAtLastSnapshot = weatherDatap->currentTime.PktCnt;
  
  // Clear out energy sensor data after snapshot is saved...
  if (weatherDatap->energy.Timestamp.PktCnt != 0) {
          int i;
	  pthread_rwlock_wrlock(&energy_sample_array_rw_lock);
	  for (i=0;i<ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++)
	    weatherDatap->energy.WattsHistory[i] = 0;
          pthread_rwlock_unlock(&energy_sample_array_rw_lock);
  }
  if (weatherDatap->owl.Timestamp.PktCnt != 0) {
          int i;
	  pthread_rwlock_wrlock(&energy_sample_array_rw_lock);
	  for (i=0;i<ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++)
	    weatherDatap->owl.WattsHistory[i] = 0;
          pthread_rwlock_unlock(&energy_sample_array_rw_lock);
  }  
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Get a historical weather station data set.
// HowFarBackToGo specifies which historical record to retrieve, counting backwards starting at 1 for most recent
//--------------------------------------------------------------------------------------------------------------------------------------------
WX_Data *WX_GetWeatherDataRecord(int howFarBackToGo)
{
  int currentPosition;

  if (ringBuffer == (WX_Data *) 0) {
    DPRINTF("WX_GetWeatherDataRecord() called before initialization\n");
    return(( WX_Data *) 0);
  }

  // if howFarBackToGo is 0, return current record.
  if (howFarBackToGo == 0)
     return(&wxData);

  // Make howFarBackToGO  be 0 based instead of 1 based (but watch for out of bounds error)
  if (howFarBackToGo >= 1) 
     howFarBackToGo -= 1;

  if (ringBuffer == (WX_Data *) 0) // Not initialized
      return((WX_Data *) 0);

  // If next record goes in at index 0, last record in (record #1) can be found at index (maxRecordCount-1)
  // Don't worry about empty ring buffer case, since caller must be able to handle empty buffer being returned.
  if (inIndex == 0)
     currentPosition = maxRecordCount-1;
  else
     currentPosition = inIndex-1;

  if (howFarBackToGo <= currentPosition)
     return(&ringBuffer[currentPosition-howFarBackToGo]);
  else
     return(&ringBuffer[maxRecordCount - (howFarBackToGo-currentPosition)]);
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Save a weather station dataset to the datastore by copying the contents into the ring buffer
//--------------------------------------------------------------------------------------------------------------------------------------------
void WX_SaveRainDataRecord(WX_Data *weatherDatap)
{
  unsigned int rainTotal = weatherDatap->rg.Total;
  
  if (rainRingBuffer == (unsigned int *) 0) {
   DPRINTF("WX_SaveRainData called before initialization\n");
   return;
  }

  if (rainInitComplete == 0) {
    rainTotalAtLastSave=rainTotal; // At startup, have to save device total to avoid counting it as "new" rainfall
    rainInitComplete = 1;
  }
  if (rainTotal < rainTotalAtLastSave)
    rainRingBuffer[rainInIndex] = rainTotal;
  else
    rainRingBuffer[rainInIndex] = rainTotal-rainTotalAtLastSave;
  //DPRINTF("SavingRainDataRecord:  index: %d value: %d last value: %d\n",rainInIndex,rainRingBuffer[rainInIndex],rainTotalAtLastSave);
 
  rainTotalAtLastSave = rainTotal;
 
  rainInIndex++;
  if ( rainInIndex >= maxRainRecordCount )
     rainInIndex = 0;

}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Get a historical weather station RAIN data set.
// HowFarBackToGo specifies which historical record to retrieve, counting backwards starting at 1 for most recent
//--------------------------------------------------------------------------------------------------------------------------------------------
unsigned int WX_GetRainDataRecord(int howFarBackToGo)
{
  int currentPosition;

  if (rainRingBuffer == (unsigned int *) 0) {
    DPRINTF("WX_GetWeatherRainRecord() called before initialization\n");
    return(0);
  }

  // if howFarBackToGo is 0, return current record.
  if (howFarBackToGo == 0)
     return(wxData.rg.Total-rainTotalAtLastSave);

  // Make howFarBackToGO  be 0 based instead of 1 based (but watch for out of bounds error)
  if (howFarBackToGo >= 1) 
     howFarBackToGo -= 1;

  if (rainRingBuffer == (unsigned int *) 0) // Not initialized
      return(0);

  // If next record goes in at index 0, last record in (record #1) can be found at index (maxRecordCount-1)
  // Don't worry about empty ring buffer case, since caller must be able to handle empty buffer being returned.
  if (rainInIndex == 0)
     currentPosition = maxRainRecordCount-1;
  else
     currentPosition = rainInIndex-1;

  if (howFarBackToGo <= currentPosition)
     return(rainRingBuffer[currentPosition-howFarBackToGo]);
  else
     return(rainRingBuffer[maxRainRecordCount - (howFarBackToGo-currentPosition)]);
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Get a data set that contains all the minimum values for each data field (along with timestamps of when it happened)
//--------------------------------------------------------------------------------------------------------------------------------------------
WX_Data *WX_GetMinDataRecord()
{
  return (&minData);
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// Get a data set that contains all the minimum values for each data field (along with timestamps of when it happened)
//--------------------------------------------------------------------------------------------------------------------------------------------
WX_Data *WX_GetMaxDataRecord()
{
  return (&maxData);
}

int isNewFloatLower(float newxData, WX_Timestamp *newTs, float minData, WX_Timestamp *minTs)
{
   if (isTimestampPresent(newTs) == FALSE) // No new data to store...
     return (FALSE);
   else if (isTimestampPresent(minTs) == FALSE) // No old data, and new data is present
     return (TRUE);
   else if (newxData < minData)
     return (TRUE);
   else
     return (FALSE);
}
int isNewIntLower(int newxData, WX_Timestamp *newTs, int minData, WX_Timestamp *minTs)
{
   if (isTimestampPresent(newTs) == FALSE) // No new data to store...
     return (FALSE);
   else if (isTimestampPresent(minTs) == FALSE) // No old data, and new data is present
     return (TRUE);
   else if (newxData < minData)
     return (TRUE);
   else
     return (FALSE);
}
int getLowestHistoryWatts(int historyArray[]) {
   int i;
   int lowest=0xffff;
   for (i=0; i< ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++)
      if ((historyArray[i] != 0) && (historyArray[i] < lowest))
	lowest = historyArray[i];
   return lowest;
}
int getHighestHistoryWatts(int historyArray[]) {
   int i;
   int highest=0;
   for (i=0; i< ENERGY_HISTORY_SAMPLES_PER_SNAPSHOT;i++)
      if (historyArray[i] > highest)
	highest = historyArray[i];
   return highest;
}
//--------------------------------------------------------------------------------------------------------------------------------------------
// Given a new sample of weather station data, see if any of the historical min data values need to be updated.
//--------------------------------------------------------------------------------------------------------------------------------------------
void updateMinData(WX_Data *datap)
{
  int sensorIdx;

  int lowestWatts = getLowestHistoryWatts(datap->energy.WattsHistory);
  if (isNewIntLower(lowestWatts,  &datap->energy.Timestamp,
                    minData.energy.Watts, &minData.energy.Timestamp) == TRUE) {
    minData.energy.Watts = lowestWatts;
    minData.energy.Timestamp = datap->energy.Timestamp;
  }
  lowestWatts = getLowestHistoryWatts(datap->owl.WattsHistory);
  if (isNewIntLower(lowestWatts,  &datap->owl.Timestamp,
                    minData.owl.Watts, &minData.owl.Timestamp) == TRUE) {
    minData.owl.Watts = lowestWatts;
    minData.owl.Timestamp = datap->owl.Timestamp;
  } 
  if (isNewIntLower(datap->owl.BurnerRuntimeSeconds,  &datap->owl.Timestamp,
                    minData.owl.BurnerRuntimeSeconds, &minData.owl.Timestamp) == TRUE) {
    minData.owl.BurnerRuntimeSeconds = datap->owl.BurnerRuntimeSeconds;
    minData.owl.Timestamp = datap->owl.Timestamp;
  }
  if (isNewFloatLower(datap->wg.Speed,  &datap->wg.SpeedTimestamp,
                    minData.wg.Speed, &minData.wg.SpeedTimestamp) == TRUE) {
    minData.wg.Speed = datap->wg.Speed;
    minData.wg.SpeedTimestamp = datap->wg.SpeedTimestamp;
  }
  if (isNewFloatLower(datap->wg.AvgSpeed,  &datap->wg.AvgSpeedTimestamp,
                    minData.wg.AvgSpeed, &minData.wg.AvgSpeedTimestamp) == TRUE) {
    minData.wg.AvgSpeed = datap->wg.AvgSpeed;
    minData.wg.AvgSpeedTimestamp = datap->wg.AvgSpeedTimestamp;
  } 
  if (isNewIntLower(datap->rg.Rate,  &datap->rg.RateTimestamp,
                  minData.rg.Rate, &minData.rg.RateTimestamp) == TRUE) {
    minData.rg.Rate = datap->rg.Rate;
    minData.rg.RateTimestamp = datap->rg.RateTimestamp;
  } 
  if (isNewFloatLower(datap->odu.Temp,  &datap->odu.TempTimestamp,
                    minData.odu.Temp, &minData.odu.TempTimestamp) == TRUE) {
    minData.odu.Temp = datap->odu.Temp;
    minData.odu.TempTimestamp = datap->odu.TempTimestamp;
  } 
  if (isNewIntLower(datap->odu.RelHum,  &datap->odu.RelHumTimestamp,
                  minData.odu.RelHum, &minData.odu.RelHumTimestamp) == TRUE) {
    minData.odu.RelHum = datap->odu.RelHum;
    minData.odu.RelHumTimestamp = datap->odu.RelHumTimestamp;
  } 
  if (isNewFloatLower(datap->odu.Dewpoint,  &datap->odu.DewpointTimestamp,
                    minData.odu.Dewpoint, &minData.odu.DewpointTimestamp) == TRUE) {
    minData.odu.Dewpoint = datap->odu.Dewpoint;
    minData.odu.DewpointTimestamp = datap->odu.DewpointTimestamp;
  } 
  if (isNewFloatLower(datap->idu.Temp,  &datap->idu.TempTimestamp,
                    minData.idu.Temp, &minData.idu.TempTimestamp) == TRUE) {
    minData.idu.Temp = datap->idu.Temp;
    minData.idu.TempTimestamp = datap->idu.TempTimestamp;
  } 
  if (isNewIntLower(datap->idu.RelHum,  &datap->idu.RelHumTimestamp,
                  minData.idu.RelHum, &minData.idu.RelHumTimestamp) == TRUE) {
    minData.idu.RelHum = datap->idu.RelHum;
    minData.idu.RelHumTimestamp = datap->idu.RelHumTimestamp;
  } 
  if (isNewFloatLower(datap->idu.Dewpoint,  &datap->idu.DewpointTimestamp,
                    minData.idu.Dewpoint, &minData.idu.DewpointTimestamp) == TRUE) {
    minData.idu.Dewpoint = datap->idu.Dewpoint;
    minData.idu.DewpointTimestamp = datap->idu.DewpointTimestamp;
  } 
  if (isNewIntLower(datap->idu.Pressure,  &datap->idu.PressureTimestamp,
                  minData.idu.Pressure, &minData.idu.PressureTimestamp) == TRUE) {
    minData.idu.Pressure = datap->idu.Pressure;
    minData.idu.PressureTimestamp = datap->idu.PressureTimestamp;
  } 
  for (sensorIdx=0;sensorIdx <= MAX_SENSOR_CHANNEL_INDEX; sensorIdx++) {
   if (isNewFloatLower(datap->ext[sensorIdx].Temp,  &datap->ext[sensorIdx].TempTimestamp,
                     minData.ext[sensorIdx].Temp, &minData.ext[sensorIdx].TempTimestamp) == TRUE) {

    minData.ext[sensorIdx].Temp = datap->ext[sensorIdx].Temp;
    minData.ext[sensorIdx].TempTimestamp = datap->ext[sensorIdx].TempTimestamp;
   } 
   if (isNewIntLower(datap->ext[sensorIdx].RelHum,  &datap->ext[sensorIdx].RelHumTimestamp,
                  minData.ext[sensorIdx].RelHum, &minData.ext[sensorIdx].RelHumTimestamp) == TRUE) {
    minData.ext[sensorIdx].RelHum = datap->ext[sensorIdx].RelHum;
    minData.ext[sensorIdx].RelHumTimestamp = datap->ext[sensorIdx].RelHumTimestamp;
   } 
   if (isNewFloatLower(datap->ext[sensorIdx].Dewpoint,  &datap->ext[sensorIdx].DewpointTimestamp,
                    minData.ext[sensorIdx].Dewpoint, &minData.ext[sensorIdx].DewpointTimestamp) == TRUE) {
    minData.ext[sensorIdx].Dewpoint = datap->ext[sensorIdx].Dewpoint;
    minData.ext[sensorIdx].DewpointTimestamp = datap->ext[sensorIdx].DewpointTimestamp;
   } 
  }
}

int isNewFloatHigher(float newxData, WX_Timestamp *newTs, float maxData, WX_Timestamp *maxTs)
{
   if (isTimestampPresent(newTs) == FALSE) // No new data to store...
     return (FALSE);
   else if (isTimestampPresent(maxTs) == FALSE) // No old data, and new data is present
     return (TRUE);
   else if (newxData > maxData)
     return (TRUE);
   else
     return (FALSE);
}

int isNewIntHigher(int newxData, WX_Timestamp *newTs, int maxData, WX_Timestamp *maxTs)
{
   if (isTimestampPresent(newTs) == FALSE) // No new data to store...
     return (FALSE);
   else if (isTimestampPresent(maxTs) == FALSE) // No old data, and new data is present
     return (TRUE);
   else if (newxData > maxData)
     return (TRUE);
   else
     return (FALSE);
}
//--------------------------------------------------------------------------------------------------------------------------------------------
// Given a new sample of weather station data, see if any of the historical min data values need to be updated.
//--------------------------------------------------------------------------------------------------------------------------------------------
void updateMaxData(WX_Data *datap)
{
 int sensorIdx;

  int highestWatts = getHighestHistoryWatts(datap->energy.WattsHistory);
  if (isNewIntHigher(highestWatts,  &datap->energy.Timestamp,
                    maxData.energy.Watts, &maxData.energy.Timestamp) == TRUE) {
    maxData.energy.Watts = highestWatts;
    maxData.energy.Timestamp = datap->energy.Timestamp;
  }
  highestWatts = getHighestHistoryWatts(datap->owl.WattsHistory);
  if (isNewIntHigher(highestWatts,  &datap->owl.Timestamp,
                    maxData.owl.Watts, &maxData.owl.Timestamp) == TRUE) {
    maxData.owl.Watts = highestWatts;
    maxData.owl.Timestamp = datap->owl.Timestamp;
  }
  if (isNewIntHigher(datap->owl.BurnerRuntimeSeconds,  &datap->owl.Timestamp,
                    maxData.owl.BurnerRuntimeSeconds, &maxData.owl.Timestamp) == TRUE) {
    maxData.owl.BurnerRuntimeSeconds = datap->owl.BurnerRuntimeSeconds;
    maxData.owl.Timestamp = datap->owl.Timestamp;
  }   
 if (isNewFloatHigher(datap->wg.Speed,  &datap->wg.SpeedTimestamp,
                    maxData.wg.Speed, &maxData.wg.SpeedTimestamp) == TRUE) {
    maxData.wg.Speed = datap->wg.Speed;
    maxData.wg.SpeedTimestamp = datap->wg.SpeedTimestamp;
 }
 if (isNewFloatHigher(datap->wg.AvgSpeed,  &datap->wg.AvgSpeedTimestamp,
                    maxData.wg.AvgSpeed, &maxData.wg.AvgSpeedTimestamp) == TRUE) {
    maxData.wg.AvgSpeed = datap->wg.AvgSpeed;
    maxData.wg.AvgSpeedTimestamp = datap->wg.AvgSpeedTimestamp;
 } 
 if (isNewIntHigher(datap->rg.Rate,  &datap->rg.RateTimestamp,
                  maxData.rg.Rate, &maxData.rg.RateTimestamp) == TRUE) {
    maxData.rg.Rate = datap->rg.Rate;
    maxData.rg.RateTimestamp = datap->rg.RateTimestamp;
 } 
 if (isNewFloatHigher(datap->odu.Temp,  &datap->odu.TempTimestamp,
                    maxData.odu.Temp, &maxData.odu.TempTimestamp) == TRUE) {
    maxData.odu.Temp = datap->odu.Temp;
    maxData.odu.TempTimestamp = datap->odu.TempTimestamp;
 } 
 if (isNewIntHigher(datap->odu.RelHum,  &datap->odu.RelHumTimestamp,
                  maxData.odu.RelHum, &maxData.odu.RelHumTimestamp) == TRUE) {
    maxData.odu.RelHum = datap->odu.RelHum;
    maxData.odu.RelHumTimestamp = datap->odu.RelHumTimestamp;
 } 
 if (isNewFloatHigher(datap->odu.Dewpoint,  &datap->odu.DewpointTimestamp,
                    maxData.odu.Dewpoint, &maxData.odu.DewpointTimestamp) == TRUE) {
    maxData.odu.Dewpoint = datap->odu.Dewpoint;
    maxData.odu.DewpointTimestamp = datap->odu.DewpointTimestamp;
 } 
 if (isNewFloatHigher(datap->idu.Temp,  &datap->idu.TempTimestamp,
                    maxData.idu.Temp, &maxData.idu.TempTimestamp) == TRUE) {
    maxData.idu.Temp = datap->idu.Temp;
    maxData.idu.TempTimestamp = datap->idu.TempTimestamp;
 } 
 if (isNewIntHigher(datap->idu.RelHum,  &datap->idu.RelHumTimestamp,
                  maxData.idu.RelHum, &maxData.idu.RelHumTimestamp) == TRUE) {
    maxData.idu.RelHum = datap->idu.RelHum;
    maxData.idu.RelHumTimestamp = datap->idu.RelHumTimestamp;
 } 
 if (isNewFloatHigher(datap->idu.Dewpoint,  &datap->idu.DewpointTimestamp,
                    maxData.idu.Dewpoint, &maxData.idu.DewpointTimestamp) == TRUE) {
    maxData.idu.Dewpoint = datap->idu.Dewpoint;
    maxData.idu.DewpointTimestamp = datap->idu.DewpointTimestamp;
 } 
 if (isNewIntHigher(datap->idu.Pressure,  &datap->idu.PressureTimestamp,
                  maxData.idu.Pressure, &maxData.idu.PressureTimestamp) == TRUE) {
    maxData.idu.Pressure = datap->idu.Pressure;
    maxData.idu.PressureTimestamp = datap->idu.PressureTimestamp;
 } 
 for (sensorIdx=0;sensorIdx <= MAX_SENSOR_CHANNEL_INDEX; sensorIdx++) {
  if (isNewFloatHigher(datap->ext[sensorIdx].Temp,  &datap->ext[sensorIdx].TempTimestamp,
                     maxData.ext[sensorIdx].Temp, &maxData.ext[sensorIdx].TempTimestamp) == TRUE) {

    maxData.ext[sensorIdx].Temp = datap->ext[sensorIdx].Temp;
    maxData.ext[sensorIdx].TempTimestamp = datap->ext[sensorIdx].TempTimestamp;
  } 
 if (isNewIntHigher(datap->ext[sensorIdx].RelHum,  &datap->ext[sensorIdx].RelHumTimestamp,
                  maxData.ext[sensorIdx].RelHum, &maxData.ext[sensorIdx].RelHumTimestamp) == TRUE) {
    maxData.ext[sensorIdx].RelHum = datap->ext[sensorIdx].RelHum;
    maxData.ext[sensorIdx].RelHumTimestamp = datap->ext[sensorIdx].RelHumTimestamp;
  } 
 if (isNewFloatHigher(datap->ext[sensorIdx].Dewpoint,  &datap->ext[sensorIdx].DewpointTimestamp,
                    maxData.ext[sensorIdx].Dewpoint, &maxData.ext[sensorIdx].DewpointTimestamp) == TRUE) {
    maxData.ext[sensorIdx].Dewpoint = datap->ext[sensorIdx].Dewpoint;
    maxData.ext[sensorIdx].DewpointTimestamp = datap->ext[sensorIdx].DewpointTimestamp;
  } 
 }
}

void appendLineToCsvFile(char *fname, char *header_line, char *lineToAppend) {
   FILE *fd;
   
   // First test if file exists.  If it doesn't, create file and write header
   if ((fd = fopen(fname, "r")) == NULL) {
     if ((fd = fopen(fname, "w")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open %s file for writing.  Exiting...\n\n", fname);
      exit(1);     
     } else {
      fprintf(fd, header_line);
      fclose(fd);
     }
   }
   else
      fclose(fd);
      
   if ((fd = fopen(fname, "a")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open %s file for appending.  Exiting...\n\n", fname);
      exit(1);     
     } 
     
   fprintf(fd, lineToAppend);
   fclose(fd);  
}

// Append a line of sensor data to a CSV file by combining info from one or more saved data records
// numSamplesToInclude determines the time interval represented by each csv line.  (eg num samples of 1=15minutes, 4=1 hour, 16=4 hours, 96=1 day --> assuming 15 minutes per sample snapshot)
void WX_WriteSensorDataToCSVFile(char *csvFilename, WX_Data *weatherDatap, WX_ConfigSettings *cVarp, int numSamplesToInclude) {
   int efergySamples=0; int efergyWattsAvg=0; 
   int owlSamples=0; int owlWattsAvg=0; int burnerRuntimeSeconds=0; 
   int iduSamples=0; float iduTemp=0; float iduDewpoint=0; float iduSealevelPressure=0;
   int oduSamples=0; float oduTemp=0; float oduDewpoint=0;
   int extraSensorSamples[EXTRA_SENSOR_ARRAY_SIZE]; float extraSensorTemp[EXTRA_SENSOR_ARRAY_SIZE]; float extraSensorDewpoint[EXTRA_SENSOR_ARRAY_SIZE];
   
   int sensor, sample;
   for(sensor=0; sensor < EXTRA_SENSOR_ARRAY_SIZE; sensor++) {
      extraSensorSamples[sensor] = 0;
      extraSensorTemp[sensor] = 0;
      extraSensorDewpoint[sensor] = 0;      
   }
  
   for (sample=1;sample<=numSamplesToInclude;sample++) {
      WX_Data *wxDatap = WX_GetWeatherDataRecord(sample);
      if (wxDatap != NULL) {
         if (isTimestampPresent(&wxDatap->energy.Timestamp)) {
	   efergyWattsAvg += wxDatap->energy.WattsAvg;
	   burnerRuntimeSeconds += wxDatap->energy.BurnerRuntimeSeconds; // Update this for either sensor (only one will have data)
	   efergySamples++;
	 }
         if (isTimestampPresent(&wxDatap->owl.Timestamp)) {
	   owlWattsAvg += wxDatap->owl.WattsAvg;
	   burnerRuntimeSeconds += wxDatap->owl.BurnerRuntimeSeconds; 
	   owlSamples++;
	 }
         if (isTimestampPresent(&wxDatap->idu.Timestamp)) {
	   iduTemp += wxDatap->idu.Temp;
	   iduDewpoint += wxDatap->idu.Dewpoint;
	   iduSealevelPressure += ((wxDatap->idu.Pressure+wxDatap->idu.SeaLevelOffset)/33.8638866667);
	   iduSamples++;
	 }
         if (isTimestampPresent(&wxDatap->odu.Timestamp)) {
	   oduTemp += wxDatap->odu.Temp;
	   oduDewpoint += wxDatap->odu.Dewpoint;
	   oduSamples++;
	 }	 
         for(sensor=0; sensor < EXTRA_SENSOR_ARRAY_SIZE; sensor++)
            if (isTimestampPresent(&wxDatap->ext[sensor].Timestamp)) {
               extraSensorTemp[sensor] += wxDatap->ext[sensor].Temp;
               extraSensorDewpoint[sensor] += wxDatap->ext[sensor].Dewpoint;      
               extraSensorSamples[sensor] ++;      
            }
	}
   }
   
   if (efergySamples > 1)
     efergyWattsAvg /= efergySamples;
   if (owlSamples > 1)
     owlWattsAvg /= owlSamples;	 
   if (iduSamples > 0) {
      iduTemp = (iduTemp/iduSamples)*1.8+32;
      iduDewpoint = (iduDewpoint/iduSamples)*1.8+32;
      iduSealevelPressure /= iduSamples;
   }
   if (oduSamples > 0) {
      oduTemp = (oduTemp/oduSamples)*1.8+32;
      oduDewpoint = (oduDewpoint/oduSamples)*1.8+32;
   }
   for(sensor=0; sensor < EXTRA_SENSOR_ARRAY_SIZE; sensor++)
     if (extraSensorSamples[sensor] > 0) {
      extraSensorTemp[sensor] = (extraSensorTemp[sensor]/extraSensorSamples[sensor])*1.8+32;
      extraSensorDewpoint[sensor] = (extraSensorDewpoint[sensor]/extraSensorSamples[sensor])*1.8+32;
     }
     
   char lineToAppend[500];
   time_t timestamp = time(NULL);
   //                                                     time efergy owl  fuel       odu                          idu                          ext1                        ext2                         ext3                       ext4                         pressure
   sprintf(lineToAppend,"%lu,%d,%d,%4.2f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%5.2f\n", 
      timestamp,efergyWattsAvg,owlWattsAvg,(float) burnerRuntimeSeconds/3600 * cVarp->fuelBurnerGallonsPerHour,
      oduTemp,oduDewpoint,iduTemp,iduDewpoint,
      extraSensorTemp[0],extraSensorDewpoint[0],extraSensorTemp[1],extraSensorDewpoint[1],
      extraSensorTemp[2],extraSensorDewpoint[2],extraSensorTemp[3],extraSensorDewpoint[3],
      iduSealevelPressure);
   
   appendLineToCsvFile(csvFilename, 
     "Time,efergyWatts,owlWatts,fuelGallonsBurned,oduTemp,oduDewpoint,iduTemp,iduDewpoint,ext1Temp,ext1Dewpoint,ext2Temp,ext2Dewpoint,ext3Temp,ext3Dewpoint,ext4Temp,ext4Dewpoint,iduSealevelPressure\n",
     lineToAppend);
}

// Create a single record CSV file with the latest sensor data
void WX_WriteRealTimeCSVFile() {
   int efergyWatts = wxData.energy.Watts;
   int efergyWattsLastHour = getWattsAvgAvg(1, 4);
   int efergyWattsLastDay  = getWattsAvgAvg(1, 24*4);

   int owlWatts = wxData.owl.Watts;
   int owlWattsLastHour = getWattsAvgAvg(0, 4);
   int owlWattsLastDay = getWattsAvgAvg(0, 24*4);	 
   
   float fuelBurnedLastHour = (float) getBurnerRunSecondsTotal(0, 4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
   float fuelBurnedLastDay = (float) getBurnerRunSecondsTotal(0, 24*4) /(60*60) * WxConfig.fuelBurnerGallonsPerHour;
   float fuelBurnedTotal = (float) WX_totalBurnerRunSeconds/(60*60) * WxConfig.fuelBurnerGallonsPerHour;
	
   char csvString[500];
   time_t timestamp = time(NULL);
   //                                             time  efergy               owl                   fuel                                         odu                          idu                          ext1                        ext2                         ext3                       ext4                         pressure
   sprintf(csvString,"%lu,%d,%d,%d,%d,%d,%d,%4.2f,%4.2f,%4.2f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%5.2f\n", 
      timestamp,
      efergyWatts,efergyWattsLastHour,efergyWattsLastDay,
      owlWatts,owlWattsLastHour,owlWattsLastDay, 
      fuelBurnedLastHour,fuelBurnedLastDay,fuelBurnedTotal,
      wxData.odu.Temp*1.8+32, wxData.odu.Dewpoint*1.8+32, 
      wxData.idu.Temp*1.8+32, wxData.idu.Dewpoint*1.8+32, 
      wxData.ext[0].Temp*1.8+32, wxData.ext[0].Dewpoint*1.8+32, 
      wxData.ext[1].Temp*1.8+32, wxData.ext[1].Dewpoint*1.8+32, 
      wxData.ext[2].Temp*1.8+32, wxData.ext[2].Dewpoint*1.8+32, 
      wxData.ext[3].Temp*1.8+32, wxData.ext[3].Dewpoint*1.8+32, 
      (wxData.idu.Pressure + wxData.idu.SeaLevelOffset)/33.8638866667);
     
   FILE *fd; 
   char *fname = WxConfig.realtimeCsvFile;   
   if ((fd = fopen(fname, "w+")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open %s file for writing.  Exiting...\n\n", fname);
      exit(1);     
     } 
   fprintf(fd,"Time,efergyWatts,efergyLastHr,efergyLastDay,owlWatts,owlLastHr,owlLastDay,fuelGallonsLastHr,fuelLastDay,fuelTotal,oduTemp,oduDewpoint,iduTemp,iduDewpoint,ext1Temp,ext1Dewpoint,ext2Temp,ext2Dewpoint,ext3Temp,ext3Dewpoint,ext4Temp,ext4Dewpoint,iduSealevelPressure\n");
   fprintf(fd,csvString);
   fclose(fd); 
}





