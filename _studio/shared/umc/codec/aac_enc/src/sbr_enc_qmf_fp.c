//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_ENCODER)

#include <math.h>
#include "align.h"
#include "sbr_enc_api_fp.h"
#include "aac_status.h"

/********************************************************************/

#ifndef NULL
#define NULL       0
#endif

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

/********************************************************************/

static const Ipp32f
SBR_TAB_QMF_WINDOW_640_FP[] = {
     0.0000000000f, -0.0005525287f, -0.0005617692f, -0.0004947518f,
    -0.0004875228f, -0.0004893791f, -0.0005040714f, -0.0005226564f,
    -0.0005466565f, -0.0005677803f, -0.0005870931f, -0.0006132748f,
    -0.0006312493f, -0.0006540333f, -0.0006777691f, -0.0006941615f,
    -0.0007157737f, -0.0007255043f, -0.0007440942f, -0.0007490598f,
    -0.0007681372f, -0.0007724849f, -0.0007834332f, -0.0007779869f,
    -0.0007803665f, -0.0007801449f, -0.0007757978f, -0.0007630794f,
    -0.0007530001f, -0.0007319357f, -0.0007215392f, -0.0006917937f,
    -0.0006650415f, -0.0006341595f, -0.0005946119f, -0.0005564576f,
    -0.0005145572f, -0.0004606326f, -0.0004095121f, -0.0003501176f,
    -0.0002896981f, -0.0002098337f, -0.0001446381f, -0.0000617334f,
    0.0000134950f,  0.0001094383f,  0.0002043017f,  0.0002949531f,
    0.0004026540f,  0.0005107389f,  0.0006239376f,  0.0007458026f,
    0.0008608443f,  0.0009885988f,  0.0011250156f,  0.0012577885f,
    0.0013902495f,  0.0015443220f,  0.0016868083f,  0.0018348265f,
    0.0019841141f,  0.0021461584f,  0.0023017256f,  0.0024625617f,
    0.0026201759f,  0.0027870464f,  0.0029469447f,  0.0031125420f,
    0.0032739614f,  0.0034418874f,  0.0036008267f,  0.0037603923f,
    0.0039207432f,  0.0040819752f,  0.0042264271f,  0.0043730722f,
    0.0045209853f,  0.0046606460f,  0.0047932561f,  0.0049137603f,
    0.0050393022f,  0.0051407353f,  0.0052461168f,  0.0053471681f,
    0.0054196776f,  0.0054876041f,  0.0055475715f,  0.0055938023f,
    0.0056220642f,  0.0056455196f,  0.0056389198f,  0.0056266114f,
    0.0055917129f,  0.0055404366f,  0.0054753781f,  0.0053838976f,
    0.0052715759f,  0.0051382277f,  0.0049839686f,  0.0048109470f,
    0.0046039531f,  0.0043801861f,  0.0041251644f,  0.0038456409f,
    0.0035401247f,  0.0032091886f,  0.0028446757f,  0.0024508541f,
    0.0020274175f,  0.0015784682f,  0.0010902329f,  0.0005832264f,
    0.0000276045f, -0.0005464281f, -0.0011568136f, -0.0018039473f,
    -0.0024826725f, -0.0031933777f, -0.0039401124f, -0.0047222595f,
    -0.0055337213f, -0.0063792295f, -0.0072615817f, -0.0081798229f,
    -0.0091325333f, -0.0101150218f, -0.0111315548f, -0.0121849999f,
    0.0132718217f,  0.0143904667f,  0.0155405551f,  0.0167324711f,
    0.0179433376f,  0.0191872437f,  0.0204531793f,  0.0217467546f,
    0.0230680164f,  0.0244160984f,  0.0257875845f,  0.0271859430f,
    0.0286072176f,  0.0300502665f,  0.0315017626f,  0.0329754092f,
    0.0344620943f,  0.0359697565f,  0.0374812856f,  0.0390053689f,
    0.0405349173f,  0.0420649089f,  0.0436097533f,  0.0451488420f,
    0.0466843024f,  0.0482165702f,  0.0497385748f,  0.0512556173f,
    0.0527630746f,  0.0542452782f,  0.0557173640f,  0.0571616441f,
    0.0585915670f,  0.0599837489f,  0.0613455176f,  0.0626857802f,
    0.0639715865f,  0.0652247071f,  0.0664367527f,  0.0676075965f,
    0.0687043816f,  0.0697630271f,  0.0707628727f,  0.0717002675f,
    0.0725682601f,  0.0733620226f,  0.0741003677f,  0.0747452527f,
    0.0753137320f,  0.0758008361f,  0.0761992484f,  0.0764992163f,
    0.0767093524f,  0.0768174008f,  0.0768230036f,  0.0767204911f,
    0.0765050724f,  0.0761748329f,  0.0757305771f,  0.0751576275f,
    0.0744664371f,  0.0736405998f,  0.0726774633f,  0.0715826377f,
    0.0703533068f,  0.0689664036f,  0.0674525052f,  0.0657690689f,
    0.0639444813f,  0.0619602762f,  0.0598166585f,  0.0575152710f,
    0.0550460033f,  0.0524093807f,  0.0495978668f,  0.0466303304f,
    0.0434768796f,  0.0401458293f,  0.0366418101f,  0.0329583921f,
    0.0290824007f,  0.0250307564f,  0.0207997076f,  0.0163701251f,
    0.0117623834f,  0.0069636861f,  0.0019765601f, -0.0032086896f,
    -0.0085711749f, -0.0141288824f, -0.0198834129f, -0.0258227289f,
    -0.0319531262f, -0.0382776558f, -0.0447806828f, -0.0514804162f,
    -0.0583705343f, -0.0654409826f, -0.0726943314f, -0.0801372901f,
    -0.0877547562f, -0.0955533385f, -0.1035329551f, -0.1116826907f,
    -0.1200077981f, -0.1285002828f, -0.1371551752f, -0.1459766477f,
    -0.1549607068f, -0.1640958786f, -0.1733808219f, -0.1828172505f,
    -0.1923966706f, -0.2021250129f, -0.2119735926f, -0.2219652683f,
    -0.2320690900f, -0.2423016876f, -0.2526480258f, -0.2631053329f,
    -0.2736634016f, -0.2843214273f, -0.2950716615f, -0.3059098721f,
    -0.3168278933f, -0.3278113604f, -0.3388722837f, -0.3499914110f,
    0.3611589968f,  0.3723795414f,  0.3836350143f,  0.3949211836f,
    0.4062317610f,  0.4175696969f,  0.4289119840f,  0.4402553737f,
    0.4515996575f,  0.4629307985f,  0.4742453098f,  0.4855253100f,
    0.4967708290f,  0.5079817772f,  0.5191234946f,  0.5302240849f,
    0.5412553549f,  0.5522051454f,  0.5630789399f,  0.5738524199f,
    0.5845403075f,  0.5951123238f,  0.6055783629f,  0.6159110069f,
    0.6261242628f,  0.6361979842f,  0.6461269855f,  0.6559016109f,
    0.6655139923f,  0.6749663353f,  0.6842353344f,  0.6933282614f,
    0.7022388577f,  0.7109410167f,  0.7194462419f,  0.7277448773f,
    0.7358211875f,  0.7436828017f,  0.7513137460f,  0.7587080598f,
    0.7658674717f,  0.7727780938f,  0.7794287801f,  0.7858353257f,
    0.7919735909f,  0.7978466153f,  0.8034485579f,  0.8087695241f,
    0.8138191104f,  0.8185775876f,  0.8230419755f,  0.8272275329f,
    0.8311038613f,  0.8346937299f,  0.8379717469f,  0.8409541249f,
    0.8436238170f,  0.8459818363f,  0.8480315804f,  0.8497804999f,
    0.8511971235f,  0.8523046970f,  0.8531020880f,  0.8535720706f,
    0.8537385464f,  0.8535720706f,  0.8531020880f,  0.8523046970f,
    0.8511971235f,  0.8497804999f,  0.8480315804f,  0.8459818363f,
    0.8436238170f,  0.8409541249f,  0.8379717469f,  0.8346937299f,
    0.8311038613f,  0.8272275329f,  0.8230419755f,  0.8185775876f,
    0.8138191104f,  0.8087695241f,  0.8034485579f,  0.7978466153f,
    0.7919735909f,  0.7858353257f,  0.7794287801f,  0.7727780938f,
    0.7658674717f,  0.7587080598f,  0.7513137460f,  0.7436828017f,
    0.7358211875f,  0.7277448773f,  0.7194462419f,  0.7109410167f,
    0.7022388577f,  0.6933282614f,  0.6842353344f,  0.6749663353f,
    0.6655139923f,  0.6559016109f,  0.6461269855f,  0.6361979842f,
    0.6261242628f,  0.6159110069f,  0.6055783629f,  0.5951123238f,
    0.5845403075f,  0.5738524199f,  0.5630789399f,  0.5522051454f,
    0.5412553549f,  0.5302240849f,  0.5191234946f,  0.5079817772f,
    0.4967708290f,  0.4855253100f,  0.4742453098f,  0.4629307985f,
    0.4515996575f,  0.4402553737f,  0.4289119840f,  0.4175696969f,
    0.4062317610f,  0.3949211836f,  0.3836350143f,  0.3723795414f,
    -0.3611589968f, -0.3499914110f, -0.3388722837f, -0.3278113604f,
    -0.3168278933f, -0.3059098721f, -0.2950716615f, -0.2843214273f,
    -0.2736634016f, -0.2631053329f, -0.2526480258f, -0.2423016876f,
    -0.2320690900f, -0.2219652683f, -0.2119735926f, -0.2021250129f,
    -0.1923966706f, -0.1828172505f, -0.1733808219f, -0.1640958786f,
    -0.1549607068f, -0.1459766477f, -0.1371551752f, -0.1285002828f,
    -0.1200077981f, -0.1116826907f, -0.1035329551f, -0.0955533385f,
    -0.0877547562f, -0.0801372901f, -0.0726943314f, -0.0654409826f,
    -0.0583705343f, -0.0514804162f, -0.0447806828f, -0.0382776558f,
    -0.0319531262f, -0.0258227289f, -0.0198834129f, -0.0141288824f,
    -0.0085711749f, -0.0032086896f,  0.0019765601f,  0.0069636861f,
    0.0117623834f,  0.0163701251f,  0.0207997076f,  0.0250307564f,
    0.0290824007f,  0.0329583921f,  0.0366418101f,  0.0401458293f,
    0.0434768796f,  0.0466303304f,  0.0495978668f,  0.0524093807f,
    0.0550460033f,  0.0575152710f,  0.0598166585f,  0.0619602762f,
    0.0639444813f,  0.0657690689f,  0.0674525052f,  0.0689664036f,
    0.0703533068f,  0.0715826377f,  0.0726774633f,  0.0736405998f,
    0.0744664371f,  0.0751576275f,  0.0757305771f,  0.0761748329f,
    0.0765050724f,  0.0767204911f,  0.0768230036f,  0.0768174008f,
    0.0767093524f,  0.0764992163f,  0.0761992484f,  0.0758008361f,
    0.0753137320f,  0.0747452527f,  0.0741003677f,  0.0733620226f,
    0.0725682601f,  0.0717002675f,  0.0707628727f,  0.0697630271f,
    0.0687043816f,  0.0676075965f,  0.0664367527f,  0.0652247071f,
    0.0639715865f,  0.0626857802f,  0.0613455176f,  0.0599837489f,
    0.0585915670f,  0.0571616441f,  0.0557173640f,  0.0542452782f,
    0.0527630746f,  0.0512556173f,  0.0497385748f,  0.0482165702f,
    0.0466843024f,  0.0451488420f,  0.0436097533f,  0.0420649089f,
    0.0405349173f,  0.0390053689f,  0.0374812856f,  0.0359697565f,
    0.0344620943f,  0.0329754092f,  0.0315017626f,  0.0300502665f,
    0.0286072176f,  0.0271859430f,  0.0257875845f,  0.0244160984f,
    0.0230680164f,  0.0217467546f,  0.0204531793f,  0.0191872437f,
    0.0179433376f,  0.0167324711f,  0.0155405551f,  0.0143904667f,
    -0.0132718217f, -0.0121849999f, -0.0111315548f, -0.0101150218f,
    -0.0091325333f, -0.0081798229f, -0.0072615817f, -0.0063792295f,
    -0.0055337213f, -0.0047222595f, -0.0039401124f, -0.0031933777f,
    -0.0024826725f, -0.0018039473f, -0.0011568136f, -0.0005464281f,
    0.0000276045f,  0.0005832264f,  0.0010902329f,  0.0015784682f,
    0.0020274175f,  0.0024508541f,  0.0028446757f,  0.0032091886f,
    0.0035401247f,  0.0038456409f,  0.0041251644f,  0.0043801861f,
    0.0046039531f,  0.0048109470f,  0.0049839686f,  0.0051382277f,
    0.0052715759f,  0.0053838976f,  0.0054753781f,  0.0055404366f,
    0.0055917129f,  0.0056266114f,  0.0056389198f,  0.0056455196f,
    0.0056220642f,  0.0055938023f,  0.0055475715f,  0.0054876041f,
    0.0054196776f,  0.0053471681f,  0.0052461168f,  0.0051407353f,
    0.0050393022f,  0.0049137603f,  0.0047932561f,  0.0046606460f,
    0.0045209853f,  0.0043730722f,  0.0042264271f,  0.0040819752f,
    0.0039207432f,  0.0037603923f,  0.0036008267f,  0.0034418874f,
    0.0032739614f,  0.0031125420f,  0.0029469447f,  0.0027870464f,
    0.0026201759f,  0.0024625617f,  0.0023017256f,  0.0021461584f,
    0.0019841141f,  0.0018348265f,  0.0016868083f,  0.0015443220f,
    0.0013902495f,  0.0012577885f,  0.0011250156f,  0.0009885988f,
    0.0008608443f,  0.0007458026f,  0.0006239376f,  0.0005107389f,
    0.0004026540f,  0.0002949531f,  0.0002043017f,  0.0001094383f,
    0.0000134950f, -0.0000617334f, -0.0001446381f, -0.0002098337f,
    -0.0002896981f, -0.0003501176f, -0.0004095121f, -0.0004606326f,
    -0.0005145572f, -0.0005564576f, -0.0005946119f, -0.0006341595f,
    -0.0006650415f, -0.0006917937f, -0.0007215392f, -0.0007319357f,
    -0.0007530001f, -0.0007630794f, -0.0007757978f, -0.0007801449f,
    -0.0007803665f, -0.0007779869f, -0.0007834332f, -0.0007724849f,
    -0.0007681372f, -0.0007490598f, -0.0007440942f, -0.0007255043f,
    -0.0007157737f, -0.0006941615f, -0.0006777691f, -0.0006540333f,
    -0.0006312493f, -0.0006132748f, -0.0005870931f, -0.0005677803f,
    -0.0005466565f, -0.0005226564f, -0.0005040714f, -0.0004893791f,
    -0.0004875228f, -0.0004947518f, -0.0005617692f, -0.0005525287f
};

