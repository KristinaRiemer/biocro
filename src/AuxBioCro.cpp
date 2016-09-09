/*
 *  /src/AuxBioCro.c by Fernando Ezequiel Miguez  Copyright (C) 2007 - 2015
 *
 *  Part of the code here (sunML, EvapoTrans, SoilEvapo, TempTo and
 *  the *prof functions) are based on code in WIMOVAC. WIMOVAC is
 *  copyright of Stephen Long and Stephen Humphries.
 *  Documentation for WIMOVAC can be found at
 *  http://www.life.illinois.edu/plantbio/wimovac/ (checked 02-13-2010)
 *
 */

/* This file will contain functions which are common to several */
/* routines in the BioCro package. These are functions needed */
/* internally. The normal user will not need them */

#include <R.h>
#include <Rmath.h>
#include <Rinternals.h>
#include <stdexcept>
#include "c4photo.h"
#include "BioCro.h"

/**
 * Computation of the cosine of the zenith angle from latitute (`lat`), day of
 * the year (`DOY`), and time of day (`td`).
 *
 * For the values of angles in radians, we'll use the common practice of
 * denoting the latitude by `phi` (\f$\phi\f$), the declination by `delta`
 * (\f$\delta\f$), and the hour angle by `tau` (\f$\tau\f$).  `NDS` denotes the
 * number of days after December solstice and `omega` (\f$\omega\f$) denotes the
 * angle (in radians) of the orbital position of the earth around the sun
 * relative to its position at the December solstice.
 *
 * Denoting the axial tilt of the earth by \f$\varepsilon\f$, the equation used
 * for declination (`delta`) is
 * \f[
 *     \delta = -\varepsilon \cos(\omega)
 * \f]
 * which is an approximation (always accurate to within 0.26 degrees) of the
 * more accurate formula
 * \f[
 *     \sin(\delta) = -\sin(\varepsilon) \cos(\omega)
 * \f]
 *
 * The cosine of the solar zenith angle \f$\theta_s\f$ may be calculated from
 * the declination \f$\delta\f$, the latitude \f$\phi\f$, and the hour angle
 * \f$\tau\f$ by the formula
 * \f[
 *     \cos(\theta_s) = \sin(\delta) \sin(\phi) + \cos(\delta) \cos(\phi) \cos(\tau)
 * \f]
 * which is a straight-forward application of the law of cosines for spherical
 * triangles, substituting cofunctions of coangles in the case of latitude and
 * declination.
 */
double cos_zenith_angle(double latitude, int day_of_year, int hour_of_day) {

    constexpr double RADIANS_PER_DEGREE = M_PI/180;
    constexpr int SOLAR_NOON = 12;
    constexpr double RADIANS_ROTATION_PER_HOUR = 15 * RADIANS_PER_DEGREE;
    constexpr double AXIAL_TILT = 23.5 * RADIANS_PER_DEGREE;

    double phi = latitude * RADIANS_PER_DEGREE;
    int NDS = day_of_year + 10;

    double omega = 360.0 * (NDS / 365.0) * RADIANS_PER_DEGREE;
    
    double delta = -AXIAL_TILT * cos(omega);

    double tau = (hour_of_day - SOLAR_NOON) * RADIANS_ROTATION_PER_HOUR;

    return sin(delta) * sin(phi) + cos(delta) * cos(phi) * cos(tau);
}

/**
 * Light Macro Environment
 */
Light_model lightME(double latitude, int day_of_year, int hour_of_day)
{
    // day_of_year and hour_of_day are given as local time for the specified latitude.
    //
    // Return values.
    //  light_model.direct_irradiance_fraction: The fraction of irradiance that is direct radiation. dimensionless.
    //  light_model.diffuse_irradiance_fraction: The fraction of irradiance that is diffuse radiation. dimensionless.
    //  light_model.cosine_zenith_angle: The cosine of the zenith angle of the Sun. When the Sun is directly overhead, the angle is 0 and cos(angle) is 1. dimensionless.
    //
    // The basis for this function is given in chapter 11 of Norman and Campbell. An Introduction to Environmental Biophysics. 2nd edition.

    double cosine_zenith_angle = cos_zenith_angle(latitude, day_of_year, hour_of_day);  // dimensionless.
    double direct_irradiance_transmittance;
    double diffuse_irradiance_transmittance;

    if (cosine_zenith_angle <= 0) { // Check if the Sun is at or below the horizon. If it is, directly set direct_irradiance_transmittance and diffuse_irradiance_transmittance because the equations below would otherwise produce erroneous results. The Sun is at or below the horizon when cosine_zenith_angle is less than or equal to 0.
        direct_irradiance_transmittance = 0;
        diffuse_irradiance_transmittance = 1;
    } else { // If the Sun is above the horizon, calculate diffuse and direct irradiance from the Sun's angle to the ground and the path through the atmosphere.
        constexpr double atmospheric_transmittance = 0.85; // dimensionless.
        constexpr double atmospheric_pressure_at_sea_level = 1e5; // Pa.
        constexpr double local_atmospheric_pressure = 1e5; // Pa.
        constexpr double pressure_ratio = local_atmospheric_pressure / atmospheric_pressure_at_sea_level; // dimensionless.
        constexpr double proportion_of_irradiance_scattered = 0.3; // dimensionless.

        direct_irradiance_transmittance = pow(atmospheric_transmittance, (pressure_ratio / cosine_zenith_angle)); // Modified from equation 11.11 of Norman and Campbell.
        diffuse_irradiance_transmittance = proportion_of_irradiance_scattered * (1 - direct_irradiance_transmittance) * cosine_zenith_angle; // Modified from equation 11.13 of Norman and Campbell.
    }

    Light_model light_model;
    light_model.direct_irradiance_fraction = direct_irradiance_transmittance / (direct_irradiance_transmittance + diffuse_irradiance_transmittance); // dimensionless.
    light_model.diffuse_irradiance_fraction = diffuse_irradiance_transmittance / (direct_irradiance_transmittance + diffuse_irradiance_transmittance); // dimensionless.
    light_model.cosine_zenith_angle = cosine_zenith_angle; // dimensionless.

    return light_model;
}

/**
 * Computation of an n-layered light profile from the direct light `Idir`,
 * diffuse light `Idiff`, leaf area index `LAI`, the cosine of the zenith angle
 * `cosTheta` and other parameters.
 *
 * Preconditions:
 *    `Idir` and `Idiff` are positive.
 *    `LAI` is non-negative.
 *    `nlayers` is at least 1 and not more than MAXLAY, the compile-time
 *        constant defining the maximum array size for arrays used to store
 *        profile data (enforced).
 *    `cosTheta` is greater than 0 and at most 1 (enforced).
 *    `kd` is between 0 and 1.
 *    `chil` is non-negative.
 *    `heightf` is positive.
 */ 
Light_profile sunML(double Idir, double Idiff, double LAI, int nlayers,
        double cosTheta, double kd, double chil, double heightf)
{
    if (nlayers < 1 || nlayers > MAXLAY) {
        throw std::out_of_range("nlayers must be at least 1 but no more than 200");
    }
    if (cosTheta > 1 || cosTheta <= 0) {
        throw std::out_of_range("cosTheta must be positive but no more than 1.");
    }

    constexpr auto alphascatter = 0.8;

    auto theta = acos(cosTheta);
    auto k0 = sqrt( pow(chil, 2) + pow(tan(theta), 2) );
    auto k1 = chil + 1.744 * pow((chil + 1.183), -0.733);
    auto k = (k1 > 0) ? k0/k1 : -k0/k1;
    
    auto LAIi = LAI / nlayers;

    Light_profile light_profile;
    for (int i = 0; i < nlayers; i++) {
        auto CumLAI = LAIi * (i + 0.5);

        auto Ibeam = Idir * cosTheta;
        auto Iscat = Ibeam * exp(-k * sqrt(alphascatter) * CumLAI)
            - Ibeam * exp(-k * CumLAI);

        auto Isolar = Ibeam * k;
        auto Idiffuse = Idiff * exp(-kd * CumLAI) + Iscat;

        auto Ls = (1 - exp(-k * LAIi)) * exp(-k * CumLAI) / k;
        auto Ld = LAIi - Ls;

        auto Fsun = Ls/(Ls + Ld);
        auto Fshade = Ld/(Ls + Ld);

        auto Iaverage = (Fsun * (Isolar + Idiffuse) + Fshade * Idiffuse)
            * (1 - exp(-k * LAIi)) / k;

        light_profile.direct_irradiance[i] = Isolar + Idiffuse;
        light_profile.diffuse_irradiance[i]= Idiffuse;
        light_profile.total_irradiance[i] = Iaverage;
        light_profile.sunlit_fraction[i] = Fsun;
        light_profile.shaded_fraction[i] = Fshade;
        light_profile.height[i] = (LAI - CumLAI)/heightf;
    }
    return light_profile;
}

