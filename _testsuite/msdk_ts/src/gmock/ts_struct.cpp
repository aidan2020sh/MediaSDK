#include "ts_struct.h"

namespace tsStruct
{

Field::Field(mfxU32 _offset, mfxU32 _size, std::string _name) 
    : name  (_name  )
    , offset(_offset)
    , size  (_size  )
{}

void check_eq(void* base, Field field, mfxU64 expected)
{
    mfxU64 real = 0;
    memcpy(&real, (mfxU8*)base + field.offset, field.size);

    if(expected != real)
    {
        g_tsLog << "ERROR: FAILED: expected " << field.name << "(" << real << ") == " << expected << "\n";
        ADD_FAILURE();
    }
}

void check_ne(void* base, Field field, mfxU64 expected)
{
    mfxU64 real = 0;
    memcpy(&real, (mfxU8*)base + field.offset, field.size);

    if(expected == real)
    {
        g_tsLog << "ERROR: FAILED: expected " << field.name << "(" << real << ") != " << expected << "\n";
        ADD_FAILURE();
    }
}

#define STRUCT(name, fields) Wrap_##name::Wrap_##name(mfxU32 base, mfxU32 size, std::string _name) : Field(base, size, _name) fields {}
#define FIELD_T(type, _name) , _name(offset + offsetof(base_type, _name), sizeof(::type), name + "::" + #_name)
#define FIELD_S FIELD_T

#include "ts_struct_decl.h"

#undef STRUCT
#undef FIELD_T
#undef FIELD_S

};