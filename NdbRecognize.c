//The Neural Database - Recognition routines
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


//Functions:
//NdbData is often included for any diagnostic code that could be added
int RecognizeTEXT(NdbData *, char *);
int PreProcessText(NdbData *, RECdata *, char *);
int PreProcessQuestion(NdbData *, RECdata *, char *);
int mpRecognizeIMAGE(long, char *);
int mpRunIMAGE(char *, ImageMP *);
void mpLoadImageTasks(int *, ImageMP *);
void RecognizeINPUT(NdbData *, RECdata *);
int FinishRecognition(NdbData *, RECdata *);
int RemoveEnvelopments(NdbData *, RECdata *);
int RetractAnomalousBoundaries(NdbData *, RECdata *);
int RetractWeakBoundaries(NdbData *, RECdata *);
int ExpandBoundaries(NdbData *, RECdata *);
int ExpandBeginBoundary(NdbData *, RECdata *, int);
int ExpandEndBoundary(NdbData *, RECdata *, int);
void RemoveWeakONs(NdbData *, RECdata *);
void GetAnomalyCounts(RECdata *);
int MakeBranches(NdbData *, RECdata *);
void BuildBranches(NdbData *, RECdata *);
void addBRHcount(RECdata *);
void addBRcount(RECdata *);
int CompeteBranches(NdbData *, RECdata *);
void addTcount(RECdata *);
int RunCompetitions(NdbData *, RECdata *);
int GetWinners(NdbData *, RECdata *, int, int);


int RecognizeTEXT(NdbData *Ndb, char *INPUT) {
	//
	//	Recognize the INPUT[] using Ndb #N
	//
	//	Results in R[] will be put in the array OUTPUT[]
	//
	//----------

	int er;
	int i;
	RECdata pRD;

	if (ShowProgress == 1) printf("\nNdb #%d: Identifying: %s ...", Ndb->ID, INPUT);


	if ((strcmp(Ndb->Type, "TEXT") != 0) && (strcmp(Ndb->Type, "CENTRAL") != 0)) {
		printf("\nERROR: Unknown Ndb TYPE: %s", Ndb->Type);
		return 1;
	}

	//Get memory for pRD->pR[]
	pRD.pR = (Rdata *)malloc(R_RECORDS * sizeof(Rdata));
	pRD.Rcount = 0;
	pRD.Rblocks = 1;


	//Get memory for pBRH...pBR[] - linked lists of ONs/pRD->pR[] making up the branches
	pRD.pBRH = (BRHdata *)malloc(BR_RECORDS * sizeof(BRHdata)); //memory for Branch Head
	pRD.BRHcount = 0;
	pRD.BRHblocks = 1;

	pRD.pBR = (BRdata *)malloc(BR_RECORDS * sizeof(BRdata)); //memory for Branch Members
	pRD.BRcount = 0;
	pRD.BRblocks = 1;


	//Get memory for pT[] - list of the competing Branches
	pRD.pT = (Tdata *)malloc(T_RECORDS * sizeof(Tdata));
	pRD.Tcount = 0;
	pRD.Tblocks = 1;


	if (strcmp(Ndb->Type, "TEXT") == 0) {
		//Find the characters in INPUT[] and load their RNs into ISRN[]
		er = PreProcessText(Ndb, &pRD, INPUT);
	}

	if (strcmp(Ndb->Type, "CENTRAL") == 0) {
		//Find the words in INPUT[] and load their RNs into ISRN[]
		er = PreProcessQuestion(Ndb, &pRD, INPUT);
	}

	RecognizeINPUT(Ndb, &pRD); //[IS] -> pRES[]

	if (pRD.RESULTcount == 0) {
		OUTcount = 0;
		er = 99; //Unable to recognize INPUT[]
	} else {
		er = FinishRecognition(Ndb, &pRD); //pRES[] -> pOUT[]
	}

	for (i = 1; i <= pRD.Tcount; i++) free(pRD.pT[i].pNC);
	free(pRD.pT);
	free(pRD.pBR);
	free(pRD.pBRH);
	free(pRD.pR);
	return er;
}

int PreProcessText(NdbData *Ndb, RECdata *pRD, char *INPUT) {
	//
	//	Convert INPUT[] into ISRN[], the Input Stream of RNs
	//
	//	1.	Get rid of everything that will not be recognized by an RN.
	//
	//	2.	Reduce all repeating characters to 2-of-a-kind
	//		"THURSOOOOOOOOODAY" --> THURSOODAY
	//
	//	3.	Replace double-doubles with single-doubles
	//		FFRRIIDDAAYY --> FRRIDDAYY
	//
	//	4.	Replace any multiple spaces with a single space
	//		Remove any leading/trailing space
	//		Record the location of the remaining spaces with Space[]
	//		Then remove all spaces
	//
	//----------

	int i, k;
	char ch;
	int RNcode;
	int rn;
	char x[INQUIRY_LENGTH+1];
	char y[INQUIRY_LENGTH+1];
	int len;


	//Remove anything from INPUT[] that will not be recognized: doesn't have an RN
	//But retain the spaces for now...
	for (i = 0; i < INQUIRY_LENGTH; i++) x[i] = 0; //clear buffer
	k = 0;

	for (i = 0; i < INQUIRY_LENGTH; i++) {
		ch = toupper(INPUT[i]);

		rn = (int) ch; //ASCII value
		if (rn == 0) break;
		if (ch == ' ') { //preserve spaces
			x[k] = ch;
			k++;
		} else {
			for (RNcode = 1; RNcode <= Ndb->RNcount; RNcode++) {
				if (Ndb->pRN[RNcode].RN[0] == ch) {
					x[k] = ch;
					k++;
					break;
				}
			}
		}
	}


	// Don't keep more than 2-in-a-row of the same character
	for (i = 0; i < INQUIRY_LENGTH; i++) y[i] = 0; //clear buffer
	len = strlen(x);
	k = 0;
	for (i = 0; i < (len - 2); i++) {
		if ((x[i] == x[i+1])&&(x[i] == x[i+2])) {
			if ((x[i] < 'A')||(x[i] > 'Z')) {
				y[k] = x[i];
				k++;
			}
		} else {
			y[k] = x[i];
			k++;
		}
	}
	while (i < len) {
		y[k] = x[i];
		k++;
		i++;
	}

	//Don't allow paired doubles
	for (i = 0; i < INQUIRY_LENGTH; i++) x[i] = 0; //clear buffer
	len = strlen(y);
	k = 0;
	for (i = 0; i < (len - 3); i++) {
		if ((y[i] >= 'A')&&(y[i] <= 'Z')) {
			if ((y[i+1] >= 'A')&&(y[i+1] <= 'Z')) {
				if ((y[i+2] >= 'A')&&(y[i+2] <= 'Z')) {
					if ((y[i+3] >= 'A')&&(y[i+3] <= 'Z')) {
						if ((y[i] == y[i+1])&&(y[i+2] == y[i+3])&&(y[i] != y[i+2])) {
							i++;
							x[k] = y[i];
							k++;
							i++;
							x[k] = y[i];
							k++;
							i++;
							x[k] = y[i];
							k++;
							continue;
						}
					}
				}
			}
		}
		x[k] = y[i];
		k++;
	}
	while (i < len) {
		x[k] = y[i];
		k++;
		i++;
	}

	//Leave only single spaces
	len = strlen(x);
	for (i = 0; i < INQUIRY_LENGTH; i++) y[i] = 0; //clear buffer
	k = 0;
	for (i = 0; i < (len - 1); i++) {
		if ((x[i] == ' ')&&(x[i+1] == ' ')) continue;
		y[k] = x[i];
		k++;
	}
	while (i < len) {
		y[k] = x[i];
		k++;
		i++;
	}

	//Remove any space from the beginning and/or end
	len = strlen(y);
	for (i = 0; i < INQUIRY_LENGTH; i++) x[i] = 0; //clear buffer
	i = 0;
	if (y[0] == ' ') i++;
	if (y[len - 1] == ' ') len--;
	k = 0;
	while (i < len) {
		x[k] = y[i];
		k++;
		i++;
	}

	//Clear result arrays
	for (i = 0; i < INQUIRY_LENGTH; i++) {
		pRD->Space[i] = 0; // locations of relevant spaces
		pRD->ISRN[i] = 0; // Input Stream in RNs
	}

	//Remove ALL spaces, but mark the location of each character that is preceeded by a space
	len = strlen(x);
	k = 0;
	for (i = 0; i < len; i++) {
		if (x[i] == ' ') {
			pRD->Space[k + 1] = 1;
			continue;
		}

		// Get the Input Stream in RNcodes...
		for (RNcode = 1; RNcode <= Ndb->RNcount; RNcode++) {
			if (Ndb->pRN[RNcode].RN[0] == x[i]) {
				pRD->ISRN[k] = RNcode;
				break;
			}
		}
		k++;
	}
	
	return 0;
}

int PreProcessQuestion(NdbData *Ndb, RECdata *pRD, char *INPUT) {
	//
	//	Convert INPUT[] into ISRN[], the Input Stream in RNs
	//
	//	Question = WORD space WORD space ...
	//
	//	If there's no RN for a word, the word will be ignored
	//
	//----------

	int i, j, k;
	int RNcode;
	char word[INQUIRY_LENGTH];

	for (i = 0; i < INQUIRY_LENGTH; i++) { //clear the arrays
		pRD->Space[i] = 0;
		pRD->ISRN[i] = 0;
		word[i] = 0;
	}
	k = 0;
	j = 0;
	for (i=0; i<INQUIRY_LENGTH; i++) {
		if (INPUT[i] != ' ') {
			word[j] = toupper(INPUT[i]); //An ON Surrogate may have been used and may be 'contaminated' with lowercase
			j++;
		}
		if ((INPUT[i] == 0)||(INPUT[i] == ' ')) { //end of the word and/or end of the INPUT text
			for (RNcode = 1; RNcode <= Ndb->RNcount; RNcode++) { //Is there an RN code for it?
				if (strcmp(Ndb->pRN[RNcode].RN, word) == 0) {
					pRD->ISRN[k] = RNcode;
					k++;
					break;
				}
			}
			if (INPUT[i] == ' ') { //get next word
				for (j = 0; j < INQUIRY_LENGTH; j++) { //clear the word
					if (word[j] == 0) break;
					word[j] = 0;
				}
				j = 0;
			}
		}
		if (INPUT[i] == 0) break; //end of the INPUT text
	}

	return 0;
}