/********************************************************************/

static const Ipp32f
SBR_TAB4_QMF_ENC_FP[] = {
    0.99983058179582340000f, 0.01840672990580482000f, 0.99847558057329477000f, 0.05519524434968993400f,
    0.99576741446765982000f, 0.09190895649713272400f, 0.99170975366909953000f, 0.12849811079379317000f,
    0.98630809724459867000f, 0.16491312048996989000f, 0.97956976568544052000f, 0.20110463484209187000f,
    0.97150389098625178000f, 0.23702360599436723000f, 0.96212140426904158000f, 0.27262135544994892000f,
    0.95143502096900834000f, 0.30784964004153492000f, 0.93945922360218992000f, 0.34266071731199438000f,
    0.92621024213831138000f, 0.37700741021641826000f, 0.91170603200542988000f, 0.41084317105790391000f,
    0.89596624975618522000f, 0.44412214457042920000f, 0.87901222642863353000f, 0.47679923006332209000f,
    0.86086693863776731000f, 0.50883014254310699000f, 0.84155497743689844000f, 0.54017147272989285000f,
    0.82110251499110465000f, 0.57078074588696726000f, 0.79953726910790501000f, 0.60061647938386897000f,
    0.77688846567323244000f, 0.62963823891492698000f, 0.75318679904361252000f, 0.65780669329707864000f,
    0.72846439044822531000f, 0.68508366777270024000f, 0.70275474445722530000f, 0.71143219574521643000f,
    0.67609270357531592000f, 0.73681656887736990000f, 0.64851440102211244000f, 0.76120238548426178000f,
    0.62005721176328921000f, 0.78455659715557513000f, 0.59075970185887428000f, 0.80684755354379922000f,
    0.56066157619733603000f, 0.82804504525775580000f, 0.52980362468629461000f, 0.84812034480329734000f,
    0.49822766697278187000f, 0.86704624551569265000f, 0.46597649576796613000f, 0.88479709843093779000f,
    0.43309381885315201000f, 0.90134884704602203000f, 0.39962419984564679000f, 0.91667905992104270000f,
    0.36561299780477396000f, 0.93076696107898371000f, 0.33110630575987643000f, 0.94359345816196039000f,
    0.29615088824362396000f, 0.95514116830577067000f, 0.26079411791527557000f, 0.96539444169768940000f,
    0.22508391135979300000f, 0.97433938278557586000f, 0.18906866414980628000f, 0.98196386910955524000f,
    0.15279718525844341000f, 0.98825756773074946000f, 0.11631863091190466000f, 0.99321194923479450000f,
    0.07968243797143012600f, 0.99682029929116567000f, 0.04293825693494073700f, 0.99907772775264536000f,
    0.00613588464915451520f, 0.99998117528260111000f, -0.03067480317663645900f, 0.99952941750109314000f,
    -0.06744391956366398200f, 0.99772306664419164000f, -0.10412163387205438000f, 0.99456457073425553000f,
    -0.14065823933284891000f, 0.99005821026229712000f, -0.17700422041214875000f, 0.98421009238692903000f,
    -0.21311031991609125000f, 0.97702814265775439000f, -0.24892760574572034000f, 0.96852209427441727000f,
    -0.28440753721127188000f, 0.95870347489587149000f, -0.31950203081601564000f, 0.94758559101774120000f,
    -0.35416352542049040000f, 0.93518350993894761000f, -0.38834504669882619000f, 0.92151403934204201000f,
    -0.42200027079979929000f, 0.90659570451491556000f, -0.45508358712634372000f, 0.89044872324475799000f,
    -0.48755016014843572000f, 0.87309497841829020000f, -0.51935599016558964000f, 0.85455798836540053000f,
    -0.55045797293660470000f, 0.83486287498638012000f, -0.58081395809576442000f, 0.81403632970594852000f,
    -0.61038280627630959000f, 0.79210657730021228000f, -0.63912444486377573000f, 0.76910333764557959000f,
    -0.66699992230363736000f, 0.74505778544146606000f, -0.69397146088965378000f, 0.72000250796138177000f
};

