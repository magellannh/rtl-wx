
/*========================================================================
    
   TagProc.h

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
#ifndef __TAG_PROC_h
#define __TAG_PROC_h

/**************************************************************************************************************** 
TEXT PARSER WITH TAG SUBSTITUTION

 The text file processor (Tag Processor) can be used to read user specified files and generate an output file that copies the text
 from the input file, but replaces all recognized  tags with the appropriate value from data gathered
 from the weather station.
  
Tags are generally of the form: ^WXTAG_WG_AVGSPEED_K#21:25^
 Where:  "^WXTAG_"  identifies the tag to the software parsing routine
         "WG_" tells the parser to fetch Wind Guage data
               Some other sensor names are RG (rain gauge), ODU (outdoor unit), IDU (indoor unit) , EXTx (extra sensor)
         "AVGSPEED" tells the parser to fetch the average speed data reported from the wind gauge (there are many more fields)
         "_K" is an optional formatting control string and this tells the parser how to format the output - in this case, use Km/hr
         "#21:25" is optional and tells the parser to use data from historical records 21 to 25
               Records are recorded every 15 minutes.  Record 0 (default) is the most current, record 1 is 15 minutes old.
               Record 21 will retrieve data that is (21/4) = 5 hours and 15 minutes old.  Up to 96 historical records can be
               retrieved (96 is largest supported historical record number).
         "^" tells the parser that the tag is complete
  The parser will completely replace the tag (everything between and including the  '^' with the value referenced by the tag.  
  If the value is not available (eg data has not yet been received), the parser will return "--" in place of
  the value (this behavior can be controlled with a global tag - described later)

Please see the file WXTAGTest.in for the complete list of supported Tags and tag options.

*/

#define READ_BUFSIZE 1000
#define MAX_TAG_SIZE 100
#define MAX_TAG_OUTPUT_SIZE 500

// Parser control variables and paser data created and/or used during parsing operations.
typedef struct tag_ParserControlVars {

// Formatting Control setting to control how to handle no data condition
char formatForNoData;          // (D,O,B,Sxxx)   d:use dashes  0:use 0   b:leave blank  Sxxx:any string var length
char spacerForMultipleRecords; // any character used as a spacer when a range of records (ie 1:23 is requested

char NoDataStr[MAX_TAG_SIZE];

// All the pieces of the tag currently being processed.
char sensorToGetFrom[MAX_TAG_SIZE];
char fieldToGet[MAX_TAG_SIZE];
char formatControlStr[MAX_TAG_SIZE];

WX_Data *weatherDatap; // ptr to Complete set of weather station data for sample  that tag refers to
WX_Timestamp *ts;      // Pointer to timestamp info for object being referenced in current tag

// String buffer used to store tag replacement text that will be written in output file.
char outputStr[MAX_TAG_OUTPUT_SIZE];
FILE *outfd;
} ParserControlVars;

#endif