/* Additional Functions needed for EvapoTrans */


/**
 * RH and Wind profile function
 *
 * Preconditions:
 *     `WindSpeed` is non-negative.
 *     `LAI` is non-negative
 *     `nlayers` is at least 1 and at most MAXLAY.
 *     `wind_speed_profile` is an array of at least size `nlayers`.
 */
void WINDprof(double WindSpeed, double LAI, int nlayers,
              double* wind_speed_profile)
{
    constexpr auto k = 0.7;

    auto LI = LAI / nlayers;
    for (int i = 0; i < nlayers; i++)
    {
        auto CumLAI = LI * (i + 1);
        wind_speed_profile[i] = WindSpeed * exp(-k * (CumLAI - LI));
    }
}

/**
 * Preconditions:
 *     `RH` is between 0 and 1.
 *     `nlayers` is at least 1 and at most MAXLAY.
 *     `relative_humidity_profile` is an array of at least size `nlayers`.
 *
 * Postconditions:
 *     `relative_humidity_profile` contains values between 0 and 1.
 */
void RHprof(double RH, int nlayers, double* relative_humidity_profile)
{
    const double kh = 1 - RH;
    /* kh = 0.2; */
    /* kh = log(1/RH); */

    for (int i = 0; i < nlayers; i++)
    {
        // explicitly make j a double so that j/nlayers isn't truncated:
        double j = i + 1;

        auto temp_rh = RH * exp(kh * (j/nlayers));
        //   temp_rh = RH * exp(-kh * (j/nlayers));  // new simpler version from Joe Iverson*
        relative_humidity_profile[i] = temp_rh;
    }
}

void LNprof(double LeafN, double LAI, int nlayers, double kpLN, double* leafN_profile)
{
    int i;
    double LI, CumLAI;

    LI = LAI / nlayers;
    for(i = 0; i < nlayers; i++)
    {
        CumLAI = LI * (i + 1);
        leafN_profile[i] = LeafN * exp(-kpLN * (CumLAI-LI));
    }
}

double TempToDdryA(double Temp)
{
    return 1.295163636 + -0.004258182 * Temp;
}

double TempToLHV(double Temp)
{
    return 2.501 + -0.002372727 * Temp;
}

double TempToSFS(double Temp)
{
    return 0.338376068 + 0.011435897 * Temp + 0.001111111 * pow(Temp, 2);
}

double TempToSWVC(double Temp)
{
    /* Temp should be in Celsius */
    /* This is the arden buck equation */
    auto a = (18.678 - Temp/234.5) * Temp;
    auto b = 257.14 + Temp;
    /* SWVC = (6.1121 * exp(a/b))/10; */
    return 6.1121 * exp(a/b); /* This is in hecto Pascals */
}