/*******************************************************************/

static const Ipp32f
 SBR_TAB3_QMF_ENC_FP[] = {
  1.00000000000000000000f, 0.00000000000000000000f, 0.99969881869620425000f, -0.02454122852291228800f,
    0.99879545620517241000f, -0.04906767432741801500f, 0.99729045667869021000f, -0.07356456359966742600f,
    0.99518472667219693000f, -0.09801714032956060400f, 0.99247953459870997000f, -0.12241067519921620000f,
    0.98917650996478101000f, -0.14673047445536175000f, 0.98527764238894122000f, -0.17096188876030122000f,
    0.98078528040323043000f, -0.19509032201612825000f, 0.97570213003852857000f, -0.21910124015686980000f,
    0.97003125319454397000f, -0.24298017990326387000f, 0.96377606579543984000f, -0.26671275747489837000f,
    0.95694033573220882000f, -0.29028467725446233000f, 0.94952818059303667000f, -0.31368174039889152000f,
    0.94154406518302081000f, -0.33688985339222005000f, 0.93299279883473896000f, -0.35989503653498811000f,
    0.92387953251128674000f, -0.38268343236508978000f, 0.91420975570353069000f, -0.40524131400498986000f,
    0.90398929312344334000f, -0.42755509343028208000f, 0.89322430119551532000f, -0.44961132965460654000f,
    0.88192126434835505000f, -0.47139673682599764000f, 0.87008699110871146000f, -0.49289819222978404000f,
    0.85772861000027212000f, -0.51410274419322166000f, 0.84485356524970712000f, -0.53499761988709715000f,
    0.83146961230254524000f, -0.55557023301960218000f, 0.81758481315158371000f, -0.57580819141784534000f,
    0.80320753148064494000f, -0.59569930449243336000f, 0.78834642762660634000f, -0.61523159058062682000f,
    0.77301045336273699000f, -0.63439328416364549000f, 0.75720884650648457000f, -0.65317284295377676000f,
    0.74095112535495911000f, -0.67155895484701833000f, 0.72424708295146700000f, -0.68954054473706683000f,
    0.70710678118654757000f, -0.70710678118654746000f, 0.68954054473706694000f, -0.72424708295146689000f,
    0.67155895484701833000f, -0.74095112535495911000f, 0.65317284295377676000f, -0.75720884650648446000f,
    0.63439328416364549000f, -0.77301045336273699000f, 0.61523159058062682000f, -0.78834642762660623000f,
    0.59569930449243347000f, -0.80320753148064483000f, 0.57580819141784534000f, -0.81758481315158371000f,
    0.55557023301960229000f, -0.83146961230254524000f, 0.53499761988709726000f, -0.84485356524970701000f,
    0.51410274419322166000f, -0.85772861000027212000f, 0.49289819222978409000f, -0.87008699110871135000f,
    0.47139673682599781000f, -0.88192126434835494000f, 0.44961132965460660000f, -0.89322430119551532000f,
    0.42755509343028220000f, -0.90398929312344334000f, 0.40524131400498986000f, -0.91420975570353069000f,
    0.38268343236508984000f, -0.92387953251128674000f, 0.35989503653498828000f, -0.93299279883473885000f,
    0.33688985339222005000f, -0.94154406518302081000f, 0.31368174039889157000f, -0.94952818059303667000f,
    0.29028467725446233000f, -0.95694033573220894000f, 0.26671275747489842000f, -0.96377606579543984000f,
    0.24298017990326398000f, -0.97003125319454397000f, 0.21910124015686977000f, -0.97570213003852857000f,
    0.19509032201612833000f, -0.98078528040323043000f, 0.17096188876030136000f, -0.98527764238894122000f,
    0.14673047445536175000f, -0.98917650996478101000f, 0.12241067519921628000f, -0.99247953459870997000f,
    0.09801714032956077000f, -0.99518472667219682000f, 0.07356456359966745400f, -0.99729045667869021000f,
    0.04906767432741812600f, -0.99879545620517241000f, 0.02454122852291226400f, -0.99969881869620425000f
};

