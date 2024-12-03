//The Neural Database - Routines to create Image databases
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


// Functions:
int AddImages();
int AddImage(MNISTimage *, ImageData *, char *, long, int, int);
int WriteImageNdb(char *, ImageData *, int);
int mpCreateImageNdbs();
int StoreImages(MNISTimage *, ImageData *, char *, long, int, int);
void GetImageData(ImageData *, int, int *, NDBimage *, NDBimage *, NDBimage *);
void StoreImage(ImageData *, int, int *, char *, char);
void addImageRNcount(ImageData *);
void GetSmallPanelImage(int , NDBimage *, NDBimage *);
void GetLargePanelImage(int , NDBimage *, NDBimage *);
void ImageFormatter(ImageData *, NDBimage *, int *); //Also called from NdbRecognize.c
int RNvalue(int *, int);
void GetImageFromRec(MNISTimage *, long, char *);
int GetImage(char *, int, NDBimage *, NDBimage *, NDBimage *); //Also called from NdbRecognize.c
int CheckNeighbors(char, char, int, int, NDBimage *);
void EliminateNoise(NDBimage *);
int CheckForNoise(int, int, char, NDBimage *);
void GetImageRNs(ImageData *, NDBimage *);
int FindCavitiesFB(NDBimage *);
int CavityCount(NDBimage *);
int Pedestal(NDBimage *);
int FindInterior(NDBimage *);
int GetLongestRow(NDBimage *);
void ImageShape(ImageData *, NDBimage *);
void GetLongestLine(ImageData *, NDBimage *);
void GetImageWeights(ImageData *, NDBimage *);
int Curvy(NDBimage *);
int FindNeighbor(int *, int *, int *, int *, int *, int *, char *, ImageBoundary *, NDBimage *);


int AddImages(void) {
	//
	//	Add new MNIST images to the existing image Ndb's 2000+
	//
	//	When testing the 10,000 images from the MNIST Test Set (menu option #11),
	//	the record numbers of the images that fail to be correctly recognized are
	//	written into the file MNISTerrors[] = "C:\Ndb\mnist_errors.txt"
	//
	//	This function adds these failed Test Set images to the 399 image databases as
	//	a demonstration of the Ndb's ability to rapidly learn from mistakes by simply
	//	updating the databases with new data.
	//
	//	Since this application is NOT a "real" database with API functions for adding
	//	and deleting individual items of data, the following method is more complex
	//	than it would otherwise need to be.
	//
	//	This routine does the following:
	//
	//		1) Load into memory all the MNIST Test images from:	C:\Ndb\mnist_test.txt
	//
	//		2) Load into memory all the Record Numbers stored in C:\Ndb\mnist_errors.txt
	//
	//		3) Load into memory all 399 image Ndb's, each copied into STRUCT mp[i].pIdata
	//		   with room for the additional images.
	//
	//		4) Update all the image Ndb's by adding the images listed in mnist_errors.txt
	//
	//		5) Write the updated Ndb files back into C:\Ndb\ndbdatabases\... overwriting
	//		   the existing image Ndb's.
	//
	//	You can then test the result by again running Option #11.
	//
	//----------

	typedef struct {
		int N;
		int contrast;
		ImageData pIdata;
	} NMP; //list of databases to update

	NMP mp[399];
	int MPcount;
	FILE *fh_read;
	char line[IMAGE_LENGTH];
	int N;
	int contrast, maxlen;
	int i, j, k, p;
	long RecNum;
	NdbData Ndb;
	int WriteError, BadImage, er;
	char ndbfile[INQUIRY_LENGTH];

	long NumberOfImages = 10000;
	char TestImageFile[20] = "mnist_test.txt";
	char ImageSource[10] = "test"; //part of the image name that's stored in the Ndb

	char num[10];
	MNISTimage *pIMG;	//MNIST images are loaded from the file into this memory
	int IMGcount;

	int RECcount;
	int *pRecNum;
	int RECblocks;

	clock_t T;
	double runtime;

	ShowProgress = 0;

	//Loading MNIST Test images from TestImageFile[];
	fh_read = fopen(TestImageFile, "r");
	if (fh_read == NULL) {
		printf("\nERROR: Failed to OPEN file %s for reading.", TestImageFile);
		printf("\n");
		return 1;
	}
	pIMG = (MNISTimage *)malloc((NumberOfImages+50) * IMAGE_LENGTH); //a little extra room (+50)
	IMGcount = 1; //=RecNum: 1, 2, ...
	while (fgets(line, (IMAGE_LENGTH), fh_read) != NULL) {
		j = 0;
		for (i = 0; i < IMAGE_LENGTH; i++) {
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
		printf("\nERROR: Failed to find any images in %s", TestImageFile);
		printf("\nThe line format should be 'digit,pixel,pixel,...'");
		printf("\n");
		free(pIMG);
		return 1;
	}

	//Loading the Record Numbers of the images to be added from DataFile
	fh_read = fopen(MNISTerrors, "r");
	if (fh_read == NULL) {
		printf("\nERROR: Failed to OPEN file %s for reading.", MNISTerrors);
		printf("\n");
		free(pIMG);
		return 1;
	}
	//Start off with room for 256 Record Numbers
	pRecNum = (int *)malloc(256 * sizeof(int));
	RECblocks = 1;
	RECcount = 0;
	while (fgets(line, FILE_LINE_LENGTH, fh_read) != NULL) {
		//Ignore all lines except those that begin with ';;'
		//Looking for line[] = ;;RecNum, some text...
		if ((line[0] == ';') && (line[1] == ';')) {
			p = 2;
			while (line[p] != 10) { //linefeed
				for (i = 0; i < 10; i++) num[i] = 0; //clear the buffer
				i = 0;
				while ((line[p] != ',') && (line[p] != 10)) { // not a comma, not a linefeed
					num[i] = line[p];
					i++;
					p++;
				}
				if (i > 0) {
					RecNum = atol(num);
					if (RecNum > 0) { //just in case
						RECcount++;
						if ((RECcount % 256) == 0) { // The list is full, get more memory...
							RECblocks++;
							pRecNum = (int *)realloc(pRecNum, (RECblocks * 256 * sizeof(int)));
						}
						pRecNum[RECcount] = RecNum;
					}
				}
				break; //In this file there is only one Record Number per line, so get the next line...
			}
		}
	}
	fclose(fh_read);
	if (RECcount == 0) {
		printf("\nERROR: Failed to find any Record Numbers in %s", MNISTerrors);
		printf("\nThe line format should be ';;RecNum, ... some text...'");
		printf("\n");
		free(pRecNum);
		free(pIMG);
		return 1;
	}

	printf("\nLoading into memory the image Ndb's #2000 through #2569...");
	T = clock();

	//Row perspective...
	MPcount = 0;
	for (N = 2000; N <= 2069; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N>1999) && (N < 2010)) contrast=2;
		if ((N > 2009) && (N < 2020)) contrast=33;
		if ((N > 2019) && (N < 2030)) contrast=66;
		if ((N > 2029) && (N < 2040)) contrast=100;
		if ((N > 2039) && (N < 2050)) contrast=133;
		if ((N > 2049) && (N < 2060)) contrast=166;
		if ((N > 2059) && (N < 2070)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2101; N <= 2169; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2099) && (N < 2110)) contrast=2;
		if ((N > 2109) && (N < 2120)) contrast=33;
		if ((N > 2119) && (N < 2130)) contrast=66;
		if ((N > 2129) && (N < 2140)) contrast=100;
		if ((N > 2139) && (N < 2150)) contrast=133;
		if ((N > 2149) && (N < 2160)) contrast=166;
		if ((N > 2159) && (N < 2170)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	//COL() perspective...
	for (N = 2200; N <= 2269; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N > 2199) && (N < 2210)) contrast=2;
		if ((N > 2209) && (N < 2220)) contrast=33;
		if ((N > 2219) && (N < 2230)) contrast=66;
		if ((N > 2229) && (N < 2240)) contrast=100;
		if ((N > 2239) && (N < 2250)) contrast=133;
		if ((N > 2249) && (N < 2260)) contrast=166;
		if ((N > 2259) && (N < 2270)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2301; N <= 2369; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2299) && (N < 2310)) contrast=2;
		if ((N > 2309) && (N < 2320)) contrast=33;
		if ((N > 2319) && (N < 2330)) contrast=66;
		if ((N > 2329) && (N < 2340)) contrast=100;
		if ((N > 2339) && (N < 2350)) contrast=133;
		if ((N > 2349) && (N < 2360)) contrast=166;
		if ((N > 2359) && (N < 2370)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	//DIAG1() perspective (upper left to lower right)...
	for (N = 2400; N <= 2469; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N > 2399) && (N < 2410)) contrast=2;
		if ((N > 2409) && (N < 2420)) contrast=33;
		if ((N > 2419) && (N < 2430)) contrast=66;
		if ((N > 2429) && (N < 2440)) contrast=100;
		if ((N > 2439) && (N < 2450)) contrast=133;
		if ((N > 2449) && (N < 2460)) contrast=166;
		if ((N > 2459) && (N < 2470)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2501; N <= 2569; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2499) && (N < 2510)) contrast=2;
		if ((N > 2509) && (N < 2520)) contrast=33;
		if ((N > 2519) && (N < 2530)) contrast=66;
		if ((N > 2529) && (N < 2540)) contrast=100;
		if ((N > 2539) && (N < 2550)) contrast=133;
		if ((N > 2549) && (N < 2560)) contrast=166;
		if ((N > 2559) && (N < 2570)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	for (i = 0; i < MPcount; i++) {
		Ndb.ID = mp[i].N;
		er = LoadNdb(&Ndb);
		if (er != 0) { //Ndb #N failed to load...
			sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, Ndb.ID);
			printf("\n\nERROR: Failed to load Ndb #%d from the database sub-directory: %s", Ndb.ID, ndbfile);
			for (j = 0; j < i; j++) {
				free(mp[j].pIdata.pImageRN);
				free(mp[j].pIdata.pON);
				free(mp[j].pIdata.pRNtoON);
			}
			free(pRecNum);
			free(pIMG);
			return 1;
		}
		//Copy Ndb -> mp[i].pIdata
		//Allocate memory for the set of Image RN values
		mp[i].pIdata.pImageRN = (NdbImageRN *)malloc(IMAGE_RN_RECORDS * sizeof(NdbImageRN));
		mp[i].pIdata.ImageRNblocks = 1; //Initially room for 100
		mp[i].pIdata.RNcount = 0;
		for (j = 1; j <= Ndb.RNcount; j++) {
			addImageRNcount(&(mp[i].pIdata)); //use this in case the RN block in memory has to increase (unlikely)
			mp[i].pIdata.pImageRN[j].RN = Ndb.pIRN[j].RN;//always numeric for images
		}
		//Allocate memory for ONs with room for the new ONs
		mp[i].pIdata.ONcount = Ndb.ONcount;
		mp[i].pIdata.pON = (NdbON *)malloc((Ndb.ONcount + RECcount + 1) * sizeof(NdbON));
		for (j = 0; j < Ndb.ONcount; j++) {
			//copy Ndb.pON data to mp[i].pIdata.pON;
			strcpy(mp[i].pIdata.pON[j].ON, Ndb.pON[j+1].ON);
			strcpy(mp[i].pIdata.pON[j].SUR, Ndb.pON[j+1].SUR);
			strcpy(mp[i].pIdata.pON[j].ACT, Ndb.pON[j+1].ACT);
			mp[i].pIdata.pON[j].Len = Ndb.pON[j+1].Len;
			for (k = 0; k < Ndb.pON[j+1].Len; k++) {
				mp[i].pIdata.pON[j].RL[k+1] = Ndb.pON[j+1].RL[k];
			}
		}
		//maxlen = number of RNs in the longest Recognition List
		//For MNIST images, they all have the same number of RNs
		//Allocate menory for RN-to-ON connections with room for the new ONs
		mp[i].pIdata.ConnectCount = Ndb.ConnectCount;
		maxlen = NUMBER_OF_IMAGE_RNS;
		mp[i].pIdata.pRNtoON = (NdbRNtoON *)malloc((Ndb.RNcount+1) * (maxlen+1) * (Ndb.ONcount+RECcount+1) * sizeof(NdbRNtoON));
		for (j = 0; j < Ndb.ConnectCount; j++) {
			mp[i].pIdata.pRNtoON[j].RNcode = Ndb.pRNtoON[j].RNcode;
			mp[i].pIdata.pRNtoON[j].Pos = Ndb.pRNtoON[j].Pos;
			mp[i].pIdata.pRNtoON[j].ONcode = Ndb.pRNtoON[j].ONcode;
		}
		FreeMem(&Ndb);
	}

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;
	printf("finished in %fsec", runtime);

	printf("\nAdding %d MNIST Test Set images to these Ndb's...", RECcount);
	printf("\nRecNum=");
	T = clock();
	BadImage = 0;
	for (i = 1; i <= RECcount; i++) {
		RecNum = pRecNum[i];
		printf("%d ",RecNum);
		//Store this image in all the Image Ndb's
		for (j = 0; j < MPcount; j++) {
			N = mp[j].N;
			contrast = mp[j].contrast;
			er = AddImage(pIMG, &(mp[j].pIdata), ImageSource, RecNum, N, contrast);
			if (er == 1) BadImage = RecNum;
		}
	}

	if (BadImage != 0) {
		printf("\nERROR: Record Number %d is a bad image.", BadImage);
		printf("\nThe MNIST image file may be corrupt.");
		printf("\n");
	} else {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf("\nFinished storing these additional images in %fsec.", runtime);

		printf("\nWriting out the 399 updated Ndb files...");
		T = clock();
		WriteError = 0;
		for (i = 0; i < MPcount; i++) {
			//There's no reason at this point to create the subdirectory for
			//datafiles because you wouldn't have gotten this far without it.
			N = mp[i].N;
			sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, N); //file name
			er = WriteImageNdb(ndbfile, &(mp[i].pIdata), N);
			if (er != 0) WriteError = N;
		}
		if (WriteError > 0) {
			printf("\nERROR: One or more Ndb files (such as %d.ndb) failed to be written.", WriteError);
			printf("\n");
		} else {
			T = clock() - T;
			runtime = ((double)T) / CLOCKS_PER_SEC;
			printf("finished in %fsec.", runtime);
		}
	}

	for (i = 0; i < MPcount; i++) {
		free(mp[i].pIdata.pImageRN);
		free(mp[i].pIdata.pON);
		free(mp[i].pIdata.pRNtoON);
	}
	free(pRecNum);
	free(pIMG);

	return 0;
}

