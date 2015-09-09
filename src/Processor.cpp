#include "Processor.h"
#include <ctime>
#include <pthread.h>
#include <ratio>
#include <chrono>

using namespace std::chrono;

Processor::Processor() {
	if (g_verbose) cout << "Processor c'tor\n";
	iSpecial = -1;
	iAverage = 0;

	sConfigFileName = "\0";
	sRawDataFile = "\0";
	sRootFile = "\0";
	cSource[0] = '\0';

	memset(&digitizer, 0, sizeof(digitizer));

	usMask = 0;

	memset(iChan,			0, sizeof(iChan));
	iEventlength = 0;
	iEventsize = 0;
	iFailed = 0;
	memset(iFastTime,		0, sizeof(iFastTime));
	iLevel = 0;
	iNchan = 0;
	iNumEvents = 0;
	memset(iPGASamples,		0, sizeof(iPGASamples));
	memset(iSlowTime,		0, sizeof(iSlowTime));
	iTrigPost = 0;
	iXSQ_ndf = 0;

	memset(uiDCOffset,		0, sizeof(uiDCOffset));
	memset(uiThreshold,		0, sizeof(uiThreshold));

	memset(fGain,			0, sizeof(fGain));
	memset(fDetectorZ,		0, sizeof(fDetectorZ));
	memset(fDetectorR,		0, sizeof(fDetectorR));

	// arrays for trees
	memset(bFullWave,		0, sizeof(bFullWave));
	memset(bSaturated,		0, sizeof(bSaturated));
	memset(bPileUp,			0, sizeof(bPileUp));
	memset(sDecay,			0, sizeof(sDecay));
	memset(sRise,			0, sizeof(sRise));
	memset(sPeakX,			0, sizeof(sPeakX));
	memset(sPeakXs,			0, sizeof(sPeakXs));
	memset(sTrigger,		0, sizeof(sTrigger));
	memset(sPeaks,			0, sizeof(sPeaks));
	memset(dBase,			0, sizeof(dBase));
	memset(dSigma,			0, sizeof(dSigma));
	memset(dBaseP,			0, sizeof(dBaseP));
	memset(dBasePS,			0, sizeof(dBasePS));
	memset(dPeak0,			0, sizeof(dPeak0));
	memset(dPeak0s,			0, sizeof(dPeak0s));
	memset(dFullInt,		0, sizeof(dFullInt));
	memset(dBasePeakP,		0, sizeof(dBasePeakP));
	memset(dBasePeakN,		0, sizeof(dBasePeakN));
	memset(bTruncated,		0, sizeof(bTruncated));
	memset(dBaseline,		0, sizeof(dBaseline));
	memset(dBaseSigma,		0, sizeof(dBaseSigma));
	memset(dBasePost,		0, sizeof(dBasePost));
	memset(dBasePostSigma,	0, sizeof(dBasePostSigma));
	memset(dFastInt,		0, sizeof(dFastInt));
	memset(dSlowInt,		0, sizeof(dSlowInt));
	memset(dPeak1,			0, sizeof(dPeak1));
	memset(dPeak2,			0, sizeof(dPeak2));
	memset(sdSample,		0, sizeof(sdSample));
	memset(dOdd,			0, sizeof(dOdd));
	memset(dEven,			0, sizeof(dEven));
	memset(dLaplaceLow,		0, sizeof(dLaplaceLow));
	memset(dLaplaceHigh,	0, sizeof(dLaplaceHigh));
	memset(dXsq,			0, sizeof(dXsq));
	memset(dPeak_scale,		0, sizeof(dPeak_scale));
	memset(dBase_shift,		0, sizeof(dBase_shift));
	memset(dOffset,			0, sizeof(dOffset));
	memset(dPeak_err,		0, sizeof(dPeak_err));
	memset(dBase_err,		0, sizeof(dBase_err));
	memset(dOff_err,		0, sizeof(dOff_err));
}

Processor::~Processor() {
	if (g_verbose) cout << "Processor d'tor\n";
	for (auto ch = 0; ch < MAX_CH; ch++) {
		dTrace[ch] = nullptr;
		event[ch] = nullptr;
		method[ch] = nullptr;
		discriminator[ch] = nullptr;
	}
	f = nullptr;
	buffer = nullptr;
	if (fin.is_open()) fin.close();
}