/*******************************************************************/

static const Ipp32f
SBR_TAB2_QMF_ENC_FP[] = {
  0.99998117528260111000f, -0.00613588464915447530f, 0.99952941750109314000f, -0.03067480317663662600f,
    0.99847558057329477000f, -0.05519524434968993400f, 0.99682029929116567000f, -0.07968243797143012600f,
    0.99456457073425542000f, -0.10412163387205459000f, 0.99170975366909953000f, -0.12849811079379317000f,
    0.98825756773074946000f, -0.15279718525844344000f, 0.98421009238692903000f, -0.17700422041214875000f,
    0.97956976568544052000f, -0.20110463484209190000f, 0.97433938278557586000f, -0.22508391135979283000f,
    0.96852209427441738000f, -0.24892760574572015000f, 0.96212140426904158000f, -0.27262135544994898000f,
    0.95514116830577078000f, -0.29615088824362379000f, 0.94758559101774109000f, -0.31950203081601569000f,
    0.93945922360218992000f, -0.34266071731199438000f, 0.93076696107898371000f, -0.36561299780477385000f,
    0.92151403934204201000f, -0.38834504669882625000f, 0.91170603200542988000f, -0.41084317105790391000f,
    0.90134884704602203000f, -0.43309381885315196000f, 0.89044872324475788000f, -0.45508358712634384000f,
    0.87901222642863353000f, -0.47679923006332209000f, 0.86704624551569265000f, -0.49822766697278187000f,
    0.85455798836540053000f, -0.51935599016558964000f, 0.84155497743689844000f, -0.54017147272989285000f,
    0.82804504525775580000f, -0.56066157619733603000f, 0.81403632970594841000f, -0.58081395809576453000f,
    0.79953726910790501000f, -0.60061647938386897000f, 0.78455659715557524000f, -0.62005721176328910000f,
    0.76910333764557970000f, -0.63912444486377573000f, 0.75318679904361252000f, -0.65780669329707864000f,
    0.73681656887736990000f, -0.67609270357531592000f, 0.72000250796138165000f, -0.69397146088965400000f,
    0.70275474445722530000f, -0.71143219574521643000f, 0.68508366777270036000f, -0.72846439044822520000f,
    0.66699992230363747000f, -0.74505778544146595000f, 0.64851440102211255000f, -0.76120238548426178000f,
    0.62963823891492710000f, -0.77688846567323244000f, 0.61038280627630948000f, -0.79210657730021239000f,
    0.59075970185887428000f, -0.80684755354379922000f, 0.57078074588696737000f, -0.82110251499110465000f,
    0.55045797293660481000f, -0.83486287498638001000f, 0.52980362468629483000f, -0.84812034480329712000f,
    0.50883014254310699000f, -0.86086693863776731000f, 0.48755016014843605000f, -0.87309497841829009000f,
    0.46597649576796613000f, -0.88479709843093779000f, 0.44412214457042926000f, -0.89596624975618511000f,
    0.42200027079979979000f, -0.90659570451491533000f, 0.39962419984564679000f, -0.91667905992104270000f,
    0.37700741021641831000f, -0.92621024213831127000f, 0.35416352542049051000f, -0.93518350993894750000f,
    0.33110630575987643000f, -0.94359345816196039000f, 0.30784964004153498000f, -0.95143502096900834000f,
    0.28440753721127182000f, -0.95870347489587160000f, 0.26079411791527557000f, -0.96539444169768940000f,
    0.23702360599436734000f, -0.97150389098625178000f, 0.21311031991609136000f, -0.97702814265775439000f,
    0.18906866414980628000f, -0.98196386910955524000f, 0.16491312048997009000f, -0.98630809724459867000f,
    0.14065823933284924000f, -0.99005821026229712000f, 0.11631863091190488000f, -0.99321194923479450000f,
    0.09190895649713269600f, -0.99576741446765982000f, 0.06744391956366410600f, -0.99772306664419164000f,
    0.04293825693494095900f, -0.99907772775264536000f, 0.01840672990580482000f, -0.99983058179582340000f
};

