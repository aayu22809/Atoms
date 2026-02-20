#include "WaveFunction.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double a0 = 1.0;

double computeRadial(int n, int l, double r) {
    double rho = 2.0 * r / (n * a0);
    int k = n - l - 1;
    int alpha = 2 * l + 1;

    double L;
    if (k == 0) {
        L = 1.0;
    } else if (k == 1) {
        L = 1.0 + alpha - rho;
    } else {
        double Lm2 = 1.0;
        double Lm1 = 1.0 + alpha - rho;
        for (int j = 2; j <= k; ++j) {
            L = ((2*j - 1 + alpha - rho) * Lm1 - (j - 1 + alpha) * Lm2) / j;
            Lm2 = Lm1;
            Lm1 = L;
        }
    }

    double norm = sqrt(pow(2.0 / (n * a0), 3) * tgamma(n - l) / (2.0 * n * tgamma(n + l + 1)));
    double R = norm * exp(-rho / 2.0) * pow(rho, l) * L;
    return R;
}

double computeLegendre(int l, int absM, double x) {
    double Pmm = 1.0;
    if (absM > 0) {
        double somx2 = sqrt((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        for (int j = 1; j <= absM; ++j) {
            Pmm *= -fact * somx2;
            fact += 2.0;
        }
    }
    if (l == absM) return Pmm;

    double Pm1m = x * (2 * absM + 1) * Pmm;
    if (l == absM + 1) return Pm1m;

    double Plm = 0.0;
    for (int ll = absM + 2; ll <= l; ++ll) {
        Plm = ((2 * ll - 1) * x * Pm1m - (ll + absM - 1) * Pmm) / (ll - absM);
        Pmm = Pm1m;
        Pm1m = Plm;
    }
    return Pm1m;
}

double probabilityDensity(int n, int l, int m, double r, double theta) {
    int absM = std::abs(m);
    double R = computeRadial(n, l, r);
    double Plm = computeLegendre(l, absM, cos(theta));
    double radial = R * R;
    double angular = Plm * Plm;
    return radial * angular;
}
