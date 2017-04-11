//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_reflect.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstddef>

#include "mfxstructures.h"
#include "mfxvp8.h"
#include "mfxvp9.h"
#include "mfxplugin.h"
#include "mfxmvc.h"
#include "mfxcamera.h"
#include "mfxfei.h"
#include "mfxfeih265.h"
#include "mfxla.h"
#include "mfxsc.h"

#include "ts_typedef.h"

#include <memory>

namespace mfx_reflect
{

#if defined(MFX_HAS_CPP11)
#define MAKE_SHARED(T, ARGS) ::std::make_shared<T> ARGS
#else
#define MAKE_SHARED(T, ARGS) mfx_cpp11::shared_ptr<T>(new T ARGS)
#endif

    template<class T>
    struct mfx_ext_buffer_id {
        enum { id = 0 }; //TODO remove this
    };
    template<>struct mfx_ext_buffer_id<mfxExtCodingOption> {
        enum { id = MFX_EXTBUFF_CODING_OPTION };
    };
    template<>struct mfx_ext_buffer_id<mfxExtCodingOption2> {
        enum { id = MFX_EXTBUFF_CODING_OPTION2 };
    };
    template<>struct mfx_ext_buffer_id<mfxExtCodingOption3> {
        enum { id = MFX_EXTBUFF_CODING_OPTION3 };
    };
    template<>struct mfx_ext_buffer_id<mfxExtAvcTemporalLayers> {
        enum { id = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS };
    };
    template<>struct mfx_ext_buffer_id<mfxExtAVCRefListCtrl> {
        enum { id = MFX_EXTBUFF_AVC_REFLIST_CTRL };
    };
    template<>struct mfx_ext_buffer_id<mfxExtVideoSignalInfo> {
        enum { id = MFX_EXTBUFF_VIDEO_SIGNAL_INFO };
    };
    template<>struct mfx_ext_buffer_id<mfxExtEncoderCapability> {
        enum { id = MFX_EXTBUFF_ENCODER_CAPABILITY };
    };
    template<>struct mfx_ext_buffer_id<mfxExtAVCEncodedFrameInfo> {
        enum { id = MFX_EXTBUFF_ENCODED_FRAME_INFO };
    };
    //template<>struct mfx_ext_buffer_id<mfxExtPAVPOption> {
    //    enum { id = MFX_EXTBUFF_PAVP_OPTION };
    //};
    //template<>struct mfx_ext_buffer_id<mfxExtAVCEncoderWiDiUsage> {
    //    enum { id = MFX_EXTBUFF_ENCODER_WIDI_USAGE };
    //};
    template<>struct mfx_ext_buffer_id<mfxExtEncoderROI> {
        enum { id = MFX_EXTBUFF_ENCODER_ROI };
    };
    template<>struct mfx_ext_buffer_id<mfxExtDirtyRect> {
        enum { id = MFX_EXTBUFF_DIRTY_RECTANGLES };
    };
    template<>struct mfx_ext_buffer_id<mfxExtEncoderResetOption> {
        enum { id = MFX_EXTBUFF_ENCODER_RESET_OPTION };
    };
    //template<>struct mfx_ext_buffer_id<mfxExtCodingOptionDDI> {
    //    enum { id = MFX_EXTBUFF_DDI };
    //};

    void AccessorField::SetFieldAddress()
    {
        m_P = (*m_Iterator)->GetAddress(m_BaseStruct.m_P);
    }

    AccessorField& AccessorField::operator++()
    {
        m_Iterator++;
        if (IsValid())
        {
            SetIterator(m_Iterator);
        }
        return *this;
    }

    bool AccessorField::IsValid() const
    {
        return m_Iterator != m_BaseStruct.m_pReflection->m_Fields.end();
    }

    AccessorType AccessorField::AccessSubtype() const
    {
        return AccessorType(m_P, *(m_pReflection->FieldType));
    }


    AccessorField AccessorType::AccessField(ReflectedField::FieldsCollectionCI iter) const
    {
        if (m_pReflection->m_Fields.end() != iter)
        {
            return AccessorField(*this, iter); //Address of base, not field.
        }
        else
        {
            throw ::std::invalid_argument(::std::string("No such field"));
        }
    }

    AccessorField AccessorType::AccessField(const ::std::string fieldName) const
    {
        ReflectedField::FieldsCollectionCI iter = m_pReflection->FindField(fieldName);
        return AccessField(iter);
    }

