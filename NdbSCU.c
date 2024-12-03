//The Neural Database - Simple Competitive Unit (SCU)
//(c) Copyright 2024 Gary J. Lassiter. All Rights Reserved.

#include <ndb.h>


//Functions:
int RunSCU(NdbData *, RECdata *, COMP *, COMP *);
void Compete(NdbData *, RECdata *, COMP *, COMP *);
void GetCompetitionData(NdbData *, RECdata *, COMP *);
void SpaceB(COMP *, COMP *);
void Anomaly(COMP *, COMP *);
void Rec(COMP *, COMP *);
void MinPR(COMP *, COMP *);
void Bound(COMP *, COMP *);
void UnCount(COMP *, COMP *);
void MisLead(COMP *, COMP *);
int SetScore(NdbData *, RECdata *, COMP *, int);
int ExcitatorySpike(int);
int InhibitorySpike(int);


int RunSCU(NdbData *Ndb, RECdata *pRD, COMP *pA, COMP *pZ) {
	//
	//	Compete pA[] vs pZ[]
	//
	//	One of these won't exist if this is a call just to fill in the competition data.
	//
	//----------

	int i;
	int qpos;
	int aB, aE, zB, zE;
	int BEGIN, END;

	//From the two competitors, get the full qpos scanning range for INPUT[]
	BEGIN = (INQUIRY_LENGTH + 1);
	END = -1;

	aB = (INQUIRY_LENGTH + 1);
	aE = -1;
	if (pA->Mcount > 0) {
		for (i = 0; i < pA->Mcount; i++) {
			if (pA->m[i].B < aB) aB = pA->m[i].B;
			if (pA->m[i].E > aE) aE = pA->m[i].E;
		}
	}

	zB = (INQUIRY_LENGTH + 1);
	zE = -1;
	if (pZ->Mcount > 0) {
		for (i = 0; i < pZ->Mcount; i++) {
			if (pZ->m[i].B < zB) zB = pZ->m[i].B;
			if (pZ->m[i].E > zE) zE = pZ->m[i].E;
		}
	}

	if ((aE == -1)&&(zE == -1)) return 0; //Nothing to compete

	if (aE == -1) { //only one competitor in Z
		BEGIN = zB;
		END = zE;
	} else {
		if (zE == -1) { //only one competitor in A
			BEGIN = aB;
			END = aE;
		} else {
			if ((aB>zE)||(zB>aE)) return 0; //Out of range
			BEGIN = aB;
			if (zB < aB) BEGIN = zB;
			END = aE;
			if (zE > aE) END = zE;
		}
	}

	//Get the Stand-Alone scores
	for (qpos = BEGIN; qpos <= END; qpos++) {
		if (pA->Mcount > 0) pA->Score = SetScore(Ndb, pRD, pA, qpos);
		if (pZ->Mcount > 0) pZ->Score = SetScore(Ndb, pRD, pZ, qpos);
	}

	//It takes 2 to compete
	if ((pA->Mcount > 0) && (pZ->Mcount > 0)) Compete(Ndb, pRD, pA, pZ);

	return -1;
}

void Compete(NdbData *Ndb, RECdata *pRD, COMP *pA, COMP *pZ) {
	//
	//	Calculate the SCU scores for 2 competitors: player "A" vs. player "Z"
	//	Both players begin the contest with their Stand-Alone Scores - generated in SetScore().
	//	A player may be a single ON or multiple ONs with aligned Begin/End boundaries.
	//
	//	The competitive 'agents' for each player are:
	//
	//	1) SpaceB = number of ON boundaries that begin with a space
	//
	//	2) Anomaly = the total out-of-order RNs
	//
	//	3) Rec = Recognition Strength: total (%rec-cntA-qual)*len of the entire branch
	//
	//	4) MinPR = the minimum %rec-qual of any ON in the branch, i.e. its weakest ON
	//
	//	5) Bound = the number of ON boundaries that do not begin with a space
	//
	//	6) UnCount = the number of RNs unaccounted for in the Input Stream
	//
	//	7) MisLead = the number of RNs missing from the beginning of each ON in the branch
	//
	//----------

	int i;

	GetCompetitionData(Ndb, pRD, pA);
	GetCompetitionData(Ndb, pRD, pZ);

	//Remove weaker SpaceBoundary 'credit' if there is an ON conflict.
	for (i = 2; i < INQUIRY_LENGTH; i++) {
		if ((pA->SpaceClaim[i] > 0)||(pZ->SpaceClaim[i] > 0)) {
			//
			//Who has the stronger claim on this space?
			if (pA->SpaceClaim[i] > pZ->SpaceClaim[i]) {
				pZ->spaceB = pZ->spaceB - 1;
			}
			if (pZ->SpaceClaim[i] > pA->SpaceClaim[i]) {
				pA->spaceB = pA->spaceB - 1;
			}
		}
	}

	// Modify the Stand-Alone scores by running the SCU agents
 	if (SCUswitch.SpaceB == 1) SpaceB(pA, pZ);
	if (SCUswitch.Anomaly == 1) Anomaly(pA, pZ);
	if (SCUswitch.Rec == 1) Rec(pA, pZ);
	if (SCUswitch.MinPR == 1) MinPR(pA, pZ);
	if (SCUswitch.Bound == 1) Bound(pA, pZ);
	if (SCUswitch.UnCount == 1) UnCount(pA, pZ);
	if (SCUswitch.MisLead == 1) MisLead(pA, pZ);
}