/********************************************************************/

AACStatus ownAnalysisFilterInitAlloc_SBREnc_RToC_32f32fc(ownFilterSpec_SBR_C_32fc* pQMFSpec,
                                                         Ipp32s *sizeAll)
{
  IppStatus status = ippStsNoErr;
  Ipp32s flag  = IPP_FFT_NODIV_BY_ANY;
  Ipp32s order = 6; //64 point
  IppHintAlgorithm hint = ippAlgHintAccurate;
  Ipp32s sizeSpec, sizeInit, sizeWork, sizeQMFSpec;
  Ipp8u* pBufInit = NULL;

  status = ippsFFTGetSize_C_32fc(order, flag, hint,
                                 &sizeSpec, &sizeInit, &sizeWork);

  if (ippStsNoErr != status){
    return AAC_ALLOC;
  }

  sizeQMFSpec = sizeof( ownFilterSpec_SBR_C_32fc );

  *sizeAll = sizeSpec + sizeWork + sizeQMFSpec + sizeInit;

  if (pQMFSpec) {
    pQMFSpec->pMemSpec = ((Ipp8u*)pQMFSpec + sizeQMFSpec);
    pQMFSpec->pWorkBuf = ((Ipp8u*)pQMFSpec->pMemSpec + sizeQMFSpec + sizeSpec);
    pBufInit = (Ipp8u*)pQMFSpec->pWorkBuf + sizeQMFSpec;

    pQMFSpec->pFFTSpec = (IppsFFTSpec_C_32fc*)pQMFSpec->pMemSpec;
    status = ippsFFTInit_C_32fc( &(pQMFSpec->pFFTSpec), order, flag, hint, pQMFSpec->pMemSpec, pBufInit);
    if (ippStsNoErr != status){
      return AAC_ALLOC;
    }
  }
  return AAC_OK;
}

