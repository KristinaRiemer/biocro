#ifndef AUXBIOCRO_H
#define AUXBIOCRO_H

#include <map>

/*
 *  /src/AuxBioCro.h by Fernando Ezequiel Miguez  Copyright (C) 2007-2010
 *
 */


/* This file will contain functions which are common to several */
/* routines in the BioCro package. These are functions needed */
/* internally. The normal user will not need them */


#define MAXLAY    200 /* Maximum number of layers */

struct Light_profile {
	double direct_irradiance[MAXLAY];
	double diffuse_irradiance[MAXLAY];
	double total_irradiance[MAXLAY];
	double sunlit_fraction[MAXLAY];
	double shaded_fraction[MAXLAY];
	double height[MAXLAY];
};

struct ET_Str {
  double TransR;
  double EPenman;
  double EPriestly;
  double Deltat;
  double LayerCond;
};

struct Can_Str {
  double Assim;
  double Trans;
  double GrossAssim;
  double result_matrix[MAXLAY * 21];
  double canopy_transpiration_penman;
  double canopy_transpiration_priestly;
  double canopy_conductance;
};

struct ws_str {
  double rcoefPhoto;
  double rcoefSpleaf;
  double awc;
  double psim;
  double runoff;
  double Nleach;

};

struct soilML_str {
  double rcoefPhoto;
  double rcoefSpleaf;
  double cws[MAXLAY];
  double drainage;
  double Nleach;
  double SoilEvapo;
  double rootDist[MAXLAY];
  double hourlyWflux[MAXLAY];
  double dailyWflux[MAXLAY];
  double width[MAXLAY];
  double swclimit[MAXLAY];
  double pH[MAXLAY];
  double bulkd[MAXLAY];
  double fieldc[MAXLAY];
  double dpthmx[MAXLAY];
  double dpthmn[MAXLAY];
  double soiltavg[MAXLAY];
  double sand[MAXLAY];
  double silt[MAXLAY];
  double clay[MAXLAY];
  int Num_BioCro_soil_layers;
};


struct dbp_str{
	double kLeaf;
	double kStem;
	double kRoot;
	double kRhiz;
	double kGrain;

};

struct soilText_str{
  double silt;
  double clay;
  double sand;
  double air_entry;
  double b;
  double Ks;
  double satur;
  double fieldc;
  double wiltp;
  double bulkd;

};



enum class SoilType { sand, loamy_sand, sandy_loam, loam, silt_loam,
                      sandy_clay_loam, clay_loam, silty_clay_loam, sandy_clay,
                      silty_clay, clay };

const soilText_str soilTchoose(SoilType soiltype);

// For backward compatibility:
soilText_str soilTchoose(int soiltype);


// This map is based on Table 9.1 on page 130 of Campbell and Norman,
// Introduction to Environmental Biophysics.  Bulk density values are
// taken from function getsoilprop.c from Melanie (Colorado).  The
// bulk density value for sandy soil is actually zero or undefined but
// is assigned the value 0.01 here (again, see getsoilprop.c).
//
// The wiltp value of 0.21 (corrected from 0.32) for silty clay loam
// is based on the list of book corrections published at
// http://www.public.iastate.edu/~bkh/teaching/505/norman_book_corrections.pdf.

const std::map<SoilType, soilText_str> soil_parameters
{
  //                             silt  clay  sand  air_entry   b    Ks    satur fieldc wiltp bulkd
  { SoilType::sand,            { 0.05, 0.03, 0.92,      -0.7, 1.7, 5.8e-3, 0.87, 0.09, 0.03, 0.01 } },
  { SoilType::loamy_sand,      { 0.12, 0.07, 0.81,      -0.9, 2.1, 1.7e-3, 0.72, 0.13, 0.06, 1.55 } },
  { SoilType::sandy_loam,      { 0.25, 0.10, 0.65,      -1.5, 3.1, 7.2e-4, 0.57, 0.21, 0.10, 1.50 } },
  { SoilType::loam,            { 0.40, 0.18, 0.42,      -1.1, 4.5, 3.7e-4, 0.57, 0.27, 0.12, 1.43 } },
  { SoilType::silt_loam,       { 0.65, 0.15, 0.20,      -2.1, 4.7, 1.9e-4, 0.59, 0.33, 0.13, 1.36 } },
  { SoilType::sandy_clay_loam, { 0.13, 0.27, 0.60,      -2.8, 4.0, 1.2e-4, 0.48, 0.26, 0.15, 1.39 } },
  { SoilType::clay_loam,       { 0.34, 0.34, 0.32,      -2.6, 5.2, 6.4e-5, 0.52, 0.32, 0.20, 1.35 } },
  { SoilType::silty_clay_loam, { 0.58, 0.33, 0.09,      -3.3, 6.6, 4.2e-5, 0.52, 0.37, 0.21, 1.24 } },
  { SoilType::sandy_clay,      { 0.07, 0.40, 0.53,      -2.9, 6.0, 3.3e-5, 0.51, 0.34, 0.24, 1.30 } },
  { SoilType::silty_clay,      { 0.45, 0.45, 0.10,      -3.4, 7.9, 2.5e-5, 0.52, 0.39, 0.25, 1.28 } },
  { SoilType::clay,            { 0.20, 0.60, 0.20,      -3.7, 7.6, 1.7e-5, 0.53, 0.40, 0.27, 1.19 } }
};

struct seqRD_str{
  double rootDepths[MAXLAY+1];
};

struct seqRD_str seqRootDepth(double to, int lengthOut);

struct rd_str{
  double rootDist[MAXLAY];
};

struct rd_str rootDist(int layer, double rootDepth, double *depths, double rfl);

struct frostParms {
  double leafT0;
  double leafT100;
  double stemT0;
  double stemT100;
  double rootT0;
  double rootT100;
  double rhizomeT0;
  double rhizomeT100;
};

struct nitroParms {
  double ileafN;
	double kln;
	double Vmaxb1;
	double Vmaxb0;
	double alphab1;
	double alphab0;
	double Rdb1;
	double Rdb0;
	double kpLN;
	double lnb0;
	double lnb1;
	int lnFun;
	double maxln;
    double minln;
	double daymaxln;
};

struct crop_phenology {
        struct dbp_str DBP;
        struct minerals1 {
        double CN;
        double CP;
        double CK;
        double CS;
      }leaf,stem,root,rhiz;
};

double leafboundarylayer(double windspeed, double leafwidth, double AirTemp,
                         double deltat, double stomcond, double vappress);

double SoilEvapo(double LAI, double k, double AirTemp, double IRad,
                 double awc, double fieldc, double wiltp, double winds, double RelH, double rsec );

void LNprof(double LeafN, double LAI, int nlayers, double kpLN, double* leafNla);

#endif

