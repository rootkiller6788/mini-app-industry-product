#include "energy_core.h"
#include "energy_solar.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float energy_day_of_year(int year, int month, int day) {
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int doy = day;
    for (int m = 0; m < month - 1 && m < 12; m++) doy += dm[m];
    if (month > 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) doy++;
    return doy;
}
float energy_solar_declination(int day_of_year) {
    float b = 2.0f * M_PI * (float)(day_of_year - 1) / 365.0f;
    return 0.006918f - 0.399912f*cosf(b) + 0.070257f*sinf(b)
         - 0.006758f*cosf(2*b) + 0.000907f*sinf(2*b)
         - 0.002697f*cosf(3*b) + 0.001480f*sinf(3*b);
}
float energy_solar_hour_angle(float lon_d, float tz, int doy, float dh) {
    float lstm = 15.0f * tz;
    float b = 2.0f * M_PI * (float)(doy - 81) / 365.0f;
    float eot = 9.87f*sinf(2*b) - 7.53f*cosf(b) - 1.5f*sinf(b);
    float tcf = 4.0f*(lon_d - lstm) + eot;
    float lst = dh + tcf / 60.0f;
    float ha = (lst - 12.0f) * 15.0f;
    while (ha > 180.0f) ha -= 360.0f;
    while (ha < -180.0f) ha += 360.0f;
    return ha;
}
float energy_solar_sunrise_hour(float lat_d, float dec_d) {
    float lat = lat_d * M_PI / 180.0f, dec = dec_d;
    float cos_ha = -tanf(lat)*tanf(dec);
    if (cos_ha > 1.0f) return 0.0f;
    if (cos_ha < -1.0f) return 24.0f;
    return 12.0f - acosf(cos_ha) * 12.0f / M_PI;
}
energy_solar_geometry_t energy_solar_geometry(const energy_location_t* loc,
                                               int year, int month, int day,
                                               float dh) {
    energy_solar_geometry_t geo; memset(&geo,0,sizeof(geo));
    if (!loc) return geo;
    int doy = energy_day_of_year(year,month,day);
    geo.declination_deg = energy_solar_declination(doy) * 180.0f / M_PI;
    geo.hour_angle_deg = energy_solar_hour_angle(loc->longitude_deg,loc->timezone_offset_h,doy,dh);
    geo.elevation_deg = energy_solar_elevation(loc->latitude_deg,geo.declination_deg,geo.hour_angle_deg);
    geo.azimuth_deg = energy_solar_azimuth(loc->latitude_deg,geo.declination_deg,geo.hour_angle_deg,geo.elevation_deg);
    geo.sunrise_h = energy_solar_sunrise_hour(loc->latitude_deg,geo.declination_deg);
    geo.sunset_h = 24.0f - geo.sunrise_h;
    geo.day_length_h = geo.sunset_h - geo.sunrise_h;
    geo.extraterrestrial_w_m2 = energy_extraterrestrial_radiation(doy);
    return geo;
}

float energy_extraterrestrial_radiation(int doy) {
    float b = 2.0f*M_PI*(float)(doy-1)/365.0f;
    float ecc = 1.000110f + 0.034221f*cosf(b) + 0.001280f*sinf(b)
              + 0.000719f*cosf(2*b) + 0.000077f*sinf(2*b);
    return ENERGY_SOLAR_CONSTANT * ecc;
}
float energy_air_mass(float el_d, float alt_m) {
    if (el_d <= 0) return 38.0f;
    float el = el_d*M_PI/180.0f;
    float am = 1.0f/(sinf(el) + 0.50572f*powf(el_d+6.07995f,-1.6364f));
    float pr = expf(-alt_m/8435.0f);
    return am * pr;
}
float energy_clearsky_ghi(float el_d, float alt_m) {
    if (el_d <= 0) return 0;
    float am = energy_air_mass(el_d,alt_m);
    float el = el_d*M_PI/180.0f;
    float ghi = 1098.0f*sinf(el)*expf(-0.057f/(am>0.01f?am:0.01f));
    return ghi > 0 ? ghi : 0;
}

