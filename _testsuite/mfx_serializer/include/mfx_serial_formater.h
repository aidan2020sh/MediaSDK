/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_serial_formater.h

*******************************************************************************/

#include <string>
#include <sstream>

#include "hash_array.h"
#include "yaml-cpp/yaml.h"
#include "mfx_serial_utils.h"

//formaters used to complete actual serialisation to particula output in particular form

namespace Formater
{
    //default formatter
    class SimpleString 
    {
    protected:
        int m_align;
        std::string m_prefix;
        std::string m_delimiter;
    public:
        SimpleString(int key_alignment = 40, const std::string & prefix = "", const std::string & delimiter = ": ")
            : m_align(key_alignment)
            , m_prefix(prefix)
            , m_delimiter(delimiter)
        {}

        std::string operator () (const hash_array<std::string,std::string> & elements) const 
        {
            std::stringstream stream;
            hash_array<std::string,std::string>::const_iterator it;
            for(it = elements.begin(); it != elements.end(); it++)
            {
                stream << m_prefix << std::left << std::setw(m_align) << it->first << m_delimiter << it->second << std::endl;
            }
            return stream.str();
        }
    };

    class Yaml 
    {
    protected:
        std::string m_struct_name;
        hash_array<std::string,std::string> m_elements;
        hash_array<std::string,std::string>::const_iterator m_it;

        YAML::Emitter m_out;
    public:
        Yaml(const std::string & struct_name = "UNNAMED")
            : m_struct_name(struct_name)
            , m_elements()
            , m_out()
        {}

        std::string operator () (const hash_array<std::string,std::string> & elements)
        {
            m_elements = elements;
            m_it       = m_elements.begin();

            m_out << YAML::BeginMap;
            m_out << YAML::Key << m_struct_name.c_str();
            m_out << YAML::Value;
            
            if ((elements.size() == 1) && (m_it->first == ""))
            {
                m_out << m_it->second;
            }
            else
            {
                m_out << YAML::BeginMap;
                build_yaml();
                m_out << YAML::EndMap;
            }

            return m_out.c_str();
        }

    private:
        void build_yaml()
        {
            std::string prefix, cur_prefix, key;

            while (m_it != m_elements.end())
            {
                if (m_it->first.rfind(".") == std::string::npos)
                {
                    cur_prefix = "";
                    key        = m_it->first;
                }
                else
                {
                    cur_prefix = m_it->first.substr(0, m_it->first.rfind(".") + 1);
                    key        = m_it->first.substr(   m_it->first.rfind(".") + 1);
                }

                if (cur_prefix.find("[0]") != std::string::npos) 
                {
                    save_complex_sequence(cur_prefix.substr(0, cur_prefix.find("[0]")));
                }
                else if (cur_prefix == prefix)
                {
                    push_value(key, m_it->second);
                    m_it++;
                }
                else
                {
                    align_position(prefix, cur_prefix);
                }
            }
        };

        void push_value(std::string key, std::string value)
        {
            if (value.find(",") != std::string::npos)
            {
                m_out << YAML::Key << key;
                save_simple_sequence(value);
            }
            else 
            {
                m_out << YAML::Key   << key;
                m_out << YAML::Value << value;
            }
        }

        void align_position(std::string & prefix, const std::string & cur_prefix)
        {
            std::string field;

            if (cur_prefix.find(prefix) != std::string::npos)
            {
                while (prefix != cur_prefix)
                {
                    field = cur_prefix.substr(prefix.size());
                    field = field.substr(0, field.find("."));
                    m_out << YAML::Key << field.c_str();
                    m_out << YAML::Value << YAML::BeginMap;
                    prefix = prefix + field + ".";
                }
            }
            else
            {
                while (cur_prefix.find(prefix) == std::string::npos)
                {
                    m_out << YAML::EndMap;
                    if (prefix.rfind(".") == std::string::npos)
                        prefix = "";
                    else
                        prefix = prefix.substr(0, prefix.rfind(".", prefix.size() - 2) + 1);
                }
            }
        }

        void save_simple_sequence(std::string value)
        {
            std::vector<mfxI32> values;
            size_t start = 0, end = 0;

            while ((end = value.find(",", start)) != std::string::npos)
            {
                values.push_back(atoi(value.substr(start, end - start).c_str()));
                start = end + 1;
            }
            values.push_back(atoi(value.substr(start).c_str()));

            m_out << YAML::Value << YAML::Flow << values;
        }

        void save_complex_sequence(std::string prefix)
        {
            m_out << YAML::Key << prefix;
            m_out << YAML::Value << YAML::BeginSeq << YAML::BeginMap;
            std::string seq_key, seq_value, idx, next_idx;

            idx = m_it->first.substr(m_it->first.find("[") + 1, m_it->first.find("]") - m_it->first.find("[") - 1);

            while(1)
            {
                seq_key   = m_it->first.substr(m_it->first.rfind(".") + 1);
                seq_value = m_it->second;
                m_out << YAML::Key << seq_key;

                if (seq_value.find(",") != std::string::npos)
                {
                    save_simple_sequence(seq_value);
                }
                else 
                {
                    m_out << YAML::Value << seq_value;
                }

                m_it++;
                if (m_it->first.find("[") == std::string::npos) break;

                next_idx = m_it->first.substr(m_it->first.find("[") + 1, m_it->first.find("]") - m_it->first.find("[") - 1);

                if (idx != next_idx)
                {
                    m_out << YAML::EndMap << YAML::BeginMap;
                    idx = next_idx;
                }
            }

            m_out << YAML::EndMap << YAML::EndSeq;
        }
    };

    class MapCopier
    {
    protected:
        hash_array<std::string,std::string> * m_refIn;
    public:
        MapCopier(hash_array<std::string,std::string> & inmap)
            : m_refIn(&inmap)
        {}

