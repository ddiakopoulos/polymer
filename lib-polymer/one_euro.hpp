/*
 * Quoted from https://hal.inria.fr/hal-00670496/file/CHI2012-casiez.pdf:
 *   "To minimize jitter and lag when tracking human motion, the two parameters can be set using a simple
 *   two-step procedure. First is set to 0 and fcmin to a reasonable middle-ground value such as 1 Hz.
 *   Then the body part is held steady or moved at a very low speed while fcmin is adjusted to re-move
 *   jitter and preserve an acceptable lag during these slow movements. Next, the body part is moved
 *   quickly in different directions while is increased with a focus on minimizing lag. Note that
 *   parameters fcmin and have clear conceptual relationships: if high speed lag is a problem,
 *   increase; if slow speed jitter is a problem, decrease fcmin."
 */ 

#pragma once

#ifndef one_euro_filter_h
#define one_euro_filter_h

#include "math-common.hpp"

namespace impl
{
    using namespace linalg;

    template<typename T, int N>
    class low_pass
    {
    protected:
        bool firstTime;
        vec<T, N> value;
        static const int dimension = N;
    public:
        low_pass() : firstTime(true) { }

        void reset() { firstTime = true; }

        vec<T, N> filter(const vec<T, N> x, float alpha)
        {
            if (firstTime)
            {
                firstTime = false;
                value = x;
            }

            vec<T, N> hatx;
            for (int i = 0; i < dimension; ++i)
                hatx[i] = alpha * x[i] + (1 - alpha) * value[i];

            value = hatx;
            return value;
        }

        vec<T, N> hatxprev() { return value; }
    };

    template<typename T, int N>
    struct vector_filterable : public low_pass<T, N>
    {
        static void set_dx_identity(vec<T, N> & dx)
        {
            for (int i = 0; i < low_pass<T, N>::dimension; ++i) dx[i] = 0;
        }

        static void compute_derivative(vec<T, N> dx, vec<T, N> prev, const  vec<T, N> current, float dt)
        {
            for (int i = 0; i < low_pass<T, N>::dimension; ++i) dx[i] = (current[i] - prev[i]) / dt;
        }

        static float compute_derivative_mag(vec<T, N> const dx)
        {
            float sqnorm = 0.f;
            for (int i = 0; i < low_pass<T, N>::dimension; ++i) sqnorm += dx[i] * dx[i];
            return sqrtf(sqnorm);
        }

    };

    template<typename T, int N>
    struct quaternion_filterable : public low_pass<T, N>
    {
        static void set_dx_identity(vec<T, N> & dx) { dx = {0, 0, 0, 1}; }

        static void compute_derivative(vec<T, 4> dx, vec<T, 4> prev, const vec<T, 4> current, float dt)
        {
            const float rate = 1.0f / dt;
            dx = qmul(current, qinv(prev));

            // nlerp instead of slerp
            dx.x *= rate;
            dx.y *= rate;
            dx.z *= rate;
            dx.w = dx.w * rate + (1.0f - rate);

            dx = linalg::normalize(dx);
        }

        static float compute_derivative_mag(vec<T, N> const dx)
        {
            return 2.0f * acosf(static_cast<float>(dx.w)); // Should be safe since the quaternion we're given has been normalized.
        }
    };

    template<typename Filterable>
    class one_euro_filter
    {
    protected:
        bool firstTime {true};
        float minCutoff;
        float derivCutoff;
        float betaCoeff;

        Filterable xFilter;
        Filterable dxFilter;

        static float alpha(float dt, float cutoff)
        {
            const float myTau = 1.f / 2.f * ((POLYMER_TAU * 0.5f) * cutoff);
            return 1.f / (1.f + myTau / dt);
        }

    public:
        one_euro_filter(float mincutoff, float beta, float dcutoff) : minCutoff(mincutoff), derivCutoff(dcutoff), betaCoeff(beta) {};

        void reset() { firstTime = true; }

        void set_parameters(float mincutoff, float beta, float dcutoff)
        {
            minCutoff = mincutoff;
            betaCoeff = beta;
            derivCutoff = dcutoff;
        }
    };

}

template<typename T, int N>
struct one_euro_filter_vec : public impl::one_euro_filter< impl::vector_filterable<T, N> >
{
    one_euro_filter_vec() : impl::one_euro_filter< impl::vector_filterable<T, N> >(1.0f, 0.05f, 1.0f) { }

    const linalg::vec<T, N> filter(float dt, const linalg::vec<T, N> x)
    {
        linalg::vec<T, N> dx;

        if (this->firstTime)
        {
            this->firstTime = false;
            impl::vector_filterable<T, N>::set_dx_identity(dx);
        }
        else
        {
            impl::vector_filterable<T, N>::compute_derivative(dx, this->xFilter.hatxprev(), x, dt);
        }

        float derivMag = impl::vector_filterable<T, N>::compute_derivative_mag(this->dxFilter.filter(dx, this->alpha(dt, this->derivCutoff)));
        float cutoff = this->minCutoff + this->betaCoeff * derivMag;

        auto returnedVal = this->xFilter.filter(x, this->alpha(dt, cutoff));
        return returnedVal;
    }
};

template<typename T>
struct one_euro_filter_quat : public impl::one_euro_filter<impl::quaternion_filterable<T, 4>>
{
    one_euro_filter_quat() : impl::one_euro_filter< impl::quaternion_filterable<T, 4> >(1.0f, 0.05f, 1.0f) { }

    linalg::vec<T, 4> hatxPrev;

    const linalg::vec<T, 4> filter(float dt, const linalg::vec<T, 4> x)
    {
        linalg::vec<T, 4> dx;

        if (this->firstTime)
        {
            this->firstTime = false;
            hatxPrev = x;
            impl::quaternion_filterable<T, 4>::set_dx_identity(dx);
        }
        else
        {
            impl::quaternion_filterable<T, 4>::compute_derivative(dx, this->xFilter.hatxprev(), x, dt);
        }

        float derivMag = impl::quaternion_filterable<T, 4>::compute_derivative_mag(this->dxFilter.filter(dx, this->alpha(dt, this->derivCutoff)));
        float cutoff = this->minCutoff + this->betaCoeff * derivMag;

        linalg::vec<T, 4> hatx = linalg::qslerp(hatxPrev, x, this->alpha(dt, cutoff));
        hatxPrev = hatx;
        return hatxPrev;
    }
};

#endif // end one_euro_filter_h
