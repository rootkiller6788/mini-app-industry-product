#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lab_analyzer.h"
#include "patient_monitor.h"

int main(void) {
    printf("=== Lab Analyzer & Patient Monitor Demo ===\n");

    /* Lab Catalog */
    LabTestCatalog *cat = lab_catalog_create();
    assert(cat);
    int n = lab_catalog_load_defaults(cat);
    printf("Lab catalog: %d tests loaded\n", n);

    /* Reference Range Evaluation */
    LabTest *glucose = lab_catalog_find(cat, LAB_CHEM_GLUCOSE);
    assert(glucose);
    printf("Glucose ref range: %.0f-%.0f %s\n",
           glucose->range.low, glucose->range.high, glucose->unit);

    LabResultFlag f1 = lab_evaluate_result(glucose, 95.0);
    printf("Glucose 95 mg/dL: %s (severity %d)\n",
           lab_flag_name(f1), lab_flag_severity(f1));

    LabResultFlag f2 = lab_evaluate_result(glucose, 45.0);
    printf("Glucose 45 mg/dL: %s (critical=%s panic=%s)\n",
           lab_flag_name(f2),
           lab_is_critical(f2) ? "YES" : "NO",
           lab_is_panic(f2) ? "YES" : "NO");

    LabResultFlag f3 = lab_evaluate_result(glucose, 500.0);
    printf("Glucose 500 mg/dL: %s\n", lab_flag_name(f3));

    /* Potassium */
    LabTest *k_test = lab_catalog_find(cat, LAB_CHEM_K);
    assert(k_test);
    printf("Potassium ref: %.1f-%.1f %s\n",
           k_test->range.low, k_test->range.high, k_test->unit);

    /* Troponin */
    LabTest *tni = lab_catalog_find(cat, LAB_CARDIAC_TNI);
    assert(tni);
    LabResultFlag tni_f = lab_evaluate_result(tni, 0.5);
    printf("Troponin I 0.5 ng/mL: %s\n", lab_flag_name(tni_f));

    /* Trend Analysis (Linear Regression) */
    LabHistory hist;
    memset(&hist, 0, sizeof(hist));
    hist.test_type = LAB_CHEM_CREAT;
    double cr_vals[] = {0.8, 0.9, 1.1, 1.3, 1.6};
    hist.count = 5;
    for (int i = 0; i < 5; i++) {
        hist.history[i].test_type = LAB_CHEM_CREAT;
        hist.history[i].value = cr_vals[i];
    }
    LabTrend trend = lab_compute_trend(&hist);
    printf("Creatinine trend: slope=%.4f, R^2=%.4f (n=%d)\n",
           trend.slope, trend.r_squared, trend.n_points);
    double next_val;
    if (lab_predict_next(&trend, &next_val) == 0)
        printf("  Predicted next value: %.2f mg/dL\n", next_val);

    /* Delta Check */
    LabResult prev = {LAB_CHEM_CREAT, 1.0, 0, 0, "", LAB_NORMAL, ""};
    LabResult curr = {LAB_CHEM_CREAT, 1.8, 0, 0, "", LAB_NORMAL, ""};
    DeltaCheck dc = lab_delta_check(&prev, &curr, 50.0);
    printf("Delta check: d=%.2f (%.0f%%) significant=%s increase=%s\n",
           dc.delta_value, dc.delta_percent,
           dc.significant ? "YES" : "NO",
           dc.increase ? "YES" : "NO");

    /* Panel Summary */
    LabResult panel[4] = {
        {LAB_CBC_WBC, 8.5, 0, 0, "", LAB_NORMAL, ""},
        {LAB_CHEM_GLUCOSE, 98.0, 0, 0, "", LAB_NORMAL, ""},
        {LAB_CHEM_K, 3.0, 0, 0, "", LAB_LOW, ""},
        {LAB_CHEM_NA, 125.0, 0, 0, "", LAB_CRITICAL_LOW, ""},
    };
    char summary[1024];
    lab_panel_summary(panel, 4, summary, sizeof(summary));
    printf("\n%s", summary);

    /* Special Calculations */
    printf("eGFR (Cr=1.1, 65M): %.0f mL/min\n",
           lab_calculate_egfr(1.1, 65, false, false).egfr);
    printf("Corrected Ca (Ca=8.0, Alb=3.0): %.2f mg/dL\n",
           lab_corrected_calcium(8.0, 3.0));
    printf("LDL (TC=220, HDL=50, TG=150): %.0f mg/dL\n",
           lab_ldl_cholesterol(220, 50, 150));
    printf("Serum Osmolality (Na=140, Glu=90, BUN=14): %.0f mOsm/kg\n",
           lab_serum_osmolality(140, 90, 14));
    printf("Anion Gap (Na=140, Cl=104, HCO3=24): %.0f mEq/L (elevated=%s)\n",
           lab_anion_gap(140, 104, 24),
           lab_is_anion_gap_elevated(lab_anion_gap(140, 104, 24)) ? "YES" : "NO");
    printf("MELD Score (Bili=3.0, INR=1.5, Cr=1.5): %.0f\n",
           lab_meld_score(3.0, 1.5, 1.5));
    printf("RPI (retic=5%%, HCT=30%%): %.2f\n", lab_rpi(5.0, 30.0));
    printf("Osmolar Gap (measured=310, calc=295): %.0f (elev=%s)\n",
           lab_osmolar_gap(310, lab_serum_osmolality(140, 90, 14)),
           lab_is_osmolar_gap_elevated(
               lab_osmolar_gap(310, lab_serum_osmolality(140, 90, 14))
           ) ? "YES" : "NO");

    /* Patient Monitoring */
    MonitorSystem *ms = monitor_create();
    assert(ms);
    monitor_add_patient(ms, "PT-001", "Alice Cooper", 72, false);
    monitor_add_patient(ms, "PT-002", "Bob Wilson", 45, true);

    monitor_record_vital(ms, "PT-001", PM_HR, 72);
    monitor_record_vital(ms, "PT-001", PM_SBP, 130);
    monitor_record_vital(ms, "PT-001", PM_DBP, 82);
    monitor_record_vital(ms, "PT-001", PM_RR, 16);
    monitor_record_vital(ms, "PT-001", PM_SPO2, 97);
    monitor_record_vital(ms, "PT-001", PM_TEMP, 36.8);
    monitor_record_vital(ms, "PT-001", PM_AVPU, 0);

    PatientVitals *pv = monitor_find_patient(ms, "PT-001");
    assert(pv);
    printf("PT-001 NEWS2=%d risk=%d\n",
           pv->last_news.news2_score, (int)pv->last_news.risk_level);

    PatientAlarm alarm = monitor_check_alarms(ms, "PT-001");
    printf("Alarm level: %s\n", alarm_level_name(alarm.level));
    printf("Needs rapid response: %s\n",
           monitor_needs_rapid_response(&pv->last_news) ? "YES" : "NO");

    char protocol[512];
    monitor_escalation_protocol(pv, protocol, sizeof(protocol));
    printf("Escalation protocol: %s\n", protocol);

    char report[2048];
    monitor_shift_report(ms, report, sizeof(report));
    printf("\n%s", report);

    PatientAlarm pending[8];
    int np = monitor_pending_alarms(ms, pending, 8);
    printf("Pending alarms: %d\n", np);

    lab_catalog_destroy(cat);
    monitor_destroy(ms);
    printf("All lab & monitor demos passed.\n");
    return 0;
}