/********************************************************************/

static void ownsWindowing_SBREnc_fp(Ipp32f* pSrc, Ipp32f* pDst, Ipp32f* pTabWin)
{
  Ipp32s n, j;
  Ipp32f sum;

  for(n=0; n<128; n++){
    sum = 0.0f;
    for(j=0; j<5; j++){
      sum += pSrc[639 - n - j*128] * pTabWin[n+j*128];
    }

    pDst[n] = sum;
  }

  return;
}

/*******************************************************************/
/*
static Ipp32s ref_ownsAnalysisFilter_SBREnc_FT_fp(Ipp32f* pSrc, Ipp32fc* pDst)
{
  Ipp32s n = 0, k = 0;
  Ipp64f sumRe = 0.0, sumIm = 0.0;
  Ipp64f arg = 0.0;

  for(k = 0; k < 64; k++)
  {
    sumRe = sumIm = 0.f;
    for(n = 0; n < 128; n++)
    {
      arg = (k + 0.5) * (2*n +1.0 ) * M_PI / (128.0);
      sumRe += pSrc[n] * cos(arg);
      sumIm += pSrc[n] * sin(arg);
    }

    pDst[k].re = (Ipp32f)sumRe;
    pDst[k].im = (Ipp32f)sumIm;
  }

  return 0;
}
*/
/*******************************************************************/
/*
static Ipp32s ippsDCT4_32f(Ipp32f* pSrc, Ipp32f* pDst, Ipp32s len)
{
  Ipp32s j = 0, k = 0;
  Ipp64f sum = 0.f;
  Ipp64f arg = 0.f;

  for(k = 0; k < len; k++)
  {
    sum = 0.0;
    for(j = 0; j < len; j++)
    {
      arg = (2*j + 1) * (2*k + 1) * M_PI / (4*len);
      sum += pSrc[j] * cos(arg);
    }

    pDst[k] = (Ipp32f)sum;
  }

  return 0;//OK
}
*/
/*******************************************************************/