int AddImage(MNISTimage *pIMG, ImageData *pIdata, char *ImageSource, long RecNum, int N, int contrast) {
	//
	//	ImageSource = "test", i.e. adding MNIST test image to Ndb #2000+
	//
	//	RecNum = MNIST record number
	//
	//	N = NdbID for this image, or 'panel' in the image, at this 'contrast'
	//
	//----------

	NDBimage ROW;
	NDBimage COL;
	NDBimage DIAG;
	char ImageString[IMAGE_LENGTH];
	int res;
	int i;
	char ImageName[32];
	int ImageRN[NUMBER_OF_IMAGE_RNS+4]; //some extra room
	char digit;

	pIdata->RecNum = RecNum;
	pIdata->digit = pIMG[RecNum].image[0];
	GetImageFromRec(pIMG, RecNum, ImageString);
	res = GetImage(ImageString, contrast, &ROW, &COL, &DIAG); //Returns ROW[], COL[], DIAG[]
	if (res == 1) {
		//Bad image (there aren't any unless the file has been corrupted)
		return 1;
	}
	GetImageData(pIdata, N, ImageRN, &ROW, &COL, &DIAG); //Fills in the Recognition List: ImageRN[]
	for (i = 0; i < 32; i++) ImageName[i] = 0;
	sprintf(ImageName, "Image^%s^%d", ImageSource, RecNum); //i.e "Image:test:12345"

	digit = pIMG[RecNum].image[0]; //The "label" assigned to this image, i.e. 0, 1, ..., 9
	StoreImage(pIdata, N, ImageRN, ImageName, digit);

	return 0;
}

int WriteImageNdb(char *ndbfile, ImageData *pIdata, int N) {
	//
	//	Write this database into the filename specified in ndbfile[]
	//
	//----------

	FILE *fh_write;
	time_t ltime;
	char timestamp[32];
	int i, j;

	fh_write = fopen(ndbfile, "w");
	if (fh_write == NULL) return 1; //OPEN failed

	// Get timestamp
	ltime = time(NULL);
	strcpy(timestamp, ctime(&ltime));
	//Get rid of linefeed at the end of timestamp
	for (i = 0; i < 32; i++) {
		if (timestamp[i] != 10) continue;
		timestamp[i] = 0;
		break;
	}

	// Write ndbhead to file 'N'.ndb
	fprintf(fh_write, "NDB_HEAD\n");
	fprintf(fh_write, "Created=%s\n", timestamp);
	fprintf(fh_write, "ID=%d\n", N);
	fprintf(fh_write, "ONcount=%d\n", pIdata->ONcount);
	fprintf(fh_write, "RNcount=%d\n", pIdata->RNcount);
	fprintf(fh_write, "ConnectCount=%d\n", pIdata->ConnectCount);
	fprintf(fh_write, "Type=%s\n", "IMAGE_28X28");

	//Write ON data
	fprintf(fh_write, "\nNDB_ON\n");
	for (i = 0; i < pIdata->ONcount; i++) {
		fprintf(fh_write, "ONcode=%d,Len=%d,ON=%s", i+1, pIdata->pON[i].Len, pIdata->pON[i].ON);

		if (pIdata->pON[i].SUR[0] != 0) {
			fprintf(fh_write, ":%s", pIdata->pON[i].SUR);
			if (pIdata->pON[i].ACT[0] != 0) {
				fprintf(fh_write, ":%s", pIdata->pON[i].ACT);
			}
		} else {
			if (pIdata->pON[i].ACT[0] != 0) {
				fprintf(fh_write, "::%s", pIdata->pON[i].ACT);
			}
		}

		fprintf(fh_write, ",RL=%d", pIdata->pON[i].RL[1]);
		for (j = 2; j <= pIdata->pON[i].Len; j++) {
			fprintf(fh_write, ",%d", pIdata->pON[i].RL[j]);
		}
		fprintf(fh_write, "\n");
	}

	// Write RN codes (i)
	fprintf(fh_write, "\nNDB_RN\n");
	for (i = 1; i <= pIdata->RNcount; i++) {
		fprintf(fh_write, "%d=%d\n", i, pIdata->pImageRN[i].RN);
	}

	// Write RN-to-ON Connections
	fprintf(fh_write, "\nNDB_RN_TO_ON\n");
	for (i = 0; i < pIdata->ConnectCount; i++) {
		fprintf(fh_write, "RNcode=%d,Pos=%d,ON=%d\n", pIdata->pRNtoON[i].RNcode, pIdata->pRNtoON[i].Pos, pIdata->pRNtoON[i].ONcode);
	}

	fprintf(fh_write, "\n$$$ End Of File\n");
	fclose(fh_write);

	return 0;
}