int mpRecognizeIMAGE(long RecNum, char *INPUT) {
	//
	//	Setup multi-threading to run this inquiry on the 399 'image' databases...
	//
	//----------

	ImageMP mp[399];
	int MPcount;

	int Counts[10]; //Number of databases that recognized this digit
	int i, j, k;
	char digit;
	int max;
	int ResultFlag;
	int er;

	ResultFlag = 0; //or =N of any database that failed to load
	er = 0;	//Success"

	mpLoadImageTasks(&MPcount, mp);

	for (i = 0; i < MPcount; i++) {
		mp[i].RecNum = RecNum;
		for (j = 1; j <= TOTAL_ALLOWED_RESULTS; j++) {
			for (k = 0; k < 10; k++) mp[i].Results[j][k] = 0; //clear the Results buffer for all databases
		}
	}


	//Request ActualThreads from the OS. You may or may not be given this many.
	omp_set_num_threads(ActualThreads);
	#pragma omp parallel
	{ // ------------------------------------------------------------ Start of OpenMP

		int er;
		int i;

		#pragma omp for //OpenMP will split the loop up among the threads, each having a unique i
		for (i = 0; i < MPcount; i++) {
			er = mpRunIMAGE(INPUT, &mp[i]);

			if (er != 0) {
				//Setting ResultFlag is a race condition, but who cares - nobody is setting it back to zero.
				if (er > 1999) ResultFlag = er; //Failed to load mp[i].N, OR Bad INPUT/Panel Image.
				continue;
			}
		}

	} // ------------------------------------------------------------ End of OpenMP

	//Failed to load at least one database (corrupt file?), or bad image in INPUT[]
	if (ResultFlag != 0) return ResultFlag;
	
	for (i = 0; i < 10; i++) Counts[i] = 0; //clear the digit counts

	for (i = 0; i < MPcount; i++) {
		for (j = 1; j <= TOTAL_ALLOWED_RESULTS; j++) {
			for (k = 0; k < 10; k++) {
				digit = mp[i].Results[j][k];
				if (digit == 0) break; //done with this set of digits

				switch (digit) {
					case '0':
						Counts[0]++;
						break;
					case '1':
						Counts[1]++;
						break;
					case '2':
						Counts[2]++;
						break;
					case '3':
						Counts[3]++;
						break;
					case '4':
						Counts[4]++;
						break;
					case '5':
						Counts[5]++;
						break;
					case '6':
						Counts[6]++;
						break;
					case '7':
						Counts[7]++;
						break;
					case '8':
						Counts[8]++;
						break;
					case '9':
						Counts[9]++;
						break;
					default:
						break;
				}
			}
		}
	}

	max = 0;
	j = 99;
	for (i = 0; i < 10; i++) {
		if (Counts[i] > max) {
			max = Counts[i];
			j = i;
		}
	}

	if (j == 99) {
		OUTcount = 0; // No Result
	} else {
		OUTcount = 1;
		pOUT[1].REScount = 1;
		sprintf(pOUT[1].Result[1].ON, "%d", j);
	}

	return 0; //success
}

int mpRunIMAGE(char *INPUT, ImageMP *mp) {
	//
	//	Get the result for this Image/Panel from Ndb mp[i].N
	//
	//----------

	NDBimage ROW;
	NDBimage COL;
	NDBimage DIAG;
	NDBimage A;

	NdbData Ndb;
	RECdata pRD;
	ImageData pIdata;
	int ImageRN[NUMBER_OF_IMAGE_RNS+1]; //some extra room
	int rb, re, cb, ce, row, col, ro, co;
	int i, j, k;
	int RNcode;
	int ONcode;
	int N;
	int er;
	int Tcnt;
	char digit;

	N = mp->N;
	Ndb.ID = N;

	er = LoadNdb(&Ndb);
	if (er != 0) return N; //Ndb #N failed to load...
	
	er = GetImage(INPUT, mp->Contrast, &ROW, &COL, &DIAG); //Returns ROW[], COL[], DIAG[]
	if (er == 1) {
		//Bad image (there actually aren't any unless the file has been corrupted)
		FreeMem(&Ndb);
		return N;
	}

	rb = mp->rb;
	re = mp->re;
	cb = mp->cb;
	ce = mp->ce;

	for (row = rb; row <= re; row++) {
		for (col = cb; col <= ce; col++) {

			//Offsets for image A
			ro = 0;
			co = 0;
			if (rb != 1) ro = rb - 1;
			if (cb != 1) co = cb - 1;

			if (mp->Type == 'R') {
				A.image[row-ro][col-co] = ROW.image[row][col];
			} else {
				if (mp->Type == 'C') {
					A.image[row-ro][col-co] = COL.image[row][col];
				} else {
					A.image[row-ro][col-co] = DIAG.image[row][col];
				}
			}
		}
	}
	A.ImageSize = mp->Size;

	//Get the Recognition List of RNs: ImageRN[]
	ImageFormatter(&pIdata, &A, ImageRN);

	for (j = 0; j < INQUIRY_LENGTH; j++) { //clear data
		pRD.ISRN[j] = 0;
		pRD.Space[j] = 0;
		pRD.Owned[j] = 0;
	}

	//Load the Input Stream with the RNs produced by the inquiry image
	//ImageRN[] begins at 1, but pRD.ISRN[j] begins at 0
	//Convert the RN-values to the RNcodes that are specific to this Ndb...
	k = 0;
	for (j = 1; j < INQUIRY_LENGTH; j++) {
		if (ImageRN[j] == 0) break;

		for (RNcode = 1; RNcode <= Ndb.RNcount; RNcode++) {
			if (Ndb.pIRN[RNcode].RN == ImageRN[j]) {
				pRD.ISRN[k] = RNcode;
				k++;
				break;
			}
		}
	}

	//Get memory for pRD->pR[]
	pRD.pR = (Rdata *)malloc(R_RECORDS * sizeof(Rdata));
	pRD.Rcount = 0;
	pRD.Rblocks = 1;

	//Get memory for pBRH...pBR[] - linked lists of ONs/pRD->pR[] making up the Branches
	pRD.pBRH = (BRHdata *)malloc(BR_RECORDS * sizeof(BRHdata)); //memory for Branch Head
	pRD.BRHcount = 0;
	pRD.BRHblocks = 1;

	pRD.pBR = (BRdata *)malloc(BR_RECORDS * sizeof(BRdata)); //memory for the Branch Members
	pRD.BRcount = 0;
	pRD.BRblocks = 1;

	//Get memory for pT[] - list of the competing Branches
	pRD.pT = (Tdata *)malloc(T_RECORDS * sizeof(Tdata));
	pRD.Tcount = 0;
	pRD.Tblocks = 1;

	RecognizeINPUT(&Ndb, &pRD); //[IS] -> pRES[]

	if (pRD.RESULTcount == 0) {
		er = 99; //Unable to recognize INPUT[]
	} else {
		er = 0;
		//The Image ON's Surrogate has the recognition results: the list of digits recognized.
		//Also, there may be multiple winning ONs, each representing multiple digits... 
		Tcnt = 0;
		for (i = 1; i <= pRD.RESULTcount; i++) {
			ONcode = pRD.pRES[i].Result[1].ONcode;
			for (j = 0; j < 10; j++) {
				digit = Ndb.pON[ONcode].SUR[j];
				if (digit == 0) break; //end of SUR
				Tcnt++;
			}
		}

		if (Tcnt < IMAGE_AMBIGUITY_THRESHOLD) {
			for (i = 1; i <= pRD.RESULTcount; i++) {
				ONcode = pRD.pRES[i].Result[1].ONcode;
				for (j = 0; j < 10; j++) {
					digit = Ndb.pON[ONcode].SUR[j];
					if (digit == 0) break; //end of SUR
					mp->Results[i][j] = digit;
				}
			}
		}
	}

	for (i = 1; i <= pRD.Tcount; i++) free(pRD.pT[i].pNC);
	free(pRD.pT);
	free(pRD.pBR);
	free(pRD.pBRH);
	free(pRD.pR);
	FreeMem(&Ndb);

	return er;
}

