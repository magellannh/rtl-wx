
/*========================================================================
    
	SlugWx.h
    
	This provides state machine structs for reading serial data coming from
	Oregon Scientific weather sensors.

	The extenal interface to this functionality can be found at the
	bottom of this file (the top bits are declarations for packet
	structures).
    
========================================================================*/
#ifndef __SLUGWX_h
#define __SLUGWX_h
#include <time.h>

#define CONFIG_FILE_PATH	"rtl-wx.conf"
#define LOG_FILE_PATH		"rtl-wx.log"
#define DEFAULT_WORKING_DIR	"../www"

typedef enum _BOOL { FALSE = 0, TRUE } BOOL;

//-------------------------------------------------------------------------------------------------------------------------------
// Define main structure for storage of weather data
//-------------------------------------------------------------------------------------------------------------------------------

// Timestamps - Timestamps are used in several places throughout the weather station monitoring program and 
// serve two distinct functions 1) They indicate whether a record has any data in it or is empty and 2) they
// indicate when the data for that record was gathered.
//
// The placement of timestamps on gathered data has been optimized to support some the historical data gathering
// functions of the software.  For example, even though it would be sufficient to timestamp a block of data that was
// received in one packet from a sensor, in some cases, each data item is individually marked with a timestamp.  This is
// to help support min/max data tracking using the same C structures and functions that manipulate the current data struct.  
typedef struct WX_timestamp
{
unsigned int	PktCnt;       // Value of global pkt count when timestamp was taken
unsigned int	ClockTickCnt; // Total number of elapsed minutes when timestamp taken
int		      Year;
unsigned char	Month;
unsigned char	Day;
unsigned char	Hour;
unsigned char	Minute;
} WX_Timestamp;


typedef struct WX_wind_gauge_data
{
WX_Timestamp Timestamp;
WX_Timestamp SpeedTimestamp;
WX_Timestamp AvgSpeedTimestamp;
BOOL		BatteryLow;
int	   LockCode;
int		LockCodeMismatchCount;
int		Bearing;	   //°
float		Speed;		//m/sec
float		AvgSpeed;	//m/sec
BOOL		ChillValid;	//not overrun and not invalid
int		WindChill;	//°C
} WX_WindGaugeData;

typedef struct WX_rain_gauge_gata
{
WX_Timestamp Timestamp;
WX_Timestamp RateTimestamp;
BOOL		BatteryLow;
int      LockCode;
int		LockCodeMismatchCount;
int	   Rate;		         //mm/hr
int		Total;		      //mm
int		TotalYesterday;	//mm
WX_Timestamp 	LastReset; 	// Date and time of last reset
} WX_RainGaugeData;

typedef struct WX_outdoor_unit_data
{
WX_Timestamp Timestamp;
WX_Timestamp TempTimestamp;
WX_Timestamp RelHumTimestamp;
WX_Timestamp DewpointTimestamp;
BOOL		BatteryLow;
int      LockCode;
int		LockCodeMismatchCount;
int		Channel;	   //(don't think this is relevant for this unit)
float		Temp;		   //°C
int		RelHum;		//%
float		Dewpoint;	//°C
} WX_OutdoorUnitData;

typedef struct WX_indoor_unit_data
{
WX_Timestamp Timestamp;
WX_Timestamp TempTimestamp;
WX_Timestamp RelHumTimestamp;
WX_Timestamp DewpointTimestamp;
WX_Timestamp PressureTimestamp;
BOOL		BatteryLow;
int   	LockCode;
int		LockCodeMismatchCount;
float		Temp;		      //°C
int		RelHum;		   //%
float		Dewpoint;	   //°C
int		Pressure;	   //mbar
char    	*ForecastStr;	//3 == rain, 6 == partly cloudy, 2 == cloudy, 12 == sunny
int		SeaLevelOffset;// mbar
} WX_IndoorUnitData;

