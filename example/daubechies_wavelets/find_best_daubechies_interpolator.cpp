// Copyright Nick Thompson, 2020
// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <unordered_map>
#include <string>
#include <future>
#include <thread>
#include <fstream>
#include <boost/math/special_functions/daubechies_scaling.hpp>
#include <boost/math/special_functions/detail/daubechies_scaling_integer_grid.hpp>
#include <boost/math/interpolators/cubic_hermite.hpp>
#include <boost/math/interpolators/quintic_hermite.hpp>
#include <boost/math/interpolators/cardinal_quintic_hermite.hpp>
#include <boost/math/interpolators/septic_hermite.hpp>
#include <boost/math/interpolators/cardinal_quadratic_b_spline.hpp>
#include <boost/math/interpolators/cardinal_cubic_b_spline.hpp>
#include <boost/math/interpolators/cardinal_quintic_b_spline.hpp>
#include <boost/math/interpolators/whittaker_shannon.hpp>
#include <boost/math/interpolators/cardinal_trigonometric.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/math/interpolators/makima.hpp>
#include <boost/math/interpolators/pchip.hpp>
#include <boost/multiprecision/float128.hpp>
#include <boost/core/demangle.hpp>

using boost::multiprecision::float128;


template<typename Real, typename PreciseReal, int p>
void choose_refinement()
{
    std::cout << "Choosing refinement for " << boost::core::demangle(typeid(Real).name()) << " precision Daubechies scaling function with " << p << " vanishing moments.\n";
    using std::abs;
    int rmax = 21;
    auto phi_dense = boost::math::detail::dyadic_grid<PreciseReal, p, 0>(rmax);
    Real dx_dense = (2*p-1)/static_cast<Real>(phi_dense.size()-1);

    for (int r = 2; r <= rmax - 2; ++r)
    {
        auto phi_accurate = boost::math::detail::dyadic_grid<PreciseReal, p, 0>(r);
        std::vector<Real> phi(phi_accurate.size());
        for (size_t i = 0; i < phi_accurate.size(); ++i)
        {
            phi[i] = Real(phi_accurate[i]);
        }
        auto phi_prime_accurate = boost::math::detail::dyadic_grid<PreciseReal, p, 1>(r);
        std::vector<Real> phi_prime(phi_accurate.size());
        for (size_t i = 0; i < phi_prime_accurate.size(); ++i)
        {
            phi_prime[i] = Real(phi_prime_accurate[i]);
        }

        Real dx = (2*p-1)/static_cast<Real>(phi.size()-1);
        std::cout << "\tdx = 1/" << (1/dx) << " = " << dx << "\n";

        if constexpr (p < 6 && p >= 3)
        {
            auto ch = boost::math::interpolators::cardinal_cubic_hermite(std::move(phi), std::move(phi_prime), Real(0), Real(dx));
            Real flt_distance = 0;
            Real sup  = 0;
            Real worst_abscissa = 0;
            Real worst_value = 0;
            Real worst_computed = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real t = i*dx_dense;
                Real computed = ch(t);
                Real expected = Real(phi_dense[i]);
                if (std::abs(expected) < 100*std::numeric_limits<Real>::epsilon())
                {
                    continue;
                }
                Real diff = abs(computed - expected);
                Real distance = abs(boost::math::float_distance(computed, expected));
                if (distance > flt_distance)
                {
                    flt_distance = distance;
                    worst_abscissa = t;
                    worst_value = expected;
                    worst_computed = computed;
                }
                if (diff > sup)
                {
                    sup = diff;
                }
            }
            std::cout << "\t\tFloat distance at r = " << r << " is " << flt_distance << ", sup distance = " << sup << "\n";
            std::cout << "\t\tWorst abscissa = " << worst_abscissa << ", worst value = " << worst_value << ", computed = " << worst_computed << "\n";
        }
        else if constexpr (p >= 6)
        {
            auto phi_dbl_prime = boost::math::detail::dyadic_grid<Real, p, 2>(r);
            auto qh = boost::math::interpolators::cardinal_quintic_hermite(std::move(phi), std::move(phi_prime), std::move(phi_dbl_prime), Real(0), dx);
            Real flt_distance = 0;
            Real sup  = 0;
            Real worst_abscissa = 0;
            Real worst_value = 0;
            Real worst_computed = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real t = i*dx_dense;
                Real computed = qh(t);
                Real expected = Real(phi_dense[i]);
                if (std::abs(expected) < 100*std::numeric_limits<Real>::epsilon())
                {
                    continue;
                }

                Real diff = abs(computed - expected);
                Real distance = abs(boost::math::float_distance(computed, expected));
                if (distance > flt_distance)
                {
                    flt_distance = distance;
                    worst_abscissa = t;
                    worst_value = expected;
                    worst_computed = computed;
                }
                if (diff > sup)
                {
                    sup = diff;
                }
                std::cout << "Float distance at r = " << r << " is " << flt_distance << ", sup distance = " << sup << "\n";
                std::cout << "\tWorst abscissa = " << worst_abscissa << ", worst value = " << worst_value << ", computed = " << worst_computed << "\n"; 
            }
        }
    }
}

