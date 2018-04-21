#include <stdio.h>

#include "aux_macros.h"
#include <stdio.h>
#include <tgmath.h>
#include <stdlib.h>
#include <string.h>

#include "Python.h"
#include "numpy/arrayobject.h"
#include "capi_macros.h"

//-----------------------------------------------------------------------------
// STRUCTS
//-----------------------------------------------------------------------------

typedef struct { float lon, lat;  } psxy;
typedef struct { int ni; float lon, lat, he, ve; } psxys;
typedef struct { double x, y, z, lat, lon, h; } station; // [m,rad]

typedef unsigned int uint;

// satellite orbit record
typedef struct { double t, x, y, z; } torb;


//-----------------------------------------------------------------------------
// AUXILLIARY FUNCTIONS
//-----------------------------------------------------------------------------

static FILE * sfopen(const char * path, const char * mode)
{
    FILE * file = fopen(path, mode);

    if (!file) {
        errora("Could not open file \"%s\"   ", path);
        perror("fopen");
        return NULL;
    }
    return file;
}

static inline int plc(const int i, const int j)
{
    // position of i-th, j-th element  0
    // in the lower triangle           1 2
    // stored in vector                3 4 5

    int k, l;
    k = (i < j) ? i + 1 : j + 1;
    l = (i > j) ? i + 1 : j + 1;
    return((l - 1) * l / 2 + k - 1);
} // end plc

static void ATA_ATL(const uint m, const uint u, const double *A, const double *L,
                    double *ATA, double *ATL)
{
    // m - number of measurements
    // u - number of unknowns
    uint kk;
    double *buf = PyMem_New(double, m);
    
    // row of ATPA
    FOR(ii, 0, u)  {
        FOR(kk, 0, m) {
            buf[kk] = 0.0;

            FOR(ll, 0, m)
                buf[kk] += A[ ll* u + ii ];
        }

        FOR(ll, 0, m)
            ATL[ii] += buf[ll] * L[ll];
        
        // column of ATPA
        FOR(jj, 0, u)  {
            kk = plc(ii, jj);

            FOR(ll, 0, m)
                ATA[kk] += buf[ll] * A[ ll * u + jj ];
        } //end - column of ATPA
    } // end - row of ATPA

    PyMem_Del(buf);
} // end ATA_ATL

static int chole(double *q, const uint n)
{
    // Cholesky decomposition of
    // symmetric positive delatnit
    // normal matrix

    uint ia, l1, ll;
    double a, sqr, sum;

    FOR(ii, 0, n) {
        a =  q[plc(ii, ii)];
        if (a <= 0.0) return(1);
        sqr = sqrt(a);

        FOR(kk, ii, n)
            q[plc(ii, kk)] /= sqr;

        ll = ii + 1;
        if (ll == n) goto end1;

        FOR(jj, ll, n) FOR(kk, jj, n)
            q[plc(jj, kk)] -= q[plc(ii, jj)] * q[plc(ii, kk)];
    }

end1:
    FOR(ii, 0, n) {
        ia = plc(ii, ii); q[ia] = 1.0 / q[ia];
        l1 = ii + 1; if (l1 == n) goto end2;

        FOR(jj, l1, n) {
            sum = 0.0;
            FOR(kk, ii, jj)
                sum += q[plc(ii, kk)] * q[plc(kk, jj)];

            q[plc(ii, jj)] = -sum / q[plc(jj, jj)];
        }
    }

end2:
    FOR(ii, 0, n) FOR(kk, ii, n) {
        sum = 0.0;

        FOR(jj, kk, n)
            sum += q[plc(ii, jj)] * q[plc(kk, jj)];

        q[plc(ii, kk)] = sum;
    }

    return(0);
} // end chole


static int poly_fit(const uint m, const uint u, const torb *orb,
                    double *X, const char c)
{
    // o(t) = a0 + a1*t + a2*t^2 + a3*t^3  + ...

    double t, t0, L, *A, *ATA, *ATL, mu0 = 0.0;

    t0 = orb[0].t;

    A   = PyMem_New(double, u);
    ATA = PyMem_New(double, u * (u + 1) / 2);
    ATL = PyMem_New(double, u);

    if (c != 'x' OR c != 'y' OR c != 'z') {
        errora("Invalid option for char \"c\" %c\t Valid options "
               "are: \"x\", \"y\", \"z\"\n", c);
        return(-1);
    }

    FOR(ii, 0, m) {
             if (c == 'x') L = orb[ii].x;
        else if (c == 'y') L = orb[ii].y;
        else if (c == 'z') L = orb[ii].z;

        t = orb[ii].t - t0;

        FOR(jj, 0, u)
            A[jj] = pow(t, 1.0 * jj);

        ATA_ATL(1, u, A, &L, ATA, ATL);
    }

    if( chole(ATA, u) != 0 ) {
        error("poly_fit: Error - singular normal matrix!");
        exit(NUM_ERR);
    }
    
    // update of unknowns
    FOR(ii, 0, u) {
        X[ii] = 0.0;

        FOR(jj, 0, u)
            X[ii] += ATA[plc(ii, jj)] * ATL[jj];
    }

    FOR(ii, 0, m) {
             if (c == 'x') L = -orb[ii].x;
        else if (c == 'y') L = -orb[ii].y;
        else if (c == 'z') L = -orb[ii].z;

        t = orb[ii].t - t0;

        FOR(jj, 0, u)
            L += X[jj] * pow(t, (double) jj);

        mu0 += L * L;
    }

    mu0 = sqrt(mu0 / ((double) (m - u)) );

    printf("\n\n          mu0 = %8.4lf", mu0);
    printf("degree of freedom = %d", m - u);
    printf("\n\n         coefficients                  std\n");

    FOR(jj, 0, u)
        printf("\n%2d %23.15e   %23.15e", jj, X[jj],
                                mu0 * sqrt( ATA[plc(jj, jj)] ));
    
    PyMem_Del(A); PyMem_Del(ATA); PyMem_Del(ATL);
    
    return(0);
} // end poly_fit