void energy_irradiance_decompose(energy_irradiance_t* irr, float ghi,
                                  const energy_solar_geometry_t* geo) {
    if (!irr) return; memset(irr,0,sizeof(energy_irradiance_t));
    irr->ghi_w_m2 = ghi > 0 ? ghi : 0;
    if (ghi <= 0 || !geo || geo->extraterrestrial_w_m2 <= 0) return;
    float etr_n = geo->extraterrestrial_w_m2*sinf(geo->elevation_deg*M_PI/180.0f);
    if (etr_n <= 0) return;
    float kt = ghi/etr_n;
    irr->clearness_index = kt < 0 ? 0 : (kt > 1.0f ? 1.0f : kt);
    float kd;
    if (kt <= 0.22f) kd = 1.0f - 0.09f*kt;
    else if (kt <= 0.80f) kd = 0.9511f - 0.1604f*kt + 4.388f*kt*kt - 16.638f*kt*kt*kt + 12.336f*kt*kt*kt*kt;
    else kd = 0.165f;
    irr->diffuse_fraction = kd;
    irr->dhi_w_m2 = ghi*kd;
    irr->dni_w_m2 = (ghi-irr->dhi_w_m2)/(sinf(geo->elevation_deg*M_PI/180.0f)+0.0001f);
    if (irr->dni_w_m2 < 0) irr->dni_w_m2 = 0;
}

float energy_irradiance_tilted(const energy_irradiance_t* irr,
                                const energy_solar_geometry_t* geo,
                                const energy_panel_orientation_t* orient) {
    if (!irr||!geo||!orient) return 0;
    float beta=orient->tilt_deg*M_PI/180.0f, gamma=orient->azimuth_deg*M_PI/180.0f;
    float el=geo->elevation_deg*M_PI/180.0f, az_s=geo->azimuth_deg*M_PI/180.0f;
    if (el<=0) return 0;
    float ct = sinf(el)*cosf(beta)+cosf(el)*sinf(beta)*cosf(az_s-gamma);
    if (ct<0) ct=0;
    float beam=irr->dni_w_m2*ct;
    float Ai=irr->dni_w_m2/(irr->dni_w_m2+irr->dhi_w_m2+0.001f);
    float f=sqrtf(irr->dni_w_m2/(irr->ghi_w_m2+0.001f));
    float Rb=ct/(sinf(el)+0.001f);
    float sd=irr->dhi_w_m2*((1.0f-Ai)*(1.0f+cosf(beta))/2.0f*(1.0f+f*sinf(beta/2)*sinf(beta/2)*sinf(beta/2))+Ai*Rb);
    float ground=irr->ghi_w_m2*orient->albedo*(1.0f-cosf(beta))/2.0f;
    return beam+sd+ground;
}

float energy_pv_cell_temperature(float t_amb, float g, float noct, float ws) {
    if (ws < 0.1f) ws = 1.0f;
    return t_amb + g/(noct+5.0f*ws)*(noct-20.0f)/800.0f*g/800.0f;
}
energy_pv_output_t energy_pv_output(const energy_pv_module_t* pv, float gti,
                                     float t_amb, float ws) {
    energy_pv_output_t out; memset(&out,0,sizeof(out));
    if (!pv || gti<=0) return out;
    float tc = energy_pv_cell_temperature(t_amb,gti,pv->noct_c,ws);
    out.cell_temp_c = tc;
    float gamma = pv->temp_coeff_p_pct_K * 0.01f;
    float power = pv->p_max_W*(gti/1000.0f)*(1.0f+gamma*(tc-25.0f));
    out.power_kw = power*0.001f;
    out.voltage_v = pv->v_mp_V*(1.0f+pv->temp_coeff_v_pct_K*0.01f*(tc-25.0f));
    out.current_a = power/(out.voltage_v>0?out.voltage_v:1.0f);
    out.efficiency_pct = power/(gti*pv->p_max_W/(pv->p_max_W>0?0.15f:0.15f))*15.0f;
    return out;
}

float energy_pv_current_A(float V, const energy_pv_module_t* pv, float G, float Tc) {
    if (!pv||G<=0) return 0;
    float vt=0.02585f*(Tc+273.15f)/298.15f;
    float iph=pv->i_sc_A*(G/1000.0f);
    float i0=pv->i_sc_A/(expf(pv->v_oc_V/(pv->ideality_factor*vt))-1.0f);
    float I=iph;
    for (int it=0;it<20;it++) {
        float vd=V+I*pv->series_resistance_ohm;
        float di=iph-i0*(expf(vd/(pv->ideality_factor*vt))-1.0f)-vd/pv->shunt_resistance_ohm-I;
        float didi=-i0*pv->series_resistance_ohm/(pv->ideality_factor*vt)*expf(vd/(pv->ideality_factor*vt))-pv->series_resistance_ohm/pv->shunt_resistance_ohm-1.0f;
        if (fabsf(didi)<1e-12f) break;
        float dI=di/didi; I-=dI;
        if (fabsf(dI)<1e-6f) break;
    }
    return I>0?I:0;
}
float energy_pv_max_power_point(const energy_pv_module_t* pv, float G, float Tc,
                                 float* v_mp, float* i_mp) {
    if (!pv||G<=0) { if(v_mp)*v_mp=0; if(i_mp)*i_mp=0; return 0; }
    float bp=0,bv=0,bi=0;
    for (float v=0.1f;v<pv->v_oc_V;v+=0.5f) {
        float i=energy_pv_current_A(v,pv,G,Tc), p=v*i;
        if (p>bp) { bp=p; bv=v; bi=i; }
    }
    if (v_mp)*v_mp=bv; if (i_mp)*i_mp=bi;
    return bp;
}

