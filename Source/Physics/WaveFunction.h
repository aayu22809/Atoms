#ifndef WaveFunction_h
#define WaveFunction_h

#include <cmath>
#include <cstdlib>

double computeRadial(int n, int l, double r);
double computeLegendre(int l, int absM, double cosTheta);
double probabilityDensity(int n, int l, int m, double r, double theta);

#endif