static void change_ext ( char *name, char *ext )
{
    // change the extent of name for ext

    int  ii = 0;
    while( name[ii] != '.' && name[ii] != '\0') ii++;
    name[ii] ='\0';

    sprintf(name, "%s.%s", name, ext);
 } // end change_ext

static void cart_ell( station *sta )
{
    // from cartesian to ellipsoidal
    // coordinates

    double n, p, o, so, co, x, y, z;

    n = (WA * WA - WB * WB);
    x = sta->x; y = sta->y; z = sta->z;
    p = sqrt(x * x + y * y);

    o = atan(WA / p / WB * z);
    so = sin(o); co = cos(o);
    o = atan( (z + n / WB * so * so * so) / (p - n / WA * co * co * co) );
    so = sin(o); co = cos(o);
    n= WA * WA / sqrt(WA * co * co * WA + WB * so * so * WB);

    sta->lat = o;
    o = atan(y/x); if(x < 0.0) o += M_PI;
    sta->lon = o;
    sta->h = p / co - n;

}  // end of cart_ell

// --------------------------

static void ell_cart( station *sta )
{
    // from ellipsoidal to cartesian
    // coordinates

    double lat, lon, n;

    lat = sta->lat;
    lon = sta->lon;
    n = WA / sqrt(1.0 - E2 * sin(lat) * sin(lat));

    sta->x = (              n + sta->h) * cos(lat) * cos(lon);
    sta->y = (              n + sta->h) * cos(lat) * sin(lon);
    sta->z = ( (1.0 - E2) * n + sta->h) * sin(lat);

}  // end of ell_cart

static void estim_dominant(const psxys *buffer, const uint ps1, const uint ps2,
                           FILE *ou)
{
    double dist, dx, dy, dz, sumw, sumwve;
    station ps, psd;

    // coordinates of dominant point - weighted mean
    psd.x = psd.y = psd.z = 0.0;

    FOR(ii, 0, ps1 + ps2) {
        ps.lat = buffer[ii].lat * DEG2RAD;
        ps.lon = buffer[ii].lon * DEG2RAD;
        ps.h = buffer[ii].he;

        // compute ps.x ps.y ps.z
        ell_cart( &ps );

        if(ii<ps1) {
            psd.x += ps.x/ps1;
            psd.y += ps.y/ps1;
            psd.z += ps.z/ps1;
        }
        else {
            psd.x += ps.x/ps2;
            psd.y += ps.y/ps2;
            psd.z += ps.z/ps2;
        }
        // sum (1/ps1 + 1/ps2) = 2
    } //end for

   psd.x /= 2.0;
   psd.y /= 2.0;
   psd.z /= 2.0;

   cart_ell( &psd );

   fprintf(ou, "%16.7le %15.7le %9.3lf",
               psd.lon * RAD2DEG, psd.lat * RAD2DEG, psd.h);

    // interpolation of ascending velocities
    sumwve = sumw = 0.0;

    FOR(ii, 0, ps1) {
        ps.lat = buffer[ii].lat * DEG2RAD;
        ps.lon = buffer[ii].lon * DEG2RAD;
        ps.h = buffer[ii].he;

        ell_cart( &ps );

        dx = psd.x - ps.x;
        dy = psd.y - ps.y;
        dz = psd.z - ps.z;
        dist = distance(dx, dy, dz);

        sumw   += 1.0 / dist / dist; // weight
        sumwve += buffer[ii].ve / dist / dist;
    }

    fprintf(ou," %8.3lf", sumwve / sumw);

    // interpolation of descending velocities

    sumwve = sumw = 0.0;

    FOR(ii, ps1, ps1 + ps2) {
        ps.lat = buffer[ii].lat * DEG2RAD;
        ps.lon = buffer[ii].lon * DEG2RAD;
        ps.h = buffer[ii].he;

        ell_cart( &ps );

        dx = psd.x - ps.x;
        dy = psd.y - ps.y;
        dz = psd.z - ps.z;

        dist = distance(dx, dy, dz);


        sumw   += 1.0 / dist / dist; // weight
        sumwve += buffer[ii].ve / dist / dist;
    }
    fprintf(ou," %8.3lf\n", sumwve / sumw);
} //end estim_dominant