    AccessorField AccessorType::AccessFirstField() const
    {
        return AccessField(m_pReflection->m_Fields.begin());
    }


    ReflectedType::ReflectedType(ReflectedTypesCollection *pCollection, TypeIndex typeIndex, const ::std::string typeName, size_t size, bool isPointer, mfxU32 extBufferId)
        : m_TypeIndex(typeIndex)
        , TypeNames(1, typeName)
        , Size(size)
        , m_pCollection(pCollection)
        , m_bIsPointer(isPointer)
        , ExtBufferId(extBufferId)
    {
        if (NULL == m_pCollection)
        {
            m_pCollection = NULL;
        }
    }

    ReflectedField::P ReflectedType::AddField(TypeIndex typeIndex, const ::std::string typeName, size_t typeSize, bool isPointer, size_t offset, const ::std::string fieldName, size_t count, mfxU32 extBufferId)
    {
        ReflectedField::P pField;
        if (NULL != m_pCollection)
        {
            ReflectedType::P pType = m_pCollection->FindOrDeclareType(typeIndex, typeName, typeSize, isPointer, extBufferId);
            if (pType != NULL)
            {
                ::std::string *fieldTypeName = NULL;
                if (!typeName.empty())
                {
                    StringList::iterator findIterator = ::std::find<StringList::iterator, ::std::string>(pType->TypeNames.begin(), pType->TypeNames.end(), typeName);
                    if (pType->TypeNames.end() != findIterator)
                    {
                        fieldTypeName = &(*findIterator);
                    }
                }
                if (NULL == fieldTypeName)
                {
                    throw ::std::invalid_argument(::std::string("Unexpected behavior - fieldTypeName is NULL"));
                }
                mfx_cpp11::shared_ptr<ReflectedType> aggregatingType = mfx_cpp11::shared_ptr<ReflectedType>(this);
                pField = ReflectedField::P(new ReflectedField(m_pCollection, aggregatingType, pType, *fieldTypeName, offset, fieldName, count)); //std::make_shared cannot access protected constructor
                m_Fields.push_back(pField);
            }
        }
        return pField;
    }

    ReflectedField::FieldsCollectionCI ReflectedType::FindField(const ::std::string fieldName) const
    {
        ReflectedField::FieldsCollectionCI iter = m_Fields.begin();
        for (; iter != m_Fields.end(); ++iter)
        {
            if ((*iter)->FieldName == fieldName)
            {
                break;
            }
        }
        return iter;
    }

    ReflectedType::P ReflectedTypesCollection::FindExistingByTypeInfoName(const char* name) //currenly we are not using this
    {
        ReflectedType::P pType;
        for (Container::iterator iter = m_KnownTypes.begin(); iter != m_KnownTypes.end(); ++iter)
        {
            if (0 == strcmp(iter->first.name(), name))
            {
                pType = iter->second;
                break;
            }
        }
        return pType;
    }

    ReflectedType::P ReflectedTypesCollection::FindExistingType(TypeIndex typeIndex)
    {
        ReflectedType::P pType;
        Container::const_iterator it = m_KnownTypes.find(typeIndex);
        if (m_KnownTypes.end() != it)
        {
            pType = it->second;
        }

        return pType;
    }

    ReflectedType::P ReflectedTypesCollection::FindExtBufferTypeById(mfxU32 ExtBufferId) //optimize with map (index -> type)
    {
        ReflectedType::P pExtBufferType;
        for (Container::iterator iter = m_KnownTypes.begin(); iter != m_KnownTypes.end(); ++iter)
        {
            ReflectedType::P pTempType = iter->second;
            if (ExtBufferId == pTempType->ExtBufferId)
            {
                pExtBufferType = pTempType;
                break;
            }
        }
        return pExtBufferType;
    }

    ReflectedType::P ReflectedTypesCollection::DeclareType(TypeIndex typeIndex, const ::std::string typeName, size_t typeSize, bool isPointer, mfxU32 extBufferId)
    {
        ReflectedType::P pType;
        if (m_KnownTypes.end() == m_KnownTypes.find(typeIndex))
        {
            pType = MAKE_SHARED(ReflectedType,(this, typeIndex, typeName, typeSize, isPointer, extBufferId));
            m_KnownTypes.insert(::std::make_pair(pType->m_TypeIndex, pType));
        }
        return pType;
    }

