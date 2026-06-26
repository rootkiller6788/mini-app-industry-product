#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "healthcare_core.h"
#include "clinical_dss.h"

int main(void) {
    printf("=== Clinical Workflow Demo ===\n");
    PatientRegistry *reg = reg_create();
    assert(reg);
    Patient *p = reg_add_patient(reg, "Jane Doe", 68, BLOOD_TYPE_O);
    assert(p);
    p->weight_kg = 72.0;
    p->height_cm = 163.0;
    double bmi = reg_bmi(p);
    printf("Patient: %s, Age %d, BMI %.1f (%s)\n",
           p->name, p->age, bmi, bmi_classification(bmi));
    reg_add_condition(reg, 1, "I48.91", "Atrial Fibrillation", SEVERITY_MODERATE);
    VitalSigns *vs = reg_record_vitals(reg, 1, 38.2, 110, 135, 85, 94);
    printf("NEWS2=%d (%s)\n", news2_score(vs),
           news2_is_urgent(vs) ? "URGENT" : "STABLE");
    printf("GCS(4,5,6)=%d %s\n", gcs_score(4,5,6),
           gcs_severity(gcs_score(4,5,6)));
    PERC_Input perc = {35, false, false, false, false, false, false, false};
    printf("PERC: %s\n", perc_recommendation(&perc));
    WellsDVT_Input wells = {false, false, false, true, false, false, false, false, true};
    printf("Wells DVT=%.0f: %s\n", wells_dvt_score(&wells),
           wells_dvt_recommendation(wells_dvt_score(&wells)));
    CURB65_Input curb = {true, 8.5, 32, 85, 55, 72};
    printf("CURB-65=%d mortality %.1f%%\n",
           curb65_score(&curb), curb65_mortality_pct(curb65_score(&curb)));
    printf("QTc(Bazett, QT=400, HR=72)=%.0fms\n", qtc_bazett(400, 72));
    CHA2DS2_VASc_Input cha = {75, true, true, true, true, false, false};
    CHA2DS2_VASc_Result cr = dss_cha2ds2_vasc(&cha);
    printf("CHA2DS2-VASc=%d stroke risk %.1f%%\n", cr.score, cr.stroke_risk_pct);
    HAS_BLED_Input hb = {true, false, false, true, false, true, false, true};
    HAS_BLED_Result hr = dss_has_bled(&hb);
    printf("HAS-BLED=%d bleed risk %.1f%%\n", hr.score, hr.major_bleeding_risk_pct);
    printf("D-dimer post-test: %.1f%%\n",
           dss_bayesian_posttest(0.05, 0.96, 0.55) * 100.0);
    printf("NNT(CER=15%%,EER=8%%)=%.0f\n", dss_nnt(0.15, 0.08));
    printf("NNH(EER=5%%,CER=2%%)=%.0f\n", dss_nnh(0.05, 0.02));
    printf("Odds Ratio(exposed=45:30, unexposed=15:80)=%.2f\n",
           dss_odds_ratio(45, 30, 15, 80));
    printf("Relative Risk(RR)=%.2f\n", dss_relative_risk(45, 1000, 15, 1000));
    DSSDrugDatabase db;
    dss_db_init(&db);
    dss_db_add_drug(&db, "Warfarin", "Coumadin", "B01AA03", 40.0, 10);
    dss_db_add_drug(&db, "Aspirin", "Bayer", "B01AC06", 0.5, 4000);
    dss_db_add_interaction(&db, "Warfarin", "Aspirin", DSS_INTERACTION_SEVERE,
        "Additive anticoagulation effect via platelet inhibition",
        "Avoid combination. Consider alternative.");
    printf("Warfarin+Aspirin severity: %d\n",
           (int)dss_check_interaction(&db, "Warfarin", "Aspirin"));
    printf("Therapeutic Index margin (TD50=10, ED50=0.5): %.1f\n",
           dss_therapeutic_index_margin(10.0, 0.5));
    AI_Triage_Input ai = {18, 72, 120, 36.8, 98, 35, 15};
    printf("AI Triage: %s\n",
           dss_clinical_urgency_label(dss_ai_triage_classify(&ai)));
    reg_destroy(reg);
    printf("All clinical workflow demos passed.\n");
    return 0;
}