/* EvapoTrans function */
ET_Str EvapoTrans(
        double Rad, // unused
        double Itot,
        double airTemp,
        double RH,
        double WindSpeed,
        double LeafAreaIndex, // unused
        double CanopyHeight,
        double StomataWS,
        int ws,
        double vmax2,
        double alpha2,
        double kparm,
        double theta,
        double beta,
        double Rd2,
        double b02,
        double b12,
        double upperT,
        double lowerT,
        double Catm)
{
    // const double LeafWidth = 0.04; unused
    const auto kappa = 0.41;
    const auto WindSpeedHeight = 5;
    const auto dCoef = 0.77;
    const auto tau = 0.2;
    const auto ZetaCoef = 0.026;
    const auto ZetaMCoef = 0.13;
    const auto LeafReflectance = 0.2;
    const auto SpecificHeat = 1010;

    // double BoundaryLayerThickness, DiffCoef, LeafboundaryLayer; unused

    CanopyHeight = fmax(0.1, CanopyHeight); // ensure CanopyHeight's at least 0.1
    WindSpeed = fmax(0.5, WindSpeed); // ensure WindSpeed is as least 0.5
    auto LayerWindSpeed = WindSpeed; // alias

    auto DdryA = TempToDdryA(airTemp);
    auto LHV = TempToLHV(airTemp) * 1e6;
    /* Here LHV is given in MJ kg-1 and this needs to be converted
       to Joules kg-1  */
    auto SlopeFS = TempToSFS(airTemp) * 1e-3;

    /* Relative Humidity is giving me a headache */
    /* RH2 = RH * 1e2; A high value of RH makes the
       difference of temperature between the leaf and the air huge.
       This is what is causing the large difference in DeltaT at the
       bottom of the canopy. I think it is very likely.  */
    auto LayerRelativeHumidity = RH * 100;
    if (LayerRelativeHumidity > 100) {
        error("LayerRelativehumidity > 100");
    }

    auto SWVC = TempToSWVC(airTemp) * 1e-3;
    if (SWVC < 0) {
        error("SWVC < 0");
    }

    /* FIRST CALCULATIONS*/

    auto Zeta = ZetaCoef * CanopyHeight;
    auto Zetam = ZetaMCoef * CanopyHeight;
    auto d = dCoef * CanopyHeight;

    /* Convert light assuming 1 micromole PAR photons = 0.235 J/s */
    auto totalradiation = Itot * 0.235;

    auto LayerConductance = (c4photoC(Itot, airTemp, RH, vmax2, alpha2, kparm,
                                     theta, beta, Rd2, b02, b12, StomataWS,
                                     Catm, ws, upperT, lowerT)
                             ).Gs;

    /* Convert mmoles/m2/s to moles/m2/s
       LayerConductance = LayerConductance * 1e-3
       Convert moles/m2/s to mm/s
       LayerConductance = LayerConductance * 24.39
       Convert mm/s to m/s
       LayerConductance = LayerConductance * 1e-3 */

    auto LayerConductance_in_m_per_s = LayerConductance * 1e-6 * 24.39;
    /* Thornley and Johnson use m s^-1 on page 418 */

    /* prevent errors due to extremely low Layer conductance */
    if (LayerConductance_in_m_per_s <= 0) {
        LayerConductance_in_m_per_s = 0.01;
    }

    /* Now RHprof returns relative humidity in the 0-1 range */
    auto DeltaPVa = SWVC * (1 - LayerRelativeHumidity / 100);

    auto PsycParam =(DdryA * SpecificHeat) / LHV;

    /* Ja = (2 * totalradiation * ((1 - LeafReflectance - tau) / (1 - tau))) * LeafAreaIndex; */
    /* It seems that it is not correct to multiply by the leaf area index. Thornley
       and johnson pg. 400 suggest using (1-exp(-k*LAI) but note that for a full canopy
       this is 1. so I'm using 1 as an approximation. */
    auto Ja = (2 * totalradiation * ((1 - LeafReflectance - tau) / (1 - tau)));
    /* Calculation of ga */
    /* According to thornley and Johnson pg. 416 */
    auto ga0 = pow(kappa, 2) * LayerWindSpeed;
    auto ga1 = log( (WindSpeedHeight + Zeta - d) / Zeta );
    auto ga2 = log( (WindSpeedHeight + Zetam - d) / Zetam );
    auto ga = ga0 / (ga1 * ga2);

    /*  Rprintf("ga: %.5f \n", ga); */
    if (ga < 0) {
        error("ga is less than zero");
    }

    // DiffCoef = (2.126 * 1e-5) + ((1.48 * 1e-7) * Airtemperature); set but not used
    // BoundaryLayerThickness = 0.004 * sqrt(LeafWidth / LayerWindSpeed); set but not used
    // LeafboundaryLayer = DiffCoef / BoundaryLayerThickness; // set but not used

    /* Temperature of the leaf according to Campbell and Norman (1998) Chp 4.*/
    /* This version is non-iterative and an approximation*/
    /* Stefan-Boltzmann law: B = sigma * T^4. where sigma is the Boltzmann
constant: 5.67 * 1e-8 W m^-2 K^-4. */

    /* From Table A.3 in the book we know that*/

    /*  rlc = (4 * (5.67*1e-8) * pow(273 + Airtemperature, 3) * Deltat) * LeafAreaIndex;   */
    /*  PhiN = Ja - rlc; */

    /*  dd = 0.72 * LeafWidth; */
    /*  gHa = 1.4 * 0.135 * sqrt(LayerWindSpeed * 0.72 * LeafWidth);  */
    /*  gva = 1.4 * 0.147 * sqrt(LayerWindSpeed * 0.72 * LeafWidth);  */

    /*  TopValue = Ja - (SWVC/7.28) * 5.67*1e-8 * pow(Airtemperature + 273.15,4) - LHV * LayerConductance * 1e-3 * DeltaPVa / 101.3; */
    /* Here I divide SWVC by 7.28 to approximately change from g/m3 to kPa */
    /* I'm also multiplying Layer conductance by 1e-3 since it should be in mol m^-2 s^-1 */

    /*  BottomValue = SpecificHeat * gHr + LHV * SlopeFS * LayerConductance * 1e-3;  */

    /*  Deltat = Airtemperature + TopValue/BottomValue; */

    /*  PhiN = TopValue; */

    /* This is the original from WIMOVAC*/

    // Declare variables we need after exiting the loop:
    auto Deltat = 0.01;
    double PhiN;
    double rlc;

    {
        auto ChangeInLeafTemp = 10.0;
        auto BottomValue = LHV * (SlopeFS + PsycParam * (1 + ga / LayerConductance_in_m_per_s));
        for (auto Counter = 0; (ChangeInLeafTemp > 0.5) && (Counter <= 10); ++Counter) {

            auto OldDeltaT = Deltat;

            /*         rlc = (4 * (5.67*1e-8) * pow(273 + Airtemperature, 3) * Deltat) * LeafAreaIndex;   */
            rlc = (4 * (5.67*1e-8) * pow(273 + airTemp, 3) * Deltat);

            PhiN = (Ja - rlc);

            auto TopValue = PhiN * (1 / ga + 1 / LayerConductance_in_m_per_s) - LHV * DeltaPVa;
            Deltat = fmin(fmax(TopValue / BottomValue, -5), 5); // confine to interval [-5, 5]

            ChangeInLeafTemp = fabs(OldDeltaT - Deltat);
        }
    }


    PhiN = fmax(PhiN, 0); // ensure PhiN >= 0

    auto TransR = (SlopeFS * PhiN + (LHV * PsycParam * ga * DeltaPVa))
        /
        (LHV * (SlopeFS + PsycParam * (1 + ga / LayerConductance_in_m_per_s)));

    auto EPries = 1.26 * ((SlopeFS * PhiN) / (LHV * (SlopeFS + PsycParam)));

    auto EPen = ((SlopeFS * PhiN) + LHV * PsycParam * ga * DeltaPVa)
        /
        (LHV * (SlopeFS + PsycParam));

    /* This values need to be converted from Kg/m2/s to
       mmol H20 /m2/s according to S Humphries */
    /*res[1,1] = EPen * 10e6 / 18;*/
    /*res[1,2] = EPries * 10e6 / 18;*/
    /* 1e3 - kgrams to grams  */
    /* 1e3 - mols to mmols */
    /*  res[0] = ((TransR * 1e3) / 18) * 1e3; */
    /*  res[1] = Deltat; */
    /*  res[2] = LayerConductance; */
    /* Let us return the structure now */

    ET_Str et_results;
    et_results.TransR = TransR * 1e6 / 18;
    et_results.EPenman = EPen * 1e6 / 18;
    et_results.EPriestly = EPries * 1e6 / 18;
    et_results.Deltat = Deltat;
    et_results.LayerCond = LayerConductance_in_m_per_s * 1e6 * (1/24.39);
    /*    et_results.LayerCond = RH2;   */
    /*   et_results.LayerCond = 0.7; */
    return et_results;
}