int mpCreateImageNdbs(void) {
	//
	//	Create 399 Ndb's to recognize MNIST images: Ndb's #2000 - #2569
	//
	//	NOTE:	Since all these databases are independent of each other, multiple
	//			threads are used to create them. Also, although it's unimportant,
	//			the 399 databases are not numbered consecutively.
	//
	//
	//	The MNIST Training Set of handwritten digits has 60,000 images stored in 3 files:
	//			mnist_train_1.txt
	//			mnist_train_2.txt
	//			mnist_train_3.txt
	//	The MNIST Test Set has another 10,000 images stored in mnist_test.txt
	//	Both sets of data are available from http://yann.lecun.com/exdb/mnist
	//
	//	Each image is made up of 784 pixels in a 28x28 array with each pixel having a value
	//	from 0 to 255.
	//
	//	Instead of loading actual images into an Ndb, the Ndb solution converts an image
	//	into 399 different 'views'. The image is translated into 3 perspectives: Row, Column
	//	and Diagonal. Each of these is copied at 7 contrasts: 2/33/66/100/133/166/200.
	//	In a contrast view, all pixels below the contrast value are set to zero. All 21 of
	//	these views are further split into 9 overlapping 16x16 pixel panels, plus another
	//	9 overlapping 18x18 pixel panels. Each MNIST image is therefore translated into a total
	//	of 21 + (21*9) + (21*9) = 399 views.
	//
	//	Each of these views is then simplified by replacing all of the pixels with one of the
	//	following 3 characters:
	//				" " for a numeric zero
	//				"n" for any other numeric
	//				"+" for any zeros occupying interior spaces (areas completely surrounded by "n"s)
	//
	//	Each of these abstract views is used to generate 10 Recognition Neurons. The RNs represent
	//	general aspects of an image, such as whether it seems slanted to the left or right, whether
	//	it seems curvy or mostly straight, etc. An Ndb is created for each type of view and stores
	//	the RNs specific to that view.
	//
	//	When the Neural Database is presented with one of the Test images, it is run through the
	//	above process to generate the 399 sets of 10 RNs representing the image. An inquiry is
	//	then made into each of the Ndbs by the appropriate set of RNs. Whichever digit appears
	//	in the majority of the results is the Ndb's recognition of the image.
	//
	//----------

	typedef struct {
		int N;
		int contrast;
	} MP; //multithreading list of databases to create

	MP mp[399];
	int MPcount;
	FILE *fh_read;
	char line[IMAGE_LENGTH];
	int N;
	int contrast;
	int i, j;
	int WriteError;

	long NumberOfImages = 60000;
	char DataFile1[20] = "mnist_train_1.txt";
	char DataFile2[20] = "mnist_train_2.txt";
	char DataFile3[20] = "mnist_train_3.txt";
	char ImageSource[10] = "train"; //part of image name that is stored in the Ndb

	MNISTimage *pIMG;	//MNIST images are loaded from the 3 files into this memory
	long IMGcount;

	clock_t T;
	double runtime;

	ShowProgress = 0;
	ShowProgress = 1;

	WriteError = 0;

	printf("\nCreating MNIST image Ndb's #2000 through #2569 from %s, etc...", DataFile1);

	// Begin by reading all the images into memory...
	T = clock();

	pIMG = (MNISTimage *)malloc((NumberOfImages+50) * IMAGE_LENGTH); //a little extra room (+50)
	IMGcount = 1; //RecNum: 1, 2, ...

	fh_read = fopen(DataFile1, "r");
	if (fh_read == NULL) {
		printf("Error opening file %s for reading.\n", DataFile1);
		return 1;
	}
	while (fgets(line, (IMAGE_LENGTH), fh_read) != NULL) {
		j = 0;
		for (i = 0; i < IMAGE_LENGTH; i++) {
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
		if (IMGcount <= 20000) continue;
		break;
	}
	fclose(fh_read);

	fh_read = fopen(DataFile2, "r");
	if (fh_read == NULL) {
		printf("Error opening file %s for reading.\n", DataFile2);
		return 1;
	}
	while (fgets(line, (IMAGE_LENGTH), fh_read) != NULL) {
		j = 0;
		for (i = 0; i < IMAGE_LENGTH; i++) {
			if (line[i] == 10) break; //linefeed
			pIMG[IMGcount].image[j] = line[i];
			j++;
			if ((line[i] == ',') && (line[i+1] == ',')) {
				pIMG[IMGcount].image[j] = '0';
				j++;
			}
		}
		pIMG[IMGcount].image[j] = 0;
		IMGcount++;
		if (IMGcount <= 40000) continue;
		break;
	}
	fclose(fh_read);

	fh_read = fopen(DataFile3, "r");
	if (fh_read == NULL) {
		printf("Error opening file %s for reading.\n", DataFile3);
		return 1;
	}
	while (fgets(line, (IMAGE_LENGTH), fh_read) != NULL) {
		j = 0;
		for (i = 0; i < IMAGE_LENGTH; i++) {
			if (line[i] == 10) break; //linefeed
			pIMG[IMGcount].image[j] = line[i];
			j++;
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
		printf("Error: failed to find any images in %s, etc\n", DataFile1);
		printf("The line format should be 'digit,pixel,pixel,...'\n");
		free(pIMG);
		return 1;
	}

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;
	printf("\nCopied %d images into memory in %fsec", (IMGcount-1), runtime);

	printf("\nLoading images into the databases...");

	T = clock();

	//Create multiple Ndbs for all of the different 'views' of an image
	//ROW() perspective...
	MPcount = 0;
	for (N = 2000; N <= 2069; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N>1999) && (N < 2010)) contrast=2;
		if ((N > 2009) && (N < 2020)) contrast=33;
		if ((N > 2019) && (N < 2030)) contrast=66;
		if ((N > 2029) && (N < 2040)) contrast=100;
		if ((N > 2039) && (N < 2050)) contrast=133;
		if ((N > 2049) && (N < 2060)) contrast=166;
		if ((N > 2059) && (N < 2070)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2101; N <= 2169; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2099) && (N < 2110)) contrast=2;
		if ((N > 2109) && (N < 2120)) contrast=33;
		if ((N > 2119) && (N < 2130)) contrast=66;
		if ((N > 2129) && (N < 2140)) contrast=100;
		if ((N > 2139) && (N < 2150)) contrast=133;
		if ((N > 2149) && (N < 2160)) contrast=166;
		if ((N > 2159) && (N < 2170)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	//COL() perspective...
	for (N = 2200; N <= 2269; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N > 2199) && (N < 2210)) contrast=2;
		if ((N > 2209) && (N < 2220)) contrast=33;
		if ((N > 2219) && (N < 2230)) contrast=66;
		if ((N > 2229) && (N < 2240)) contrast=100;
		if ((N > 2239) && (N < 2250)) contrast=133;
		if ((N > 2249) && (N < 2260)) contrast=166;
		if ((N > 2259) && (N < 2270)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2301; N <= 2369; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2299) && (N < 2310)) contrast=2;
		if ((N > 2309) && (N < 2320)) contrast=33;
		if ((N > 2319) && (N < 2330)) contrast=66;
		if ((N > 2329) && (N < 2340)) contrast=100;
		if ((N > 2339) && (N < 2350)) contrast=133;
		if ((N > 2349) && (N < 2360)) contrast=166;
		if ((N > 2359) && (N < 2370)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	//DIAG1() perspective (upper left to lower right)...
	for (N = 2400; N <= 2469; N++) { //70 databases (1 full image + 9 panels, each at 7 contrasts)
		if ((N > 2399) && (N < 2410)) contrast=2;
		if ((N > 2409) && (N < 2420)) contrast=33;
		if ((N > 2419) && (N < 2430)) contrast=66;
		if ((N > 2429) && (N < 2440)) contrast=100;
		if ((N > 2439) && (N < 2450)) contrast=133;
		if ((N > 2449) && (N < 2460)) contrast=166;
		if ((N > 2459) && (N < 2470)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}
	for (N = 2501; N <= 2569; N++) { //63 databases (9 panels, each at 7 contrasts)
		if ((N % 10) == 0) continue;
		if ((N > 2499) && (N < 2510)) contrast=2;
		if ((N > 2509) && (N < 2520)) contrast=33;
		if ((N > 2519) && (N < 2530)) contrast=66;
		if ((N > 2529) && (N < 2540)) contrast=100;
		if ((N > 2539) && (N < 2550)) contrast=133;
		if ((N > 2549) && (N < 2560)) contrast=166;
		if ((N > 2559) && (N < 2570)) contrast=200;
		mp[MPcount].N = N;
		mp[MPcount].contrast = contrast;
		MPcount++;
	}

	//Request ActualThreads from the OS. You may or may not be given this many.
	omp_set_num_threads(ActualThreads);
	#pragma omp parallel
	{
		//Each thread has its own N, etc.
		int N;
		int contrast;
		double time;
		int res;
		int ThreadID;
		ImageData pIdata; //Image data unique to this thread

		ThreadID = omp_get_thread_num(); //0, 1, 2, ...

		//OMP will split this for-loop up among the threads, each thread will have its own i
		#pragma omp for
		for (i = 0; i < MPcount; i++) {
			N = mp[i].N;
			contrast = mp[i].contrast;

			time = omp_get_wtime(); //the start time

			res = StoreImages(pIMG, &pIdata, ImageSource, NumberOfImages, N, contrast);
			if (res != 0) WriteError = 1; //WriteError is shared - just to warn of a WriteError no matter who got it

			time = omp_get_wtime() - time; //runtime

			if (ShowProgress == 1) printf("\n  mpCreateImageNdbs(): ThreadID=%d loaded Ndb #%d in %fsec.", ThreadID, N, time);
		}
	}

	T = clock() - T;
	runtime = ((double)T) / CLOCKS_PER_SEC;

	printf("\nFinished loading MNIST images into all the Ndb's in %fsec.\n", runtime);
	if (WriteError == 1) printf("\nERROR: one or more threads couldn't OPEN it's N.ndb file for writing");

	free(pIMG);

	return 0;
}

int StoreImages(MNISTimage *pIMG, ImageData *pIdata, char *ImageSource, long NumberOfImages, int N, int contrast) {
	//
	//	ImageSource = "train", i.e. MNIST training images
	//
	//	RecNum = MNIST record number
	//
	//	N = NdbID for this view of an image or 'panel' at this 'contrast'
	//
	//----------

	NDBimage ROW;
	NDBimage COL;
	NDBimage DIAG;
	long RecNum;
	char ImageString[IMAGE_LENGTH];
	int res;
	int i, j;
	int maxlen;
	char ImageName[32];
	int ImageRN[NUMBER_OF_IMAGE_RNS+4]; //some extra room
	char digit;
	FILE *fh_write;
	char ndbfile[INQUIRY_LENGTH];
	time_t ltime;
	char timestamp[32];

	//Allocate memory for an initial 100 different RN values
	//There will be an RNcode created for each...
	pIdata->pImageRN = (NdbImageRN *)malloc(IMAGE_RN_RECORDS * sizeof(NdbImageRN));
	pIdata->ImageRNblocks = 1;

	// Run through the images in pIMG[] to get counts of the various components...
	//		1st time through get the counts of the number of records needed
	//		Then allocate memory for this Ndb
	//		2nd time through fill in the data, creating a copy of the Ndb in memory
	//		Finally, write out the 'N'.ndb file
	pIdata->RNcount = 0;
	pIdata->ONcount = 0;


	for (RecNum = 1; RecNum <= NumberOfImages; RecNum++) {
		pIdata->RecNum = RecNum;
		pIdata->digit = pIMG[RecNum].image[0];
		GetImageFromRec(pIMG, RecNum, ImageString);
		res = GetImage(ImageString, contrast, &ROW, &COL, &DIAG); //Returns ROW[], COL[], DIAG[]
		if (res == 1) {
			//Bad image (there aren't any bad MNIST images, unless the file has been corrupted)
			continue;
		}
		GetImageData(pIdata, N, ImageRN, &ROW, &COL, &DIAG); //Fills in the Recognition List: ImageRN[]
		pIdata->ONcount++;
	}

	// Allocate memory
	pIdata->pON = (NdbON *)malloc((pIdata->ONcount + 1) * sizeof(NdbON));

	// maxlen = number of RNs in the longest Recognition List
	//	For TEXT this would be the number of letters in the longest word
	//	For MNIST images, they all have the same number of RNs: see GetImageRNs()
	maxlen = NUMBER_OF_IMAGE_RNS;
	pIdata->pRNtoON = (NdbRNtoON *)malloc((pIdata->RNcount+1) * (maxlen+1) * (pIdata->ONcount+1) * sizeof(NdbRNtoON));

	pIdata->ONcount = 0;
	pIdata->ConnectCount = 0;

	// Run through the images again, loading data into the memory-resident Ndb...
	for (RecNum = 1; RecNum <= NumberOfImages; RecNum++) {
		pIdata->RecNum = RecNum;
		pIdata->digit = pIMG[RecNum].image[0];
		GetImageFromRec(pIMG, RecNum, ImageString);
		res = GetImage(ImageString, contrast, &ROW, &COL, &DIAG); //Returns ROW[], COL[], DIAG[]
		if (res == 1) continue; //Bad image
			
		GetImageData(pIdata, N, ImageRN, &ROW, &COL, &DIAG); //Fills in the Recognition List: ImageRN[]
		for (i = 0; i < 32; i++) ImageName[i] = 0;
		sprintf(ImageName, "Image^%s^%d", ImageSource, RecNum); //i.e "Image:train:12345"

		digit = pIMG[RecNum].image[0]; //The "label" assigned to this image: 0, 1, ..., 9

		StoreImage(pIdata, N, ImageRN, ImageName, digit);
	}

	// Write the data into file 'N'.ndb
	res = _mkdir(SubDirectoryNdbs); //create this subdirectory if it doesn't already exist
	sprintf(ndbfile, "%s%d.ndb", SubDirectoryNdbs, N);

	fh_write = fopen(ndbfile, "w");

	if (fh_write == NULL) {
		free(pIdata->pRNtoON);
		free(pIdata->pON);
		free(pIdata->pImageRN);
		return 1;
	}

	// Get timestamp
	ltime = time(NULL);
	strcpy(timestamp, ctime(&ltime));
	//Get rid of linefeed at the end of timestamp
	for (i = 0; i < 32; i++) {
		if (timestamp[i] != 10) continue;
		timestamp[i] = 0;
		break;
	}

	// Write header data...
	fprintf(fh_write, "NDB_HEAD\n");
	fprintf(fh_write, "Created=%s\n", timestamp);
	fprintf(fh_write, "ID=%d\n", N);
	fprintf(fh_write, "ONcount=%d\n", pIdata->ONcount);
	fprintf(fh_write, "RNcount=%d\n", pIdata->RNcount);
	fprintf(fh_write, "ConnectCount=%d\n", pIdata->ConnectCount);
	fprintf(fh_write, "Type=%s\n", "IMAGE_28X28");

	//Write ON data
	fprintf(fh_write, "\nNDB_ON\n");
	for (i = 0; i < pIdata->ONcount; i++) {
		fprintf(fh_write, "ONcode=%d,Len=%d,ON=%s", i+1, pIdata->pON[i].Len, pIdata->pON[i].ON);

		if (pIdata->pON[i].SUR[0] != 0) {
			fprintf(fh_write, ":%s", pIdata->pON[i].SUR);
			if (pIdata->pON[i].ACT[0] != 0) {
				fprintf(fh_write, ":%s", pIdata->pON[i].ACT);
			}
		} else {
			if (pIdata->pON[i].ACT[0] != 0) {
				fprintf(fh_write, "::%s", pIdata->pON[i].ACT);
			}
		}

		fprintf(fh_write, ",RL=%d", pIdata->pON[i].RL[1]);
		for (j = 2; j <= pIdata->pON[i].Len; j++) {
			fprintf(fh_write, ",%d", pIdata->pON[i].RL[j]);
		}
		fprintf(fh_write, "\n");
	}

	// Write RN codes (i)
	fprintf(fh_write, "\nNDB_RN\n");
	for (i = 1; i <= pIdata->RNcount; i++) {
		fprintf(fh_write, "%d=%d\n", i, pIdata->pImageRN[i].RN);
	}

	// Write RN-to-ON Connections
	fprintf(fh_write, "\nNDB_RN_TO_ON\n");
	for (i = 0; i < pIdata->ConnectCount; i++) {
		fprintf(fh_write, "RNcode=%d,Pos=%d,ON=%d\n", pIdata->pRNtoON[i].RNcode, pIdata->pRNtoON[i].Pos, pIdata->pRNtoON[i].ONcode);
	}

	fprintf(fh_write, "\n$$$ End Of File\n");
	fclose(fh_write);

	free(pIdata->pRNtoON);
	free(pIdata->pON);
	free(pIdata->pImageRN);
	return 0;
}

void GetImageData(ImageData *pIdata, int N, int *ImageRN, NDBimage *ROW, NDBimage *COL, NDBimage *DIAG) {
	//
	//	Get the Image RNs and set them into the appropriate Ndb
	//
	//	Specifically, from the database ID (N) and the panel number,
	//	select the perspective for this database (ROW, COL, or DIAG),
	//	and either the full image (panel=0) or the sub-image panel.
	//
	//	Then copy the appropriate pixels into A[r][c] and use that
	//	'view' to generate the Recognition List (ImageRN) for the
	//	image/sub-image.
	//
	//	Finally, store ImageRN[] with its "digit" ON in Ndb #N.
	//
	//----------

	int r, c;
	int panel;
	int rn;
	NDBimage A;
	int fnd;
	int RNcode;

	A.ImageSize = 28;
	for (r = 1; r <= 28; r++) {
		for (c = 1; c <= 28; c++) A.image[r][c] = ' ';
	}

	panel = N - ((N / 10) * 10); //0,1,...,9

	if (N < 2099) { //ROW[] and 16x16 ROW() panels
		if (panel == 0) { //ROW full image 28x28
			A.ImageSize = 28;
			for (r = 1; r <= 28; r++) {
				for (c = 1; c <= 28; c++) A.image[r][c] = ROW->image[r][c];
			}
		} else {
			GetSmallPanelImage(panel, ROW, &A);
		}
	} else {
		if (N < 2199) { //18x18 ROW[] panels
			GetLargePanelImage(panel, ROW, &A);
		} else {
			if (N < 2299) { //COL[] and 16x16 COL[] panels
				if (panel == 0) { //COL full image 28x28
					A.ImageSize = 28;
					for (r = 1; r <= 28; r++) {
						for (c = 1; c <= 28; c++) A.image[r][c] = COL->image[r][c];
					}
				} else {
					GetSmallPanelImage(panel, COL, &A);
				}
			} else {
				if (N < 2399) { //18x18 COL[] panels
					GetLargePanelImage(panel, COL, &A);
				} else {
					if (N < 2499) { //DIAG[] and 16x16 DIAG[] panels
						if (panel == 0) { //DIAG full image 28x28
							A.ImageSize = 28;
							for (r = 1; r <= 28; r++) {
								for (c = 1; c <= 28; c++) A.image[r][c] = DIAG->image[r][c];
							}
						} else {
							GetSmallPanelImage(panel, DIAG, &A);
						}
					} else {
						if (N < 2599) { //18x18 DIAG([] panels
							GetLargePanelImage(panel, DIAG, &A);
						}
					}
				}
			}
		}
	}

	ImageFormatter(pIdata, &A, ImageRN); //Get the Recognition List of RNs: ImageRN[]

	//Continue building the list of RNcodes for all these images...
	for (r = 1; r <= NUMBER_OF_IMAGE_RNS; r++) {
		rn = ImageRN[r];
		if (rn == 0) break;
		//Is this rn already known?
		fnd = 0;
		for (RNcode = 1; RNcode <= pIdata->RNcount; RNcode++) {
			if (pIdata->pImageRN[RNcode].RN != rn) continue;
			fnd = 1;
			break;
		}
		if (fnd == 0) {
			addImageRNcount(pIdata); //1, 2, ...
			RNcode = pIdata->RNcount;
			pIdata->pImageRN[RNcode].RN = rn;//always numeric for images
		}
	}
}

void StoreImage(ImageData *pIdata, int N, int *ImageRN, char *ImageName, char digit) {
	//
	//	Store the image recognition data in a memory-resident copy of the Ndb
	//
	//----------

	int i, j, k;
	int len1, len2;
	int fnd;
	int rn;
	int RNcode;

	// Does this image already exist in the database?
	fnd = 0;
	for (i = 0; i < pIdata->ONcount ;i++) {
		if (strcmp(pIdata->pON[i].ON, ImageName) != 0) continue;
		fnd = 1;
		break;
	}
	if (fnd == 1) return;

	//Does this Recognition List already exist?
	fnd = 0; //assume NO
	for (i = 0; i < pIdata->ONcount ;i++) {

		len1 = NUMBER_OF_IMAGE_RNS;
		len2 = 0; //count of the number of identical RNs (in a row)
		for (k = 0; k < len1; k++) {
			rn = ImageRN[k+1];
			//What is the RNcode for this rn value?
			RNcode = 0;
			for (j = 1; j <= pIdata->RNcount; j++) {
				if (pIdata->pImageRN[j].RN != rn) continue;
				RNcode = j;
				break;
			}
			if (RNcode == 0) break; //Should never happen
			if (pIdata->pON[i].RL[k+1] != RNcode) break;
			len2++;
		}
		if (len2 == len1) {
			fnd = 1;
			break;
		}
	}
	if (fnd == 1) {
		//Found the same Recognition List in ONcode, [i];
		//Add this digit to the end of that ON's list of Surrogates,
		//if it's not already there...
		for (k = 0; k < INQUIRY_LENGTH; k++) {
			if (pIdata->pON[i].SUR[k] == digit) break;
			if (pIdata->pON[i].SUR[k] != 0) continue;
			pIdata->pON[i].SUR[k] = digit;
			break;
		}
		return;
	}

	//Add this ON/Image to the database...

	//Clear the Recognition and Surrogate Lists
	for (k = 0; k < INQUIRY_LENGTH; k++) {
		pIdata->pON[pIdata->ONcount].RL[k] = 0;
		pIdata->pON[pIdata->ONcount].SUR[k] = 0;
	}

	for (i = 1; i <= NUMBER_OF_IMAGE_RNS; i++) {
		rn = ImageRN[i];
		if (rn == 0) break;
		//What is the RNcode for this rn?
		RNcode = 0;
		for (k = 1; k <= pIdata->RNcount; k++) {
			if (pIdata->pImageRN[k].RN != rn) continue;
			RNcode = k;
			break;
		}

		//NOTE: RNcode should NEVER be zero

		// Load the connections between RNs and ONs...
		pIdata->pRNtoON[pIdata->ConnectCount].RNcode = RNcode;
		pIdata->pRNtoON[pIdata->ConnectCount].Pos = i;
		pIdata->pRNtoON[pIdata->ConnectCount].ONcode = pIdata->ONcount + 1;
		pIdata->ConnectCount++;

		//This Image/ON's Recognition List of RNcodes
		pIdata->pON[pIdata->ONcount].RL[i] = RNcode;
	}

	pIdata->pON[pIdata->ONcount].Len = NUMBER_OF_IMAGE_RNS;
	strcpy(pIdata->pON[pIdata->ONcount].ON, ImageName);
	pIdata->pON[pIdata->ONcount].ACT[0] = 0; //None of these images trigger an Action
	pIdata->pON[pIdata->ONcount].SUR[0] = digit;
	pIdata->ONcount++;
}

void addImageRNcount(ImageData *pIdata) {
	//
	//	Increment ImageRNcount and add more memory if necessary
	//
	//----------

	pIdata->RNcount++;

	if ((pIdata->RNcount % IMAGE_RN_RECORDS) == 0) { // The list is full
		pIdata->ImageRNblocks++;
		pIdata->pImageRN = realloc(pIdata->pImageRN, (pIdata->ImageRNblocks * IMAGE_RN_RECORDS * sizeof(NdbImageRN)));
	}
}

void GetSmallPanelImage(int panel, NDBimage *FROM, NDBimage *TO) {
	//
	//	Split the 28x28 MNIST image up into 9 overlapping panels, each 16x16
	//
	//----------

	int r, c;

	TO->ImageSize = 16;

	switch (panel) {

		case 1:	//Panel 1: 16x16, upper left
			for (r = 1; r <= 16; r++) {
				for (c = 1; c <= 16; c++) TO->image[r][c] = FROM->image[r][c];
			}
			break;

		case 2:	//Panel 2: 16x16, left center
			for (r = 7; r <= 22; r++) {
				for (c = 1; c <= 16; c++) TO->image[r-6][c] = FROM->image[r][c];
			}
			break;

		case 3:	//Panel 3: 16x16, lower left
			for (r = 13; r <= 28; r++) {
				for (c = 1; c <= 16; c++) TO->image[r-12][c] = FROM->image[r][c];
			}
			break;

		case 4:	//Panel 4: 16x16, upper middle
			for (r = 1; r <= 16; r++) {
				for (c = 7; c <= 22; c++) TO->image[r][c-6] = FROM->image[r][c];
			}
			break;

		case 5:	//Panel 5: 16x16, center
			for (r = 7; r <= 22; r++) {
				for (c = 7; c <= 22; c++) TO->image[r-6][c-6] = FROM->image[r][c];
			}
			break;

		case 6:	//Panel 6: 16x16, lower middle
			for (r = 13; r <= 28; r++) {
				for (c = 7; c <= 22; c++) TO->image[r-12][c-6] = FROM->image[r][c];
			}
			break;

		case 7:	//Panel 7: 16x16, upper right
			for (r = 1; r <= 16; r++) {
				for (c = 13; c <= 28; c++) TO->image[r][c-12] = FROM->image[r][c];
			}
			break;

		case 8:	//Panel 8: 16x16, right center
			for (r = 7; r <= 22; r++) {
				for (c = 13; c <= 28; c++) TO->image[r-6][c-12] = FROM->image[r][c];
			}
			break;

		default: //Panel 9: 16x16, lower right
			for (r = 13; r <= 28; r++) {
				for (c = 13; c <= 28; c++) TO->image[r-12][c-12] = FROM->image[r][c];
			}
	}
}

void GetLargePanelImage(int panel, NDBimage *FROM, NDBimage *TO) {
	//
	//	Split the 28x28 MNIST image up into 9 overlapping panels, each 18x18
	//
	//----------

	int r, c;

	TO->ImageSize = 18;

	switch (panel) {

		case 1:	//Panel 1: 18x18, upper left
			for (r = 1; r <= 18; r++) {
				for (c = 1; c <= 18; c++) TO->image[r][c] = FROM->image[r][c];
			}
			break;

		case 2:	//Panel 2: 18x18, left center
			for (r = 6; r <= 23; r++) {
				for (c = 1; c <= 18; c++) TO->image[r-5][c] = FROM->image[r][c];
			}
			break;

		case 3:	//Panel 3: 18x18, lower left
			for (r = 11; r <= 28; r++) {
				for (c = 1; c <= 18; c++) TO->image[r-10][c] = FROM->image[r][c];
			}
			break;

		case 4:	//Panel 4: 18x18, upper middle
			for (r = 1; r <= 18; r++) {
				for (c = 6; c <= 23; c++) TO->image[r][c-5] = FROM->image[r][c];
			}
			break;

		case 5:	//Panel 5: 18x18, center
			for (r = 6; r <= 23; r++) {
				for (c = 6; c <= 23; c++) TO->image[r-5][c-5] = FROM->image[r][c];
			}
			break;

		case 6:	//Panel 6: 18x18, lower middle
			for (r = 11; r <= 28; r++) {
				for (c = 6; c <= 23; c++) TO->image[r-10][c-5] = FROM->image[r][c];
			}
			break;

		case 7:	//Panel 7: 18x18, upper right
			for (r = 1; r <= 18; r++) {
				for (c = 11; c <= 28; c++) TO->image[r][c-10] = FROM->image[r][c];
			}
			break;

		case 8:	//Panel 8: 18x18, right center
			for (r = 6; r <= 23; r++) {
				for (c = 11; c <= 28; c++) TO->image[r-5][c-10] = FROM->image[r][c];
			}
			break;

		default: //Panel 9: 18x18, lower right
			for (r = 11; r <= 28; r++) {
				for (c = 11; c <= 28; c++) TO->image[r-10][c-10] = FROM->image[r][c];
			}
	}
}

void ImageFormatter(ImageData *pIdata, NDBimage *IMG, int *ImageRN) { //Also called from NdbRecognize.c
	//
	//	Produce ImageRNs to identify/recognize the image...
	//
	//	The image IMG is a square matrix of pixels (ImageSize x ImageSize) where:
	//		" " is a pixel of value zero
	//		"n" is a non-zero pixel
	//		"+" is a fully enclosed pixel of value zero
	//
	//	Most, if not all, of the ImageRNs have values of 0, 10, 20, ... , etc. Multiple
	//	occurences of RNs with the same value in the same Recognition List will cause
	//	the Ndb scanner to create many more possible competitors, each a different
	//	Bound Section, slowing down the recognition process. Adding an ever-increasing
	//	'offset' to these RNs makes them numerically unique, avoiding unnecessary
	//	competitions. For example, if these are the initial ImageRNs:
	//
	//		10,30,10,10...
	//
	//	the offset will change these values to:
	//
	//		110,230,310,410,...
	//
	//----------

	int offset;
	int ix;

	GetImageRNs(pIdata, IMG);

	//Clear the Recognition List (and its extra space)
	for (ix = 0; ix <= (NUMBER_OF_IMAGE_RNS+1); ix++) ImageRN[ix] = 0;

	offset = 0;
	ix = 0;

	//Is the image generally Curvy or Straight?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->CS);

	//The number of enclosed/interior spaces
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->INT);

	//Is the image slanted left or right?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->SLANT);

	//Is the image slim, medium or fat?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->GIRTH);

	//Where is the longest row line: top, middle, or bottom third of the image?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->LR);

	//Is the image top heavy, bottom heavy, or neither?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->RWTB);

	//Is the image weighted to the left, or right, or neither?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->RWRL);

	//Where are cavities located?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->CAV);

	//Is the bottom half of the image top-heavy (formal "7" central bulge) or
	//bottom-heavy (formal "1" pedestal)?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->PED);

	//Where are the longest line: in a row, column, or one of the diagonals?
	ix++;
	ImageRN[ix] = RNvalue(&offset, pIdata->LL);

}