void Processor::BusinessTime() {
	int ch(0), ev(0), iProgCheck(0), iRate(0), iTimeleft(0), iLivetime(0);

	unsigned long* ulpTimestamp = (unsigned long*)(buffer.get() + sizeof(long));
	unsigned long ulTSFirst(0), ulTSLast(0), ulTSPrev(0);

	TS->Branch("Timestamp", &ulpTimestamp[0], "time_stamp/l");
	TS->Branch("Timestamp_prev", &ulTSPrev, "time_stamp_prev/l");

	steady_clock::time_point t_this, t_that;
	duration<double> t_elapsed;
	iProgCheck = max(iNumEvents/100 + 1, 10000); // too many print statements slows the process, every 1% or 10000 events

	t_that = steady_clock::now();
	cout << "Processing:\n";
	cout << "Completed\tRate (ev/s)\tTime left\n";
	fin.seekg(sizeof_f_header, fin.beg);
	f->cd();
	for (ev = 0; ev < iNumEvents; ev++) {
		fin.read(buffer.get(), iEventsize);
		for (ch = 0; ch < iNchan; ch++) {
			event[ch]->Analyze();
			if (iLevel > 0) {
				method[ch]->Analyze();
				if (iLevel > 1) discriminator[ch]->Discriminate();
			}
		}
		TS->Fill();
		T0->Fill();
		if (iLevel > 0) {
			T1->Fill();
			if (iLevel > 1) Discriminator::Cuts_fill();
		}
		ulTSPrev = ulpTimestamp[0];
		if (ev % iProgCheck == iProgCheck-1) { // progress updates
			cout << ev*100l/iNumEvents << "%\t\t";
			t_this = steady_clock::now();
			t_elapsed = duration_cast<duration<double>>(t_this-t_that);
			t_that = steady_clock::now();
			iRate = t_elapsed.count() == 0 ? 9001 : iProgCheck/t_elapsed.count(); // it's OVER 9000!
			cout << iRate << "\t\t";
			iTimeleft = (iNumEvents - ev)/iRate;
			if (iTimeleft > (1 << 12)) cout << iTimeleft/3600 << "h" << (iTimeleft%3600)/60 << "m\n";
			else if (iTimeleft > (1 << 7)) cout << iTimeleft/60 << "m" << iTimeleft%60 << "s\n";
			else cout << iTimeleft << "s\n";
		}
		if (ev == 0) ulTSFirst = ulpTimestamp[0];
	}
	cout << "Processing completed\n";
	T0->AddFriend("TS");
	if (iLevel > 0) {
		T1->AddFriend("TS");
		T1->AddFriend("T0");
		T0->AddFriend("T1");
		if (iLevel > 1) {
			T0->AddFriend("T2");
			T1->AddFriend("T2");
			Discriminator::FriendshipIsMagic();
			Discriminator::Cuts_write();
		}
		T1->Write();
	}
	TS->Write("",TObject::kOverwrite);
	T0->Write("",TObject::kOverwrite);
	ulTSLast = ulpTimestamp[0];
	iLivetime = (ulTSLast - ulTSFirst)/125e6;
	cout << "Acquisition livetime: " << iLivetime << "s\nBeginning cleanup: ";
	fin.close();
	buffer.reset();
	Discriminator::Cuts_deinit();
	TS.reset();
	T0.reset();
	T1.reset();
	f->Close();
	f.reset();
	cout << " done\n";
	return;
}