/* New EvapoTrans function */
struct ET_Str EvapoTrans2(double Rad,
        double Iave,
        double Airtemperature,
        double RH,
        double WindSpeed,
        double LeafAreaIndex,
        double CanopyHeight,
        double stomatacond,
        double leafw,
        int eteq)
{

    /* creating the structure to return */
    struct ET_Str tmp;

    // const double kappa = 0.41; /* von Karmans constant */ unused
    double WindSpeedHeight = 2; /* This is the height at which the wind speed was measured */
    // const double dCoef = 0.77;  unused
    const double tau = 0.2; /* Leaf transmission coefficient */
    // const double ZetaCoef = 0.026; unused
    // const double ZetaMCoef = 0.13; unused
    const double LeafReflectance = 0.2; 
    const double SpecificHeat = 1010; /* J kg-1 K-1 */
    const double StefanBoltzmann = 5.67037e-8; /* J m^-2 s^-1 K^-4 */

    double Tair;
    double DdryA, LHV, SlopeFS, SWVC, SWVP;
    double LayerWindSpeed, totalradiation;
    double DeltaPVa, PsycParam, ga;
    double gvs; /* Conductance to vapor from stomata same as stomatacond (input variable) */ 
    double ActualVaporPressure;
    double Ja, Ja2, Deltat;
    double PhiN, PhiN2;
    double TopValue, BottomValue;
    double EPen, TransR,EPries; 
    double OldDeltaT, ChangeInLeafTemp;
    double rlc; /* Long wave radiation for iterative calculation */
    int Counter;

    // double WindSpeedTopCanopy = WindSpeed; // unused.
    Tair = Airtemperature;

    if(CanopyHeight < 0.1)
        CanopyHeight = 0.1; 

    /* When the height at which wind was measured is lower than the canopy height */
    /* There can be problems with the calculations */
    /* This is a very crude way of solving this problem */
    if(CanopyHeight + 1 > WindSpeedHeight)
        WindSpeedHeight = CanopyHeight + WindSpeedHeight;

    DdryA = TempToDdryA(Tair); /* Density of dry air, kg / m^3 */

    /* In the original code in WIMOVAC this is used in J kg-1
       but Thornley and Johnson use it as MJ kg-1  */
    LHV = TempToLHV(Tair); /* This should be MJ kg^-1*/
    LHV = LHV * 1e6; /* Now it is converted to Joules kg^-1*/
    SlopeFS = TempToSFS(Tair) * 1e-3; /* kg m^-3 K^-1 */
    SWVP = TempToSWVC(Tair); /* this is hecto Pascals */
    /* Convert to kg/m3 */
    SWVC = (DdryA * 0.622 * SWVP)/1013.25; /* This last number is atmospheric pressure in hecto pascals */
    /* SWVC is saturated water vapor concentration (or density) in kg/m3 */

    PsycParam =(DdryA * SpecificHeat) / LHV; /* This is in kg m-3 K-1 */

    DeltaPVa = SWVC * (1 - RH); /* kg/m3 */

    ActualVaporPressure = RH * SWVP; /* hecto Pascals */

    /* SOLAR RADIATION COMPONENT*/

    /* First calculate the Radiation term */
    /*' Convert light assuming 1 micromole PAR photons = 0.235 J */
    /* The next step converts from PAR photons to Joules */
    /* There are 2.35 x 10^5 Joules in a mol */
    /* or 0.235 Joules in a micro mol */
    /* A Watt is equivalent to J/s */
    totalradiation = Rad * 0.235; /* This is essentially Watts m^-2 */
    /* On a clear sky it may exceed 1000 in some parts of the world 
       Thornley and Johnson pg 400 */
    /* This values can not possibly be higher than 650 */
    if(totalradiation > 650) error("total radiation too high");

    /* Ja = (2 * totalradiation * ((1 - LeafReflectance - tau) / (1 - tau))) * LeafAreaIndex; */
    /* It seems that it is not correct to multiply by the leaf area index. The previous
       version was used in WIMOVAC (check) */
    Ja = (2 * totalradiation * ((1 - LeafReflectance - tau) / (1 - tau)));

    /* The value below is only for leaf temperature */
    Ja2 = (2 * Iave * 0.235 * ((1 - LeafReflectance - tau) / (1 - tau)));

    /* AERODYNAMIC COMPONENT */
    if(WindSpeed < 0.5) WindSpeed = 0.5;

    LayerWindSpeed = WindSpeed;

    /* Rprintf("Gs %.3f \n", stomatacond); */
    /* Leaf Conductance */
    gvs = stomatacond; 
    /* Convert from mmol H20/m2/s to m/s */
    gvs = gvs * (1.0/41000.0);
    /* 1/41000 is the same as 24.39 * 1e-6 */
    /* Thornley and Johnson use m s^-1 on page 418 */

    /* prevent errors due to extremely low Layer conductance */
    if(gvs <= 0.001)
        gvs = 0.001;

    /* This is the original from WIMOVAC*/
    Deltat = 0.01;
    ChangeInLeafTemp = 10;

    Counter = 0;
    while( (ChangeInLeafTemp > 0.5) && (Counter <= 10))
    {
        OldDeltaT = Deltat;

        rlc = 4 * StefanBoltzmann * pow(273 + Tair, 3) * Deltat;  

        /* rlc=net long wave radiation emittted per second =radiation emitted per second - radiation absorbed per second=sigma*(Tair+deltaT)^4-sigma*Tair^4 */

        /* Then you do a Taylor series about deltaT = 0 and keep only the zero and first order terms. */

        /* or rlc=sigma*Tair^4+deltaT*(4*sigma*Tair^3)-sigma*Tair^4=4*sigma*Tair^3*deltaT */

        /* where 4*sigma*Tair^3 is the derivative of sigma*(Tair+deltaT)^4 evaluated at deltaT=0, */

        ga = leafboundarylayer(LayerWindSpeed, leafw, 
                Airtemperature, Deltat,
                gvs, ActualVaporPressure);
        /* This returns leaf-level boundary layer conductance */ 
        /* In WIMOVAC this was added to the canopy conductance */
        /* ga = (ga * gbcW)/(ga + gbcW);  */

        PhiN2 = (Ja2 - rlc);  /* * LeafAreaIndex;  */

        TopValue = PhiN2 * (1 / ga + 1 / gvs) - LHV * DeltaPVa;
        BottomValue = LHV * (SlopeFS + PsycParam * (1 + ga / gvs));
        Deltat = TopValue / BottomValue; /* This equation is from Thornley and Johnson pg. 418 */
        if(Deltat > 10)	Deltat = 10;
        if(Deltat < -10) Deltat = -10;

        ChangeInLeafTemp = OldDeltaT - Deltat;
        if(ChangeInLeafTemp <0)
            ChangeInLeafTemp = -ChangeInLeafTemp;
        Counter++;
    }

    /* Net radiation */
    PhiN = Ja - rlc;

    if(PhiN < 0)
        PhiN = 0;

    TransR = (SlopeFS * PhiN + (LHV * PsycParam * ga * DeltaPVa)) / (LHV * (SlopeFS + PsycParam * (1 + ga / gvs)));

    /* Penman will use the WIMOVAC conductance */
    EPen = (((SlopeFS * PhiN) + LHV * PsycParam * ga * DeltaPVa)) / (LHV * (SlopeFS + PsycParam));

    EPries = 1.26 * ((SlopeFS * PhiN) / (LHV * (SlopeFS + PsycParam)));

    /* Choose equation to report */
    if(eteq == 1) {
        TransR = EPen;
    }
    if(eteq == 2) {
        TransR = EPries;
    }

    /* This values need to be converted from Kg/m2/s to
       mmol H20 /m2/s according to S Humphries */
    /* 1e3 - kgrams to grams  */
    /* 1e3 - mols to mmols */
    /* grams to mols - 18g in a mol */
    /* Let us return the structure now */

    tmp.TransR = TransR * 1e6 / 18; 
    tmp.EPenman = EPen * 1e6 / 18; 
    tmp.EPriestly = EPries * 1e6 / 18; 
    tmp.Deltat = Deltat;
    tmp.LayerCond = gvs * 41000;   
    return(tmp);
}


double leafboundarylayer(double windspeed, double leafwidth, double AirTemp,
        double deltat, double stomcond, double vappress) {
    /* This is the leaf boundary layer computed using the approach in MLcan
       which is based on (Nikolov, Massman, Schoettle),         %
       Ecological Modelling, 80 (1995), 205-235 */
    const double Pa = 101325;
    const double cf = 1.6361e-3;

    double leaftemp = AirTemp + deltat;
    double gsv = stomcond; /* input is in m/s */
    double Tak = AirTemp + 273.15; /* Converts from C to K */
    double Tlk = leaftemp + 273.15;  /* Converts from C to K */
    double ea = vappress * 1e2; /* From hPa to Pa */
    double ws = windspeed; /* m s^-1 */
    double lw = leafwidth; /* meters */

    double esTl, eb;
    double gbv_forced, gbv_free, gbv;
    // double gbh; // unused
    double Tvdiff;

    esTl = TempToSWVC(leaftemp) * 100; /* The function returns hPa, but need Pa */

    /* Forced convection */ 
    gbv_forced = cf *  pow(Tak,0.56) * pow((Tak+120)*((ws/lw)/Pa),0.5);
    gbv_free = gbv_forced;
    eb = (gsv * esTl + gbv_free * ea)/(gsv + gbv_free); /* Eq 35 */
    Tvdiff = (Tlk / (1 - 0.378 * eb/Pa)) - (Tak / (1-0.378*ea/Pa)); /* Eq 34*/

    if(Tvdiff < 0) Tvdiff = -Tvdiff;

    gbv_free = cf * pow(Tlk,0.56) * pow((Tlk+120)/Pa,0.5) * pow(Tvdiff/lw,0.25);

    if(gbv_forced > gbv_free) {
        gbv = gbv_forced;
    } else {
        gbv = gbv_free;
    } 
    // gbh = 0.924 * gbv; // set but not used

    return(gbv); 
}




/* Soil Evaporation Function */
/* Variables I need */
/* LAI = Leaf Area Index */
/* k = extinction coefficient */
/* Temp = Air Temperature */
/* DirectRad = Direct Total Radiation */
/* awc, wiltp, fieldc = available water content, wilting point and field capacity */
/* winds = wind speed */

