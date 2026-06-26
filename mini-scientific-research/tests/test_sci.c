#include "sci_numeric.h"
#include "sci_statistics.h"
#include "sci_optimize.h"
#include "sci_linearalgebra.h"
#include "sci_research.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define EPS 1e-4

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(n) do{tests_run++;printf("  TEST %s... ",n);}while(0)
#define PASS() do{tests_passed++;printf("PASSED\n");}while(0)
#define FAIL(m) do{printf("FAILED: %s\n",m);}while(0)
#define CHECK(c,m) do{if(!(c)){FAIL(m);return;}}while(0)

static double f1(double x){return x*x-2.0;}
static double df1(double x){return 2.0*x;}
static double ode_f(double t,double y){(void)t;return y;}
static double qf(double x){return x*x;}
static double mv_f(const double*x,int n){(void)n;return x[0]*x[0]+x[1]*x[1];}
static void mv_g(const double*x,int n,double*g){(void)n;g[0]=2*x[0];g[1]=2*x[1];}
static double sq_func(double x){return x*x;}

static void t_bisect(void){TEST("Bisection");SCI_RootConfig c=sci_root_config_default(SCI_ROOT_BISECTION);SCI_RootResult r=sci_find_root(f1,1,2,&c);CHECK(r.converged&&fabs(r.root-1.4142)<EPS,"fail");PASS();}
static void t_brent(void){TEST("Brent");SCI_RootResult r=sci_brent_root(f1,1,2,1e-8,100);CHECK(r.converged&&fabs(r.root-1.4142)<EPS,"fail");PASS();}
static void t_newton(void){TEST("Newton");SCI_RootResult r=sci_newton_root(f1,df1,1.5,1e-8,50);CHECK(r.converged&&fabs(r.root-1.4142)<EPS,"fail");PASS();}
static void t_deriv(void){TEST("Derivative");double d=sci_derivative_first(sq_func,3,1e-6);CHECK(fabs(d-6)<0.01,"fail");PASS();}
static void t_ode(void){TEST("ODE");SCI_ODEConfig c=sci_ode_config_default(SCI_ODE_RK4);c.h0=0.01;SCI_ODEResult r=sci_ode_solve(ode_f,0,1,1,&c);CHECK(r.converged&&fabs(r.y[r.num_points-1]-2.7183)<0.05,"fail");sci_ode_result_free(&r);PASS();}
static void t_quad(void){TEST("Quad");SCI_QuadConfig c=sci_quad_config_default(SCI_QUAD_ADAPTIVE);SCI_QuadResult r=sci_integrate(qf,0,1,&c);CHECK(r.converged&&fabs(r.integral-0.33333)<0.001,"fail");PASS();}
static void t_spline(void){TEST("Spline");double x[]={0,1,2,3},y[]={0,1,4,9},y2[4];CHECK(sci_spline_coefficients(x,y,4,y2),"fail");CHECK(fabs(sci_spline_eval(x,y,4,y2,1.5)-2.25)<0.5,"fail");PASS();}
static void t_stats(void){TEST("Summary");double d[]={1,2,3,4,5};SCI_SummaryStats s=sci_compute_summary(d,5);CHECK(fabs(s.mean-3)<EPS,"fail");CHECK(s.median==3,"fail");PASS();}
static void t_norm(void){TEST("Normal");CHECK(fabs(sci_normal_cdf(1.96)-0.975)<0.01,"fail");PASS();}
static void t_ttest(void){TEST("T-test");double d[]={1,2,3,4,5,6,7,8,9,10};SCI_TestResult r=sci_ttest_1sample(d,10,5.5,0.05,SCI_TEST_TWO_TAILED);CHECK(r.p_value>=0&&r.p_value<=1,"fail");CHECK(r.df==9,"fail");PASS();}
static void t_chi(void){TEST("Chi-sq");int o[]={50,30,40,80};SCI_TestResult r=sci_chisq_test(o,2,2,0.05);CHECK(r.statistic>0&&r.df==1,"fail");PASS();}
static void t_anova(void){TEST("ANOVA");double g1[]={2,3,4,3,3},g2[]={5,6,6,5,5},g3[]={7,8,9,8,8};const double*g[]={g1,g2,g3};int s[]={5,5,5};SCI_AnovaResult r=sci_anova_oneway(g,s,3,0.05);CHECK(r.f_statistic>1,"fail");CHECK(r.ss_between>r.ss_within,"fail");PASS();}
static void t_ols(void){TEST("OLS");double X[]={1,2,3,4,5},y[]={3,5,7,9,11};SCI_RegressionResult r;memset(&r,0,sizeof(r));CHECK(sci_regression_ols(X,y,5,1,&r),"fail");CHECK(r.r_squared>0.99,"fail");sci_regression_free(&r);PASS();}
static void t_gd(void){TEST("GD");double x[]={3,4};SCI_OptConfig c=sci_opt_config_default(SCI_OPT_GRADIENT_DESCENT);c.max_iter=2000;c.gtol=1e-4;SCI_OptResult r=sci_minimize_gradient_descent(mv_f,mv_g,x,2,&c);CHECK(r.optimal_value<0.1,"fail");sci_opt_result_free(&r);PASS();}
static void t_nm(void){TEST("NM");double x[]={3,4};SCI_OptConfig c=sci_opt_config_default(SCI_OPT_NELDER_MEAD);c.max_iter=200;SCI_OptResult r=sci_minimize_nelder_mead(mv_f,x,2,1,&c);CHECK(r.optimal_value<0.01,"fail");sci_opt_result_free(&r);PASS();}
static void t_lp(void){TEST("Simplex");double c2[]={1,1},A[]={1,1},b[]={1},x[2]={0},obj;bool ok=sci_linprog_simplex(c2,A,b,1,2,x,&obj);CHECK(ok,"fail");CHECK(x[0]>=0&&x[1]>=0,"fail");PASS();}
static void t_lu(void){TEST("LU");SCI_Matrix A=sci_matrix_create(2,2);sci_matrix_set(&A,0,0,2);sci_matrix_set(&A,0,1,1);sci_matrix_set(&A,1,0,1);sci_matrix_set(&A,1,1,3);double b[]={5,8};CHECK(sci_solve_lu(&A,b,2),"fail");CHECK(fabs(b[0]-1.4)<EPS,"fail");sci_matrix_destroy(&A);PASS();}
static void t_ch(void){TEST("Cholesky");SCI_Matrix A=sci_matrix_create(2,2);sci_matrix_set(&A,0,0,4);sci_matrix_set(&A,0,1,1);sci_matrix_set(&A,1,0,1);sci_matrix_set(&A,1,1,3);SCI_CholeskyResult ch=sci_cholesky_decompose(&A);CHECK(ch.L.data!=NULL,"fail");double b[]={1,2},x[2]={0};sci_cholesky_solve(&ch,b,x);CHECK(fabs(4*x[0]+x[1]-1)<EPS,"fail");sci_cholesky_free(&ch);sci_matrix_destroy(&A);PASS();}
static void t_qr(void){TEST("QR");SCI_Matrix A=sci_matrix_create(3,2);sci_matrix_set(&A,0,0,1);sci_matrix_set(&A,0,1,2);sci_matrix_set(&A,1,0,3);sci_matrix_set(&A,1,1,4);sci_matrix_set(&A,2,0,5);sci_matrix_set(&A,2,1,6);SCI_QRResult qr=sci_qr_decompose(&A);CHECK(qr.QR.data!=NULL,"fail");sci_qr_free(&qr);sci_matrix_destroy(&A);PASS();}
static void t_pi(void){TEST("PowerIter");SCI_Matrix A=sci_matrix_create(3,3);sci_matrix_set(&A,0,0,3);sci_matrix_set(&A,1,1,2);sci_matrix_set(&A,2,2,1);double v[]={1,0,0},ev;int it;CHECK(sci_eigen_power_iteration(&A,v,3,100,1e-8,&ev,&it),"fail");CHECK(fabs(ev-3)<0.01,"fail");sci_matrix_destroy(&A);PASS();}
static void t_exp(void){TEST("Experiment");sci_research_init();int id=sci_experiment_create("T","H0",SCI_STUDY_EXPERIMENTAL);CHECK(id>=1,"fail");CHECK(sci_experiment_set_stage(id,SCI_EXP_RUNNING),"fail");SCI_Experiment e;CHECK(sci_experiment_get(id,&e)&&e.stage==SCI_EXP_RUNNING,"fail");PASS();}
static void t_rnd(void){TEST("Random");int g[20];sci_randomize_subjects(20,3,42,g);int ok=1;for(int i=0;i<20;i++)if(g[i]<0||g[i]>2)ok=0;CHECK(ok,"fail");PASS();}
static void t_pwr(void){TEST("Power");int n=sci_power_sample_size_ttest(0.5,0.05,0.80);CHECK(n>50&&n<100,"fail");PASS();}
static void t_cd(void){TEST("Cohen");double d=sci_cohens_d(10,5,4,4,30,30);CHECK(fabs(d-2.5)<0.1,"fail");PASS();}
static void t_out(void){TEST("Outlier");double d[]={1,2,3,4,5,100};bool o[6];sci_outlier_detection(d,6,3.5,o);CHECK(o[5]&&!o[0],"fail");PASS();}
static void t_rep(void){TEST("Repro");double a[]={1,2,3},b[]={1.001,1.999,3.002};CHECK(sci_reproducibility_check(a,b,3,0.01),"fail");PASS();}

int main(void){
    printf("mini-scientific-research: Running Tests\n");
    printf("========================================\n\n");
    printf("[sci_numeric]\n");
    t_bisect();t_brent();t_newton();t_deriv();t_ode();t_quad();t_spline();
    printf("\n[sci_statistics]\n");
    t_stats();t_norm();t_ttest();t_chi();t_anova();t_ols();
    printf("\n[sci_optimize]\n");
    t_gd();t_nm();t_lp();
    printf("\n[sci_linearalgebra]\n");
    t_lu();t_ch();t_qr();t_pi();
    printf("\n[sci_research]\n");
    t_exp();t_rnd();t_pwr();t_cd();t_out();t_rep();
    printf("\n========================================\n");
    printf("RESULTS: %d/%d tests passed\n",tests_passed,tests_run);
    if(tests_passed==tests_run){printf("ALL TESTS PASSED\n");sci_research_shutdown();return 0;}
    printf("SOME TESTS FAILED\n");sci_research_shutdown();return 1;
}