static int cluster(psxys *indata1, const uint n1, psxys *indata2, const uint n2,
                   psxys *buffer, uint *nb, const float dam)
{
    uint kk = 0, jj = 0;
    double dlon, dlat, dd, lon, lat;
    double dm = dam / R_earth * RAD2DEG * dam / R_earth * RAD2DEG;

    // skip selected PSs
    while( (indata1[kk].ni == 0) && (kk < n1) )  kk++;

    lon = indata1[kk].lon;
    lat = indata1[kk].lat;

    // 1 for
    FOR(ii, kk, n1) {

        dlon = indata1[ii].lon - lon;
        dlat = indata1[ii].lat - lat;
        dd = dlon * dlon + dlat*dlat;

        if( (indata1[ii].ni > 0) && (dd < dm) ) {
            buffer[jj] = indata1[ii];
            indata1[ii].ni = 0;
            
            jj++;
            if( jj == *nb) {
                (*nb)++;
                PyMem_Resize(buffer, psxys, *nb);
            }
        } // end if
    } // end  1 for
    // 2 for

    FOR(ii, 0, n2) {
        dlon = indata2[ii].lon - lon;
        dlat = indata2[ii].lat - lat;
        dd = dlon * dlon + dlat * dlat;

        if( (indata2[ii].ni > 0) AND (dd < dm) ) {
            buffer[jj] = indata2[ii];
            indata2[ii].ni = 0;
            
            jj++;
            if(jj == *nb) {
                (*nb)++;
                PyMem_Resize(buffer, psxys, *nb);
            }
        } // end if
    } // end for

    return(jj);
}  // end cluster

static int selectp(const float dam, FILE *in1, const psxy *in2, const uint ni,
                   FILE *ou1)
{
    // The PS"lon1,lat1" is selected if the latrst PS"lon2,lat1"
    // is closer than the sepration distance "dam"
    // The "dam", "lon1,lat1" and "lon2,lat1" are interpreted
    // on spherical Earth with radius 6372000 m

    uint n = 0, // # of selected PSs
         m = 0, // # of data in in1
         ef;
    float lat1, lon1, v1, he1, dhe1, lat2, lon2, da;

    // for faster run
    float dm = dam / R_earth * RAD2DEG * dam / R_earth * RAD2DEG;

    while( fscanf(in1,"%e %e %e %e %e", &lon1, &lat1, &v1, &he1, &dhe1) > 0) {
        ef = 0;
        do {
            lon2 = in2[ef].lon;
            lat2 = in2[ef].lat;

//        da= R * acos( sin(lat1/C)*sin(lat2/C)+cos(lat1/C)*cos(lat2/C)*cos((lon1-lon2)/C) );
//        da= da-dam;

            da =   (lat1 - lat2) * (lat1 - lat2)
                 + (lon1 - lon2) * (lon1 - lon2); // faster run
            da = da - dm;

            ef++;
        } while( (da > 0.0) && ((ef - 1) < ni) );

        if( (ef - 1) < ni )
        {
            fprintf(ou1, "%16.7e %16.7e %16.7e %16.7e %16.7e\n",
                          lon1, lat1, v1, he1, dhe1);
            n++;
        }
        m++;  if( !(m % 10000) ) printf("\n %6d ...", m);
    }

    return(n);
} // end selectp

static void azim_elev(const station ps, const station sat, double *azi,
                      double *inc)
{
    // topocentric parameters in PS local system
    double xf, yf, zf, xl, yl, zl, t0;

    // cart system
    xf = sat.x - ps.x;
    yf = sat.y - ps.y;
    zf = sat.z - ps.z;

    xl = - sin(ps.lat) * cos(ps.lon) * xf
         - sin(ps.lat) * sin(ps.lon) * yf + cos(ps.lat) * zf ;

    yl = - sin(ps.lon) * xf + cos(ps.lon) * yf;

    zl = + cos(ps.lat) * cos(ps.lon) * xf
         + cos(ps.lat) * sin(ps.lon) * yf + sin(ps.lat) * zf ;

    t0 = distance(xl, yl, zl);

    *inc = acos(zl / t0) * RAD2DEG;

    if(xl == 0.0) xl = 0.000000001;

    *azi = atan(abs(yl / xl));

    if( (xl < 0.0) && (yl > 0.0) ) *azi = M_PI - *azi;
    if( (xl < 0.0) && (yl < 0.0) ) *azi = M_PI + *azi;
    if( (xl > 0.0) && (yl < 0.0) ) *azi = 2.0 * M_PI - *azi;

    *azi *= RAD2DEG;

    if(*azi > 180.0)
        *azi -= 180.0;
    else
        *azi +=180.0;
} // azim_elev

static void poly_sat_pos(station *sat, const double time, const double *poli,
                         const uint deg_poly)
{
    sat->x = sat->y = sat->z = 0.0;

    FOR(ii, 0, deg_poly) {
        sat->x += poli[ii]                * pow(time, (double) ii);
        sat->y += poli[deg_poly + ii]     * pow(time, (double) ii);
        sat->z += poli[2 * deg_poly + ii] * pow(time, (double) ii);
    }
}

