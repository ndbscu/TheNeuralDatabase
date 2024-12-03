//The Neural Database - Routines to create databases
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


// Functions:
int CreateNdb(int, char *);
int CreateNdbAdditions(int, char *, char *, int, int);
int CreateQuestionNdb(int, char *);
void addRNcount(NdbData *);


int CreateNdb(int N, char *DataFile) {
	//
	//	Create Ndb #N (N.ndb) from text file DataFile
	//
	//	The words in DataFile will be translated into uppercase and stripped
	//	of everything except letters and digits. This allows for 'words' such
	//	as "4ever", and insures that no space-character will appear in a word.
	//
	//	The word is used to create the Recognition Neurons (RNs) and the
	//	space-character is not allowed to be an RN. Spaces are unique in that
	//	they have an influence in the SCU and in the boundary adjustment routines.
	//
	//	In creating an Ndb, the code reads through the source text file to:
	//		1. Count the number of ONs
	//		2. Get the length of the longest ON
	//		3. Identify all the RNs in the ONs
	//		4. From this data, allocate memory for the components of the Ndb
	//		5. Then read the file again to fill in the data structers
	//		6. Finally, write out the data structures to file 'N'.ndb
	//
	//	When a neural database is being used, it is always memory-resident. It
	//	is loaded into memory structures from the contents in the 'N'.ndb file.
	//
	//----------

	FILE *fh_read;
	FILE *fh_write;
	char ndbfile[INQUIRY_LENGTH];
	char line[FILE_LINE_LENGTH];
	char z[INQUIRY_LENGTH];
	char word[INQUIRY_LENGTH];
	char surrogate[INQUIRY_LENGTH];
	char action[INQUIRY_LENGTH];
	int p; //pointer to characters in line[]
	int i, j, k, c;
	char ch;
	int rn;
	int RNcode;
	int fnd;
	int len;
	int maxlen; //helps to allocate more than enough memory
	NdbData Ndb;

	clock_t T;
	double runtime;

	time_t ltime;
	char timestamp[32];

	printf("\nCreating %d.ndb from %s...", N, DataFile);

	T = clock();

	// Begin by counting the number of ONs/words...
	fh_read = fopen(DataFile, "r");
	if (fh_read == NULL) {
		printf("Error opening file %s for reading.\n", DataFile);
		return 1;
	}

	Ndb.ID = N;
	strcpy(Ndb.Type, "TEXT");
	Ndb.pRN = (NdbRN *)malloc(RN_RECORDS * sizeof(NdbRN));
	Ndb.RNcount = 0;
	Ndb.RNblocks = 1;

	//Read through the file to get counts of the various components...
	maxlen = 0;
	Ndb.ONcount = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				j++;
				if (j > maxlen) maxlen = j;
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					//
					//The word[] array generates the Recognition List of RNs
					//The space-character is NEVER allowed to be an RN
					//Any spaces must be stripped out of word[], but anything is allowed in Surrogate
					//This Ndb only allows letters (converted to uppercase) and digits: 0 - 9
					//4ever is OK, presumably with a Surrogate="forever"
					//
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}
				if (len > 0) {
					//Pick out the RNs
					for (c = 0; c < len; c++) {
						rn = (int)word[c]; //ACSII value
						//Is this rn already known?
						fnd = 0;
						for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
							if (Ndb.pRN[RNcode].RN[0] != rn) continue;
							fnd = 1;
							break;
						}
						if (fnd == 0) {
							addRNcount(&Ndb); //1, 2, ...
							Ndb.pRN[Ndb.RNcount].RN[0] = rn;
						}
					}
					Ndb.ONcount++;
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any words in %s\n", DataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		free(Ndb.pRN);
		fclose(fh_read);
		return 1;
	}

	// Allocate memory
	Ndb.pON = (NdbON *)malloc(Ndb.ONcount * sizeof(NdbON));

	// maxlen = maximum number of Positions in ON-text
	Ndb.pRNtoON = (NdbRNtoON *)malloc((Ndb.RNcount+1) * (maxlen+1) * (Ndb.ONcount+1) * sizeof(NdbRNtoON));

	rewind(fh_read);

	Ndb.ONcount = 0;
	Ndb.ConnectCount = 0;

	//Read through the file again, this time loading data...
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) {
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}
				if (z[c] == ':') {
					c++; //move past the ":"
					j = 0;
					for (i = c; i < INQUIRY_LENGTH; i++) {
						if (z[i] == 0) break; //end of the text
						if (z[i] == ':') break; //end of this component
						surrogate[j]=z[i];
						j++;
					}
					if (z[i] == ':') {
						i++; //move past the ":"
						j = 0;
						for (c = i; c < INQUIRY_LENGTH; c++) {
							if (z[c] == 0) break; //end of the text
							action[j]=z[c];
							j++;
						}
					}
				}
				if (len > 0) {
					// Does this word[] already exist in the database?
					fnd = 0;
					for (i = 0; i < Ndb.ONcount ;i++) {
						if (strcmp(Ndb.pON[i].ON, word) != 0) continue;
						fnd = 1;
						break;
					}
					if (fnd == 0) { //Add this ON to the database...
						//Scan word[] for its RNs
						for (i = 0; i < len; i++) {
							rn = (int) word[i]; //ASCII value
							RNcode = 0;
							for (k = 1; k <= Ndb.RNcount; k++) {
								if (Ndb.pRN[k].RN[0] != rn) continue;
								RNcode = k;
								break;
							}

							//NOTE: RNcode should NEVER be zero

							// Load the connections between RNs and ONs...
							Ndb.pRNtoON[Ndb.ConnectCount].RNcode = RNcode;
							Ndb.pRNtoON[Ndb.ConnectCount].Pos = i + 1;
							Ndb.pRNtoON[Ndb.ConnectCount].ONcode = Ndb.ONcount + 1;
							Ndb.ConnectCount++;

							//ON's Recognition List of RNcodes
							Ndb.pON[Ndb.ONcount].RL[i] = RNcode;
						}

						Ndb.pON[Ndb.ONcount].Len = len;
						strcpy(Ndb.pON[Ndb.ONcount].ON, word);
						strcpy(Ndb.pON[Ndb.ONcount].SUR, surrogate);
						strcpy(Ndb.pON[Ndb.ONcount].ACT, action);
						Ndb.ONcount++;
					}
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}
	fclose(fh_read);

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any words in %s\n", DataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		FreeMem(&Ndb);
		return 1;
	}

	// Write the data into file 'N'.ndb
	fnd = _mkdir(SubDirectoryNdbs); //creates this subdirectory if it doesn't already exist
	sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, N);

	fh_write = fopen(ndbfile, "w");
	if (fh_write == NULL) {
		printf("\nERROR: under sub-directory '%s', failed to open file %d.ndb\n", SubDirectoryNdbs, N);
		FreeMem(&Ndb);
		return 1;
	}

	// Get timestamp
	ltime = time(NULL);
	strcpy(timestamp, ctime(&ltime));
	//Get rid of the linefeed at the end of timestamp
	for (int i = 0; i < 32; i++) {
		if (timestamp[i] != 10) continue;
		timestamp[i] = 0;
		break;
	}

	// Write header data...
	fprintf(fh_write, "NDB_HEAD\n");
	fprintf(fh_write, "Created=%s\n", timestamp);
	fprintf(fh_write, "ID=%d\n", Ndb.ID);
	fprintf(fh_write, "ONcount=%d\n", Ndb.ONcount);
	fprintf(fh_write, "RNcount=%d\n", Ndb.RNcount);
	fprintf(fh_write, "ConnectCount=%d\n", Ndb.ConnectCount);
	fprintf(fh_write, "Type=%s\n", Ndb.Type);

	//Write ON data
	fprintf(fh_write, "\nNDB_ON\n");
	for (i = 0; i < Ndb.ONcount; i++) {
		fprintf(fh_write, "ONcode=%d,Len=%d,ON=%s", i+1, Ndb.pON[i].Len, Ndb.pON[i].ON);

		if (Ndb.pON[i].SUR[0] != 0) {
			fprintf(fh_write, ":%s", Ndb.pON[i].SUR);
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, ":%s", Ndb.pON[i].ACT);
			}
		} else {
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, "::%s", Ndb.pON[i].ACT);
			}
		}

		fprintf(fh_write, ",RL=%d", Ndb.pON[i].RL[0]);
		for (j = 1; j < Ndb.pON[i].Len; j++) {
			fprintf(fh_write, ",%d", Ndb.pON[i].RL[j]);
		}
		fprintf(fh_write, "\n");
	}

	// Write RN codes
	fprintf(fh_write, "\nNDB_RN\n");
	for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
		fprintf(fh_write, "%d=%c\n", RNcode, (char)Ndb.pRN[RNcode].RN[0]);
	}

	// Write RN-to-ON Connections
	fprintf(fh_write, "\nNDB_RN_TO_ON\n");
	for (int i = 0; i < Ndb.ConnectCount; i++) {
		fprintf(fh_write, "RNcode=%d,Pos=%d,ON=%d\n", Ndb.pRNtoON[i].RNcode, Ndb.pRNtoON[i].Pos, Ndb.pRNtoON[i].ONcode);
	}

	fprintf(fh_write, "\n$$$ End Of File\n");
	fclose(fh_write);

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;

	printf("\nFile %s created with %d ONs, %d RNs, %d Connections in %fsec.\n",
							ndbfile,
							Ndb.ONcount,
							Ndb.RNcount,
							Ndb.ConnectCount,
							runtime);
	FreeMem(&Ndb);

	return 0;
}