int RNvalue(int *offset, int v) {
	*offset += 100;
	v += *offset;
	return v;
}

void GetImageFromRec(MNISTimage *pIMG, long RecNum, char *ImageString) {
	//
	//	From the Record Number, get the Image String of 28x28 = 784 characters
	//
	//		pIMG[RecNum].image[0] = The Digit
	//		pIMG[RecNum].image[1] = ','
	//		pIMG[RecNum].image[2] = 1st pixel value
	//		pIMG[RecNum].image[3] = ','
	//		pIMG[RecNum].image[4] = 2nd pixel value
	//		pIMG[RecNum].image[5] = ','
	//			etc.
	//
	//----------

	int p, i;
	char ch;

	for (i = 0; i < IMAGE_LENGTH; i++) ImageString[i] = 0;

	p = 2; //start reading the image string from here
	for (i = 0; i < IMAGE_LENGTH; i++) {
		ch = pIMG[RecNum].image[p];
		if ((ch == 0) || (ch == 10)) break;
		ImageString[i] = ch;
		p++;
	}
}

int GetImage(char *ImageString, int CONTRAST, NDBimage *ROW, NDBimage *COL, NDBimage *DIAG) {
	//
	//	Convert the image from a string of text:
	//
	//		ImageString[0] = 1st pixel value
	//		ImageString[1] = ','
	//		ImageString[2] = 2nd pixel value
	//		ImageString[3] = ','
	//			etc.
	//
	//	into 28x28 arrays in these 3 perspectives:
	//		ROW()
	//		COL()
	//		DIAG()
	//
	//	where:
	//		1) any numeric below CONTRAST will be converted to a zero
	//	then:
	//		2) " " is a pixel of value zero
	//		3) "n" is a non-zero pixel
	//		4) "+" is a fully enclosed pixel of value zero
	//
	//----------

	char ch;
	char x[4]; //the pixel in characters: 0 to 255 with room for a terminating 0
	int px;
	int p, i;
	int r, c;
	int fnd;
	int FirstRow, LastRow;
	int startc, startr;
	int row[MAX_IMAGE_SIZE+1][MAX_IMAGE_SIZE+1];
	char diag[MAX_IMAGE_SIZE+MAX_IMAGE_SIZE+1][MAX_IMAGE_SIZE+1];
	NDBimage T;

	int ImageSize = MAX_IMAGE_SIZE; //28x28, or 18x18, or 16x16

	p = 0; //start reading the Image String from here
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			ch = ImageString[p];
			for (i = 0; i < 4; i++) x[i] = 0; //clear the pixel buffer
			i = 0;
			while ((ch != ',') && (ch != 0)) {
				x[i] = ch;
				i++;
				p++;
				ch = ImageString[p];
			}
			x[i] = 0;
			px = atoi(x);
			if (px < CONTRAST) px = 0;
			row[r][c] = px;
			p++; //move past the ','
		}
	}

	//Remove any feature that contacts the edge of the image (noise)
	//This guarantees the image is surrounded by zeros
	for (r = 1; r <= ImageSize; r++) {
		row[r][1] = 0;
		row[r][ImageSize] = 0;
	}
	for (c = 1; c <= ImageSize; c++) {
		row[1][c] = 0;
		row[ImageSize][c] = 0;
	}

	//Copy row[][] into T.image[][], translating numbers into characters
	T.ImageSize = ImageSize;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (row[r][c] == 0) {
				T.image[r][c] = ' ';
			} else {
				T.image[r][c] = 'n';
			}
		}
	}

	//Mark the end boundary of each row with a 'B'
	fnd = 0;
	for (r = 1; r <= ImageSize; r++) {
		//Find the 1st non-zero pixel from the end...
		for (c = (ImageSize-1); c > 0; c--) {
			if (T.image[r][c] == ' ') continue;
			T.image[r][c+1] = 'B';
			fnd = 1;
			break;
		}
	}
	if (fnd == 0) return 1; //Bad image

	//Mark the Top boundary
	FirstRow = 0;
	for (r = 1; r <= (ImageSize-1); r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r+1][c] == 'n') {
				T.image[r][c] = 'B';
				FirstRow = r; //1st boundary row
			}
		}
		if (FirstRow != 0) break;
	}

	//Mark the Bottom boundary
	LastRow = 0;
	for (r = ImageSize; r > 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r-1][c] == 'n') {
				T.image[r][c] = 'B';
				LastRow = r; //Last boundary row
			}
		}
		if (LastRow != 0) break;
	}

	//Set the boundary pixels for all the rest of the rows,
	//including interior spaces
	for (r = (FirstRow+1); r <= (LastRow-1); r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r][c] != ' ') continue; //looking for 'zero', aka ' '

			//check behind
			if (c != 1) {
				if (T.image[r][c-1] == 'n') {
					T.image[r][c] = 'B';
					continue;
				}
			}

			//check forward
			if (c != ImageSize) {
				if (T.image[r][c+1] == 'n') {
					T.image[r][c] = 'B';
					continue;
				}
			}

			//check above
			if (T.image[r-1][c] == 'n') {
				T.image[r][c] = 'B';
				continue;
			}

			//check below
			if (T.image[r+1][c] == 'n') {
				T.image[r][c] = 'B';
				continue;
			}
		}
	}

 	//Identify all "exterior" boundaries with an 'E'
	for (r = FirstRow; r <= LastRow; r++) {
		for (c = ImageSize; c > 0; c--) {
			if (T.image[r][c] == ' ') continue;
			while (T.image[r][c] == 'B') {
				T.image[r][c] = 'E';
				c--;
				if (c < 1) break;
			}
			break;
		}
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r][c] == ' ') continue;
			while (T.image[r][c] == 'B') {
				T.image[r][c] = 'E';
				c++;
				if (c > ImageSize) break;
			}
			break;
		}
	}

	//There are 8 possible neighbors to each boundary marker ("B" and "E")
	//All boundary neighbors must be the same kind.
	//Change every "B" touching an "E" into an "E" (External Boundary)
	//All remaining "B"'s are "Interior Boundaries".
	fnd = 1; //Check front to back...
	while (fnd > 0) {
		fnd = 0;
		for (r = FirstRow; r <= LastRow; r++) {
			for (c = 1; c <= ImageSize; c++) {
				if (T.image[r][c] != 'B') continue;
				//Are any of B's eight neighbors an E?
				fnd = CheckNeighbors('B', 'E', r, c, &T);
			}
		}
	}

	fnd = 1; //Check Back to front...
	while (fnd > 0) {
		fnd = 0;
		for (r = FirstRow; r <= LastRow; r++) {
			for (c = ImageSize; c >= 1; c--) {
				if (T.image[r][c] != 'B') continue;
				//Are any of B's eight neighbors an E?
				fnd = CheckNeighbors('B', 'E', r, c, &T);
			}
		}
	}
	
	fnd = 1; //Check top to bottom...
	while (fnd > 0) {
		fnd = 0;
		for (c = 1; c <= ImageSize; c++) {
			for (r = FirstRow; r <= LastRow; r++) {
				if (T.image[r][c] != 'B') continue;
				//Are any of B's eight neighbors an E?
				fnd = CheckNeighbors('B', 'E', r, c, &T);
			}
		}
	}
	
	fnd = 1; //Check bottom to top...
	while (fnd > 0) {
		fnd = 0;
		for (c = 1; c <= ImageSize; c++) {
			for (r = LastRow; r >= FirstRow; r--) {
				if (T.image[r][c] != 'B') continue;
				//Are any of B's eight neighbors an E?
				fnd = CheckNeighbors('B', 'E', r, c, &T);
			}
		}
	}

	//Restore all "E"s back to " "s, leaving the "B"s to mark the interior boundaries
	for (r = FirstRow; r <= LastRow; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r][c] == 'E') T.image[r][c] = ' ';
		}
	}

	//Change all "B"s and their " "s to "+"s
	for (r = FirstRow; r <= LastRow; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (T.image[r][c] == 'B') {
				while (T.image[r][c] != 'n') {
					T.image[r][c] = '+';
					c++;
					if (c > ImageSize) break;
				}
			}
		}
	}

	//If any "+" touches a " ", change it to a " "
	fnd = 1;
	while (fnd > 0) {
		fnd = 0;
		for (r = FirstRow; r <= LastRow; r++) {
			for (c = 1; c <= ImageSize; c++) {
				if (T.image[r][c] != '+') continue;
				//Are any of +'s eight neighbors a ' '?
				fnd = CheckNeighbors('+', ' ', r, c, &T);
			}
			if (fnd > 0) break;
		}
	}

	EliminateNoise(&T);

	//Create ROW perspective
	ROW->ImageSize = ImageSize;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) ROW->image[r][c] = T.image[r][c];
	}

	//Create COL perspective
	COL->ImageSize = ImageSize;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) COL->image[c][r] = T.image[r][c];
	}

	//Create DIAG perspective...
	//initialize the diagonal array to zero, i.e. " "
	for (r = 1; r <= (ImageSize+ImageSize); r++) {
		for (c = 1; c <= ImageSize; c++) diag[r][c] = ' ';
	}

	p=0;
	for (startc = ImageSize; startc > 0; startc--) {
		c = startc;
		p++;
		for (r = 1; r <= ImageSize; r++) {
			if (c > ImageSize) break;
			diag[p][c-startc+1] = T.image[r][c];
			c++;
		}
	}

	for (startr = 2; startr <= ImageSize; startr++) {
		r = startr;
		p++;
		for (c = 1; c <= ImageSize; c++) {
			diag[p][c] = T.image[r][c];
			r++;
			if (r > ImageSize) break;
		}
	}

	//Reduce the diagonal image from 55x28 to 28x28 by keeping only the odd rows
	r = 1;
	for (px = 1; px <= p; px++) {
		for (c = 1; c <= ImageSize; c++) {
			DIAG->image[r][c] = diag[px][c];
		}
		r++;
		px++; //every other row
	}
	DIAG->ImageSize = ImageSize;

	return 0;
}