void SpaceB(COMP *pA, COMP *pZ) {
	//
	//	How many boundaries are supported with a Space?
	//
	//----------

	if (pA->spaceB > pZ->spaceB) {

		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);

	} else {
		if (pZ->spaceB > pA->spaceB) {

			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
		}
	}
}

void Anomaly(COMP *pA, COMP *pZ) {
	//
	//	Anomalies, out-of-order RNs
	//
	//----------

	if (pA->anomaly > pZ->anomaly) {

		pZ->Score = ExcitatorySpike(pZ->Score);
		pZ->Score = ExcitatorySpike(pZ->Score);

		pA->Score = InhibitorySpike(pA->Score);
		pA->Score = InhibitorySpike(pA->Score);
		pA->Score = InhibitorySpike(pA->Score);
		pA->Score = InhibitorySpike(pA->Score);

	} else {
		if (pZ->anomaly > pA->anomaly) {

			pA->Score = ExcitatorySpike(pA->Score);
			pA->Score = ExcitatorySpike(pA->Score);

			pZ->Score = InhibitorySpike(pZ->Score);
			pZ->Score = InhibitorySpike(pZ->Score);
			pZ->Score = InhibitorySpike(pZ->Score);
			pZ->Score = InhibitorySpike(pZ->Score);
		}
	}
}

void Rec(COMP *pA, COMP *pZ) {
	//
	//	Who has the highest cumulative (%rec-cntA-qual)*len...
	//
	//----------

	if (pA->rec > pZ->rec) {

		pA->Score = ExcitatorySpike(pA->Score);
		pA->Score = ExcitatorySpike(pA->Score);

		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);

	} else {
		if (pZ->rec > pA->rec) {

			pZ->Score = ExcitatorySpike(pZ->Score);
			pZ->Score = ExcitatorySpike(pZ->Score);

			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
		}
 	}
}

void MinPR(COMP *pA, COMP *pZ) {
	//
	//	Who has the lowest %rec over any length of RNs...
	//
	//----------

	if (pA->minpr > pZ->minpr) {

		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);
		pZ->Score = InhibitorySpike(pZ->Score);

	} else {
		if (pZ->minpr > pA->minpr) {

			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
		}
	}
}

void Bound(COMP *pA, COMP *pZ) {
	//
	//	Demerits for having the most boundaries unsupported by spaces
	//
	//----------

	int i;
	int cnt;

	if (pA->bound > pZ->bound) {

		if (pZ->anomaly <= pA->anomaly) {

			cnt = pA->bound - pZ->bound;
			cnt++;
			for (i = 0; i < cnt; i++) {

				pA->Score = InhibitorySpike(pA->Score);

				pZ->Score = ExcitatorySpike(pZ->Score);
			}
		}
	} else {
		if (pZ->bound > pA->bound) {

			if (pA->anomaly <= pZ->anomaly) {

				cnt = pZ->bound - pA->bound;
				cnt++;
				for (i = 0; i < cnt; i++) {

					pZ->Score = InhibitorySpike(pZ->Score);

					pA->Score = ExcitatorySpike(pA->Score);
				}
			}
		}
	}
}

