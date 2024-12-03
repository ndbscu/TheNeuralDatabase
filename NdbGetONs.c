//The Neural Database - Generate and refine possible solutions
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


int GetOns(NdbData *, RECdata *);
int GetBoundSections(NdbData *, RECdata *);
void addBHcount(RECdata *);
void addBLcount(RECdata *);
void mpCombineBoundSections(RECdata *);
void mpCombineBound1(RECdata *, int, long, long, int, int, long, int, int, int);
void mpCombineBound2(RECdata *, int, long, long, int, int, long, int, int);
void mpaddCcount(RECdata *, int);
int mpRunHitThreshold(NdbData *, RECdata *);
void addDcount(RECdata *);
void sortD(RECdata *);
void SimpleAnomalyCount(NdbData *, RECdata *);
int RunAnomalyThreshold(NdbData *, RECdata *);
void addEcount(RECdata *);
void sortE(NdbData *, RECdata *);
void LoadONs(NdbData *, RECdata *);
void addRcount(RECdata *);
void LoadR(NdbData *, RECdata *, int, int, int, int);
void StoreResults(NdbData *, RECdata *, int, COMP *);
int GetPER(NdbData *, RECdata *, int);
int GetQuality(NdbData *, RECdata *, int);
float CompositeScore(RECdata *, int);


int GetONs(NdbData *Ndb, RECdata *pRD) {
	//
	//	Use the RNs in the Input Stream (ISRN) to identify ALL possible ONs.
	//
	//	Based on the position of an RN in the ON's Recognition List, each ON stakes 
	//	out the Begin and End Boundaries of its "Bound Section" in the Input Stream.
	//
	//	The ONs are then filtered by various Thresholds to remove 'obvious' losers.
	//	Results are returned in R[].
	//
	//----------

	int res = 0;
	int i;


	// Initial memory allocations...

	//Head of an ON's linked list of Bound Sections derived from the Input Stream
	pRD->pBH = (BH *)malloc(BH_RECORDS * sizeof(BH));
	pRD->BHcount = 0;
	pRD->BHblocks = 1;

	pRD->pBL = (BL *)malloc(BL_RECORDS * sizeof(BL)); //Members of the ON's linked list
	pRD->BLcount = 0;
	pRD->BLblocks = 1;


	//"Combined" Bound Sections constructed from the initial Bound Sections in pBH/pBL
	for (i = 0; i < ActualThreads; i++) {
		pRD->mpC[i] = (BC *)malloc(C_RECORDS * sizeof(BC));
		pRD->mpCcount[i] = 0;
		pRD->mpCblocks[i] = 1;
	}

	//Bound Sections from mpC[] that have survived the Hit Threshold
	for (i = 0; i < (ActualThreads+1); i++) {
		pRD->mpD[i] = (D *)malloc(D_RECORDS * sizeof(D));
		pRD->mpDcount[i] = 0;
		pRD->mpDblocks[i] = 1;
	}

	//Bound Sections from pB[] that have survived the Hit Threshold
	pRD->pD = (D *)malloc(D_RECORDS * sizeof(D));
	pRD->Dcount = 0;
	pRD->Dblocks = 1;

	//Bound Sections from pD[] that have survived the Anomaly Threshold
	pRD->pE = (E *)malloc(E_RECORDS * sizeof(E));
	pRD->Ecount = 0;
	pRD->Eblocks = 1;


	//Get pB[], the list of all ONs and their Bound Sections identified by all the RNs in INPUT[]
	res = GetBoundSections(Ndb, pRD); //Returns the number of Bound Sections found

	if (res != 0) {

		//Use multiple threads to combine the rudimentary Bound Sections in pB[] into more
		//complete Bound Sections in mpC[]. Then copy from both pB[] and mpC[] into pD[] the
		//Bound Sections which pass the Hit Threshold.
		mpCombineBoundSections(pRD);
		res = mpRunHitThreshold(Ndb, pRD);
		if (res != 0) { //Some ONs have survived the HIT_THRESHOLD

			//Count the number of positional RN anomalies in each ON's Bound Section
			SimpleAnomalyCount(Ndb, pRD);

			//Get pE[], the list of all Bound Sections that survive the Anomaly Threshold
			res = RunAnomalyThreshold(Ndb, pRD);
			if (res != 0) {

				//Move the survivors in pE[] into pR[] along with their competition data
				LoadONs(Ndb, pRD);
			}
		}
	}


	free(pRD->pE);
	free(pRD->pD);
	for (i = 0; i < (ActualThreads+1); i++) free(pRD->mpD[i]);
	for (i = 0; i < ActualThreads; i++) free(pRD->mpC[i]);
	free(pRD->pBL);
	free(pRD->pBH);

	return res;
}

