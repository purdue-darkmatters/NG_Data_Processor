#ifndef DFT_H
#define DFT_H

#ifndef METHOD_H
#include "Method.h"
#endif

const double pi = 3.14159265358979;

class DFT : public Method {
	private:
		const int order; // 3 non-zero
		double scalefactor;
		unique_ptr<double[]> COS;
		unique_ptr<double[]> SIN;

		static shared_ptr<TTree> tree;
		static int howmany;
		static bool initialized;
		
		static double magnitude[8][4]; // order = 3
		static double phase[8][4]; // but we want 0th as well

		
	public:
		DFT(const int ch, const int len);
		virtual ~DFT();
		virtual void evaluate(const shared_ptr<Event> event);
		static void root_init(shared_ptr<TTree> tree_in);
		static void root_deinit() {DFT::tree.reset();}
		static int HowMany() {return DFT::howmany;}
		static float version;
};

#endif // DFT_H