void mpLoadImageTasks(int *MPcount, ImageMP *mp) {
	//
	//	Define all the parameters to run the inquiry on all 399 image databases...
	//
	//----------

	int Contrast;
	int N;

	*MPcount = 0;

	//Setup parameters for all the images & panels to inquire into the multiple Image Ndbs
	N = 2000;
	while (N < 2070) {

		if (N == 2000) Contrast = 2;
		if (N == 2010) Contrast = 33;
		if (N == 2020) Contrast = 66;
		if (N == 2030) Contrast = 100;
		if (N == 2040) Contrast = 133;
		if (N == 2050) Contrast = 166;
		if (N == 2060) Contrast = 200;


		//ROW: 28x28 (full image)
		mp[*MPcount].N = N;
		mp[*MPcount].Size = 28;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;

		//Chop the ROW image up into Overlapping 16x16 panels...
		mp[*MPcount].N = N+1;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+2;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;

		mp[*MPcount].N = N+3;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+4;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+5;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+6;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+7;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+8;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+9;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		

		//mp[*MPcount].A = COL; //28x28 (full image)
		mp[*MPcount].N = N+200;
		mp[*MPcount].Size = 28;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;

		//Chop the COL image up into Overlapping 16x16 panels...
		mp[*MPcount].N = N+201;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+202;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+203;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+204;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+205;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+206;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+207;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+208;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+209;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;



		//mp[*MPcount].A = DIAG; //28x28 (full image)
		mp[*MPcount].N = N+400;
		mp[*MPcount].Size = 28;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;

		//Chop the DIAG image up into Overlapping 16x16 panels...
		mp[*MPcount].N = N+401;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+402;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+403;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 16;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+404;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+405;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+406;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 7;
		mp[*MPcount].ce = 22;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+407;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 16;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+408;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 7;
		mp[*MPcount].re = 22;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+409;
		mp[*MPcount].Size = 16;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 13;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 13;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;

		N = N + 10;
 	}
	

	N = 2100;
	while (N < 2170) {
 
		if (N == 2100) Contrast = 2;
		if (N == 2110) Contrast = 33;
		if (N == 2120) Contrast = 66;
		if (N == 2130) Contrast = 100;
		if (N == 2140) Contrast = 133;
		if (N == 2150) Contrast = 166;
		if (N == 2160) Contrast = 200;

		//The parameters for the full 28x28 images have already been added above.

		//Chop the ROW image up into Overlapping 18x18 panels...
		mp[*MPcount].N = N+1;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+2;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+3;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+4;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+5;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+6;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+7;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+8;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;
		
		mp[*MPcount].N = N+9;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'R';
		(*MPcount)++;



		//Chop the COL image up into Overlapping 18x18 panels...
		mp[*MPcount].N = N+201;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+202;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+203;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+204;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+205;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+206;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+207;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+208;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;
		
		mp[*MPcount].N = N+209;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'C';
		(*MPcount)++;


		//Chop the DIAG image up into Overlapping 18x18 panels...
		mp[*MPcount].N = N+401;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+402;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+403;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 1;
		mp[*MPcount].ce = 18;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+404;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+405;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+406;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 6;
		mp[*MPcount].ce = 23;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+407;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 1;
		mp[*MPcount].re = 18;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+408;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 6;
		mp[*MPcount].re = 23;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;
		
		mp[*MPcount].N = N+409;
		mp[*MPcount].Size = 18;
		mp[*MPcount].Contrast = Contrast;
		mp[*MPcount].rb = 11;
		mp[*MPcount].re = 28;
		mp[*MPcount].cb = 11;
		mp[*MPcount].ce = 28;
		mp[*MPcount].Type = 'D';
		(*MPcount)++;

		N = N + 10;
	}
}

void RecognizeINPUT(NdbData *Ndb, RECdata *pRD) {
	//
	//	Recognize the ON(s) in INPUT[]
	//
	//----------

	int i, j;
	int res;

	// clear the Results arrays
	for (i = 0; i < R_RECORDS; i++) {
		pRD->pR[i].B = 0;
		pRD->pR[i].E = 0;
		pRD->pR[i].ONcode = 0;
	}
	for (i = 1; i <= TOTAL_ALLOWED_RESULTS; i++) {
		pRD->pRES[i].NumberOfONs = 0;
		for (j = 0; j < INQUIRY_LENGTH; j++) {
			pRD->pRES[i].Result[j].B = 0;
			pRD->pRES[i].Result[j].E = 0;
			pRD->pRES[i].Result[j].ONcode = 0;
			pRD->pRES[i].Result[j].ON[0] = 0;
		}
	}
	pRD->RESULTcount = 0;

	if (pRD->ISRN[0] == 0) return; //It's not an error if there's nothing to recognize

	res = GetONs(Ndb, pRD);

	if (res > 0) { //Some ONs have survived the initial thresholds and are in pRD->R[]

		RemoveEnvelopments(Ndb, pRD); //Remove weak, enveloped ONs
		RetractAnomalousBoundaries(Ndb, pRD); //Drop anomalous trailing RNs if they belong to another ON
		RetractWeakBoundaries(Ndb, pRD); //Retract over-reaching boundaries of less-dominate ONs
		ExpandBoundaries(Ndb, pRD); //Expand pattern-matched boundaries (stutters?)
		RemoveWeakONs(Ndb, pRD); //Remove competitors that fall below the WEAK_ON_THRESHOLD
		GetAnomalyCounts(pRD); //Count anomalies in qpos and dpos positions

		MakeBranches(Ndb, pRD); //Combine ONs, boundary-to-boundary, into branches - each one a possible solution
		CompeteBranches(Ndb, pRD); //Use the SCU to determine which branch is best --> pRD->R[]
	}

	return;
}

int FinishRecognition(NdbData *Ndb, RECdata *pRD) {
	//
	//	Load results into OUTPUT, making Surrogate replacements if necessary and
	//	returning any Actions associated with each recognized ON...
	//
	//----------

	int i, j;
	int REScount;
	int ONcode;


	OUTcount = pRD->RESULTcount;
	for (i = 1; i <= OUTcount; i++) {
		REScount = pRD->pRES[i].NumberOfONs;
		pOUT[i].REScount = REScount;

		for (j = 1; j <= REScount; j++) {
			pOUT[i].Result[j].B = pRD->pRES[i].Result[j].B;
			pOUT[i].Result[j].E = pRD->pRES[i].Result[j].E;
			ONcode = pRD->pRES[i].Result[j].ONcode;
			pOUT[i].Result[j].ONcode = ONcode;

			// Replace ON with Surrogate?
			if (Ndb->pON[ONcode].SUR[0] != 0) {
				strcpy(pOUT[i].Result[j].ON, Ndb->pON[ONcode].SUR);
			} else {
				strcpy(pOUT[i].Result[j].ON, Ndb->pON[ONcode].ON);
			}

			//Return any Action text associated with this ON
			pOUT[i].Result[j].ACT[0] = 0;
			if (Ndb->pON[ONcode].ACT[0] != 0) {
				strcpy(pOUT[i].Result[j].ACT, Ndb->pON[ONcode].ACT);
			}
		}
	}

	return 0;
}

int RemoveEnvelopments(NdbData *Ndb, RECdata *pRD) {
	//
	//	Remove any ON that is fully enveloped and has a Composite Score
	//	lower than the enveloper.
	//
	//----------

	int redo;
	int sort;
	int i, j, k;
	int ilen, jlen;
	Rdata pt;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nRemoveEnvelopments():... ");
		T = clock();
	}

	// sort pRD->pR[] by the Composite Score
	sort = 1;
	while (sort == 1) {
		sort = 0;
		for (i = 1; i < pRD->Rcount; i++) {
			for (j = i; j < pRD->Rcount; j++) {
				if (pRD->pR[j].C < pRD->pR[j+1].C) {
					pt = pRD->pR[j+1];
					pRD->pR[j+1] = pRD->pR[j];
					pRD->pR[j] = pt;
					sort = 1;
				}
			}
		}
	}

	redo = 1;
	while (redo == 1) {
		redo = 0;

		//Check for envelopments from the most powerful (CS value) to the least powerful...
		for (i = 1; i <= pRD->Rcount; i++) {
			if (redo > 0) break; //get out of i-forloop

			//Ignore ONs that are too short
			if (pRD->pR[i].B == pRD->pR[i].E) continue;

			//Is this ON well enough recognized (by %recognition) to envelop anything?
			if (pRD->pR[i].PER < ENVELOPMENT_THRESHOLD) continue; //No

			//Start comparing ONs from the bottom up...
			for (j = pRD->Rcount; j > i; j--) {
				//Is this ON fully enveloped by pRD->pR[i]?
				if ((pRD->pR[i].B <= pRD->pR[j].B) && (pRD->pR[j].E <= pRD->pR[i].E)) { //Yes...

					//Keeping 'perfect' ONs (%recognized=100) from being removed
					//helps define the boundaries of less well recognized ONs. 
					if (pRD->pR[j].PER < 100) {
						//But pRD->pR[j] cannot be removed by an ON that is recognized by fewer RNs...
						for (ilen = 0; ilen < INQUIRY_LENGTH; ilen++) if (pRD->pR[i].dpos[ilen] == 0) break;
						for (jlen = 0; jlen < INQUIRY_LENGTH; jlen++) if (pRD->pR[j].dpos[jlen] == 0) break;
						if (jlen <= ilen) {
							//remove pRD->pR[j] by moving everyone below up
							for (k = j; k < pRD->Rcount; k++) pRD->pR[k] = pRD->pR[k+1];
							pRD->Rcount--;
							redo = 1;
							break;
						}
					}
				}
			}
		}
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

	return 0;
}

int RetractAnomalousBoundaries(NdbData *Ndb, RECdata *pRD) {
	//
	//	Remove an ONs trailing RNs if they belong to another ON
	//
	//----------

	int h, i, j, k;
	int len;
	int p, b;
	int newE;
	int dpos1, dpos2;
	int qpos1, qpos2;
	int fnd, ok;
	int x, z;
	Rdata Rtest;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nRetractAnomalousBoundaries():... ");
		T = clock();
	}

	for (i = 1; i <= pRD->Rcount; i++) {

		//Don't retract the boundary of a well-recognized ON (%recognition = 100)
		if (pRD->pR[i].PER == 100) continue;

		//Check for anomalies starting from the end of the RN data pRD->pR[i].dpos.
		for (len = 0; len < INQUIRY_LENGTH; len++) if (pRD->pR[i].dpos[len] == 0) break;

		p = 0;
		for (j = (len - 1); j > 0; j--) {
			dpos1 = pRD->pR[i].dpos[j];
			dpos2 = pRD->pR[i].dpos[j-1];
			if ((dpos2 - dpos1) != 1) {
				p = j;
				break;
			}
		}

		if (p > 0) { //Found an anomaly at location p

			//See if pulling back the End Boundary increases the ON's Composite Score.
			//pRD->pR[i].qpos has the positions of the Bound Section from the Input Stream
			newE = p - 1; //This is the position of a possible new End Boundary if the anomaly is "given up"

			b = pRD->pR[i].qpos[newE] + 1; //Does any other ON have this b as a Begin Boundary?

			for (j = 1; j <= pRD->Rcount; j++) {
				if (pRD->pR[j].B != b) continue;

				//Ignore overlapping by the same ON
				if ((pRD->pR[i].ONcode == pRD->pR[j].ONcode) && (b <= pRD->pR[i].E)) continue;
				
				//Ignore pRD->pR[j] if it is less well recognized than pRD->pR[i]: %recognized - quality
				x = pRD->pR[i].PER - pRD->pR[i].QUAL;
				z = pRD->pR[j].PER - pRD->pR[j].QUAL;
				if (x > z) continue;

				//The trailing part of pRD->pR[i] to be removed is from b to pRD->pR[i].E
				//These are array elements p to the end
				//Is all of that found in pRD->pR[j]?
				ok = 1; //assume yes
				k = p;
				while (pRD->pR[i].qpos[k] != 0) {
					qpos1 = pRD->pR[i].qpos[k]; //Is this qpos in pRD->pR[j].qpos[]?
					fnd = 0;
					for (h = 0; h < INQUIRY_LENGTH; h++) {
						qpos2 = pRD->pR[j].qpos[h];
						if (qpos2 == 0) break; //searched to the end
						if (qpos2 != qpos1) continue;
						fnd = 1;
						break;
					}
					if (fnd == 0) {
						ok = 0;
						break;
					}
					k++; //get next qpos1
				}
				if (ok == 0) continue; //The part to be removed is NOT in pRD->pR[j]...
	

				//Will this retraction give pRD->pR[i] a higher Composite Score?
				Rtest = pRD->pR[i]; //Save the original
				LoadR(Ndb, pRD, i, pRD->pR[i].B, (b-1), pRD->pR[i].ONcode);
				if (Rtest.C > pRD->pR[i].C) { //Compare Composite Scores
					// The retracted boundary is NOT better, restore the original...
					pRD->pR[i] = Rtest;
				} else {
					break; //Check the next pRD->pR[i]...
				}
			}
		}
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

	return 0;
}