int GetBoundSections(NdbData *Ndb, RECdata *pRD) {
	//
	//	Go through all the RNs in the Input Stream, ISRN[], and build a list in pB[] of
	//	all the ONs recognized by the RNs, along with their hypothetical boundaries
	//
	//		qpos is the position of the RN in the Input Stream
	//		dpos is the position of that RN in the ON's Recognition List
	//
	//	In a TEXT Ndb, INPUT[qpos]=letter is the same as ISRN[qpos]=RN
	//
	//	Note that these hypothetical (and temporary) boundaries can extend into negative
	//	numbers. ZERO is a legitimate boundary location.
	//
	//	The processing after this expects pB[] to be sorted by Begin Boundary.
	//
	//----------
	
	int RNcode;
	int qpos;
	int dpos;
	long ONcode;
	int len;
	int B;
	int E;
	int j;
	long ic;
	long ihead, ifirst, ilast, inew, inext, iprev;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		T = clock();
		printf("\n\nGetBoundSections()...");
	}

	pRD->BHcount = 0;
	pRD->BLcount = 0;

	for (qpos = 0; qpos < INQUIRY_LENGTH; qpos++) {
		RNcode = pRD->ISRN[qpos];
		if (RNcode == 0) break; //End of Input Stream

		//Go through all known positions for this Recognition Neuron and mark
		//the hit in the Output Neuron, defining the range of its boundaries.
		for (ic = 0; ic < Ndb->ConnectCount; ic++) {
			if (Ndb->pRNtoON[ic].RNcode == RNcode) {
				dpos = Ndb->pRNtoON[ic].Pos;
				ONcode = Ndb->pRNtoON[ic].ONcode;

				len = Ndb->pON[ONcode].Len;
				B = qpos + 1 - dpos; //Begin Boundary
				E = B + len - 1; //End Boundary

				//pRD->ISRN[0] is boundary position 1
				B++;
				E++;

				addBLcount(pRD); //Get the index (pRD->BLcount) for the link for this Bound Section
				inew = pRD->BLcount;

				//Find the Header for this ON, if there is one...
				ihead = 0;
				for (j = 1; j <= pRD->BHcount; j++) {
					if (pRD->pBH[j].ONcode != ONcode) continue;
					ihead = j;
					break;
				}
				if (ihead == 0) { //create a new linked list for this ON
					addBHcount(pRD); //Get the next Header, index = pRD->BHcount
					ihead = pRD->BHcount;

					//header data
					pRD->pBH[ihead].First = inew;
					pRD->pBH[ihead].Last = inew;
					pRD->pBH[ihead].ONcode = ONcode;

					//First list member
					pRD->pBL[inew].Next = 0;
					pRD->pBL[inew].Prev = 0;

				} else {
					//Insert new Bound Section in Begin Boundry order
					ifirst = pRD->pBH[ihead].First;
					ilast = pRD->pBH[ihead].Last;
					if (B <= pRD->pBL[ifirst].B) {
						//Insert at the beginning
						pRD->pBH[ihead].First = inew;
						pRD->pBL[ifirst].Prev = inew;
						pRD->pBL[inew].Next = ifirst;
						pRD->pBL[inew].Prev = 0;
					} else {
						if (B >= pRD->pBL[ilast].B) {
							//Insert at the end
							pRD->pBH[ihead].Last = inew;
							pRD->pBL[ilast].Next = inew;
							pRD->pBL[inew].Next = 0;
							pRD->pBL[inew].Prev = ilast;
						} else {
							//Insert somewhere inbetween ifirst and ilast...
							//		B is > B(ifirst) and B < B(ilast)
							inext = ifirst;
							while (B > pRD->pBL[inext].B) {
								inext = pRD->pBL[inext].Next;
								if (inext == 0) {
									//Bad linked list error - should NEVER occur
									printf("\n\n ***** Bad Structure in GetBoundSections *****\n\n");
									return 0;
								}
							}
							pRD->pBL[inew].Next = inext;
							iprev = pRD->pBL[inext].Prev;
							pRD->pBL[inext].Prev = inew;
							pRD->pBL[inew].Prev = iprev;
							pRD->pBL[iprev].Next = inew;
						}
					}
				}
				pRD->pBL[inew].B = B;
				pRD->pBL[inew].E = E;
				pRD->pBL[inew].qpos = (qpos+1);
				pRD->pBL[inew].dpos = dpos;
				pRD->pBL[inew].Skip = 0;
			}
		}
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" created %d   runtime: %fsec", pRD->BLcount, runtime);
	}

	return pRD->BLcount;
}

void addBHcount(RECdata *pRD) {
	//
	//	Increment pRD->BHcount and add more memory if necessary
	//
	//----------

	pRD->BHcount++;

	if ((pRD->BHcount % BH_RECORDS) == 0) { // The list is full
		pRD->BHblocks++;
		pRD->pBH = realloc(pRD->pBH, (pRD->BHblocks * BH_RECORDS * sizeof(BH))); //pBH = Header for this ON
	}
}

void addBLcount(RECdata *pRD) {
	//
	//	Increment pRD->BLcount and add more memory if necessary
	//
	//----------

	pRD->BLcount++;

	if ((pRD->BLcount % BL_RECORDS) == 0) { // The list is full
		pRD->BLblocks++;
		pRD->pBL = realloc(pRD->pBL, (pRD->BLblocks * BL_RECORDS * sizeof(BL))); //pBL = linked list member
	}

}

void mpCombineBoundSections(RECdata *pRD) {
	//
	//	Create a new list of combined RNs within the same Bound Section
	//
	//	Combine any members of pRD->pB[] which reside in the same Bound Section, but
	//	retain the original members of pRD->pB[] in case any of them provide a better
	//	recognition.
	//
	//----------

	clock_t T;
	double runtime;
	int th, totc;

	if (ShowProgress == 1) {
		T = clock();
		printf("\n\nmpCombineBoundSections()...");
	}

	//Request ActualThreads from the OS. You may or may not be given this many.
	//Each thread will work on the Bound Sections belonging to a specific ON
	omp_set_num_threads(ActualThreads);
	#pragma omp parallel
	{
		int ThreadID;
		int ihead; //pointer to ONcode in pBH
		int B1, E1, qpos1, dpos1;
		int B2, E2, qpos2, dpos2;
		long firstI, I, J, K, ONcode;

		ThreadID = omp_get_thread_num(); //0, 1, 2, ...

		#pragma omp for //let omp divide up the work
		for (ihead = 1; ihead <= pRD->BHcount; ihead++) {
			ONcode = pRD->pBH[ihead].ONcode; //Every thread has it's own ONcode

			//construct combined Bound Sections from any of this ONcode's records
			I = pRD->pBH[ihead].First;
			while (I > 0) {
				B1 = pRD->pBL[I].B;
				E1 = pRD->pBL[I].E;

				//Find the lowest qpos for the B1,E1
				//The links are in B1 order
				qpos1 = pRD->pBL[I].qpos;
				dpos1 = pRD->pBL[I].dpos;
				firstI = I;
				J = I;
				while (J > 0) {
					J = pRD->pBL[J].Next;
					if (pRD->pBL[J].B != B1) break;
					if (pRD->pBL[J].E != E1) break;
					if (pRD->pBL[J].qpos < qpos1) {
						qpos1 = pRD->pBL[J].qpos;
						dpos1 = pRD->pBL[J].dpos;
						I = J;
					}
				}
				//I = the last member of J's Bound Section
				J = pRD->pBL[I].Next;
				while (J > 0) {
					B2 = pRD->pBL[J].B;
					if (B2 == B1) {
						J = pRD->pBL[J].Next;
						continue;
					}
					if (B2 > E1) break;
					E2 = pRD->pBL[J].E;
					qpos2 = pRD->pBL[J].qpos;
					dpos2 = pRD->pBL[J].dpos;

					//Find the lowest qpos for B2,E2
					//The links are in B2 order
					K = J;
					while (K > 0) {
						K = pRD->pBL[K].Next;
						if (pRD->pBL[K].B != B2) break;
						if (pRD->pBL[K].E != E2) break;
						if (pRD->pBL[K].qpos < qpos2) {
							qpos2 = pRD->pBL[K].qpos;
							dpos2 = pRD->pBL[K].dpos;
						}
					}
					//J = the last member of K's Bound Section

					if (qpos1 < qpos2) {
						if ((dpos1 <= dpos2) && (qpos2 <= E1)) {

							mpCombineBound1(pRD, ThreadID, firstI, J, B1, E1, ONcode, B2, E2, qpos2);
						}
					} else {
						if ((dpos1 >= dpos2) && (qpos1 <= E2)) {

							mpCombineBound2(pRD, ThreadID, firstI, J, B1, E1, ONcode, B2, E2);
						}
					}
					J = pRD->pBL[J].Next;
					while (J > 0) {
						//Exit this Bound Section
						if (pRD->pBL[J].B != B2) break;
						if (pRD->pBL[J].E != E2) break;
						J = pRD->pBL[J].Next;
					}
				}
				I = pRD->pBL[I].Next;
				while (I > 0) {
					//Exit this Bound Section
					if (pRD->pBL[I].B != B1) break;
					if (pRD->pBL[I].E != E1) break;
					I = pRD->pBL[I].Next;
				}
			}
		}
	}


	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC; //runtime for all threads
		totc = 0;
		for (th = 0; th < ActualThreads; th++) totc = totc + pRD->mpCcount[th];
		printf(" %d new combinations   runtime: %fsec", totc, runtime);
	}

}

