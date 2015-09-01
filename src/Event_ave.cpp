#include "Event_ave.h"

Event_ave::Event_ave(int len, shared_ptr<Digitizer> digitizer) : Event(len, digitizer) {
	if (g_verbose) cout << "Event_ave " << Event::siHowMany << " c'tor\n";
	try { dTrace = unique_ptr<double[]>(new double[iEventlength]);}
	catch (bad_alloc& ba) {iFailed |= (1 << alloc_error); return;}
}

Event_ave::~Event_ave() {
	if (g_verbose) cout << "Event_ave " << Event::siHowMany << " d'tor\n";
	dTrace.reset();
}

void Event_ave::SetAverage(int average) {
	iAverage = average;
	iEventlength -= iAverage;
	Event::siLength = iEventlength;
	dScale = 1./(iAverage ? 2.*iAverage+1. : 1);
}

void Event_ave::Analyze() {
	int i(0), j(0);
	for (i = iAverage; i < iEventlength-iAverage; i++) { // waveform averaging
		dTrace[i] = 0;
		for (j = -iAverage; j <= iAverage; j++) dTrace[i] += uspTrace[i+j]; // Jacques not confident of results between this and LAP
		dTrace[i] *= dScale;
	}
	dBaseline = 0;
	dBaseSigma = 0;
	dPeakY = 16000; // some arbitratily high number as -1 doesn't work for floats
	usPeakX = 0;
	dBasePkP = 0;
	dBasePkN = 16000;
	dPeakPos = 0;
	usTrigger = 0;
	dBasePost = 0;
	dBasePostSigma = 0;
	double dTemp(0);
	for (i = 0; i < iEventlength; i++) {
		dPeakPos = max(dPeakPos, dTrace[i]);
		if (i < iBaselength) {
			dBaseline += dTrace[i]; // baseline at start
			dBasePkP = max(dTrace[i],dBasePkP);
			dBasePkN = min(dTrace[i],dBasePkN);
			dBasePost += dTrace[iEventlength-iBaselength+i]; // baseline at end
		}
		if (dPeakY > dTrace[i]) { // peakfinder
			dPeakY = dTrace[i];
			usPeakX = i;
		}
		if ((usTrigger == 0) && (dTrace[i] < iThreshold)) usTrigger = i; // trigger
	}
	dBaseline *= dBaseScale;
	dBasePost *= dBaseScale;
	for (i = 0; i < iBaselength; i++) { // RMS baseline deviation
		dTemp = dTrace[i] - dBaseline;
		dBaseSigma += dTemp*dTemp;
		dTemp = dTrace[iEventlength-iBaselength+i] - dBasePost;
		dBasePostSigma += dTemp*dTemp;
	}
	dBaseSigma = sqrt(dBaseSigma*dBaseScale);
	dBasePostSigma = sqrt(dBasePostSigma*dBaseScale);
}