int RetractWeakBoundaries(NdbData *Ndb, RECdata *pRD) {
	//
	//	Retract over-reaching boundaries of less-dominate ONs, but only if
	//	the more dominate ON is preceded by a Space[].
	//
	//----------

	int i, j, k;
	int b, e;
	int fnd;
	int qpos;
	int topPER;
	int BB, EB;
	Rdata Rtest;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nRetractWeakBoundaries():... ");
		T = clock();
	}

	//Sort by Begin Boundary
	fnd = 1;
	while (fnd == 1) {
		fnd = 0;
		for (i = 1; i < pRD->Rcount; i++) {
			for (j = i; j < pRD->Rcount; j++) {
				if (pRD->pR[j].B > pRD->pR[j+1].B) {
					Rtest = pRD->pR[j+1];
					pRD->pR[j+1] = pRD->pR[j];
					pRD->pR[j] = Rtest;
					fnd = 1;
				}
			}
		}
	}

	//Get the highest value for PER (%recognition)
	topPER = 0;
	for (i = 1; i <= pRD->Rcount; i++) {
		if (pRD->pR[i].PER > topPER) topPER = pRD->pR[i].PER;
	}

	for (j = 0; j < INQUIRY_LENGTH; j++) pRD->Owned[j] = 0;
	
	fnd = 0;
	for (i = 1; i <= pRD->Rcount; i++) {
		if (pRD->pR[i].PER != topPER) continue;
		b = pRD->pR[i].B;
		if (b == 1) continue; // Begin Boundary b=1 is NOT preceded by a pRD->Space[]
		if (pRD->Space[b] == 0) continue;

		//Found an interior, top-level ON with a pRD->Space[] at its Begin Boundary
		//Mark all of its qpos's as 'owned' by a Top-level ON...
		for (j = 0; j < INQUIRY_LENGTH; j++) {
			qpos = pRD->pR[i].qpos[j];
			if (qpos == 0) break; //end of the list
			pRD->Owned[qpos] = 1;
		}
		fnd = 1;
	}

	if (fnd == 1) {

		//Are there any ONs below the TOP which should retract their boundaries?
		for (i = 1; i <= pRD->Rcount; i++) {

			if (pRD->pR[i].PER == topPER) continue;

			//Does any part of this weaker ON overlap the territory owned by Top-level ONs?
			fnd = 0;
			for (j = 0; j < INQUIRY_LENGTH; j++) {
				qpos = pRD->pR[i].qpos[j];
				if (qpos == 0) break; //end of the ON's qpos list
				if (pRD->Owned[qpos] == 1) {
					fnd = 1;
					break;
				}
			}

			if (fnd == 0) continue; //Ignore this 'weak' ON, there's no overlap...

			//Of all the 'weak' ONs with pRD->pR[i]'s Begin Boundary, pick the longest one...
			BB = pRD->pR[i].B;
			EB = 0;
			k = 0;
			for (j = 1; j <= pRD->Rcount; j++) {
				if (pRD->pR[j].B != BB) {
					if (EB == 0) {
						continue;
					} else {
						break; //The ONs are already sorted by Begin Boundary
					}
				}
				if (pRD->pR[j].PER == topPER) continue;
				if (pRD->pR[j].E > EB) {
					EB = pRD->pR[j].E;
					k = j;
				}
			}

			//Find new Begin & End Boundaries of the stripped pRD->pR[i]
			b = 0; //Begin Boundary
			e = 0; //End Boundary
			for (j = 0; j < INQUIRY_LENGTH; j++) {
				qpos = pRD->pR[k].qpos[j];
				if (qpos == 0) break; //end of the list
				if (pRD->Owned[qpos] == 1) continue;
				if (b == 0) b = qpos;
				e = qpos;
			}

			//Load new competition data for this reduction of pRD->pR[i]
			LoadR(Ndb, pRD, k, b, e, pRD->pR[k].ONcode);
			if (pRD->pR[k].PER < RETRACT_BOUNDARY_THRESHOLD) {
				//Remove pRD->pR[k] by moving those below up.
				for (j = k; j < pRD->Rcount; j++) pRD->pR[j] = pRD->pR[j+1];
				pRD->Rcount--;
				i--; //Don't skip over any
			}
		}
	}

	//Eliminate any duplicates
	for (i = 1; i < pRD->Rcount; i++) {
		for (j = (i+1); j < pRD->Rcount; j++) {
			if (pRD->pR[i].B != pRD->pR[j].B) break;
			if (pRD->pR[i].E != pRD->pR[j].E) continue;
			if (pRD->pR[i].ONcode != pRD->pR[j].ONcode) continue;
			//Move all those below up one...
			for (k = j; k < pRD->Rcount; k++) pRD->pR[k] = pRD->pR[k+1];
			pRD->Rcount--;
		}
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

	return 0;
}

int ExpandBoundaries(NdbData *Ndb, RECdata *pRD) {
	//
	//	Can any of the ONs have their BEGIN and/or END Boundaries expanded by a leading
	//	or trailing pattern?
	//
	//	Ignore other ONs and go directly to the Input Stream to check for patterns.
	//	Take over any in-order RN neighbors. When the new Bound Section is established,
	//	kill any enveloped ONs.
	//
	//----------

	//Create a list of positions that are 'owned' by the most powerful ONs.
	//Ownership is established from the longest powerful ON to the shortest.
	//Boundary crossings are not allowed: a shorter ON cannot intrude on the
	//territory owned by a longer/stronger ON
	typedef struct {
		int B; //Begin Boundary;
		int E; //End Boundary
		int len;
	} Owners;
	Owners pOwner[INQUIRY_LENGTH];
	int OWNcount = 0;

	int i, j;
	int B, E, END;
	int topPER, nextPER;
	int redo;
	int len, maxlen, fnd;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nExpandBoundaries():... ");
		T = clock();
	}

	topPER = 0;
	END = 0;
	for (i = 1; i <= pRD->Rcount; i++) {
		if (pRD->pR[i].E > END) END = pRD->pR[i].E; //END = the last possible End Boundary
		if (pRD->pR[i].PER > topPER) topPER = pRD->pR[i].PER;
	}

	//Positions in pRD->Owned[] are NOT reset even though,
	//as powerful ONs expand, they may become somewhat weaker.
	for (j = 0; j < INQUIRY_LENGTH; j++) pRD->Owned[j] = 0;

	//Check for Boundary Expansions from the most powerful to the least...
	//Every candidate MUST have ALL=1, i.e. the ON is reognized by ALL of its RNs, even if out-of-order.
	//Among these, check from highest PER to lowest:
	while (topPER > 0) { //This only has value if ALL=1
	
		redo = 1;
		while (redo == 1) {
			redo = 0;

			for (i = 1; i <= pRD->Rcount; i++) {

				//Does this ON have perfect recognition?
				if (pRD->pR[i].PER != 100) continue; //no
				if (pRD->pR[i].QUAL != 0) continue; //no

				//Mark all of its qpos's as 'owned' by a perfectly recognized ON...
				//NOTE: this information should be retained during further loops through this code
				B = pRD->pR[i].B;
				E = pRD->pR[i].E;
				len = E - B + 1;
				fnd = 0;
				for (j = 0; j < OWNcount; j++) {
					if (pOwner[j].B != B) continue;
					if (pOwner[j].E != E) continue;
					if (pOwner[j].len != len) continue;
					fnd = 1;
					break;
				}
				if (fnd == 0) {
					pOwner[OWNcount].B = B;
					pOwner[OWNcount].E = E;
					pOwner[OWNcount].len = len;
					OWNcount++;
				}
			}

			//Mark the positions 'owned' by powerful ONs, from the longest to the shortest...
			maxlen = 0;
			for (i = 0; i < OWNcount; i++) if (pOwner[i].len > maxlen) maxlen = pOwner[i].len;
			while (maxlen > 0) {
				for (i = 0; i < OWNcount; i++) {
					if (pOwner[i].len == maxlen) {
						B = pOwner[i].B;
						E = pOwner[i].E;
						//Does anybody already 'own' some of this Bound Section?
						fnd = 0; //assume nobody does
						for (j = B; j <= E; j++) {
							if (pRD->Owned[j] != 0) {
								fnd = 1;
								break;
							}
						}
						if (fnd == 0) for (j = B; j <= E; j++) pRD->Owned[j] = 1;
					}
				}
				//Get the next 'maxlen' below this maxlen
				len = maxlen;
				maxlen = 0;
				for (i = 0; i < OWNcount; i++) {
					if ((pOwner[i].len < len)&&(pOwner[i].len > maxlen)) maxlen = pOwner[i].len;
				}
			}
		
			//Check for Boundry Expansions for any ON with ALL=1 and PER=topPER
			for (i = 1; i <= pRD->Rcount; i++) {
				if (pRD->pR[i].ALL != 1) continue;
				if (pRD->pR[i].PER != topPER) continue;

				B = pRD->pR[i].B;
				E = pRD->pR[i].E;

				if (B > 1) {
					if (((B-1) > 1)&&((pRD->Space[B-1] == 0))) { //Do not expand across a pRD->Space[]
						if ((E - B + 1) > 1) { //A length of one doesn't get to expand

							redo=ExpandBeginBoundary(Ndb, pRD, i);
							if (redo == 1) break;
						}
					}
				}
				if (E < END) {
					if (pRD->Space[E+1] == 0) { //Do not expand across a pRD->Space[]
						if ((E - B + 1) > 1) { //A length of one doesn't get to expand

							redo=ExpandEndBoundary(Ndb, pRD, i);
							if (redo == 1) break;
						}
					}
				}
			}
		}

		//Find the next lowest value for topPER...
		nextPER = 0;
		for (i = 1; i <= pRD->Rcount; i++) {
			if ((pRD->pR[i].PER > nextPER) && (pRD->pR[i].PER < topPER)) nextPER = pRD->pR[i].PER;
		}
		topPER = nextPER;
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

	return 0;
}