    ReflectedType::P ReflectedTypesCollection::FindOrDeclareType(TypeIndex typeIndex, const ::std::string typeName, size_t typeSize, bool isPointer, mfxU32 extBufferId)
    {
        ReflectedType::P pType = FindExistingType(typeIndex);
        if (pType == NULL)
        {
            pType = DeclareType(typeIndex, typeName, typeSize, isPointer, extBufferId);
        }
        else
        {
            if (typeSize != pType->Size)
            {
                pType.reset();
            }
            else if (!typeName.empty())
            {
                ReflectedType::StringList::iterator findIterator = ::std::find<ReflectedType::StringList::iterator, ::std::string>(pType->TypeNames.begin(), pType->TypeNames.end(), typeName);
                if (pType->TypeNames.end() == findIterator)
                {
                    pType->TypeNames.push_back(typeName);
                }
            }
        }
        return pType;
    }

    template <class T> bool PrintFieldIfTypeMatches(::std::ostream& stream, AccessorField field)
    {
        bool bResult = false;
        ReflectedType::P pType = field.m_pReflection->m_pCollection->FindExistingType<T>();
        if (field.m_pReflection->FieldType->m_TypeIndex == pType->m_TypeIndex)
        {
            stream << field.Get<T>();
            bResult = true;
        }
        return bResult;
    }

    void PrintFieldValue(::std::stringstream &stream, AccessorField field)
    {
        if (field.m_pReflection->FieldType->m_bIsPointer)
        {
            stream << "0x" << field.Get<void*>();
        }

        else
        {
            bool bPrinted = false;

#define PRINT_IF_TYPE(T) if (!bPrinted) { bPrinted = PrintFieldIfTypeMatches<T>(stream, field); }
            PRINT_IF_TYPE(mfxU16);
            PRINT_IF_TYPE(mfxI16);
            PRINT_IF_TYPE(mfxU32);
            PRINT_IF_TYPE(mfxI32);
            if (!bPrinted)
            {
                size_t fieldSize = field.m_pReflection->FieldType->Size;
                {
                    stream << "<Unknown type \"" << field.m_pReflection->FieldType->m_TypeIndex.name() << "\" (size = " << fieldSize << ")>";
                }
            }
        }
    }

    ::std::ostream& operator<< (::std::ostream& stream, AccessorField field)
    {
        ::std::stringstream ss;
        ss << field.m_pReflection->FieldName << " = ";
        PrintFieldValue(ss, field);

        stream << ss.str();
        return stream;
    }

    ::std::ostream& operator<< (::std::ostream& stream, AccessorType data)
    {
        stream << data.m_pReflection->m_TypeIndex.name() << " = {";
        size_t nFields = data.m_pReflection->m_Fields.size();
        for (AccessorField field = data.AccessFirstField(); field.IsValid(); ++field)
        {
            if (nFields > 1)
            {
                stream << ::std::endl;
            }
            stream << "\t" << field;
        }
        stream << "}";
        return stream;
    }

    TypeComparisonResultP CompareExtBufferLists(mfxExtBuffer** pExtParam1, mfxU16 numExtParam1, mfxExtBuffer** pExtParam2, mfxU16 numExtParam2, ReflectedTypesCollection* collection);
    AccessorTypeP GetAccessorOfExtBufferOriginalType(mfxExtBuffer& pExtInnerParam, ReflectedTypesCollection& collection);

