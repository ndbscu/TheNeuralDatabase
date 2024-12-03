//The Neural Database - Testing routines
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


//All files are expected to be in directory C:\Ndb
//Neural Databases will be created in C:\Ndb\ndbdatabases
//If this subdirectory doesn't exist, the routines will create it.


//Global variables available to all routines
int ActualThreads; //The number of logical processors available on this system
char SubDirectoryNdbs[] = "ndbdatabases/"; //Name of subdirectory to store neural databases
char MNISTerrors[] = "mnist_errors.txt"; //Name of file listing the MNIST Test records that failed recognition
int ShowProgress; //Used to display processing events
SCUswitches SCUswitch; //Used to turn SCU agents On/Off
OUTPUT pOUT[TOTAL_ALLOWED_RESULTS + 1]; //Results of an inquiry
int OUTcount; //Number of results in pOUT


// Functions:
int InquireNdbN(void);
int InquireNdb1to12(void);
int TestNdbN(void);
int TestNdb1to12(void);
int TestLinkedNdbs(void);
int TestMNIST(void);
int TestAllMNIST(void);
void DisplayImage(MNISTimage *, long);
void FreeMem(NdbData *);


int main(void) {
	//
	//	This Release has been compiled with Pelles C on Windows 11 (64-bit)
	//	Double-Click on ndb.exe to launch this Win64 Console
	//
	//----------

	int quit;
	char Read_Char[10];
	int menu;
	int N;
	int res;
	char FileName[INQUIRY_LENGTH];
	int skip, offset;

	printf("\nThe Neural Database");
	printf("\n(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.");

	//Ask OpenMP for the number of threads (logical processors) available on this system.
	//For the memory structures to handle more than MAX_THREADS, you'll have to change its
	//value in ndb.h and recompile.
	ActualThreads=omp_get_num_procs();
	if (ActualThreads > MAX_THREADS) ActualThreads = MAX_THREADS; //Can't exceed memory allocated in ndb.h

	//Begin with all SCU spike train switches turned ON
	SCUswitch.SpaceB = 1;
	SCUswitch.Anomaly = 1;
	SCUswitch.Rec = 1;
	SCUswitch.MinPR = 1;
	SCUswitch.Bound = 1;
	SCUswitch.UnCount = 1;
	SCUswitch.MisLead = 1;

	quit = 0;
	while (quit == 0) {
		//
		printf("\n");
		printf("\nCreate Ndb's from text files:");
		printf("\n     1) Create Ndb's #1, #2, ... #12 from WordMin.txt and WordMax.txt");
		printf("\n     2) Create Ndb #N from a text file of your choice");
		printf("\n     3) Create a 'CENTRAL' Ndb (#50) with executable Actions from Questions.txt");
		printf("\n     4) Create 399 Ndb's (#2000 through #2569) from the MNIST Training Set");
		printf("\n");
		printf("\nTest Ndb:");
		printf("\n     5) Inquire into Ndb #N");
		printf("\n     6) Run an inquiry on all the Ndb's: #1, #2, ... #12");
		printf("\n     7) Run the Test Library (TestLib.txt) on Ndb #N");
		printf("\n     8) Run the Test Library on all the Ndb's: #1, #2, ... #12");
		printf("\n     9) Test 'linked' Ndb's: Ndb #1 --> Ndb #50 --> execute Action");
		printf("\n    10) Recognize a specific MNIST Test image using the image Ndb's");
		printf("\n    11) Run all 10,000 MNIST Test Set images on the image Ndb's");
		printf("\n    12) Add failures from Option 11 (mnist_errors.txt) to the image Ndb's");
		printf("\n");
		printf("\nSwitch SCU Spike Trains ON/OFF:");

		printf("\n    13) SpaceB ");
		if (SCUswitch.SpaceB == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("      17) Bound ");
		if (SCUswitch.Bound == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("         20) Turn them all ON");

		printf("\n    14) Anomaly ");
		if (SCUswitch.Anomaly == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("     18) UnCount ");
		if (SCUswitch.UnCount == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("       21) Turn them all OFF");

		printf("\n    15) Rec ");
		if (SCUswitch.Rec == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("         19) MisLead ");
		if (SCUswitch.MisLead == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}
		printf("\n    16) MinPR ");
		if (SCUswitch.MinPR == 1) {
			printf("(ON) ");
		} else {
			printf("(OFF)");
		}

		printf("\n\nEnter 1, 2, ... 21, or Enter/Return to Exit: ");
		fgets(Read_Char, 10, stdin);
		menu = atoi(Read_Char); // convert %s to %d

		if (menu == 0) {
			quit = 1;
		}
		if (menu == 1) {
			printf("\n");
			printf("\nEnter a word-loading 'Offset'...");
			printf("\nThis Offset can be used to generate different sets of words in the");
			printf("\nintermediate databases (#2 through #11). These in turn will produce");
			printf("\ndifferent sets of SCU competitors to more thoroughly test the Ndb.");
			printf("\n");
			printf("\nEnter an Offset (0, 1, 2, ..., or 9): ");
			fgets(Read_Char, 10, stdin);
			if (strlen(Read_Char) == 2) {
				offset = (int)Read_Char[0];
				if ((offset >= 48) && (offset <= 57)) { //ASCII 0 - 9
					offset = offset - 48;
					N = 1;
					res = CreateNdb(N, "WordMin.txt");
					skip = 1024; //number of words in WordMax.txt to skip before adding the next word
					for (N = 2; N < 12; N++) {
						res = CreateNdbAdditions(N, "WordMin.txt", "WordMax.txt", skip, offset);
						skip = skip/2;
					}
					N = 12;
					res = CreateNdbAdditions(N, "WordMin.txt", "WordMax.txt", 0, 0); //Get ALL the words
				}
			}
		}
		if (menu == 2) {
			printf("\nWhat will this database's number be: ");
			fgets(Read_Char, 10, stdin);
			N = atoi(Read_Char);
			if (N != 0) {
				FileName[0] = 0;
				res = 1;
				while (res > 0) {
					res = 0;
					printf("\nWhat txt file do you want to use to create %d.ndb? ", N);
					fgets(FileName, INQUIRY_LENGTH, stdin);
					//Get rid of linefeed at the end of FileName
					for (int i = 0; i < INQUIRY_LENGTH; i++) {
						if (FileName[i] == 0) break;
						if (FileName[i] != 10) continue;
						FileName[i] = 0;
						break;
					}
					if (FileName[0] != 0) {
						res = CreateNdb(N, FileName);
						if (res > 0) printf("\nFAILED to create %d.ndb from file: %s  rec=%d\n\n", N, FileName, res);
					}
				}
			}
		}
		if (menu == 3) {
			N = 50;
			res = CreateQuestionNdb(N, "Questions.txt");
			if (res == 1) printf("\nFAILED to create Neural Database 50.ndb\n\n");
		}
		if (menu == 4) {
			res = mpCreateImageNdbs();
		}
		if (menu == 5) {
			res = InquireNdbN();
		}
		if (menu == 6) {
			res = InquireNdb1to12();
		}
		if (menu == 7) {
			res = TestNdbN();
		}
		if (menu == 8) {
			res = TestNdb1to12();
		}
		if (menu == 9) {
			res = TestLinkedNdbs();
		}
		if (menu == 10) {
			res = TestMNIST();
		}
		if (menu == 11) {
			res = TestAllMNIST();
		}
		if (menu == 12) {
			res = AddImages(); //Add mnist_er.txt to Ndb #2000+
		}
		if (menu == 13) {
			if (SCUswitch.SpaceB == 0) {
				SCUswitch.SpaceB = 1;
			} else {
				SCUswitch.SpaceB = 0;
			}
		}
		if (menu == 14) {
			if (SCUswitch.Anomaly == 0) {
				SCUswitch.Anomaly = 1;
			} else {
				SCUswitch.Anomaly = 0;
			}
		}
		if (menu == 15) {
			if (SCUswitch.Rec == 0) {
				SCUswitch.Rec = 1;
			} else {
				SCUswitch.Rec = 0;
			}
		}
		if (menu == 16) {
			if (SCUswitch.MinPR == 0) {
				SCUswitch.MinPR = 1;
			} else {
				SCUswitch.MinPR = 0;
			}
		}
		if (menu == 17) {
			if (SCUswitch.Bound == 0) {
				SCUswitch.Bound = 1;
			} else {
				SCUswitch.Bound = 0;
			}
		}
		if (menu == 18) {
			if (SCUswitch.UnCount == 0) {
				SCUswitch.UnCount = 1;
			} else {
				SCUswitch.UnCount = 0;
			}
		}
		if (menu == 19) {
			if (SCUswitch.MisLead == 0) {
				SCUswitch.MisLead = 1;
			} else {
				SCUswitch.MisLead = 0;
			}
		}
		if (menu == 20) {
			//Turn ON all SCU spike train switches
			SCUswitch.SpaceB = 1;
			SCUswitch.Anomaly = 1;
			SCUswitch.Rec = 1;
			SCUswitch.MinPR = 1;
			SCUswitch.Bound = 1;
			SCUswitch.UnCount = 1;
			SCUswitch.MisLead = 1;
		}
		if (menu == 21) {
			//Turn OFF all SCU spike train switches
			SCUswitch.SpaceB = 0;
			SCUswitch.Anomaly = 0;
			SCUswitch.Rec = 0;
			SCUswitch.MinPR = 0;
			SCUswitch.Bound = 0;
			SCUswitch.UnCount = 0;
			SCUswitch.MisLead = 0;
		}
	}
	return 0;
}

int InquireNdbN(void) {  // Query Ndb #N

	char txt[INQUIRY_LENGTH];
	int res;
	int len;
	char ch;
	int a;
	int i, j, k;
	int REScount;
	char result[INQUIRY_LENGTH];
	NdbData Ndb;
	int N;

	clock_t T;
	double runtime;

	printf("\nQuery Ndb #N");
	printf("\nEnter the number of the Ndb: ");
	fgets(txt, 10, stdin);
	N = atoi(txt);
	if (N == 0) return 0;

	Ndb.ID = N;
	res = LoadNdb(&Ndb);
	if (res != 0) {
		sprintf(txt, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
		printf("\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, txt);
		return 1;
	}
	printf("\nLoaded Ndb #%d, %d ONs, %d RNs, %d Connections\n", N, Ndb.ONcount, Ndb.RNcount, Ndb.ConnectCount);

	ShowProgress = 0;
	printf("\nShow the progress of the processing functions? (y/n): No ");
	fgets(txt, 10, stdin);
	if ((txt[0] == 'y') || (txt[0] == 'Y')) ShowProgress = 1;

	// Inquiries into Ndb #N...
	while (1) {
		printf("\nEnter any text, or Enter/Return to go back to the Menu\n");
		fgets(txt, INQUIRY_LENGTH, stdin); //fgets() accepts spaces
		
		len = strlen(txt);
		if (len < 2) break; // Enter/Return => linefeed, len = 1

		for (int c = 0; c < len; c++) {
			ch = txt[c];
			a = (int) ch; //ASCII value
			if (a == 10) {
				txt[c] = 0; //replace linefeed with NULL terminator
				break;
			}
		}

		T = clock();

		res = RecognizeTEXT(&Ndb, txt); //Call the Ndb, Results in pOUT[]

		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;

		if (OUTcount == 0) {
			printf("\nResult: ERROR - NO RESULT!");
 			printf("\n");
		} else {
			if (OUTcount > 1) { //There are multiple results
				printf("\nMultiple results:  (%fsec)", runtime);
				for (i = 1; i <= OUTcount; i++) {
					REScount = pOUT[i].REScount;
					for (k = 0; k < INQUIRY_LENGTH; k++) result[k] = 0; //clear the buffer
					for (j = 1; j <= REScount; j++) {
						if (j == 1) {
							strcpy(result, pOUT[i].Result[j].ON);
						} else {
							sprintf(result, "%s %s", result, pOUT[i].Result[j].ON);
						}
					}
					printf("\n  Result %d: %s", i, result);
				}
				printf("\n");
			} else {
				//There is only one result
				i = 1;
				REScount = pOUT[i].REScount;
				printf("\nResult:");
				for (j = 1; j <= REScount; j++) {
					printf(" %s", pOUT[i].Result[j].ON);
				}
				printf(" (%fsec)", runtime);
 				printf("\n");
			}
		}
 		printf("\n");
	}

	FreeMem(&Ndb);
	return 0;
}

int InquireNdb1to12(void) { //Run the same query on Ndb's #1 through #12

	char txt[INQUIRY_LENGTH];
	int res;
	int len;
	char ch;
	int a;
	int i, j, k;
	int REScount;
	char result[INQUIRY_LENGTH];
	NdbData Ndb;
	int N;

	ShowProgress = 0; //Set to 1 to show the time spent in each function
	//ShowProgress = 1;

	clock_t T;
	double runtime;

	printf("\nRun an inquiry on Ndb's #1 through #12");
	printf("\nEnter any text...\n");
	fgets(txt, INQUIRY_LENGTH, stdin);
		
	len = strlen(txt);
	if (len < 2) return 0; // Enter/Return => linefeed, len = 1

	for (int c = 0; c < len; c++) {
		ch = txt[c];
		a = (int) ch; //ASCII value
		if (a == 10) {
			txt[c] = 0; //replace linefeed with NULL terminator
			break;
		}
	}

	printf("\nIdentifying: %s...", txt);
	for (N = 1; N <= 12; N++) {

		Ndb.ID = N;
		res = LoadNdb(&Ndb);
		if (res != 0) {
			sprintf(txt, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
			printf("\n\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, txt);
			return 1;
		} else {

			printf("\n%d (%d words):", N, Ndb.ONcount);

			T = clock();

			res = RecognizeTEXT(&Ndb, txt); //Call the Ndb, Results in pOUT[]

			FreeMem(&Ndb);
			T = clock() - T;
			runtime = ((double)T) / CLOCKS_PER_SEC;
		
			if (OUTcount == 0) {
				printf("\nResult: ERROR - NO RESULT!");
 				printf("\n");
			} else {
				if (OUTcount > 1) { //There are multiple results
					printf("\nMultiple results:  (%fsec)", runtime);
					for (i = 1; i <= OUTcount; i++) {
						REScount = pOUT[i].REScount;
						for (k = 0; k < INQUIRY_LENGTH; k++) result[k] = 0; //clear the buffer
						for (j = 1; j <= REScount; j++) {
							if (j == 1) {
								strcpy(result, pOUT[i].Result[j].ON);
							} else {
								sprintf(result, "%s %s", result, pOUT[i].Result[j].ON);
							}
						}
						printf("\n  Result %d: %s", i, result);
					}
					printf("\n");
				} else {
					//There is only one result
					i = 1;
					REScount = pOUT[i].REScount;
					printf("\nResult:");
					for (j = 1; j <= REScount; j++) {
						printf(" %s", pOUT[i].Result[j].ON);
					}
					printf(" (%fsec)", runtime);
 					printf("\n");
				}
			}
		}
	}
	printf("\n");

	return 0;
}

int TestNdbN(void) {  // Run the Test Library (TestLib.txt) on Ndb #N

	FILE *fh_read;
	char txt[INQUIRY_LENGTH];
	char result[INQUIRY_LENGTH];
	char answer[INQUIRY_LENGTH];
	char line[FILE_LINE_LENGTH];
	char testfile[INQUIRY_LENGTH];
	int res;
	int i, j, k, p;
	int REScount;
	int OK, notOK, Multiples;
	int fnd;
	int display;
	NdbData Ndb;
	int N;
	char TestNumber[4]; //1 to 999

	//Set to 1 to show the time spent in each function
	ShowProgress = 0;
	//ShowProgress = 1;
	
	clock_t T;
	double runtime;
	time_t t;
	char timenow[32];


	strcpy(testfile, "TestLib.txt");
	printf("\nRun the Test Library (%s) on Ndb #N:\n", testfile);


	printf("\nEnter the number of the Ndb: ");
	fgets(txt, 10, stdin); // accepts <space>
	N = atoi(txt); // convert %s to %d
	if (N == 0) return 0;

	Ndb.ID = N;
	res = LoadNdb(&Ndb);
	if (res != 0) {
		sprintf(txt, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
		printf("\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, txt);
		return 1;
	}
	printf("\nLoaded Ndb #%d, %d ONs, %d RNs, %d Connections\n", N, Ndb.ONcount, Ndb.RNcount, Ndb.ConnectCount);

	printf("\nEnter: 1 to display ALL results");
	printf("\n       2 to display only results with errors");
	printf("\n       3 to display just the error count");
	printf("\n       anything else will Exit: ");

	fgets(txt, 10, stdin); // accepts <space>
	display = atoi(txt); // convert %s to %d

	if ((display != 1)&&(display != 2)&&(display != 3)) {
		FreeMem(&Ndb);
		return 0;
	}

	fh_read = fopen(testfile, "r");
	if (fh_read == NULL) {
		printf("ERROR: Failed to OPEN file %s for reading.\n", testfile);
		FreeMem(&Ndb);
		return 1;
	} else {
		time(&t);
		strcpy(timenow, ctime(&t));
		//Get rid of the linefeed at the end of timenow
		for (i = 0; i < 32; i++) {
			if (timenow[i] == 0) break;
			if (timenow[i] != 10) continue;
			timenow[i] = 0;
			break;
		}
		printf("\nTest Ndb #%d (%d words, %d RNs, %d Connections) starting: %s ...", N,
																				 Ndb.ONcount,
																				 Ndb.RNcount,
																				 Ndb.ConnectCount,
																				 timenow);
		// read through the Test Library file
		T = clock();
		OK = 0;
		notOK = 0;
		Multiples = 0;

		while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {

			if (line[0] == '$') break; //End Of File

			if ((line[0] == ';') && (line[1] == '#')) {
				//get the Test Number
				i = 0;
				for (k = 2; k < 6; k++) {
					if ((line[k] >= '0') && (line[k] <= '9')) {
						TestNumber[i] = line[k];
						i++;
						continue;
					}
					break;
				}
				TestNumber[i] = 0;
				continue;
			}

			if (line[0] != '@') continue; // Search for line[] = @"text"

			for (k = 0; k < INQUIRY_LENGTH; k++) txt[k] = 0; //clear the buffer
			for (p = 2; p < INQUIRY_LENGTH; p++) {
				if ((line[p] == '"')||(line[p] == 10)||(line[p] == 0)) {
					txt[p-2] = 0;
					break;
				}
				txt[p-2] = line[p];
			}

			if (display == 1) printf("\nTest #%s: %s", TestNumber, txt);

			res = RecognizeTEXT(&Ndb, txt); ///Call the Ndb, Results in pOUT[]

			if (OUTcount == 0) {
				notOK++;
				if (display == 1) {
					printf("\n+++NO RESULT!\n");
				} else {
					if (display == 2) printf("\n+++NO RESULT for Test #%s: %s !\n", TestNumber, txt);
				}
			} else {
				fnd = 0; //Do any of these match an acceptable result?
				while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
					if (line[0] == '#') break; //end of possible answers
					for (k = 0; k < INQUIRY_LENGTH; k++) answer[k] = 0; //clear the buffer
					for (p = 2; p < INQUIRY_LENGTH; p++) {
						if (line[p] == '"') break; //end of quoted text
						if (line[p] == 10) break;
						if (line[p] == 0) break;
						answer[p-2] = line[p];
					}
					for (i = 1; i <= OUTcount; i++) {
						REScount = pOUT[i].REScount;
						for (k = 0; k < INQUIRY_LENGTH; k++) result[k] = 0; //clear the buffer
						for (j = 1; j <= REScount; j++) {
							if (j == 1) {
								strcpy(result, pOUT[i].Result[j].ON);
							} else {
								sprintf(result, "%s %s", result, pOUT[i].Result[j].ON);
							}
						}
						if (strcmp(result, answer) == 0) {
							fnd = 1;
							//Move the file pointer past all the possible answers...
							while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) if (line[0] == '#') break;
							break;
						}
					}
					if (fnd == 1) break;
				}
				if (display == 1) printf("\nResult: %s", result);
				if (fnd == 1) {
					OK++;
				} else {
					notOK++;
					if (display == 2) {
						printf("\nTest #%s: %s", TestNumber, txt);
						printf("\nResult: %s", result);
					}
					if (display != 3) printf("\n+++ERROR+++\n");
				}
				if (display == 1) printf("\n");
			}
		}
	}
	fclose(fh_read);
	FreeMem(&Ndb); //assigned to in-memory Ndb

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;

	time(&t);
	strcpy(timenow, ctime(&t));
	//Get rid of the linefeed at the end of timenow
	for (i = 0; i < 32; i++) {
		if (timenow[i] == 0) break;
		if (timenow[i] != 10) continue;
		timenow[i] = 0;
		break;
	}
	printf("\n  Wrong=%d ", notOK);
	if (Multiples > 0) printf("of which %d had multiple answers. ", Multiples);
	if (notOK > 0) printf("Correct=%d  ", OK);
	printf("Finished Ndb #%d: %s  (%fsec)\n", N, timenow, runtime);

	return 0;
}

int TestNdb1to12(void) {  // Run the Test Library on Ndb's #1, #2, ... #12

	FILE *fh_read;
	char txt[INQUIRY_LENGTH];
	char result[INQUIRY_LENGTH];
	char answer[INQUIRY_LENGTH];
	char line[FILE_LINE_LENGTH];
	char testfile[INQUIRY_LENGTH];
	int res;
	int i, j, k, p;
	int REScount;
	int OK, notOK, Multiples;
	int fnd;
	int display;
	NdbData Ndb;
	int N;
	char TestNumber[4]; //1 to 999

	ShowProgress = 0; //Set to 1 to show the time spent in each function
	
	clock_t T;
	double runtime;
	time_t t;
	char timenow[32];


	strcpy(testfile, "TestLib.txt");
	printf("\nRun the Test Library (%s) on Ndb's #1 through #12", testfile);

	printf("\n\nEnter: 1 to display ALL results");
	printf("\n       2 to display only results with errors");
	printf("\n       3 to display just the error count");
	printf("\n       anything else will Exit: ");

	fgets(txt, 10, stdin);
	display = atoi(txt); // convert %s to %d
	if ((display != 1)&&(display != 2)&&(display != 3)) return 0;

	fh_read = fopen(testfile, "r");
	if (fh_read == NULL) {
		printf("ERROR: Failed to OPEN file %s for reading.\n", testfile);
		return 1;
	}

	//Run the tests...
	for (N = 1; N <= 12; N++) {

		Ndb.ID = N;
		res = LoadNdb(&Ndb);
		if (res != 0) {
			sprintf(txt, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
			printf("\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, txt);
			break; //Not 'return 1;' - you still have to CLOSE testfile.
		}

		time(&t);
		strcpy(timenow, ctime(&t));
		//Get rid of the linefeed at the end of timenow
		for (i = 0; i < 32; i++) {
			if (timenow[i] == 0) break;
			if (timenow[i] != 10) continue;
			timenow[i] = 0;
			break;
		}
		printf("\nTest Ndb #%d (%d words, %d RNs, %d Connections) starting: %s ...", N,
																				 Ndb.ONcount,
																				 Ndb.RNcount,
																				 Ndb.ConnectCount,
																				 timenow);
		// read through the Test Library file
		rewind(fh_read);
		OK = 0;
		notOK = 0;
		Multiples = 0;

		T = clock();

		while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {

			if (line[0] == '$') break; //End Of File

			if ((line[0] == ';') && (line[1] == '#')) {
				//get the Test Number
				i = 0;
				for (k = 2; k < 6; k++) {
					if ((line[k] >= '0') && (line[k] <= '9')) {
						TestNumber[i] = line[k];
						i++;
						continue;
					}
					break;
				}
				TestNumber[i] = 0;
				continue;
			}

			if (line[0] != '@') { // Search for line[] = @"text"
				for (k = 0; k < FILE_LINE_LENGTH; k++) line[k] = 0; //clear the buffer
				continue;
			}

			for (k = 0; k < INQUIRY_LENGTH; k++) txt[k] = 0; //clear the buffer
			for (p = 2; p < INQUIRY_LENGTH; p++) {
				if ((line[p] == '"')||(line[p] == 10)||(line[p] == 0)) {
					txt[p-2] = 0;
					break;
				}
				txt[p-2] = line[p];
			}

			if (display == 1) printf("\nTest #%s: %s", TestNumber, txt);

			res = RecognizeTEXT(&Ndb, txt); //Call the Ndb, Results in pOUT[]

			if (OUTcount == 0) {
				notOK++;
				if (display == 1) {
					printf("\n+++NO RESULT!\n");
				} else {
					if (display == 2) printf("\n+++NO RESULT for Test #%s: %s !\n", TestNumber, txt);
				}
			} else {
				fnd = 0; //Do any of these match an acceptable result?
				while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
					if (line[0] == '#') break; //end of possible answers
					for (k = 0; k < INQUIRY_LENGTH; k++) answer[k] = 0; //clear the buffer
					for (p = 2; p < INQUIRY_LENGTH; p++) {
						if (line[p] == '"') break; //end of quoted text
						if (line[p] == 10) break;
						if (line[p] == 0) break;
						answer[p-2] = line[p];
					}
					for (i = 1; i <= OUTcount; i++) {
						REScount = pOUT[i].REScount;
						for (k = 0; k < INQUIRY_LENGTH; k++) result[k] = 0; //clear the buffer
						for (j = 1; j <= REScount; j++) {
							if (j == 1) {
								strcpy(result, pOUT[i].Result[j].ON);
							} else {
								sprintf(result, "%s %s", result, pOUT[i].Result[j].ON);
							}
						}
						if (strcmp(result, answer) == 0) {
							fnd = 1;
							//Move the file pointer past all the possible answers...
							while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) if (line[0] == '#') break;
							break;
						}
					}
					if (fnd == 1) break;
				}
				if (display == 1) printf("\nResult: %s", result);
				if (fnd == 1) {
					OK++;
				} else {
					notOK++;
					if (display == 2) {
						printf("\nTest #%s: %s", TestNumber, txt);
						printf("\nResult: %s", result);
					}
					if (display != 3) printf("\n+++ERROR+++\n");
				}
				if (display == 1) printf("\n");
			}
		}
		FreeMem(&Ndb); //assigned to in-memory Ndb

		//Results for Ndb #N
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;

		time(&t);
		strcpy(timenow, ctime(&t));
		//Get rid of the linefeed at the end of timenow
		for (i = 0; i < 32; i++) {
			if (timenow[i] == 0) break;
			if (timenow[i] != 10) continue;
			timenow[i] = 0;
			break;
		}
		printf("\n  Wrong=%d ", notOK);
		if (Multiples > 0) printf("of which %d had multiple answers. ", Multiples);
		if (notOK > 0) printf("Correct=%d  ", OK);
		printf("Finished Ndb #%d: %s  (%fsec)\n", N, timenow, runtime);
	}
	fclose(fh_read);

	return 0;
}

int TestLinkedNdbs(void) { // Test 'linked' Ndb's: Ndb #1 --> Ndb #50 --> execute Action

	typedef struct {
		int cnt; //number of times this Action is identified in the results
		char action[INQUIRY_LENGTH+1];
		char ON[INQUIRY_LENGTH+1];
	} Actions;
	Actions pACT[10];
	int ACTcnt;

	char txt[INQUIRY_LENGTH];
	char x[INQUIRY_LENGTH];
	int res;
	int len;
	char ch;
	int a;
	int i, j, k, c, fnd;
	int REScount;
	NdbData Ndb;
	int N;
	char ndbfile[INQUIRY_LENGTH];

	clock_t T;
	double runtime;

	ShowProgress = 0;

	N = 1;

	Ndb.ID = N;
	res = LoadNdb(&Ndb);
	if (res != 0) {
		sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
		printf("\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, ndbfile);
		return 1;
	}

	// Begin with an inquiry into Ndb #1...
	while (1) {
		printf("\nEnter any text...\n");
		fgets(txt, INQUIRY_LENGTH, stdin);
		
		len = strlen(txt);
		if (len < 2) break; // Enter/Return => linefeed, len = 1

		for (c = 0; c < len; c++) {
			ch = txt[c];
			a = (int) ch; //ASCII value
			if (a == 10) {
				txt[c] = 0; //replace linefeed with NULL terminator
				break;
			}
		}

		T = clock();
		res = RecognizeTEXT(&Ndb, txt); //Call the Ndb, Results in pOUT[]

		printf("\n%d.ndb Result:", N);
		
		if (OUTcount == 0) {
			printf(" ERROR - NO RESULT!");
		} else {
			for (i = 0; i < INQUIRY_LENGTH; i++) txt[i] = 0; //clear the buffer
			for (i = 1; i <= OUTcount; i++) {
				REScount = pOUT[i].REScount;
				for (j = 1; j <= REScount; j++) {
					//The recognized ON's output may have come from a Surrogate containing a space
					//Strip it of any possible spaces...
					for (k = 0; k < INQUIRY_LENGTH; k++) x[k] = 0;
					c = 0;
					for (k = 0; k < INQUIRY_LENGTH; k++) {
						ch = pOUT[i].Result[j].ON[k];
						if (ch == ' ') continue;
						x[c] = ch;
						c++;
					}
					printf(" %s", pOUT[i].Result[j].ON);
					if (j == 1) {
						sprintf(txt, "%s", x);
					} else {
						strcat(txt, " ");
						strcat(txt, x);
					}
				}
				//**********
				//This 'break;' will ignore multiple results
				//**********
				break;
			}

			//Use the results from Ndb #1 to inquire into Ndb #50 
			FreeMem(&Ndb);
			N = 50;
			Ndb.ID = N;
			res = LoadNdb(&Ndb);
			if (res != 0) {
				sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
				printf("\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", N, ndbfile);
				return 1;
			}

			ShowProgress = 0;
			//ShowProgress = 1;

			res = RecognizeTEXT(&Ndb, txt); //Call the Ndb, Results in pOUT[]

			printf("\n%d.ndb Result:", N);
		
			if (OUTcount == 0) {
				printf(" ERROR - NO RESULT!");
			} else {
				//Clear the list of Actions...
				ACTcnt = 0;
				for (i = 0; i < 10; i++) {
					pACT[i].cnt = 0;
					for (j = 0; j < INQUIRY_LENGTH; j++) {
						pACT[i].action[j] = 0;
						pACT[i].ON[j] = 0;
					}
				}
				for (i = 1; i <= OUTcount; i++) {
					REScount = pOUT[i].REScount;
					for (j = 1; j <= REScount; j++) {

						//Check for any Actions to execute...
						if (pOUT[i].Result[j].ACT[0] != 0) { //accumulate the Actions from this result
							fnd = 0;
							for (k = 0; k < ACTcnt; k++) {
								if (strcmp(pOUT[i].Result[j].ACT, pACT[k].action) == 0) {
									pACT[k].cnt++;
									fnd = 1;
									break;
								}
							}
							if (fnd == 0) {
								pACT[ACTcnt].cnt = 1;
								strcpy(pACT[ACTcnt].action, pOUT[i].Result[j].ACT);
								strcpy(pACT[ACTcnt].ON, pOUT[i].Result[j].ON);
								ACTcnt++;
							}
						}
					}
					//What is the most significant Action
					fnd = 0; //Most significant Action
					j = -1; //ID of the most significant Action
					for (k = 0; k < ACTcnt; k++) {
						if (pACT[k].cnt > fnd) {
							fnd = pACT[k].cnt;
							j = k;
						}
					}
					if (fnd > 0) { //Perform the ACTION
						printf(" %s", pACT[j].ON);
						res = ExecuteActions(pACT[j].action);
					} else {
						//Otherwise just list the results
						for (j = 1; j <= REScount; j++) printf(" %s", pOUT[i].Result[j].ON);
					}

					//**********
					//This 'break;' will ignore multiple results
					//**********
					break;
				}
			}

			FreeMem(&Ndb);
			N = 1;
			Ndb.ID = N;
			res = LoadNdb(&Ndb);
			if (res != 0) {
				sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
				printf("\nERROR: Failed to re-load Ndb #%d from the database sub-directory: %s !!", N, ndbfile);
				return 1;
			}
		}
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" (%fsec)", runtime);
 		printf("\n");
	}

	FreeMem(&Ndb);
	return 0;
}

int TestMNIST(void) {  // Run a MNIST Test Image on Ndb's #2000+

	char num[20];
	long RecNum;
	FILE *fh_read;
	char line[IMAGE_LENGTH];
	int res;
	int i, j;
	int REScount;

	MNISTimage *pIMG;	//MNIST images are loaded from the file into this memory
	int IMGcount;

	char ndbfile[INQUIRY_LENGTH];

	clock_t T;
	double runtime;

	long NumberOfImages = 10000;
	char DataFile[20] = "mnist_test.txt";

	ShowProgress = 0;

	printf("\nLoading images from %s...", DataFile);
	fh_read = fopen(DataFile, "r");
	if (fh_read == NULL) {
		printf("ERROR: Failed to OPEN file %s for reading.\n", DataFile);
		return 1;
	}

	pIMG = (MNISTimage *)malloc((NumberOfImages+50) * IMAGE_LENGTH);
	IMGcount = 1; //=RecNum: 1, 2, ...
	while (fgets(line, IMAGE_LENGTH, fh_read) != NULL) {
		j = 0;
		for (i = 0; i < (IMAGE_LENGTH-1); i++) {
			if (line[i] == 10) break; //linefeed
			pIMG[IMGcount].image[j] = line[i];
			j++;

			//In order to reduce the size of these MNIST image files, all pixels of
			//value 0 have been removed except for any 0 at the end of a line.
			//This code puts the 0's back in...
			if ((line[i] == ',') && (line[i+1] == ',')) {
				pIMG[IMGcount].image[j] = '0';
				j++;
			}
		}
		pIMG[IMGcount].image[j] = 0;
		IMGcount++;
		if (IMGcount <= NumberOfImages) continue;
		break;
	}
	fclose(fh_read);

	if (IMGcount == 0) {
		printf("\nERROR: Failed to find any images in %s", DataFile);
		printf("\nThe line format should be 'digit,pixel,pixel,...'\n");
		free(pIMG);
		return 1;
	}
	printf("\n");


	//Specify MNIST Test image(s)...
	while (1) {

		printf("\nRecord Number (1, 2, ..., %d): ", NumberOfImages);
		fgets(num, 10, stdin);
		RecNum = atol(num);
		if ((RecNum < 1) || (RecNum > NumberOfImages)) break;

		T = clock();

		DisplayImage(pIMG, RecNum);

		i = 2; //start reading the image string from here
		j = 0;
		while (pIMG[RecNum].image[i] != 0) {
			line[j] = pIMG[RecNum].image[i];
			i++;
			j++;
		}
		line[j] = 0;

		res = mpRecognizeIMAGE(RecNum, line); //Call the Image Ndbs, Results in pOUT[]

		if (res > 1999) {
			sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, res);
			printf("\n\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", res, ndbfile);
			break;
		}

		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;

		printf("\nResult: ");
		
		if (OUTcount == 0) {
			printf(" ERROR - NO RESULT!");
		} else {
			for (i = 1; i <= OUTcount; i++) {
				REScount = pOUT[i].REScount;
				for (j = 1; j <= REScount; j++) {
					printf(" %s", pOUT[i].Result[j].ON);
				}
				break; //If there are multiple results, ignore all but the 1st one
			} 
		}
		printf(" (%fsec)", runtime);
 		printf("\n");
	}

	free(pIMG);
	return 0;
}

int TestAllMNIST(void) {  // Run the full MNIST Image Test Set of 10,000 images on Ndb's #2000+

	long RecNum;
	FILE *fh_read;
	FILE *fh_write;
	char line[IMAGE_LENGTH];
	int res;
	int i, j;
	int errors;

	MNISTimage *pIMG;	//MNIST images are loaded from the file into this memory
	int IMGcount;

	char ndbfile[INQUIRY_LENGTH];

	clock_t TotalT;
	clock_t T;
	double runtime;

	long NumberOfImages = 10000;
	char DataFile[20] = "mnist_test.txt";

	ShowProgress = 0;

	printf("\nLoading %d images from %s...", NumberOfImages, DataFile);
	fh_read = fopen(DataFile, "r");
	if (fh_read == NULL) {
		printf("ERROR: Failed to OPEN file %s for reading.\n", DataFile);
		return 1;
	}

	pIMG = (MNISTimage *)malloc((NumberOfImages+50) * IMAGE_LENGTH);
	IMGcount = 1; //=RecNum: 1, 2, ...
	while (fgets(line, IMAGE_LENGTH, fh_read) != NULL) {
		j = 0;
		for (i = 0; i < (IMAGE_LENGTH-1); i++) {
			if (line[i] == 10) break; //linefeed
			pIMG[IMGcount].image[j] = line[i];
			j++;

			//In order to reduce the size of these MNIST image files, all pixels of
			//value 0 have been removed except for any 0 at the end of a line.
			//This code puts the 0's back in...
			if ((line[i] == ',') && (line[i+1] == ',')) {
				pIMG[IMGcount].image[j] = '0';
				j++;
			}
		}
		pIMG[IMGcount].image[j] = 0;
		IMGcount++;
		if (IMGcount <= NumberOfImages) continue;
		break;
	}
	fclose(fh_read);

	if (IMGcount == 0) {
		printf("\nERROR: Failed to find any images in %s", DataFile);
		printf("\nThe line format should be 'digit,pixel,pixel,...'\n");
		free(pIMG);
		return 1;
	}
	printf("\n");

	//Open an empty file to store a list of image Record Numbers that failed to be correctly recognized...
	fh_write = fopen(MNISTerrors, "w");
	if (fh_write == NULL) {
		printf("ERROR: Failed to OPEN file %s for writing Record Numbers that failed to be correctly recognized.\n", MNISTerrors);
		free(pIMG);
		return 1;
	}
	fprintf(fh_write, ";Neural Database data - Record Numbers of MNIST Test images that failed\n");
	fflush(fh_write);

	TotalT = clock();
	errors = 0;

	//Run through all the images...
	for (RecNum = 1; RecNum <= NumberOfImages; RecNum++) {

		i = 2; //start reading the image-string from here
		j = 0;
		while (pIMG[RecNum].image[i] != 0) {
			line[j] = pIMG[RecNum].image[i];
			i++;
			j++;
		}
		line[j] = 0;

		printf("\nRecord %d, image of %c: ", RecNum, pIMG[RecNum].image[0]);
		T = clock();
		res = mpRecognizeIMAGE(RecNum, line); //Call the Image Ndbs, Results in pOUT[]
		T = clock() - T;
		
		if (res > 1999) {
			sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, res);
			printf("\n\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", res, ndbfile);
			fclose(fh_write);
			free(pIMG);
			return 1;
		}

		runtime = ((double)T) / CLOCKS_PER_SEC;
		
		j = 0;
		if (OUTcount == 0) {
			printf("ERROR: NO RESULT!");
			fprintf(fh_write, "\n;;%d, ERROR: NO RESULT!", RecNum);
			fflush(fh_write);
			errors++;
			j = 1;
		} else {
			printf("recognized %c ", pOUT[1].Result[1].ON[0]);

			if (pIMG[RecNum].image[0] != pOUT[1].Result[1].ON[0]) {
				fprintf(fh_write, "\n;;%d, ERROR: recognized a %c instead of a %c",
													RecNum,
													pOUT[1].Result[1].ON[0],	//recognition result
													pIMG[RecNum].image[0]);		//image 'label', i.e. the digit
				fflush(fh_write);
				errors++;
				j = 1;
			}
		}
		printf(" (%fsec)", runtime);
		if (j > 0) printf(" Errors = %d", errors);
	}
	
	TotalT = clock() - TotalT;
	runtime = ((double)TotalT) / CLOCKS_PER_SEC;
	printf("\nTotal runtime: %fsec", runtime);
	printf("\nTotal Errors: %d", errors);
 	printf("\n");

	fprintf(fh_write, "\n\n$$$ End Of File - Total Errors: %d\n", errors);
	fclose(fh_write);
	free(pIMG);
	return 0;
}

void DisplayImage(MNISTimage *pIMG, long RecNum) {

	char digit;
	char ch;
	char px[4]; //the pixel in characters: 0 to 255 with room for a terminating 0
	int p, i;
	int r, c;
	int image[MAX_IMAGE_SIZE+1][MAX_IMAGE_SIZE+1];

	int ImageSize = MAX_IMAGE_SIZE;

	digit = pIMG[RecNum].image[0]; //The "label" assigned to this image: 0, 1, ..., 9
	p = 2; //start reading the image string from here
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			ch = pIMG[RecNum].image[p];
			for (i = 0; i <4; i++) px[i] = 0; //clear the pixel buffer
			i = 0;
			while ((ch != ',') && (ch != 0)) {
				px[i] = ch;
				i++;
				p++;
				ch = pIMG[RecNum].image[p];
			}
			image[r][c] = atoi(px);
			p++;
		}
	}

	for (r = 1; r <= ImageSize; r++) {
		printf("\n     ");
		for (c = 1; c <= ImageSize; c++) {
			if (image[r][c] == 0) printf(".");
			else printf("x");
		}
	}
	printf("\nThis image (%d) is labeled in MNIST as a '%c'", RecNum, digit);

}

void FreeMem(NdbData *Ndb) {

	free(Ndb->pON);
	free(Ndb->pRNtoON);
	if ((strcmp(Ndb->Type, "TEXT") == 0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {
		free(Ndb->pRN);
	} else {
		free(Ndb->pIRN);
	}
}