void mpCombineBound1(RECdata *pRD, int ThreadID, long firstI, long J, int B1, int E1, long ONcode, int B2, int E2, int qpos2) {
	//
	//	Create a new combination of Bound Sections by:
	//		1) copying pRD->pB[ON,B1,E1] to pRD->mpC[ON,B1,E1[]
	//		2) then add pRD->pB[ON,B2,E2].
	//
	//----------

	int i, lastq;
	int ok;
	long K, L;
	int cp;
	int is[INQUIRY_LENGTH];


	//DO NOT continue if there's a missing qpos between qpos2 and the last qpos
	for (i = 0; i < INQUIRY_LENGTH; i++) is[i] = 0; //clear the qpos list
	lastq = 0;
	K = J;
	while (K > 0) {
		if (pRD->pBL[K].B != B2) break;
		if (pRD->pBL[K].E != E2) break;
		i = pRD->pBL[K].qpos;
		is[i] = 1;
		if (i > lastq) lastq = i;
		K = pRD->pBL[K].Next;
	}
	//Are there any qpos's missing between (qpos2+1) and lastq?
	ok = 1;
	for (i = qpos2+1; i <= lastq; i++) {
		if (is[i] != 1) {
			ok = 0;
			break;
		}
	}
	if (ok == 0) return; //Get the next Bound Section: J+1 ...
	

	//Create a new combination: first copy pRD->pBL[ON,B1,E1] to pRD->mpC[].[ON,B1,E1]
	K = firstI;
	while (K > 0) {
		if (pRD->pBL[K].B < B1) {
			K = pRD->pBL[K].Next;
			continue;
		}
		if (pRD->pBL[K].B > B1) break;
		if (pRD->pBL[K].E != E1) {
			K = pRD->pBL[K].Next;
			continue;
		}
		//See if there's already a record for this qpos/dpos...
		ok = 1;
		for (cp = 0; cp < pRD->mpCcount[ThreadID]; cp++) {
			if (pRD->mpC[ThreadID][cp].B != B1) continue;
			if (pRD->mpC[ThreadID][cp].E != E1) continue;
			if (pRD->mpC[ThreadID][cp].ONcode != ONcode) continue;
			if (pRD->mpC[ThreadID][cp].qpos == pRD->pBL[K].qpos) {
				pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos; //only update the dpos
				ok = 0;
				break;
			}
		}
		if (ok == 1) {
			//Add a record for this qpos/dpos...
			cp = pRD->mpCcount[ThreadID];
			pRD->mpC[ThreadID][cp].B = B1;
			pRD->mpC[ThreadID][cp].E = E1;
			pRD->mpC[ThreadID][cp].ONcode = ONcode;
			pRD->mpC[ThreadID][cp].qpos = pRD->pBL[K].qpos;
			pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos;
			mpaddCcount(pRD, ThreadID);
		}
		K = pRD->pBL[K].Next;
	}

	//Now add the rest from pRD->pB[ON,B2,E2] to pRD->mpC[ON,B1,E1]...
	K = J;
	while (K > 0) {
		//Check all the qpos's of pRD->pB[ON,B2,E2]
		if (pRD->pBL[K].B != B2) break;
		if (pRD->pBL[K].E != E2) {
			K = pRD->pBL[K].Next;
			continue;
		}
		//Don't add this dpos if it's already attached to some qpos in
		//the source records from pRD->pBL[ON,B1,E1]
		ok = 1;
		L = firstI;
		while (L > 0) {
			if (pRD->pBL[L].B < B1) {
				L = pRD->pBL[L].Next;
				continue;
			}
			if (pRD->pBL[L].B > B1) break;
			if (pRD->pBL[L].E != E1) {
				L = pRD->pBL[L].Next;
				continue;
			}

			if (pRD->pBL[L].dpos == pRD->pBL[K].dpos) {
				ok = 0;
				break;
			}
			L = pRD->pBL[L].Next;
		}
		if (ok == 1) {
			//See if there's already a record for this qpos/dpos...
			for (cp = 0; cp < pRD->mpCcount[ThreadID]; cp++) {
				if (pRD->mpC[ThreadID][cp].B != B1) continue;
				if (pRD->mpC[ThreadID][cp].E != E1) continue;
				if (pRD->mpC[ThreadID][cp].ONcode != ONcode) continue;
				if (pRD->mpC[ThreadID][cp].qpos == pRD->pBL[K].qpos) {
					pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos; //only update the dpos
					ok = 0;
					break;
				}
			}
			if (ok == 1)  {
				//Add a record for this qpos/dpos...
				cp = pRD->mpCcount[ThreadID];
				pRD->mpC[ThreadID][cp].B = B1;
				pRD->mpC[ThreadID][cp].E = E1;
				pRD->mpC[ThreadID][cp].ONcode = ONcode;
				pRD->mpC[ThreadID][cp].qpos = pRD->pBL[K].qpos;
				pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos;
				pRD->mpC[ThreadID][cp].Skip = 0;
				mpaddCcount(pRD, ThreadID);
			}
		}
		K = pRD->pBL[K].Next;
	}
}

