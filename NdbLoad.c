//The Neural Database - Routines to load databases into memory
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


// Functions:
int LoadNdb(NdbData *);
int GetHeaderData(NdbData *, FILE *);
int LoadHead(NdbData *, int *, char *, FILE *);
int LoadON(NdbData *, int *, char *, FILE *);
int LoadRN(NdbData *, int *, char *, FILE *);
long LoadConnections(NdbData *, int *, char *, FILE *);
int getnum(int, char *, int, long *);
int gettxt(int, char *, int, char *);


int LoadNdb(NdbData *Ndb) {
	//
	//	Load the 'N'.ndb database into the memory structure Ndb-> ...
	//
	//	The function returns 0 if successful, otherwise it returns N, the
	//	number of the database that failed to load.
	//
	//----------

	FILE *fh_read;
	char ndbfile[INQUIRY_LENGTH];
	int nlines;
	char line[FILE_LINE_LENGTH];
	int len;
	int res;
	long Lres;
	int badfile;

	sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, Ndb->ID);
	fh_read = fopen(ndbfile, "r");
	if (fh_read == NULL) {
		//Failed to OPEN the file
		return Ndb->ID;
	}

	Ndb->Type[0] = 0;
	Ndb->ONcount = 0;
	Ndb->RNcount = 0;
	Ndb->ConnectCount = 0;

	res = GetHeaderData(Ndb, fh_read);
	if (res != 0) {
		printf("ERROR: failed to read the Ndb Header data in the datafile\n");
		fclose(fh_read);
		return Ndb->ID;
	}

	//Now that the sizes are known, allocate memory
	if ((strcmp(Ndb->Type, "TEXT") == 0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {
		Ndb->pRN = (NdbRN *)malloc((Ndb->RNcount+1) * sizeof(NdbRN));
	} else {
		if (strcmp(Ndb->Type, "IMAGE_28X28") == 0) {
			Ndb->pIRN = (NdbImageRN *)malloc((Ndb->RNcount+1) * sizeof(NdbImageRN));
		} else {
			printf("ERROR: unknown Ndb Type=%s\n", Ndb->Type);
			fclose(fh_read);
			return Ndb->ID;
		}
	}
	Ndb->pON = (NdbON *)malloc((Ndb->ONcount+1) * sizeof(NdbON));
	Ndb->pRNtoON = (NdbRNtoON *)malloc((Ndb->ConnectCount+1) * sizeof(NdbRNtoON));

	rewind(fh_read); //Not really necessary

	badfile = 0;
	nlines = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		nlines++;

		len = strlen(line);
		if (line[len-1] == 10) line[len-1] = 0; // get rid of the linefeed

		if (strcmp(line, "NDB_ON") == 0) {
			Lres = LoadON(Ndb, &nlines, line, fh_read);
			if (Lres != Ndb->ONcount) {
				printf("ERROR: bad data in NDB_ON section\n");
				badfile=1;
				break;
			}
			continue;
		}

		if (strcmp(line, "NDB_RN") == 0) {
			res = LoadRN(Ndb, &nlines, line, fh_read);
			if (res != Ndb->RNcount) {
				printf("ERROR: bad data in NDB_RN section\n");
				badfile=1;
				break;
			}
			continue;
		}

		if (strcmp(line, "NDB_RN_TO_ON") == 0) {
			Lres = LoadConnections(Ndb, &nlines, line, fh_read);
			if (Lres != Ndb->ConnectCount) {
				printf("ERROR: bad data in NDB_RN_TO_ON section\n");
				badfile=1;
				break;
			}
			continue;
		}
	}

	fclose(fh_read);

	if (badfile != 0) {
		printf("Loading FAILED at line %d in the file: %s\n", nlines, ndbfile);
		FreeMem(Ndb); //assigned to in-memory Ndb
		return Ndb->ID;
	}

	return 0;
}

int GetHeaderData(NdbData *Ndb, FILE *fh_read) {

	int nlines;
	char line[FILE_LINE_LENGTH];
	int len = 0;
	long res = 0;

	nlines = 0;
	while (fgets( line, FILE_LINE_LENGTH, fh_read) != NULL) {
		nlines++;

		len = strlen(line);
		if (line[len-1] == 10) line[len-1] = 0; // get rid of the linefeed

		if (strcmp(line, "NDB_HEAD") == 0) {
			res = LoadHead(Ndb, &nlines, line, fh_read);
			if (res == 0) {
				printf("ERROR: bad data in NDB_HEAD section\n");
				return 1;
			}
			return 0; //Success
		}

	}

	return 1; //Unknown file
}

