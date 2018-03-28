// https://github.com/erich666/GraphicsGems/blob/master/gems/Roots3And4.c (Jochen Schwarze in Game Gems, 1990)
// https://github.com/erich666/GraphicsGems/blob/master/LICENSE.md

#pragma once

#ifndef solvers_hpp
#define solvers_hpp

#include "util.hpp"
#include "math-common.hpp"

inline bool is_zero(double d) { return d > -double(1e-9) && d < double(1e-9); }

// Solve linear equation: c0 + c1 * x = 0
// Returns number of solutions
int solve_linear(const double c0, const double c1, double & s0)
{
    if (std::fabs(c1) < double(1e-9)) return 0;
    s0 = -c0 / c1;
    return 1;
}

// Solve quadratic equation: c0*x^2 + c1*x + c2. 
// Returns number of solutions.
inline int solve_quadratic(const double c0, const double c1, const double c2, double & s0, double & s1) 
{
    s0 = s1 = std::numeric_limits<double>::signaling_NaN();

    double p, q, D;

    // Normal form: x^2 + px + q = 0
    p = c1 / (2 * c0);
    q = c2 / c0;

    D = p * p - q;

    if (is_zero(D)) 
    {
        s0 = -p;
        return 1;
    }
    else if (D < 0) 
    {
        return 0;
    }
    else
    {
        double sqrt_D = std::sqrt(D);
        s0 =  sqrt_D - p;
        s1 = -sqrt_D - p;
        return 2;
    }
}

// Solve cubic equation: c0*x^3 + c1*x^2 + c2*x + c3. 
// Returns number of solutions.
inline int solve_cubic(const double c0, const double c1, const double c2, const double c3, double & s0, double & s1, double & s2)
{
    s0 = s1 = s2 = std::numeric_limits<double>::signaling_NaN();

    int num;
    double sub, A, B, C, sq_A, p, q, cb_p, D;

    // Normal form: x^3 + Ax^2 + Bx + C = 0
    A = c1 / c0;
    B = c2 / c0;
    C = c3 / c0;

    // Substitute x = y - A/3 to eliminate quadric term:  x^3 +px + q = 0
    sq_A = A * A;
    p = 1.0/3.0 * (- 1.0/3.0 * sq_A + B);
    q = 1.0/2.0 * (2.0/27.0 * A * sq_A - 1.0/3.0 * A * B + C);

    // Cardano's formula
    cb_p = p * p * p;
    D = q * q + cb_p;

    if (is_zero(D)) 
    {
        if (is_zero(q))  
        {
            // One triple solution
            s0 = 0;
            num = 1;
        }
        else  
        {
            // One single and one double solution
            double u = std::pow(-q, 1.0/3.0);
            s0 = 2 * u;
            s1 = - u;
            num = 2;
        }
    }
    else if (D < 0) 
    {
        // Three real solutions
        double phi = 1.0/3.0 * std::acos(-q / std::sqrt(-cb_p));
        double t = 2.0 * std::sqrt(-p);

        s0 =   t * std::cos(phi);
        s1 = - t * std::cos(phi + ANVIL_PI / 3.0);
        s2 = - t * std::cos(phi - ANVIL_PI / 3.0);
        num = 3;
    } 
    else 
    {
        // One real solution
        double sqrt_D = std::sqrt(D);
        double u = std::pow(sqrt_D - q, 1.0/3.0);
        double v = - std::pow(sqrt_D + q, 1.0/3.0);

        s0 = u + v;
        num = 1;
    }

    // Resubstitute
    sub = 1.0 / 3.0 * A;

    if (num > 0) s0 -= sub;
    if (num > 1) s1 -= sub;
    if (num > 2) s2 -= sub;

    return num;
}

// Solve quartic function: c0*x^4 + c1*x^3 + c2*x^2 + c3*x + c4. 
// Returns number of solutions.
inline int solve_quartic(double c0, double c1, double c2, double c3, double c4, double & s0, double & s1, double & s2, double & s3) 
{
    s0 = s1 = s2 = s3 = std::numeric_limits<double>::signaling_NaN(); 

    std::vector<double> coeffs(4, 0.0);

    int num;
    double z, u, v, sub;
    double A, B, C, D;
    double sq_A, p, q, r;

    // Normal form: x^4 + Ax^3 + Bx^2 + Cx + D = 0
    A = c1 / c0;
    B = c2 / c0;
    C = c3 / c0;
    D = c4 / c0;

    // Substitute x = y - A/4 to eliminate cubic term: x^4 + px^2 + qx + r = 0
    sq_A = A * A;
    p = -3.0/8.0 * sq_A + B;
    q = 1.0/8.0 * sq_A * A - 1.0/2.0 * A * B + C;
    r = -3.0/256.0*sq_A*sq_A + 1.0/16.0*sq_A*B - 1.0/4.0*A*C + D;

    if (is_zero(r)) 
    {
        // No absolute term: y(y^3 + py + q) = 0
        coeffs[3] = q;
        coeffs[2] = p;
        coeffs[1] = 0;
        coeffs[0] = 1;

        num = solve_cubic(coeffs[0], coeffs[1], coeffs[2], coeffs[3], s0, s1, s2);
    }
    else 
    {
        // Solve the resolvent cubic
        coeffs[3] = 1.0/2.0 * r * p - 1.0/8.0 * q * q;
        coeffs[2] = -r;
        coeffs[1] = -1.0/2.0 * p;
        coeffs[0] = 1.0;

        solve_cubic(coeffs[0], coeffs[1], coeffs[2], coeffs[3], s0, s1, s2);

        // Take the one real solution
        z = s0;

        // Then build two quadric equations
        u = z * z - r;
        v = 2.0 * z - p;

        if (is_zero(u)) u = 0;
        else if (u > 0) u = std::sqrt(u);
        else return 0;

        if (is_zero(v)) v = 0;
        else if (v > 0) v = std::sqrt(v);
        else return 0;

        coeffs[2] = z - u;
        coeffs[1] = q < 0 ? -v : v;
        coeffs[0] = 1.0;

        num = solve_quadratic(coeffs[0], coeffs[1], coeffs[2], s0, s1);

        coeffs[2] = z + u;
        coeffs[1] = q < 0 ? v : -v;
        coeffs[0] = 1.0;

        if (num == 0) num += solve_quadratic(coeffs[0], coeffs[1], coeffs[2], s0, s1);
        if (num == 1) num += solve_quadratic(coeffs[0], coeffs[1], coeffs[2], s1, s2);
        if (num == 2) num += solve_quadratic(coeffs[0], coeffs[1], coeffs[2], s2, s3);
    }

    sub = 1.0/4.0 * A;

    if (num > 0) s0 -= sub;
    if (num > 1) s1 -= sub;
    if (num > 2) s2 -= sub;
    if (num > 3) s3 -= sub;

    return num;
}

#endif // end solvers_hpp