int CreateNdbAdditions(int N, char *MinDataFile, char *MaxDataFile, int skip, int offset) {
	//
	//	Create Ndb #N (N.ndb) from all the words in MinDataFile and some or
	//	all of the words in MaxDataFile
	//
	//	skip = number of words in MaxDataFile to skip before adding the next word
	//	offset = user entered addition to the skip-value to get a different mix of words
	//
	//	Read through both files to:
	//		1. Count the number of ONs
	//		2. Get the length of the longest ON
	//		3. Identify all the RNs in the ONs
	//		4. From this data allocate memory for the components of the Ndb
	//		5. Then read the files again to fill in the data structers
	//		6. Finally, write out the data structures to file 'N'.ndb
	//
	//----------

	FILE *fh_readMin;
	FILE *fh_readMax;
	FILE *fh_write;
	char ndbfile[INQUIRY_LENGTH];
	char line[FILE_LINE_LENGTH];
	char z[INQUIRY_LENGTH];
	char word[INQUIRY_LENGTH];
	char surrogate[INQUIRY_LENGTH];
	char action[INQUIRY_LENGTH];
	int p; //pointer to characters in line[]
	int i, j, k, c;
	int skipcnt;
	int addONcount; //Additional ONs from MaxDataFile
	int Lcnt;
	char ch;
	int rn;
	int RNcode;
	int fnd;
	int len;
	int maxlen; //helps to allocate more than enough memory
	NdbData Ndb;

	// timer
	clock_t T;
	double runtime;

	time_t ltime;
	char timestamp[32];

	printf("\nCreating %d.ndb from %s and %s...", N, MinDataFile, MaxDataFile);

	T = clock();

	//Get the essential ONs from all the words in MinDataFile
	//No ONs are 'skipped' in this file
	fh_readMin = fopen(MinDataFile, "r");
	if (fh_readMin == NULL) {
		printf("Error opening file %s for reading.\n", MinDataFile);
		return 1;
	}

	Ndb.ID = N;
	strcpy(Ndb.Type, "TEXT");
	Ndb.pRN = (NdbRN *)malloc(RN_RECORDS * sizeof(NdbRN));
	Ndb.RNcount = 0;
	Ndb.RNblocks = 1;

	Lcnt = 0;
	maxlen = 0;
	Ndb.ONcount = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_readMin) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		Lcnt++;
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				j++;
				if (j > maxlen) maxlen = j;
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}
				if (len > 0) {
					//Pick out the RNs
					for (c = 0; c < len; c++) {
						rn = (int)word[c]; //ACSII value
						//Is this rn already known?
						fnd = 0;
						for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
							if (Ndb.pRN[RNcode].RN[0] != rn) continue;
							fnd = 1;
							break;
						}
						if (fnd == 0) {
							addRNcount(&Ndb); //1, 2, ...
							Ndb.pRN[Ndb.RNcount].RN[0] = rn;
						}
					}
					Ndb.ONcount++;
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any words in %d lines in %s\n", Lcnt, MinDataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		fclose(fh_readMin);
		free(Ndb.pRN);
		return 1;
	}


	//Get additional ONs from some or all the words in MaxDataFile
	fh_readMax = fopen(MaxDataFile, "r");
	if (fh_readMax == NULL) {
		printf("Error opening file %s for reading.\n", MaxDataFile);
		fclose(fh_readMin);
		free(Ndb.pRN);
		return 1;
	}

	//Read through the MaxDataFile file...
	addONcount = 0;
	skipcnt = 0;
	Lcnt = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_readMax) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		Lcnt++;
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				j++;
				if (j > maxlen) maxlen = j;
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}

				if (skip != 0) {
					skipcnt++;
					if (skipcnt < (skip + offset)) {
						len = 0;
					} else {
						skipcnt = offset;
					}
				}

				if (len > 0) {
					//Pick out the RNs
					for (c = 0; c < len; c++) {
						rn = (int)word[c]; //ACSII value
						//Is this rn already known?
						fnd = 0;
						for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
							if (Ndb.pRN[RNcode].RN[0] != rn) continue;
							fnd = 1;
							break;
						}
						if (fnd == 0) {
							addRNcount(&Ndb); //1, 2, ...
							Ndb.pRN[Ndb.RNcount].RN[0] = rn;
						}
					}
					Ndb.ONcount++;
					addONcount++;
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}

	if (addONcount == 0) {
		printf("Error: failed to find any words in %d lines in %s\n", Lcnt, MaxDataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		fclose(fh_readMin);
		fclose(fh_readMax);
		free(Ndb.pRN);
		return 1;
	}


	// Allocate memory
	Ndb.pON = (NdbON *)malloc(Ndb.ONcount * sizeof(NdbON));

	// maxlen = maximum number of Positions in ON-text
	Ndb.pRNtoON = (NdbRNtoON *)malloc((Ndb.RNcount+1) * (maxlen+1) * (Ndb.ONcount+1) * sizeof(NdbRNtoON));

	rewind(fh_readMin);
	rewind(fh_readMax);

	Ndb.ONcount = 0;
	Ndb.ConnectCount = 0;

	//Read through the MinDataFile file, this time loading data...
	Lcnt = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_readMin) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		Lcnt++;
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}
				if (z[c] == ':') {
					c++; //move past the ":"
					j = 0;
					for (i = c; i < INQUIRY_LENGTH; i++) {
						if (z[i] == 0) break; //end of the text
						if (z[i] == ':') break; //end of this component
						surrogate[j]=z[i];
						j++;
					}
					if (z[i] == ':') {
						i++; //move past the ":"
						j = 0;
						for (c = i; c < INQUIRY_LENGTH; c++) {
							if (z[c] == 0) break; //end of the text
							action[j]=z[c];
							j++;
						}
					}
				}

				if (len > 0) {
					// Does this word[] already exist in the database?
					fnd = 0;
					for (i = 0; i < Ndb.ONcount ;i++) {
						if (strcmp(Ndb.pON[i].ON, word) != 0) continue;
						fnd = 1;
						break;
					}
					if (fnd == 0) { //Add this ON to the database...
						//Scan word[] for RN codes, adding new ones if necessary
						for (i = 0; i < len; i++) {
							rn = (int) word[i]; //ASCII value
							RNcode = 0;
							for (k = 1; k <= Ndb.RNcount; k++) {
								if (Ndb.pRN[k].RN[0] != rn) continue;
								RNcode = k;
								break;
							}

							//NOTE: RNcode should NEVER be zero

							// Load the connections between RNs and ONs...
							Ndb.pRNtoON[Ndb.ConnectCount].RNcode = RNcode;
							Ndb.pRNtoON[Ndb.ConnectCount].Pos = i + 1;
							Ndb.pRNtoON[Ndb.ConnectCount].ONcode = Ndb.ONcount + 1;
							Ndb.ConnectCount++;

							//ON's Recognition List of RNcodes
							Ndb.pON[Ndb.ONcount].RL[i] = RNcode;
						}

						Ndb.pON[Ndb.ONcount].Len = len;
						strcpy(Ndb.pON[Ndb.ONcount].ON, word);
						strcpy(Ndb.pON[Ndb.ONcount].SUR, surrogate);
						strcpy(Ndb.pON[Ndb.ONcount].ACT, action);
						Ndb.ONcount++;
					}
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}
	fclose(fh_readMin);

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any words in %d lines in %s\n", Lcnt, MinDataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		fclose(fh_readMax);
		FreeMem(&Ndb);
		return 1;
	}

	//Read through the MaxDataFile file, this time loading data...
	addONcount = 0;
	skipcnt = 0;
	Lcnt = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_readMax) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;word,word:SUR:ACT,word,...
		Lcnt++;
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					word[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||((ch >= 'A') && (ch <= 'Z'))) {
						word[len] = ch;
						len++;
					}
				}
				if (z[c] == ':') {
					c++; //move past the ":"
					j = 0;
					for (i = c; i < INQUIRY_LENGTH; i++) {
						if (z[i] == 0) break; //end of the text
						if (z[i] == ':') break; //end of this component
						surrogate[j]=z[i];
						j++;
					}
					if (z[i] == ':') {
						i++; //move past the ":"
						j = 0;
						for (c = i; c < INQUIRY_LENGTH; c++) {
							if (z[c] == 0) break; //end of the text
							action[j]=z[c];
							j++;
						}
					}
				}

				if (skip != 0) {
					skipcnt++;
					if (skipcnt < (skip + offset)) {
						len = 0;
					} else {
						skipcnt = offset;
					}
				}

				if (len > 0) {
					// Does this word[] already exist in the database?
					fnd = 0;
					for (i = 0; i < Ndb.ONcount ;i++) {
						if (strcmp(Ndb.pON[i].ON, word) != 0) continue;
						fnd = 1;
						break;
					}
					if (fnd == 0) { //Add this ON to the database...
						//Scan word[] for RN codes, adding new ones if necessary
						for (i = 0; i < len; i++) {
							rn = (int) word[i]; //ASCII value
							RNcode = 0;
							for (k = 1; k <= Ndb.RNcount; k++) {
								if (Ndb.pRN[k].RN[0] != rn) continue;
								RNcode = k;
								break;
							}

							//NOTE: RNcode should NEVER be zero

							// Load the connections between RNs and ONs...
							Ndb.pRNtoON[Ndb.ConnectCount].RNcode = RNcode;
							Ndb.pRNtoON[Ndb.ConnectCount].Pos = i + 1;
							Ndb.pRNtoON[Ndb.ConnectCount].ONcode = Ndb.ONcount + 1;
							Ndb.ConnectCount++;

							//ON's Recognition List of RNcodes
							Ndb.pON[Ndb.ONcount].RL[i] = RNcode;
						}
						Ndb.pON[Ndb.ONcount].Len = len;
						strcpy(Ndb.pON[Ndb.ONcount].ON, word);
						strcpy(Ndb.pON[Ndb.ONcount].SUR, surrogate);
						strcpy(Ndb.pON[Ndb.ONcount].ACT, action);
						Ndb.ONcount++;
						addONcount++;
					}
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}
	fclose(fh_readMax);

	if (addONcount == 0) {
		printf("Error: failed to find any words in %d lines in %s\n", Lcnt, MaxDataFile);
		printf("The line format should be ';;word,word,word,...'\n");
		FreeMem(&Ndb);
		return 1;
	}

	// Write the data into file 'N'.ndb
	fnd = _mkdir(SubDirectoryNdbs); //creates this subdirectory if it doesn't already exist
	sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, N);

	fh_write = fopen(ndbfile, "w");
	if (fh_write == NULL) {
		printf("\nERROR: under sub-directory '%s', failed to open file %d.ndb\n", SubDirectoryNdbs, N);
		FreeMem(&Ndb);
		return 1;
	}

	// Get timestamp
	ltime = time(NULL);
	strcpy(timestamp, ctime(&ltime));
	//Get rid of linefeed at the end of timestamp
	for (int i = 0; i < 32; i++) {
		if (timestamp[i] != 10) continue;
		timestamp[i] = 0;
		break;
	}

	// Write header data...
	fprintf(fh_write, "NDB_HEAD\n");
	fprintf(fh_write, "Created=%s\n", timestamp);
	fprintf(fh_write, "ID=%d\n", Ndb.ID);
	fprintf(fh_write, "ONcount=%d\n", Ndb.ONcount);
	fprintf(fh_write, "RNcount=%d\n", Ndb.RNcount);
	fprintf(fh_write, "ConnectCount=%d\n", Ndb.ConnectCount);
	fprintf(fh_write, "Type=%s\n", Ndb.Type);

	//Write ON data
	fprintf(fh_write, "\nNDB_ON\n");
	for (i = 0; i < Ndb.ONcount; i++) {
		fprintf(fh_write, "ONcode=%d,Len=%d,ON=%s", i+1, Ndb.pON[i].Len, Ndb.pON[i].ON);

		if (Ndb.pON[i].SUR[0] != 0) {
			fprintf(fh_write, ":%s", Ndb.pON[i].SUR);
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, ":%s", Ndb.pON[i].ACT);
			}
		} else {
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, "::%s", Ndb.pON[i].ACT);
			}
		}

		fprintf(fh_write, ",RL=%d", Ndb.pON[i].RL[0]);
		for (j = 1; j < Ndb.pON[i].Len; j++) {
			fprintf(fh_write, ",%d", Ndb.pON[i].RL[j]);
		}
		fprintf(fh_write, "\n");
	}

	// Write RN codes
	fprintf(fh_write, "\nNDB_RN\n");
	for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
		fprintf(fh_write, "%d=%c\n", RNcode, (char)Ndb.pRN[RNcode].RN[0]);
	}

	// Write RN-to-ON Connections
	fprintf(fh_write, "\nNDB_RN_TO_ON\n");
	for (int i = 0; i < Ndb.ConnectCount; i++) {
		fprintf(fh_write, "RNcode=%d,Pos=%d,ON=%d\n", Ndb.pRNtoON[i].RNcode, Ndb.pRNtoON[i].Pos, Ndb.pRNtoON[i].ONcode);
	}

	fprintf(fh_write, "\n$$$ End Of File\n");

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;

	printf("\nFile %s created with %d ONs, %d RNs, %d Connections in %fsec.\n",
							ndbfile,
							Ndb.ONcount,
							Ndb.RNcount,
							Ndb.ConnectCount,
							runtime);

	fclose(fh_write);
	FreeMem(&Ndb);

	return 0;
}

