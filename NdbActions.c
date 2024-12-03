//The Neural Database - Execute ON Actions
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


//----------
//
//	Execute Actions associated with recognized ONs
//
//	The following Actions are defined in the file Questions.txt which is the source data for Ndb #50
//
//----------


// Functions:
int ExecuteActions(char *);
void gettime(void);
void getday(int);
void getdate(int);
void getmonthtx(int);
void getmonth(int);
void getyear(int);

char tx[INQUIRY_LENGTH];


int ExecuteActions(char *ACTION) {
	//
	//	ACTION has a numeric ActionCode at the beginning of the text to identify
	//	the code to be executed in the following SWITCH statement...
	//
	//----------

	switch (atoi(ACTION)) {

		case 1: //Time, Now
			printf(" -> The time is ");
			gettime(); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 2: //Day, Today
			printf(" -> Today is ");
			getday(0); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 3: //Yesterday
			printf(" -> Yesterday was ");
			getday(-1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 4: //Tomorrow
			printf(" -> Tomorrow will be ");
			getday(1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 5: //Today's date
			printf(" -> The date today is ");
			getdate(0); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 6: //Yesterday's date
			printf(" -> The date yesterday was ");
			getdate(-1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 7: //Tomorrow's date
			printf(" -> The date tomorrrow will be ");
			getdate(1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 8: //Month
			printf(" -> The month is ");
			getmonth(0); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 9: //Last Month
			printf(" -> Last month was ");
			getmonth(-1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 10: //Next Month
			printf(" -> Next month will be ");
			getmonth(1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 11: //Year
			printf(" -> The year is ");
			getyear(0); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 12: //Last year
			printf(" -> Last year was ");
			getyear(-1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		case 13: //Next year
			printf(" -> Next year will be ");
			getyear(1); //-> tx[]
			printf("%s", tx);
			printf("\n");
			break;

		default:
			printf("*** Huh? ***");
			printf(" -> %s", ACTION);
			printf("\n");
	}

	return 0;
}

void gettime(void) {

	time_t ltime;
	struct tm tm;

	for (int i = 0; i < INQUIRY_LENGTH; i++) tx[i] = 0;; //clear the buffer

	ltime = time(NULL);
	tm = *localtime(&ltime);

	sprintf(tx, "%d:%d:%d", tm.tm_hour, tm.tm_min, tm.tm_sec);

}

void getday(int offset) {

	int day;
	time_t ltime;
	struct tm tm;

	for (int i = 0; i < INQUIRY_LENGTH; i++) tx[i] = 0;; //clear the buffer

	ltime = time(NULL); //Now
	tm = *localtime(&ltime);

	day = tm.tm_wday + offset;
	if (day > 6) day = day - 7;
	if (day < 0) day = day + 7;

	switch (day) {
		case 0:
		strcpy(tx, "Sunday");
		break;

		case 1:
		strcpy(tx, "Monday");
		break;

		case 2:
		strcpy(tx, "Tuesday");
		break;

		case 3:
		strcpy(tx, "Wednesday");
		break;

		case 4:
		strcpy(tx, "Thursday");
		break;

		case 5:
		strcpy(tx, "Friday");
		break;

		default:
		strcpy(tx, "Saturday");
	}
}

void getdate(int offset) { //offset is in days

	time_t ltime;
	struct tm tm;
	int m;
	char mtx[20];

	ltime = time(NULL); //Now in sec
	ltime = ltime + (offset * 86400);

	tm = *localtime(&ltime);

	m = tm.tm_mon + 1;
	getmonthtx(m);
	strcpy(mtx, tx);

	for (int i = 0; i < INQUIRY_LENGTH; i++) tx[i] = 0; //clear the buffer

	sprintf(tx, "%s %d, %d", mtx, tm.tm_mday, (tm.tm_year + 1900));

}

void getmonth(int offset) { //offset is in number of months

	time_t ltime;
	struct tm tm;
	int m;

	for (int i = 0; i < INQUIRY_LENGTH; i++) tx[i] = 0; //clear the buffer

	ltime = time(NULL); //Now
	tm = *localtime(&ltime);

	m = tm.tm_mon + offset;
	if (m > 11) m = m - 12;
	if (m < 0) m = m + 12;
	m++;

	getmonthtx(m);
}

void getmonthtx(int m) {

	switch (m) {
		case 1:
		strcpy(tx, "January");
		break;

		case 2:
		strcpy(tx, "February");
		break;

		case 3:
		strcpy(tx, "March");
		break;

		case 4:
		strcpy(tx, "April");
		break;

		case 5:
		strcpy(tx, "May");
		break;

		case 6:
		strcpy(tx, "June");
		break;

		case 7:
		strcpy(tx, "July");
		break;

		case 8:
		strcpy(tx, "August");
		break;

		case 9:
		strcpy(tx, "September");
		break;

		case 10:
		strcpy(tx, "October");
		break;

		case 11:
		strcpy(tx, "November");
		break;

		default:
		strcpy(tx, "December");
	}
}

void getyear(int offset) { //offset is in number of years

	time_t ltime;
	struct tm tm;

	for (int i = 0; i < INQUIRY_LENGTH; i++) tx[i] = 0;; //clear the buffer

	ltime = time(NULL); //Now in sec
	ltime = ltime + (offset * 86400 * 365);

	tm = *localtime(&ltime);

	sprintf(tx, "%d", (tm.tm_year + 1900));

}