void mpCombineBound2(RECdata *pRD, int ThreadID, long firstI, long J, int B1, int E1, long ONcode, int B2, int E2) {
	//
	//	Create a new combination of Bound Sections by:
	//		1) copying pRD->pB[ON,B2,E2] to pRD->mpC[ON,B2,E2]
	//		2) then add pRD->pB[ON,B1,E1].
	//
	//----------

	int ok;
	long K, L;
	int cp;

	//Create a new combination: copy pRD->pB[ON,B2,E2] to pRD->mpC[ON,B2,E2]
	K = J;
	while (K > 0) {
		if (pRD->pBL[K].B != B2) break;
		if (pRD->pBL[K].E != E2) {
			K = pRD->pBL[K].Next;
			continue;
		}
		//Don't overwrite a qpos that's already there
		ok = 1;
		for (cp = 0; cp < pRD->mpCcount[ThreadID]; cp++) {
			if (pRD->mpC[ThreadID][cp].B != B2) continue;
			if (pRD->mpC[ThreadID][cp].E != E2) continue;
			if (pRD->mpC[ThreadID][cp].ONcode != ONcode) continue;
			if (pRD->mpC[ThreadID][cp].qpos == pRD->pBL[K].qpos) {
				pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos; //update the dpos
				ok = 0;
				break;
			}
		}
		if (ok == 1) {
			cp = pRD->mpCcount[ThreadID];
			pRD->mpC[ThreadID][cp].B = B2;
			pRD->mpC[ThreadID][cp].E = E2;
			pRD->mpC[ThreadID][cp].ONcode = ONcode;
			pRD->mpC[ThreadID][cp].qpos = pRD->pBL[K].qpos;
			pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos;
			mpaddCcount(pRD, ThreadID);
		}
		K = pRD->pBL[K].Next;
	}

	//Now add the rest from pRD->pBL[ON,B1,E1] to pRD->mpC[ON,B2,E2]
	K = firstI;
	while (K > 0) {
		//check all the qpos's of ON,B1,E1
		if (pRD->pBL[K].B < B1) {
			K = pRD->pBL[K].Next;
			continue;
		}
		if (pRD->pBL[K].B > B1) break;
		if (pRD->pBL[K].E != E1) {
			K = pRD->pBL[K].Next;
			continue;
		}
		//Don't add this dpos if it's already attached to some qpos in
		//the source records from pRD->pB[ON,B2,E2]
		ok = 1;
		L = J;
		while (L > 0) {
			if (pRD->pBL[L].B != B2) break;
			if (pRD->pBL[L].E != E2) {
				L = pRD->pBL[L].Next;
				continue;
			}
			if (pRD->pBL[L].dpos == pRD->pBL[K].dpos) {
				ok = 0;
				break;
			}
			L = pRD->pBL[L].Next;
		}
		if (ok == 1) {
			for (cp = 0; cp < pRD->mpCcount[ThreadID]; cp++) {
				if (pRD->mpC[ThreadID][cp].B != B2) continue;
				if (pRD->mpC[ThreadID][cp].E != E2) continue;
				if (pRD->mpC[ThreadID][cp].ONcode != ONcode) continue;
				if (pRD->mpC[ThreadID][cp].qpos == pRD->pBL[K].qpos) {
					pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos; //update the dpos
					ok = 0;
					break;
				}
			}
			if (ok == 1) {
				cp = pRD->mpCcount[ThreadID];
				pRD->mpC[ThreadID][cp].B = B2;
				pRD->mpC[ThreadID][cp].E = E2;
				pRD->mpC[ThreadID][cp].ONcode = ONcode;
				pRD->mpC[ThreadID][cp].qpos = pRD->pBL[K].qpos;
				pRD->mpC[ThreadID][cp].dpos = pRD->pBL[K].dpos;
				pRD->mpC[ThreadID][cp].Skip = 0;
				mpaddCcount(pRD, ThreadID);
			}
		}
		K = pRD->pBL[K].Next;
	}
}

void mpaddCcount(RECdata *pRD, int ThreadID) {
	//
	//	Increment pRD->mpCcount and add more memory if necessary
	//
	//----------

	pRD->mpCcount[ThreadID]++;

	if ((pRD->mpCcount[ThreadID] % C_RECORDS) == 0) { // The list is full
		pRD->mpCblocks[ThreadID]++;
		pRD->mpC[ThreadID] = realloc(pRD->mpC[ThreadID], (pRD->mpCblocks[ThreadID] * C_RECORDS * sizeof(BC))); //pRD->mpC[i][B,E,ON,qpos,dpos]
	}
}