double SoilEvapo(double LAI, double k, double AirTemp, double IRad,
        double awc, double fieldc, double wiltp, double winds, double RelH, double rsec )
{

    double SoilArea;
    double SoilTemp;
    double Up; /*Maximum Dimensionless Uptake Rate */
    double TotalRadiation;
    double BoundaryLayerThickness, DiffCoef;
    double SoilBoundaryLayer, Ja, rlc;
    double PhiN, PsycParam, DeltaPVa;
    double Evaporation = 0.0;  
    double DdryA, LHV, SlopeFS, SWVC;

    double rawc; /* Relative available water content */

    int method = 1;

    /* some constants */
    const double SoilClodSize = 0.04;
    const double SoilReflectance = 0.2;
    const double SoilTransmission = 0.01;
    const double SpecificHeat = 1010;
    const double StefanBoltzman = 5.67e-8; /* Stefan Boltzman Constant */

    const double cf2 = 3600 * 1e-3 * 18 * 1e-6 * 10000; 

    /* For Transpiration */
    /* 3600 converts seconds to hours */
    /* 1e-3 converts mili mols to mols */
    /* 18 is the grams in one mol of H20 */
    /* 1e-6 converts g to Mg */
    /* 10000 scales from meter squared to hectare */

    /* Let us assume a simple way of calculating the proportion of the
       soil with direct radiation */
    SoilArea = exp(-k * LAI);

    /* For now the temperature of the soil will be the same as the air.
       At a later time this can be made more accurate. I looked at the
       equations for this and the issue is that it is strongly dependent on
       depth. Since the soil model now has a single layer, this cannot be
       implemented correctly at the moment.  */

    SoilTemp = AirTemp;

    /* Let us use an idea of Campbell and Norman. Environmental
       Biophysics. */
    /* If relative available water content is */
    rawc = (awc - wiltp)/(fieldc - wiltp);

    /* Page 142 */
    /* Maximum Dimensionless Uptake Rate */
    Up = 1 - pow((1 + 1.3 * rawc),-5);  
    /* This is a useful idea because dry soils evaporate little water when dry*/

    /* Total Radiation */
    /*' Convert light assuming 1 micromole PAR photons = 0.235 J/s Watts*/
    /* At the moment soil evaporation is grossly overestimated. In WIMOVAC
       the light reaching the last layer of leaves is used. Here instead
       of calculating this again, I will for now assume a 10% as a rough
       estimate. Note that I could maybe get this since layIdir and
       layIDiff in sunML are external variables.  Rprintf("IRad
       %.5f",layIdir[0],"\n"); Update: 03-13-2009. I tried printing this
       value but it is still too high and will likely overestimate soil
       evaporation. However, this is still a work in progress.
       */
    IRad *= rsec; /* Radiation soil evaporation coefficient */ 

    TotalRadiation = IRad * 0.235;

    DdryA = TempToDdryA(AirTemp);
    LHV = TempToLHV(AirTemp) * 1e6; 
    /* Here LHV is given in MJ kg-1 and this needs to be converted
       to Joules kg-1  */
    SlopeFS = TempToSFS(AirTemp) * 1e-3;
    SWVC = TempToSWVC(AirTemp) * 1e-3;

    PsycParam = (DdryA * SpecificHeat) / LHV;
    DeltaPVa = SWVC * (1 - RelH / 100);

    BoundaryLayerThickness = 4e-3 * sqrt(SoilClodSize / winds); 
    DiffCoef = 2.126e-5 * 1.48e-7 * SoilTemp;
    SoilBoundaryLayer = DiffCoef / BoundaryLayerThickness;

    Ja = 2 * TotalRadiation * ((1 - SoilReflectance - SoilTransmission) / (1 - SoilTransmission));

    rlc = 4 * StefanBoltzman * pow((273 + SoilTemp),3) * 0.005;
    /* the last term should be the difference between air temperature and
       soil. This is not actually calculated at the moment. Since this is
       mostly relevant to the first soil layer where the temperatures are
       similar. I will leave it like this for now. */

    PhiN = Ja - rlc; /* Calculate the net radiation balance*/
    if(PhiN < 0) PhiN = 1e-7;

    /* Priestly-Taylor */
    if(method == 0) {
        Evaporation = 1.26 * (SlopeFS * PhiN) / (LHV * (SlopeFS + PsycParam));
    } else {
        /* Penman-Monteith */
        Evaporation = (SlopeFS * PhiN + LHV * PsycParam * SoilBoundaryLayer * DeltaPVa) / (LHV * (SlopeFS + PsycParam));
    }
    /*  Report back the soil evaporation rate in Units mmoles/m2/s */
    /*     Evaporation = Evaporation * 1000:   ' Convert Kg H20/m2/s to g H20/m2/s */
    /*     Evaporation = Evaporation / 18:     ' Convert g H20/m2/s to moles H20/m2/s */
    /*     Evaporation = Evaporation * 1000:   ' Convert moles H20/m2/s to mmoles H20/m2/s */

    /*     If Evaporation <= 0 Then Evaporation = 0.00001: 
           ' Prevent any odd looking values which might get through at very low light levels */

    Evaporation *= 1e6/18;
    /* Adding the area dependence and the effect of drying */
    /* Converting from m2 to ha (times 1e4) */
    /* Converting to hour */
    Evaporation *= SoilArea * Up * cf2; 
    if(Evaporation < 0) Evaporation = 1e-6;

    return(Evaporation);
}

/* This is a new function that attempts to keep a water budget and then
   calcualte an empirical coefficient that reduces the specific leaf area.
   This results from the general idea that water stress reduces first the 
   rate of leaf expansion. */ 

/* This is meant to be a simple function that calculates a
   simple empirical coefficient that reduces specifi leaf area
   according to the water stress of the plant. This is done
   for now, with a very simple empirical approach. */

