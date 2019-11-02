#include <stdint.h>
#include <stdlib.h>

#include "../inc/tm4c123gh6pm.h"
#include "../inc/PLL.h"
#include "../inc/Timer0A.h"
#include "Dac.h"
#include "Tracks.h"
#include "Switch.h"

#define SAMPLE_RATE 8000
#define MAX_TRACKS 16
#define MAX_TRACKS_POW2 4
#define BPM 226
#define WAVE_SIZE 32

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

const unsigned short Wave[32] = {  
  1024,1055,1250,1436,1605,1753,1873,1959,2010,2024,1998,
  1935,1838,1709,1553,1377,1188,993,798,612,443,295,
  175,89,38,24,50,113,210,339,495,671
};  

const unsigned short Guitar[64] = {  
  
1024,
1026,
1001,
935,
833,
730,
647,
619,
666,
782,
964,
1172,
1337,
1461,
1536,
1558,
1503,
1396,
1235,
1040,
864,
755,
758,
877,
1087,
1358,
1613,
1789,
1846,
1772,
1625,
1454,
1285,
1173,
1096,
1052,
1034,
1027,
1036,
1079,
1137,
1184,
1247,
1264,
1250,
1182,
1081,
974,
865,
801,
779,
810,
844,
859,
859,
851,
849,
875,
922,
977,
1024,
1024,
1024,
}; 

//this supports up to MAX_TRACKS tracks  through pre-allocated memory
//the tracks are made of a list of notes along with an envelope for the sound
struct TrackList{
	uint8_t trackNum;
	uint8_t enabled;
	const uint16_t *notes;
	const uint8_t *noteTiming;
	uint32_t noteIdx;
	uint32_t nlength;
	uint16_t envIdx;
	uint32_t lastTicks;
};

typedef struct TrackList TrackList_t;
TrackList_t tracks[MAX_TRACKS];

uint8_t numTracks = 0;
uint8_t playMusic = 0;

//const uint16_t Summertime0[16] = {E4, C4, E4, D4, C4, D4, E4, C4, A3, E3, 0, E4, C4, D4, D4, 0};
//const uint16_t Summertime[16] = {E5, C5, E5, D5, C5, D5, E5, C5, A4, E4, 0, E5, C5, D5, D5, 0};
//const uint16_t Summertime2[16] = {E6, C6, E6, D6, C6, D6, E6, C6, A5, E5, 0, E6, C6, D6, D6, 0};
//const uint8_t SummertimeT[16] = {2,  2,  10, 1,  1,  1,  1,  2,  4,  6,  2,  2,  2,  1,  7, 2};
const uint16_t top_chord[16] = {0,D5,0, Bb4,0, G5,0,Eb5,0,Eb5,0,D5,0,Eb5,0,Eb5};
const uint16_t middle_chord[16] = {0, G4, 0, G4, 0, C5, 0, C5, 0, G4, 0, Bb4, 0, G4, 0, Bb4};
const uint16_t bottom_chrod[12] = {C4, 0, C4, F4, 0, F4, C4, 0, C4, Bb3, 0, Bb3};

//uint16_t mid[4] = {489, 653, 518, 733};
//uint16_t treb[4] = {582, 777, 653, 872};
//uint16_t treb2[4] = {1744, 1306, 1554, 1164};
//uint8_t myTiming[4] = {4,4,4,4};
//uint8_t myTiming2[4] = {1,1,1,1};

const uint8_t top_timing[16] = {4, 1, 5, 6, 4, 1, 5, 6, 4, 1, 5, 6, 4, 1, 4, 8 };
const uint8_t mid_timing[16] = {2, 3, 2, 9, 2, 3, 2, 9, 2, 3, 2, 9, 2, 3, 5, 6 }; 
const uint8_t bottom_timing[12] = {5, 1, 10, 5, 1, 10, 5, 1, 10,5, 1, 10};

//uint16_t ssl[11] = {146<<2, 146<<2, 130<<2, 146<<2, 0<<2, 220<<2, 208<<2, 196<<2, 146<<2, 175<<2, 146<<2};
//uint8_t sslt[11] = {1, 1, 1, 1, 2, 2, 2, 1, 1, 2, 2};

void tracksInit(){
	for(uint8_t i = 0; i < MAX_TRACKS; i++){
		tracks[i].enabled = 0;
	}
	
	numTracks = 0;
	playMusic = 0;
}

void rewind(){
	
	for(uint8_t i = 0; i < MAX_TRACKS; i++){
		tracks[i].noteIdx = 0;
	}
}
	
uint8_t modulate = 0;

void mod(){
	modulate ^= 1;
}
	
void play(){
	playMusic = 1;
}

void pause(){
	playMusic = 0;
}

uint8_t addNewTrack(const uint16_t *notes, const uint8_t *noteTiming, uint32_t nlength){
	tracks[numTracks].trackNum = numTracks;
	tracks[numTracks].enabled = 0;
	tracks[numTracks].notes = notes;
	tracks[numTracks].noteTiming = noteTiming;
	tracks[numTracks].noteIdx = 0;
	tracks[numTracks].nlength = nlength;
	tracks[numTracks].lastTicks = 0;
	
	numTracks++;
	return (numTracks - 1);
}