int ExpandBeginBoundary(NdbData *Ndb, RECdata *pRD, int Rindex) {
	//
	//	Check the leading RNs from the Input Stream to see if there's a pattern that
	//	can expand this ON's BEGIN Boundary.
	//
	//		These should work:
	//			FRIFRIFRFRIFRFRIDAY
	//			miMISSISSIPPI
	//
	//		BUT this FAILS...
	//			missiMISSISSIPPI
	//
	//		The fundamental issue is when does a pattern become repeating words?
	//
	//----------

	int i, j, k, p;
	int redo;
	int stop;
	int pos, end;
	int ONcode;
	int Len, patlen, ln;
	int B1, E1;
	int RNcode;
	int ok, fnd;
	int Pi, Pj;
	int zln,zok;

	redo = 0;
	ONcode = pRD->pR[Rindex].ONcode;
	Len = Ndb->pON[ONcode].Len;

	B1 = pRD->pR[Rindex].B;
	E1 = pRD->pR[Rindex].E;

	stop = 0;
	while (stop == 0) {
		stop = 1;

		//rn = last character at the end of the neighboring ON
		RNcode = pRD->ISRN[B1 - 2]; // -2 because pRD->ISRN[] starts at 0, but boundaries start at 1

		//Does this RN exist in pRD->pR[Rindex] BEFORE E1?
		for (end = 0; end < INQUIRY_LENGTH; end++) if (Ndb->pON[ONcode].RL[end] == 0) break;
		end--; //this is the last position in the list

		pos = 0;
		for (i = 0; i < (Len-1); i++) { //Don't want to go all the way to the end, only go from 0 to Len-2
			if (Ndb->pON[ONcode].RL[i] == RNcode) {
				pos = i + 1;
				break;
			}
		}

		if (pos == 0) break;
		patlen = pos; //length of the pattern

		//A pattern is now defined from pRD->pR[Rindex].rn[0] to pRD->pR[Rindex].rn[pos-1]
		//Is this pattern blocked by a Space[]?
		//But allow for a Space at the beginning (B1-patlen).
		ok = 1;
		for (i = B1; i > (B1-patlen); i--) {
			if (i > 1) {
				if (pRD->Space[i] == 1) { //These i's are Boundaries: 1, 2, 3, ...
					ok = 0;
					break;	//Don't expand across a Space
				}
			}
		}
		if (ok == 0) break;

		//Is the pattern made of RNs from perfect ONs?
		ok = 0;
		for (i = (B1-patlen); i < B1; i++) {
			if (pRD->Owned[i] != 1) { // [i] like qpos
				ok = 1;
				break;	//Not perfect
			}
		}
		if (ok == 0) break;


		//A pattern has been found in the ON, such as FRI at the beginning of FRIDAY
		//Does this pattern exist from INPUT(B1-patlen-1) to INPUT(B1-1-1) ?
		ok = 1;
		p = 0;
		for (i = (B1-1-patlen); i < (B1-1); i++) { //-1 because these start at ZERO
			if (pRD->ISRN[i] != Ndb->pON[ONcode].RL[p]) { //Same RNcodes?
				ok = 0;
				break;
			}
			p++;
		}
		if (ok == 0) break; //no pattern
		
		//Is this expansion allowed by other ONs?
		ok = 1; //assume yes
		for (j = 1; j <= pRD->Rcount; j++) {
			
			if (j == Rindex) continue; //self

			if (pRD->pR[j].ONcode == 0) continue; //This record has been deleted...
			
			if (pRD->pR[j].E < (B1-patlen)) continue; //out of range
			
			if ((pRD->pR[j].PER != 100)||(pRD->pR[j].QUAL != 0)) continue; //not powerful enough
			
			if (pRD->pR[j].B > (B1-patlen)) continue; //A 'perfect' ON is enveloped by this pattern

			//Cannot cross a powerful ON, UNLESS more RNs can be gained on the other side...
			//Find the previous RN in the Input Stream BEFORE the current beginning of pRD->pR[Rindex]
			fnd = 0;
			RNcode = pRD->ISRN[B1-1-patlen-1]; //go one position beyond the beginning of the pattern

			//Does this RN also exist in pRD->pR[Rindex] BEFORE E1?
			pos = 0;
			for (i = 0; i < (Len-1); i++) { //don't want to go all the way to the end, only go from 0 to Len-2
				if (Ndb->pON[ONcode].RL[i] == RNcode) {
					pos = i + 1;
					break;
				}
			}

			if (pos != 0){
				zln = pos; //length of the pattern

				//Does this pattern exist from INPUT(B1-patlen-zln-1) to INPUT(B1-1-patlen-1) ?
				zok = 1;
				p = 0;
				for (i = (B1-patlen-zln-1); i < (B1-1-patlen); i++) {
					if (pRD->ISRN[i] != Ndb->pON[ONcode].RL[p]) {
						zok = 0;
						break;
					}
					p++;
				}
				if (zok == 1) fnd = 1; //found a pattern beyond the 1st one

			}
			if (fnd == 1) continue;
			ok = 0;
		}
		if (ok == 0) break;	//Cannot cross over powerful ONs

		//Load new competition data for this expansion of pRD->pR[Rindex]
		LoadR(Ndb, pRD, Rindex, (B1-patlen), E1, ONcode);
		B1 = pRD->pR[Rindex].B; //The new Begin Boundary

		stop = 0; //Now that pRD->pR[Rindex] is different, run through this loop again
		redo = 1; //This will force a reassessment of the entire Rcount list when this loop finally exits

		//Are there weaker ONs that are now enveloped by pRD->pR[Rindex]'s new Bound Section?
		//Get a measure of the 'power' of each record
		//pRD->pR[j] cannot be removed by an ON that is recognized by fewer RNs...
		for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[Rindex].dpos[ln] == 0) break;
		Pi = pRD->pR[Rindex].PER * ln;
		for (j = 1; j <= pRD->Rcount; j++) {
			if (j == Rindex) continue;
			if (pRD->pR[j].ONcode == 0) continue; //deleted record
			if ((pRD->pR[j].E < B1) || (pRD->pR[j].B > E1)) continue; //out of range
			if ((pRD->pR[j].PER == 100) && (pRD->pR[j].QUAL == 0)) continue; //Don't mess with a 'perfect' ON

			if ((B1 <= pRD->pR[j].B) && (pRD->pR[j].E <= E1)) { //enveloped
				for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[j].dpos[ln] == 0) break;
				Pj = pRD->pR[j].PER * ln;
				if ((Pi > Pj) || (pRD->pR[j].ONcode == ONcode)) {
					pRD->pR[j].ONcode = 0; //record deleted
				}
			}
		}
	}

	//Records may have been deleted during this loop.
	//If so, move all non-empty records up and reset Rcount
	k = 0; //new record count
	for (i = 1; i <= pRD->Rcount; i++) {
		if (pRD->pR[i].ONcode != 0) {
			k++;
			continue;
		}
		//Found an empty record at pRD->pR[i]...
		for (j = (i+1); j <= pRD->Rcount; j++) {
			if (pRD->pR[j].ONcode == 0) continue;
			//found a record for pRD->pR[i]
			pRD->pR[i] = pRD->pR[j];
			pRD->pR[j].ONcode = 0;
			k++;
			break; //get the next i
		}
	}
	pRD->Rcount = k;

	return redo;
}

