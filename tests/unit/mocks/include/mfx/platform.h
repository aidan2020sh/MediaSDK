// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mocks/include/mfx/detail/platform.hxx"

namespace mocks { namespace mfx
{
    inline
    int family(int type)
    {
        switch (type)
        {
            case HW_IVB:     return detail::IGFX_IVYBRIDGE;
            case HW_HSW:
            case HW_HSW_ULT: return detail::IGFX_HASWELL;
            case HW_VLV:     return detail::IGFX_VALLEYVIEW;
            case HW_SCL:     return detail::IGFX_SKYLAKE;
            case HW_BDW:     return detail::IGFX_BROADWELL;
            case HW_CHT:     return detail::IGFX_CHERRYVIEW;
            case HW_KBL:     return detail::IGFX_KABYLAKE;
            case HW_CFL:     return detail::IGFX_COFFEELAKE;
            case HW_APL:     return detail::IGFX_APOLLOLAKE;
            case HW_GLK:     return detail::IGFX_GEMINILAKE;
            case HW_CNL:     return detail::IGFX_CANNONLAKE;
            case HW_ICL:     return detail::IGFX_ICELAKE;
            case HW_CNX_G:   return detail::IGFX_CNX_G;
            case HW_LKF:     return detail::IGFX_LAKEFIELD;
            case HW_JSL:     return detail::IGFX_JASPERLAKE;
            case HW_TGL_LP:  return detail::IGFX_TIGERLAKE_LP;
            case HW_RYF:     return detail::IGFX_RYEFIELD;
            case HW_RKL:     return detail::IGFX_ROCKETLAKE;
            case HW_DG1:     return detail::IGFX_DG1;
            case HW_DG2:     return detail::IGFX_DG2;
            case HW_ADL_S:   return detail::IGFX_ALDERLAKE_S;
            case HW_ADL_P:   return detail::IGFX_ALDERLAKE_P;
            case HW_PVC:     return detail::IGFX_PVC;

            case HW_MTL:     return deteail::IGFX_METEORLAKE;
            case HW_ARL:     return deteail::IGFX_ARROWLAKE;
            case HW_ELG:     return detail::IGFX_ELASTICG;

            default:             return detail::IGFX_UNKNOWN;
        }
    }