vector<void*> Processor::SetAddresses(int ch, int level) {
	vector<void*> add(50);
	int i(0);
	if (level == 0) {
		add[i++] = (void*)&bFullWave[ch];
		add[i++] = (void*)&bSaturated[ch];
		add[i++] = (void*)&bPileUp[ch];

		add[i++] = (void*)&sDecay[ch];
		add[i++] = (void*)&sRise[ch];
		add[i++] = (void*)&sPeakX[ch];
		add[i++] = (void*)&sPeakXs[ch];
		add[i++] = (void*)&sTrigger[ch];
		add[i++] = (void*)&sPeaks[ch];

		add[i++] = (void*)&dBase[ch];
		add[i++] = (void*)&dSigma[ch];
		add[i++] = (void*)&dBaseP[ch];
		add[i++] = (void*)&dBasePS[ch];
		add[i++] = (void*)&dPeak0[ch];
		add[i++] = (void*)&dPeak0s[ch];
		add[i++] = (void*)&dFullInt[ch];
		add[i++] = (void*)&dBasePeakP[ch];
		add[i++] = (void*)&dBasePeakN[ch];
	} else if (level == 1) {
		add[i++] = (void*)&bTruncated[ch];

		add[i++] = (void*)&dBaseline[ch];
		add[i++] = (void*)&dBaseSigma[ch];
		add[i++] = (void*)&dBasePost[ch];
		add[i++] = (void*)&dBasePostSigma[ch];
		add[i++] = (void*)&dFastInt[ch];
		add[i++] = (void*)&dSlowInt[ch];

		add[i++] = (void*)&dPeak1[ch];
		add[i++] = (void*)&dPeak2[ch];

		add[i++] = (void*)&sdSample[ch];

		add[i++] = (void*)&dOdd[ch];
		add[i++] = (void*)&dEven[ch];

		add[i++] = (void*)&dLaplaceLow[ch];
		add[i++] = (void*)&dLaplaceHigh[ch];

		add[i++] = (void*)&dXsq[0][ch];
		add[i++] = (void*)&dXsq[1][ch];
		add[i++] = (void*)&dPeak_scale[0][ch];
		add[i++] = (void*)&dPeak_scale[1][ch];
		add[i++] = (void*)&dBase_shift[0][ch];
		add[i++] = (void*)&dBase_shift[1][ch];
		add[i++] = (void*)&dOffset[0][ch];
		add[i++] = (void*)&dOffset[1][ch];

		add[i++] = (void*)&dPeak_err[0][ch];
		add[i++] = (void*)&dPeak_err[1][ch];
		add[i++] = (void*)&dBase_err[0][ch];
		add[i++] = (void*)&dBase_err[1][ch];
		add[i++] = (void*)&dOff_err[0][ch];
		add[i++] = (void*)&dOff_err[1][ch];
	} else if (level == 2) {
		add[i++] = (void*)&dFastInt[ch];
		add[i++] = (void*)&dSlowInt[ch];
		add[i++] = (void*)&sdSample[ch];
		add[i++] = (void*)&dPeak2[ch];
		add[i++] = (void*)&dXsq[0][ch];
		add[i++] = (void*)&dXsq[1][ch];
		add[i++] = (void*)&dPeak_scale[0][ch];
		add[i++] = (void*)&dPeak_scale[1][ch];
		add[i++] = (void*)&dBase_shift[0][ch];
		add[i++] = (void*)&dBase_shift[1][ch];
		add[i++] = (void*)&dOdd[ch];
		add[i++] = (void*)&dEven[ch];
		add[i++] = (void*)&dLaplaceLow[ch];
		add[i++] = (void*)&dLaplaceHigh[ch];
		add[i++] = (void*)&dFullInt[ch];
	}
	return add;
}

void Processor::SetDetectorPositions(string in) { // "z0=#,z1=#,z2=#,r0=#,r1=#,r2=#" or some permutation
	if (in == "\0") {
		bPositionsSet = false;
		return;
	}
	if (g_verbose) cout << "Setting detector positions\n";
	unsigned int i(0), iCommas[2] = {0,1};
	string glob; // for dealing with each each substring
	for (i = 0; i < 6; i++) {
		iCommas[1] = in.find(',',iCommas[0]);
		glob = in.substr(iCommas[0],iCommas[1]-iCommas[0]); // grabs input between commas
		if ((glob[0] == 'z') || (glob[0] == 'Z')) fDetectorZ[atoi(glob.c_str()+1)] = atof(glob.c_str()+glob.find('=')+1);
		if ((glob[0] == 'r') || (glob[0] == 'R')) fDetectorR[atoi(glob.c_str()+1)] = atof(glob.c_str()+glob.find('=')+1);
		iCommas[0] = iCommas[1]+1; // shift forward to next substring
		if (iCommas[1] >= in.length()) break;
	}
	bPositionsSet = true;
}