float energy_pv_degraded_power(const energy_pv_module_t* pv, int year) {
    if (!pv||year<0) return 0;
    float deg=1.0f-pv->degradation_first_year_pct*0.01f;
    for (int y=1;y<year;y++) deg*=(1.0f-pv->degradation_annual_pct*0.01f);
    return pv->p_max_W*deg;
}
float energy_pv_bifacial_gain(float gti_f, float alb, float tilt, float bif) {
    float ri=gti_f*alb*0.5f*(1.0f+cosf(tilt*M_PI/180.0f));
    return gti_f+ri*bif;
}
float energy_pv_soiling_loss(float bp, float sr, int dsc) {
    float dl=1.0f-sr*0.01f, f=powf(dl,(float)dsc);
    if (f<0.3f) f=0.3f;
    return bp*f;
}
float energy_pv_shading_loss(float up, float sf, float tf) {
    return up*(1.0f-sf*tf);
}
float energy_solar_thermal_power_kw(const energy_solar_thermal_collector_t* sc,
                                     float gti, float ta, float tfi) {
    if (!sc||gti<=0) return 0;
    float dT=tfi-ta;
    float eff=sc->f_ta_eta0-sc->f_ta_a1*dT/(gti+0.001f)-sc->f_ta_a2*dT*dT/(gti+0.001f);
    if (eff<0) eff=0;
    return gti*sc->aperture_area_m2*eff*0.001f;
}
float energy_solar_thermal_efficiency(const energy_solar_thermal_collector_t* sc,
                                       float dT, float gti) {
    if (!sc||gti<=0) return 0;
    return sc->f_ta_eta0-sc->f_ta_a1*dT/gti-sc->f_ta_a2*dT*dT/gti;
}
float energy_albedo_lookup(const char* st) {
    if (!st) return 0.2f;
    if (strstr(st,"snow")||strstr(st,"ice")) return 0.75f;
    if (strstr(st,"sand")||strstr(st,"desert")) return 0.40f;
    if (strstr(st,"grass")||strstr(st,"field")) return 0.25f;
    if (strstr(st,"asphalt")||strstr(st,"road")) return 0.12f;
    if (strstr(st,"water")||strstr(st,"ocean")) return 0.08f;
    return 0.2f;
}
int energy_is_daylight(const energy_solar_geometry_t* geo) {
    return geo&&geo->elevation_deg>0?1:0;
}

float energy_solar_elevation(float latitude_deg, float declination_deg,
                              float hour_angle_deg) {
    float lat_r = latitude_deg * M_PI / 180.0f;
    float dec_r = declination_deg * M_PI / 180.0f;
    float ha_r = hour_angle_deg * M_PI / 180.0f;
    float sin_elev = sinf(lat_r) * sinf(dec_r)
                   + cosf(lat_r) * cosf(dec_r) * cosf(ha_r);
    if (sin_elev > 1.0f) sin_elev = 1.0f;
    if (sin_elev < -1.0f) sin_elev = -1.0f;
    return asinf(sin_elev) * 180.0f / M_PI;
}

float energy_solar_azimuth(float latitude_deg, float declination_deg,
                            float hour_angle_deg, float elevation_deg) {
    float lat_r = latitude_deg * M_PI / 180.0f;
    float dec_r = declination_deg * M_PI / 180.0f;
    float ha_r = hour_angle_deg * M_PI / 180.0f;
    float elev_r = elevation_deg * M_PI / 180.0f;
    float cos_elev = cosf(elev_r);
    if (cos_elev < 1e-6f) return 180.0f;
    float sin_az = -cosf(dec_r) * sinf(ha_r) / cos_elev;
    float cos_az = (sinf(dec_r) - sinf(lat_r) * sinf(elev_r))
                  / (cosf(lat_r) * cos_elev);
    float az = atan2f(sin_az, cos_az) * 180.0f / M_PI;
    if (az < 0.0f) az += 360.0f;
    return az;
}
