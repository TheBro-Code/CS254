/*
 * Copyright (C) 2012-2014 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <makestuff.h>
#include <libfpgalink.h>
#include <libbuffer.h>
#include <liberror.h>
#include <libdump.h>
#include <argtable2.h>
#include <time.h>
#include <readline/readline.h>
#include <unistd.h>
#include <readline/history.h>
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif

bool sigIsRaised(void);
void sigRegisterHandler(void);

static const char *ptr;
static bool enableBenchmarking = false;

static bool isHexDigit(char ch) {
	return
		(ch >= '0' && ch <= '9') ||
		(ch >= 'a' && ch <= 'f') ||
		(ch >= 'A' && ch <= 'F');
}

static uint16 calcChecksum(const uint8 *data, size_t length) {
	uint16 cksum = 0x0000;
	while ( length-- ) {
		cksum = (uint16)(cksum + *data++);
	}
	return cksum;
}

static bool getHexNibble(char hexDigit, uint8 *nibble) {
	if ( hexDigit >= '0' && hexDigit <= '9' ) {
		*nibble = (uint8)(hexDigit - '0');
		return false;
	} else if ( hexDigit >= 'a' && hexDigit <= 'f' ) {
		*nibble = (uint8)(hexDigit - 'a' + 10);
		return false;
	} else if ( hexDigit >= 'A' && hexDigit <= 'F' ) {
		*nibble = (uint8)(hexDigit - 'A' + 10);
		return false;
	} else {
		return true;
	}
}

static int getHexByte(uint8 *byte) {
	uint8 upperNibble;
	uint8 lowerNibble;
	if ( !getHexNibble(ptr[0], &upperNibble) && !getHexNibble(ptr[1], &lowerNibble) ) {
		*byte = (uint8)((upperNibble << 4) | lowerNibble);
		byte += 2;
		return 0;
	} else {
		return 1;
	}
}

static const char *const errMessages[] = {
	NULL,
	NULL,
	"Unparseable hex number",
	"Channel out of range",
	"Conduit out of range",
	"Illegal character",
	"Unterminated string",
	"No memory",
	"Empty string",
	"Odd number of digits",
	"Cannot load file",
	"Cannot save file",
	"Bad arguments"
};

typedef enum {
	FLP_SUCCESS,
	FLP_LIBERR,
	FLP_BAD_HEX,
	FLP_CHAN_RANGE,
	FLP_CONDUIT_RANGE,
	FLP_ILL_CHAR,
	FLP_UNTERM_STRING,
	FLP_NO_MEMORY,
	FLP_EMPTY_STRING,
	FLP_ODD_DIGITS,
	FLP_CANNOT_LOAD,
	FLP_CANNOT_SAVE,
	FLP_ARGS
} ReturnCode;

static ReturnCode doRead(
	struct FLContext *handle, uint8 chan, uint32 length, FILE *destFile, uint16 *checksum,
	const char **error)
{
	ReturnCode retVal = FLP_SUCCESS;
	uint32 bytesWritten;
	FLStatus fStatus;
	uint32 chunkSize;
	const uint8 *recvData;
	uint32 actualLength;
	const uint8 *ptr;
	uint16 csVal = 0x0000;
	#define READ_MAX 65536

	// Read first chunk
	chunkSize = length >= READ_MAX ? READ_MAX : length;
	fStatus = flReadChannelAsyncSubmit(handle, chan, chunkSize, NULL, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");
	length = length - chunkSize;

	while ( length ) {
		// Read chunk N
		chunkSize = length >= READ_MAX ? READ_MAX : length;
		fStatus = flReadChannelAsyncSubmit(handle, chan, chunkSize, NULL, error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");
		length = length - chunkSize;

		// Await chunk N-1
		fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");

		// Write chunk N-1 to file
		bytesWritten = (uint32)fwrite(recvData, 1, actualLength, destFile);
		CHECK_STATUS(bytesWritten != actualLength, FLP_CANNOT_SAVE, cleanup, "doRead()");

		// Checksum chunk N-1
		chunkSize = actualLength;
		ptr = recvData;
		while ( chunkSize-- ) {
			csVal = (uint16)(csVal + *ptr++);
		}
	}

	// Await last chunk
	fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doRead()");

	// Write last chunk to file
	bytesWritten = (uint32)fwrite(recvData, 1, actualLength, destFile);
	CHECK_STATUS(bytesWritten != actualLength, FLP_CANNOT_SAVE, cleanup, "doRead()");

	// Checksum last chunk
	chunkSize = actualLength;
	ptr = recvData;
	while ( chunkSize-- ) {
		csVal = (uint16)(csVal + *ptr++);
	}

	// Return checksum to caller
	*checksum = csVal;
cleanup:
	return retVal;
}

static ReturnCode doWrite(
	struct FLContext *handle, uint8 chan, FILE *srcFile, size_t *length, uint16 *checksum,
	const char **error)
{
	ReturnCode retVal = FLP_SUCCESS;
	size_t bytesRead, i;
	FLStatus fStatus;
	const uint8 *ptr;
	uint16 csVal = 0x0000;
	size_t lenVal = 0;
	#define WRITE_MAX (65536 - 5)
	uint8 buffer[WRITE_MAX];

	do {
		// Read Nth chunk
		bytesRead = fread(buffer, 1, WRITE_MAX, srcFile);
		if ( bytesRead ) {
			// Update running total
			lenVal = lenVal + bytesRead;

			// Submit Nth chunk
			fStatus = flWriteChannelAsync(handle, chan, bytesRead, buffer, error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doWrite()");

			// Checksum Nth chunk
			i = bytesRead;
			ptr = buffer;
			while ( i-- ) {
				csVal = (uint16)(csVal + *ptr++);
			}
		}
	} while ( bytesRead == WRITE_MAX );

	// Wait for writes to be received. This is optional, but it's only fair if we're benchmarking to
	// actually wait for the work to be completed.
	fStatus = flAwaitAsyncWrites(handle, error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup, "doWrite()");

	// Return checksum & length to caller
	*checksum = csVal;
	*length = lenVal;
cleanup:
	return retVal;
}

static int parseLine(struct FLContext *handle, const char *line, const char **error) {
	ReturnCode retVal = FLP_SUCCESS, status;
	FLStatus fStatus;
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	uint8 *data = NULL;
	char *fileName = NULL;
	FILE *file = NULL;
	double totalTime, speed;
	#ifdef WIN32
		LARGE_INTEGER tvStart, tvEnd, freq;
		DWORD_PTR mask = 1;
		SetThreadAffinityMask(GetCurrentThread(), mask);
		QueryPerformanceFrequency(&freq);
	#else
		struct timeval tvStart, tvEnd;
		long long startTime, endTime;
	#endif
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	CHECK_STATUS(bStatus, FLP_LIBERR, cleanup);
	ptr = line;
	do {
		while ( *ptr == ';' ) {
			ptr++;
		}
		switch ( *ptr ) {
		case 'r':{
			uint32 chan;
			uint32 length = 1;
			char *end;
			ptr++;

			// Get the channel to be read:
			errno = 0;
			chan = (uint32)strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(chan > 127, FLP_CHAN_RANGE, cleanup);
			ptr = end;

			// Only three valid chars at this point:
			CHECK_STATUS(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR, cleanup);

			if ( *ptr == ' ' ) {
				ptr++;

				// Get the read count:
				errno = 0;
				length = (uint32)strtoul(ptr, &end, 16);
				CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);
				ptr = end;

				// Only three valid chars at this point:
				CHECK_STATUS(*ptr != '\0' && *ptr != ';' && *ptr != ' ', FLP_ILL_CHAR, cleanup);
				if ( *ptr == ' ' ) {
					const char *p;
					const char quoteChar = *++ptr;
					CHECK_STATUS(
						(quoteChar != '"' && quoteChar != '\''),
						FLP_ILL_CHAR, cleanup);

					// Get the file to write bytes to:
					ptr++;
					p = ptr;
					while ( *p != quoteChar && *p != '\0' ) {
						p++;
					}
					CHECK_STATUS(*p == '\0', FLP_UNTERM_STRING, cleanup);
					fileName = malloc((size_t)(p - ptr + 1));
					CHECK_STATUS(!fileName, FLP_NO_MEMORY, cleanup);
					CHECK_STATUS(p - ptr == 0, FLP_EMPTY_STRING, cleanup);
					strncpy(fileName, ptr, (size_t)(p - ptr));
					fileName[p - ptr] = '\0';
					ptr = p + 1;
				}
			}
			if ( fileName ) {
				uint16 checksum = 0x0000;

				// Open file for writing
				file = fopen(fileName, "wb");
				CHECK_STATUS(!file, FLP_CANNOT_SAVE, cleanup);
				free(fileName);
				fileName = NULL;

				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					status = doRead(handle, (uint8)chan, length, file, &checksum, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					status = doRead(handle, (uint8)chan, length, file, &checksum, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Read %d bytes (checksum 0x%04X) from channel %d at %f MiB/s\n",
						length, checksum, chan, speed);
				}
				CHECK_STATUS(status, status, cleanup);

				// Close the file
				fclose(file);
				file = NULL;
			} else {
				size_t oldLength = dataFromFPGA.length;
				bStatus = bufAppendConst(&dataFromFPGA, 0x00, length, error);
				CHECK_STATUS(bStatus, FLP_LIBERR, cleanup);
				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					fStatus = flReadChannel(handle, (uint8)chan, length, dataFromFPGA.data + oldLength, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					fStatus = flReadChannel(handle, (uint8)chan, length, dataFromFPGA.data + oldLength, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Read %d bytes (checksum 0x%04X) from channel %d at %f MiB/s\n",
						length, calcChecksum(dataFromFPGA.data + oldLength, length), chan, speed);
				}
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			}
			break;
		}
		case 'w':{
			unsigned long int chan;
			size_t length = 1, i;
			char *end, ch;
			const char *p;
			ptr++;

			// Get the channel to be written:
			errno = 0;
			chan = strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(chan > 127, FLP_CHAN_RANGE, cleanup);
			ptr = end;

			// There must be a space now:
			CHECK_STATUS(*ptr != ' ', FLP_ILL_CHAR, cleanup);

			// Now either a quote or a hex digit
		   ch = *++ptr;
			if ( ch == '"' || ch == '\'' ) {
				uint16 checksum = 0x0000;

				// Get the file to read bytes from:
				ptr++;
				p = ptr;
				while ( *p != ch && *p != '\0' ) {
					p++;
				}
				CHECK_STATUS(*p == '\0', FLP_UNTERM_STRING, cleanup);
				fileName = malloc((size_t)(p - ptr + 1));
				CHECK_STATUS(!fileName, FLP_NO_MEMORY, cleanup);
				CHECK_STATUS(p - ptr == 0, FLP_EMPTY_STRING, cleanup);
				strncpy(fileName, ptr, (size_t)(p - ptr));
				fileName[p - ptr] = '\0';
				ptr = p + 1;  // skip over closing quote

				// Open file for reading
				file = fopen(fileName, "rb");
				CHECK_STATUS(!file, FLP_CANNOT_LOAD, cleanup);
				free(fileName);
				fileName = NULL;

				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					status = doWrite(handle, (uint8)chan, file, &length, &checksum, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					status = doWrite(handle, (uint8)chan, file, &length, &checksum, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Wrote "PFSZD" bytes (checksum 0x%04X) to channel %lu at %f MiB/s\n",
						length, checksum, chan, speed);
				}
				CHECK_STATUS(status, status, cleanup);

				// Close the file
				fclose(file);
				file = NULL;
			} else if ( isHexDigit(ch) ) {
				// Read a sequence of hex bytes to write
				uint8 *dataPtr;
				p = ptr + 1;
				while ( isHexDigit(*p) ) {
					p++;
				}
				CHECK_STATUS((p - ptr) & 1, FLP_ODD_DIGITS, cleanup);
				length = (size_t)(p - ptr) / 2;
				data = malloc(length);
				dataPtr = data;
				for ( i = 0; i < length; i++ ) {
					getHexByte(dataPtr++);
					ptr += 2;
				}
				#ifdef WIN32
					QueryPerformanceCounter(&tvStart);
					fStatus = flWriteChannel(handle, (uint8)chan, length, data, error);
					QueryPerformanceCounter(&tvEnd);
					totalTime = (double)(tvEnd.QuadPart - tvStart.QuadPart);
					totalTime /= freq.QuadPart;
					speed = (double)length / (1024*1024*totalTime);
				#else
					gettimeofday(&tvStart, NULL);
					fStatus = flWriteChannel(handle, (uint8)chan, length, data, error);
					gettimeofday(&tvEnd, NULL);
					startTime = tvStart.tv_sec;
					startTime *= 1000000;
					startTime += tvStart.tv_usec;
					endTime = tvEnd.tv_sec;
					endTime *= 1000000;
					endTime += tvEnd.tv_usec;
					totalTime = (double)(endTime - startTime);
					totalTime /= 1000000;  // convert from uS to S.
					speed = (double)length / (1024*1024*totalTime);
				#endif
				if ( enableBenchmarking ) {
					printf(
						"Wrote "PFSZD" bytes (checksum 0x%04X) to channel %lu at %f MiB/s\n",
						length, calcChecksum(data, length), chan, speed);
				}
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
				free(data);
				data = NULL;
			} else {
				FAIL(FLP_ILL_CHAR, cleanup);
			}
			break;
		}
		case '+':{
			uint32 conduit;
			char *end;
			ptr++;

			// Get the conduit
			errno = 0;
			conduit = (uint32)strtoul(ptr, &end, 16);
			CHECK_STATUS(errno, FLP_BAD_HEX, cleanup);

			// Ensure that it's 0-127
			CHECK_STATUS(conduit > 255, FLP_CONDUIT_RANGE, cleanup);
			ptr = end;

			// Only two valid chars at this point:
			CHECK_STATUS(*ptr != '\0' && *ptr != ';', FLP_ILL_CHAR, cleanup);

			fStatus = flSelectConduit(handle, (uint8)conduit, error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			break;
		}
		default:
			FAIL(FLP_ILL_CHAR, cleanup);
		}
	} while ( *ptr == ';' );
	CHECK_STATUS(*ptr != '\0', FLP_ILL_CHAR, cleanup);

	dump(0x00000000, dataFromFPGA.data, dataFromFPGA.length);

cleanup:
	bufDestroy(&dataFromFPGA);
	if ( file ) {
		fclose(file);
	}
	free(fileName);
	free(data);
	if ( retVal > FLP_LIBERR ) {
		const int column = (int)(ptr - line);
		int i;
		fprintf(stderr, "%s at column %d\n  %s\n  ", errMessages[retVal], column, line);
		for ( i = 0; i < column; i++ ) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "^\n");
	}
	return retVal;
}

static const char *nibbles[] = {
	"0000",  // '0'
	"0001",  // '1'
	"0010",  // '2'
	"0011",  // '3'
	"0100",  // '4'
	"0101",  // '5'
	"0110",  // '6'
	"0111",  // '7'
	"1000",  // '8'
	"1001",  // '9'

	"XXXX",  // ':'
	"XXXX",  // ';'
	"XXXX",  // '<'
	"XXXX",  // '='
	"XXXX",  // '>'
	"XXXX",  // '?'
	"XXXX",  // '@'

	"1010",  // 'A'
	"1011",  // 'B'
	"1100",  // 'C'
	"1101",  // 'D'
	"1110",  // 'E'
	"1111"   // 'F'
};

char ack1[33] = "01010011010001010100111001000100"; // predefined ack1
char ack2[33] = "01001110010101010100010001000101"; // predefined ack2
char key[33] = "10001010011001011100101001010101"; // predefined key
// function to calculate number of ones in a string
int n1(char s1[]){
	int count = 0;
	for(int i = 0;i<32;i++){
		if(s1[i]=='1'){
			count++;
		}
	}
	return count;
}
// function to calculate xor of 2 chars
char myxor(char a, char b){
	if(a==b){
		return '0';
	}
	else {return '1';}
}
// encrypt p using k and store it in c
void encryptText(char p[], char k[], char c[]){
	int n = n1(k);
	for(int i = 0;i<32;i++){
		c[i] = p[i];
	}
	c[32] = '\0';
	char t[5];
	t[0] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[0],k[4]),k[8]),k[12]),k[16]),k[20]),k[24]),k[28]);
	t[1] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[1],k[5]),k[9]),k[13]),k[17]),k[21]),k[25]),k[29]);
	t[2] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[2],k[6]),k[10]),k[14]),k[18]),k[22]),k[26]),k[30]);
	t[3] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[3],k[7]),k[11]),k[15]),k[19]),k[23]),k[27]),k[31]);
	t[4] = '\0';
	for(int i = 0;i<n;i++){
		for(int j = 0;j<32;j++){
			c[j] = myxor(c[j],t[j%4]);
		}
		if(t[3]=='0'){
			t[3] = '1';
		}
		else if(t[2]=='0'){
			t[3] = '0';
			t[2] = '1';
		}
		else if(t[1]=='0'){
			t[3] = '0';
			t[2] = '0';
			t[1] = '1';
		}
		else if(t[0]=='0'){
			t[3] = '0';
			t[2] = '0';
			t[1] = '0';
			t[0] = '1';
		}
		else{
			t[3] = '0';
			t[2] = '0';
			t[1] = '0';
			t[0] = '0';
		}
	}
}
// count number of zeros in a string
int n0(char s1[]){
	int count = 0;
	for(int i = 0;i<32;i++){
		if(s1[i]=='0'){
			count++;
		}
	}
	return count;
}
// decrypt c using k and store it in p
void decryptText(char c[], char k[], char p[]){
	int n = n0(k);
	for(int i = 0;i<32;i++){
		p[i] = c[i];
	}
	char t[5];
	p[32] = '\0';
	t[4] = '\0';
	t[0] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[0],k[4]),k[8]),k[12]),k[16]),k[20]),k[24]),k[28]);
	t[1] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[1],k[5]),k[9]),k[13]),k[17]),k[21]),k[25]),k[29]);
	t[2] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[2],k[6]),k[10]),k[14]),k[18]),k[22]),k[26]),k[30]);
	t[3] = myxor(myxor(myxor(myxor(myxor(myxor(myxor(k[3],k[7]),k[11]),k[15]),k[19]),k[23]),k[27]),k[31]);
	if(t[3]=='1'){
		t[3] = '0';
	}
	else if(t[2]=='1'){
		t[3] = '1';
		t[2] = '0';
	}
	else if(t[1]=='1'){
		t[3] = '1';
		t[2] = '1';
		t[1] = '0';
	}
	else if(t[0]=='1'){
		t[3] = '1';
		t[2] = '1';
		t[1] = '1';
		t[0] = '0';

	}
	else{
		t[3] = '1';
		t[2] = '1';
		t[1] = '1';
		t[0] = '1';
	}
	for(int i = 0;i<n;i++){
		for(int j = 0;j<32;j++){
			p[j] = myxor(p[j],t[j%4]);
		}
		if(t[3]=='1'){
			t[3] = '0';
		}
		else if(t[2]=='1'){
			t[3] = '1';
			t[2] = '0';
		}
		else if(t[1]=='1'){
			t[3] = '1';
			t[2] = '1';
			t[1] = '0';
		}
		else if(t[0]=='1'){
		t[3] = '1';
		t[2] = '1';
		t[1] = '1';
		t[0] = '0';
		}
		else{
		t[3] = '1';
		t[2] = '1';
		t[1] = '1';
		t[0] = '1';
		}
	}
}
// add characters corresponding to a byte in text string
void addChars(char* text, int input, int count){
  for(int i = 0;i<8;i++){
    if(input%2==0){
      text[8*count+7-i] = '0';
    }
    else{
      text[8*count+7-i] = '1';
    }
    input = input/2;
  }
}
// generate signal data from given coordinates
void generateSignalData(char* finalAnswer,const char *filename, int x, int y){
  FILE* stream = fopen(filename, "r");
  for(int i=0;i<8;i++)
  {
    finalAnswer[i*8] = '0';
    finalAnswer[i*8+1] = '0';
    switch (i) {
      case 0: finalAnswer[i*8+2] = '0'; finalAnswer[i*8+3] = '0';finalAnswer[i*8+4] = '0'; break;
      case 1: finalAnswer[i*8+2] = '0'; finalAnswer[i*8+3] = '0';finalAnswer[i*8+4] = '1'; break;
      case 2: finalAnswer[i*8+2] = '0'; finalAnswer[i*8+3] = '1';finalAnswer[i*8+4] = '0'; break;
      case 3: finalAnswer[i*8+2] = '0'; finalAnswer[i*8+3] = '1';finalAnswer[i*8+4] = '1'; break;
      case 4: finalAnswer[i*8+2] = '1'; finalAnswer[i*8+3] = '0';finalAnswer[i*8+4] = '0'; break;
      case 5: finalAnswer[i*8+2] = '1'; finalAnswer[i*8+3] = '0';finalAnswer[i*8+4] = '1'; break;
      case 6: finalAnswer[i*8+2] = '1'; finalAnswer[i*8+3] = '1';finalAnswer[i*8+4] = '0'; break;
      case 7: finalAnswer[i*8+2] = '1'; finalAnswer[i*8+3] = '1';finalAnswer[i*8+4] = '1'; break;
      default: ;
    }
    finalAnswer[i*8+7] = '0';
    finalAnswer[i*8+6] = '0';
    finalAnswer[i*8+5] = '0';
  }
  int x1,y1,dir,trackOk,nextHop;
  while(fscanf(stream, "%d,%d,%d,%d,%d\n", &x1, &y1, &dir, &trackOk, &nextHop) != EOF){
    if((x1==x)&&(y1==y)){
    	finalAnswer[dir*8] = '1';
      if(trackOk){
        finalAnswer[dir*8+1] = '1';
      }
      else{
        finalAnswer[dir*8+1] = '0';
      }
      for(int i = 7;i>4;i--){
        if(nextHop%2==1){
          finalAnswer[dir*8+i] = '1';
        }
        nextHop = nextHop/2;
      }
    }
  }
}
// main function
static void simulateTrack(struct FLContext *handle, const char **error){
	int i = 0, xCd, yCd;
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	size_t oldLength = dataFromFPGA.length;
	bool done = false;
	// loop to find channel
	while (!done) {
			char encryptedCds[33]; // char array to store the incoming encrypted coordinates
			encryptedCds[32] = '\0';
			// fill the encryptedCds array
			for(int j = 0;j<4;j++){
				fStatus = flReadChannel(handle, 2*i, 1, dataFromFPGA.data + oldLength, error);
				oldLength = dataFromFPGA.length;
				addChars(encryptedCds, calcChecksum(dataFromFPGA.data + oldLength, 1),3-j);
			}
			printf("Read encrypted coordinates : %s\n", encryptedCds);
			char receivedCds[33]; // char array to store decrypted coordinates
			decryptText(encryptedCds,key,receivedCds); // fill the receivedCds array
			printf("Received coordinates : %s\n", receivedCds);
			char reencryptedCds[33]; // char array to encrypt the receivedCds array
			xCd = 8*(receivedCds[0]-48) + 4*(receivedCds[1]-48) + 2*(receivedCds[2]-48) + (receivedCds[3]-48); // get xCd from the data received
			yCd = 8*(receivedCds[4]-48) + 4*(receivedCds[5]-48) + 2*(receivedCds[6]-48) + (receivedCds[7]-48); // get yCd from the data received
			printf("X coordinate : %d\n", xCd);
			printf("Y coordinate : %d\n", yCd);
			encryptText(receivedCds,key,reencryptedCds); // encrypt the receivedCds
			printf("Reencrypted received coordinates : %s\n", reencryptedCds);
			for(int j = 0;j<4;j++){
				uint8 y = 0;
				int two_power = 128;
				for(int k=0;k<8;k++)
				{
					y += ((reencryptedCds[8*(3-j)+k]-48)*two_power);
					two_power/=2;
				}
				fStatus = flWriteChannel(handle, 2*i+1, 1, &y, error);
			}
			printf("Wrote the reencrypted coordinates on board \n");
			sleep(0.001);
			char receivedAck[33];
			receivedAck[32] = '\0';
			for(int j = 0;j<4;j++){
				fStatus = flReadChannel(handle, 2*i, 1, dataFromFPGA.data + oldLength, error);
				addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-j);
				oldLength = dataFromFPGA.length;
			}
			printf("Received encrypted Ack %s\n", receivedAck);
			char decryptReceivedAck[33];
			decryptText(receivedAck,key,decryptReceivedAck);
			printf("Actual received Ack %s\n", decryptReceivedAck);
			done = true;
			for(int  j = 0;j<32;j++){
				if(decryptReceivedAck[j]!=ack1[j] && done){
					done = false;
				}
			}
			if(done){
				printf("Channel number is %d\n", i);
				break;
			}
			sleep(5);
			for(int j = 0;j<4;j++){
				fStatus = flReadChannel(handle, 2*i, 1, dataFromFPGA.data + oldLength, error);
				addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-j);
				oldLength = dataFromFPGA.length;
			}
			printf("Received encrypted Ack %s\n", receivedAck);
			decryptText(receivedAck,key,decryptReceivedAck);
			printf("Actual received Ack %s\n", decryptReceivedAck);
			done = true;
			for(int  j = 0;j<32;j++){
				if(decryptReceivedAck[j]!=ack1[j] && done){
					done = false;
				}
			}
			if(done){
				printf("Channel number is %d\n", i);
				break;
			}
			i = (i+1)%64;
			printf("Channel number is not %d\n", i);
	}
	char encryptedAck2[33];
	encryptText(ack2,key,encryptedAck2);
	printf("Sending encrypted ack2 : %s\n", encryptedAck2);
	for (int j = 0;j<4;j++){
		uint8 x = 0;
		int two_power = 128;
		for(int k=0;k<8;k++)
		{
			x+=((encryptedAck2[8*(3-j)+k]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,2*i+1,1,&x,error);
	}
	sleep(0.001);
	char signalData[65], signalData1[33], signalData2[33];
	char filename[] = "network.txt";
	generateSignalData(signalData, filename, xCd, yCd);
	printf("Genrated Singal Data : %s\n", signalData);
	for(int j = 0;j<32;j++){
		signalData1[j] = signalData[j];
		signalData2[j] = signalData[32+j];
	}
	signalData1[32] = '\0';
	signalData2[32] = '\0';
	char dataToSend[33];
	encryptText(signalData1,key,dataToSend);
	printf("Sent first 4 bytes encrypted : %s\n", dataToSend);
	for (int j = 0;j<4;j++){
		uint8 y = 0;
		int two_power = 128;
		for(int k=0;k<8;k++)
		{
			y+=((dataToSend[8*j+k]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,2*i+1,1,&y,error);
	}
	sleep(0.001);
	char receivedAck[33], decryptReceivedAck[33];
	receivedAck[32] = '\0';
	done = false;
	time_t start,now;
  time(&start);
	while(!done){
		for(int j = 0;j<4;j++){
			fStatus = flReadChannel(handle, 2*i, 1, dataFromFPGA.data + oldLength, error);
			addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-j);
			oldLength = dataFromFPGA.length;
		}
		printf("Received encrypted Ack : %s\n", receivedAck);
		decryptText(receivedAck,key,decryptReceivedAck);
		printf("Actual received Ack : %s\n", decryptReceivedAck);
		done = true;
		for(int  j = 0;j<32;j++){
			if(decryptReceivedAck[j]!=ack1[j] && done){
				done = false;
			}
		}
		if(done){
			printf("Ack1 correctly received\n");
		}
		time(&now);
		if(difftime(now,start)>256){
			printf("Timeout occured\n");
			break;
		}
	}
	if(!done){
		printf("Restarting...\n");
		simulateTrack(handle,error);
	}
	encryptText(signalData2,key,dataToSend);
	printf("Sent last 4 bytes encrypted : %s\n", dataToSend);
	for (int j = 0;j<4;j++){
		uint8 y = 0;
		int two_power = 128;
		for(int k=0;k<8;k++)
		{
			y+=((dataToSend[8*j+k]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,2*i+1,1,&y,error);
	}
	sleep(0.001);
	receivedAck[32] = '\0';
	done = false;
	time(&start);
	while(!done){
		for(int j = 0;j<4;j++){
			fStatus = flReadChannel(handle, 2*i, 1, dataFromFPGA.data + oldLength, error);
			addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-j);
			oldLength = dataFromFPGA.length;
		}
		printf("Received encrypted Ack : %s\n", receivedAck);
		decryptText(receivedAck,key,decryptReceivedAck);
		printf("Actual received Ack : %s\n", decryptReceivedAck);
		done = true;
		for(int  j = 0;j<32;j++){
			if(decryptReceivedAck[j]!=ack1[j] && done){
				done = false;
			}
		}
		if(done){
			printf("Ack1 correctly received\n");
		}
		time(&now);
		if(difftime(now,start)>256){
			printf("Timeout occured\n");
			break;
		}
	}
	if(!done){
		printf("Restarting...\n");
		simulateTrack(handle,error);
	}
	encryptText(ack2,key,encryptedAck2);
	printf("Sending encrypted ack2 : %s\n", encryptedAck2);
	for (int j = 0;j<4;j++){
		uint8 x = 0;
		int two_power = 128;
		for(int k=0;k<8;k++)
		{
			x+=((encryptedAck2[8*(3-j)+k]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,2*i+1,1,&x,error);
	}
	sleep(32);
	printf("Restarting...\n");
	simulateTrack(handle, error);
}

int main(int argc, char *argv[]) {
	ReturnCode retVal = FLP_SUCCESS, pStatus;
	struct arg_str *ivpOpt = arg_str0("i", "ivp", "<VID:PID>", "            vendor ID and product ID (e.g 04B4:8613)");
	struct arg_str *vpOpt = arg_str1("v", "vp", "<VID:PID[:DID]>", "       VID, PID and opt. dev ID (e.g 1D50:602B:0001)");
	struct arg_str *progOpt = arg_str0("p", "program", "<config>", "         program a device");
	struct arg_lit *railwaySimulation = arg_lit0("r", "railway",   "                  simulate railway track");
	struct arg_lit *shellOpt  = arg_lit0("s", "shell", "                    start up an interactive CommFPGA session");
	struct arg_lit *benOpt  = arg_lit0("b", "benchmark", "                enable benchmarking & checksumming");
	struct arg_lit *helpOpt  = arg_lit0("h", "help", "                     print this help and exit");
	struct arg_str *eepromOpt  = arg_str0(NULL, "eeprom", "<std|fw.hex|fw.iic>", "   write firmware to FX2's EEPROM (!!)");
	struct arg_str *backupOpt  = arg_str0(NULL, "backup", "<kbitSize:fw.iic>", "     backup FX2's EEPROM (e.g 128:fw.iic)\n");
	struct arg_end *endOpt   = arg_end(20);
	void *argTable[] = {
		ivpOpt, vpOpt, progOpt, railwaySimulation, shellOpt, benOpt, helpOpt, eepromOpt, backupOpt, endOpt
	};
	const char *progName = "flcli";
	int numErrors;
	struct FLContext *handle = NULL;
	FLStatus fStatus;
	const char *error = NULL;
	const char *ivp = NULL;
	const char *vp = NULL;
	bool isNeroCapable, isCommCapable;
	uint32 numDevices, scanChain[16], i;
	const char *line = NULL;
	uint8 conduit = 0x01;

	if ( arg_nullcheck(argTable) != 0 ) {
		fprintf(stderr, "%s: insufficient memory\n", progName);
		FAIL(1, cleanup);
	}

	numErrors = arg_parse(argc, argv, argTable);

	if ( helpOpt->count > 0 ) {
		printf("FPGALink Command-Line Interface Copyright (C) 2012-2014 Chris McClelland\n\nUsage: %s", progName);
		arg_print_syntax(stdout, argTable, "\n");
		printf("\nInteract with an FPGALink device.\n\n");
		arg_print_glossary(stdout, argTable,"  %-10s %s\n");
		FAIL(FLP_SUCCESS, cleanup);
	}

	if ( numErrors > 0 ) {
		arg_print_errors(stdout, endOpt, progName);
		fprintf(stderr, "Try '%s --help' for more information.\n", progName);
		FAIL(FLP_ARGS, cleanup);
	}

	fStatus = flInitialise(0, &error);
	CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);

	vp = vpOpt->sval[0];

	printf("Attempting to open connection to FPGALink device %s...\n", vp);
	fStatus = flOpen(vp, &handle, NULL);
	if ( fStatus ) {
		if ( ivpOpt->count ) {
			int count = 60;
			uint8 flag;
			ivp = ivpOpt->sval[0];
			printf("Loading firmware into %s...\n", ivp);
			fStatus = flLoadStandardFirmware(ivp, vp, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);

			printf("Awaiting renumeration");
			flSleep(1000);
			do {
				printf(".");
				fflush(stdout);
				fStatus = flIsDeviceAvailable(vp, &flag, &error);
				CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
				flSleep(250);
				count--;
			} while ( !flag && count );
			printf("\n");
			if ( !flag ) {
				fprintf(stderr, "FPGALink device did not renumerate properly as %s\n", vp);
				FAIL(FLP_LIBERR, cleanup);
			}

			printf("Attempting to open connection to FPGLink device %s again...\n", vp);
			fStatus = flOpen(vp, &handle, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		} else {
			fprintf(stderr, "Could not open FPGALink device at %s and no initial VID:PID was supplied\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	printf(
		"Connected to FPGALink device %s (firmwareID: 0x%04X, firmwareVersion: 0x%08X)\n",
		vp, flGetFirmwareID(handle), flGetFirmwareVersion(handle)
	);

	if ( eepromOpt->count ) {
		if ( !strcmp("std", eepromOpt->sval[0]) ) {
			printf("Writing the standard FPGALink firmware to the FX2's EEPROM...\n");
			fStatus = flFlashStandardFirmware(handle, vp, &error);
		} else {
			printf("Writing custom FPGALink firmware from %s to the FX2's EEPROM...\n", eepromOpt->sval[0]);
			fStatus = flFlashCustomFirmware(handle, eepromOpt->sval[0], &error);
		}
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}

	if ( backupOpt->count ) {
		const char *fileName;
		const uint32 kbitSize = strtoul(backupOpt->sval[0], (char**)&fileName, 0);
		if ( *fileName != ':' ) {
			fprintf(stderr, "%s: invalid argument to option --backup=<kbitSize:fw.iic>\n", progName);
			FAIL(FLP_ARGS, cleanup);
		}
		fileName++;
		printf("Saving a backup of %d kbit from the FX2's EEPROM to %s...\n", kbitSize, fileName);
		fStatus = flSaveFirmware(handle, kbitSize, fileName, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}
	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle, conduit);

	if ( progOpt->count ) {
		printf("Programming device...\n");
		if ( isNeroCapable ) {
			fStatus = flSelectConduit(handle, 0x00, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flProgram(handle, progOpt->sval[0], NULL, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		} else {
			fprintf(stderr, "Program operation requested but device at %s does not support NeroProg\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	if(railwaySimulation->count){
		printf("Beggining track simulation.\n");
		uint8 isRunning;
		fStatus = flSelectConduit(handle, conduit, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fStatus = flIsFPGARunning(handle, &isRunning, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		simulateTrack(handle, &error);
	}

	if ( shellOpt->count ) {
		printf("\nEntering CommFPGA command-line mode:\n");
		if ( isCommCapable ) {
		   uint8 isRunning;
			fStatus = flSelectConduit(handle, conduit, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if ( isRunning ) {
				do {
					do {
						line = readline("> ");
					} while ( line && !line[0] );
					if ( line && line[0] && line[0] != 'q' ) {
						add_history(line);
						pStatus = parseLine(handle, line, &error);
						CHECK_STATUS(pStatus, pStatus, cleanup);
						free((void*)line);
					}
				} while ( line && line[0] != 'q' );
			} else {
				fprintf(stderr, "The FPGALink device at %s is not ready to talk - did you forget --xsvf?\n", vp);
				FAIL(FLP_ARGS, cleanup);
			}
		} else {
			fprintf(stderr, "Shell requested but device at %s does not support CommFPGA\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

cleanup:
	free((void*)line);
	flClose(handle);
	if ( error ) {
		fprintf(stderr, "%s\n", error);
		flFreeError(error);
	}
	return retVal;
}