int ExpandEndBoundary(NdbData *Ndb, RECdata *pRD, int Rindex) {
	//
	//	Check the follow-on RNs from the Input Stream to see if there's a pattern that
	//	can expand this ON's END Boundary.
	//
	//----------

	int i, j, k, p;
	int redo;
	int stop;
	int pos, end;
	int ONcode;
	int Len, patlen, ln;
	int B1, E1;
	int RNcode;
	int ok;
	int Pi, Pj;

	redo = 0;
	ONcode = pRD->pR[Rindex].ONcode;
	Len = Ndb->pON[ONcode].Len;

	B1 = pRD->pR[Rindex].B;
	E1 = pRD->pR[Rindex].E;

	stop = 0;
	while (stop == 0) {
		stop = 1;

		RNcode = pRD->ISRN[E1]; // It's E1, not E1+1 because pRD->ISRN[] starts at 0, but boundaries start at 1

		//Does this RN exist in Ndb->pON[ONcode].RL AFTER B1?
		for (end = 0; end < INQUIRY_LENGTH; end++) {
			if (Ndb->pON[ONcode].RL[end] == 0) break;
		}
		end--; //this is the last position in the list

		pos = 0;
		for (i = (Len-1); i > 0; i--) { //Don't want to go all the way to the beginning, only go from Len-1 to 1
			if (Ndb->pON[ONcode].RL[i] == RNcode) {
				pos = i;
				break;
			}
		}
		if (pos == 0) break;

		patlen = end - pos + 1; //pos & end are positions in a list that starts at zero

		//A pattern is now defined from Ndb->pON[ONcode].RL[pos] to Ndb->pON[ONcode].RL[end]
		//Is this pattern blocked by a Space[]?
		ok = 1;
		for (i = (E1+1); i < (E1+patlen+1); i++) {
			if (i > end) {
				if (pRD->Space[i] == 1) { //Space[] is 1, 2, 3, ...
					ok = 0;
					break;	//Don't expand across a Space
				}
			}
		}
		if (ok == 0) break;

		//Is the pattern made of RNs from perfect ONs?
		ok = 0;
		for (i = E1+1; i < (E1+patlen+1); i++) {
			if (pRD->Owned[i] != 1) { //Owned[] is 1, 2, 3, ...
				ok = 1;
				break;	//Not perfect
			}
		}
		if (ok == 0) break;

		//A pattern has been found in the ON, such as AY at the end of FRIDAY
		//Does this pattern exist from INPUT(E1) to INPUT(E1+patlen-1) ?
		ok = 1;
		p = pos;
		for (i = E1; i < (E1+patlen); i++) {
			if (pRD->ISRN[i] != Ndb->pON[ONcode].RL[p]) {
				ok = 0;
				break;
			}
			p++;
		}
		if (ok == 0) break; //no pattern

		//Load new competition data for this expansion of pRD->pR[Rindex]
		LoadR(Ndb, pRD, Rindex, B1, (E1+patlen), ONcode);
		E1 = pRD->pR[Rindex].E;

		stop = 0; //Now that pRD->pR[Rindex] is different, run through this loop again
		redo = 1; //This will force a reassessment of the entire pRD->Rcount list when this loop finally exits

		//Are there weaker ONs that are now enveloped by pRD->pR[Rindex]'s new Bound Section?
		//Get a measure of the 'power' of each record
		//pRD->pR[j] cannot be removed by an ON that is recognized by fewer RNs...
		for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[Rindex].dpos[ln] == 0) break;
		Pi = pRD->pR[Rindex].PER * ln;
		for (j = 1; j <= pRD->Rcount; j++) {
			if (j == Rindex) continue;
			if (pRD->pR[j].ONcode == 0) continue; //deleted record
			if ((pRD->pR[j].E < B1) || (pRD->pR[j].B > E1)) continue; //out of range
			if ((pRD->pR[j].PER == 100) && (pRD->pR[j].QUAL == 0)) continue; //Don't mess with a 'perfect' ON

			if ((B1 <= pRD->pR[j].B) && (pRD->pR[j].E <= E1)) { //enveloped
				for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[j].dpos[ln] == 0) break;
				Pj = pRD->pR[j].PER * ln;
				if ((Pi > Pj) || (pRD->pR[j].ONcode == ONcode)) {
					pRD->pR[j].ONcode = 0; //record deleted
				}
			}
		}
	}

	//Records may have been deleted during this loop.
	//If so, move all non-empty records up and reset Rcount
	k = 0; //new record count
	for (i = 1; i <= pRD->Rcount; i++) {
		if (pRD->pR[i].ONcode != 0) {
			k++;
			continue;
		}
		//Found an empty record at pRD->pR[i]...
		for (j = (i+1); j <= pRD->Rcount; j++) {
			if (pRD->pR[j].ONcode == 0) continue;
			//found a record for pRD->pR[i]
			pRD->pR[i] = pRD->pR[j];
			pRD->pR[j].ONcode = 0;
			k++;
			break; //get the next i
		}
	}
	pRD->Rcount = k;

	return redo;
}

void RemoveWeakONs(NdbData *Ndb, RECdata *pRD) {
	//
	//	Remove competitors that fall below the WEAK_ON_THRESHOLD
	//
	//----------

	int i, j;
	int len;
	float power;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nRemoveWeakONs():... ");
		T = clock();
	}

	for (i = 1; i <= pRD->Rcount; i++) {
		len = pRD->pR[i].E - pRD->pR[i].B + 1;
		power = pRD->pR[i].C * (float)len;
		if ((power - 0.001) > WEAK_ON_THRESHOLD) continue;
 
		// Remove pRD->pR[i] by moving all those below up
		for (j = i; j < pRD->Rcount; j++) pRD->pR[j] = pRD->pR[j+1];
		pRD->Rcount--;
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

}

void GetAnomalyCounts(RECdata *pRD) {
	//
	//	Count the qpos and dpos anomalies AFTER any boundary adjustments
	//
	//----------

	int i, j;
	int ln, x, y;
	int cntA;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nGetAnomalyCounts():... ");
		T = clock();
	}

	for (i = 1; i <= pRD->Rcount; i++) {

		cntA = 0;

		for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[i].qpos[ln] == 0) break;

		//The Begin Boundary should match the beginning of pRD->pR[i].qpos[]
		x = pRD->pR[i].B - pRD->pR[i].qpos[0];
		if (x < 0) x = -x;
		cntA = cntA + x;

		for (j = 0; j < (ln-1); j++) {
			x = pRD->pR[i].qpos[j];
			y = pRD->pR[i].qpos[j+1];
			x = y - x;
			if (x != 1) {
				if (x < 0) x = -x;
				cntA = cntA + x;
				j++; //Skip past the problem
			}
		}
		
		//Ideally pRD->pR[i].dpos[0] should be 1
		for (ln = 0; ln < INQUIRY_LENGTH; ln++) if (pRD->pR[i].dpos[ln] == 0) break;
		
		if ((ln == 1) && (pRD->pR[i].dpos[0] != 1)) {
			cntA = cntA + (pRD->pR[i].dpos[0] - 1);
		} else {
			for (j = 0; j < (ln-1); j++) {
				x = pRD->pR[i].dpos[j];
				y = pRD->pR[i].dpos[j+1];
				if ((j == 0)&&(x != 1)) {
					cntA = cntA + (x - 1);
				}
				x = y - x;
				if (x != 1) {
					if (x < 0) x = -x;
					cntA = cntA + x;
					j++; //Skip past the problem
				}
			}
		}
		pRD->pR[i].cntA = cntA;
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

}

int MakeBranches(NdbData *Ndb, RECdata *pRD) { //Ndb included for any diagnostic code
	//
	//	Combine ONs, boundary-to-boundary, into branches. Each branch is a possible
	//	solution to the inquiry. The process finishes when no more ONs can be added
	//	to any branch.
	//
	//	They are called "branches" because some ON's End Boundary any accomodate multiple ONs
	//	with appropriate Begin Boundaries, forcing a fork in an initially linear structure.
	//
	//----------

	int i, j, r;
	int lowB, highB;
	int Tlen;
	float TC;
	int fnd;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nMakeBranches():... ");
		T = clock();
	}

	pRD->BRHcount = 0;
	pRD->BRcount = 0;

	// Initialization: find the lowest Begin Boundary
	lowB = INQUIRY_LENGTH;
	highB = 0;
	for (r = 1; r <= pRD->Rcount; r++) {
		if (pRD->pR[r].B < lowB) lowB = pRD->pR[r].B;
		pRD->pR[r].InBranch = 0; //Also make sure no ON is assigned to a branch yet
	}

	//Store all the Branch Heads: they all begin with the lowest Begin Boundary: lowB
	//Use these few ONs to set highB as a cutoff for branches that may be created with
	//any Unused ONs. These would be ONs with Begin Boundaries that are after lowB AND
	//before highB, i.e. Bound Sections that start in the middle of a lowB ON. They will
	//never attach to an ON, but they nevertheless might be legitimate solutions.
	for (r = 1; r <= pRD->Rcount; r++) {
		if (pRD->pR[r].B != lowB) continue;

		if (pRD->pR[r].E > highB) highB = pRD->pR[r].E;

		addBRHcount(pRD); //Get the next Header, index = pRD->BRHcount
		addBRcount(pRD); //Get the index for the first ON in the branch, index = pRD->BRcount

		//Branch header data
		pRD->pBRH[pRD->BRHcount].First = pRD->BRcount;
		pRD->pBRH[pRD->BRHcount].Last = pRD->BRcount;
		pRD->pBRH[pRD->BRHcount].Tlength = 0;
		pRD->pBRH[pRD->BRHcount].TCscore = 0.0;
		pRD->pBRH[pRD->BRHcount].Deleted = 0;

		//First branch member
		pRD->pBR[pRD->BRcount].R = r; //Points back at its ON data
		pRD->pBR[pRD->BRcount].Next = 0;
		pRD->pBR[pRD->BRcount].Prev = 0;

		pRD->pR[r].InBranch = 1; //Flag this ON as already part of a branch
	}

	BuildBranches(Ndb, pRD);

	//Can any Branches be made out of any 'overlooked' ONs?
	//For these it doesn't matter what the Begin Boundary
	//is as long as it's less than or equal to highB.
	fnd = 0;
	for (r = 1; r <= pRD->Rcount; r++) {
		if (pRD->pR[r].InBranch != 0) continue;
		if (pRD->pR[r].PER <= UNUSED_THRESHOLD) continue;

		if (pRD->pR[r].B > highB) continue; //This ON's Bound Section starts too far down the Input Stream

		addBRHcount(pRD); //Get next BRHcount
		addBRcount(pRD); //Get next BRcount

		//Branch header data
		pRD->pBRH[pRD->BRHcount].First = pRD->BRcount;
		pRD->pBRH[pRD->BRHcount].Last = pRD->BRcount;
		pRD->pBRH[pRD->BRHcount].Tlength = 0;
		pRD->pBRH[pRD->BRHcount].TCscore = 0.0;
		pRD->pBRH[pRD->BRHcount].Deleted = 0;

		//First branch member
		pRD->pBR[pRD->BRcount].R = r; //Points back at its ON data
		pRD->pBR[pRD->BRcount].Next = 0;
		pRD->pBR[pRD->BRcount].Prev = 0;

		pRD->pR[r].InBranch = 1;

		fnd++;
	}
	
	if (fnd > 0) BuildBranches(Ndb, pRD);

	//For each viable branch, get its total length and its cumulative Composite Score
	for (i = 1; i <= pRD->BRHcount; i++) {
		if (pRD->pBRH[i].Deleted == 1) continue;
		j = pRD->pBRH[i].First;
		r = pRD->pBR[j].R;
		Tlen = pRD->pR[r].E - pRD->pR[r].B + 1;
		TC = pRD->pR[r].C;

		while (pRD->pBR[j].Next > 0) {
			j = pRD->pBR[j].Next;
			r = pRD->pBR[j].R;
			Tlen = Tlen + (pRD->pR[r].E - pRD->pR[r].B + 1);
			TC = TC + pRD->pR[r].C;
		}
		pRD->pBRH[i].Tlength = Tlen;
		pRD->pBRH[i].TCscore = TC;
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

	return 0;
}