int mpRunHitThreshold(NdbData *Ndb, RECdata *pRD) {
	//
	//	Remove contenders that don't have enough RN hits. Also, trim back the boundaries
	//	to their real limits in the Input Stream, i.e. no negative, or zero, boundaries.
	//
	//	Results: pB[] & mpC[] --> pD[]
	//
	//----------
	
	long I, J;
	long ONcode;
	int ihead;
	int i, j, th;
	int len;
	int qpos;
	int RNhits;
	int B1, E1;
	int BB, EB; //the real boundaries of the Bound Section in the Input Stream (from qpos)
	int is[INQUIRY_LENGTH];
	float x;
	BC *pc;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		T = clock();
		printf("\n\nmpRunHitThreshold()...");
	}

	pRD->Dcount = 0;

	for (ihead = 1; ihead <= pRD->BHcount; ihead++) {
		ONcode = pRD->pBH[ihead].ONcode;
		len = Ndb->pON[ONcode].Len;
		I = pRD->pBH[ihead].First;

		while (I > 0) {
			if (pRD->pBL[I].Skip == 0) {

				for (j = 0; j < INQUIRY_LENGTH; j++) is[j] = 0; //clear the qpos list
				B1 = pRD->pBL[I].B;
				E1 = pRD->pBL[I].E;
				BB = pRD->pBL[I].qpos;
				EB = BB;
				is[BB] = pRD->pBL[I].dpos;
				pRD->pBL[I].Skip = 1;

				J = pRD->pBL[I].Next;
				while (J > 0) {
					if (pRD->pBL[J].Skip == 0) {
						if (pRD->pBL[J].B != B1) break;
						if (pRD->pBL[J].E == E1) {
							qpos = pRD->pBL[J].qpos;
							is[qpos] = pRD->pBL[J].dpos;

							//The pB[] records are NOT in qpos order
							if (qpos < BB) BB = qpos;
							if (qpos > EB) EB = qpos;

							pRD->pBL[J].Skip = 1;
						}
					}
					J = pRD->pBL[J].Next;
				}

				RNhits = 0;
				for (j = 0; j < INQUIRY_LENGTH; j++) if (is[j] != 0) RNhits++;

				x = ((float)RNhits / (float)len);
				x = x - HIT_THRESHOLD - 0.001;

				//Don't filter out CENTRAL ONs
				if ((x > 0.0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {

					pRD->pD[pRD->Dcount].BB = BB; //boundaries of the Bound Section in the Input Stream
					pRD->pD[pRD->Dcount].EB = EB;
					pRD->pD[pRD->Dcount].ONcode = ONcode;
					for (j = 0; j < INQUIRY_LENGTH; j++) pRD->pD[pRD->Dcount].is[j] = is[j];
					pRD->pD[pRD->Dcount].RNhits = RNhits;
					pRD->pD[pRD->Dcount].cntA = 0;
					addDcount(pRD);
				}
			}
			I = pRD->pBL[I].Next;
		}
	}

	for (th = 0; th < ActualThreads; th++) {

		pc = pRD->mpC[th]; //simplify the syntax below

		for (i = 0; i < pRD->mpCcount[th]; i++) {

			if (pc[i].Skip == 1) continue;

			for (j = 0; j < INQUIRY_LENGTH; j++) is[j] = 0; //clear the qpos list
			B1 = pc[i].B;
			E1 = pc[i].E;
			BB = pc[i].qpos;
			EB = BB;
			ONcode = pc[i].ONcode;

			len = Ndb->pON[ONcode].Len;
			is[BB] = pc[i].dpos;
			pc[i].Skip = 1;
			for (j = (i+1); j < pRD->mpCcount[th]; j++) {

				//mpC[] records are NOT sorted by Begin Boundary
				if (pc[j].B != B1) continue;
				if (pc[j].E != E1) continue;
				if (pc[j].ONcode != ONcode) continue;
				//
				qpos = pc[j].qpos;
				is[qpos] = pc[j].dpos;

				//The mpC[] records are not necessairly in qpos order
				if (qpos < BB) BB = qpos;
				if (qpos > EB) EB = qpos;
				//
				pc[j].Skip = 1;
			}

			RNhits = 0;
			for (j = 0; j < INQUIRY_LENGTH; j++) if (is[j] != 0) RNhits++;

			x = ((float)RNhits / (float)len);
			x = x - HIT_THRESHOLD - 0.001;

			//Don't filter out CENTRAL ONs
			if ((x > 0.0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {

				pRD->pD[pRD->Dcount].BB = BB; //boundaries of the Bound Section in the Input Stream
				pRD->pD[pRD->Dcount].EB = EB;
				pRD->pD[pRD->Dcount].ONcode = ONcode;
				for (j = 0; j < INQUIRY_LENGTH; j++) pRD->pD[pRD->Dcount].is[j] = is[j];
				pRD->pD[pRD->Dcount].RNhits = RNhits;
				pRD->pD[pRD->Dcount].cntA = 0;
				addDcount(pRD);
			}
		}
	}

	if (pRD->Dcount > 0) sortD(pRD);

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" %d BoundSections survived  runtime: %fsec", pRD->Dcount, runtime);
	}

	return pRD->Dcount;
}

void addDcount(RECdata *pRD) {
	//
	//	Increment pRD->Dcount and add more memory if necessary
	//
	//----------

	pRD->Dcount++;

	if ((pRD->Dcount % D_RECORDS) == 0) { // The list is full
		pRD->Dblocks++;
		pRD->pD = realloc(pRD->pD, (pRD->Dblocks * D_RECORDS * sizeof(D))); //pRD->pD[B,E,ON,...]
	}
}

void sortD(RECdata *pRD) {
	//
	//	Sort pRD->pD[].B
	//
	//----------

	int i, j, c;
	int sort;
	D pt;
	D *pTEMP;

	// Remove Duplicates
	c = 0;
	pTEMP = (D *)malloc(D_RECORDS * pRD->Dblocks * sizeof(D));
	for (i = 0; i < pRD->Dcount; i++) {

		if (pRD->pD[i].ONcode == 0) continue; //ignore this record

		pTEMP[c] = pRD->pD[i];
		c++;
		for (j = (i+1); j < pRD->Dcount; j++) {

			if (pRD->pD[j].ONcode == 0) continue; //ignore this record

			if (pRD->pD[i].BB != pRD->pD[j].BB) continue;
			if (pRD->pD[i].EB != pRD->pD[j].EB) continue;
			if (pRD->pD[i].ONcode != pRD->pD[j].ONcode) continue;
			pRD->pD[j].ONcode = 0; //ignore this record
		}
	}
	pRD->Dcount = c;
	for (i = 0; i < pRD->Dcount; i++) pRD->pD[i] = pTEMP[i];
	free(pTEMP);

	//Sort by Begin Boundary
	sort = 1;
	while (sort == 1) {
		sort = 0;
		for (i = 0; i < pRD->Dcount; i++) {
			for (j = i; j < (pRD->Dcount-1); j++) {
				if (pRD->pD[j].BB > pRD->pD[j+1].BB) {
					pt = pRD->pD[j+1];
					pRD->pD[j+1] = pRD->pD[j];
					pRD->pD[j] = pt;
					sort = 1;
				}
			}
		}
	}
}

void SimpleAnomalyCount(NdbData *Ndb, RECdata *pRD) {
	//
	//	Add to pRD->D[] the number of Positional Anomalies: cntA.
	//
	//----------
	
	int i, qpos, dpos;
	int prevqpos, prevdpos;
	int cntA;

	for (i = 0; i < pRD->Dcount; i++) {
		cntA = 0;
		prevqpos = 0;
		prevdpos = 0;

		for (qpos = pRD->pD[i].BB; qpos <= pRD->pD[i].EB; qpos++) {
			dpos = pRD->pD[i].is[qpos];
			if (dpos != 0) {
				if (prevdpos == 0) {
					prevqpos = qpos;
					prevdpos = dpos;
				} else {
					if ((qpos-prevqpos) != 1) cntA++; //qpos anomaly
					if ((dpos-prevdpos) != 1) cntA++; //dpos anomaly
					prevqpos = qpos;
					prevdpos = dpos;
				}
			}
		}
		pRD->pD[i].cntA = cntA;
	}
}

int RunAnomalyThreshold(NdbData *Ndb, RECdata *pRD) {
	//
	//	Move ONs from pRD->pD[] to pRD->pE[] unless they have "too many" anomalies as
	//	determined by the Anomaly Threshold.
	//
	//	Also, if several ONs have the same Begin and End boundaries, AND the same number
	//	of RN hits, check only the one with the fewest number of anomalies. If there
	//	are multiple ONs with the fewest anomalies, check only those with the shortest
	//	lengths.
	//
	//----------
	
	int redo;
	int i, j;
	long ONcode;
	int ilen, jlen;
	int cntA;
	float x;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		T = clock();
		printf("\nRunAnomalyThreshold()...");
	}

	redo = 1;
	while (redo == 1) {
		redo = 0;
		for (i = 0; i < pRD->Dcount; i++) {
			if (redo == 1) break;
			if (pRD->pD[i].ONcode == 0) continue; //Ignore this deleted record
			for (j = (i+1); j < pRD->Dcount; j++) {
				if (redo == 1) break;
				if (pRD->pD[j].ONcode == 0) continue; //Ignore this deleted record

				if (pRD->pD[j].BB != pRD->pD[i].BB) continue;
				if (pRD->pD[j].EB != pRD->pD[i].EB) continue;
				if (pRD->pD[j].RNhits != pRD->pD[i].RNhits) continue;

				//Found another ON with the same boundaries and the same number of RNhits
				//Delete the record with the most anomalies
				if (pRD->pD[j].cntA > pRD->pD[i].cntA) {
					pRD->pD[j].ONcode = 0;
				} else {
					if (pRD->pD[j].cntA < pRD->pD[i].cntA) {
						pRD->pD[i].ONcode = 0;
						redo = 1;
					} else {
						//These two have the same BB, EB, RNhits, & cntA. Keep the shorter ON...
						ONcode = pRD->pD[i].ONcode;
						ilen = Ndb->pON[ONcode].Len;
						ONcode = pRD->pD[j].ONcode;
						jlen = Ndb->pON[ONcode].Len;
						if (jlen > ilen) {
							pRD->pD[j].ONcode = 0;
						} else {
							if (ilen > jlen) {
								pRD->pD[i].ONcode = 0;
								redo = 1;
							} else {
								//Check the anomaly threshold on both of these
							}
						}
					}
				}
			}
		}
	}

	pRD->Ecount = 0;

	for (i = 0; i < pRD->Dcount; i++) {
		if (pRD->pD[i].ONcode == 0) continue; //Ignore this deleted record

		cntA = pRD->pD[i].cntA;
		ONcode = pRD->pD[i].ONcode;
		ilen = Ndb->pON[ONcode].Len;
		x = ((float)cntA / (float)ilen);

		//If x is below the Anomaly Threshold, it is still a contender
		x = ANOMALY_THRESHOLD - x - 0.001;

		//Don't filter out CENTRAL ONs
		if ((x > 0.0) || (strcmp(Ndb->Type, "CENTRAL") == 0)) {

			pRD->pE[pRD->Ecount].B = pRD->pD[i].BB;
			pRD->pE[pRD->Ecount].E = pRD->pD[i].EB;
			pRD->pE[pRD->Ecount].ONcode = pRD->pD[i].ONcode;
			pRD->pE[pRD->Ecount].cntA = pRD->pD[i].cntA;
			addEcount(pRD);
		}
	}

	if (pRD->Ecount > 0) sortE(Ndb, pRD);

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" %d BoundSections survived  runtime: %fsec", pRD->Ecount, runtime);
	}

	return pRD->Ecount;
}

