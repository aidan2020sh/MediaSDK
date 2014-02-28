//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <vector>
#include <sample_utils.h>
#include <iomanip>
#include "generic_types_to_text.h"

namespace detail {
    template <class T> struct InternalParser;
};


//interface to work with particular option description
class OptionHandler {
    template <class T>
    friend struct detail::InternalParser;
public:
    virtual ~OptionHandler(){}
    //option name that should be unique to start with
    virtual msdk_string GetName()const = 0;
    //returns number of bytes taken from params
    virtual size_t  Handle(const msdk_string & )  = 0;
    virtual void PrintHelp(msdk_ostream & , size_t optAlign) const = 0;
    virtual bool Exist() const = 0;
    
    template <class T>
    T as () const;
protected:
    //returns value tokens
    virtual msdk_string  GetValue(int) const = 0;
    virtual bool CanHaveDelimiters()const = 0;
};

namespace detail {

    class OptionHandlerBase : public OptionHandler {
    protected:
        msdk_string m_desc;
        msdk_string m_name;
        bool m_bHandled;
        msdk_string m_value;
    public:
        OptionHandlerBase (const msdk_string & name, const msdk_string &description) 
            : m_desc(description)
            , m_name(name)
            , m_bHandled(false) {
        }
        OptionHandlerBase (const OptionHandlerBase & that) 
            : m_desc(that.m_desc)
            , m_name(that.m_name)
            , m_bHandled(that.m_bHandled)
            , m_value(that.m_value) {

        }
        virtual msdk_string GetName()const {
            return m_name;
        }
        virtual size_t Handle(const msdk_string & str);

        virtual bool Exist() const {
            return m_bHandled;
        }

        //returns value tokens
        virtual msdk_string GetValue(int) const {
            return m_value;
        }
        virtual bool CanHaveDelimiters()const {
            return false;
        }
    };

    template <class T>
    class OptionHandlerBaseTmpl : public OptionHandlerBase {
    public:
        OptionHandlerBaseTmpl(const msdk_string & name, const msdk_string &description) 
            : detail::OptionHandlerBase(name, description) {
        }
        virtual void PrintHelp(msdk_ostream & os, size_t alignLen) const  {
            msdk_stringstream ss;
            ss <<MSDK_CHAR(" [")<<TypeToText<T>()<<MSDK_CHAR("]");
            os<<std::left<<std::setw(alignLen)<< m_name<<std::left<<std::setw(10)<<ss.str()<<m_desc;
        }
    };

    template <class T>
    struct InternalParser {
        T m_value;
        InternalParser(const OptionHandler & refHdl)  {
            msdk_stringstream s(refHdl.GetValue(0) + MSDK_CHAR(' '));
            s >> m_value;
        }
        operator T() const {
            return m_value;
        }
    };

    template <>
    struct InternalParser<msdk_string> {
        msdk_string m_value;
        InternalParser(const OptionHandler & refHdl)  {
            m_value = refHdl.GetValue(0);
        }
        operator msdk_string() const {
            return m_value;
        }
    };

    template <class T>
    struct InternalParser<std::vector<T> >{
        std::vector<T> m_value;
        InternalParser(const OptionHandler & refHdl) {
            for (int i = 0;;i++) {
                msdk_string s = refHdl.GetValue(i);
                if (s.empty())
                    break;
                msdk_stringstream ss(s + MSDK_CHAR(' '));
                m_value.push_back(T());
                ss >> m_value.back();
            }
        }
        operator std::vector<T> () const {
            return m_value;
        }
    };
    template<>
    struct InternalParser<std::vector<msdk_string> >  {
        std::vector<msdk_string> m_value;
        InternalParser(const OptionHandler & refHdl) {
            for (int i = 0;;i++) {
                msdk_string s = refHdl.GetValue(i);
                if (s.empty())
                    break;
                m_value.push_back(s);
            }
        }
        operator std::vector<msdk_string> () const {
            return m_value;
        }
    };

    template<class T>
    struct CanHaveDelimiters {
        enum {value = 0};
    };
    
    template<>
    struct CanHaveDelimiters<msdk_string> {
        enum {value = 1};
    };
}

template <class T>
T OptionHandler::as () const {
    if (!Exist()) {
        return T();
    }
    detail::InternalParser<T> obj(*this);
    return  obj;
} 


template <class T>
class ArgHandler : public detail::OptionHandlerBaseTmpl<T> {
public:
    ArgHandler(const msdk_string & name, const msdk_string &description) 
        : detail::OptionHandlerBaseTmpl<T>(name, description) {
    }
};

template <>
class ArgHandler<msdk_string> : public detail::OptionHandlerBaseTmpl<msdk_string> {
public:
    ArgHandler(const msdk_string & name, const msdk_string &description) 
        : detail::OptionHandlerBaseTmpl<msdk_string>(name, description) {
    }

protected:
    virtual bool CanHaveDelimiters()const {
        return true;
    }
};

template<>
class ArgHandler<bool> : public detail::OptionHandlerBase {
public:
    ArgHandler(const msdk_string & name, const msdk_string &description) 
        : detail::OptionHandlerBase(name, description) {
    }
    virtual size_t Handle(const msdk_string & str);
    virtual void PrintHelp(msdk_ostream & os, size_t alignLen) const  {
        os<<std::left<<std::setw(alignLen + 10)<<m_name<<m_desc;
    }
};

template<class T>
class ArgHandler<std::vector<T> > : public detail::OptionHandlerBaseTmpl<T> {
    std::vector<msdk_string> m_vec_values;
public:
    ArgHandler(const msdk_string & name, const msdk_string &description) 
        : detail::OptionHandlerBaseTmpl<T>(name, description) {
    }
    virtual size_t Handle(const msdk_string & str) {
        this->m_bHandled = false;
        int nParsed = 0;
        if (0 != (nParsed = detail::OptionHandlerBaseTmpl<T>::Handle(str))) {
            m_vec_values.push_back(this->m_value);
        }
        return nParsed;
    }
protected:
    virtual msdk_string GetValue(int n) const {
        if ((size_t)n >= m_vec_values.size())
            return msdk_string();

        return m_vec_values[n];
    }
    virtual bool CanHaveDelimiters()const {
        return detail::CanHaveDelimiters<T>::value;
    }
};
