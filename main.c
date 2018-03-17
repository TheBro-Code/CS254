#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <makestuff.h>
#include <libfpgalink.h>
#include <libbuffer.h>
#include <liberror.h>
#include <libdump.h>
#include <argtable2.h>
#include <readline/readline.h>
#include <readline/history.h>
#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#endif

char ack1[33] = "01010011010001010100111001000100";
char ack2[33] = "01001110010101010100010001000101";
char key[33] = "10001010011001011100101001010101";
char signalData[65];
int n1(char s1[]){
	int count = 0;
	for(int i = 0;i<32;i++){
		if(s1[i]=='1'){
			count++;
		}
	}
	return count;
}

char myxor(char a, char b){
	if(a==b){
		return '0';
	}
	else {return '1';}
}

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

void encryptedText(char p[], char k[], char c[]){
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

int n0(char s1[]){
	int count = 0;
	for(int i = 0;i<32;i++){
		if(s1[i]=='0'){
			count++;
		}
	}
	return count;
}

void decryptedText(char c[], char k[], char p[]){
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

void generateDirectionData(char* finalAnswer,const char *filename, int x, int y){
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
bool sigIsRaised(void);
void sigRegisterHandler(void);


char encryptedCds[33];
char Encrypted_Info[65];

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

static void writeToBoard(struct FLContext *handle, const char **error){
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	size_t oldLength = dataFromFPGA.length;
	char cd1[33];
	char cd2[33];
	cd1[32] ='\0';
	cd2[32] ='\0';
	for(int i=0;i<32;i++) cd1[i] = signalData[i];
	for(int i=0;i<32;i++) cd2[i] = signalData[32+i];
	char en1[33];
	char en2[33];
	char encryptedAck2[33];
	encryptedText(cd1,key,en1);
	encryptedText(cd2,key,en2);
	encryptedText(ack2,key,encryptedAck2);
	for (int i = 0;i<4;i++){
		uint8 x = 0;
		int two_power = 128;
		for(int j=0;j<8;j++)
		{
			x+=((encryptedAck2[8*(3-i)+j]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,1,1,&x,error);
	}
	printf("Ack2 sent\n");
	sleep(0.001);
	for (int i = 0;i<4;i++){
		uint8 y = 0;
		int two_power = 128;
		for(int j=0;j<8;j++)
		{
			y+=((en1[8*i+j]-48)*two_power);
			two_power/=2;
		}
		fStatus = flWriteChannel(handle,1,1,&y,error);
	}
	printf("First 4 bytes sent\n");
	sleep(0.001);
	char receivedAck[33];
	receivedAck[32] = '\0';
	for(int i = 0;i<4;i++){
		fStatus = flReadChannel(handle, 0, 1, dataFromFPGA.data + oldLength, error);
		addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-i);
		oldLength = dataFromFPGA.length;	
	}
	printf("Received encrypted Ack1 %s\n", receivedAck);
	char decryptReceivedAck[33];
	decryptedText(receivedAck,key,decryptReceivedAck);
	printf("Decrypted Ack1\n");
	bool correct = true;
	for(int  i = 0;i<32;i++){
		if(decryptReceivedAck[i]!=ack1[i] && correct){
			correct = false;
			printf("ACK1 not received\n");
		}
	}
	if(correct){
		for (int i = 0;i<4;i++){
			uint8 y = 0;
			int two_power = 128;
			for(int j=0;j<8;j++){
				y+=((en2[8*i+j]-48)*two_power);
				two_power/=2;
			}
			fStatus = flWriteChannel(handle,1,1,&y,error);
		}
		printf("Last 4 bytes sent\n");
		receivedAck[32] = '\0';
		sleep(0.001);
		for(int i = 0;i<4;i++){
			fStatus = flReadChannel(handle, 0, 1, dataFromFPGA.data + oldLength, error);
			addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-i);
			oldLength = dataFromFPGA.length;
		}
		printf("Received encrypted Ack1\n");
		decryptedText(receivedAck,key,decryptReceivedAck);
		printf("Decrypted Ack1\n");
		for(int  i = 0;i<32;i++){
			if(decryptReceivedAck[i]!=ack1[i] && correct){
				correct = false;
				printf("ACK1 not received\n");
			}
		}
	}
	if(correct){
		encryptedText(ack2,key,encryptedAck2);
		for (int i = 0;i<4;i++){
			uint8 x = 0;
			int two_power = 128;
			for(int j=0;j<8;j++){
				x+=((encryptedAck2[8*(3-i)+j]-48)*two_power);
				two_power/=2;
			}
			fStatus = flWriteChannel(handle,1,1,&x,error);
		}
		printf("Sent Ack2\n");
	}
}

static bool readFromBoard(struct FLContext *handle, const char **error){
	struct Buffer dataFromFPGA = {0,};
	BufferStatus bStatus;
	FLStatus fStatus;
	bStatus = bufInitialise(&dataFromFPGA, 1024, 0x00, error);
	size_t oldLength = dataFromFPGA.length;
	encryptedCds[32] = '\0';
	for(int i = 0;i<4;i++){
		fStatus = flReadChannel(handle, 0, 1, dataFromFPGA.data + oldLength, error);
		printf("%d\n", calcChecksum(dataFromFPGA.data + oldLength, 1));
		oldLength = dataFromFPGA.length;
		addChars(encryptedCds, calcChecksum(dataFromFPGA.data + oldLength, 1),3-i);
	}
	printf("Received encrypted coordinates : %s\n", encryptedCds);
	char Cds[33];
	decryptedText(encryptedCds,key,Cds);
	printf("Decrypted coordinates : %s\n", Cds);
	int xCd = 8*(Cds[0]-48) + 4*(Cds[1]-48) + 2*(Cds[2]-48) + (Cds[3]-48);
	int yCd = 8*(Cds[4]-48) + 4*(Cds[5]-48) + 2*(Cds[6]-48) + (Cds[7]-48);
	char textToEncrypt[33];
	textToEncrypt[32] = '\0';
	char encryptedCds2[33];
	textToEncrypt[32] = '\0';
	for(int i = 0;i<8;i++){
		textToEncrypt[i] = Cds[i];
	}
	for(int i = 8;i<32;i++){
		textToEncrypt[i] = '0';
	}
	encryptedText(textToEncrypt,key,encryptedCds2);
	for (int i = 0;i<4;i++){
		uint8 y = 0;
		int two_power = 128;
		for(int j=0;j<8;j++)
		{
			y += ((encryptedCds2[8*(3-i)+j]-48)*two_power);
			two_power/=2;
		}
		//printf("%hhu \n", &y);
		fStatus = flWriteChannel(handle, 1, 1, &y, error);
	}
	printf("Sent encrypted coordinates %s\n", encryptedCds2);
	sleep(0.001);
	char receivedAck[33];
	receivedAck[32] = '\0';
	for(int i = 0;i<4;i++){
		fStatus = flReadChannel(handle, 0, 1, dataFromFPGA.data + oldLength, error);
		addChars(receivedAck, calcChecksum(dataFromFPGA.data + oldLength, 1),3-i);
		oldLength = dataFromFPGA.length;
	}
	printf("Received encrypted Ack1 %s\n", receivedAck);
	char decryptReceivedAck[33];
	decryptedText(receivedAck,key,decryptReceivedAck);
	printf("Decrypted Ack1 %s \n", decryptReceivedAck);
	bool correct = true;
	for(int  i = 0;i<32;i++){
		if(decryptReceivedAck[i]!=ack1[i] && correct){
			correct = false;
			printf("ACK1 not received\n");
		}
	}
	char filename[] = "track_data.csv";
	generateDirectionData(signalData,filename,xCd,yCd);
	printf("Direction data generated\n");
	printf("%s\n", signalData);
	return correct;
}


static void ourFunction(struct FLContext *handle, const char **error){
	while(true){
		printf("==================================\n");
		printf("Entering our function\n");
		sleep(0.001);
		bool successfullyRead = readFromBoard(handle, error);
		if(successfullyRead){
			writeToBoard(handle, error);
		}
		sleep(16.001);
	}
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

int main(int argc, char *argv[]) {
	ReturnCode retVal = FLP_SUCCESS, pStatus;
	struct arg_str *ivpOpt = arg_str0("i", "ivp", "<VID:PID>", "            vendor ID and product ID (e.g 04B4:8613)");
	struct arg_str *vpOpt = arg_str1("v", "vp", "<VID:PID[:DID]>", "       VID, PID and opt. dev ID (e.g 1D50:602B:0001)");
	struct arg_str *fwOpt = arg_str0("f", "fw", "<firmware.hex>", "        firmware to RAM-load (or use std fw)");
	struct arg_str *portOpt = arg_str0("d", "ports", "<bitCfg[,bitCfg]*>", " read/write digital ports (e.g B13+,C1-,B2?)");
	struct arg_str *queryOpt = arg_str0("q", "query", "<jtagBits>", "         query the JTAG chain");
	struct arg_str *progOpt = arg_str0("p", "program", "<config>", "         program a device");
	struct arg_uint *conOpt = arg_uint0("c", "conduit", "<conduit>", "        which comm conduit to choose (default 0x01)");
	struct arg_str *actOpt = arg_str0("a", "action", "<actionString>", "    a series of CommFPGA actions");
	struct arg_lit *shellOpt  = arg_lit0("s", "shell", "                    start up an interactive CommFPGA session");
	struct arg_lit *theBroCodeOpt  = arg_lit0("t", "thebrocode", "                    execute railway track simulation");
	struct arg_lit *benOpt  = arg_lit0("b", "benchmark", "                enable benchmarking & checksumming");
	struct arg_lit *rstOpt  = arg_lit0("r", "reset", "                    reset the bulk endpoints");
	struct arg_str *dumpOpt = arg_str0("l", "dumploop", "<ch:file.bin>", "   write data from channel ch to file");
	struct arg_lit *helpOpt  = arg_lit0("h", "help", "                     print this help and exit");
	struct arg_str *eepromOpt  = arg_str0(NULL, "eeprom", "<std|fw.hex|fw.iic>", "   write firmware to FX2's EEPROM (!!)");
	struct arg_str *backupOpt  = arg_str0(NULL, "backup", "<kbitSize:fw.iic>", "     backup FX2's EEPROM (e.g 128:fw.iic)\n");
	struct arg_end *endOpt   = arg_end(20);
	void *argTable[] = {
		ivpOpt, vpOpt, fwOpt, portOpt, queryOpt, progOpt, conOpt, actOpt,
		shellOpt, benOpt, rstOpt, dumpOpt, helpOpt, eepromOpt, backupOpt, endOpt
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
			if ( fwOpt->count ) {
				fStatus = flLoadCustomFirmware(ivp, fwOpt->sval[0], &error);
			} else {
				fStatus = flLoadStandardFirmware(ivp, vp, &error);
			}
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

	if ( rstOpt->count ) {
		// Reset the bulk endpoints (only needed in some virtualised environments)
		fStatus = flResetToggle(handle, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
	}

	if ( conOpt->count ) {
		conduit = (uint8)conOpt->ival[0];
	}

	isNeroCapable = flIsNeroCapable(handle);
	isCommCapable = flIsCommCapable(handle, conduit);

	if ( portOpt->count ) {
		uint32 readState;
		char hex[9];
		const uint8 *p = (const uint8 *)hex;
		printf("Configuring ports...\n");
		fStatus = flMultiBitPortAccess(handle, portOpt->sval[0], &readState, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		sprintf(hex, "%08X", readState);
		printf("Readback:   28   24   20   16    12    8    4    0\n          %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf("  %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s", nibbles[*p++ - '0']);
		printf(" %s\n", nibbles[*p++ - '0']);
		flSleep(100);
	}

	if ( queryOpt->count ) {
		if ( isNeroCapable ) {
			fStatus = flSelectConduit(handle, 0x00, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = jtagScanChain(handle, queryOpt->sval[0], &numDevices, scanChain, 16, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if ( numDevices ) {
				printf("The FPGALink device at %s scanned its JTAG chain, yielding:\n", vp);
				for ( i = 0; i < numDevices; i++ ) {
					printf("  0x%08X\n", scanChain[i]);
				}
			} else {
				printf("The FPGALink device at %s scanned its JTAG chain but did not find any attached devices\n", vp);
			}
		} else {
			fprintf(stderr, "JTAG chain scan requested but FPGALink device at %s does not support NeroProg\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

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

	if ( benOpt->count ) {
		enableBenchmarking = true;
	}

	if ( actOpt->count ) {
		printf("Executing CommFPGA actions on FPGALink device %s...\n", vp);
		if ( isCommCapable ) {
			uint8 isRunning;
			fStatus = flSelectConduit(handle, conduit, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flIsFPGARunning(handle, &isRunning, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			if ( isRunning ) {
				ourFunction(handle, &error);
			} else {
				fprintf(stderr, "The FPGALink device at %s is not ready to talk - did you forget --program?\n", vp);
				FAIL(FLP_ARGS, cleanup);
			}
		} else {
			fprintf(stderr, "Action requested but device at %s does not support CommFPGA\n", vp);
			FAIL(FLP_ARGS, cleanup);
		}
	}

	if ( dumpOpt->count ) {
		const char *fileName;
		unsigned long chan = strtoul(dumpOpt->sval[0], (char**)&fileName, 10);
		FILE *file = NULL;
		const uint8 *recvData;
		uint32 actualLength;
		if ( *fileName != ':' ) {
			fprintf(stderr, "%s: invalid argument to option -l|--dumploop=<ch:file.bin>\n", progName);
			FAIL(FLP_ARGS, cleanup);
		}
		fileName++;
		printf("Copying from channel %lu to %s", chan, fileName);
		file = fopen(fileName, "wb");
		CHECK_STATUS(!file, FLP_CANNOT_SAVE, cleanup);
		sigRegisterHandler();
		fStatus = flSelectConduit(handle, conduit, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fStatus = flReadChannelAsyncSubmit(handle, (uint8)chan, 22528, NULL, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		do {
			fStatus = flReadChannelAsyncSubmit(handle, (uint8)chan, 22528, NULL, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, &error);
			CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
			fwrite(recvData, 1, actualLength, file);
			printf(".");
		} while ( !sigIsRaised() );
		printf("\nCaught SIGINT, quitting...\n");
		fStatus = flReadChannelAsyncAwait(handle, &recvData, &actualLength, &actualLength, &error);
		CHECK_STATUS(fStatus, FLP_LIBERR, cleanup);
		fwrite(recvData, 1, actualLength, file);
		fclose(file);
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
						ourFunction(handle, &error);
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

	if ( theBroCodeOpt->count ) {
		printf("\nStarting Railway Track Simulation\n");
		ourFunction(handle, &error);
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