void enableTrack(uint8_t track){
	tracks[track].enabled = 1;
}

void disableTrack(uint8_t track){
	tracks[track].enabled = 0;
}

uint16_t envelopeSound(uint8_t track, uint32_t elength,  uint16_t sample){
	//uint32_t shiftVal = (tracks[track].envIdx++ << 3) / elength;
	uint32_t shiftVal = ((elength - tracks[track].envIdx++) << 10) / elength;
	//uint16_t indexInBetween = shiftVal - 1000 * (shiftVal / 1000);
	return ((sample * shiftVal) >> 10 ); //- (sample/shiftVal >> shiftVal);
	//return sample >> shiftVal;
	
}

uint16_t getTrackData(uint8_t track, uint32_t ticks){
	
	uint16_t retVal;
	uint16_t envVal;
	
	if(tracks[track].enabled){
		uint32_t timeElasped = (ticks >= tracks[track].lastTicks) ? (ticks - tracks[track].lastTicks) : ((0xFFFFFFFF - tracks[track].lastTicks) + ticks);
		uint32_t timeForNote = (tracks[track].noteTiming[tracks[track].noteIdx] * 60 * SAMPLE_RATE) / BPM;
		//is the time elasped greated than the period*beats for the note?
		if(timeElasped >= timeForNote){
			tracks[track].noteIdx = (tracks[track].noteIdx + 1) % tracks[track].nlength;
			tracks[track].lastTicks = ticks;
			tracks[track].envIdx = 0;
		}
		
		if(tracks[track].notes[tracks[track].noteIdx] != 0){
			//is the time elasped greater than one note period?
			uint32_t samplesPerPeriod = modulate ? (2*SAMPLE_RATE / (tracks[track].notes[tracks[track].noteIdx])) : (SAMPLE_RATE / (tracks[track].notes[tracks[track].noteIdx]));
			uint16_t waveIdx = (((ticks + (tracks[track].trackNum << 1)) % samplesPerPeriod) << 5) / samplesPerPeriod;

			//retVal = Wave[waveIdx%32];	
			retVal = envelopeSound(track, timeForNote, Wave[waveIdx % 32]);
			//apply the envelope
			//retVal = (retVal * (tracks[track].envIdx++ % (2*SAMPLE_RATE))) / SAMPLE_RATE;
		} else {
			retVal = 0;
		}
	} else {
		retVal = 0;
	}
	
	return retVal;
}

uint16_t closestPowerOfTwo(uint16_t num){
	uint16_t i;
	for(i = 1; num >> i > 0; i++){}
	return i;
}

uint16_t getDacOutput(uint32_t ticks){
	uint32_t sum = 0;
	
	if(playMusic){
		for(uint8_t i = 0; i < numTracks; i++){
			sum += (tracks[i].enabled) ? getTrackData(i, ticks) : 0;
		}
	}
	
	return (uint16_t) (sum / numTracks);
}

uint32_t getplayMusic(){
	return playMusic;
}
void setplayMusic(uint32_t g){
	playMusic = g;
}



uint32_t timerTicks = 0;
uint16_t sample;
void handler(void){
	if(playMusic){
		for(uint8_t i = 0; i < numTracks; i++){
			sample = getTrackData(i, timerTicks);
			if(sample > 0xFFF)
				break;
			else
				DAC_Output(sample >> 3);
		}
	}
	//DAC_Output(getDacOutput(timerTicks));
	//DAC_Output(Wave[timerTicks % 32]);
	timerTicks++;
}

int main(void){
	DisableInterrupts();
	PLL_Init(Bus80MHz);
	Timer0A_Init(&handler, 80000000/SAMPLE_RATE);
	DAC_Init(0);
	//Timer0A_Init(&handler, 80000000/(32*440));
	tracksInit();
	//enableTrack(addNewTrack(ssl, sslt, 11));
	//enableTrack(addNewTrack(mid, myTiming, 4));
	//enableTrack(addNewTrack(treb, myTiming, 4));
	//enableTrack(addNewTrack(treb2, myTiming2, 4));
	//enableTrack(addNewTrack(top_chord, top_timing, 16));
	//enableTrack(addNewTrack(middle_chord, mid_timing, 16));
	//enableTrack(addNewTrack(middle_chord, mid_timing, 16));
	//enableTrack(addNewTrack(middle_chord, mid_timing, 16));
	//enableTrack(addNewTrack(middle_chord, mid_timing, 16));
	//enableTrack(addNewTrack(bottom_chrod, bottom_timing, 12));
	//enableTrack(addNewTrack(Summertime, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime0, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime2, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime2, SummertimeT, 16));
	//enableTrack(addNewTrack(Summertime2, SummertimeT, 16));
	play();
	Switch_Init();
	
	EnableInterrupts();
	
	while(1){
		WaitForInterrupt();
	}
}