void addEcount(RECdata *pRD) {
	//
	//	Increment pRD->Ecount and add more memory if necessary
	//
	//----------

	pRD->Ecount++;

	if ((pRD->Ecount % E_RECORDS) == 0) { // The list is full
		pRD->Eblocks++;
		pRD->pE = (E *)realloc(pRD->pE, (pRD->Eblocks * E_RECORDS * sizeof(E))); //pE[B,E,ON,...]
	}

}

void sortE(NdbData *Ndb, RECdata *pRD) {
	//
	//	Sort pE[].B
	//
	//----------

	int i, j, k, c, x;
	int sort;
	E pt;

	//Sort by Begin Boundary
	sort = 1;
	while (sort == 1) {
		sort = 0;
		for (i = 0; i < pRD->Ecount; i++) {
			for (j = i; j < (pRD->Ecount-1); j++) {
				if (pRD->pE[j].B > pRD->pE[j+1].B) {
					pt = pRD->pE[j+1];
					pRD->pE[j+1] = pRD->pE[j];
					pRD->pE[j] = pt;
					sort = 1;
				}
			}
		}
	}


	//When two of the same ON have the same Begin Boundary, remove the shorter Bound Section
	c = 0; //number removed
	for (i = 0; i < (pRD->Ecount-1); i++) {
		for (j = (i+1); j < pRD->Ecount; j++) {
			
			if (pRD->pE[i].B != pRD->pE[j].B) break;

			if (pRD->pE[i].ONcode != pRD->pE[j].ONcode) continue;
			if (pRD->pE[i].E == pRD->pE[j].E) continue;

			// Same ON, same Begin Boundary, different End Boundaries
			if (pRD->pE[i].E > pRD->pE[j].E) {
				x = j;	//Move everything below j up...
			} else {
				x = i;	//Move everything below i up
			}
			for (k = x; k < (pRD->Ecount-1); k++) {
				pRD->pE[k] = pRD->pE[k+1];
			}
			j--;
			c++;
		}
	}
	if (c > 0) pRD->Ecount = pRD->Ecount - c;

}

