 /*
 *  soyCro/src/BioCro.c by Venkatraman Srinivasan
 */
#include <R.h>
#include <Rdefines.h>
#include <Rmath.h>
#include <string.h>
#include "soyGro.h"

SEXP RWrapper(SEXP RParameterFileName,SEXP RWeatherFileName,SEXP ROutputFolderName){
    // C local variables (Transfer R input variables)
    int FileLength=strlen(CHAR(STRING_ELT(RParameterFileName,0)));
    char *ParameterFileName; ParameterFileName = (char*)malloc(sizeof(char)*FileLength);
    strcpy(ParameterFileName,CHAR(STRING_ELT(RParameterFileName, 0)));
    FileLength=strlen(CHAR(STRING_ELT(RWeatherFileName,0)));
    char *WeatherFileName; WeatherFileName = (char*)malloc(sizeof(char)*FileLength);
    strcpy(WeatherFileName,CHAR(STRING_ELT(RWeatherFileName, 0)));
    FileLength=strlen(CHAR(STRING_ELT(ROutputFolderName,0)));
    char *OutputFolderName; OutputFolderName = (char*)malloc(sizeof(char)*FileLength);
    strcpy(OutputFolderName,CHAR(STRING_ELT(ROutputFolderName, 0)));
    // Perform computations
    soyGro(ParameterFileName,WeatherFileName,OutputFolderName);
    // R output variables (Transfer C local variables)
    SEXP Result; PROTECT(Result=allocVector(INTSXP,1));
    INTEGER(Result)[0] = FileLength;
    UNPROTECT(1);
return(Result);}