static Ipp32s ownsPreProc_AQMF_DCT4_fp(Ipp32f* pSrc, Ipp32f* pDst)
{
  Ipp32s i;

  pDst[0] = pSrc[0];
  pDst[1] = pSrc[1];

  for(i=1; i<63; i++){
    pDst[2*i]   = -pSrc[128-i];
    pDst[2*i+1] = pSrc[i+1];
  }

  pDst[127] = pSrc[64];
  pDst[126] = -pSrc[65];

  return 0;
}

/*******************************************************************/

static Ipp32s ownsPostProc_AQMF_DCT4_fp(Ipp32f* pSrc, Ipp32fc* pDst)
{
  Ipp32s i;
  Ipp64f a, b, c, s;

  for(i = 0; i < 64; i++)
  {
    a = pDst[i].re = pSrc[i];
    b = pDst[i].im = -pSrc[127 - i];

    /* auxiliary */
    c = SBR_TAB4_QMF_ENC_FP[2*i];
    s = SBR_TAB4_QMF_ENC_FP[2*i+1];

    pDst[i].re = (Ipp32f)(a*c - b*s);
    pDst[i].im = (Ipp32f)(b*c + a*s);

  }

  return 0;
}

/*******************************************************************/

static Ipp32s ownsPreProc_DCT4_FFT_fp(Ipp32f* pSrc, Ipp32fc* pDst, Ipp32s len)
{
  Ipp32s j;
  Ipp64f a, b, c, s;

  for(j=0; j < len/2; j++)
  {
    c = SBR_TAB2_QMF_ENC_FP[2*j]; //cos(arg);
    s = SBR_TAB2_QMF_ENC_FP[2*j+1]; //sin(arg);

    a = pSrc[2*j];
    b = pSrc[len - 1 - 2*j];

    pDst[j].re = (Ipp32f)(a*c - b*s);
    pDst[j].im = (Ipp32f)(a*s + b*c);
  }

  return 0;
}