static void poly_sat_vel(double *vx, double *vy, double *vz,
                         const double time, const double *poli,
                         const uint deg_poly)
{
    *vx = 0.0;
    *vy = 0.0;
    *vz = 0.0;

    FOR(ii, 1, deg_poly) {
        *vx += ii * poli[ii]                * pow(time, (double) ii - 1);
        *vy += ii * poli[deg_poly + ii]     * pow(time, (double) ii - 1);
        *vz += ii * poli[2 * deg_poly + ii] * pow(time, (double) ii - 1);
    }
}

static double sat_ps_scalar(station * sat, const station * ps,
                            const double time, const double * poli,
                            const uint poli_deg)
{
    double dx, dy, dz, vx, vy, vz, lv, lps;
    // satellite position
    poly_sat_pos(sat, time, poli, poli_deg);

    dx = sat->x - ps->x;
    dy = sat->y - ps->y;
    dz = sat->z - ps->z;

    lps = distance(dx, dy, dz);

    // satellite velocity
    poly_sat_vel(&vx, &vy, &vz, time, poli, poli_deg);

    lv = distance(vx, vy, vz);

    // normed scalar product of satellite position and velocity vector
    return(  vx / lv * dx / lps
           + vy / lv * dy / lps
           + vz / lv * dz / lps);
}

static void closest_appr(const double *poli, const size_t pd,
                         const double tfp, const double tlp,
                         const station * ps, station * sat,
                         const uint max_iter)
{
    // compute the sat position using closest approache
    double tf, tl, tm; // first, last and middle time
    double vs, vm = 1.0; // vectorial products

    uint itr = 0;

    tf = 0.0;
    tl = tlp - tfp;

    vs = sat_ps_scalar(sat, ps, tf, poli, pd);

    while( fabs(vm) > 1.0e-11 && itr < max_iter)
    {
        tm = (tf + tl) / 2.0;

        vm = sat_ps_scalar(sat, ps, tm, poli, pd);

        if ((vs * vm) > 0.0)
        {
            // change start for middle
            tf = tm;
            vs = vm;
        }
        else
            // change  end  for middle
            tl = tm;

        itr++;
    }
} // end closest_appr

static void axd(const double  a1, const double  a2, const double  a3,
                const double  d1, const double  d2, const double  d3,
                double *n1, double *n2, double *n3)
{
    // vectorial multiplication a x d
   *n1 = a2 * d3 - a3 * d2;
   *n2 = a3 * d1 - a1 * d3;
   *n3 = a1 * d2 - a2 * d1;
} // end axd

static void movements(const double azi1, const double inc1, const float v1,
                      const double azi2, const double inc2, const float v2,
                      float *up, float *east)
{
    double  a1, a2, a3;       // unit vector of sat1
    double  d1, d2, d3;       // unit vector of sat2
    double  n1, n2, n3,  ln;  // 3D vector and its legths
    double  s1, s2, s3,  ls;  // 3D vector and its legths
    double zap, zdp;     // angles in observation plain
    double az, ti, hl;
    double al1,al2,in1,in2;
    double sm, vm;            // movements in observation plain

    al1 = azi1 * DEG2RAD;
    in1 = inc1 * DEG2RAD;
    al2 = azi2 * DEG2RAD;
    in2 = inc2 * DEG2RAD;

//-------------------------

    a1 = -sin(al1) * sin(in1);    // E
    a2 = -cos(al1) * sin(in1);    // N
    a3 =  cos(in1);               // U

    d1 = -sin(al2) * sin(in2);
    d2 = -cos(al2) * sin(in2);
    d3 =  cos(in2);

//-------------------------------------------------------

    // normal vector
    axd(a1, a2, a3, d1, d2, d3, &n1, &n2, &n3);
    ln = sqrt(n1 * n1 + n2 * n2 + n3 * n3);

//-------------------------------------------------------

    n1 /= ln; n2 /= ln; n3 /= ln;

    az = atan(n1 / n2);

    hl = sqrt(n1 * n1 + n2 * n2);
    ti = atan(n3 / hl);

    s1 = -n3 * sin(az);
    s2 = -n3 * cos(az);
    s3 = hl;

    //  vector in the plain
    n1 = s1;
    n2 = s2;
    n3 = s3;

//---------------------------------------

    axd(a1, a2, a3, n1, n2, n3, &s1, &s2, &s3);
    ls = sqrt(s1 * s1 + s2 * s2 + s3 * s3);

    // alfa
    zap=asin(ls);

    axd(d1, d2, d3, n1, n2, n3, &s1, &s2, &s3);
    ls = sqrt(s1 * s1 + s2 * s2 + s3 * s3);

    // beta
    zdp=asin(ls);

    // strike movement
    sm = (v2 / cos(zdp) - v1 / cos(zap)) / (tan(zap) + tan(zdp));

    // tilt movement
    vm = v1 / cos(zap) + tan(zap) * sm;

    // biased Up   component
    *up   = vm / cos(ti);
    // biased East component
    *east = sm / cos(az);

} // end  movement