void LoadONs(NdbData *Ndb, RECdata *pRD) {
	//
	//	From pRD->pE[] load pRD->pR[] with the remaining ONs along with their competition data
	//
	//----------

	int i;

	clock_t T;
	double runtime;

	if (ShowProgress == 1) {
		printf("\nLoadONs()...");
		T = clock();
	}

	pRD->Rcount = 0;
	for (i = 0; i < pRD->Ecount; i++) {
		addRcount(pRD); //pRD->Rcount = 1, 2, 3, ...
		LoadR(Ndb, pRD, pRD->Rcount, pRD->pE[i].B, pRD->pE[i].E, pRD->pE[i].ONcode);
	}

	if (ShowProgress == 1) {
		T = clock() - T;
		runtime = ((double)T) / CLOCKS_PER_SEC;
		printf(" BoundSections=%d  runtime: %fsec", pRD->Rcount, runtime);
	}

}

void addRcount(RECdata *pRD) {
	//
	//	Increment pRD->Rcount and add more memory if necessary
	//
	//----------

	pRD->Rcount++;

	if ((pRD->Rcount % R_RECORDS) == 0) { // The list is full
		pRD->Rblocks++;
		pRD->pR = (Rdata *)realloc(pRD->pR, (pRD->Rblocks * R_RECORDS * sizeof(Rdata))); //pRD->pR[B,E,ON,...]
	}
}

void LoadR(NdbData *Ndb, RECdata *pRD, int Index, int B, int E, int ONcode) {
	//
	//	Load pRD->pR with the "competition data" generated in the SCU. This routine calls
	//	into the SCU with no second competitor - just to gather the data.
	//
	//----------

	int j;
	int res;

	//Memory for the 2 competitors
	COMP A;
	COMP Z;

	//All it takes to ignore "Z" as a competitor is this:
	Z.Mcount = 0;

	// Load the "A" competitor with 'minimal' identifying data
	A.Mcount = 1; //This competitor is comprised of a single ON
	A.m[0].B = B;
	A.m[0].E = E;
	A.m[0].ONcode = ONcode;
	A.m[0].nrec = 0; // number of hits in .order[]
	for (j = 0; j < INQUIRY_LENGTH; j++) {
		A.m[0].RLhit[j] = 0;
		A.m[0].order[j].dpos = 0;
		A.m[0].order[j].qpos = 0;
		A.m[0].order[j].rn = 0;
	}
	A.m[0].PER = 0; //%recognition
	A.m[0].QUAL = 0; //orderliness
	A.m[0].cntA = 0; //positional anomalies
	A.Score = 0;
	A.spaceB = 0;
	A.anomaly = 0;
	A.rec = 0;
	A.minpr = 0;
	A.bound = 0;
	A.uncount = 0;
	A.mislead = 0;
	for (j = 0; j < INQUIRY_LENGTH; j++) A.SpaceClaim[j] = 0;

	// Run this fake 'competition' to load "A" with its competition data
	res = RunSCU(Ndb, pRD, &A, &Z);

	StoreResults(Ndb, pRD, Index, &A);
}

void StoreResults(NdbData *Ndb, RECdata *pRD, int Index, COMP *pA) {
	//
	// SCU and all other results
	//
	//----------

	int nrec;
	int i, j, k, fnd;
	int ONcode;
	int Len;

	for (i = 0; i < pA->Mcount; i++) { //check each ON in this competitor

		ONcode = pA->m[i].ONcode;
		Len = Ndb->pON[ONcode].Len;

		//Load the results into pRD->pR[]
		pRD->pR[Index].B = pA->m[i].B;
		pRD->pR[Index].E = pA->m[i].E;
		pRD->pR[Index].ONcode = ONcode;
 		pRD->pR[Index].Score = pA->Score;

		for (j = 0; j < INQUIRY_LENGTH; j++) { //clear memory, just in case
			pRD->pR[Index].qpos[j] = 0;
			pRD->pR[Index].rn[j] = 0;
			pRD->pR[Index].dpos[j] = 0;
		}

		nrec = pA->m[i].nrec; // number of hits recorded in the 'order' array: ->m[i].order[]

		for (j = 0; j < nrec; j++) {
			pRD->pR[Index].qpos[j] = pA->m[i].order[j+1].qpos;
			pRD->pR[Index].rn[j] = pA->m[i].order[j+1].rn;
			pRD->pR[Index].dpos[j] = pA->m[i].order[j+1].dpos;
		}

		pRD->pR[Index].InBranch = 0; //0: not in a branch, 1: in some branch (can be in more than one)
		
		pRD->pR[Index].PER = GetPER(Ndb, pRD, Index); //%recognition

		pRD->pR[Index].QUAL = GetQuality(Ndb, pRD, Index); //QUAL
		
		pRD->pR[Index].cntA = 0; //This count will be made AFTER any upcoming boundary adjustments

		pRD->pR[Index].C = CompositeScore(pRD, Index); //depends on all the above data

		//Is this ON reognized by ALL of its RNs, even if some are out-of-order?
		pRD->pR[Index].ALL = 1;  //assume yes
		for (j = 1; j <= Len; j++) { //Are all these positions recorded?
			fnd = 0;
			for (k = 0; k < INQUIRY_LENGTH; k++) {
				if (pRD->pR[Index].dpos[k] == 0) break; //end of list
				if (pRD->pR[Index].dpos[k] == j) {
					fnd = 1;
					break;
				}
			}
			if (fnd == 0) { //didn't find dpos j
				pRD->pR[Index].ALL = 0;
				break;
			}
		}		
	}
}

