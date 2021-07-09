#include <stdio.h>
#include <sys/stat.h>

enum eMFXHWType
{
    MFX_HW_UNKNOWN   = 0,
    MFX_HW_SNB       = 0x300000,

    MFX_HW_IVB       = 0x400000,

    MFX_HW_HSW       = 0x500000,
    MFX_HW_HSW_ULT   = 0x500001,

    MFX_HW_VLV       = 0x600000,

    MFX_HW_BDW       = 0x700000,

    MFX_HW_CHT       = 0x800000,

    MFX_HW_SKL       = 0x900000,

    MFX_HW_APL       = 0x1000000,

    MFX_HW_KBL       = 0x1100000,
    MFX_HW_GLK       = MFX_HW_KBL + 1,
    MFX_HW_CFL       = MFX_HW_KBL + 2,

    MFX_HW_CNL       = 0x1200000,

    MFX_HW_ICL       = 0x1400000,
    MFX_HW_ICL_LP    = MFX_HW_ICL + 1,
#ifndef STRIP_EMBARGO
    MFX_HW_CNX_G     = MFX_HW_ICL + 2,

    MFX_HW_LKF       = 0x1500000,
#endif
    MFX_HW_JSL       = 0x1500001, // MFX_HW_LKF + 1
    MFX_HW_EHL       = 0x1500002,

    MFX_HW_TGL_LP    = 0x1600000,
    MFX_HW_RKL       = MFX_HW_TGL_LP + 2,
    MFX_HW_DG1       = 0x1600003,
    MFX_HW_ADL_S     = MFX_HW_TGL_LP + 4,
    MFX_HW_ADL_P     = MFX_HW_TGL_LP + 5,

#ifndef STRIP_EMBARGO
    MFX_HW_RYF       = MFX_HW_TGL_LP + 1,
    MFX_HW_XE_HP_SDV = MFX_HW_TGL_LP + 6,
    MFX_HW_DG2       = MFX_HW_XE_HP_SDV + 1,
    MFX_HW_PVC       = MFX_HW_XE_HP_SDV + 2,

    MFX_HW_MTL       = 0x1700000,
    MFX_HW_ARL       = MFX_HW_MTL + 1
#endif
};

enum GraphicsType {
    GT_INTEGRATED = 0,
    GT_DISCRETE,
};

typedef struct {
    int          device_id;
    eMFXHWType   platform;
    GraphicsType type;
} mfx_device_item;

