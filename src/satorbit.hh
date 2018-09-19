/* Copyright (C) 2018  István Bozsó
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SATORBIT_H
#define SATORBIT_H

#include "utils.hh"

using namespace utils;

/***********
 * Structs *
 ***********/

// structure for storing fitted orbit polynom coefficients
struct orbit_fit {
    double mean_t, start_t, stop_t;
    double *mean_coords, *coeffs;
    uint is_centered, deg;
    
    orbit_fit() {};
    
    orbit_fit(double _mean_t, double _start_t, double _stop_t,
              double * _mean_coords, double * _coeffs, uint _is_centered,
              uint _deg)
    {
        mean_t = _mean_t;
        start_t = _start_t;
        stop_t = _stop_t;
        
        mean_coords = _mean_coords;
        coeffs = _coeffs;
        
        is_centered = _is_centered;
        deg = _deg;
    }
};

// cartesian coordinate
struct cart {
    double x, y, z;
    
    cart() {};
    
    cart(double _x, double _y, double _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
};

void ell_cart (cdouble lon, cdouble lat, cdouble h,
               double& x, double& y, double& z);

void cart_ell(cdouble x, cdouble y, cdouble z,
              double& lon, double& lat, double& h);

void calc_azi_inc(const orbit_fit& orb, cdouble X, cdouble Y,
                  cdouble Z, cdouble lon, cdouble lat,
                  cuint max_iter, double& azi, double& inc);

#endif // SATORBIT_HPP