/*******************************************************************/

static Ipp32s ownsPostProc_DCT4_FFT_fp(Ipp32fc* pSrc, Ipp32f* pDst, Ipp32s len)
{
  Ipp32s u;
  Ipp64f a, b, c, s;

  for(u=0; u<len/2; u++) {
    c = SBR_TAB3_QMF_ENC_FP[2*u];
    s = SBR_TAB3_QMF_ENC_FP[2*u+1];

    a = pSrc[ u ].re;
    b = pSrc[ u ].im;

    pDst[2*u] = (Ipp32f)(a*c - b*s);
    pDst[len - 1 - 2*u] = - (Ipp32f)(a*s + b*c);
  }

  return 0;
}

/*******************************************************************/

static Ipp32s ownsDCT4_FFT_fp(Ipp32f* pSrc, Ipp32f* pDst, Ipp32s len, const IppsFFTSpec_C_32fc* pFFTSpec, Ipp8u* pBuffer)
{

#if !defined(ANDROID)
  IPP_ALIGNED_ARRAY(32, Ipp32fc, bufPre, 64);
  IPP_ALIGNED_ARRAY(32, Ipp32fc, bufFFT, 64);
#else
  static IPP_ALIGNED_ARRAY(32, Ipp32fc, bufPre, 64);
  static IPP_ALIGNED_ARRAY(32, Ipp32fc, bufFFT, 64);
#endif

  ownsPreProc_DCT4_FFT_fp(pSrc, bufPre, len);

  ippsFFTFwd_CToC_32fc( (const Ipp32fc*)bufPre, bufFFT, pFFTSpec, pBuffer);

  ownsPostProc_DCT4_FFT_fp(bufFFT, pDst, len);

  return 0;
}

/*******************************************************************/

Ipp32s ownsAnalysisFilter_SBREnc_FT_fp(Ipp32f* pSrc, Ipp32fc* pDst, const IppsFFTSpec_C_32fc* pFFTSpec, Ipp8u* pBuffer)
{
#if !defined(ANDROID)
  IPP_ALIGNED_ARRAY(32, Ipp32f, inDCT4_128_32f, 128);
  IPP_ALIGNED_ARRAY(32, Ipp32f, outDCT4_128_32f, 128);
#else
  static IPP_ALIGNED_ARRAY(32, Ipp32f, inDCT4_128_32f, 128);
  static IPP_ALIGNED_ARRAY(32, Ipp32f, outDCT4_128_32f, 128);
#endif

   /* opt AQMF */
  ownsPreProc_AQMF_DCT4_fp(pSrc, inDCT4_128_32f);
  ownsDCT4_FFT_fp(inDCT4_128_32f, outDCT4_128_32f, 128, pFFTSpec, pBuffer);
  ownsPostProc_AQMF_DCT4_fp(outDCT4_128_32f, pDst);

  return 0;
}

/*******************************************************************/

AACStatus ownAnalysisFilter_SBREnc_RToC_32f32fc(Ipp32f* pSrc, Ipp32fc* pDst, ownFilterSpec_SBR_C_32fc* pSpec)
{
#if !defined(ANDROID)
   IPP_ALIGNED_ARRAY(32, Ipp32f, bufU, 1024);
#else
   static IPP_ALIGNED_ARRAY(32, Ipp32f, bufU, 1024);
#endif

  ownsWindowing_SBREnc_fp(pSrc, bufU, (Ipp32f*)SBR_TAB_QMF_WINDOW_640_FP);

  ownsAnalysisFilter_SBREnc_FT_fp(bufU, pDst, pSpec->pFFTSpec, pSpec->pWorkBuf);
  //ref_ownsAnalysisFilter_SBREnc_FT_fp(bufU, pDst);

  return AAC_OK;
}

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_AAC_AUDIO_ENCODER