typedef struct WX_extra_sensor_data
{
WX_Timestamp Timestamp;
WX_Timestamp TempTimestamp;
WX_Timestamp RelHumTimestamp;
WX_Timestamp DewpointTimestamp;
BOOL		BatteryLow;
int      LockCode;
int		LockCodeMismatchCount;
float		Temp;		   //°C
int		RelHum;		//%
float		Dewpoint;	//°C
} WX_ExtraSensorData;

typedef struct WX_minute_data
{
WX_Timestamp   Timestamp;
BOOL		      BatteryLow;
unsigned char  Minutes;
} WX_MinuteData;


typedef struct WX_date_time_data
{
// Date and time for this packet type are stored elsewhere so don't duplicate here.
unsigned int	PktCntAtLastUpdate;
BOOL		BatteryLow;
} WX_DateTimeData;


#define MAX_SENSOR_CHANNEL_INDEX 9
#define EXTRA_SENSOR_ARRAY_SIZE MAX_SENSOR_CHANNEL_INDEX+1

//Collection of latest data  received.
typedef struct WX_data
{
 WX_Timestamp currentTime;
 int BadPktCnt;
 int UnsupportedPktCnt;
 int ResyncCnt;
 int dataTimeoutCnt;
 
 WX_WindGaugeData wg;
 WX_RainGaugeData rg;
 WX_OutdoorUnitData odu;
 WX_IndoorUnitData idu;
 WX_ExtraSensorData ext[EXTRA_SENSOR_ARRAY_SIZE]; // indexes 0..9 are used
 WX_MinuteData m;
 WX_DateTimeData dt;
} WX_Data;

//-------------------------------------------------------------------------------------------------------------------------------
// Util.c routines
//-------------------------------------------------------------------------------------------------------------------------------

extern void WX_DumpInfo(FILE *fd);
extern void WX_DumpMaxMinInfo(FILE *fd);
extern void WX_DumpSensorInfo(FILE *fd);
extern void WX_DumpConfigInfo(FILE *fd);
extern BOOL isTimestampPresent(WX_Timestamp *ts);

//-------------------------------------------------------------------------------------------------------------------------------
// SlugWx.c routines and data
//-------------------------------------------------------------------------------------------------------------------------------

//the global collection of latest weather station data
extern WX_Data wxData;
extern time_t WX_programStartTime;
extern char WX_uptimeString[];

//-------------------------------------------------------------------------------------------------------------------------------
// TagProc.c routines
//-------------------------------------------------------------------------------------------------------------------------------
extern void WX_ReplaceTagsInTextFile(char *inFname, char *outFname);

//-------------------------------------------------------------------------------------------------------------------------------
// DataStore.c definitions
//-------------------------------------------------------------------------------------------------------------------------------

#define WX_NUM_RECORDS_TO_STORE 96 // default is 1 day at 4 records per hour
#define WX_NUM_RAIN_RECORDS_TO_STORE 168 // default is 1 week at 1 per hour
extern void WX_InitHistoricalWeatherData(int numberOfRecordsToStore);
extern void WX_InitHistoricalRainData(int numberOfRainRecordsToStore);
extern void WX_InitHistoricalMaxMinData(void);
extern void WX_SaveWeatherDataRecord(WX_Data *weatherDatap);
extern void WX_SaveRainDataRecord(WX_Data *weatherDatap);

extern WX_Data *WX_GetWeatherDataRecord(int howFarBackToGo);
extern unsigned int WX_GetRainDataRecord(int howFarBackToGo);
extern WX_Data *WX_GetMinDataRecord();
extern WX_Data *WX_GetMaxDataRecord();

//-------------------------------------------------------------------------------------------------------------------------------
//  Macro to control sending of status and debug messages to the remote client pipe and to the logfile
//-------------------------------------------------------------------------------------------------------------------------------

extern FILE *outputfd;  // Desc to send output to, could be stdout, or a pipe to client
extern FILE *logfd;     // Desc for program log file 

