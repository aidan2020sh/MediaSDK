#pragma once

#include "test_trace.h"
#include "test_sample_allocator.h"
#include "gtest\gtest.h"

typedef enum {
      tsOK = 0
    , tsFAIL
} tsRes;

class MFXVideoTest : public ::testing::TestWithParam<unsigned int> 
{ 
    void SetUp();
    void TearDown();
};

class tsStatus
{
public:
    mfxStatus m_status;
    mfxStatus m_expected;
    bool      m_failed;
    bool      m_throw_exceptions;

    tsStatus();
    ~tsStatus();

    bool check(mfxStatus status);
    bool check();
    inline void      expect (mfxStatus expected) { m_expected = expected; }
    inline mfxStatus get    ()                   { return m_status; }
};

extern mfxIMPL      g_tsImpl;
extern mfxVersion   g_tsVersion;
extern tsStatus     g_tsStatus;
extern mfxU32       g_tsTrace;
extern std::ostream g_tsLog;

#define TS_REG_TEST_SUITE(name, routine, n_cases) \
  TEST_P(MFXVideoTest, name) {\
      EXPECT_EQ(0, (routine)(GetParam()));\
  }\
  INSTANTIATE_TEST_CASE_P(ts, MFXVideoTest, ::testing::Range<unsigned int>(0, (n_cases)));

#define TRACE_FUNCN(n, name, p1, p2, p3, p4, p5, p6, p7)\
    g_tsLog << "-------------------------------------------------------------------------------\n";\
    INC_PADDING();\
    if(g_tsTrace){\
        if((n) > 0) g_tsLog << print_param.padding << #p1 " = " << p1 << '\n';\
        if((n) > 1) g_tsLog << print_param.padding << #p2 " = " << p2 << '\n';\
        if((n) > 2) g_tsLog << print_param.padding << #p3 " = " << p3 << '\n';\
        if((n) > 3) g_tsLog << print_param.padding << #p4 " = " << p4 << '\n';\
        if((n) > 4) g_tsLog << print_param.padding << #p5 " = " << p5 << '\n';\
        if((n) > 5) g_tsLog << print_param.padding << #p6 " = " << p6 << '\n';\
        if((n) > 8) g_tsLog << print_param.padding << #p7 " = " << p6 << '\n';\
    }\
    g_tsLog << "CALL: " << #name << '(';\
    if((n) > 0) g_tsLog << #p1;\
    if((n) > 1) g_tsLog << ", " << #p2;\
    if((n) > 2) g_tsLog << ", " << #p3;\
    if((n) > 3) g_tsLog << ", " << #p4;\
    if((n) > 4) g_tsLog << ", " << #p5;\
    if((n) > 5) g_tsLog << ", " << #p6;\
    if((n) > 6) g_tsLog << ", " << #p7;\
    g_tsLog << ");\n";\
    DEC_PADDING();
#define TRACE_FUNC6(name, p1, p2, p3, p4, p5, p6) TRACE_FUNCN(6, name, p1, p2, p3, p4, p5, p6, 0)
#define TRACE_FUNC5(name, p1, p2, p3, p4, p5) TRACE_FUNCN(5, name, p1, p2, p3, p4, p5, 0, 0)
#define TRACE_FUNC4(name, p1, p2, p3, p4) TRACE_FUNCN(4, name, p1, p2, p3, p4, 0, 0, 0)
#define TRACE_FUNC3(name, p1, p2, p3) TRACE_FUNCN(3, name, p1, p2, p3, 0, 0, 0, 0)
#define TRACE_FUNC2(name, p1, p2) TRACE_FUNCN(2, name, p1, p2, 0, 0, 0, 0, 0)
#define TRACE_FUNC1(name, p1) TRACE_FUNCN(1, name, p1, 0, 0, 0, 0, 0, 0)
#define TRACE_FUNC0(name) TRACE_FUNCN(0, name, 0, 0, 0, 0, 0, 0, 0)
#define TS_TRACE(par)   if(g_tsTrace) {INC_PADDING(); g_tsLog << print_param.padding << #par " = " << par << '\n'; DEC_PADDING();}
#define TS_CHECK        { if(g_tsStatus.m_failed) return 1; }
#define TS_CHECK_MFX    { if(g_tsStatus.m_failed) return g_tsStatus.get(); }
#define TS_MAX(x,y) ((x)>(y) ? (x) : (y))
#define TS_MIN(x,y) ((x)<(y) ? (x) : (y))
#define TS_START try {
#define TS_END   } catch(tsRes r) { return r; }