        std::string operator () (const hash_array<std::string,std::string> & elements) 
        {
            //map coping
            m_refIn->insert(m_refIn->end(), elements.begin(), elements.end());
            return "";
        }
    };

    class KeyPrefix
        : public MapCopier
    {
        std::string m_prefix;
    public:
        KeyPrefix(const std::string & prefix, hash_array<std::string,std::string> & inmap)
            : MapCopier(inmap)
            , m_prefix(prefix)
        {}

        std::string operator () (const hash_array<std::string,std::string> & elements)
        {
            hash_array<std::string, std::string>::const_iterator it;
            for(it = elements.begin(); it != elements.end(); it++)
            {
                (*m_refIn)[m_prefix+it->first] = it->second;
            }
            return "";
        }
    };
}

namespace DeFormater
{
    //default deformatter
    class SimpleString 
    {
    protected:
        std::string m_prefix;
        std::string m_delimiter;
    public:
        SimpleString(const std::string & prefix = "", const std::string & delimiter = ": ")
            : m_prefix(prefix)
            , m_delimiter(delimiter)
        {}

        hash_array<std::string,std::string> operator () (const std::string & in) const 
        {
            std::stringstream stream(in.c_str());
            std::string line;
            hash_array<std::string,std::string> hash;
            size_t delim_pos, space_pos;

            while (std::getline(stream, line))
            {
                if (line.find(m_prefix) != 0)
                    continue;

                delim_pos = line.find(m_delimiter);
                space_pos = line.find(" ");
                hash[line.substr(m_prefix.size(), MIN(delim_pos, space_pos) - m_prefix.size())] = line.substr(delim_pos + m_delimiter.size());
            }

            return hash;
        }
    };

    class Yaml 
    {
    private:
        std::string m_struct_name;
    public:
        Yaml(const std::string & struct_name = "UNNAMED")
            : m_struct_name(struct_name)
        {}

        hash_array<std::string,std::string> operator () (const std::string & in)
        {
            hash_array<std::string,std::string> elements;

            std::stringstream stream(in.c_str());
            YAML::Parser parser(stream);
            YAML::Node doc;

            parser.GetNextDocument(doc);

            if (doc.FindValue(m_struct_name))
            {
                switch (doc[m_struct_name].Type())
                {
                case (YAML::NodeType::Scalar):
                    doc[m_struct_name].GetScalar(elements[""]);
                    break;
                case (YAML::NodeType::Map):
                    read_yaml(doc[m_struct_name], elements, "");
                    break;
                default:
                    break;
                }
            }

            return elements;
        }

    private:
        void read_yaml(const YAML::Node& node, hash_array<std::string,std::string>& hash, std::string prefix)
        {
            std::string key, value;

            YAML::NodeType::value type = node.Type();

            for(YAML::Iterator it=node.begin();it!=node.end();++it) {
                key.erase(); value.erase();

                it.first() >> key;

                switch (it.second().Type())
                {
                case (YAML::NodeType::Scalar):
                    it.second() >> value;
                    hash[prefix.empty() ? key : prefix + "." + key] = value;
                    break;
                case (YAML::NodeType::Sequence):

                    if (node[key].size() > 0)
                    {
                        switch(node[key][0].Type())
                        {
                        case (YAML::NodeType::Scalar):
                            read_simple_sequence(node[key], hash, prefix.empty() ? key : prefix + "." + key); //int array case
                            break;
                        case (YAML::NodeType::Map):
                            read_complex_sequence(node[key], hash, prefix.empty() ? key : prefix + "." + key); //structs array case
                            break;
                        default:
                            return;
                        }
                    }

                    break;
                case (YAML::NodeType::Map):
                    read_yaml(node[key], hash, prefix.empty() ? key : prefix + "." + key);
                    break;
                default:
                    return;
                }
            }
        }

        void read_simple_sequence(const YAML::Node& node, hash_array<std::string,std::string>& hash, std::string prefix)
        {
            std::string seq_value, int_value;

            for (int i = 0; i < (int) node.size(); i++)
            {
                node[i] >> int_value;
                seq_value += seq_value.empty() ? int_value : ',' + int_value;
            }

            hash[prefix] = seq_value;
        }

        void read_complex_sequence(const YAML::Node& node, hash_array<std::string,std::string>& hash, std::string prefix)
        {
            std::string seq_key, seq_value;
            std::stringstream idx;

            for (int i = 0; i < (int) node.size(); i++)
            {
                idx << "[" << i << "]";
                for(YAML::Iterator it=node[i].begin();it!=node[i].end();++it) {
                    it.first() >> seq_key;

                    switch(it.second().Type())
                    {
                    case (YAML::NodeType::Scalar):
                        it.second() >> seq_value;
                        hash[prefix + idx.str() + '.' + seq_key] = seq_value;
                        break;
                    case (YAML::NodeType::Sequence):
                        read_simple_sequence(node[i][seq_key], hash, prefix + idx.str() + '.' + seq_key); //int array case
                        break;
                    default:
                        return;
                    }
                }
                idx.str("");
            }
        }
    };

    class KeyPrefix
    {
        KeyPrefix();
        KeyPrefix(const KeyPrefix & obj);
    public:
        static hash_array<std::string,std::string> parse_hash (const std::string & prefix, const hash_array<std::string,std::string> & elements)
        {
            size_t pos;
            hash_array<std::string,std::string> right_elements;
            hash_array<std::string, std::string>::const_iterator it;

            for(it = elements.begin(); it != elements.end(); it++)
            {
                pos = it->first.find(prefix);

                if (pos != std::string::npos)
                    right_elements[it->first.substr(prefix.size())] = it->second;
            }
            return right_elements;
        }
    };
}