// list of legal dev ID for Intel's graphics
const mfx_device_item listLegalDevIDs[] = {
    /*IVB*/
    { 0x0156, MFX_HW_IVB, GT_INTEGRATED },   /* GT1 mobile */
    { 0x0166, MFX_HW_IVB, GT_INTEGRATED },   /* GT2 mobile */
    { 0x0152, MFX_HW_IVB, GT_INTEGRATED },   /* GT1 desktop */
    { 0x0162, MFX_HW_IVB, GT_INTEGRATED },   /* GT2 desktop */
    { 0x015a, MFX_HW_IVB, GT_INTEGRATED },   /* GT1 server */
    { 0x016a, MFX_HW_IVB, GT_INTEGRATED },   /* GT2 server */
    /*HSW*/
    { 0x0402, MFX_HW_HSW, GT_INTEGRATED },   /* GT1 desktop */
    { 0x0412, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 desktop */
    { 0x0422, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 desktop */
    { 0x041e, MFX_HW_HSW, GT_INTEGRATED },   /* Core i3-4130 */
    { 0x040a, MFX_HW_HSW, GT_INTEGRATED },   /* GT1 server */
    { 0x041a, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 server */
    { 0x042a, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 server */
    { 0x0406, MFX_HW_HSW, GT_INTEGRATED },   /* GT1 mobile */
    { 0x0416, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 mobile */
    { 0x0426, MFX_HW_HSW, GT_INTEGRATED },   /* GT2 mobile */
    { 0x0C02, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT1 desktop */
    { 0x0C12, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 desktop */
    { 0x0C22, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 desktop */
    { 0x0C0A, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT1 server */
    { 0x0C1A, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 server */
    { 0x0C2A, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 server */
    { 0x0C06, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT1 mobile */
    { 0x0C16, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 mobile */
    { 0x0C26, MFX_HW_HSW, GT_INTEGRATED },   /* SDV GT2 mobile */
    { 0x0A02, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT1 desktop */
    { 0x0A12, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 desktop */
    { 0x0A22, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 desktop */
    { 0x0A0A, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT1 server */
    { 0x0A1A, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 server */
    { 0x0A2A, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 server */
    { 0x0A06, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT1 mobile */
    { 0x0A16, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 mobile */
    { 0x0A26, MFX_HW_HSW, GT_INTEGRATED },   /* ULT GT2 mobile */
    { 0x0D02, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT1 desktop */
    { 0x0D12, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 desktop */
    { 0x0D22, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 desktop */
    { 0x0D0A, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT1 server */
    { 0x0D1A, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 server */
    { 0x0D2A, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 server */
    { 0x0D06, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT1 mobile */
    { 0x0D16, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 mobile */
    { 0x0D26, MFX_HW_HSW, GT_INTEGRATED },   /* CRW GT2 mobile */
    /* this dev IDs added per HSD 5264859 request  */
    { 0x040B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_B_GT1 *//* Reserved */
    { 0x041B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_B_GT2*/
    { 0x042B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_B_GT3*/
    { 0x040E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_E_GT1*//* Reserved */
    { 0x041E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_E_GT2*/
    { 0x042E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_E_GT3*/

    { 0x0C0B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT1*/ /* Reserved */
    { 0x0C1B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT3*/
    { 0x0C0E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT1*//* Reserved */
    { 0x0C1E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_SDV_B_GT3*/

    { 0x0A0B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_B_GT1*/ /* Reserved */
    { 0x0A1B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_B_GT2*/
    { 0x0A2B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_B_GT3*/
    { 0x0A0E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_E_GT1*/ /* Reserved */
    { 0x0A1E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_E_GT2*/
    { 0x0A2E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_ULT_E_GT3*/

    { 0x0D0B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_B_GT1*/ /* Reserved */
    { 0x0D1B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_B_GT2*/
    { 0x0D2B, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_B_GT3*/
    { 0x0D0E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_E_GT1*/ /* Reserved */
    { 0x0D1E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_E_GT2*/
    { 0x0D2E, MFX_HW_HSW, GT_INTEGRATED }, /*HASWELL_CRW_E_GT3*/

    /* VLV */
    { 0x0f30, MFX_HW_VLV, GT_INTEGRATED },   /* VLV mobile */
    { 0x0f31, MFX_HW_VLV, GT_INTEGRATED },   /* VLV mobile */
    { 0x0f32, MFX_HW_VLV, GT_INTEGRATED },   /* VLV mobile */
    { 0x0f33, MFX_HW_VLV, GT_INTEGRATED },   /* VLV mobile */
    { 0x0157, MFX_HW_VLV, GT_INTEGRATED },
    { 0x0155, MFX_HW_VLV, GT_INTEGRATED },

    /* BDW */
    /*GT3: */
    { 0x162D, MFX_HW_BDW, GT_INTEGRATED },
    { 0x162A, MFX_HW_BDW, GT_INTEGRATED },
    /*GT2: */
    { 0x161D, MFX_HW_BDW, GT_INTEGRATED },
    { 0x161A, MFX_HW_BDW, GT_INTEGRATED },
    /* GT1: */
    { 0x160D, MFX_HW_BDW, GT_INTEGRATED },
    { 0x160A, MFX_HW_BDW, GT_INTEGRATED },
    /* BDW-ULT */
    /* (16x2 - ULT, 16x6 - ULT, 16xB - Iris, 16xE - ULX) */
    /*GT3: */
    { 0x162E, MFX_HW_BDW, GT_INTEGRATED },
    { 0x162B, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1626, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1622, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1636, MFX_HW_BDW, GT_INTEGRATED }, /* ULT */
    { 0x163B, MFX_HW_BDW, GT_INTEGRATED }, /* Iris */
    { 0x163E, MFX_HW_BDW, GT_INTEGRATED }, /* ULX */
    { 0x1632, MFX_HW_BDW, GT_INTEGRATED }, /* ULT */
    { 0x163A, MFX_HW_BDW, GT_INTEGRATED }, /* Server */
    { 0x163D, MFX_HW_BDW, GT_INTEGRATED }, /* Workstation */

    /* GT2: */
    { 0x161E, MFX_HW_BDW, GT_INTEGRATED },
    { 0x161B, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1616, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1612, MFX_HW_BDW, GT_INTEGRATED },
    /* GT1: */
    { 0x160E, MFX_HW_BDW, GT_INTEGRATED },
    { 0x160B, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1606, MFX_HW_BDW, GT_INTEGRATED },
    { 0x1602, MFX_HW_BDW, GT_INTEGRATED },

    /* CHT */
    { 0x22b0, MFX_HW_CHT, GT_INTEGRATED },
    { 0x22b1, MFX_HW_CHT, GT_INTEGRATED },
    { 0x22b2, MFX_HW_CHT, GT_INTEGRATED },
    { 0x22b3, MFX_HW_CHT, GT_INTEGRATED },

    /* SKL */
    /* GT1F */
    { 0x1902, MFX_HW_SKL, GT_INTEGRATED }, // DT, 2x1F, 510
    { 0x1906, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x1F, 510
    { 0x190A, MFX_HW_SKL, GT_INTEGRATED }, // Server, 4x1F
    { 0x190B, MFX_HW_SKL, GT_INTEGRATED },
    { 0x190E, MFX_HW_SKL, GT_INTEGRATED }, // Y-ULX 2x1F
    /*GT1.5*/
    { 0x1913, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x1.5
    { 0x1915, MFX_HW_SKL, GT_INTEGRATED }, // Y-ULX, 2x1.5
    { 0x1917, MFX_HW_SKL, GT_INTEGRATED }, // DT, 2x1.5
    /* GT2 */
    { 0x1912, MFX_HW_SKL, GT_INTEGRATED }, // DT, 2x2, 530
    { 0x1916, MFX_HW_SKL, GT_INTEGRATED }, // U-ULD 2x2, 520
    { 0x191A, MFX_HW_SKL, GT_INTEGRATED }, // 2x2,4x2, Server
    { 0x191B, MFX_HW_SKL, GT_INTEGRATED }, // DT, 2x2, 530
    { 0x191D, MFX_HW_SKL, GT_INTEGRATED }, // 4x2, WKS, P530
    { 0x191E, MFX_HW_SKL, GT_INTEGRATED }, // Y-ULX, 2x2, P510,515
    { 0x1921, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x2F, 540
    /* GT3 */
    { 0x1923, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x3, 535
    { 0x1926, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x3, 540 (15W)
    { 0x1927, MFX_HW_SKL, GT_INTEGRATED }, // U-ULT, 2x3e, 550 (28W)
    { 0x192A, MFX_HW_SKL, GT_INTEGRATED }, // Server, 2x3
    { 0x192B, MFX_HW_SKL, GT_INTEGRATED }, // Halo 3e
    { 0x192D, MFX_HW_SKL, GT_INTEGRATED },
    /* GT4e*/
    { 0x1932, MFX_HW_SKL, GT_INTEGRATED }, // DT
    { 0x193A, MFX_HW_SKL, GT_INTEGRATED }, // SRV
    { 0x193B, MFX_HW_SKL, GT_INTEGRATED }, // Halo
    { 0x193D, MFX_HW_SKL, GT_INTEGRATED }, // WKS

    /* APL */
    { 0x0A84, MFX_HW_APL, GT_INTEGRATED },
    { 0x0A85, MFX_HW_APL, GT_INTEGRATED },
    { 0x0A86, MFX_HW_APL, GT_INTEGRATED },
    { 0x0A87, MFX_HW_APL, GT_INTEGRATED },
    { 0x1A84, MFX_HW_APL, GT_INTEGRATED },
    { 0x1A85, MFX_HW_APL, GT_INTEGRATED },
    { 0x5A84, MFX_HW_APL, GT_INTEGRATED },
    { 0x5A85, MFX_HW_APL, GT_INTEGRATED },

    /* KBL */
    { 0x5902, MFX_HW_KBL, GT_INTEGRATED }, // DT GT1
    { 0x5906, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT1
    { 0x5908, MFX_HW_KBL, GT_INTEGRATED }, // HALO GT1F
    { 0x590A, MFX_HW_KBL, GT_INTEGRATED }, // SERV GT1
    { 0x590B, MFX_HW_KBL, GT_INTEGRATED }, // HALO GT1
    { 0x590E, MFX_HW_KBL, GT_INTEGRATED }, // ULX GT1
    { 0x5912, MFX_HW_KBL, GT_INTEGRATED }, // DT GT2
    { 0x5913, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT1 5
    { 0x5915, MFX_HW_KBL, GT_INTEGRATED }, // ULX GT1 5
    { 0x5916, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT2
    { 0x5917, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT2 R
    { 0x591A, MFX_HW_KBL, GT_INTEGRATED }, // SERV GT2
    { 0x591B, MFX_HW_KBL, GT_INTEGRATED }, // HALO GT2
    { 0x591C, MFX_HW_KBL, GT_INTEGRATED }, // ULX GT2
    { 0x591D, MFX_HW_KBL, GT_INTEGRATED }, // WRK GT2
    { 0x591E, MFX_HW_KBL, GT_INTEGRATED }, // ULX GT2
    { 0x5921, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT2F
    { 0x5923, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT3
    { 0x5926, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT3 15W
    { 0x5927, MFX_HW_KBL, GT_INTEGRATED }, // ULT GT3 28W
    { 0x592A, MFX_HW_KBL, GT_INTEGRATED }, // SERV GT3
    { 0x592B, MFX_HW_KBL, GT_INTEGRATED }, // HALO GT3
    { 0x5932, MFX_HW_KBL, GT_INTEGRATED }, // DT GT4
    { 0x593A, MFX_HW_KBL, GT_INTEGRATED }, // SERV GT4
    { 0x593B, MFX_HW_KBL, GT_INTEGRATED }, // HALO GT4
    { 0x593D, MFX_HW_KBL, GT_INTEGRATED }, // WRK GT4
    { 0x87C0, MFX_HW_KBL, GT_INTEGRATED }, // ULX GT2

    /* GLK */
    { 0x3184, MFX_HW_GLK, GT_INTEGRATED },
    { 0x3185, MFX_HW_GLK, GT_INTEGRATED },

    /* CFL */
    { 0x3E90, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E91, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E92, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E93, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E94, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E96, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E98, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E99, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E9A, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E9C, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3E9B, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA5, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA6, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA7, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA8, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA9, MFX_HW_CFL, GT_INTEGRATED },
    { 0x87CA, MFX_HW_CFL, GT_INTEGRATED },

    /* WHL */
    { 0x3EA0, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA1, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA2, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA3, MFX_HW_CFL, GT_INTEGRATED },
    { 0x3EA4, MFX_HW_CFL, GT_INTEGRATED },


    /* CML GT1 */
    { 0x9b21, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9baa, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bab, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bac, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9ba0, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9ba5, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9ba8, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9ba4, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9ba2, MFX_HW_CFL, GT_INTEGRATED },

    /* CML GT2 */
    { 0x9b41, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bca, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bcb, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bcc, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc0, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc5, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc8, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc4, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc2, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bc6, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9be6, MFX_HW_CFL, GT_INTEGRATED },
    { 0x9bf6, MFX_HW_CFL, GT_INTEGRATED },


    /* CNL */
    { 0x5A51, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A52, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A5A, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A40, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A42, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A4A, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A4C, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A50, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A54, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A59, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A5C, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A41, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A44, MFX_HW_CNL, GT_INTEGRATED },
    { 0x5A49, MFX_HW_CNL, GT_INTEGRATED },

    /* ICL LP */
    { 0xFF05, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A50, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A51, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A52, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A53, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A54, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A56, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A57, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A58, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A59, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A5A, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A5B, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A5C, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A5D, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A70, MFX_HW_ICL_LP, GT_INTEGRATED },
    { 0x8A71, MFX_HW_ICL_LP, GT_INTEGRATED },  // GT05, but 1 ok in this context

    /* JSL */
    { 0x4E51, MFX_HW_JSL, GT_INTEGRATED },
    { 0x4E55, MFX_HW_JSL, GT_INTEGRATED },
    { 0x4E61, MFX_HW_JSL, GT_INTEGRATED },
    { 0x4E71, MFX_HW_JSL, GT_INTEGRATED },

    /* EHL */
    { 0x4500, MFX_HW_EHL, GT_INTEGRATED },
    { 0x4541, MFX_HW_EHL, GT_INTEGRATED },
    { 0x4551, MFX_HW_EHL, GT_INTEGRATED },
    { 0x4555, MFX_HW_EHL, GT_INTEGRATED },
    { 0x4569, MFX_HW_EHL, GT_INTEGRATED },
    { 0x4571, MFX_HW_EHL, GT_INTEGRATED },

    /* TGL */
    { 0x9A40, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A49, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A59, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A60, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A68, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A70, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A78, MFX_HW_TGL_LP, GT_INTEGRATED },

    /* DG1/SG1 */
    { 0x4905, MFX_HW_DG1, GT_DISCRETE },
    { 0x4906, MFX_HW_DG1, GT_DISCRETE },
    { 0x4907, MFX_HW_DG1, GT_DISCRETE },
    { 0x4908, MFX_HW_DG1, GT_DISCRETE },

    /* RKL */
    { 0x4C80, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    { 0x4C8A, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    { 0x4C81, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    { 0x4C8B, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    { 0x4C90, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    { 0x4C9A, MFX_HW_RKL, GT_INTEGRATED }, // RKL-S
    
    /* ADL */
    { 0x4600, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4680, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4681, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4683, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4690, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4691, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4693, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4698, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S
    { 0x4699, MFX_HW_ADL_S, GT_INTEGRATED },//ADL-S

    /* ADL-P */
    { 0x46A0, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46A1, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46A3, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46A6, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x4626, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P

    { 0x46B0, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46B1, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46B3, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46A8, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x4628, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P

    { 0x46C0, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46C1, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46C3, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x46AA, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P
    { 0x462A, MFX_HW_ADL_P, GT_INTEGRATED },//ADL-P


#ifndef STRIP_EMBARGO
    { 0xFF20, MFX_HW_TGL_LP, GT_INTEGRATED },//iTGLSIM
    { 0x9A09, MFX_HW_TGL_LP, GT_INTEGRATED },
    { 0x9A39, MFX_HW_TGL_LP, GT_INTEGRATED },//iTGLLPDEVMOB4
    { 0x9A69, MFX_HW_TGL_LP, GT_INTEGRATED },//iTGLLPDEVMOB7
    { 0x9A79, MFX_HW_TGL_LP, GT_INTEGRATED }, //iTGLLPDEVMOB8

    /* RYF */
    { 0x9940, MFX_HW_RYF, GT_INTEGRATED },
    /* XE HP */
    { 0x0201, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0202, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0203, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0204, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0205, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0206, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0207, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0208, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0209, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020A, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020B, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020C, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020D, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020E, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x020F, MFX_HW_XE_HP_SDV, GT_DISCRETE },
    { 0x0210, MFX_HW_XE_HP_SDV, GT_DISCRETE },


    /* DG2 */
    { 0x4F80, MFX_HW_DG2, GT_DISCRETE }, // DG2-512 / IDG2_8x4x16_SKU_DEVICE_F0_ID
    { 0x4F81, MFX_HW_DG2, GT_DISCRETE }, // DG2-448 / IDG2_7x4x16_SKU_DEVICE_F0_ID
    { 0x4F82, MFX_HW_DG2, GT_DISCRETE }, // DG2-384 / IDG2_6x4x16_SKU_DEVICE_F0_ID
    { 0x4F83, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x4F84, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x4F85, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x4F86, MFX_HW_DG2, GT_DISCRETE }, // DG2-192 / IDG2_3x4x16_SKU_DEVICE_F0_ID
    { 0x4F87, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x4F88, MFX_HW_DG2, GT_DISCRETE }, // DG2-96  / IDG2_2x3x16_SKU_DEVICE_F0_ID
    { 0x5690, MFX_HW_DG2, GT_DISCRETE }, // DG2-512 / IDG2_8x4x16_SKU_DEVICE_F0_ID
    { 0x5691, MFX_HW_DG2, GT_DISCRETE }, // DG2-384 / IDG2_6x4x16_SKU_DEVICE_F0_ID
    { 0x5692, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x5693, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x5694, MFX_HW_DG2, GT_DISCRETE }, // DG2-96  / IDG2_2x3x16_SKU_DEVICE_F0_ID
    { 0x5695, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x5696, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x5697, MFX_HW_DG2, GT_DISCRETE }, // DG2-192 / IDG2_3x4x16_SKU_DEVICE_F0_ID
    { 0x5698, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x56A0, MFX_HW_DG2, GT_DISCRETE }, // DG2-512 / IDG2_8x4x16_SKU_DEVICE_F0_ID
    { 0x56A1, MFX_HW_DG2, GT_DISCRETE }, // DG2-448 / IDG2_7x4x16_SKU_DEVICE_F0_ID
    { 0x56A2, MFX_HW_DG2, GT_DISCRETE }, // DG2-384 / IDG2_6x4x16_SKU_DEVICE_F0_ID
    { 0x56A3, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x56A4, MFX_HW_DG2, GT_DISCRETE }, // DG2-96  / IDG2_2x3x16_SKU_DEVICE_F0_ID
    { 0x56A5, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x56A6, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x56A7, MFX_HW_DG2, GT_DISCRETE }, // DG2-256 / IDG2_4x4x16_SKU_DEVICE_F0_ID
    { 0x56A8, MFX_HW_DG2, GT_DISCRETE }, // DG2-192 / IDG2_3x4x16_SKU_DEVICE_F0_ID
    { 0x56A9, MFX_HW_DG2, GT_DISCRETE }, // DG2-128 / IDG2_2x4x16_SKU_DEVICE_F0_ID
    { 0x56B0, MFX_HW_DG2, GT_DISCRETE }, // DG2-96  / IDG2_2x3x16_SKU_DEVICE_F0_ID
    { 0x56B1, MFX_HW_DG2, GT_DISCRETE }, // DG2-96  / IDG2_2x3x16_SKU_DEVICE_F0_ID
    { 0x56C0, MFX_HW_DG2, GT_DISCRETE }, // DG2-512 / IDG2_8x4x16_SKU_DEVICE_F0_ID

    /* PVC */
    { 0x0BD0, MFX_HW_PVC, GT_DISCRETE },
    { 0x0BD5, MFX_HW_PVC, GT_DISCRETE }
#endif

};

static inline const char *get_name(eMFXHWType platform) {
    switch (platform) {
    case MFX_HW_SNB: return "SNB";
    case MFX_HW_IVB: return "IVB";
    case MFX_HW_HSW: return "HSW";
    case MFX_HW_HSW_ULT: return "HSW_ULT";
    case MFX_HW_VLV: return "VLV";
    case MFX_HW_BDW: return "BDW";
    case MFX_HW_CHT: return "CHT";
    case MFX_HW_SKL: return "SKL";
    case MFX_HW_APL: return "APL";
    case MFX_HW_KBL: return "KBL";
    case MFX_HW_GLK: return "GLK";
    case MFX_HW_CFL: return "CFL";
    case MFX_HW_CNL: return "CNL";
    case MFX_HW_ICL: return "ICL";
    case MFX_HW_ICL_LP: return "ICL_LP";
#ifndef STRIP_EMBARGO
    case MFX_HW_CNX_G: return "CNX_G";
    case MFX_HW_LKF: return "LKF";
#endif
    case MFX_HW_JSL: return "JSL";
    case MFX_HW_EHL: return "EHL";
    case MFX_HW_TGL_LP: return "TGL_LP";
    case MFX_HW_RKL: return "RKL";
    case MFX_HW_DG1: return "DG1";
    case MFX_HW_ADL_S: return "ADL_S";
    case MFX_HW_ADL_P: return "ADL_P";
#ifndef STRIP_EMBARGO
    case MFX_HW_RYF: return "RYF";
    // case MFX_HW_XE_HP_SDV: return "TGL_HP"; <-- MFX_HW_XE_HP_SDV == MFX_HW_XE_HP_SDV
    case MFX_HW_XE_HP_SDV: return "XE_HP";
    case MFX_HW_DG2: return "DG2";
    case MFX_HW_PVC: return "PVC";
    case MFX_HW_MTL: return "MTL";
    case MFX_HW_ARL: return "ARL";
#endif
    case MFX_HW_UNKNOWN: default: return "UNKNOWN";
    }
}

typedef struct {
    int vendor_id;
    int device_id;
    eMFXHWType platform;
    GraphicsType type;
} Device;

static inline eMFXHWType get_platform(int device_id) {
    for (unsigned i = 0; i < sizeof(listLegalDevIDs) / sizeof(listLegalDevIDs[0]); ++i) {
        if (listLegalDevIDs[i].device_id == device_id) {
            return listLegalDevIDs[i].platform;
        }
    }
    return MFX_HW_UNKNOWN;
}

static inline GraphicsType get_graphics_type(int device_id) {
    for (unsigned i = 0; i < sizeof(listLegalDevIDs) / sizeof(listLegalDevIDs[0]); ++i) {
        if (listLegalDevIDs[i].device_id == device_id) {
            return listLegalDevIDs[i].type;
        }
    }
    return GT_INTEGRATED;
}

std::vector <Device> get_devices() {
    const char *dir = "/sys/class/drm";
    const char *device_id_file = "/device/device";
    const char *vendor_id_file = "/device/vendor";
    int i = 0;
    std::vector <Device> result;
    for (; i < 64; ++i) {
        Device device;
        std::string path = std::string(dir) + "/renderD" + std::to_string(128 + i) + vendor_id_file;
        FILE *file = fopen(path.c_str(), "r");
        if (!file) break;
        fscanf(file, "%x", &device.vendor_id);
        fclose(file);
        if (device.vendor_id != 0x8086) {  // Filter out non-Intel devices
            continue;
        }
        path = std::string(dir) + "/renderD" + std::to_string(128 + i) + device_id_file;
        file = fopen(path.c_str(), "r");
        if (!file) break;
        fscanf(file, "%x", &device.device_id);
        fclose(file);
        device.platform = get_platform(device.device_id);
        device.type = get_graphics_type(device.device_id);
        result.emplace_back(device);
    }
    std::sort(result.begin(), result.end(), [](const Device &a, const Device &b) {
        if (a.type == b.type)
            return a.device_id < b.device_id;
        return a.type < b.type;
    });
    return result;
}