int LoadHead(NdbData *Ndb, int *nlines, char *line, FILE *fh_read) {
	
	int i;
	int c;
	char x[20];
	long v;
	int cnt;

	cnt = 0; //count of the Header elements

	while (fgets( line, FILE_LINE_LENGTH, fh_read) != NULL) {
		(*nlines)++;

		if ((line[0] == 0)||(line[0] == 10)) break; // End of section

		// get: Created, ID, ONcount, RNcount, Type
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(0, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign

		if (strcmp(x, "Created") == 0) {
			cnt++;
			continue; //next line
		}

		if (strcmp(x, "ID") == 0) {
			i = getnum(i, line, 10, &v); // 10 = linefeed terminator
			Ndb->ID = (int)v;
			cnt++;
			continue;
		}

		if (strcmp(x, "ONcount") == 0) {
			i = getnum(i, line, 10, &v);
			Ndb->ONcount = v;
			cnt++;
			continue;
		}

		if (strcmp(x, "RNcount") == 0) {
			i = getnum(i, line, 10, &v);
			Ndb->RNcount = (int)v;
			cnt++;
			continue;
		}

		if (strcmp(x, "ConnectCount") == 0) {
			i = getnum(i, line, 10, &v);
			Ndb->ConnectCount = v;
			cnt++;
			continue;
		}

		if (strcmp(x, "Type") == 0) {
			for (c=0; c<20; c++) x[c] = 0; //clear buffer
			i = gettxt(i, line, 10, x);
			x[19] = 0; //Just in case
			strcpy(Ndb->Type, x);
			cnt++;
			continue;
		}
	}
	if (cnt == 6) return 1; //success
	return 0;
}

int LoadON(NdbData *Ndb, int *nlines, char *line, FILE *fh_read) {

	int ONcount;
	int i, j, k;
	int c;
	char x[20];
	int ONcode = 0;
	int RNcode = 0;
	int Len = 0;
	long v;
	char z[INQUIRY_LENGTH];
	char ON[INQUIRY_LENGTH];
	char SUR[INQUIRY_LENGTH];
	char ACT[INQUIRY_LENGTH];


	ONcount = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		(*nlines)++;

		if ((line[0] == 0)||(line[0] == 10)) break; // End of section

		// get from line: ONcode=n,Len=n,ON=ALPHANUMERIC
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = 0;
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign

		if (strcmp(x, "ONcode") != 0) return 1; //bad file
		i = getnum(i, line, ',', &v); // comma = terminator
		ONcode = v;

		i++; //move past the comma
			
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign
		
		if (strcmp(x, "Len") != 0) return 1; //bad file
		i = getnum(i, line, ',', &v); // comma = terminator
		Len = (int)v;
		
		i++; //move past the comma
			
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign


		//ON[] may contain a SURROGATE and/or an ACTION, i.e. ON[] = "OUTPUT:SURROGATE:ACTION"
		if (strcmp(x, "ON") != 0) return 1; //bad file
		for (c=0; c<INQUIRY_LENGTH; c++) { //clear the buffers
			z[c] = 0;
			ON[c] = 0;
			SUR[c] = 0;
			ACT[c] = 0;
		}
		i = gettxt(i, line, ',', z); // comma = terminator
		for (c=0; c<INQUIRY_LENGTH; c++) {
			if (z[c] == 0) break; //end of the text
			if (z[c] == ':') break; //end of this component
			ON[c]=z[c];
		}
		if (z[c] == ':') {
			c++; //move past the ":"
			j = 0;
			for (k = c; k < INQUIRY_LENGTH; k++) {
				if (z[k] == 0) break; //end of the text
				if (z[k] == ':') break; //end of this component
				SUR[j]=z[k];
				j++;
			}
			if (z[k] == ':') {
				k++; //move past the ":"
				j = 0;
				for (c = k; c < INQUIRY_LENGTH; c++) {
					if (z[c] == 0) break; //end of the text
					ACT[j]=z[c];
					j++;
				}
			}
		}


		//Get the ON's Recognition List
		i++; //move past the comma
			
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign
		
		if (strcmp(x, "RL") != 0) return 1; //bad file

		if (Len == 1) {
			i = getnum(i, line, 10, &v); // linefeed = terminator
			RNcode = (int)v;
			Ndb->pON[ONcode].RL[0] = RNcode;
		} else {
			i = getnum(i, line, ',', &v); // comma = terminator
			RNcode = (int)v;
			Ndb->pON[ONcode].RL[0] = RNcode;
			for (j = 1; j < (Len-1); j++) {
				i++; //move past the comma
				i = getnum(i, line, ',', &v); // comma = terminator
				RNcode = (int)v;
				Ndb->pON[ONcode].RL[j] = RNcode;
			}
			i++; //move past the comma
			i = getnum(i, line, 10, &v); // linefeed = terminator
			RNcode = (int)v;
			Ndb->pON[ONcode].RL[j] = RNcode;
		}
		Ndb->pON[ONcode].RL[Len] = 0;

		Ndb->pON[ONcode].Len = Len; 
		strcpy(Ndb->pON[ONcode].ON, ON);
		strcpy(Ndb->pON[ONcode].SUR, SUR);
		strcpy(Ndb->pON[ONcode].ACT, ACT);

		ONcount++;

	}
	return ONcount;
}