    TypeComparisonResultP CompareTwoStructs(AccessorType data1, AccessorType data2) //always return not null result
    {
        TypeComparisonResultP result = MAKE_SHARED(TypeComparisonResult,());
        if (data1.m_pReflection != data2.m_pReflection)
        {
            throw ::std::invalid_argument(::std::string("Types mismatch"));
        }
        for (AccessorField field1 = data1.AccessFirstField(); field1.IsValid(); ++field1) //TODO: compare arrays
        {
            AccessorField field2 = data2.AccessField(field1.m_Iterator);
            ReflectedType::P p_mfxExtBufferType = field1.m_pReflection->m_pCollection->FindExistingType<mfxExtBuffer**>();
            if (field1.m_pReflection->FieldType->m_TypeIndex == p_mfxExtBufferType->m_TypeIndex)
            {
                mfxExtBuffer** pExtParam1 = field1.Get<mfxExtBuffer**>();
                mfxExtBuffer** pExtParam2 = field2.Get<mfxExtBuffer**>();
                ++field1; ++field2;
                if (!field1.IsValid() || !field2.IsValid()) break;
                if (!(field1.m_pReflection->FieldType->m_TypeIndex == TypeIndex(typeid(mfxU16)) && field1.m_pReflection->FieldType->m_TypeIndex == TypeIndex(typeid(mfxU16)))) break;
                mfxU16 numExtParam1 = field1.Get<mfxU16>();
                mfxU16 numExtParam2 = field2.Get<mfxU16>();

                TypeComparisonResultP extBufferCompareResult = CompareExtBufferLists(pExtParam1, numExtParam1, pExtParam2, numExtParam2, field1.m_pReflection->m_pCollection);
                if (extBufferCompareResult != NULL)
                {
                    result->splice(result->end(), *extBufferCompareResult); //move elements of *extBufferCompareResult to the end of *result
                    if (!extBufferCompareResult->extBufferIdList.empty())
                    {
                        result->extBufferIdList.splice(result->extBufferIdList.end(), extBufferCompareResult->extBufferIdList);
                    }
                }
                else
                {
                    throw ::std::invalid_argument(::std::string("Unexpected behavior - ExtBuffer comparison result is NULL"));
                }
            }
            AccessorType subtype1 = field1.AccessSubtype();

            TypeComparisonResultP subtypeResult = NULL;

            if (subtype1.m_pReflection->m_Fields.size() > 0)
            {
                AccessorType subtype2 = field2.AccessSubtype();
                if (subtype1.m_pReflection != subtype2.m_pReflection)
                {
                    throw ::std::invalid_argument(::std::string("Subtypes mismatch - should never happen for same types"));
                }
                subtypeResult = CompareTwoStructs(subtype1, subtype2);
            }
            if (!field1.Compare(field2))
            {
                FieldComparisonResult fields = { field1 , field2, subtypeResult };
                result->push_back(fields);
            }
        }
        return result;
    }

    AccessorTypeP GetAccessorOfExtBufferOriginalType(mfxExtBuffer& pExtBufferParam, ReflectedTypesCollection& collection)
    {
        AccessorTypeP pExtBuffer;
        mfxU32 id = pExtBufferParam.BufferId;
        ReflectedType::P pTypeExtBuffer = collection.FindExtBufferTypeById(id); //find in KnownTypes this BufferId
        if (pTypeExtBuffer != NULL)
        {
            pExtBuffer = MAKE_SHARED(AccessorType,(&pExtBufferParam, *pTypeExtBuffer));
        }
        return pExtBuffer;
    }

    TypeComparisonResultP CompareExtBufferLists(mfxExtBuffer** pExtParam1, mfxU16 numExtParam1, mfxExtBuffer** pExtParam2, mfxU16 numExtParam2, ReflectedTypesCollection* collection) //always return not null result
    {
        TypeComparisonResultP result = MAKE_SHARED(TypeComparisonResult,());
        if (NULL != pExtParam1 && NULL != pExtParam2 && NULL != collection)
        {
            for (int i = 0; i < numExtParam1 && i < numExtParam2; i++)
            {
                //supported only extBuffers with same Id order
                if (NULL != pExtParam1[i] && NULL != pExtParam2[i])
                {
                    AccessorTypeP extBufferOriginTypeAccessor1 = GetAccessorOfExtBufferOriginalType(*pExtParam1[i], *collection);
                    AccessorTypeP extBufferOriginTypeAccessor2 = GetAccessorOfExtBufferOriginalType(*pExtParam2[i], *collection);
                    if (NULL != extBufferOriginTypeAccessor1 && NULL != extBufferOriginTypeAccessor2)
                    {
                        TypeComparisonResultP tempResult = CompareTwoStructs(*extBufferOriginTypeAccessor1, *extBufferOriginTypeAccessor2);
                        if (NULL != tempResult)
                        {
                            result->splice(result->end(), *tempResult);
                        }
                        else
                        {
                            throw ::std::invalid_argument(::std::string("Unexpected behavior - comparison result is NULL"));
                        }
                    }
                    else
                    {
                        result->extBufferIdList.push_back(pExtParam1[i]->BufferId);
                    }
                }
            }
        }
        return result;
    }