//-----------------------------------------------------------------------------
// MAIN FUNCTIONS -- CAN BE CALLED FROM PYTHON
//-----------------------------------------------------------------------------

PyDoc_STRVAR(
    data_select__doc__,
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    "\n +                  ps_data_select                    +"
    "\n + adjacent ascending and descending PSs are selected +"
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "\n   usage:  ps_data_select asc_data.xy dsc_data.xy 100  \n"
    "\n           asc_data.xy  - (1st) ascending  data file"
    "\n           dsc_data.xy  - (2nd) descending data file"
    "\n           100          - (3rd) PSs separation (m)\n"
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

static PyObject * daisy_data_select(PyObject * self, PyObject * args,
                                    PyObject * kwargs)
{
    const char *in_asc, *in_dsc,
               *out_asc = "asc_select.xy",
               *out_dsc = "dsc_select.xy";
    float max_diff = 100.0;

    static char * keywords[] = {"in_asc", "in_dsc", "out_asc", "out_dsc",
                                "max_diff", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|ssf:data_select",
                                     keywords, &in_asc, &in_dsc, &out_asc,
                                     &out_dsc, &max_diff))
        return NULL;

   uint n, ni1, ni2;
   psxy *indata;

   FILE *in1, *in2, *ou1, *ou2;

   float lon, lat, v, he, dhe;

    printf("\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                  ps_data_select                    +"
           "\n + adjacent ascending and descending PSs are selected +"
           "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    in1 = sfopen(in_asc, "rt");
    in2 = sfopen(in_dsc, "rt");

    ou1 = sfopen(out_asc, "w+t");
    ou2 = sfopen(out_dsc, "w+t");

    println("\n Appr. PSs separation %5.1f (m)", max_diff);

    ni1 = 0;
    while(fscanf(in1,"%e %e %e %e %e", &lon, &lat, &v, &he, &dhe) > 0) ni1++;
    rewind(in1);

    ni2 = 0;
    while(fscanf(in2,"%e %e %e %e %e", &lon, &lat, &v, &he, &dhe) > 0) ni2++;
    rewind(in2);

    //  Copy data to memory
    indata = PyMem_New(psxy, ni2);

    FOR(ii, 0, ni2)
        fscanf(in2, "%e %e %e %e %e", &(indata[ii].lon), &(indata[ii].lat),
                                      &v, &he, &dhe);

    println("\n\n %s  PSs %d", in_asc, ni1);

    printf("\n Select PSs ...\n");
    n = selectp(max_diff, in1, indata, ni2, ou1);

    rewind(ou1);
    rewind(in1);
    rewind(in2);

    println("\n\n %s PSs %d", out_dsc, n);
    println("\n\n %s PSs %d", in_dsc, ni2);

    PyMem_Del(indata);

    // Copy data to memory
    indata = PyMem_New(psxy, n);

    FOR(ii, 0, n)
        fscanf(ou1, "%e %e %e %e %e", &(indata[ii].lon), &(indata[ii].lat),
                                      &v, &he, &dhe);

    printf("\n Select PSs ...\n");
    n = selectp(max_diff, in2, indata, n, ou2);

    printf("\n\n %s PSs %d\n" , out_dsc, n);

    printf("\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                end   ps_data_select                +"
           "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    PyMem_Del(indata);

    Py_RETURN_NONE;

}  // end main

// -------------------------------------------------------------

PyDoc_STRVAR(
    dominant__doc__,
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    "\n +                      ps_dominant                      +"
    "\n + clusters of ascending and descending PSs are selected +"
    "\n +     and the dominant points (DSs) are estimated       +"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"

    "\n    usage:  ps_dominant asc_data.xys dsc_data.xys 100\n"
    "\n            asc_data.xys   - (1st) ascending  data file"
    "\n            dsc_data.xys   - (2nd) descending data file"
    "\n            100            - (3rd) cluster separation (m)\n"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

static PyObject * daisy_dominant(PyObject * self, PyObject * args,
                                 PyObject * kwargs)
{
    const char *in_asc, *in_dsc,
               *out = "dominant.xy";
    float max_diff = 100.0;

    static char *keywords[] = {"in_asc", "in_dsc", "out_dom",
                                "max_diff", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|sf:data_select",
                                     keywords, &in_asc, &in_dsc, &out,
                                     &max_diff))
        return NULL;

    uint n1, n2,        // number of data in input files
         nb = 2,        // starting number of data in cluster buffer,
                        // continiusly updated
         nc,          // number of preselected clusters
         nsc,         // number of selected clusters
         nhc,         // number of hermit clusters
         nps,         // number of selected PSs in actual cluster
         ps1,         // number of PSs from 1 input file
         ps2;         // number of PSs from 2 input file

    psxys *indata1, *indata2, *buffer = NULL;  // names of allocated memories

    FILE *in1, *in2, *ou;

    float lon, lat, he, dhe, ve;

    printf("\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                      ps_dominant                      +"
           "\n + clusters of ascending and descending PSs are selected +"
           "\n +     and the dominant points (DSs) are estimated       +"
           "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    in1 = sfopen(in_asc, "rt");
    in2 = sfopen(in_dsc, "rt");
    ou  = sfopen(out, "w+t");

    println("\n  input: %s\n         %s", in_asc, in_dsc);
    printf("\n output: %s\n\n", out);

    println("\n Appr. cluster size %5.1f (m)",max_diff);
    printf("\n Copy data to memory ...\n");

    n1 = 0;
    while(fscanf(in1, "%e %e %e %e %e", &lon, &lat, &ve, &he, &dhe) > 0) n1++;
    rewind(in1);

    indata1 = PyMem_New(psxys, n1);

    FOR(ii, 0, n1) {
         fscanf(in1,"%e %e %e %e %e", &(indata1[ii].lon),
                                      &(indata1[ii].lat),
                                      &(indata1[ii].ve), &he, &dhe);
         indata1[ii].ni = 1;
         indata1[ii].he = he + dhe;
    }
    fclose(in1);

    n2 = 0;
    while(fscanf(in2, "%e %e %e %e %e", &lon, &lat, &ve, &he, &dhe) > 0) n2++;
    rewind(in2);
    printf("%d\n", n2);
    
    indata2 = PyMem_New(psxys, n2);

    FOR(ii, 0, n2) {
        fscanf(in2, "%e %e %e %e %e", &(indata2[ii].lon),
                                      &(indata2[ii].lat),
                                      &(indata2[ii].ve), &he, &dhe);
        indata2[ii].ni = 2;
        indata2[ii].he = he + dhe;
    }
    fclose(in2);

    printf("\n selected clusters:\n");

    nps = nc = nhc = nsc = 0;

    do {
        nps = cluster(indata1, n1, indata2, n2, buffer, &nb, max_diff);

        ps1 = ps2 = 0;
        FOR(ii, 0, nps) if( buffer[ii].ni == 1) ps1++;
        else            if( buffer[ii].ni == 2) ps2++;

        if( (ps1 * ps2) > 0) {
            estim_dominant(buffer, ps1, ps2, ou);
            nsc++;
        }
        else if ( (ps1 + ps2) > 0 ) nhc++;

        nc++;

        if((nc % 2000) == 0) printf("\n %6d ...", nc);

    } while(nps > 0 );

    printf("\n %6d", nc - 1);

    printf("\n\n hermit   clusters: %6d\n accepted clusters: %6d\n", nhc, nsc);
    printf("\n Records of %s file:\n", out);

    printf("\n longitude latitude  height asc_v dsc_v");
    printf("\n (     degree          m      mm/year )\n");

    printf("\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                   end    ps_dominant                  +"
           "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    PyMem_Del(indata1); PyMem_Del(indata2);

    Py_RETURN_NONE;
}  // end daisy_dominant


PyDoc_STRVAR(
    poly_orbit__doc__,
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    "\n +                     ps_poly_orbit                     +"
    "\n +    tabular orbit data are converted to polynomials    +"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "\n          usage:    ps_poly_orbit asc_master.res 4"
    "\n                 or"
    "\n                    ps_poly_orbit dsc_master.res 4"
    "\n\n          asc_master.res or dsc_master.res - input files"
    "\n          4                                - degree     \n"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

static PyObject * daisy_poly_orbit(PyObject * self, PyObject * args,
                                   PyObject * kwargs)
{
    const char *in_data;
    uint deg_poly = 4;

    static char * keywords[] = {"in_data", "deg_poly", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|I:poly_orbit",
                                     keywords, &in_data, &deg_poly))
        return NULL;

    // number of orbit records
    uint ndp;

    // tabular orbit data
    torb *orb;
    double *X;

    char *out = NULL, buf[80], *head = "NUMBER_OF_DATAPOINTS:";

    FILE *in, *ou;

    printf("\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                     ps_poly_orbit                     +"
           "\n +    tabular orbit data are converted to polynomials    +"
           "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    sprintf(out, "%s", in_data);
    change_ext(out, "porb");

    in = sfopen(in_data, "rt");
    ou = sfopen(out, "w+t");

    printf("\n  input: %s",in_data);
    printf("\n output: %s",out);
    println("\n degree: %d",deg_poly);

    while( fscanf(in, "%s", buf) > 0 && strncmp(buf, head, 21) != 0 );
    fscanf(in, "%d", &ndp);

    orb = PyMem_New(torb, ndp);
    
    X = PyMem_New(double, deg_poly + 1);

    FOR(ii, 0, ndp)
        fscanf(in, "%lf %lf %lf %lf", &(orb[ii].t), &(orb[ii].x),
                                      &(orb[ii].y), &(orb[ii].z));
    fprintf(ou, "%3d\n", deg_poly);
    fprintf(ou, "%13.5f\n", orb[0].t);
    fprintf(ou, "%13.5f\n", orb[ndp - 1].t);

    printf("\n fit of X coordinates:");

    poly_fit(ndp, deg_poly + 1, orb, X, 'x');
    FOR(ii, 0, deg_poly + 1)  fprintf(ou," %23.15e", X[ii]);

    fprintf(ou,"\n");

    printf("\n fit of Y coordinates:");

    poly_fit(ndp, deg_poly + 1, orb, X, 'y');
    FOR(ii, 0, deg_poly + 1)  fprintf(ou," %23.15e", X[ii]);

    fprintf(ou,"\n");

    printf("\n fit of Z coordinates:");

    poly_fit(ndp, deg_poly + 1, orb, X, 'z');
    FOR(ii, 0, deg_poly + 1)  fprintf(ou," %23.15e", X[ii]);

    fprintf(ou,"\n");

    fclose(in);
    fclose(ou);

    printf("\n\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
             "\n +             end          ps_poly_orbit                +"
             "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    PyMem_Del(orb); PyMem_Del(X);

    Py_RETURN_NONE;
}  // end daisy_poly_orbit

PyDoc_STRVAR(
    integrate__doc__,
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    "\n +                       ds_integrate                          +"
    "\n +        compute the east-west and up-down velocities         +"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "\n usage:                                                      \n"
    "\n    ds_integrate dominant.xyd asc_master.porb dsc_master.porb\n"
    "\n              dominant.xyd  - (1st) dominant DSs data file   "
    "\n           asc_master.porb  - (2nd) ASC polynomial orbit file"
    "\n           dsc_master.porb  - (3rd) DSC polynomial orbit file\n"
    "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

static PyObject * daisy_integrate(PyObject * self, PyObject * args,
                                  PyObject * kwargs)
{
    const char *in_dom  = "dominant.xy",
               *asc_orb = "asc_master.porb",
               *dsc_orb = "dsc_master.porb",
               *out     = "integrate.xy";

    static char * keywords[] = {"in_dom", "asc_orb", "dsc_orb", "out_int",
                                NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ssss:intergate",
                                     keywords, &in_dom, &asc_orb,
                                     &dsc_orb, &out))
        return NULL;

    uint nn = 0;
    station ps, sat;

    double azi1, inc1, azi2, inc2;
    double ft1, lt1,               // first and last time of orbit files
           ft2, lt2,               // first and last time of orbit files
           *pol1, *pol2;           // orbit polinomials

    uint dop1, dop2;              // degree of orbit polinomials

    float lon, lat, he, v1, v2, up, east;

    FILE *ind, *ino1, *ino2, *ou;

    printf("\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                       ds_integrate                          +"
           "\n +        compute the east-west and up-down velocities         +"
           "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("\n  inputs:   %s\n          %s\n          %s",
           in_dom, asc_orb, dsc_orb);
    println("\n\n outputs:  %s", out);

//-----------------------------------------------------------------------------
    ino1 = sfopen(asc_orb, "rt");

    fscanf(ino1, "%d %lf %lf", &dop1, &ft1, &lt1); // read orbit
    dop1++;

    pol1 = PyMem_New(double, dop1 * 3);

    ino1 = sfopen(asc_orb, "rt");

    FOR(ii, 0, 3)
        FOR(jj, 0, dop1)
            fscanf(ino1," %lf", &(pol1[ii * dop1 + jj]));

    fclose(ino1);

//-----------------------------------------------------------------------------

    ino2 = sfopen(dsc_orb, "rt");

    fscanf(ino2, "%d %lf %lf", &dop2, &ft2, &lt2); // read orbit
    dop2++;
    
    pol2 = PyMem_New(double, dop2 * 3);

    FOR(ii, 0, 3)
        FOR(jj, 0, dop2)
            fscanf(ino2," %lf", &(pol2[ii * dop2 + jj]));

    fclose(ino2);

//-----------------------------------------------------------------------------

    ind = sfopen(in_dom, "rt");
    ou = sfopen(out, "w+t");

    while( fscanf(ind, "%f %f %f %f %f", &lon, &lat, &he, &v1, &v2) > 0 ) {
        ps.lat = lat / 180.0 * M_PI;
        ps.lon = lon / 180.0 * M_PI;
        ps.h = he;

        ell_cart(&ps);

        closest_appr(pol1, dop1, ft1, lt1, &ps, &sat, 1000);
        azim_elev(ps, sat, &azi1, &inc1);

        closest_appr(pol2, dop2, ft2, lt2, &ps, &sat, 1000);
        azim_elev(ps, sat, &azi2, &inc2);

        movements(azi1, inc1, v1, azi2, inc2, v2, &up, &east);

        fprintf(ou,"%16.7e %15.7e %9.3f %7.3f %7.3f\n", lon, lat, he,
                                                        east, up);
        nn++;  if( !(nn % 1000) ) printf("\n %6d ...", nn);
    }

    fclose(ind); fclose(ou);

    printf("\n %6d", nn);
    println("\n\n Records of %s file:", out);
    printf("\n longitude latitude  height  ew_v   up_v");
    printf("\n (     degree          m       mm/year )");


    printf("\n\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
             "\n +                     end    ds_integrate                     +"
             "\n +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    PyMem_Del(pol1); PyMem_Del(pol2);
    
    Py_RETURN_NONE;
}  // end daisy_integrate

PyDoc_STRVAR(
    zero_select__doc__,
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    "\n +                   ds_zero_select                   +"
    "\n +  select integrated DSs with nearly zero velocity   +"
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
    "\n     usage:   ds_zero_select integrate.xyi 0.6         \n"
    "\n            integrate.xyi  -  integrated data file"
    "\n            0.6 (mm/year)  -  zero data criteria\n"
    "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

static PyObject * daisy_zero_select(PyObject * self, PyObject * args,
                                  PyObject * kwargs)
{
    uint n = 0, nz = 0, nt = 0;

    const char *inp = "integrate.xy",   // input file
               *out1,                   // output file - zero velocity
               *out2;                   // output file - non-zero velocity

    float zch = 0.6;

    static char * keywords[] = {"out_zero", "out_nonzero", "integrate",
                                "zero_thresh", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|sf:zero_select",
                                     keywords, &out1, &out2, &inp,
                                     &zch))
        return NULL;

    FILE *in, *ou1, *ou2;

    float lon, lat, he, ve, vu;

    printf("\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +                   ds_zero_select                   +"
           "\n +  select integrated DSs with nearly zero velocity   +"
           "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("\n    input: %s"  , inp);
    println("\n   output: %s\n           %s", out1, out2);
    println("\n   zero DSs < |%3.1f| mm/year", zch);

    in = sfopen(inp, "rt");
    ou1 = sfopen(out1, "w+t");
    ou2 = sfopen(out2, "w+t");

//---------------------------------------------------------------

    while (fscanf(in, "%f %f %f %f %f", &lon, &lat, &he, &ve, &vu) > 0) {
        if (sqrt(ve * ve + vu * vu) <= zch) {
            fprintf(ou1, "%16.7e %15.7e %9.3f %7.3f %7.3f\n",
                        lon, lat, he, ve, vu);
            nz++;
        }
        else {
            fprintf(ou2, "%16.7e %15.7e %9.3f %7.3f %7.3f\n",
                        lon, lat, he, ve, vu);
            nt++;
        }
        n++; if(!(n % 1000)) printf("\n %6d ...", n);
    }
    printf("\n %6d\n", n);

    printf("\n   zero dominant DSs %6d\n        target   DSs %6d\n",nz,nt);
    printf("\n Records of output files:\n");
    printf("\n longitude latitude  height  ew_v   up_v");
    printf("\n (     degree          m       mm/year )\n");

//----------------------------------------------------------------------

    printf("\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++"
           "\n +              end    ds_zero_select                 +"
           "\n ++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    Py_RETURN_NONE;

}  // end daisy_zero_select


// Function for testing stuff
static PyObject * test(PyObject * self, PyObject * args)
{
    PyObject * arg = NULL;
    NPY_AO * array = NULL;
    
    if (!PyArg_ParseTuple(args, "O:test", &arg))
        return NULL;

    array = (NPY_AO *) PyArray_FROM_OTF(arg, NPY_DOUBLE, NPY_ARRAY_IN_ARRAY);
    
    if(array == NULL) goto fail;

    println("%ld %ld", PyArray_DIM(array, 0), PyArray_DIM(array, 1));
    println("%lf %lf", NPY_DELEM(array, 0, 0), NPY_DELEM(array, 0, 1));
    println("%lf %lf", NPY_DELEM(array, 1, 0), NPY_DELEM(array, 1, 1));
    
    Py_DECREF(array);
    Py_RETURN_NONE;

fail:
    Py_XDECREF(array);
    return NULL;
}

static PyMethodDef DaisyMethods[] = {
    {"test", (PyCFunction) test, METH_VARARGS, ""},
    {"data_select", (PyCFunction) daisy_data_select,
     METH_VARARGS | METH_KEYWORDS,
     data_select__doc__},
    {"dominant", (PyCFunction) daisy_dominant,
     METH_VARARGS | METH_KEYWORDS,
     dominant__doc__},
    {"poly_orbit", (PyCFunction) daisy_poly_orbit,
     METH_VARARGS | METH_KEYWORDS,
     poly_orbit__doc__},
    {"integrate", (PyCFunction) daisy_integrate,
     METH_VARARGS | METH_KEYWORDS,
     integrate__doc__},
    {"zero_select", (PyCFunction) daisy_zero_select,
     METH_VARARGS | METH_KEYWORDS,
     zero_select__doc__},
    {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(
    daisy__doc__,
    "DAISY");

static struct PyModuleDef daisymodule = {
    PyModuleDef_HEAD_INIT, "daisy", daisy__doc__, -1, DaisyMethods
};

PyMODINIT_FUNC
PyInit_daisy(void)
{
    import_array();
    return PyModule_Create(&daisymodule);
}

int main(int argc, char **argv)
{
    double a[3];
    //println("J0(%g) = %.18e", 5.0, gsl_sf_bessel_J0(5.0));
    
    //return 0;
    
    /*for(double ii = 0.0; ii < 1e6; ii++) {
        a[0] = 0.0;
        a[1] = ii;
        a[2] = ii * 2;
        fwrite(a, 3 * sizeof(double), 1, stdout);
    }*/
    
    while (fread(a, 3 * sizeof(double), 1, stdin) > 0) {
        //fwrite(a, 3 * sizeof(double), 1, stdout);
        println("%lf %lf %lf", a[0], a[1], a[2]);
    }
    
    
    
    return 0;
}