#define DPRINTF(...)  { \
char logPrintfStr[500]; time_t logprintftime = time(0); \
sprintf(logPrintfStr, __VA_ARGS__); \
if (logfd != NULL) { \
   char *logPrinTimeS = asctime(localtime(&logprintftime)); \
   if (strlen(logPrinTimeS) != 0) \
      logPrinTimeS[strlen(logPrinTimeS)-1] = 0; \
   fprintf(logfd,"%s %s",logPrinTimeS, logPrintfStr); \
   fflush(logfd); \
   } \
}

//-------------------------------------------------------------------------------------------------------------------------------
// Configuration (.conf) File Processor definitions
//-------------------------------------------------------------------------------------------------------------------------------

#define MAX_CONFIG_NAME_SIZE 500
#define MAX_CONFIG_LIST_SIZE 25

typedef struct _WX_TagFile {
 char inFile[MAX_CONFIG_NAME_SIZE];
 char outFile[MAX_CONFIG_NAME_SIZE];
} WX_TagFile;
typedef struct _WX_FtpFile {
 char filename[MAX_CONFIG_NAME_SIZE];
 char destpath[MAX_CONFIG_NAME_SIZE];
} WX_FtpFile;
typedef struct _WX_MailMessage {
 char recipients[MAX_CONFIG_NAME_SIZE];
 char subject[MAX_CONFIG_NAME_SIZE];
 char bodyFilename[MAX_CONFIG_NAME_SIZE];
} WX_MailMessage;

typedef struct _WX_ConfigSettings
{
 int sensorLockingEnabled;
 int altitudeInFeet;
 int configFileReadFrequency;
 int dataSnapshotFrequency;
 int rainDataSnapshotFrequency;

 int tagFileParseFrequency;
 int NumTagFilesToParse;
 WX_TagFile tagFiles[MAX_CONFIG_LIST_SIZE];

 int webcamSnapshotFrequency;

 int ftpUploadFrequency;		// Minutes
 char ftpServerHostname[MAX_CONFIG_NAME_SIZE];
 char ftpServerUsername[MAX_CONFIG_NAME_SIZE];
 char ftpServerPassword[MAX_CONFIG_NAME_SIZE];
 int  numFilesToFtp;
 WX_FtpFile ftpFiles[MAX_CONFIG_LIST_SIZE];
 
 int  mailSendFrequency;		// hours
 char mailServerHostname[MAX_CONFIG_NAME_SIZE];
 char mailServerUsername[MAX_CONFIG_NAME_SIZE];
 char mailServerPassword[MAX_CONFIG_NAME_SIZE];
 int numMailMsgsToSend; 
 WX_MailMessage mailMsgList[MAX_CONFIG_LIST_SIZE];

 char iduNameString[MAX_CONFIG_NAME_SIZE];
 char oduNameString[MAX_CONFIG_NAME_SIZE];
 char extNameStrings[EXTRA_SENSOR_ARRAY_SIZE][MAX_CONFIG_NAME_SIZE]; // sensor idx 0..9 used
 
} WX_ConfigSettings;

extern WX_ConfigSettings WxConfig;
extern int WX_processConfigSettingsFile(char *inFname, WX_ConfigSettings *cVarp);

//-------------------------------------------------------------------------------------------------------------------------------
// Scheduler  routines
//-------------------------------------------------------------------------------------------------------------------------------

// Init scheduler module that keep track of when to do ftp upload, file parsing, etc 
extern void WX_InitActionScheduler(WX_Data *weatherDatap, 
                                WX_ConfigSettings *configurationVarp);
extern void WX_DoScheduledActions();
extern void WX_DoConfigFileRead(void);
extern void WX_DoDataSnapshotSave(void);
extern void WX_DoRainDataSnapshotSave(void);
extern void WX_DoWebcamSnapshot(void);
extern void WX_DoTagFileProcessing(void);
extern int  WX_DoFtpUpload(void);
extern void WX_DumpSchedulerInfo(FILE *fd);

#endif