struct ws_str watstr(double precipit, double evapo, double cws, double soildepth, 
        double fieldc, double wiltp, double phi1, double phi2, 
        int soiltype, /* soil type indicator */ 
        int wsFun) /* flag for which water stress function to use */
{
    struct ws_str tmp;
    struct soilText_str soTexS;
    const double g = 9.8; /* m / s-2  ##  http://en.wikipedia.org/wiki/Standard_gravity */
    /* Variables */
    double precipM;
    /*    cws is current water status, which is normally in the wiltp-satur range */
    /*    aw available water (full profile) */
    double aw, awc; 
    double drainage = 0.0;
    double K_psim, J_w;
    double pawha, Newpawha, npaw;
    double runoff = 0.0;
    /* variable needed for calculation of water stress*/
    double wsPhoto = 0.0, wsSpleaf = 0.0, phi10;
    double slp = 0.0, intcpt = 0.0, theta = 0.0; 
    double Nleach = 0.0;
    /* Nleach is the NO3 leached and Ts is the sand content of the soil*/
    double theta_s; /* This is the saturated soil water content. Larger than FieldC. */

    /* Specify the soil type */
    soTexS = soilTchoose(soiltype);

    if(fieldc < 0) {
        fieldc = soTexS.fieldc;
    }
    if(wiltp < 0) {
        wiltp = soTexS.wiltp;
    }

    theta_s = soTexS.satur;

    /* unit conversion for precip */
    precipM = precipit * 1e-3; /* convert precip in mm to m*/


    aw = precipM + cws * soildepth; /* aw in meters */
    aw = aw / soildepth; /* available water in the wiltp-satur range */

    if(aw > theta_s) { 
        runoff = (aw - theta_s) * soildepth; /* This is in meters */ 
        /* Here runoff is interpreted as water content exceeding saturation level */
        /* Need to convert to units used in the Parton et al 1988 paper. */
        /* The data comes in mm/hr and it needs to be in cm/month */
        // runoff2 = runoff * 0.10 * (1/24*30); set but not used
        Nleach = runoff /18 * (0.2 + 0.7 * soTexS.sand);
        aw = theta_s;
    }

    /* plant available water per ha (pawha) */
    pawha = (aw - wiltp) * soildepth * 1e4;
    /* The density of water is 998.2 kg/m3 at 20 degrees Celsius */
    /* or 0.9982 Mg/m3 */
    /* pawha is plant available water (m3) per hectare */
    /* evapo is demanded water (Mg) per hectare */

    Newpawha = pawha - evapo / 0.9982; 

    /*  Here both are in m3 of water per ha-1 so this */
    /*  subtraction should be correct */
    /* go back to original units of water in the profile */

    npaw = Newpawha * 1e-4 / soildepth; 

    /* If demand exceeds supply the crop is getting close to wilting point 
       and transpiration will be over estimated. In this one layer model though
       the crop is practically dead */
    if(npaw < 0) { npaw = 0.0; }

    awc = npaw + wiltp;

    /* Calculating the soil water potential based on equations from Norman and Campbell */
    /* tmp.psim = soTexS.air_entry * pow((awc/soTexS.fieldc*1.1),-soTexS.b); */
    /* New version of the soil water potential is based on
     * "Dynamic Simulation of Water Deficit Effects upon Maize
     * Yield" R. F. Grant Agricultural Systems. 33(1990) 13-39. */
    tmp.psim = -exp(log(0.033) + ((log(fieldc) - log(awc))/(log(fieldc) - log(wiltp)) * (log(1.5) - log(0.033)))) * 1e3; /* This last term converts from MPa to kPa */

    /* This is drainage */
    if(awc > fieldc) {
        K_psim = soTexS.Ks * pow((soTexS.air_entry/tmp.psim),2+3/soTexS.b); /* This is hydraulic conductivity */
        J_w = -K_psim * (-tmp.psim/(soildepth*0.5)) - g * K_psim; /*  Campbell, pg 129 do not ignore the graviational effect. I multiply soil depth by 0.5 to calculate the average depth*/
        drainage = J_w * 3600 * 0.9982 * 1e-3; /* This is flow in m3 / (m^2 * hr). */
        awc = awc + drainage / soildepth;
    }

    /* three different type of equations for modeling the effect of water stress on vmax and leaf area expansion. 
       The equation for leaf area expansion is more severe than the one for vmax. */
    if(wsFun == 0) { /* linear */
        slp = 1/(fieldc - wiltp);
        intcpt = 1 - fieldc * slp;
        wsPhoto = slp * awc + intcpt;
    } else if(wsFun == 1) {
        phi10 = (fieldc + wiltp)/2;
        wsPhoto = 1/(1 + exp((phi10 - awc)/ phi1));
    } else if(wsFun == 2) {
        slp = (1 - wiltp)/(fieldc - wiltp);
        intcpt = 1 - fieldc * slp;
        theta = slp * awc + intcpt;
        wsPhoto = (1 - exp(-2.5 * (theta - wiltp)/(1 - wiltp))) / (1 - exp(-2.5));
    } else if(wsFun == 3) {
        wsPhoto = 1;
    }

    if(wsPhoto <= 0 )
        wsPhoto = 1e-10; /* This can be mathematically lower than zero in some cases but I should prevent that. */

    wsSpleaf = pow(awc,phi2) * 1/pow(fieldc,phi2); 
    if(wsFun == 3) { 
        wsSpleaf = 1;
    }

    /* Apparently wsPhoto and wsSpleaf can be greater than 1 */
    if(wsPhoto > 1) wsPhoto = 1;
    if(wsSpleaf > 1) wsSpleaf = 1;

    /* returning the structure*/
    tmp.rcoefPhoto = wsPhoto;
    tmp.awc = awc;
    tmp.runoff = runoff;
    tmp.Nleach = Nleach;
    tmp.rcoefSpleaf = wsSpleaf;
    return(tmp);
}

/* Function to simulate the multilayer behavior of soil water. In the
   future this could be coupled with Campbell (BASIC) ideas to
   esitmate water potential. */
struct soilML_str soilML(double precipit, double transp, double *cws, double soildepth, double *depths,
        double fieldc, double wiltp, double phi1, double phi2, struct soilText_str soTexS, int wsFun,
        int layers, double rootDB, double LAI, double k, double AirTemp, double IRad, double winds,
        double RelH, int hydrDist, double rfl, double rsec, double rsdf)
{

    struct rd_str root_distribution;
    struct soilML_str tmp;
    /* Constant */
    const double g = 9.8; /* m / s-2  ##  http://en.wikipedia.org/wiki/Standard_gravity */
    /* Variables */
    double waterIn, oldWaterIn = 0.0;
    /* Here is a convention aw is available water in volume and awc
       is available water content as a fraction of the soil section being investigated.
       paw is plant available water aw - wiltp */
    double aw, awc, awc2;
    double drainage = 0.0;
    double pawha, Newpawha;
    double wsPhoto = 0.0, wsSpleaf = 0.0, phi10;
    double wsPhotoCol = 0.0, wsSpleafCol = 0.0;
    double slp = 0.0, intcpt = 0.0, theta = 0.0; 
    double Nleach = 0.0;
    double layerDepth;
    double diffw;
    double rootATdepth, rootDepth;
    double EvapoTra = 0.0, oldEvapoTra = 0.0, Sevap = 0.0, Ctransp = 0.0;
    double psim1 = 0.0, psim2 = 0.0, K_psim = 0.0, J_w = 0.0, dPsim = 0.0;
    double theta_s; /* This is the saturated soil water content. Larger than FieldC. */

    /* rooting depth */
    /* Crude empirical relationship between root biomass and rooting depth*/
    rootDepth = rootDB * rsdf;
    if(rootDepth > soildepth) rootDepth = soildepth;

    root_distribution = rootDist(layers,rootDepth,&depths[0],rfl);

    /* Specify the soil type */

    if(fieldc < 0) {
        fieldc = soTexS.fieldc;
    }
    if(wiltp < 0) {
        wiltp = soTexS.wiltp;
    }

    theta_s = soTexS.satur;

    /* unit conversion for precip */
    waterIn = precipit * 1e-3; /* convert precip in mm to m*/

    for(int i = layers - 1; i >= 0; --i) { /* The counter, i, decreases because I increase the water content due to precipitation in the last layer first*/

        /* This supports unequal depths. */
        layerDepth = depths[i+1] - depths[i];

        if(hydrDist > 0) {
            /* For this section see Campbell and Norman "Environmental BioPhysics" Chapter 9*/
            /* First compute the matric potential */
            psim1 = soTexS.air_entry * pow((cws[i]/theta_s),-soTexS.b); /* This is matric potential of current layer */
            if(i > 0) {
                psim2 = soTexS.air_entry * pow((cws[i-1]/theta_s),-soTexS.b); /* This is matric potential of next layer */
                dPsim = psim1 - psim2;
                /* The substraction is from the layer i - (i-1). If this last term is positive then it will move upwards. If it is negative it will move downwards. Presumably this term is almost always positive. */
            } else {
                dPsim = 0;
            }
            K_psim = soTexS.Ks * pow((soTexS.air_entry/psim1),2+3/soTexS.b); /* This is hydraulic conductivity */
            J_w = K_psim * (dPsim/layerDepth) - g * K_psim; /*  Campbell, pg 129 do not ignore the graviational effect*/
            /* Notice that K_psim is positive because my
               reference system is reversed */
            /* This last result should be in kg/(m2 * s)*/
            J_w *= 3600 * 0.9882 * 1e-3; /* This is flow in m3 / (m^2 * hr). */
            /* Rprintf("J_w %.10f \n",J_w);  */
            if(i == (layers-1) && J_w < 0) {
                /* cws[i] = cws[i] + J_w /
                 * layerDepth; Although this
                 * should be done it drains
                 * the last layer too much.*/
                drainage += J_w;
            } else {
                if(i > 0) {
                    cws[i] = cws[i] -  J_w / layerDepth;
                    cws[i - 1] =  cws[i-1] +  J_w / layerDepth;
                } else {
                    cws[i] = cws[i] -  J_w / layerDepth;
                }
            }
        }

        if(cws[i] > theta_s) cws[i] = theta_s; 
        if(cws[i] < wiltp) cws[i] = wiltp; 

        aw = cws[i] * layerDepth;
        /* Available water (for this layer) is the current water status times the layer depth */

        if(waterIn > 0) {
            /* There is some rain. Need to add it.*/
            aw += waterIn / layers + oldWaterIn; /* They are both in meters so it works */
            /* Adding the same amount to water to each layer */
            /* In case there is overflow */
            diffw = fieldc * layerDepth - aw;

            if(diffw < 0) {
                /* This means that precipitation exceeded the capacity of the first layer */
                /* Save this amount of water for the next layer */
                oldWaterIn = -diffw;
                aw = fieldc * layerDepth;
            } else {
                oldWaterIn = 0.0;
            }
        }

        /* Root Biomass */
        rootATdepth = rootDB * root_distribution.rootDist[i];
        tmp.rootDist[i] = rootATdepth;
        /* Plant available water is only between current water status and permanent wilting point */
        /* Plant available water */
        pawha = (aw - wiltp * layerDepth) * 1e4;
        if(pawha < 0) pawha = 0; 

        if(i == 0) {
            /* Only the first layer is affected by soil evaporation */
            awc2 = aw / layerDepth;
            /* SoilEvapo function needs soil water content  */
            Sevap = SoilEvapo(LAI,k,AirTemp,IRad,awc2,fieldc,wiltp,winds,RelH,rsec);
            /* I assume that crop transpiration is distributed simlarly to
               root density.  In other words the crop takes up water proportionally
               to the amount of root in each respective layer.*/
            Ctransp = transp*root_distribution.rootDist[0];
            EvapoTra = Ctransp + Sevap;
            Newpawha = pawha - EvapoTra / 0.9982; /* See the watstr function for this last number 0.9882 */
            /* The first term in the rhs pawha is the m3 of water available in this layer.
               EvapoTra is the Mg H2O ha-1 of transpired and evaporated water. 1/0.9882 converts from Mg to m3 */
        } else {
            Ctransp = transp*root_distribution.rootDist[i];
            EvapoTra = Ctransp;
            Newpawha = pawha - (EvapoTra + oldEvapoTra);
        }

        if(Newpawha < 0) {
            /* If the Demand is not satisfied by this layer. This will be stored and added to subsequent layers*/
            oldEvapoTra = -Newpawha;
            aw = wiltp * layerDepth; 
        }

        awc = Newpawha / 1e4 / layerDepth + wiltp;   

        /* This might look like a weird place to populate the structure, but is more convenient*/
        tmp.cws[i] = awc;
        tmp.hourlyWflux[i] =J_w;

    /* three different type of equations for modeling the effect of water stress on vmax and leaf area expansion. 
       The equation for leaf area expansion is more severe than the one for vmax. */
        if(wsFun == 0) { /* linear */
            slp = 1/(fieldc - wiltp);
            intcpt = 1 - fieldc * slp;
            wsPhoto = slp * awc + intcpt;
        } else if(wsFun == 1) {
            phi10 = (fieldc + wiltp)/2;
            wsPhoto = 1/(1 + exp((phi10 - awc)/ phi1));
        } else if(wsFun == 2) {
            slp = (1 - wiltp)/(fieldc - wiltp);
            intcpt = 1 - fieldc * slp;
            theta = slp * awc + intcpt;
            wsPhoto = (1 - exp(-2.5 * (theta - wiltp)/(1 - wiltp))) / (1 - exp(-2.5));
        } else if(wsFun == 3) {
            wsPhoto = 1;
        }

        if(wsPhoto <= 0 )
            wsPhoto = 1e-10; /* This can be mathematically lower than zero in some cases but I should prevent that. */

        wsPhotoCol += wsPhoto;

        wsSpleaf = pow(awc,phi2) * 1/pow(fieldc,phi2); 
        if(wsFun == 3) { 
            wsSpleaf = 1;
        }
        wsSpleafCol += wsSpleaf;

    }

    if(waterIn > 0) { 
        drainage = waterIn;
        /* Need to convert to units used in the Parton et al 1988 paper. */
        /* The data comes in mm/hr and it needs to be in cm/month */
        Nleach = drainage * 0.1 * (1/24*30) / (18 * (0.2 + 0.7 * soTexS.sand));
    }

    /* Apparently wsPhoto and wsSpleaf can be greater than 1 */
    if(wsPhoto > 1) wsPhoto = 1;
    if(wsSpleaf > 1) wsSpleaf = 1;

    /* returning the structure */
    tmp.rcoefPhoto = (wsPhotoCol/layers);
    tmp.drainage = drainage;
    tmp.Nleach = Nleach;
    tmp.rcoefSpleaf = (wsSpleafCol/layers);
    tmp.SoilEvapo = Sevap;

    return(tmp);
}