int CheckNeighbors(char FromChar, char ToChar, int r, int c, NDBimage *Z) {
	//
	//	A 'FromChar' has been found at Z->image[r][c]. If any of its 8 neighbors
	//	is a 'ToChar', change the FromChar to the ToChar.
	//
	//	Returns 1 if any such change took place
	//
	//----------
 
	int fnd;

	int ImageSize = Z->ImageSize;

	//Are any of the 8 neighbors a ToChar?
	fnd = 0;

	if ((c+1) <= ImageSize) {
		if (Z->image[r][c+1] == ToChar) fnd = 1; //right
	}
	if (fnd == 0) {
		if ((c-1) > 0) {
			if (Z->image[r][c-1] == ToChar) fnd = 1; //left
		}
		if (fnd == 0) {
			if ((r-1) > 0) {
				if (Z->image[r-1][c] == ToChar) fnd = 1; //up
			}
			if (fnd == 0) {
				if ((r+1) <= ImageSize) {
					if (Z->image[r+1][c] == ToChar) fnd = 1; //down
				}
				if (fnd == 0) {
					if (((r-1) > 0) && ((c+1) <= ImageSize)) {
						if (Z->image[r-1][c+1] == ToChar) fnd = 1; //upper right
					}
					if (fnd == 0) {
						if (((r-1) > 0) && ((c-1) > 0)) {
							if (Z->image[r-1][c-1] == ToChar) fnd = 1; //upper left
						}
						if (fnd == 0) {
							if (((r+1) <= ImageSize) && ((c+1) <= ImageSize)) {
								if (Z->image[r+1][c+1] == ToChar) fnd = 1; //lower right
							}
							if (fnd == 0) {
								if (((r+1) <= ImageSize) && ((c-1) > 0)) {
									if (Z->image[r+1][c-1] == ToChar) fnd = 1; //lower left
								}
							}
						}
					}
				}
			}
		}
	}

	if (fnd == 1) Z->image[r][c] = ToChar;

	return fnd;
}