void UnCount(COMP *pA, COMP *pZ) {
	//
	//	Are there any RNs in the Input Stream that are unaccounted for?
	//
	//----------

	int i;
	int cnt;

	if (pA->uncount > pZ->uncount) {
		cnt = pA->uncount - pZ->uncount;
		for (i = 0; i < cnt; i++) {

			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);

			pZ->Score = ExcitatorySpike(pZ->Score);
			pZ->Score = ExcitatorySpike(pZ->Score);
			pZ->Score = ExcitatorySpike(pZ->Score);
		}
	} else {
		if (pZ->uncount > pA->uncount) {
			cnt = pZ->uncount - pA->uncount;
			for (i = 0; i < cnt; i++) {

				pZ->Score = InhibitorySpike(pZ->Score);
				pZ->Score = InhibitorySpike(pZ->Score);

				pA->Score = ExcitatorySpike(pA->Score);
				pA->Score = ExcitatorySpike(pA->Score);
				pA->Score = ExcitatorySpike(pA->Score);
			}
		}
	}
}

void MisLead(COMP *pA, COMP *pZ) {
	//
	//	Who has the most missing, leading RNs in a competitor's list of ONs?
	//
	//----------

	int i;
	int cnt;

	if (pA->mislead > pZ->mislead) {
		cnt = pA->mislead - pZ->mislead;
		for (i = 0; i < cnt; i++) {

			pA->Score = InhibitorySpike(pA->Score);
			pA->Score = InhibitorySpike(pA->Score);
		}
		if (cnt > 1) {

			pZ->Score = ExcitatorySpike(pZ->Score);
			pZ->Score = ExcitatorySpike(pZ->Score);
		}
 	} else {
		if (pZ->mislead > pA->mislead) {
			cnt = pZ->mislead - pA->mislead;
			for (i = 0; i < cnt; i++) {

				pZ->Score = InhibitorySpike(pZ->Score);
				pZ->Score = InhibitorySpike(pZ->Score);
			}
			if (cnt > 1) {

				pA->Score = ExcitatorySpike(pA->Score);
				pA->Score = ExcitatorySpike(pA->Score);
			}
		}
	}
}

void GetCompetitionData(NdbData *Ndb, RECdata *pRD, COMP *player) {

	int i, j, x;
	int ONcode;
	int ONlen;
	int len;
	int PER;
	int QUAL;
	int cntA;
	int B;

	player->spaceB = 0;
	player->anomaly = 0;
	player->rec = 0;
	player->minpr = 999999;
	player->bound = 0;
	player->mislead = 0;

	for (i = 0; i < INQUIRY_LENGTH; i++) player->SpaceClaim[i] = 0;

	//Total number of boundary crossings in this branch
	for (i = 0; i < player->Mcount; i++) player->bound++; 
	player->bound--;

	for (i = 0; i < player->Mcount; i++) {

		ONcode = player->m[i].ONcode;
		ONlen = Ndb->pON[ONcode].Len;

		//Get this ON's dataset
		len = player->m[i].nrec; //number of RN hits (length of RNs in m[i].order)
		PER = player->m[i].PER; //%recognition
		QUAL = player->m[i].QUAL; //quality
		cntA = player->m[i].cntA; //positional anomalies
		B = player->m[i].B;

		player->anomaly = (cntA + QUAL) + player->anomaly;

		x = (PER - cntA - QUAL) * len; //NOTE: units of measure do not match!
		player->rec = x + player->rec;

		if (PER < player->minpr) player->minpr = PER;

		//If there is a space at the Begin Boundary, the Score should be enhanced
		if (B > 1) {
			if (pRD->Space[B] > 0) { //The list of spaces is defined in the Preprocessor()
				player->spaceB = 1 + player->spaceB;
				player->SpaceClaim[B] = PER; //the strength of this claim on this space
				player->bound--; //No boundary demerits for a boundary crossing covered by a space
			}
		}

		//Count the number of RNs missing from the beginning of each ON in the branch...
		for (j = 0; j < ONlen; j++) {
			if (player->m[i].RLhit[j] > 0) break;
			player->mislead = 1 + player->mislead;
		}
	}
}