/* Respiration. It is assumed that some of the energy produced by the
   plant has to be used in basic tissue maintenance. This is handled
   quite empirically by some relationships developed by McCree (1970)
   and Penning de Vries (1972) */

double resp(double comp, double mrc, double temp) {

    double ans;

    ans = comp *  (1 - (mrc * pow(2,(temp/10.0))));

    if(ans <0) ans = 0;

    return(ans);

}

/* Function to select the correct dry biomass partitioning coefficients */
/* It should take a vector of size 24 as an argument and return a structure with four numbers */

struct dbp_str sel_dbp_coef(double coefs[25], double TherPrds[6], double TherTime)
{

    struct dbp_str tmp;

    tmp.kLeaf = 0.0;
    tmp.kStem = 0.0;
    tmp.kRoot = 0.0;
    tmp.kRhiz = 0.0;
    tmp.kGrain = 0.0; /* kGrain is always zero except for the last thermal period */

    if(TherTime < TherPrds[0])
    {
        tmp.kStem = coefs[0];
        tmp.kLeaf = coefs[1];
        tmp.kRoot = coefs[2];
        tmp.kRhiz = coefs[3];

    } else if( TherTime < TherPrds[1] )
    {
        tmp.kStem = coefs[4];
        tmp.kLeaf = coefs[5];
        tmp.kRoot = coefs[6];
        tmp.kRhiz = coefs[7];

    } else if( TherTime < TherPrds[2])
    {
        tmp.kStem = coefs[8];
        tmp.kLeaf = coefs[9];
        tmp.kRoot = coefs[10];
        tmp.kRhiz = coefs[11];

    } else if(TherTime < TherPrds[3])
    {
        tmp.kStem = coefs[12];
        tmp.kLeaf = coefs[13];
        tmp.kRoot = coefs[14];
        tmp.kRhiz = coefs[15];

    } else if(TherTime < TherPrds[4])
    {
        tmp.kStem = coefs[16];
        tmp.kLeaf = coefs[17];
        tmp.kRoot = coefs[18];
        tmp.kRhiz = coefs[19];

    } else {

        tmp.kStem = coefs[20];
        tmp.kLeaf = coefs[21];
        tmp.kRoot = coefs[22];
        tmp.kRhiz = coefs[23];
        tmp.kGrain = coefs[24];

    }
    return(tmp);
}

struct seqRD_str seqRootDepth(double to, int lengthOut ) {

    struct seqRD_str tmp;
    int i;
    double by;

    /* This is because in this case from is always zero */
    by = to / lengthOut;

    for(i=0;i<=lengthOut;i++) {
        tmp.rootDepths[i] = i * by;
    }
    return(tmp);
}


struct rd_str rootDist(int layer, double rootDepth, double *depthsp, double rfl)
{
    struct rd_str tmp;  
    int i, j, k;
    double layerDepth = 0.0;
    double CumLayerDepth = 0.0;
    double CumRootDist = 1.0;
    double rootDist[layer];
    double ca = 0.0, a = 0.0;

    for(i=0;i<layer;i++) {

        if(i == 0) {
            layerDepth = depthsp[1];
        } else {
            layerDepth = depthsp[i] - depthsp[i-1];
        }

        CumLayerDepth += layerDepth;

        if(rootDepth > CumLayerDepth) {
            CumRootDist++;
        }
    }

    for(j=0;j<layer;j++) {
        if(j < CumRootDist) { 
            a = dpois(j+1,CumRootDist*rfl,0);
            rootDist[j] = a;
            ca += a;
        } else {
            rootDist[j] = 0;
        }
    }

    for(k=0;k<layer;k++) {
        tmp.rootDist[k] = rootDist[k] / ca; 
    }
    return(tmp);
}