void EliminateNoise(NDBimage *Z) {
	//
	//	Remove isolated pixels
	//
	//----------

	int r, c;
	char ch;
	int fnd;

	int ImageSize = Z->ImageSize;

	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {

			if (Z->image[r][c] == '+') {
				//Is this position ("+") surrounded by "n" ?
				ch = 'n';
				fnd = CheckForNoise(r, c, ch, Z);
				if (fnd == 8) Z->image[r][c] = ch; //Change "+" to "n"
				continue;
			}

			if (Z->image[r][c] == 'n') {
				//Is this position ("n") surrounded by " " ?
				ch = ' ';
				fnd = CheckForNoise(r, c, ch, Z);
				if (fnd == 8) Z->image[r][c] = ch; //Change "n" to " "
				continue;
			}
		}
	}
}

int CheckForNoise(int r, int c, char ch, NDBimage *Z) {
	//
	//	Check all 8 positions around this pixel for the same alternative: ch
	//
	//----------

	int cnt;
	char px;

	int ImageSize = Z->ImageSize;

	cnt = 0;

	if ((c+1) <= ImageSize) {
		px = Z->image[r][c + 1]; //check the pixel in front
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if ((c-1) > 0) {
		px = Z->image[r][c - 1]; //behind
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if ((r+1) <= ImageSize) {
		px = Z->image[r + 1][c]; //below
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if ((r-1) > 0) {
		px = Z->image[r - 1][c]; //above
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if (((r+1) <= ImageSize) && ((c+1) <= ImageSize)) {
		px = Z->image[r + 1][c + 1]; //below, right
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if (((r+1) <= ImageSize) && ((c-1) > 0)) {
		px = Z->image[r + 1][c - 1]; //below, left
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if (((r-1) > 0) && ((c+1) <= ImageSize)) {
		px = Z->image[r - 1][c + 1]; //above, right
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	if (((r-1) > 0) && ((c-1) > 0)) {
		px = Z->image[r - 1][c - 1]; //above, left
		if (px == ch) cnt++;
	} else {
		cnt++;
	}

	return cnt;
}

void GetImageRNs(ImageData *pIdata, NDBimage *X) {
	//
	//	Return these RNs:
	//		CS - Curvy vs. Straight
	//		INT - Interior space(s)
	//		SLANT, GIRTH - Shape
	//		LR - location of longest row line: top, middle, or bottom
	//		RWTB, RWRL - ROW top-heavy or bottom-heavy, right-heavy or left-heavy
	//		CAV - Cavity locations
	//		PED - Pedestal detection
	//		LL - location of the Longest Line: row, column, or one of the diagonals
	//
	//----------


	//How Curvy/Straight is the image?
	pIdata->CS = Curvy(X);

	//Get the RN for enclosed spaces, those marked with "+".
	pIdata->INT = FindInterior(X);

	//Get the general shape of the image
	ImageShape(pIdata, X); //returns pIdata->SLANT and pIdata->GIRTH

	//Loation of the Longest Row
	pIdata->LR = GetLongestRow(X); //returns LR

	//Is the image top-heavy or bottom heavy, more on the right or left?
	GetImageWeights(pIdata, X); //returns pIdata->RWTB and pIdata->RWRL

	//Find cavity locations
	pIdata->CAV = FindCavitiesFB(X);
	
	//Is there a 'pedestal' feature in the lower part of the image:
	pIdata->PED = Pedestal(X);

	//Where is the longest line: in a row, column, or one of the diagonals?
	GetLongestLine(pIdata, X); //-> pIdata->LL

}

int FindCavitiesFB(NDBimage *X) {
	//
	//	Find Front & Back cavities: cavF & cavB, and return only CAV
	//
	//	CAV = 0, none
	//		 = 10, one facing left
	//		 = 20, two facing left
	//		 = 30, one facing right
	//		 = 40, two facing right
	//		 = 50, one facing left, one facing right
	//		 = 60, two facing left, one facing right
	//		 = 70, one facing left, two facing right
	//		 = 80, two facing left, two facing right
	//
	//----------

	int r, c, rc;
	int cavF, cavB, CAV;
	NDBimage Z;

	int ImageSize = X->ImageSize;

	cavF = CavityCount(X);

	//Flip the image back to front
	Z.ImageSize = ImageSize;
	for (r = 1; r <= ImageSize; r++) {
		rc = ImageSize;
		for (c = 1; c <= ImageSize; c++) {
			Z.image[r][rc] = X->image[r][c];
			rc--;
		}
	}

	cavB = CavityCount(&Z);
	

	CAV = 0;
	if ((cavF == 1) && (cavB == 0)) {
		CAV = 10;
	} else {
		if ((cavF == 2) && (cavB == 0)) {
			CAV = 20;
		} else {
			if ((cavF == 0) && (cavB == 1)) {
				CAV = 30;
			} else {
				if ((cavF == 0) && (cavB == 2)) {
					CAV = 40;
				} else {
					if ((cavF == 1) && (cavB == 1)) {
						CAV = 50;
					} else {
						if ((cavF == 2) && (cavB == 1)) {
							CAV = 60;
						} else {
							if ((cavF == 1) && (cavB == 2)) {
								CAV = 70;
							} else {
								if ((cavF == 2) && (cavB == 2)) {
									CAV = 80;
								} else {
									//Too many cavities?
									CAV = 0;
								}
							}
						}
					}
				}
			}
		}
	}
 
	return CAV;
}

int CavityCount(NDBimage *X) {
	//
	//	Scan the left side of the image, counting the number of cavities
	//
	//----------

	int r, c;
	int c1, c2;
	int inCav;
	int ncav;

	int ImageSize = X->ImageSize;

	ncav = 0;

	c1 = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				c1 = c;
				break;
			}
		}
		if (c1 != 0) break;
	}
	if (c1 == 0) return 0; //blank image

	inCav = 0; //Flag: are you in/out of a cavity?

	//Go down the left side looking for cavities...
	r++;
	while (r <= ImageSize) {
		c2 = 0;
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				c2 = c;
				break;
			}
		}
		if (c2 != 0) {
			if (c1 > c2) {
				//If inCav=0: still searching for the beginning of a cavity
				if (inCav == 1) { //1st indication of moving out of a cavity
					ncav++;
					inCav = 0;
				}
			} else {
				if (c2 > c1) { //=> in a cavity
					if (inCav == 0) inCav = 1; //initial entry into a cavity
				}
			}
			c1 = c2;
		}
		r++;
	}

	return ncav;
}

int Pedestal(NDBimage *X) {
	//
	//	Is the bottom half of the image top-heavy (formal "7"), or
	//	bottom-heavy (formal "1")
	//
	//	PED = 0, nothing
	//		= 10, bottom-heavy (pedestal)
	//		= 20, top-heavy (central bulge)
	//
	//----------

	int r, c;
	int FirstR, LastR, height, q;
	int top, bottom;
	int PED;

	int ImageSize = X->ImageSize;

	FirstR = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				FirstR = r;
				break;
			}
		}
		if (FirstR != 0) break;
	}
	if (FirstR == 0) return 0; //blank image

	LastR = 0;
	for (r = ImageSize; r >= 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				LastR = r;
				break;
			}
		}
		if (LastR != 0) break;
	}

	height = LastR - FirstR + 1;
	q = height / 4;
	if (q < 3) return 0;

	//Which has more pixels: top half or bottom half?
	bottom = 0;
	for (r = LastR; r >= (LastR - q); r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') bottom++;
		}
	}

	top = 0;
	for (r = (LastR-q-1); r >= (LastR-q-1-q); r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') top++;
		}
	}
	
	PED = 0;
	if ((bottom + bottom) < top) {
		PED = 20; //top heavy
	} else {
		if ((top + top) < bottom) {
			PED = 10; //bottom heavy
		}
	}

	return PED;

}