int SetScore(NdbData *Ndb, RECdata *pRD, COMP *player, int qpos) {
	//
	//	Determine a competitor's Stand-Alone score
	//
	//	The competitor may be made of multiple ONs in which their respective Begin
	//	and End Boundaries align.
	//
	//	The Stand-Alone score depends on RN "hits" in the ON's Recognition List by
	//	RNs from various qpos locations identified by the data in the Input Stream.
	//	
	//	The "power" of any such "hit" may include an enhancement based on the orderliness
	//	of the location in the Recognition List.
	//	
	//	If there are multiple "hit" locations to choose from, such as the letter "I" in
	//	"MISSISSIPPI", the location which gives the highest orderliness enhancement is
	//	selected.
	//
	//	Other factors effecting an ON's score, such as the appearance of spaces, do not
	//	influence the Stand-Alone Score.
	//	
	//----------

	int score;
	int rn, dpos;
	int i, j, k;
	int hit;
	int ONcode;
	int len;
	int nrec;
	int en;
	HitList HL[INQUIRY_LENGTH+1];
	int E[INQUIRY_LENGTH+1];

	score = player->Score; //The Score accumulation up to this qpos
	rn = pRD->ISRN[qpos - 1]; //get the RN from the Input Stream, ISRN[0] = qpos #1

	if (rn < 1) return 0;

	hit = 0; //Is there an RN 'hit' at this qpos?

	for (i = 0; i < player->Mcount; i++) { //check each ON in this competitor

		//Out of range?
		if (qpos < player->m[i].B) continue;
		if (qpos > player->m[i].E) continue;

		//If there is an RN Hit, the goal is to place it wherever the enhancement will be
		//highest, even if doing so replaces an existing Hit. If there is an existing Hit,
		//the greatest positional difference determines the highest enhancement.
		ONcode = player->m[i].ONcode;
		len = Ndb->pON[ONcode].Len;

		//Load the ON's Recognition List into the Hit List and initialize it
		for (j = 0; j < INQUIRY_LENGTH; j++) {
			HL[j].RNcode = 0;
			E[j] = 0; //Enhancement: E[dpos]=en
		}
		
		for (j = 0; j <= len; j++) HL[j].RNcode = Ndb->pON[ONcode].RL[j];

		nrec = player->m[i].nrec; // number of Hits recorded in the 'order' array: ->m[i].order[]

		for (dpos = 1; dpos <= len; dpos++) {

			if (rn != HL[dpos-1].RNcode) continue; // This RN is not in the Recognition List
			
			//RN Hit at dpos
			en = 0; //enhancement
			if (nrec > 0) {
				//Calculate orderliness enhancement from data already recognized
				j = dpos - player->m[i].order[nrec].dpos; //the difference with the most recent dpos Hit
				if (j == 1) {
					for (k = nrec; k > 0; k--) {
						if (player->m[i].order[k].dpos == 0) break;
						en++;
					}
				} else {
					if (j < 1) en = j - 1;
				}
 			}
			E[dpos] = en + 10000; //separate these values from zero
		}

		//Pick the dpos with the highest enhancement - there may be none.
		dpos = 0;
		en = 0;
		for (j = 1; j <= len; j++) {
			if (E[j] > 0) {
				E[j] = E[j] - 10000;
				if (dpos == 0) {
					dpos = j;
					en = E[j];
				} else {
					if (E[j] > en ) {
						en = E[j];
						dpos = j;
					}
				}
			}
		}

		if (dpos > 0) {
			player->m[i].RLhit[dpos-1] = 1; //list goes from 0, 1, 2, ...
			nrec++;
			player->m[i].nrec = nrec; // number of Hits recorded in the 'order' array: ->m[i].order[]
			player->m[i].order[nrec].dpos = dpos;
			player->m[i].order[nrec].qpos = qpos;
			player->m[i].order[nrec].rn = rn;

			if (en >= 0) {
				for (j = 0; j < (en+1); j++) score = ExcitatorySpike(score);
			} else {
				for (j = en; j < 0; j++) score = InhibitorySpike(score);
			}
			
			hit = 1;
			break;
		}
	}

	if (hit == 0) player->uncount = player->uncount + 1;

	return score;
}

int ExcitatorySpike(int score) {
	//
	//	Fast rise from low scores followed by asymptotic approach to 100
	//
	//----------
	
	float s;

	s = (float)score;
	s = (s * 0.9011) + 9.89;
	s = s + .5;
	score = (int)s;

	return score;
}

int InhibitorySpike(int score) {
	//
	//	Fast fall from high scores followed by asymptotic approach to 0
	//
	//----------

	float s;

	s = (float)score;
	s = (s * 0.9011);
	s = s + .5;
	score = (int)s;
	
	return score;
}

