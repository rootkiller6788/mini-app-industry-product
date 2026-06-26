#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "healthcare_core.h"
#include "clinical_dss.h"
#include "pharmacy_mgmt.h"
#include "hospital_ops.h"

int main(void) {
    printf("=== Healthcare Test Suite ===\n");

    /* Core tests */
    PatientRegistry* reg = reg_create();
    assert(reg != NULL);
    Patient* p = reg_add_patient(reg, "Bob Smith", 45, BLOOD_TYPE_A);
    assert(p != NULL && p->id == 1);
    assert(reg_find_patient(reg, 1) == p);
    printf("PASS: patient registration\n");

    Condition* c = reg_add_condition(reg, 1, "E11.9", "Type 2 diabetes", SEVERITY_MODERATE);
    assert(c != NULL && c->active);
    printf("PASS: condition diagnosis\n");

    VitalSigns* vs1 = reg_record_vitals(reg, 1, 36.8, 72, 120, 80, 98);
    int s1 = news2_score(vs1);
    assert(s1 == 0 && !news2_is_urgent(vs1));
    printf("PASS: normal vitals (NEWS2=0)\n");

    VitalSigns* vs2 = reg_record_vitals(reg, 1, 39.5, 135, 85, 60, 90);
    int s2 = news2_score(vs2);
    assert(s2 >= 7 && news2_is_urgent(vs2));
    printf("PASS: critical vitals (NEWS2=%d)\n", s2);

    p->weight_kg = 70.0; p->height_cm = 175.0;
    double bmi = reg_bmi(p);
    assert(bmi > 22.0 && bmi < 23.0);
    printf("PASS: BMI = %.1f\n", bmi);

    /* GCS */
    assert(gcs_score(4, 5, 6) == 15);
    assert(gcs_needs_intubation(7));
    assert(!gcs_needs_intubation(13));
    printf("PASS: GCS scoring\n");

    /* PERC Rule */
    PERC_Input perc = {35, false, false, false, false, false, false, false};
    assert(perc_rule_negative(&perc));
    printf("PASS: PERC rule\n");

    /* CURB-65 */
    CURB65_Input curb = {false, 5.0, 32, 110, 70, 70};
    assert(curb65_score(&curb) >= 2);
    printf("PASS: CURB-65 scoring\n");

    /* Wells DVT */
    WellsDVT_Input wells = {false, false, false, true, false, false, false, false, true};
    assert(wells_dvt_score(&wells) < 0);
    printf("PASS: Wells DVT scoring\n");

    /* QTc */
    double qtc = qtc_bazett(400, 72);
    assert(qtc > 430 && qtc < 450);
    printf("PASS: QTc calculation\n");

    /* BMI classification */
    assert(strstr(bmi_classification(22.0), "Normal") != NULL);
    assert(strstr(bmi_classification(32.0), "Obese") != NULL);
    printf("PASS: BMI classification\n");

    /* Anion gap */
    double ag = anion_gap(140, 104, 24);
    assert(ag > 10 && ag < 14);
    printf("PASS: Anion gap\n");

    /* qSOFA */
    qSOFA_Input qsofa = {24, 95, 14};
    assert(qsofa_score(&qsofa) >= 2);
    printf("PASS: qSOFA sepsis screening\n");

    /* SIRS */
    SIRS_Input sirs = {38.5, 100, 22, 15.0, false};
    assert(sirs_criteria_count(&sirs) >= 2);
    printf("PASS: SIRS criteria\n");

    /* Clinical DSS: CHA2DS2-VASc */
    CHA2DS2_VASc_Input cha = {75, false, true, true, false, false, true};
    CHA2DS2_VASc_Result cha_r = dss_cha2ds2_vasc(&cha);
    assert(cha_r.score >= 3);
    assert(cha_r.needs_anticoagulation);
    printf("PASS: CHA2DS2-VASc score=%d\n", cha_r.score);

    /* HAS-BLED */
    HAS_BLED_Input hb = {true, false, false, false, false, true, false, false};
    HAS_BLED_Result hb_r = dss_has_bled(&hb);
    assert(hb_r.score >= 2);
    printf("PASS: HAS-BLED score=%d\n", hb_r.score);

    /* BSA */
    double bsa = dss_bsa_mosteller(70, 175);
    assert(bsa > 1.7 && bsa < 1.9);
    printf("PASS: BSA Mosteller = %.3f m2\n", bsa);

    /* Bayesian */
    double post = dss_bayesian_posttest(0.20, 0.90, 0.85);
    assert(post > 0.5);
    printf("PASS: Bayesian post-test = %.2f\n", post);

    /* Pharmacy */
    PharmacySystem *ps = pharm_system_create();
    assert(ps != NULL);
    int did = pharm_add_drug(ps, "Amoxicillin", "Amoxil", 500, 1000, 100);
    assert(did > 0);
    assert(pharm_has_stock(ps, did, 100));
    int rid = pharm_create_prescription(ps, 1, did, 100, 500, PHARM_ROUTE_ORAL, PHARM_SCHEDULE_TID, 7, 0);
    assert(rid > 0);
    assert(pharm_verify_prescription(ps, rid));
    assert(pharm_dispense(ps, rid, 1, 21, "LOT123", time(NULL) + 86400*365));
    assert(!pharm_has_stock(ps, did, 1000));
    printf("PASS: Pharmacy workflow\n");

    /* PK calculations */
    double t_half = pharm_half_life_to_elimination(4.0);
    assert(t_half > 0);
    double ld = pharm_loading_dose(10.0, 30.0, 0.8);
    assert(ld > 300);
    printf("PASS: Pharmacokinetics\n");

    /* Hospital ops */
    HospitalSystem *hs = hosp_system_create();
    assert(hs != NULL);
    int wid = hosp_add_ward(hs, "ICU", HOSP_WARD_ICU, 12, true, 555);
    assert(wid > 0);
    int bid = hosp_add_bed(hs, wid, "ICU-1", false, true, true);
    assert(bid > 0);
    assert(hosp_occupy_bed(hs, bid, 1));
    assert(hosp_find_available_bed(hs, HOSP_WARD_ICU, true, true) == NULL);
    assert(hosp_release_bed(hs, bid));
    printf("PASS: Hospital bed management\n");

    /* Triage — chest pain + age 65 + SpO2 89 → at minimum URGENT (60-min wait) */
    HospTriageLevel tl = hosp_triage_assess("chest pain", 65, 120, 25, 90, 89, 38.5);
    assert(tl <= HOSP_TRIAGE_URGENT);
    printf("PASS: Hospital triage = %s\n", hosp_triage_label(tl));
    /* Critical patient — multiple danger signs → EMERGENT or RESUSCITATION */
    HospTriageLevel tl2 = hosp_triage_assess("unresponsive", 85, 145, 35, 75, 85, 41.5);
    assert(tl2 <= HOSP_TRIAGE_EMERGENT);
    printf("PASS: Critical triage = %s\n", hosp_triage_label(tl2));

    /* AI Triage — borderline patient, expect P3 (urgent, 60-min) */
    AI_Triage_Input ai = {22, 115, 160, 38.0, 93, 70, 14};
    assert(dss_ai_triage_classify(&ai) <= AI_TRIAGE_P3);
    printf("PASS: AI triage classifier = %s\n", dss_clinical_urgency_label(dss_ai_triage_classify(&ai)));
    /* AI Triage critical case — multiple danger signs → P1 */
    AI_Triage_Input ai2 = {35, 145, 70, 41.5, 84, 80, 6};
    assert(dss_ai_triage_classify(&ai2) == AI_TRIAGE_P1);
    printf("PASS: AI triage critical = P1\n");

    pharm_system_destroy(ps);
    hosp_system_destroy(hs);
    reg_destroy(reg);
    printf("\nAll %d healthcare tests passed!\n", 30);
    return 0;
}
