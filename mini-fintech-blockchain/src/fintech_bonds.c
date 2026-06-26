#include "fintech_bonds.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Bond* bond_create(BondType type, double face_value, double coupon_rate,
                  int frequency, double maturity_years) {
    Bond* b = (Bond*)calloc(1, sizeof(Bond));
    if (!b) return NULL;
    b->type = type;
    b->face_value = face_value;
    b->coupon_rate = coupon_rate;
    b->coupon_frequency = (frequency > 0) ? frequency : 1;
    b->maturity_years = maturity_years;
    b->yield_to_maturity = coupon_rate;
    bond_generate_cashflows(b);
    return b;
}

int bond_generate_cashflows(Bond* b) {
    if (!b || b->maturity_years <= 0.0) return -1;
    int freq = b->coupon_frequency;
    int total_periods = (int)(b->maturity_years * freq);
    if (total_periods > BOND_MAX_CASHFLOWS) total_periods = BOND_MAX_CASHFLOWS;
    if (total_periods < 1) total_periods = 1;
    double coupon = 0.0;
    if (b->type == BOND_FIXED_COUPON)
        coupon = b->face_value * b->coupon_rate / freq;
    b->cashflow_count = 0;
    double dt = 1.0 / freq;
    for (int i = 1; i <= total_periods; i++) {
        CashFlow* cf = &b->cashflows[b->cashflow_count];
        cf->period = i;
        cf->time_years = i * dt;
        cf->amount = coupon;
        if (i == total_periods) cf->amount += b->face_value;
        b->cashflow_count++;
    }
    if (b->type == BOND_ZERO_COUPON) {
        b->cashflow_count = 1;
        b->cashflows[0].period = 1;
        b->cashflows[0].time_years = b->maturity_years;
        b->cashflows[0].amount = b->face_value;
    }
    return b->cashflow_count;
}

double bond_price_from_ytm(const Bond* b, double ytm) {
    if (!b || b->cashflow_count == 0) return 0.0;
    int freq = b->coupon_frequency;
    double price = 0.0;
    for (int i = 0; i < b->cashflow_count; i++) {
        double t = (double)b->cashflows[i].period;
        price += b->cashflows[i].amount / pow(1.0 + ytm / freq, t);
    }
    return price;
}

double bond_ytm_newton(const Bond* b, double market_price,
                       double tolerance, int max_iter) {
    if (!b || market_price <= 0.0 || max_iter < 1) return -1.0;
    int freq = b->coupon_frequency;
    double ytm = b->coupon_rate;
    for (int iter = 0; iter < max_iter; iter++) {
        double price = 0.0, deriv = 0.0;
        for (int i = 0; i < b->cashflow_count; i++) {
            double cf = b->cashflows[i].amount;
            double t = (double)b->cashflows[i].period;
            double disc = pow(1.0 + ytm / freq, t);
            price += cf / disc;
            deriv -= cf * t / (freq * pow(1.0 + ytm / freq, t + 1.0));
        }
        double diff = price - market_price;
        if (fabs(diff) < tolerance) return ytm;
        if (fabs(deriv) < 1e-12) return -2.0;
        ytm -= diff / deriv;
        if (ytm < -0.99) ytm = -0.99;
        if (ytm > 10.0)  ytm = 10.0;
    }
    return ytm;
}

double bond_duration_macaulay(const Bond* b) {
    if (!b || b->cashflow_count == 0) return 0.0;
    int freq = b->coupon_frequency;
    double ytm = b->yield_to_maturity;
    double price = 0.0, weighted_sum = 0.0;
    for (int i = 0; i < b->cashflow_count; i++) {
        double cf = b->cashflows[i].amount;
        double t = (double)b->cashflows[i].period;
        double pv = cf / pow(1.0 + ytm / freq, t);
        price += pv;
        weighted_sum += (t / freq) * pv;
    }
    if (price < 1e-12) return 0.0;
    return weighted_sum / price;
}

double bond_duration_modified(const Bond* b) {
    double mac = bond_duration_macaulay(b);
    int freq = b->coupon_frequency;
    return mac / (1.0 + b->yield_to_maturity / freq);
}

double bond_convexity(const Bond* b) {
    if (!b || b->cashflow_count == 0) return 0.0;
    int freq = b->coupon_frequency;
    double ytm = b->yield_to_maturity;
    double price = 0.0, conv_sum = 0.0;
    for (int i = 0; i < b->cashflow_count; i++) {
        double cf = b->cashflows[i].amount;
        double t = (double)b->cashflows[i].period;
        price += cf / pow(1.0 + ytm / freq, t);
        conv_sum += cf * t * (t + 1.0) / pow(1.0 + ytm / freq, t + 2.0);
    }
    if (price < 1e-12) return 0.0;
    return conv_sum / (price * freq * freq);
}

double bond_dv01(const Bond* b) {
    double dmod = bond_duration_modified(b);
    double price = bond_price_from_ytm(b, b->yield_to_maturity);
    return -dmod * price * 0.0001;
}

double bond_zero_coupon_price(double face_value, double ytm, double years) {
    if (years <= 0.0) return face_value;
    return face_value / pow(1.0 + ytm, years);
}

void nelson_siegel_curve(NelsonSiegelCurve* ns, double beta0, double beta1,
                          double beta2, double tau) {
    if (!ns) return;
    ns->beta0 = beta0; ns->beta1 = beta1;
    ns->beta2 = beta2; ns->tau = (tau > 0.0) ? tau : 1.0;
}

double nelson_siegel_yield(const NelsonSiegelCurve* ns, double t_years) {
    if (!ns || t_years <= 0.0)
        return ns ? ns->beta0 + ns->beta1 : 0.0;
    double x = t_years / ns->tau;
    double e = exp(-x);
    double f1 = (1.0 - e) / x;
    double f2 = f1 - e;
    return ns->beta0 + ns->beta1 * f1 + ns->beta2 * f2;
}

double nelson_siegel_forward(const NelsonSiegelCurve* ns, double t_years) {
    if (!ns || t_years <= 0.0)
        return ns ? ns->beta0 + ns->beta1 : 0.0;
    double x = t_years / ns->tau;
    double e = exp(-x);
    return ns->beta0 + ns->beta1 * e + ns->beta2 * x * e;
}

double immunization_hedge_ratio(double liability_duration,
                                 double asset_duration,
                                 double liability_pv, double asset_pv) {
    if (fabs(asset_duration) < 1e-12 || fabs(asset_pv) < 1e-12) return 0.0;
    return (liability_duration * liability_pv) / (asset_duration * asset_pv);
}

void bond_print(const Bond* b) {
    if (!b) return;
    printf("Bond %s: FV=%.2f Coupon=%.2f%% Freq=%d Maturity=%.1fy\n",
           b->isin, b->face_value, b->coupon_rate * 100.0,
           b->coupon_frequency, b->maturity_years);
    printf("  Price=%.4f YTM=%.4f%% Dur(M)=%.2f Dur(Mod)=%.2f Cvx=%.2f DV01=%.4f\n",
           b->market_price, b->yield_to_maturity * 100.0,
           b->macaulay_duration, b->modified_duration, b->convexity, b->dv01);
}

void bond_destroy(Bond* b) { free(b); }