int FindInterior(NDBimage *X) {
	//
	//	Search for the beginning and end of interior regions marked by "+"s.
	//
	//	INT = 0, none
	//		= 40, if 2
	//		= 30, if 1 in lower half
	//		= 20, if 1 in upper half
	//		= 10, if 1 and can't tell where
	//
	//----------

	int r, c;
	int FirstR, LastR;
	int fnd;
	int start1;
	int end1;
	int start2, end2;
	int mid;
	int INT;

	int ImageSize = X->ImageSize;

	FirstR = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				FirstR = r;
				break;
			}
		}
		if (FirstR != 0) break;
	}

	if (FirstR == 0) return 0; //blank image

	LastR = 0;
	for (r = ImageSize; r >= 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				LastR = r;
				break;
			}
		}
		if (LastR != 0) break;
	}

	start1 = 0;
	for (r = FirstR; r <= LastR; r++) {
		fnd = 0;
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == '+') {
				if (start1 == 0) start1 = r;
				end1 = r;
				fnd = 1;
				break; //get next row
			}
		}
		if (fnd == 0) { //Didn't find a "+" in this row...
			if (start1 > 0) break; //start1,end1 define an interior region
		}
	}

	if (start1 == 0) return 0; //no interior regions in this image

	//Is there a 2nd interior region?
	start2 = 0;
	for (r = end1+1; r <= LastR; r++) {
		fnd = 0;
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == '+') {
				if (start2 == 0) start2 = r;
				end2 = r;
				fnd = 1;
				break; //get next row
			}
		}
		if (fnd == 0) { //Didn't find a "+" in this row...
			if (start2 > 0) break; //start2,end2 define a 2nd interior region
		}
	}

	if (start2 > 0) {
		INT = 40; //There are 2 interior regions in this image
	} else {
		INT = 10; //There is only one interior space
		//Where is start1/end1: Top if start1<middle, Bottom if end1>middle
		mid = LastR - FirstR;
		mid = FirstR + (mid / 2);
		if (start1 < mid) {
			INT = 20; //upper half
		} else {
			if (end1 > mid) INT=30; //lower half
		}
	}

	return INT;
}

int GetLongestRow(NDBimage *X) {
	//
	//	Which third of the image has the longest row line:
	//
	//		LR = 0, blank image
	//			 10, top
	//			 20, middle
	//			 30, bottom
	//
	//----------

	int r, c;
	int startR, endR, height;
	int cnt, maxR, longR;
	float ratio;
	int LR;

	int ImageSize = X->ImageSize;

	LR = 0;

	startR = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				startR = r;
				break;
			}
		}
		if (startR != 0) break;
	}
	if (startR == 0) return 0; //blank image

	endR = 0;
	for (r = ImageSize; r >= 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				endR = r;
				break;
			}
		}
		if (endR != 0) break;
	}

	height = endR - startR + 1;

	//Get the longest row of pixels and its row-location...
	longR = 0; //location of longest row
	maxR = 0; //number of pixels in the longest row
	for (r = startR; r <= endR; r++) {
		cnt = 0;
		for (c = 1; c <= ImageSize; c++) {
			if ((X->image[r][c] == 'n') || (X->image[r][c] == '+')) cnt++;
		}
		if (cnt > maxR) {
			maxR = cnt;
			longR = r;
		}
	}

	//Where is maxR located in relation to FirstR?
	r = longR - startR + 1;
	ratio = (float)r / (float)height;

	if ((ratio+0.0001) < IMAGE_RN_LONGEST_ROW_LOWER_THRESHOLD) {
		LR = 10; //top third
	} else {
		if ((ratio+0.0001) < IMAGE_RN_LONGEST_ROW_UPPER_THRESHOLD) {
			LR = 20; //middle
		} else {
			LR = 30; //bottom third
		}
	}
	
	return LR;
}

void ImageShape(ImageData *pIdata, NDBimage *X) {
	//
	//	Is the image Slanted:
	//		SLANT = 0, No, or unknown
	//			  = 10, To the right
	//			  = 20, To the left
	//
	//		GIRTH = Greatest width of the image / height
	//			  = 0, Unknown 
	//			  = 10, slim
	//			  = 20, in between
	//			  = 30, fat
	//
	//----------

	int r, c;
	int FirstR;
	int FirstC;
	int LastR;
	int LastC;
	int FC, LC;
	int height, width;
	int x;
	float ratio;

	int ImageSize = X->ImageSize;

	pIdata->SLANT = 0;
	pIdata->GIRTH = 0;

	FirstR = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				FirstR = r;
				FirstC = c;
				break;
			}
		}
		if (FirstR != 0) break;
	}

	LastR = 0;
	for (r = ImageSize; r >= 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				LastR = r;
				LastC = c;
				break;
			}
		}
		if (LastR != 0) break;
	}

	height = LastR - FirstR + 1;

	if (height == 1) return; //blank image

	FC = 0;
	for (c = 1; c <= ImageSize; c++) {
		for (r = FirstR; r <= LastR; r++) {
			if (X->image[r][c] == 'n') {
				FC = c;
				break;
			}
		}
		if (FC != 0) break;
	}
	LC = 0;
	for (c = ImageSize; c >= 1; c--) {
		for (r = FirstR; r <= LastR; r++) {
			if (X->image[r][c] == 'n') {
				LC = c;
				break;
			}
		}
		if (LC != 0) break;
	}

	width = LC - FC + 1;

	//Get the slant...
	x = FirstC - LastC;
	if (x > 0) { //to the right
		ratio = (float)x / (float)width;
		if ((ratio-0.0001) > IMAGE_RN_SLANT_THRESHOLD) pIdata->SLANT = 10;
	} else {
		if (x < 0) { //to the left
			x = -x;
			ratio = (float)x / (float)width;
			if ((ratio-0.0001) > IMAGE_RN_SLANT_THRESHOLD) pIdata->SLANT = 20;
		}
	}

	//Get the girth...
	ratio = (float)width / (float)height;

	pIdata->GIRTH = 20; //in between
	if ((ratio-0.0001) > IMAGE_RN_FAT_THRESHOLD) {
		pIdata->GIRTH = 30;
	} else {
		if ((ratio+0.0001) < IMAGE_RN_SLIM_THRESHOLD) {
			pIdata->GIRTH = 10;
		}
	}
}

void GetLongestLine(ImageData *pIdata, NDBimage *X) {
	//
	//	Where is the longest line in the image?
	//		LL	=  0, there isn't one (blank image/panel)
	//			= 10, row
	//			= 20, column
	//			= 30, diagonal, centered on upper left
	//			= 40, diagonal centered on lower left
	//
	//----------

	int r, c;
	int cnt;
	int startr, startc;
	int maxR, maxC, maxD1, maxD2, max;

	int ImageSize = X->ImageSize;

	//Row line lengths...
	maxR = 0;
	for (r = 1; r <= ImageSize; r++) {
		cnt = 0;
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				cnt++;
				continue;
			}
			if ((cnt > 1) && (cnt > maxR)) maxR = cnt;
			cnt = 0;
		}
		if ((cnt > 1) && (cnt > maxR)) maxR = cnt;
	}

	//Column line lengths...
	maxC = 0;
	for (c = 1; c <= ImageSize; c++) {
		cnt = 0;
		for (r = 1; r <= ImageSize; r++) {
			if (X->image[r][c] == 'n') {
				cnt++;
				continue;
			}
			if ((cnt > 1) && (cnt > maxC)) maxC = cnt;
			cnt = 0;
		}
		if ((cnt > 1) && (cnt > maxC)) maxC = cnt;
	}

	//Longest diagonal_1 line (upper right to lower left)...
	maxD1 = 0;
	for (startc = ImageSize; startc >= 1; startc--) {
		cnt = 0;
		c = startc;
		for (r = 1; r <= ImageSize; r++) {
			if (X->image[r][c] == 'n') {
				cnt++;
			} else {
				if ((cnt > 1) && (cnt > maxD1)) maxD1 = cnt;
				cnt = 0;
			}
			c--; //move diagonally
			if (c < 1) break;
		}
		if ((cnt > 1) && (cnt > maxD1)) maxD1 = cnt;
	}
	for (startr = 2; startr <= ImageSize; startr++) {
		cnt = 0;
		r = startr;
		for (c = ImageSize; c >= 1; c--) {
			if (X->image[r][c] == 'n') {
				cnt++;
			} else {
				if ((cnt > 1) && (cnt > maxD1)) maxD1 = cnt;
				cnt = 0;
			}
			r++; //move diagonally
			if (r > ImageSize) break;
		}
		if ((cnt > 1) && (cnt > maxD1)) maxD1 = cnt;
	}

	//Longest diagonal_2 line (upper left to lower right)...
	maxD2 = 0;
	for (startc = 1; startc <= ImageSize; startc++) {
		cnt = 0;
		c = startc;
		for (r = 1; r <= ImageSize; r++) {
			if (X->image[r][c] == 'n') {
				cnt++;
			} else {
				if ((cnt > 1) && (cnt > maxD2)) maxD2 = cnt;
				cnt = 0;
			}
			c++; //move diagonally
			if (c > ImageSize) break;
		}
		if ((cnt > 1) && (cnt > maxD2)) maxD2 = cnt;
	}
	for (startr = 2; startr <= ImageSize; startr++) {
		cnt = 0;
		r = startr;
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				cnt++;
			} else {
				if ((cnt > 1) && (cnt > maxD2)) maxD2 = cnt;
				cnt = 0;
			}
			r++; //move diagonally
			if (r > ImageSize) break;
		}
		if ((cnt > 1) && (cnt > maxD2)) maxD2 = cnt;
	}

	pIdata->LL = 0;	//longest line
	max = 0;
	if ((maxR > 0) && (maxR > max)) {
		max = maxR;
		pIdata->LL = 10; //row is the longest
	}
	if ((maxC > 0) && (maxC > max)) {
		max = maxC;
		pIdata->LL = 20; //column is the longest
	}
	if ((maxD1 > 0) && (maxD1 > max)) {
		max = maxD1;
		pIdata->LL = 30; //diagonal_1 is the longest
	}
	if ((maxD2 > 0) && (maxD2 > max)) {
		max = maxD2;
		pIdata->LL = 40; //diagonal_2 is the longest
	}

}