int CreateQuestionNdb(int N, char *DataFile) { // Create an Ndb of common questions with associated Actions
	//
	//	Create Ndb #N (N.ndb) from DataFile
	//
	//	NOTE: The TYPE of this database is "CENTRAL". Input into it has little preprocessing, if any, since
	//	it comes directly from another Ndb.
	//
	//	The ON will be the question without spaces while the Surrogate will be the original question
	//	with spaces.
	//
	//	The entries in the DataFile should therefore look like ";;word word word::action,..."
	//
	//	The RNcodes will represent the words in the question.
	//
	//----------

	FILE *fh_read;
	FILE *fh_write;
	char ndbfile[INQUIRY_LENGTH];
	char line[FILE_LINE_LENGTH];
	char z[INQUIRY_LENGTH];
	char question[INQUIRY_LENGTH];
	char word[INQUIRY_LENGTH];
	char surrogate[INQUIRY_LENGTH];
	char action[INQUIRY_LENGTH];
	int p; //pointer to characters in line[]
	int i, j, c;
	char ch;
	int RNcode;
	int fnd;
	int len;
	int maxlen; //helps to allocate more than enough memory
	int nword; //number of words in a question
	NdbData Ndb;

	clock_t T;
	double runtime;

	time_t ltime;
	char timestamp[32];

	printf("\nCreating %d.ndb from %s...", N, DataFile);

	T = clock();

	// Begin by counting the number of ONs/words...
	fh_read = fopen(DataFile, "r");
	if (fh_read == NULL) {
		printf("Error opening file %s for reading.\n", DataFile);
		return 1;
	}

	Ndb.ID = N;
	strcpy(Ndb.Type, "CENTRAL");
	Ndb.pRN = (NdbRN *)malloc(RN_RECORDS * sizeof(NdbRN));
	Ndb.RNcount = 0;
	Ndb.RNblocks = 1;

	//Read through the file to get counts of the various components...
	maxlen = 0;
	Ndb.ONcount = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;question1:SUR:ACT,question2...
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the buffers
					z[i] = 0;
					question[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					//
					//Note: Spaces are allowed here because the space-character is a delimiter
					if (((ch >= '0') && (ch <= '9'))||(ch == ' ')||((ch >= 'A') && (ch <= 'Z'))) {
						question[len] = ch;
						len++;
					}
				}
				j = j + len; //the 1st component will also fill the Surrogate
				if (j > maxlen) maxlen = j;
				if (len > 0) {
					//Pick out the words (RNs) in the question
					for (i=0; i<len; i++) word[i]=0; //clear word[]
					j = 0;
					for (c = 0; c <= len; c++) {
						ch = question[c];
						if ((ch != ' ') && (c != len)) {
							word[j] = ch;
							j++;
							continue;
						}
						//Is this rn/word already known?
						fnd = 0;
						for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
							if (strcmp(Ndb.pRN[RNcode].RN, word) != 0) continue;
							fnd = 1;
							break;
						}
						if (fnd == 0) {
							addRNcount(&Ndb); //1, 2, ...
							strcpy(Ndb.pRN[Ndb.RNcount].RN, word);
						}
						for (i=0; i<len; i++) word[i]=0;
						j = 0;
					}
					Ndb.ONcount++;
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any questions in %s\n", DataFile);
		printf("The line format should be ';;question1:SUR:ACT,question2...'\n");
		fclose(fh_read);
		free(Ndb.pRN);
		return 1;
	}

	// Allocate memory
	Ndb.pON = (NdbON *)malloc(Ndb.ONcount * sizeof(NdbON));

	// maxlen = maximum number of Positions in ON-text
	Ndb.pRNtoON = (NdbRNtoON *)malloc((Ndb.RNcount+1) * (maxlen+1) * (Ndb.ONcount+1) * sizeof(NdbRNtoON));

	rewind(fh_read);

	Ndb.ONcount = 0;
	Ndb.ConnectCount = 0;

	//Read through the file again, this time loading data...
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;question1:SUR:ACT,question2...
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < INQUIRY_LENGTH; i++) {
					z[i] = 0;
					question[i] = 0;
					surrogate[i] = 0;
					action[i] = 0;
				}
				j = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					z[j] = line[p];
					j++;
					p++;
				}
				len = 0;
				for (c=0; c<INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					if (z[c] == ':') break; //end of this component
					ch = toupper(z[c]);
					if (((ch >= '0') && (ch <= '9'))||(ch == ' ')||((ch >= 'A') && (ch <= 'Z'))) {
						question[len] = ch;
						len++;
					}
				}
				if (z[c] == ':') {
					c++; //move past the ":"
					j = 0;
					for (i = c; i < INQUIRY_LENGTH; i++) {
						if (z[i] == 0) break; //end of the text
						if (z[i] == ':') break; //end of this component
						surrogate[j]=z[i]; //If there is a Surrogate here, it will be replaced
						j++;
					}
					if (z[i] == ':') {
						i++; //move past the ":"
						j = 0;
						for (c = i; c < INQUIRY_LENGTH; c++) {
							if (z[c] == 0) break; //end of the text
							action[j]=z[c];
							j++;
						}
					}
				}
				if (len > 0) {
					//Remove all spaces -> z[]
					j = 0;
					for (i = 0; i < INQUIRY_LENGTH; i++) {
						if (question[i] == ' ') continue; //stripping spaces
						z[j] = question[i];
						j++;
						if (question[i] == 0) break; //end of the text
					}
					// Does this question[] already exist in the database?
					fnd = 0;
					for (i = 0; i < Ndb.ONcount ;i++) {
						if (strcmp(Ndb.pON[i].ON, z) != 0) continue;
						fnd = 1;
						break;
					}
					if (fnd == 0) { //Add this ON to the database...
						//Scan question[] for its RN codes
						nword = 0;
						for (i=0; i<len; i++) word[i]=0;
						j = 0;
						for (c = 0; c <= len; c++) {
							ch = question[c];
							if ((ch != ' ') && (c != len)) {
								word[j] = ch;
								j++;
								continue;
							}
							nword++;
							//Get the RNcode...
							RNcode = 0;
							for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
								if (strcmp(Ndb.pRN[RNcode].RN, word) == 0) break;
							}

							//NOTE: RNcode should NEVER be zero

							// Load the connections between RNs and ONs...
							Ndb.pRNtoON[Ndb.ConnectCount].RNcode = RNcode;
							Ndb.pRNtoON[Ndb.ConnectCount].Pos = nword;
							Ndb.pRNtoON[Ndb.ConnectCount].ONcode = Ndb.ONcount + 1;
							Ndb.ConnectCount++;

							//ON's Recognition List of RNcodes
							Ndb.pON[Ndb.ONcount].RL[nword-1] = RNcode;

							for (i=0; i<len; i++) word[i]=0;
							j = 0;
						}

						Ndb.pON[Ndb.ONcount].Len = nword;
						strcpy(Ndb.pON[Ndb.ONcount].ON, z); //with no spaces
						strcpy(Ndb.pON[Ndb.ONcount].SUR, question); //with spaces
						strcpy(Ndb.pON[Ndb.ONcount].ACT, action);
						Ndb.ONcount++;

					}
				}
				if (line[p] == ',') p++; //skip to the next word in this line
			}
		}
	}
	fclose(fh_read);

	if (Ndb.ONcount == 0) {
		printf("Error: failed to find any questions in %s\n", DataFile);
		printf("The line format should be ';;question1:SUR:ACT,question2...'\n");
		FreeMem(&Ndb);
		return 1;
	}

	// Write the data into file 'N'.ndb
	fnd = _mkdir(SubDirectoryNdbs); //create this subdirectory if it doesn't already exist
	sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, N);

	fh_write = fopen(ndbfile, "w");
	if (fh_write == NULL) {
		printf("\nERROR: under sub-directory '%s', failed to open file %d.ndb\n", SubDirectoryNdbs, N);
		FreeMem(&Ndb);
		return 1;
	}

	// Get timestamp
	ltime = time(NULL);
	strcpy(timestamp, ctime(&ltime));
	//Get rid of linefeed at the end of timestamp
	for (int i = 0; i < 32; i++) {
		if (timestamp[i] != 10) continue;
		timestamp[i] = 0;
		break;
	}

	// Write header data...
	fprintf(fh_write, "NDB_HEAD\n");
	fprintf(fh_write, "Created=%s\n", timestamp);
	fprintf(fh_write, "ID=%d\n", Ndb.ID);
	fprintf(fh_write, "ONcount=%d\n", Ndb.ONcount);
	fprintf(fh_write, "RNcount=%d\n", Ndb.RNcount);
	fprintf(fh_write, "ConnectCount=%d\n", Ndb.ConnectCount);
	fprintf(fh_write, "Type=%s\n", Ndb.Type);

	//Write ON data
	fprintf(fh_write, "\nNDB_ON\n");
	for (i = 0; i < Ndb.ONcount; i++) {
		fprintf(fh_write, "ONcode=%d,Len=%d,ON=%s", i+1, Ndb.pON[i].Len, Ndb.pON[i].ON);

		if (Ndb.pON[i].SUR[0] != 0) {
			fprintf(fh_write, ":%s", Ndb.pON[i].SUR);
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, ":%s", Ndb.pON[i].ACT);
			}
		} else {
			if (Ndb.pON[i].ACT[0] != 0) {
				fprintf(fh_write, "::%s", Ndb.pON[i].ACT);
			}
		}

		fprintf(fh_write, ",RL=%d", Ndb.pON[i].RL[0]);
		for (j = 1; j < Ndb.pON[i].Len; j++) {
			fprintf(fh_write, ",%d", Ndb.pON[i].RL[j]);
		}
		fprintf(fh_write, "\n");
	}

	//Write RN codes
	fprintf(fh_write, "\nNDB_RN\n");
	for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
		fprintf(fh_write, "%d=%s\n", RNcode, Ndb.pRN[RNcode].RN);
	}

	//Write RN-to-ON Connections
	fprintf(fh_write, "\nNDB_RN_TO_ON\n");
	for (int i = 0; i < Ndb.ConnectCount; i++) {
		fprintf(fh_write, "RNcode=%d,Pos=%d,ON=%d\n", Ndb.pRNtoON[i].RNcode, Ndb.pRNtoON[i].Pos, Ndb.pRNtoON[i].ONcode);
	}

	fprintf(fh_write, "\n$$$ End Of File\n");
	fclose(fh_write);

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;

	printf("\nFile %s created with %d ONs, %d RNs, %d Connections in %fsec.\n",
							ndbfile,
							Ndb.ONcount,
							Ndb.RNcount,
							Ndb.ConnectCount,
							runtime);
	FreeMem(&Ndb);

	return 0;
}

void addRNcount(NdbData *Ndb) {
	//
	//	Increment RNcount and add more memory if necessary
	//
	//----------

	Ndb->RNcount++;

	if ((Ndb->RNcount % RN_RECORDS) == 0) { // The list is full
		Ndb->RNblocks++;
		Ndb->pRN = (NdbRN *)realloc(Ndb->pRN, (Ndb->RNblocks * RN_RECORDS * sizeof(NdbRN)));
	}
}