void Processor::Setup(string in) { // also opens raw and processed files
	char cBuffer[sizeof_f_header];
	long filesize;
	int ch(-1);

	if (g_verbose) cout << "Opening files\n";
	sRawDataFile = sWorkingDir + "/rawdata/" + in + ".dat";
	sRootFile = sWorkingDir + "/prodata/" + in;
	if ((iSpecial == -1) && (iAverage == 0)) sRootFile += ".root";
	else if ((iSpecial == -1) && (iAverage != 0)) sRootFile += "_a.root";
	else if ((iSpecial != -1) && (iAverage == 0)) sRootFile += "_s.root";
	else sRootFile += "_as.root";
	fin.open(sRawDataFile.c_str(), ios::in | ios::binary);
	if (!fin.is_open()) {
		cout << "Error: " << sRawDataFile << " not found\n";
		cout << error_message[file_error];
		throw ProcessorException();
	}
	f = unique_ptr<TFile>(new TFile(sRootFile.c_str(), "RECREATE"));
	if (!f->IsOpen()) {
		cout << "Error: could not open " << sRootFile << '\n';
		cout << error_message[file_error];
		throw ProcessorException();
	}

	if (g_verbose) cout << "Parsing file header\n";

	fin.seekg(0, fin.end);
	filesize = fin.tellg();
	fin.seekg(0, fin.beg);
	fin.read(cBuffer, sizeof_f_header);

	strncpy(digitizer.cName, cBuffer, sizeof(digitizer.cName)); // digitizer name
	memcpy(&usMask, cBuffer + 12, sizeof(usMask)); // channel mask
	memcpy(&iEventlength, cBuffer + 14, sizeof(iEventlength)); // Eventlength
	memcpy(&iTrigPost, cBuffer + 18, sizeof(iTrigPost)); // post-trigger
	memcpy(uiDCOffset, cBuffer + 22, sizeof(uiDCOffset)); // dc offsets
	memcpy(uiThreshold, cBuffer + 54, sizeof(uiThreshold)); // trigger thresholds
//	cout << digitizer.cName << " " << usMask << " " << iEventlength << " " << iTrigPost << " " << '\n';
	if (strcmp(digitizer.cName, "DT5730") == 0) {
		digitizer.dSamplerate = 5E8;
		digitizer.sResolution = (1 << 14);
		if (iSpecial > 0) digitizer.sResolution = (1 << (14 - iSpecial));
		digitizer.dVpp = 2;
		digitizer.iBaselength = 20;
		iFailed = 0;
		digitizer.id = dt5730;
	} else if (strcmp(digitizer.cName, "DT5751") == 0) {
		digitizer.dSamplerate = (iSpecial == 0) ? 5E8 : 1E9;
		digitizer.sResolution = (1 << 10);
		digitizer.dVpp = 1;
		digitizer.iBaselength = (iSpecial == 0) ? 20 : 40;
		iFailed = 0;
		digitizer.id = dt5751;
	} else if (strcmp(digitizer.cName, "DT5751DES") == 0) {
		digitizer.dSamplerate = 2E9;
		digitizer.sResolution = (1 << 10);
		digitizer.dVpp = 1;
		digitizer.iBaselength = 80;
		iFailed = 0;
		digitizer.id = dt5751des;
	} else if (strcmp(digitizer.cName, "V1724") == 0) {
		digitizer.dSamplerate = 1E8;
		digitizer.sResolution = (1 << 14);
		digitizer.dVpp = 2.25;
		digitizer.iBaselength = 15;
		iFailed = 0;
		digitizer.id = v1724;
	} else {
		digitizer.dSamplerate = -1;
		digitizer.sResolution = -1;
		digitizer.dVpp = -1;
		digitizer.iBaselength = 1;
		cout << error_message[dig_error];
		digitizer.id = invalid_dig;
		throw ProcessorException();
	}

	digitizer.dScaleV = digitizer.dVpp/(double)digitizer.sResolution; // volts/bin
	digitizer.dScaleT = 1E9/digitizer.dSamplerate; // ns/Sa

	iNchan = 0;
	for (auto i = 0; i < MAX_CH; i++) if (usMask & (1<<i)) iChan[iNchan++] = i;
	iEventsize = sizeof_ev_header + iNchan*iEventlength*sizeof(short); // each sample is size 2
	iNumEvents = (filesize - sizeof_f_header)/iEventsize;

	if (g_verbose) cout << "Parsing config file\n";
	string sFilename = sWorkingDir + "/Data_Processor/config/" + sConfigFileName;
	ifstream fconf(sFilename.c_str(),ios::in);
	if (!fconf.is_open()) {
		cout << "Config file " << sFilename << " not found\n";
		cout << error_message[file_error];
		throw ProcessorException();
	} else if (g_verbose) cout << "Opened " << sFilename << '\n';

	cBuffer[0] = {'\0'};
	while (!fconf.eof()) {
		fconf.getline(cBuffer, 64, '\n');
		if (cBuffer[0] == '#') continue;
		if (strstr(cBuffer, "LEVEL") != 0) iLevel = atoi(cBuffer + 6);
		if (strcmp(cBuffer + 10, digitizer.cName) == 0) {
			fconf.getline(cBuffer, 64, '\n');
			while (strstr(cBuffer, "END") == NULL) {
				if (strstr(cBuffer, "CHANNEL") != NULL) { // loads processing parameters
					ch = atoi(&cBuffer[8]);
					if ((ch >= MAX_CH) || (ch < 0)) {cout << error_message[config_file_error]; throw ProcessorException();}
					fconf.getline(cBuffer, 64, '\n');
					sscanf(cBuffer, "SLOW %i FAST %i PGA %i GAIN_N %f GAIN_Y %f", &iSlowTime[ch], &iFastTime[ch], &iPGASamples[ch], &fGain[ch][0], &fGain[ch][1]);
				}
				fconf.getline(cBuffer, 64, '\n');
			} // end of while
		} // end of if dig
	} // end of file
	fconf.close();

	int iDateNow(0), iTimeNow(0);
	char cTime[12];
	time_t t_rawtime;
	tm* t_today;
	time(&t_rawtime);
	t_today = localtime(&t_rawtime); // timestamp for processing
	iTimeNow = (t_today->tm_hour)*100 + t_today->tm_min; // hhmm
	iDateNow = (t_today->tm_year-100)*10000 + (t_today->tm_mon+1)*100 + t_today->tm_mday; // yymmdd
	sprintf(cTime, "%i_%i",iDateNow,iTimeNow);
	switch (digitizer.id) {
		case dt5751 :
			if (iSpecial == 0) iXSQ_ndf = min(iEventlength/2, 225)-3; // length - number of free parameters
			else iXSQ_ndf = min(iEventlength, 450)-3;
			break;
		case dt5751des :
			iXSQ_ndf = min(iEventlength, 899)-3;
			break;
		case dt5730 :
			iXSQ_ndf = min(iEventlength, 225)-3;
			break;
		case v1724 :
		case invalid_dig :
		default :
			iXSQ_ndf = -1;
			break;
	}

	cout << "Creating config trees\n";
	try {T0.reset(new TTree("TI","Info"));}
	catch (bad_alloc& ba) {
		cout << "Could not create info tree\n";
		cout << error_message[alloc_error];
		throw ProcessorException();
	}

	T0->Branch("Digitizer",			digitizer.cName,"name[12]/B");
	T0->Branch("Source",			cSource,		"source[12]/B");
	T0->Branch("ChannelMask",		&usMask,		"mask/s"); // general info on data run
	T0->Branch("TriggerThreshold",	uiThreshold,	"threshold[8]/i");
	T0->Branch("DC_offset",			uiDCOffset,		"dc_off[8]/i");
	T0->Branch("Posttrigger",		&iTrigPost,		"tri_post/I");
	T0->Branch("Eventlength",		&iEventlength,	"ev_len/I");
	T0->Branch("Level",				&iLevel,		"level/I");
	T0->Branch("Chisquared_NDF",	&iXSQ_ndf,		"ndf/I");
	T0->Branch("Fitter",			&iXSQ_ndf,		"fitter/I");
	T0->Branch("Time",				cTime,			"time[12]/B");
	T0->Branch("PGA_samples",		iPGASamples,	"pga[8]/I");
	T0->Branch("Fast_window",		iFastTime,		"fast[8]/I");
	T0->Branch("Slow_window",		iSlowTime,		"slow[8]/I");
	if (iSpecial != -1) T0->Branch("Special", &iSpecial, "special/I");
	if (iAverage != 0) T0->Branch("Moving_average", &iAverage, "average/I");
	if ((strcmp(cSource, "NG") == 0) || (strstr(cSource, "252") != nullptr)) { // NG or Cf-252
		if (!bPositionsSet) {
			cout << "Enter detector positions:\n";
			for (auto i = 0; i < 3; i++) {
				cout << "Detector " << i << " z: "; cin >> fDetectorZ[i];
				cout << "Detector " << i << " r: "; cin >> fDetectorR[i];
			}	}
			T0->Branch("Detector_position_z", fDetectorZ, "z_pos[3]/F");
			T0->Branch("Detector_position_r", fDetectorR, "r_pos[3]/F");
	}

	T0->Fill();
	T0->Write();
	T0.reset();

	if (g_verbose) cout << "Alloc'ing\n";
	try {buffer = unique_ptr<char[]>(new char[iEventsize]);}
	catch (bad_alloc& ba) {
		cout << error_message[alloc_error] << "Buffer\n";
		throw ProcessorException();
	}
	memset(buffer.get(), 0, iEventsize);
	unsigned short* uspTrace = (unsigned short*)(buffer.get() + sizeof_ev_header);

	if (iLevel > 1) {
		try {
			T0 = unique_ptr<TTree>(new TTree("T2","Discriminator"));
		} catch (bad_alloc& ba) {
			cout << error_message[alloc_error] << "T2\n";
			throw ProcessorException();
		}
		Discriminator::CutsTree_init(T0.release());
	}

	try {
		T0 = unique_ptr<TTree>(new TTree("T0","Event"));
		if (iLevel > 0) T1 = unique_ptr<TTree>(new TTree("T1","Method"));
		TS = unique_ptr<TTree>(new TTree("TS","Timestamps"));
	} catch (bad_alloc& ba) {
		cout << error_message[alloc_error] << "Trees\n";
		throw ProcessorException();
	}

	T0->Branch("FullWaveform",	bFullWave, "fullwave[8]/O");
	T0->Branch("Saturated",		bSaturated, "sat[8]/O");
	T0->Branch("Pileup",		bPileUp,	"pileup[8]/O");

	T0->Branch("Decaytime",		sDecay,		"decay[8]/S");
	T0->Branch("Risetime",		sRise,		"rise[8]/S");
	T0->Branch("Peakx",			sPeakX,		"peakx[8]/S");
	T0->Branch("Peakxs",		sPeakXs,	"peakxs[8]/S");
	T0->Branch("Trigger",		sTrigger,	"trig[8]/S");
	T0->Branch("Peaks",			sPeaks,		"peaks[8]/S");

	T0->Branch("Base",			dBase,		"base[8]/D");
	T0->Branch("Sigma",			dSigma,		"sigma[8]/D");
	T0->Branch("BaseP",			dBaseP,		"basep[8]/D");
	T0->Branch("BasePS",		dBasePS,	"baseps[8]/D");
	T0->Branch("BasePkP",		dBasePeakP,	"basepkp[8]/D");
	T0->Branch("BasePkN",		dBasePeakN,	"basepkn[8]/D");
	T0->Branch("Peakheight0",	dPeak0,		"peak0[8]/D");
	T0->Branch("Peakheight0s",	dPeak0s,	"peak0s[8]/D");
	T0->Branch("Integral",		dFullInt,	"integral[8]/D");

	if (iLevel > 0) {
		T1->Branch("Truncated",		bTruncated, 	"trunc[8]/O");

		T1->Branch("Baseline",		dBaseline,		"baseline[8]/D");
		T1->Branch("BaseSigma",		dBaseSigma,		"basesigma[8]/D");
		T1->Branch("BasePost",		dBasePost,		"basepost[8]/D");
		T1->Branch("BasePostSigma",	dBasePostSigma, "basepostsigma[8]/D");
		T1->Branch("FastInt",		dFastInt,		"fastint[8]/D");
		T1->Branch("SlowInt",		dSlowInt,		"slowint[8]/D");

		T1->Branch("Peakheight1",	dPeak1,			"peak1[8]/D");
		T1->Branch("Peakheight2",	dPeak2,			"peak2[8]/D");

		T1->Branch("Sample",		sdSample,		"sample[8]/D");

		T1->Branch("Odd",			dOdd,			"odd[8]/D");
		T1->Branch("Even",			dEven,			"even[8]/D");

		T1->Branch("LapLow",		dLaplaceLow,	"laplow[8]/D");
		T1->Branch("LapHi",			dLaplaceHigh,	"laphi[8]/D");

		T1->Branch("Xsq_n",			dXsq[0],		"xsqn[4]/D");
		T1->Branch("Xsq_y",			dXsq[1],		"xsqy[4]/D");
		T1->Branch("Peakscale_n",	dPeak_scale[0],	"pkn[4]/D");
		T1->Branch("Peakscale_y",	dPeak_scale[1],	"pky[4]/D");
		T1->Branch("Base_shift_n",	dBase_shift[0],	"bshn[4]/D");
		T1->Branch("Base_shift_y",	dBase_shift[1],	"bshy[4]/D");
		T1->Branch("Offset_n",		dOffset[0],		"offn[4]/D");
		T1->Branch("Offset_y",		dOffset[1],		"offy[4]/D");

		T1->Branch("Peak_err_n",	dPeak_err[0],	"pkerrn[4]/D");
		T1->Branch("Peak_err_y",	dPeak_err[1],	"pkerry[4]/D");
		T1->Branch("Base_err_n",	dBase_err[0],	"baerrn[4]/D");
		T1->Branch("Base_err_y",	dBase_err[1],	"baerry[4]/D");
		T1->Branch("Off_err_n",		dOff_err[0],	"oferrn[4]/D");
		T1->Branch("Off_err_y",		dOff_err[1],	"oferry[4]/D");
	}

	cout << "Processing level " << iLevel << '\n';

	for (auto ch = 0; ch < iNchan; ch++) { // initializing all classes needed
		if (g_verbose) cout << "CH" << ch << '\n';
		try {
			dTrace[ch].reset(new double[iEventlength-iAverage]);
			event[ch].reset(new Event(iEventlength, digitizer.iBaselength, iAverage, uiThreshold[iChan[ch]], iChan[ch], uspTrace + ch*iEventlength, dTrace[ch].get()));
		} catch (bad_alloc& ba) {
			cout << error_message[alloc_error] << "Event\n";
			throw ProcessorException();
		}
		if (event[ch]->Failed()) {
			cout << error_message[method_error] << "Event\n";
			throw ProcessorException();
		}
		event[ch]->SetScales(digitizer.dScaleV, digitizer.dScaleT);
		event[ch]->SetAddresses(SetAddresses(ch,0));
		if (iLevel > 0) {
			try {
				method[ch].reset(new Method(event[ch]->Length(), iFastTime[iChan[ch]], iSlowTime[iChan[ch]], iPGASamples[iChan[ch]], fGain[iChan[ch]], digitizer.dScaleT, digitizer.dScaleV, event[ch]));
			} catch (bad_alloc& ba) {
				cout << error_message[alloc_error] << "Method\n";
				throw ProcessorException();
			}
			if (method[ch]->Failed()) {
				cout << error_message[method_error] << "Method\n";
				throw ProcessorException();
			}
			method[ch]->SetAddresses(SetAddresses(ch,1));
		}
		if (iLevel > 1) {
			try {
				discriminator[ch].reset(new Discriminator(iChan[ch]));
			} catch (bad_alloc& ba) {
				cout << error_message[alloc_error] << "Discriminator\n";
				throw ProcessorException();
			}
			if (discriminator[ch]->Failed()) {
				cout << error_message[method_error] << "Discriminator\n";
				throw ProcessorException();
			}
			discriminator[ch]->SetAddresses(SetAddresses(ch,2));
		}
	} // ch loop

}