    /* Invoke callable object 'F' with compile time constant of HW type and given arguments 'Args'.
       Note, if 'F' is overloaded function we need to use 'lifting' trick (MOCKS_LIFT) to avoid to compiler's restriction */
    template <typename F, typename ...Args>
    auto invoke(int type, F&& f, Args&&... args) -> decltype(auto)
    {
        switch (type)
        {
            case HW_IVB:     return std::forward<F>(f)(std::integral_constant<int, HW_IVB>{},     std::forward<Args>(args)...);
            case HW_HSW:     return std::forward<F>(f)(std::integral_constant<int, HW_HSW>{},     std::forward<Args>(args)...);
            case HW_HSW_ULT: return std::forward<F>(f)(std::integral_constant<int, HW_HSW_ULT>{}, std::forward<Args>(args)...);
            case HW_VLV:     return std::forward<F>(f)(std::integral_constant<int, HW_VLV>{},     std::forward<Args>(args)...);
            case HW_SCL:     return std::forward<F>(f)(std::integral_constant<int, HW_SCL>{},     std::forward<Args>(args)...);
            case HW_BDW:     return std::forward<F>(f)(std::integral_constant<int, HW_BDW>{},     std::forward<Args>(args)...);
            case HW_CHT:     return std::forward<F>(f)(std::integral_constant<int, HW_CHT>{},     std::forward<Args>(args)...);
            case HW_KBL:     return std::forward<F>(f)(std::integral_constant<int, HW_KBL>{},     std::forward<Args>(args)...);
            case HW_CFL:     return std::forward<F>(f)(std::integral_constant<int, HW_CFL>{},     std::forward<Args>(args)...);
            case HW_APL:     return std::forward<F>(f)(std::integral_constant<int, HW_APL>{},     std::forward<Args>(args)...);
            case HW_GLK:     return std::forward<F>(f)(std::integral_constant<int, HW_GLK>{},     std::forward<Args>(args)...);
            case HW_CNL:     return std::forward<F>(f)(std::integral_constant<int, HW_CNL>{},     std::forward<Args>(args)...);
            case HW_ICL:     return std::forward<F>(f)(std::integral_constant<int, HW_ICL>{},     std::forward<Args>(args)...);
            case HW_CNX_G:   return std::forward<F>(f)(std::integral_constant<int, HW_CNX_G>{},   std::forward<Args>(args)...);
            case HW_LKF:     return std::forward<F>(f)(std::integral_constant<int, HW_LKF>{},     std::forward<Args>(args)...);
            case HW_JSL:     return std::forward<F>(f)(std::integral_constant<int, HW_JSL>{},     std::forward<Args>(args)...);
            case HW_TGL_LP:  return std::forward<F>(f)(std::integral_constant<int, HW_TGL_LP>{},  std::forward<Args>(args)...);
            case HW_RYF:     return std::forward<F>(f)(std::integral_constant<int, HW_RYF>{},     std::forward<Args>(args)...);
            case HW_RKL:     return std::forward<F>(f)(std::integral_constant<int, HW_RKL>{},     std::forward<Args>(args)...);
            case HW_DG1:     return std::forward<F>(f)(std::integral_constant<int, HW_DG1>{},     std::forward<Args>(args)...);
            case HW_DG2:     return std::forward<F>(f)(std::integral_constant<int, HW_DG2>{},     std::forward<Args>(args)...);
            case HW_ADL_S:   return std::forward<F>(f)(std::integral_constant<int, HW_ADL_S>{},   std::forward<Args>(args)...);
            case HW_ADL_P:   return std::forward<F>(f)(std::integral_constant<int, HW_ADL_P>{},  std::forward<Args>(args)...);
            case HW_PVC:     return std::forward<F>(f)(std::integral_constant<int, HW_PVC>{},     std::forward<Args>(args)...);
            case HW_MTL:     return std::forward<F>(f)(std::integral_constant<int, HW_MTL>{},     std::forward<Args>(args)...);
            case HW_ELG:     return std::forward<F>(f)(std::integral_constant<int, HW_ELG>{},     std::forward<Args>(args)...);

            default:         return std::forward<F>(f)(std::integral_constant<int, HW_UNKNOWN>{}, std::forward<Args>(args)...);
        }
    }

    template <typename Base>
    struct gen : Base
    { using type = Base; };

    struct gen0               { using id = std::integral_constant<int,   0>; };  //fallback
    struct gen9  : gen< gen0> { using id = std::integral_constant<int,  90>; };
    struct gen10 : gen< gen9> { using id = std::integral_constant<int, 100>; };
    struct gen11 : gen<gen10> { using id = std::integral_constant<int, 110>; };
    struct gen12 : gen<gen11> { using id = std::integral_constant<int, 120>; };
    struct gen13 : gen<gen12> { using id = std::integral_constant<int, 130>; };

    /* Derive parent of given Gen accord. to gens hierarchy */
    template <typename Gen>
    struct parent_of
    { using type = typename Gen::type; };
    template <>
    struct parent_of<gen0>
    { using type = gen0; };

    template <int Type, typename Enable = void>
    struct gen_of
    { using type = gen0; };

    template <int Type>
    struct gen_of<Type, typename std::enable_if<Type >= HW_SCL && Type < HW_CNL>::type>
    { using type = gen9; };

    template <int Type>
    struct gen_of<Type, typename std::enable_if<Type >= HW_CNL && Type < HW_ICL>::type>
    { using type = gen10; };

    template <int Type>
    struct gen_of<Type, typename std::enable_if<Type >= HW_ICL && Type < HW_TGL_LP>::type>
    { using type = gen11; };

    template <int Type>
    struct gen_of<Type, typename std::enable_if<Type >= HW_TGL_LP && Type <= HW_PVC>::type>
    { using type = gen12; };

    template <int Type>
    struct gen_of<Type, typename std::enable_if<Type >= HW_MTL>::type>
    { using type = gen13; };

    /* Check whether given type 'T' belongs to 'gen' hierarchy */
    template <typename T, typename Enable = void>
    struct is_gen : std::false_type {};

    template <typename T>
    struct is_gen<T, typename std::enable_if<
        std::is_base_of<gen0, T>::value
    >::type> : std::true_type {};

} }