void BuildBranches(NdbData *Ndb, RECdata *pRD) {
	//
	//	Search pRD->pR[] for ONs to add to the branches
	//
	//----------

	int i, j, k, r;
	int BRHcnt;
	int BRend;
	int add;
	int B, E;
	int fnd;
	int ok;
	int PER;

	//Can an ON be added to some End Boundary?
	//If there are multiple such ONs (all with the same Begin Boundary), create new
	//branches from copies of the current branch, and add the ONs to the new branches.
	//This process finishes when no more ONs can be found to be added to any branch.
	add = 1;
	while (add > 0) {
		add = 0;
		BRHcnt = pRD->BRHcount; //Get this value now because BRHcount may change in this loop

		for (i = 1; i <= BRHcnt; i++) {
			if (pRD->pBRH[i].Deleted == 1) continue; //Ignore branches that are likely losers

			//Get the last member of the branch...
			//Then get it's End Boundary from pRD->pR[]
			//Then search all of pRD->pR[] for a Begin Boundary to add to this chain. There may be many...
			j = pRD->pBRH[i].Last;
			r = pRD->pBR[j].R;
			E = pRD->pR[r].E; //Find an ON with a Begin Boundary that fits onto this End Boundary (last in the chain)
			B = E + 1;
			fnd = 0;
			for (r = 1; r <= pRD->Rcount; r++) {
				ok = 1;
				if (pRD->pR[r].B < B) {
					continue;
				} else {
					if (pRD->pR[r].B > B) {
						k = pRD->pBRH[i].Last; //current end of the branch
						k = pRD->pBR[k].R;
						if (pRD->pR[k].E > B) ok = 0;
					}
				}
				if (ok == 0) continue;

				fnd++;
				if (fnd == 1) {
					//Add another link to the end of Branch #i...
					addBRcount(pRD);
					BRend = pRD->BRcount; //Remember this index in case you need to make copies of this branch
					pRD->pBR[pRD->BRcount].R = r;
					pRD->pBR[pRD->BRcount].Next = 0;

					pRD->pBR[pRD->BRcount].Prev = j;
					pRD->pBR[j].Next = pRD->BRcount;
					pRD->pBRH[i].Last = pRD->BRcount;
					
					pRD->pR[r].InBranch = 1;
					add++;
				} else {
					//More than one ON has been found with a compatable Begin Boundary. Add this additional
					//ON to a new copy of the branch, BUT don't copy the ON just added above in fnd=1...
					addBRHcount(pRD); //Get new Header: BRHcount
					addBRcount(pRD); //Get new list member: BRcount
					pRD->pBRH[pRD->BRHcount].First = pRD->BRcount;
					pRD->pBRH[pRD->BRHcount].Last = pRD->BRcount;
					pRD->pBRH[pRD->BRHcount].Tlength = 0;
					pRD->pBRH[pRD->BRHcount].TCscore = 0.0;
					pRD->pBRH[pRD->BRHcount].Deleted = 0;
					
					k = pRD->pBRH[i].First; //1st ON in the original branch
					pRD->pBR[pRD->BRcount].R = pRD->pBR[k].R; //Points at its ON data
					pRD->pBR[pRD->BRcount].Next = 0;
					pRD->pBR[pRD->BRcount].Prev = 0;

					while (pRD->pBR[k].Next > 0) { 
						k = pRD->pBR[k].Next;
						if (k == BRend) break; //DO NOT copy the last addition to the branch (from fnd=1 above)
						addBRcount(pRD);
						pRD->pBR[pRD->BRcount-1].Next = pRD->BRcount;
						pRD->pBR[pRD->BRcount].R = pRD->pBR[k].R;
						pRD->pBR[pRD->BRcount].Prev = pRD->BRcount-1;
						pRD->pBR[pRD->BRcount].Next = 0;
						pRD->pBRH[pRD->BRHcount].Last = pRD->BRcount;
					}
					//Now that a new copy of the branch has been made, add the ON to its end...
					addBRcount(pRD);
					pRD->pBR[pRD->BRcount].R = r; //copy the pRD->pR[] index of the ON
					pRD->pBR[pRD->BRcount].Next = 0;
					pRD->pBR[pRD->BRcount].Prev = pRD->BRcount-1;
					pRD->pBR[pRD->BRcount-1].Next = pRD->BRcount;
					pRD->pBRH[pRD->BRHcount].Last = pRD->BRcount;

					pRD->pR[r].InBranch = 1;

				}
			}
		}

		//Cull branches that contain weak ONs
		//'Weakness' is determined by a 'dynamic' threshold that depends on the number of
		//competitors. As the number increase, the threshold becomes more stringent.
		fnd = 0; //number of competitors
		for (i = 1; i <= pRD->BRHcount; i++) {
			if (pRD->pBRH[i].Deleted == 1) continue;
			fnd++;
		}

		if (fnd > 500) {
			for (i = 1; i <= pRD->BRHcount; i++) {
				if (pRD->pBRH[i].Deleted == 1) continue; //This branch can be ignored because it's a likely loser

				//Find the lowest PER (%recognized) in this branch...
				PER = 100;
				j = pRD->pBRH[i].First;
				r = pRD->pBR[j].R;
				if (pRD->pR[r].PER < PER) PER = pRD->pR[r].PER;
				while (pRD->pBR[j].Next > 0) {
					j = pRD->pBR[j].Next;
					r = pRD->pBR[j].R;
					if (pRD->pR[r].PER < PER) PER = pRD->pR[r].PER;
				}
				if (PER < PER_1_THRESHOLD) {
					pRD->pBRH[i].Deleted = 1; //This branch contains a too-weak ON
					continue; //get the next branch head
				} else {
					if (fnd > 1000) {
						if (PER < PER_2_THRESHOLD) {
							pRD->pBRH[i].Deleted = 1;
							continue;
						} else {
							if (fnd > 3000) {
								if (PER < PER_3_THRESHOLD) {
									pRD->pBRH[i].Deleted = 1;
									continue;
								}
							}
						}
					}
				}
			}
		}
	}
}

void addBRHcount(RECdata *pRD) {
	//
	//	Increment BRHcount and add more memory if necessary
	//
	//----------

	pRD->BRHcount++;

	if ((pRD->BRHcount % BR_RECORDS) == 0) { // The list is full
		pRD->BRHblocks++;
		pRD->pBRH = realloc(pRD->pBRH, (pRD->BRHblocks * BR_RECORDS * sizeof(BRHdata))); //pBRH[] branch head
	}
}

void addBRcount(RECdata *pRD) {
	//
	//	Increment BRcount and add more memory if necessary
	//
	//----------

	pRD->BRcount++;

	if ((pRD->BRcount % BR_RECORDS) == 0) { // The list is full
		pRD->BRblocks++;
		pRD->pBR = realloc(pRD->pBR, (pRD->BRblocks * BR_RECORDS * sizeof(BRdata))); //pBR[] branch member
	}
}

int CompeteBranches(NdbData *Ndb, RECdata *pRD) {
	//
	//	Compete the following select groups of branches in the SCU to get the final winner:
	//		a) all branches with the same length as the branch with the highest cumulative Composite Score
	//		b) al1 branches that have the longest length
	//
	//----------

	int ONcode;
	int i, j, b, r;
	int LongLen, LongLenC;
	float maxC;
	int cnt;
	int res;
	int Rcount;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nCompeteBranches()  #Branches=%d... ", pRD->BRHcount);
		T = clock();
	}

	//What is the length of the branch with the highest cumulative Composit Score?
	//Also, what is the longest length among all the branches?
	maxC = 0.0;
	LongLenC = 0;
	LongLen = 0;
	for (i = 1; i <= pRD->BRHcount; i++) {
		if (pRD->pBRH[i].Deleted == 1) continue;

		if (pRD->pBRH[i].Tlength > LongLen) LongLen = pRD->pBRH[i].Tlength;
		
		if (pRD->pBRH[i].TCscore > maxC) {
			maxC = pRD->pBRH[i].TCscore;
			LongLenC = pRD->pBRH[i].Tlength;
		}
	}

	//Load the competitors that have these 2 lengths...
	pRD->Tcount = 0; //Number of tournament competitors
	for (i = 1; i <= pRD->BRHcount; i++) {
		if (pRD->pBRH[i].Deleted == 1) continue;

		if ((pRD->pBRH[i].Tlength == LongLen) || (pRD->pBRH[i].Tlength == LongLenC)) {
			addTcount(pRD);
			pRD->pT[pRD->Tcount].BR = i;
			pRD->pT[pRD->Tcount].SCUscore = 0;
			pRD->pT[pRD->Tcount].Eliminated = 0;
			pRD->pT[pRD->Tcount].NCcount = 0;
			pRD->pT[pRD->Tcount].NCblocks = 1;
			pRD->pT[pRD->Tcount].pNC = (int *)malloc(NC_RECORDS * sizeof(int));
		}
	}

	if (ShowProgress == 1) {
		printf("\n Tournament: Branchs=%d -> Viable Competitors=%d... ", pRD->BRHcount, pRD->Tcount);
	}

	if (pRD->Tcount > 0) {

		res = RunCompetitions(Ndb, pRD); // Run the SCU tournament

		//Find and report the winner(s)...
		Rcount = 0; //number of results: 1, 2, ...
		for (i = 1; i <= pRD->Tcount; i++) {
			if (pRD->pT[i].Eliminated == 1) continue;
			Rcount++;
			cnt = 1;
			b = pRD->pT[i].BR;
			j = pRD->pBRH[b].First;
			r = pRD->pBR[j].R;
			ONcode = pRD->pR[r].ONcode;

			pRD->pRES[Rcount].Result[cnt].ONcode = ONcode;
			pRD->pRES[Rcount].Result[cnt].B = pRD->pR[r].B;
			pRD->pRES[Rcount].Result[cnt].E = pRD->pR[r].E;

			while (pRD->pBR[j].Next > 0) {
				j = pRD->pBR[j].Next;
				r = pRD->pBR[j].R;
				cnt++; //1, 2, 3, ...
				ONcode = pRD->pR[r].ONcode;
				pRD->pRES[Rcount].Result[cnt].ONcode = ONcode;
				pRD->pRES[Rcount].Result[cnt].B = pRD->pR[r].B;
				pRD->pRES[Rcount].Result[cnt].E = pRD->pR[r].E;
			}
			pRD->pRES[Rcount].NumberOfONs = cnt;
			if (Rcount == TOTAL_ALLOWED_RESULTS) break;
		}
		pRD->RESULTcount = Rcount;
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" runtime: %fsec\n", runtime);
	}

	return 0;
}

