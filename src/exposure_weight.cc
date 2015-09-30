/*
 * Copyright (C) 2015 Christoph L. Spiel
 *
 * This file is part of Enblend.
 *
 * Enblend is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Enblend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Enblend; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <cassert>
#include <iostream>

#include "global.h"
#include "openmp_def.h"         // omp::atomic_t

#include "exposure_weight.h"
#include "dynamic_loader.h"    // HAVE_DYNAMICLOADER_IMPL


extern const std::string command;
extern ExposureWeight* ExposureWeightFunction;


namespace exposure_weight
{
#ifdef HAVE_DYNAMICLOADER_IMPL
    class DynamicExposureWeight : public ExposureWeight
    {
    public:
        DynamicExposureWeight() = delete;

        DynamicExposureWeight(const std::string& library_name, const std::string& symbol_name) :
            ExposureWeight(0.5, 0.25),
            library(library_name), symbol(symbol_name),
            dynamic_loader(DynamicLoader(library_name)),
            function(dynamic_loader.resolve<ExposureWeight*>(symbol_name))
        {}

        DynamicExposureWeight(const std::string& library_name, const std::string& symbol_name,
                              double y_optimum, double width) :
            ExposureWeight(y_optimum, width),
            library(library_name), symbol(symbol_name),
            dynamic_loader(DynamicLoader(library_name)),
            function(dynamic_loader.resolve<ExposureWeight*>(symbol_name))
        {}

        void initialize(double y_optimum, double width_parameter,
                        const argument_list_t& argument_list = argument_list_t()) override
        {
            std::cout << "+ DynamicExposureWeight::initialize\n";
            function->initialize(y_optimum, width_parameter, argument_list);
        }

        double weight(double y) override {return function->weight(y);}

    private:
        std::string library;
        std::string symbol;
        DynamicLoader dynamic_loader;
        ExposureWeight* function;
    };


    static ExposureWeight*
    make_dynamic_weight_function(const std::string& name,
                                 const ExposureWeight::argument_list_t& arguments,
                                 double y_optimum, double width)
    {
        if (arguments.empty())
        {
            std::cerr << command << ": unknown built-in exposure weight function \"" << name << "\""
                      << std::endl;
            exit(1);
        }
        else
        {
            const std::string symbol_name(arguments.front());
            ExposureWeight::argument_list_t user_arguments;
            std::copy(std::next(arguments.begin()), arguments.end(), back_inserter(user_arguments));

            ExposureWeight* weight_object;
            try
            {
                weight_object = new DynamicExposureWeight(name, symbol_name);
                weight_object->initialize(y_optimum, width, user_arguments);
            }
            catch (ExposureWeight::error& exception)
            {
                std::cerr << command <<
                    ": user-defined weight function \"" << symbol_name <<
                    "\" defined in shared object \"" << name <<
                    "\" raised exception: " << exception.what() << std::endl;
                exit(1);
            }

            return weight_object;
        }
    }
#endif // HAVE_DYNAMICLOADER_IMPL


    ExposureWeight*
    make_weight_function(const std::string& name,
                         const ExposureWeight::argument_list_t& arguments,
                         double y_optimum, double width)
    {
        delete ExposureWeightFunction;

        std::string possible_built_in(name);
        enblend::to_lower(possible_built_in);

        if (possible_built_in == "gauss" || possible_built_in == "gaussian")
        {
            return new Gaussian(y_optimum, width);
        }
        else if (possible_built_in == "lorentz" || possible_built_in == "lorentzian")
        {
            return new Lorentzian(y_optimum, width);
        }
        else if (possible_built_in == "halfsine" || possible_built_in == "half-sine")
        {
            return new HalfSinusodial(y_optimum, width);
        }
        else if (possible_built_in == "fullsine" || possible_built_in == "full-sine")
        {
            return new FullSinusodial(y_optimum, width);
        }
        else if (possible_built_in == "bisquare" || possible_built_in == "bi-square")
        {
            return new Bisquare(y_optimum, width);
        }
        else
        {
#ifdef HAVE_DYNAMICLOADER_IMPL
            return make_dynamic_weight_function(name, arguments, y_optimum, width);
#else
            std::cerr << command << ": unknown built-in exposure weight function \"" << name << "\"\n"
                      << command << ": note: this binary has no support for dynamic loading of\n"
                      << command << ": note: exposure weight functions" << std::endl;
            exit(1);
#endif // HAVE_DYNAMICLOADER_IMPL
        }
    }


    void
    dump_weight_function(ExposureWeight* weight_function, int n)
    {
        assert(n >= 2);

        for (int i = 0; i < n; ++i)
        {
            const double x = static_cast<double>(i) / static_cast<double>(n - 1);
            const double w = weight_function->weight(x);

            std::cout << i << ' ' << x << ' ' << w << '\n';
        }
    }


    bool
    check_weight_function(ExposureWeight* weight_function, int n)
    {
        assert(n >= 2);

        omp::atomic_t number_of_faults = omp::atomic_t();

#ifdef OPENMP
#pragma omp parallel for
#endif
        for (int i = 0; i < n; ++i)
        {
            const double y = static_cast<double>(i) / static_cast<double>(n - 1);
            const double w = weight_function->weight(y);

            if (w < 0.0 || w >= 1.0)
            {
                ++number_of_faults;
            }
        }

        return number_of_faults == omp::atomic_t();
    }
} // namespace exposure_weight