template<typename Real, typename PreciseReal, int p>
void find_best_interpolator()
{
    std::string filename = "daubechies_" + std::to_string(p) + "_scaling_convergence.csv";
    std::ofstream fs{filename};
    static_assert(sizeof(PreciseReal) >= sizeof(Real), "sizeof(PreciseReal) >= sizeof(Real) is required.");
    using std::abs;
    int rmax = 17;
    std::cout << "Computing phi_dense_precise\n";
    auto phi_dense_precise = boost::math::detail::dyadic_grid<PreciseReal, p, 0>(rmax);
    std::vector<Real> phi_dense(phi_dense_precise.size());
    for (size_t i = 0; i < phi_dense.size(); ++i)
    {
        phi_dense[i] = static_cast<Real>(phi_dense_precise[i]);
    }
    phi_dense_precise.resize(0);
    std::cout << "Done\n";

    Real dx_dense = (2*p-1)/static_cast<Real>(phi_dense.size()-1);
    fs << std::setprecision(std::numeric_limits<Real>::digits10 + 3);
    fs << std::fixed;
    fs << "r, matched_holder, linear, quadratic_b_spline, cubic_b_spline, quintic_b_spline, cubic_hermite, pchip, makima, fo_taylor";
    if (p==2)
    {
        fs << "\n";
    }
    else
    {
        fs << ", quintic_hermite, second_order_taylor";
        if (p > 3)
        {
            fs << ", third_order_taylor, septic_hermite\n";
        }
        else
        {
            fs << "\n";
        }
    }
    for (int r = 2; r < rmax-1; ++r)
    {
        fs << r << ", ";
        std::map<Real, std::string> m;
        auto phi = boost::math::detail::dyadic_grid<Real, p, 0>(r);
        auto phi_prime = boost::math::detail::dyadic_grid<Real, p, 1>(r);

        std::vector<Real> x(phi.size());
        Real dx = (2*p-1)/static_cast<Real>(x.size()-1);
        std::cout << "dx = 1/" << (1 << r) << " = " << dx << "\n";
        for (size_t i = 0; i < x.size(); ++i)
        {
            x[i] = i*dx;
        }

        {
            auto phi_copy = phi;
            auto phi_prime_copy = phi_prime;
            auto mh = boost::math::detail::matched_holder(std::move(phi_copy), std::move(phi_prime_copy), r);
            Real sup = 0;
            // call to matched_holder is unchecked, so only go to phi_dense.size() -1.
            for (size_t i = 0; i < phi_dense.size() - 1; ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - mh(x));
                if (diff > sup)
                {
                    sup = diff;
                }
            }
            m.insert({sup, "matched_holder"});
            fs << sup << ", ";
        }


        {
            auto linear = [&phi, &dx, &r](Real x)->Real {
              if (x <= 0 || x >= 2*p-1)
              {
                return Real(0);
              }
              using std::floor;

              Real y = (1<<r)*x;
              Real k = floor(y);

              size_t kk = static_cast<size_t>(k);

              Real t = y - k;
              return (1-t)*phi[kk] + t*phi[kk+1];
            };

            Real linear_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - linear(x));
                if (diff > linear_sup)
                {
                    linear_sup = diff;
                }
            }
            m.insert({linear_sup, "linear interpolation"});
            fs << linear_sup << ", ";
        }
        

        {
            auto qbs = boost::math::interpolators::cardinal_quadratic_b_spline(phi.data(), phi.size(), Real(0), dx, phi_prime.front(), phi_prime.back());
            Real qbs_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - qbs(x));
                if (diff > qbs_sup) {
                    qbs_sup = diff;
                }
            }
            m.insert({qbs_sup, "quadratic_b_spline"});
            fs << qbs_sup << ", ";
        }

        {
            auto cbs = boost::math::interpolators::cardinal_cubic_b_spline(phi.data(), phi.size(), Real(0), dx, phi_prime.front(), phi_prime.back());
            Real cbs_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - cbs(x));
                if (diff > cbs_sup)
                {
                    cbs_sup = diff;
                }
            }
            m.insert({cbs_sup, "cubic_b_spline"});
            fs << cbs_sup << ", ";
        }

        {
            auto qbs = boost::math::interpolators::cardinal_quintic_b_spline(phi.data(), phi.size(), Real(0), dx, {0,0}, {0,0});
            Real qbs_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - qbs(x));
                if (diff > qbs_sup)
                {
                    qbs_sup = diff;
                }
            }
            m.insert({qbs_sup, "quintic_b_spline"});
            fs << qbs_sup << ", ";
        }

        {
            auto phi_copy = phi;
            auto phi_prime_copy = phi_prime;
            auto ch = boost::math::interpolators::cardinal_cubic_hermite(std::move(phi_copy), std::move(phi_prime_copy), Real(0), dx);
            Real chs_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - ch(x));
                if (diff > chs_sup)
                {
                    chs_sup = diff;
                }
            }
            m.insert({chs_sup, "cubic_hermite_spline"});
            fs << chs_sup << ", ";
        }

        {
            auto phi_copy = phi;
            auto x_copy = x;
            auto phi_prime_copy = phi_prime;
            auto pc = boost::math::interpolators::pchip(std::move(x_copy), std::move(phi_copy));
            Real pchip_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - pc(x));
                if (diff > pchip_sup)
                {
                  pchip_sup = diff;
                }
            }
            m.insert({pchip_sup, "pchip"});
            fs << pchip_sup << ", ";
        }

        {
            auto phi_copy = phi;
            auto x_copy = x;
            auto pc = boost::math::interpolators::makima(std::move(x_copy), std::move(phi_copy));
            Real makima_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i) {
              Real x = i*dx_dense;
              Real diff = abs(phi_dense[i] - pc(x));
              if (diff > makima_sup)
              {
                  makima_sup = diff;
              }
            }
            m.insert({makima_sup, "makima"});
            fs << makima_sup << ", ";
        }

        // Whittaker-Shannon interpolation has linear complexity; test over all points and it's quadratic.
        // I ran this a couple times and found it's not competitive; so comment out for now.
        /*{
            auto phi_copy = phi;
            auto ws = boost::math::interpolators::whittaker_shannon(std::move(phi_copy), Real(0), dx);
            Real sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i) {
              Real x = i*dx_dense;
              using std::abs;
              Real diff = abs(phi_dense[i] - ws(x));
              if (diff > sup) {
                sup = diff;
              }
            }
          
            m.insert({sup, "whittaker_shannon"});
        }

        // Again, linear complexity of evaluation => quadratic complexity of exhaustive checking.
        {
            auto trig = boost::math::interpolators::cardinal_trigonometric(phi, Real(0), dx);
            Real sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i) {
              Real x = i*dx_dense;
              using std::abs;
              Real diff = abs(phi_dense[i] - trig(x));
              if (diff > sup) {
                sup = diff;
              }
            }
            m.insert({sup, "trig"});
        }*/

        {
            auto fotaylor = [&phi, &phi_prime, &r](Real x)->Real
            {
                if (x <= 0 || x >= 2*p-1)
                {
                    return 0;
                }
                using std::floor;

                Real y = (1<<r)*x;
                Real k = floor(y);

                size_t kk = static_cast<size_t>(k);
                if (y - k < k + 1 - y)
                {
                    Real eps = (y-k)/(1<<r);
                    return phi[kk] + eps*phi_prime[kk];
                }
                else {
                    Real eps = (y-k-1)/(1<<r);
                    return phi[kk+1] + eps*phi_prime[kk+1];
                }
            };
            Real fo_sup = 0;
            for (size_t i = 0; i < phi_dense.size(); ++i)
            {
                Real x = i*dx_dense;
                Real diff = abs(phi_dense[i] - fotaylor(x));
                if (diff > fo_sup)
                {
                  fo_sup = diff;
                }
            }
            m.insert({fo_sup, "First-order Taylor"});
            if (p==2)
            {
                fs << fo_sup << "\n";
            }
            else
            {
                fs << fo_sup << ", ";
            }
        }

        if constexpr (p > 2) {
            auto phi_dbl_prime = boost::math::detail::dyadic_grid<Real, p, 2>(r);

            {
                auto phi_copy = phi;
                auto phi_prime_copy = phi_prime;
                auto phi_dbl_prime_copy = phi_dbl_prime;
                auto qh = boost::math::interpolators::cardinal_quintic_hermite(std::move(phi_copy), std::move(phi_prime_copy), std::move(phi_dbl_prime_copy), Real(0), dx);
                Real qh_sup = 0;
                for (size_t i = 0; i < phi_dense.size(); ++i)
                {
                    Real x = i*dx_dense;
                    Real diff = abs(phi_dense[i] - qh(x));
                    if (diff > qh_sup)
                    {
                        qh_sup = diff;
                    }
                }
                m.insert({qh_sup, "quintic_hermite_spline"});
                fs << qh_sup << ", ";
            }

            {
                auto sotaylor = [&phi, &phi_prime, &phi_dbl_prime, &r](Real x)->Real {
                      if (x <= 0 || x >= 2*p-1)
                      {
                          return 0;
                      }
                      using std::floor;

                      Real y = (1<<r)*x;
                      Real k = floor(y);

                      size_t kk = static_cast<size_t>(k);
                      if (y - k < k + 1 - y)
                      {
                          Real eps = (y-k)/(1<<r);
                          return phi[kk] + eps*phi_prime[kk] + eps*eps*phi_dbl_prime[kk]/2;
                      }
                      else {
                          Real eps = (y-k-1)/(1<<r);
                          return phi[kk+1] + eps*phi_prime[kk+1] + eps*eps*phi_dbl_prime[kk+1]/2;
                      }
                };
                Real so_sup = 0;
                for (size_t i = 0; i < phi_dense.size(); ++i)
                {
                    Real x = i*dx_dense;
                    Real diff = abs(phi_dense[i] - sotaylor(x));
                    if (diff > so_sup)
                    {
                        so_sup = diff;
                    }
                }
                m.insert({so_sup, "Second-order Taylor"});
                if (p > 3)
                {
                    fs << so_sup << ", ";  
                }
                else
                {
                    fs << so_sup << "\n";
                }
                
            }
        }

        if constexpr (p > 3)
        {
            auto phi_dbl_prime = boost::math::detail::dyadic_grid<Real, p, 2>(r);
            auto phi_triple_prime = boost::math::detail::dyadic_grid<Real, p, 3>(r);

            {
                auto totaylor = [&phi, &phi_prime, &phi_dbl_prime, &phi_triple_prime, &r](Real x)->Real {
                      if (x <= 0 || x >= 2*p-1) {
                          return 0;
                      }
                      using std::floor;

                      Real y = (1<<r)*x;
                      Real k = floor(y);

                      size_t kk = static_cast<size_t>(k);
                      if (y - k < k + 1 - y)
                      {
                          Real eps = (y-k)/(1<<r);
                          return phi[kk] + eps*phi_prime[kk] + eps*eps*phi_dbl_prime[kk]/2 + eps*eps*eps*phi_triple_prime[kk]/6;
                      }
                      else {
                          Real eps = (y-k-1)/(1<<r);
                          return phi[kk+1] + eps*phi_prime[kk+1] + eps*eps*phi_dbl_prime[kk+1]/2 + eps*eps*eps*phi_triple_prime[kk]/6;
                      }
                };
                Real to_sup = 0;
                for (size_t i = 0; i < phi_dense.size(); ++i)
                {
                    Real x = i*dx_dense;
                    Real diff = abs(phi_dense[i] - totaylor(x));
                    if (diff > to_sup)
                    {
                        to_sup = diff;
                    }
                }
            
                m.insert({to_sup, "Third-order Taylor"});
                fs << to_sup << ", ";
            }

            {
                auto phi_copy = phi;
                auto phi_prime_copy = phi_prime;
                auto phi_dbl_prime_copy = phi_dbl_prime;
                auto phi_triple_prime_copy = phi_triple_prime;
                auto sh = boost::math::interpolators::cardinal_septic_hermite(std::move(phi_copy), std::move(phi_prime_copy), std::move(phi_dbl_prime_copy), std::move(phi_triple_prime_copy), Real(0), dx);
                Real septic_sup = 0;
                for (size_t i = 0; i < phi_dense.size(); ++i)
                {
                    Real x = i*dx_dense;
                    Real diff = abs(phi_dense[i] - sh(x));
                    if (diff > septic_sup)
                    {
                        septic_sup = diff;
                    }
                }
                m.insert({septic_sup, "septic_hermite_spline"});
                fs << septic_sup << "\n";
            }


        }
        std::string best = "none";
        Real best_sup = 1000000000;
        std::cout << std::setprecision(std::numeric_limits<Real>::digits10 + 3) << std::fixed;
        for (auto & e : m)
        {
            std::cout << "\t" << e.first << " is error of " << e.second << "\n";
            if (e.first < best_sup)
            {
                best = e.second;
                best_sup = e.first;
            }
        }
        std::cout << "\tThe best method for p = " << p << " is the " << best << "\n";
    }
}

int main() {
    //choose_refinement<float, double, 5>();
    //choose_refinement<float, long double, 5>();
    //choose_refinement<double, float128, 15>();
    // Says linear interpolation is the best:
    find_best_interpolator<float128, float128, 2>();
    // Says linear interpolation is the best:
    find_best_interpolator<float128, float128, 3>();
    // Says cubic_hermite_spline is best:
    find_best_interpolator<float128, float128, 4>();
    // Says cubic_hermite_spline is best:
    find_best_interpolator<float128, float128, 5>();
    // Says quintic_hermite_spline is best:
    find_best_interpolator<float128, float128, 6>();
    // Says quintic_hermite_spline is best:
    find_best_interpolator<float128, float128, 7>();
    // Says quintic_hermite_spline is best:
    find_best_interpolator<float128, float128, 8>();
    // Says quintic_hermite_spline is best:
    find_best_interpolator<float128, float128, 9>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 10>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 11>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 12>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 13>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 14>();
    // Says septic_hermite_spline is best:
    find_best_interpolator<float128, float128, 15>();
}