struct soilText_str soilTchoose(int soiltype)
{
    /* This function is based on Campbell and Norman.
       Introduction to Environmental Biophysics. pg 130. */
    /* bulk density values are taken from function getsoilprop.c from Melanie (Colorado) */
    struct soilText_str tmp;

    tmp.silt = 0;
    tmp.clay = 0;
    tmp.sand = 0;
    tmp.air_entry = 0;
    tmp.b = 0;
    tmp.Ks = 0;
    tmp.satur = 0;
    tmp.fieldc = 0;
    tmp.wiltp = 0;
    tmp.bulkd= 0.0;
    if(soiltype == 0) {
        /* sand soil */
        tmp.silt = 0.05;
        tmp.clay = 0.03;
        tmp.sand = 0.92;
        tmp.air_entry = -0.7;
        tmp.b = 1.7;
        tmp.Ks = 5.8e-3;
        tmp.satur = 0.87;
        tmp.fieldc = 0.09;
        tmp.wiltp = 0.03;
        tmp.bulkd= 0.01; // This value is zero/undefined for sandy soil I am assigning a low value [see getsoilprop.c]
    } else if(soiltype == 1) {
        /* loamy sand */
        tmp.silt = 0.12;
        tmp.clay = 0.07;
        tmp.sand = 0.81;
        tmp.air_entry = -0.9;
        tmp.b = 2.1;
        tmp.Ks = 1.7e-3;
        tmp.satur = 0.72;
        tmp.fieldc = 0.13;
        tmp.wiltp = 0.06;
        tmp.bulkd= 1.55;
    } else if(soiltype == 2) {
        /* sandy loam */
        tmp.silt = 0.25;
        tmp.clay = 0.10;
        tmp.sand = 0.65;
        tmp.air_entry = -1.5;
        tmp.b = 3.1;
        tmp.Ks = 7.2e-4;
        tmp.satur = 0.57;
        tmp.fieldc = 0.21;
        tmp.wiltp = 0.10;
        tmp.bulkd= 1.50;
    } else if(soiltype == 3) {
        /* loam */
        tmp.silt = 0.40;
        tmp.clay = 0.18;
        tmp.sand = 0.52;
        tmp.air_entry = -1.1;
        tmp.b = 4.5;
        tmp.Ks = 3.7e-4;
        tmp.satur = 0.57;
        tmp.fieldc = 0.27;
        tmp.wiltp = 0.12;
        tmp.bulkd= 1.43;
    } else if(soiltype == 4) {
        /* silt loam */
        tmp.silt = 0.65;
        tmp.clay = 0.15;
        tmp.sand = 0.20;
        tmp.air_entry = -2.1;
        tmp.b = 4.7;
        tmp.Ks = 1.9e-4;
        tmp.satur = 0.59;
        tmp.fieldc = 0.33;
        tmp.wiltp = 0.13;
        tmp.bulkd= 1.36;
    } else if(soiltype == 5) {
        /* sandy clay loam */
        tmp.silt = 0.13;
        tmp.clay = 0.27;
        tmp.sand = 0.60;
        tmp.air_entry = -2.8;
        tmp.b = 4;
        tmp.Ks = 1.2e-4;
        tmp.satur = 0.48;
        tmp.fieldc = 0.26;
        tmp.wiltp = 0.15;
        tmp.bulkd= 1.39;
    } else if(soiltype == 6) {
        /* clay loam */
        tmp.silt = 0.34;
        tmp.clay = 0.34;
        tmp.sand = 0.32;
        tmp.air_entry = -2.6;
        tmp.b = 5.2;
        tmp.Ks = 6.4e-5;
        tmp.satur = 0.52;
        tmp.fieldc = 0.32;
        tmp.wiltp = 0.20;
        tmp.bulkd= 1.35;
    } else if(soiltype == 7) {
        /* silty clay loam */
        tmp.silt = 0.58;
        tmp.clay = 0.33;
        tmp.sand = 0.09;
        tmp.air_entry = -3.3;
        tmp.b = 6.6;
        tmp.Ks = 4.2e-5;
        tmp.satur = 0.52;
        tmp.fieldc = 0.37;
        tmp.wiltp = 0.21; /* Correction from the book from here http://www.public.iastate.edu/~bkh/teaching/505/norman_book_corrections.pdf */
        tmp.bulkd= 1.24;
    } else if(soiltype == 8) {
        /* sandy clay */
        tmp.silt = 0.07;
        tmp.clay = 0.40;
        tmp.sand = 0.53;
        tmp.air_entry = -2.9;
        tmp.b = 6;
        tmp.Ks = 3.3e-5;
        tmp.satur = 0.51;
        tmp.fieldc = 0.34;
        tmp.wiltp = 0.24;
        tmp.bulkd= 1.30;

    } else if(soiltype == 9) {
        /* silty clay */
        tmp.silt = 0.45;
        tmp.clay = 0.45;
        tmp.sand = 0.10;
        tmp.air_entry = -3.4;
        tmp.b = 7.9;
        tmp.Ks = 2.5e-5;
        tmp.satur = 0.52;
        tmp.fieldc = 0.39;
        tmp.wiltp = 0.25;
        tmp.bulkd= 1.28;
    } else if(soiltype == 10) {
        /* clay */
        tmp.silt = 0.20;
        tmp.clay = 0.60;
        tmp.sand = 0.20;
        tmp.air_entry = -3.7;
        tmp.b = 7.6;
        tmp.Ks = 1.7e-5;
        tmp.satur = 0.53;
        tmp.fieldc = 0.4;
        tmp.wiltp = 0.27;
        tmp.bulkd= 1.19;
    }
    return(tmp);
}

void cropcent_dbp(double coefs[25],double TherPrds[6], double TherTime, struct crop_phenology *cropdbp)

    // struct cropcent_phenolohy cropcent_dbp(double coefs[25], double TherPrds[6], double TherTime)

{     
    struct dbp_str tmp;
    // call old biomass partiitoning coefficient function
    tmp =sel_dbp_coef(coefs, TherPrds, TherTime);
    //assing it to the relevant sub structure of new cropdbp, to be retirned back to the main program
    cropdbp->DBP=tmp;
    // Here I must define what are CN ratios of different plant components under excess N fertilization


    // I can leave it empty for now
    return;
}

void initialize_biogro_results(struct BioGro_results_str *results, int soil_layers, int vector_size)
{
	int i;
	for (i = 0; i < vector_size; i++) {
		results->day_of_year[i] = 0;
		results->hour[i] = 0;
		results->CanopyAssim[i] = 0;
		results->canopy_transpiration[i] = 0;
		results->Leafy[i] = 0;
		results->Stemy[i] = 0;
		results->Rooty[i] = 0;
		results->Rhizomey[i] = 0;
		results->Grainy[i] = 0;
		results->LAIc[i] = 0;
		results->thermal_time[i] = 0;
		results->soil_water_content[i] = 0;
		results->stomata_cond_coefs[i] = 0;
		results->leaf_reduction_coefs[i] = 0;
		results->leaf_nitrogen[i] = 0;
		results->above_ground_litter[i] = 0;
		results->below_ground_litter[i] = 0;
		results->vmax[i] = 0;
		results->alpha[i] = 0;
		results->specific_leaf_area[i] = 0;
		results->min_nitro[i] = 0;
		results->respiration[i] = 0;
		results->soil_evaporation[i] = 0;
		results->leaf_psim[i] = 0;
	}
	results->psim = (double*)calloc(soil_layers * vector_size, sizeof(double));
	results->water_status = (double*)calloc(soil_layers * vector_size, sizeof(double));
	results->root_distribution = (double*)calloc(soil_layers * vector_size, sizeof(double));
}

void free_biogro_results(struct BioGro_results_str *results)
{
	free(results->psim);
	free(results->water_status);
	free(results->root_distribution);
	results->psim = NULL;
	results->water_status = NULL;
	results->root_distribution = NULL;
}