void GetImageWeights(ImageData *pIdata, NDBimage *X) {
	//
	//	Returns: Image Weight
	//
	//	Which third, top-third or bottom-third, has the most pixels?
	//		WTB	= 30, top
	//			= 20, bottom
	//			= 10, neither
	//			= 0, indeterminate, blank image
	//	
	//	Which third, right-third or left-third, has the most pixels?
	//		WRL	= 30, right
	//			= 20, left
	//			= 10, neither
	//			= 0, indeterminate, blank image
	//	
	//----------

	int WTB, WRL;
	int FirstR, FirstC;
	int LastR, LastC;

	int r, c;
	int d;
	int third;
	int toR, top, bottom;
	int toC, left, right;
	float ratio;

	int ImageSize = X->ImageSize;

	//Which has more pixels, the top third or the bottom third?
	WTB = 0;

	FirstR = 0;
	for (r = 1; r <= ImageSize; r++) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				FirstR = r;
				break;
			}
		}
		if (FirstR != 0) break;
	}

	LastR = 0;
	for (r = ImageSize; r >= 1; r--) {
		for (c = 1; c <= ImageSize; c++) {
			if (X->image[r][c] == 'n') {
				LastR = r;
				break;
			}
		}
		if (LastR != 0) break;
	}

	d = LastR - FirstR + 1;

	if (d > 1) {
		third = d / 3;
		toR = FirstR + third;
		top = 0;
		for (r = FirstR; r <= toR; r++) {
			for (c = 1; c <= ImageSize; c++) {
				if ((X->image[r][c] == 'n') || (X->image[r][c] == '+')) top++;
			}
		}
		toR = LastR - third;
		bottom = 0;
		for (r = LastR; r >= toR; r--) {
			for (c = 1; c <= ImageSize; c++) {
				if ((X->image[r][c] == 'n') || (X->image[r][c] == '+')) bottom++;
			}
		}
		d = top + bottom;
		if (d > 0) {
			ratio = (float)top / (float)d;

			if ((ratio-0.0001) > IMAGE_RN_WEIGHT_UPPER_THRESHOLD) {
				WTB = 30;
			} else {

				if ((ratio+0.0001) < IMAGE_RN_WEIGHT_LOWER_THRESHOLD) {
					WTB = 20;
				} else {
					WTB = 10; //within 10% of each other
				}
			}
		}
	}

 	//Which has more pixels, the left third or the right third?
	WRL = 0;

	FirstC = 0;
	for (c = 1; c <= ImageSize; c++) {
		for (r = 1; r <= ImageSize; r++) {
			if (X->image[r][c] == 'n') {
				FirstC = c;
				break;
			}
		}
		if (FirstC != 0) break;
	}

	LastC = 0;
	for (c = ImageSize; c >= 1; c--) {
		for (r = 1; r <= ImageSize; r++) {
			if (X->image[r][c] == 'n') {
				LastC = c;
				break;
			}
		}
		if (LastC != 0) break;
	}

	d = LastC - FirstC + 1;
	if (d > 1) {
		third = d / 3;
		toC = FirstC + third;
		left = 0;
		for (c = FirstC; c <= toC; c++) {
			for (r = 1; r <= ImageSize; r++) {
				if ((X->image[r][c] == 'n') || (X->image[r][c] == '+')) left++;
			}
		}
		toC = LastC - third;
		right = 0;
		for (c = LastC; c >= toC; c--) {
			for (r = 1; r <= ImageSize; r++) {
				if ((X->image[r][c] == 'n') || (X->image[r][c] == '+')) right++;
			}
		}
		d = left + right;
		if (d > 0) {
			ratio = (float)left / (float)d;

			if ((ratio-0.0001) > IMAGE_RN_WEIGHT_UPPER_THRESHOLD) {
				WRL = 30;
			} else {

				if ((ratio+0.0001) < IMAGE_RN_WEIGHT_LOWER_THRESHOLD) {
					WRL = 20;
				} else {
					WRL = 10; //within 10% of each other
				}
			}
		}
	}

	pIdata->RWTB = WTB;
	pIdata->RWRL = WRL;
}

int Curvy(NDBimage *Z) {
	//
	//	How Curvy/Straight is image Z?
	//
	//	CS	= 10, straight
	//		= 20, curvy
	//
	//----------

	int r1, c1;
	int r2, c2;
	int fnd;
	int Straight, Curvy, CS;
	char prev[2]; //previous boundary direction
	ImageBoundary pIB;
	int ImageSize = Z->ImageSize;
	
	//clear the boundary trace
	for (r1 = 0; r1 < 196; r1++) {
		for (c1 = 0; c1 < 196; c1++) pIB.point[r1][c1] = 0;
	}

	//In image Z, find the 1st row of the image, the 1st "n".
	//Note that it may be on the image edge, especially if this is a panel.
	fnd = 0;
	for (r1 = 1; r1 <= ImageSize; r1++) {
		for (c1 = 1; c1 <= ImageSize; c1++) {
			if (Z->image[r1][c1] == 'n') {
				fnd = 1;
				pIB.point[r1][c1] = 1;
				break;
			}
		}
		if (fnd != 0) break;
	}
	if (fnd == 0) return 0; //blank image

	//remember the previous direction you were going in
	prev[0] = 0;
	prev[1] = 0;

	Straight = 0;
	Curvy = 0;
	
	while (1) {
		fnd = FindNeighbor(&r1, &c1, &r2, &c2, &Curvy, &Straight, prev, &pIB, Z);
		if (fnd == 0) break; //Can't find the next "n"
		r1 = r2;
		c1 = c2;
	}

	if (Curvy < IMAGE_RN_CS_THRESHOLD) {
		CS = 10; //Straight image
	} else {
		CS = 20; //Curvy image
	}

	return CS;

}

int FindNeighbor(int *r1, int *c1, int *r2, int *c2, int *Curvy, int *Straight, char *prev, ImageBoundary *pIB, NDBimage *Z) {
	//
	//	Going clockwise from the "n" at r1,c1 find the next "n" at r2,c2
	//
	//	Since this 'image' may be a sub-image (panel) of the main image, some
	//	of the "n"s may be on the edge of the image and should NOT alter the
	//	values of Straight/Curvy.
	//
	//----------

	int ImageSize = Z->ImageSize;

	if ((*r1 - 1) > 0) {
		if (pIB->point[*r1 - 1][*c1] == 0) { //This location has not been seen before
			if (Z->image[*r1 - 1][*c1] == 'n') { //up
				*r2 = *r1 - 1;
				*c2 = *c1;
				pIB->point[*r1][*c1] = 1;
				//If this new location is on the edge of the image, ignore it's contribution
				if ((*c2 != 1 ) && (*c2 != ImageSize)) {
					if ((prev[0] == 'U') && (prev[1] == 0)) {
						(*Straight)++;
					} else {
						(*Curvy)++;
						prev[0] = 'U';
						prev[1] = 0;
					}
				}
				return 1;
			}
		}
	}

	if ((*c1 + 1) <= ImageSize) {
		if (pIB->point[*r1][*c1 + 1] == 0) { //This location has not been seen before
			if (Z->image[*r1][*c1 + 1] == 'n') { //right
				*r2 = *r1;
				*c2 = *c1 + 1;
				pIB->point[*r1][*c1] = 1;
				//If this new location is on the edge of the image, ignore it's contribution
				if ((*r2 != 1) && (*r2 != ImageSize)) {
					if ((prev[0] == 'R') && (prev[1] == 0)) {
						(*Straight)++;
					} else {
						(*Curvy)++;
						prev[0] = 'R';
						prev[1] = 0;
					}
				}
				return 1;
			}
		}
	}

	if ((*r1 + 1) <= ImageSize) {
		if (pIB->point[*r1 + 1][*c1] == 0) { //This location has not been seen before
			if (Z->image[*r1 + 1][*c1] == 'n') { //down
				*r2 = *r1 + 1;
				*c2 = *c1;
				pIB->point[*r1][*c1] = 1;
				//If this new location is on the edge of the image, ignore it's contribution
				if ((*c2 != 1) && (*c2 != ImageSize)) {
					if ((prev[0] == 'D') && (prev[1] == 0)) {
						(*Straight)++;
					} else {
						(*Curvy)++;
						prev[0] = 'D';
						prev[1] = 0;
					}
				}
				return 1;
			}
		}
	}

	if ((*c1 - 1) > 0) {
		if (pIB->point[*r1][*c1 - 1] == 0) { //This location has not been seen before
			if (Z->image[*r1][*c1 - 1] == 'n') { //left
				*r2 = *r1;
				*c2 = (*c1) - 1;
				pIB->point[*r1][*c1] = 1;
				//If this new location is on the edge of the image, ignore it's contribution
				if ((*r2 != 1) && (*r2 != ImageSize)) {
					if ((prev[0] == 'L') && (prev[1] == 0)) {
						(*Straight)++;
					} else {
						(*Curvy)++;
						prev[0] = 'L';
						prev[1] = 0;
					}
				}
				return 1;
			}
		}
	}

	if (((*r1 - 1) > 0) && ((*c1 + 1) <= ImageSize)) {
		if (pIB->point[*r1 - 1][*c1 + 1] == 0) { //This location has not been seen before
			if (Z->image[*r1 - 1][*c1 + 1] == 'n') { //upper right
				*r2 = *r1 - 1;
				*c2 = *c1 + 1;
				pIB->point[*r1][*c1] = 1;
				if ((prev[0] == 'U') && (prev[1] == 'R')) {
					(*Straight)++;
				} else {
					(*Curvy)++;
					prev[0] = 'U';
					prev[1] = 'R';
				}
				return 1;
			}
		}
	}

	if (((*r1 + 1) <= ImageSize) && ((*c1 + 1) <= ImageSize)) {
		if (pIB->point[*r1 + 1][*c1 + 1] == 0) { //This location has not been seen before
			if (Z->image[*r1 + 1][*c1 + 1] == 'n') { //lower right
				*r2 = *r1 + 1;
				*c2 = *c1 + 1;
				pIB->point[*r1][*c1] = 1;
				if ((prev[0] == 'L') && (prev[1] == 'R')) {
					(*Straight)++;
				} else {
					(*Curvy)++;
					prev[0] = 'L';
					prev[1] = 'R';
				}
				return 1;
			}
		}
	}

	if (((*r1 + 1) <= ImageSize) && ((*c1 - 1) > 0)) {
		if (pIB->point[*r1 + 1][*c1 - 1] == 0) { //This location has not been seen before
			if (Z->image[*r1 + 1][*c1 - 1] == 'n') { //lower left
				*r2 = *r1 + 1;
				*c2 = *c1 - 1;
				pIB->point[*r1][*c1] = 1;
				if ((prev[0] == 'L') && (prev[1] == 'L')) {
					(*Straight)++;
				} else {
					(*Curvy)++;
					prev[0] = 'L';
					prev[1] = 'L';
				}
				return 1;
			}
		}
	}

	if (((*r1 - 1) > 0) && ((*c1 - 1) > 0)) {
		if (pIB->point[*r1 - 1][*c1 - 1] == 0) { //This location has not been seen before
			if (Z->image[*r1 - 1][*c1 - 1] == 'n') { //upper left
				*r2 = *r1 - 1;
				*c2 = *c1 - 1;
				pIB->point[*r1][*c1] = 1;
				if ((prev[0] == 'U') && (prev[1] == 'L')) {
					(*Straight)++;
				} else {
					(*Curvy)++;
					prev[0] = 'U';
					prev[1] = 'L';
				}
				return 1;
			}
		}
	}
 
	return 0;
}