    void PrintStuctsComparisonResult(::std::stringstream& comparisonResult, ::std::string prefix, TypeComparisonResultP result)
    {
        for (::std::list<FieldComparisonResult>::iterator i = result->begin(); i != result->end(); ++i)
        {
            TypeComparisonResultP subtypeResult = i->subtypeComparisonResultP;
            mfx_cpp11::shared_ptr<ReflectedType> aggregatingType = i->accessorField1.m_pReflection->AggregatingType;
            ::std::list< ::std::string >::const_iterator it = aggregatingType->TypeNames.begin();
            ::std::string strTypeName = ((it != aggregatingType->TypeNames.end()) ? *(it) : "unknown_type");

            if (NULL != subtypeResult)
            {
                ::std::string strFieldName = i->accessorField1.m_pReflection->FieldName;
                ::std::string newprefix = prefix.empty() ? (strTypeName + "." + strFieldName) : (prefix + "." + strFieldName);
                PrintStuctsComparisonResult(comparisonResult, newprefix, subtypeResult);
            }
            else
            {
                if (!prefix.empty()) { comparisonResult << prefix << "."; }
                else if (!strTypeName.empty()) { comparisonResult << strTypeName << "."; }
                comparisonResult << i->accessorField1 << " -> ";
                PrintFieldValue(comparisonResult, i->accessorField2);
                comparisonResult << ::std::endl;
            }
        }
        if (!result->extBufferIdList.empty())
        {
            comparisonResult << "Id of unparsed ExtBuffer types: " << ::std::endl;
            for (size_t i = 0; i < result->extBufferIdList.size(); i++)
            {
                comparisonResult << "0x" << ::std::hex << ::std::setfill('0') << result->extBufferIdList.front();
                result->extBufferIdList.pop_front();
                if (!result->extBufferIdList.empty()) { comparisonResult << ", "; }
            }
        }
    }


    std::string CompareStructsToString(AccessorType data1, AccessorType data2)
    {
        ::std::stringstream comparisonResult;
        if (data1.m_P == data2.m_P)
        {
            comparisonResult << "Comparing of VideoParams is unsupported: In and Out pointers are the same.";
        }
        else
        {
            comparisonResult << "Incompatible VideoParams were updated:" << ::std::endl;
            TypeComparisonResultP result = CompareTwoStructs(data1, data2);
            PrintStuctsComparisonResult(comparisonResult, "", result);
        }
        return comparisonResult.str();
    }

    template <class T>
    ReflectedField::P AddFieldT(ReflectedType &type, const std::string typeName, size_t offset, const std::string fieldName, size_t count)
    {
        unsigned int extBufId = 0;
        extBufId = mfx_ext_buffer_id<T>::id;
        bool isPointer = false;
        isPointer = mfx_cpp11::is_pointer<T>();
        return type.AddField(TypeIndex(typeid(T)), typeName, sizeof(T), isPointer, offset, fieldName, count, extBufId);
    }

    template <class T>
    ReflectedType::P DeclareTypeT(ReflectedTypesCollection& collection, const std::string typeName)
    {
        unsigned int extBufId = 0;
        extBufId = mfx_ext_buffer_id<T>::id;
        bool isPointer = false;
        isPointer = mfx_cpp11::is_pointer<T>();
        return collection.DeclareType(TypeIndex(typeid(T)), typeName, sizeof(T), isPointer, extBufId);
    }

    void ReflectedTypesCollection::DeclareMsdkStructs()
    {
#define STRUCT(TYPE, FIELDS) {                                  \
    typedef TYPE BaseType;                                      \
    ReflectedType::P pType = DeclareTypeT<TYPE>(*this, #TYPE);  \
    FIELDS                                                      \
    }

#define FIELD_T(FIELD_TYPE, FIELD_NAME)                 \
    (void)AddFieldT<FIELD_TYPE>(                        \
        *pType,                                         \
        #FIELD_TYPE,                                    \
        offsetof(BaseType, FIELD_NAME),                 \
        #FIELD_NAME,                                    \
        (sizeof(((BaseType*)0)->FIELD_NAME)/sizeof(::FIELD_TYPE)) );

#define FIELD_S(FIELD_TYPE, FIELD_NAME) FIELD_T(FIELD_TYPE, FIELD_NAME)

#include "ts_struct_decl.h"
    }
}