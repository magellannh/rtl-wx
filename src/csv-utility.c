
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {

   if (argc < 4) {
     printf("This utility processes a rtl-wx CSV by rtl-wx \n");
     printf("by combining multiple source file lines per line of output\n");
     printf("  Usage: processCSV <sourcefile> <destfile> <lines of source per line of output>\n", argv[0]);
     printf("Example: processCSV rtl-wx-15min rtl-wx-2hr.csv 8\n", argv[0]);
     exit(0);
   }
   char *sourceFilename = argv[1];
   char *newFilename = argv[2];
   int numSamplesToInclude = atoi(argv[3]);
   

   FILE *newfd, *sourcefd;
   if ((newfd = fopen(newFilename, "w")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open %s file for writing.  Exiting...\n\n", newFilename);
      exit(1);     
   }
   if ((sourcefd = fopen(sourceFilename, "rt")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open %s file for reading.  Exiting...\n\n", sourceFilename);
      exit(1);
   }
   fprintf(newfd, "Time,efergyWatts,owlWatts,fuelGallonsBurned,oduTemp,oduDewpoint,iduTemp,iduDewpoint,ext1Temp,ext1Dewpoint,ext2Temp,ext2Dewpoint,ext3Temp,ext3Dewpoint,ext4Temp,ext4Dewpoint,iduSealevelPressure\n");
   
   // The new file is open for writing and header line was written and rtl-wx-15min.csv was successfully opened for reading.
   // The plan is to backfill data from rtl-wx-15min.csv into the new file by recreating the history using the correct time interval
   int timestampA, efergyWattsA=0, owlWattsA=0;
   float fuelGallonsA=0, oduTempA=0, oduDewpointA=0, iduTempA=0, iduDewpointA=0,
     ext1TempA=0, ext1DewpointA=0, ext2TempA=0, ext2DewpointA=0, 
     ext3TempA=0, ext3DewpointA=0, ext4TempA=0, ext4DewpointA=0, iduPressureA=0;
 //  "%lu,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n", 

   char sourceLine[500];
   int samples = 0;
   int foundStartSample = 0;
   int linesWritten=0, linesParsed=0, linesSkipped=0;
   while(fgets(sourceLine, 500, sourcefd) != NULL) {
     int timestamp, efergyWatts, owlWatts;
     float fuelGallons, oduTemp, oduDewpoint, iduTemp, iduDewpoint,
     ext1Temp, ext1Dewpoint, ext2Temp, ext2Dewpoint, ext3Temp, ext3Dewpoint, ext4Temp, ext4Dewpoint, iduPressure;
     int numRead = sscanf(sourceLine,"%lu,%d,%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
        &timestamp, &efergyWatts, &owlWatts, &fuelGallons, &oduTemp, &oduDewpoint, &iduTemp, &iduDewpoint,
        &ext1Temp, &ext1Dewpoint, &ext2Temp, &ext2Dewpoint, &ext3Temp, &ext3Dewpoint, &ext4Temp, &ext4Dewpoint, &iduPressure);
 
     time_t sampleTime = (time_t) timestamp;
     struct tm *localTime = localtime(&sampleTime);
     if ((numRead == 17) && (foundStartSample == 0)) {
          if ((localTime->tm_min == 0) && (localTime->tm_hour == 0))
	    foundStartSample = 1;
	//  else 
	//    printf("start time not found timestamp=%d min=%d hour=%d\n",timestamp, localTime->tm_min, localTime->tm_hour);
     }
     if ((numRead == 17) && (foundStartSample)) {
	samples++;
	linesParsed++;
	efergyWattsA += efergyWatts; fuelGallonsA += fuelGallons; owlWattsA+= owlWatts;
	iduTempA     += iduTemp; iduDewpointA += iduDewpoint; iduPressureA += iduPressure;
	oduTempA     += oduTemp; oduDewpointA += oduDewpoint;
	ext1TempA    += ext1Temp; ext1DewpointA += ext1Dewpoint; ext2TempA += ext2Temp; ext2DewpointA += ext2Dewpoint;
	ext3TempA    += ext3Temp; ext3DewpointA += ext3Dewpoint; ext4TempA += ext4Temp; ext4DewpointA += ext4Dewpoint;
	if (samples == numSamplesToInclude) {
		efergyWattsA /= samples; owlWattsA /= samples; iduTempA /= samples;
		iduDewpointA /= samples; iduPressureA /= samples; oduTempA /= samples;oduDewpointA /= samples;
		ext1TempA     /= samples; ext1DewpointA /= samples; ext2TempA /= samples; ext2DewpointA /= samples;     
		ext3TempA     /= samples; ext3DewpointA /= samples; ext4TempA /= samples; ext4DewpointA /= samples;
		//                                                     time efergy owl  fuel       odu                          idu                          ext1                        ext2                         ext3                       ext4                         pressure
		char lineToAppend[500];
		sprintf(lineToAppend,"%lu,%d,%d,%4.2f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%3.1f,%5.2f\n", 
		   timestamp+900,efergyWattsA,owlWattsA,fuelGallonsA, oduTempA, oduDewpointA, iduTempA, iduDewpointA,
		ext1TempA, ext1DewpointA, ext2TempA, ext2DewpointA, 
		ext3TempA, ext3DewpointA, ext4TempA, ext4DewpointA, iduPressureA);
		fprintf(newfd, lineToAppend);
		samples = 0;
		efergyWattsA=0; owlWattsA=0;fuelGallonsA=0; oduTempA=0;oduDewpointA=0;iduTempA=0;iduDewpointA=0;
		ext1TempA=0;ext1DewpointA=0;ext2TempA=0;ext2DewpointA=0; 
		ext3TempA=0;ext3DewpointA=0;ext4TempA=0;ext4DewpointA=0, iduPressureA=0;
	//printf("Writing line: timestamp=%d day=%d min=%d hour=%d\n",timestamp, localTime->tm_mday, localTime->tm_min, localTime->tm_hour);
		linesWritten++;
	}
     } else 
        linesSkipped++;
   }
   printf("Program Stats: Skipped %d lines.  Parsed %d lines.  Wrote %d lines\n", linesSkipped, linesParsed, linesWritten);
   fclose(sourcefd);
   fclose(newfd);  
   exit(0);
}