int GetPER(NdbData *Ndb, RECdata *pRD, int Index) {
	//
	//	To get the %recognition for this ON:
	//
	//	Get the 'perfect' score as if ALL RNs in the ON were recognized in the correct order.
	//	The %recognition (PER) is then the Stand-Alone Score from the Input Stream divided by
	//	this 'perfect' score
	//
	//----------

	int ONcode;
	int PER;
	int score;
	int i, j;
	int rn, fnd;
	int len;
	int temp[INQUIRY_LENGTH+1];

	//Memory for the 2 competitors
	COMP W;
	COMP Y;

	ONcode = pRD->pR[Index].ONcode;

	//Call the SCU with a single, perfect ON, and no second competitor 'W'
	W.Mcount = 0;

	// Load the "Y" competitor with a perfect copy of pRD->pR[Rcount].ONcode
	len = Ndb->pON[ONcode].Len;
	Y.Mcount = 1; //This competitor is comprised of a single ON
	Y.m[0].B = 1;
	Y.m[0].E = len;
	Y.m[0].ONcode = ONcode;
	Y.m[0].nrec = 0; // number of hits in .order[]
	for (j = 0; j < INQUIRY_LENGTH; j++) {
		Y.m[0].RLhit[j] = 0;
		Y.m[0].order[j].dpos = 0;
		Y.m[0].order[j].qpos = 0;
		Y.m[0].order[j].rn = 0;
	}
	Y.m[0].PER = 0; //%recognition
	Y.m[0].QUAL = 0; //orderliness
	Y.m[0].cntA = 0; //positional anomalies
	Y.Score = 0;
	Y.spaceB = 0;
	Y.anomaly = 0;
	Y.rec = 0;
	Y.minpr = 0;
	Y.bound = 0;
	Y.uncount = 0;
	Y.mislead = 0;
	for (j = 0; j < INQUIRY_LENGTH; j++) Y.SpaceClaim[j] = 0;

	//Save the Input Stream, then load it with a perfect set of RNs from the ON's Recognition List
	for (j = 0; j < INQUIRY_LENGTH; j++) temp[j] = 0;

	for (j = 0; j < INQUIRY_LENGTH; j++) {
		temp[j] = pRD->ISRN[j];
		if (pRD->ISRN[j] == 0) break;
	}
	for (j=0; j<INQUIRY_LENGTH; j++) {
		pRD->ISRN[j] = Ndb->pON[ONcode].RL[j]; //pRD->ISRN[] starts at 1
		if (pRD->ISRN[j] == 0) break;
	}


	// load competitor "Y" with competition data
	j = RunSCU(Ndb, pRD, &Y, &W);


	//restore ISRN[]
	for (j = 0; j < INQUIRY_LENGTH; j++) pRD->ISRN[j] = 0;
	for (j = 0; j < INQUIRY_LENGTH; j++) {
		pRD->ISRN[j] = temp[j];
		if (pRD->ISRN[j] == 0) break;
	}

	score = pRD->pR[Index].Score; //This is the non-perfect score from the Input Stream

	//Reduce the Stand-Alone Score from the Input Stream if there are any repetitions in dpos[]
	for (i = 0; i < INQUIRY_LENGTH; i++) temp[i] = 0;
	len = 0;
	for (i = 0; i < INQUIRY_LENGTH; i++) {
		rn = pRD->pR[Index].dpos[i];
		if (rn == 0) break;
		temp[rn] = 1;
		len++;
	}
	fnd = 0; //number of unique RNs
	for (i = 0; i < INQUIRY_LENGTH; i++) {
		if (temp[i] != 0) fnd++;
	}
	fnd = len - fnd; //number of dpos/RN repeates 

	if (fnd > 0) {
		score = score - fnd;
		if (score < fnd) score = fnd;
	}
	pRD->pR[Index].Score = score;


	if ((score == 0) || (Y.Score == 0)) {
		PER = 0;
	} else {
		PER = (int)(((float)score/(float)Y.Score) * 100.0);
	}
	if (PER > 100) PER = 100;

	//Reduction for being longer than necessary
	if ((PER == 100) && ((pRD->pR[Index].E - pRD->pR[Index].B + 1) > Ndb->pON[ONcode].Len)) PER = 90;

	return PER;
}

int GetQuality(NdbData *Ndb, RECdata *pRD, int Index) {
	//
	//	Get the recognition 'Quality' by comparing the length of Input Stream RNs
	//	to the ON's 'official' length. Higher values represent poorer recignition.
	//
	//----------

	int len;
	int ONcode;
	int q;

	//These are the positions in the Input Stream that recognize this ON
	for (len = 0; len < INQUIRY_LENGTH; len++) if (pRD->pR[Index].qpos[len] == 0) break;

	ONcode  = pRD->pR[Index].ONcode;
	q = Ndb->pON[ONcode].Len - len;
	if (q < 0) q = -q;

	return q;
}

float CompositeScore(RECdata *pRD, int Index) {
	//
	//	Create a combined-score from various measurements to help differentiate ONs
	//
	//	pRD->pR[Index].B = Begin Boundary
	//	pRD->pR[Index].E = End Boundary
	//	pRD->pR[Index].qpos = positions recognized in the Input Stream
 	//	pRD->pR[Index].dpos = positions recognized in the Output Neuron's Recognition List
	//	pRD->pR[Index].PER = %recognized
	//	pRD->pR[Index].QUAL = quality of recognition (smaller is better)
	//	
	//---------

	int i, p, fnd;
	int len;
	int qpos, dpos1, dpos2;
	int Arn, Ais; //anomaly counts
	float pr;
	float ln;
	float qu;
	float C;


	//Ais is the number of missing positions in the Bound Section from the Input Stream
	Ais = 0;
	for (p = pRD->pR[Index].B; p <= pRD->pR[Index].E; p++) { //Is position p in the qpos list?
		fnd = 0;
		for (i = 0; i < INQUIRY_LENGTH; i++) {
			qpos = pRD->pR[Index].qpos[i];
			if (qpos == 0) break; //end of list
			if (qpos != p) continue;
			fnd = 1;
			break;
		}
		if (fnd == 0) Ais++;
	}

	//Arn is the number of anomalies in the order of RNs from the ON's Recognition List
	Arn = 0;
	for (len = 0; len < INQUIRY_LENGTH; len++) if (pRD->pR[Index].dpos[len] == 0) break;

	for (i = 0; i < (len - 1); i++) {
		dpos1 = pRD->pR[Index].dpos[i];
		dpos2 = pRD->pR[Index].dpos[i+1];
		if ((dpos2 - dpos1) != 1) Arn = Arn + 2;
	}
	Arn = Arn / 3 ; //RN anomalies are not quite as bad as QPOS anomalies

	pr = (float)pRD->pR[Index].PER / 100.0;
	ln = (float)(pRD->pR[Index].E - pRD->pR[Index].B + 1); //length of this Bound Section
	qu = (float)(pRD->pR[Index].QUAL + 1); //QUAL = 0, 1, 2, ...

	C = (pr * pr * ln * ln) / qu;

	for (i = 0; i < Ais; i++) C = C / 10.0;
	for (i = 0; i < Arn; i++)  C = C / 10.0;

	return C;
}