void addTcount(RECdata *pRD) {
	//
	//	Increment Tcount and add more memory if necessary
	//
	//----------

	pRD->Tcount++;

	if ((pRD->Tcount % T_RECORDS) == 0) { // The list is full
		pRD->Tblocks++;
		pRD->pT = realloc(pRD->pT, (pRD->Tblocks * T_RECORDS * sizeof(Tdata))); //pT[BR]
	}

}

int RunCompetitions(NdbData *Ndb, RECdata *pRD) {
	//
	//	From the list of competitors in pT[], run the tournament
	//
	//	The tournament ends when:
	//		a) there were no competitor in the first place
	//		b) there is only one competitor left
	//		c) the remaining competitors all get identical scores
	//
	//----------
	
	int i, j, k;
	int cnt;
	int ok;
	int CompetitorCount;
	int res;

	int Loops, MaxLoops;

	Loops = 0;
	MaxLoops = pRD->Tcount;
	MaxLoops = MaxLoops * 2;

	while (1) {

		Loops++;
		if (Loops > MaxLoops) break; //The remaining competitors cannot be differentiated, i.e. multiple results.

		CompetitorCount = 0;

		for (i = 1; i <= pRD->Tcount; i++) {
			if (pRD->pT[i].Eliminated == 1) continue;
			CompetitorCount++;
			break;
		}
		if (CompetitorCount == 0) break; //No competitors

		for (j = pRD->Tcount; j >= 1; j--) {
			if (j == i) continue;
			if (pRD->pT[j].Eliminated == 1) continue;

			//Are i and j allowed to compete against each other?
			ok = 1; //assume YES
			for (k = 0; k < pRD->pT[i].NCcount; k++) {
				if (pRD->pT[i].pNC[k] != j) continue;
				ok = 0;
				break;
			}
			if (ok == 0) continue; //Can't compete, get next j

			CompetitorCount++;
			break;
		}

		if (CompetitorCount < 2) break; //It's over

		res = GetWinners(Ndb, pRD, i, j); //Compete i vs j

		//If neither is eliminated (identical scores), don't allow them to compete against each other again
		if ((pRD->pT[i].Eliminated == 0) && (pRD->pT[j].Eliminated == 0)) {

			cnt = pRD->pT[i].NCcount;
			pRD->pT[i].pNC[cnt] = j; //The list is 0 to < pRD->pT[i].NCcount
			cnt++;
			if ((cnt % NC_RECORDS) == 0) { // The list is now full
				pRD->pT[i].NCblocks++;
				pRD->pT[i].pNC = realloc(pRD->pT[i].pNC, (pRD->pT[i].NCblocks * NC_RECORDS * sizeof(int)));
			}
			pRD->pT[i].NCcount = cnt;

			cnt = pRD->pT[j].NCcount;
			pRD->pT[j].pNC[cnt] = i; //The list is 0 to < pRD->pT[i].NCcount
			cnt++;
			if ((cnt % NC_RECORDS) == 0) { // The list is now full
				pRD->pT[j].NCblocks++;
				pRD->pT[j].pNC = realloc(pRD->pT[j].pNC, (pRD->pT[j].NCblocks * NC_RECORDS * sizeof(int)));
			}
			pRD->pT[j].NCcount = cnt;

		}
	}

	return 0;
}

int GetWinners(NdbData *Ndb, RECdata *pRD, int a, int z) {
	//
	//	Compete branch 'A' vs. branch 'Z'
	//
	//	Use the SCU to find the winner of the competition.
	//	
	//	Each competitor contains a list of one or more ONs, each of which has its
	//	own Begin and End boundaries, all of which are aligned by their boundaries.
	//
	//----------

	int i, j, k, b, r;
	int res;

	//Memory for the 2 competitors
	COMP A;
	COMP Z;

	//Fill in the data for player A...
	b = pRD->pT[a].BR; //b = index to the branch
	j = pRD->pBRH[b].First; //index to the lead ON in the branch
	r = pRD->pBR[j].R; //r = index to the ON data
	i = 0;
	A.m[i].B = pRD->pR[r].B;
	A.m[i].E = pRD->pR[r].E;
	A.m[i].ONcode = pRD->pR[r].ONcode;
	A.m[i].nrec = 0; // number of hits in .order[]
	for (k = 0; k < INQUIRY_LENGTH; k++) {
		A.m[i].RLhit[k] = 0;
		A.m[i].order[k].dpos = 0;
		A.m[i].order[k].qpos = 0;
		A.m[i].order[k].rn = 0;
	}
	A.m[i].PER = pRD->pR[r].PER;
	A.m[i].QUAL = pRD->pR[r].QUAL;
	A.m[i].cntA = pRD->pR[r].cntA;

	while (pRD->pBR[j].Next > 0) {
		j = pRD->pBR[j].Next;
		r = pRD->pBR[j].R;
		i++;
		A.m[i].B = pRD->pR[r].B;
		A.m[i].E = pRD->pR[r].E;
		A.m[i].ONcode = pRD->pR[r].ONcode;
		A.m[i].nrec = 0; // number of hits in .order[]
		for (k = 0; k < INQUIRY_LENGTH; k++) {
			A.m[i].RLhit[k] = 0;
			A.m[i].order[k].dpos = 0;
			A.m[i].order[k].qpos = 0;
			A.m[i].order[k].rn = 0;
		}
		A.m[i].PER = pRD->pR[r].PER;
		A.m[i].QUAL = pRD->pR[r].QUAL;
		A.m[i].cntA = pRD->pR[r].cntA;
	}
	A.Mcount = (i + 1); //number of ONs in this player
	A.Score = 0;
	A.spaceB = 0;
	A.anomaly = 0;
	A.rec = 0;
	A.minpr = 0;
	A.bound = 0;
	A.uncount = 0;
	A.mislead = 0;
	for (k = 0; k < INQUIRY_LENGTH; k++) A.SpaceClaim[k] = 0;


	//Fill in the data for player Z...
	b = pRD->pT[z].BR; //b = index to the branch
	j = pRD->pBRH[b].First; //index to the lead ON in the branch
	r = pRD->pBR[j].R; //r = index to the ON data
	i = 0;
	Z.m[i].B = pRD->pR[r].B;
	Z.m[i].E = pRD->pR[r].E;
	Z.m[i].ONcode = pRD->pR[r].ONcode;
	Z.m[i].nrec = 0; // number of hits in .order[]
	for (k = 0; k < INQUIRY_LENGTH; k++) {
		Z.m[i].RLhit[k] = 0;
		Z.m[i].order[k].dpos = 0;
		Z.m[i].order[k].qpos = 0;
		Z.m[i].order[k].rn = 0;
	}
	Z.m[i].PER = pRD->pR[r].PER;
	Z.m[i].QUAL = pRD->pR[r].QUAL;
	Z.m[i].cntA = pRD->pR[r].cntA;
	while (pRD->pBR[j].Next > 0) {
		j = pRD->pBR[j].Next;
		r = pRD->pBR[j].R;
		i++;
		Z.m[i].B = pRD->pR[r].B;
		Z.m[i].E = pRD->pR[r].E;
		Z.m[i].ONcode = pRD->pR[r].ONcode;
		Z.m[i].nrec = 0; // number of hits in .order[]
		for (k = 0; k < INQUIRY_LENGTH; k++) {
			Z.m[i].RLhit[k] = 0;
			Z.m[i].order[k].dpos = 0;
			Z.m[i].order[k].qpos = 0;
			Z.m[i].order[k].rn = 0;
		}
		Z.m[i].PER = pRD->pR[r].PER;
		Z.m[i].QUAL = pRD->pR[r].QUAL;
		Z.m[i].cntA = pRD->pR[r].cntA;
	}
	Z.Mcount = (i + 1); //number of ONs in this player
	Z.Score = 0;
	Z.spaceB = 0;
	Z.anomaly = 0;
	Z.rec = 0;
	Z.minpr = 0;
	Z.bound = 0;
	Z.uncount = 0;
	Z.mislead = 0;
	for (k = 0; k < INQUIRY_LENGTH; k++) Z.SpaceClaim[k] = 0;

	res = RunSCU(Ndb, pRD, &A, &Z); //Run the competition between A & Z

	pRD->pT[a].SCUscore = A.Score;
	pRD->pT[z].SCUscore = Z.Score;
	
	if (A.Score > Z.Score) {
		pRD->pT[z].Eliminated = 1;
	} else {
		if (Z.Score > A.Score) pRD->pT[a].Eliminated = 1;
	}

	return 0;
}