int LoadRN(NdbData *Ndb, int *nlines, char *line, FILE *fh_read) {
	
	int RNcount;
	int i;
	int c;
	long v;
	int iRN;					// IMAGE RN: 110 = r1, etc. (These RNs are always numeric)
	char RN[INQUIRY_LENGTH];	// TEXT/CENTRAL RN: 'A' = r1, etc. (These RNs are always characters)
	int RNcode;

	RNcount = 0;
	while (fgets( line, FILE_LINE_LENGTH, fh_read) != NULL) {
		(*nlines)++;

		if ((line[0] == 0)||(line[0] == 10)) break; // End of section

		// get: RNcode = RN
		i = getnum(0, line, '=', &v); // equal sign = terminator
		RNcode = (int)v;

		i++; //move past the equal sign
		
		if ((strcmp(Ndb->Type, "TEXT") == 0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {
			for (c=0; c<INQUIRY_LENGTH; c++) RN[c] = 0; //clear buffer
			i = gettxt(i, line, 10, RN); // 10 = linefeed terminator
			strcpy(Ndb->pRN[RNcode].RN, RN); 
		} else {
			//Ndb->Type = "IMAGE_28X28"
			i = getnum(i, line, 10, &v); // linefeed = terminator
			iRN = (int)v; 
			Ndb->pIRN[RNcode].RN = iRN; 
		}

		RNcount++;

	}
	return RNcount;
}

long LoadConnections(NdbData *Ndb, int *nlines, char *line, FILE *fh_read) {

	long ConnectCount;
	int i;
	int c;
	long v;
	char x[20];
	int RNcode = 0;
	int Pos = 0;
	int ONcode = 0;
	
	ConnectCount = 0;
	while (fgets( line, FILE_LINE_LENGTH, fh_read) != NULL) {
		(*nlines)++;

		if ((line[0] == 0) || (line[0] == 10)) break; // End of section

		// get from line: RNcode=n Pos=n ONcode=n
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = 0;
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign

		if (strcmp(x, "RNcode") != 0) return 1; //bad file
		i = getnum(i, line, ',', &v); // comma = terminator
		RNcode = (int)v;

		i++; //move past the comma
			
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign
		
		if (strcmp(x, "Pos") != 0) return 1; //bad file
		i = getnum(i, line, ',', &v); // comma = terminator
		Pos = (int)v;
		
		i++; //move past the comma
			
		for (c=0; c<20; c++) x[c] = 0; //clear buffer
		i = gettxt(i, line, '=', x); // equal sign = terminator
		
		i++; //move past the equal sign
		
		if (strcmp(x, "ON") != 0) return 1; //bad file
		i = getnum(i, line, 10, &v); // linefeed = terminator
		ONcode = v;
		
		Ndb->pRNtoON[ConnectCount].RNcode = RNcode; 
		Ndb->pRNtoON[ConnectCount].Pos = Pos; 
		Ndb->pRNtoON[ConnectCount].ONcode = ONcode;
		ConnectCount++;

	}

	return ConnectCount;
}

int getnum(int p, char *txt, int terminator, long *val) {
	//
	//	Starting at character position p, scan the text txt[] until the terminator.
	//	Convert all the found numeric characters into a number returned in *val.
	//	The character position of the terminator is returned.
	//
	//	If the scan is contaminated by a non-numeric, the returned character position is zero.
	//
	//	It will also terminate on a linefeed: 10
	//
	//----------

	int c = 0;
	char x[20];
	int a = 0;

	for (c=0; c<20; c++) x[c] = 0; //clear buffer

	c = 0;
	while ((txt[p] != terminator)&&(c < 20)&&(txt[p] != 10)) {
		a = (int) txt[p]; //ASCII value
		if ((a > 47)&&(a < 58)) { // 0 - 9
			x[c] = txt[p];
			c++;
			p++;
		} else {
			p = 0; //Error, non-numeric
			break;
		}
	}
	if (c >19) p = 0; //buffer overflow
	if (p != 0) *val = atoi(x);

	return p;
}

int gettxt(int p, char *txt, int terminator, char *val) {
	//
	//	Starting at character position p, scan the text txt[] until the terminator.
	//	Return the characters in *val. The character position of the terminator is returned.
	//
	//	It will also terminate on a linefeed: 10
	//
	//----------

	int c = 0;
	char x[INQUIRY_LENGTH];

	for (c=0; c<INQUIRY_LENGTH; c++) x[c] = 0; //clear buffer

	c = 0;
	while ((txt[p] != terminator)&&(c < INQUIRY_LENGTH)&&(txt[p] != 10)) {
		x[c] = txt[p];
		c++;
		p++;
	}
	if (c > (INQUIRY_LENGTH - 1)) p = 0; //buffer overflow
	if (p != 0) strcpy(val, x);

	return p;
}


