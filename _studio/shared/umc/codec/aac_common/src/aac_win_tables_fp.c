// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_DECODER) || defined (UMC_ENABLE_AAC_AUDIO_ENCODER)

#include "ippdefs.h"

Ipp32f KBD_long[] = {
  0.0002925615390f, 0.0004299856735f, 0.0005467407459f, 0.0006548230430f,
  0.0007587019507f, 0.0008605933171f, 0.0009617754144f, 0.0010630609411f,
  0.0011650036308f, 0.0012680012194f, 0.0013723517233f, 0.0014782864109f,
  0.0015859901977f, 0.0016956148252f, 0.0018072876904f, 0.0019211179406f,
  0.0020372007924f, 0.0021556206592f, 0.0022764534600f, 0.0023997683541f,
  0.0025256290631f, 0.0026540948921f, 0.0027852215281f, 0.0029190616715f,
  0.0030556655443f, 0.0031950812943f, 0.0033373553240f, 0.0034825325587f,
  0.0036306566699f, 0.0037817702605f, 0.0039359150180f, 0.0040931318437f,
  0.0042534609610f, 0.0044169420067f, 0.0045836141091f, 0.0047535159544f,
  0.0049266858431f, 0.0051031617391f, 0.0052829813111f, 0.0054661819694f,
  0.0056528008964f, 0.0058428750740f, 0.0060364413071f, 0.0062335362436f,
  0.0064341963925f, 0.0066384581387f, 0.0068463577565f, 0.0070579314216f,
  0.0072732152203f, 0.0074922451587f, 0.0077150571701f, 0.0079416871213f,
  0.0081721708181f, 0.0084065440099f, 0.0086448423940f, 0.0088871016184f,
  0.0091333572848f, 0.0093836449508f, 0.0096380001314f, 0.0098964583007f,
  0.0101590548923f, 0.0104258253006f, 0.0106968048803f, 0.0109720289472f,
  0.0112515327772f, 0.0115353516066f, 0.0118235206309f, 0.0121160750040f,
  0.0124130498374f, 0.0127144801990f, 0.0130204011115f, 0.0133308475512f,
  0.0136458544463f, 0.0139654566754f, 0.0142896890653f, 0.0146185863897f,
  0.0149521833667f, 0.0152905146570f, 0.0156336148617f, 0.0159815185202f,
  0.0163342601079f, 0.0166918740338f, 0.0170543946382f, 0.0174218561904f,
  0.0177942928858f, 0.0181717388441f, 0.0185542281060f, 0.0189417946310f,
  0.0193344722950f, 0.0197322948869f, 0.0201352961068f, 0.0205435095626f,
  0.0209569687675f, 0.0213757071373f, 0.0217997579874f, 0.0222291545303f,
  0.0226639298725f, 0.0231041170117f, 0.0235497488338f, 0.0240008581104f,
  0.0244574774955f, 0.0249196395226f, 0.0253873766022f, 0.0258607210183f,
  0.0263397049257f, 0.0268243603472f, 0.0273147191701f, 0.0278108131439f,
  0.0283126738768f, 0.0288203328328f, 0.0293338213289f, 0.0298531705319f,
  0.0303784114553f, 0.0309095749565f, 0.0314466917337f, 0.0319897923229f,
  0.0325389070947f, 0.0330940662514f, 0.0336552998239f, 0.0342226376690f,
  0.0347961094657f, 0.0353757447128f, 0.0359615727256f, 0.0365536226328f,
  0.0371519233734f, 0.0377565036943f, 0.0383673921462f, 0.0389846170817f,
  0.0396082066514f, 0.0402381888014f, 0.0408745912700f, 0.0415174415850f,
  0.0421667670603f, 0.0428225947934f, 0.0434849516619f, 0.0441538643208f,
  0.0448293591995f, 0.0455114624989f, 0.0462002001882f, 0.0468955980022f,
  0.0475976814382f, 0.0483064757531f, 0.0490220059605f, 0.0497442968277f,
  0.0504733728731f, 0.0512092583629f, 0.0519519773083f, 0.0527015534628f,
  0.0534580103193f, 0.0542213711072f, 0.0549916587894f, 0.0557688960598f,
  0.0565531053401f, 0.0573443087775f, 0.0581425282414f, 0.0589477853209f,
  0.0597601013220f, 0.0605794972649f, 0.0614059938812f, 0.0622396116110f,
  0.0630803706008f, 0.0639282907000f, 0.0647833914589f, 0.0656456921257f,
  0.0665152116441f, 0.0673919686503f, 0.0682759814708f, 0.0691672681197f,
  0.0700658462959f, 0.0709717333811f, 0.0718849464366f, 0.0728055022013f,
  0.0737334170889f, 0.0746687071856f, 0.0756113882478f, 0.0765614756992f,
  0.0775189846287f, 0.0784839297883f, 0.0794563255900f, 0.0804361861042f,
  0.0814235250568f, 0.0824183558274f, 0.0834206914466f, 0.0844305445938f,
  0.0854479275955f, 0.0864728524222f, 0.0875053306869f, 0.0885453736427f,
  0.0895929921808f, 0.0906481968279f, 0.0917109977449f, 0.0927814047241f,
  0.0938594271876f, 0.0949450741852f, 0.0960383543921f, 0.0971392761074f,
  0.0982478472520f, 0.0993640753666f, 0.1004879676096f, 0.1016195307560f,
  0.1027587711945f, 0.1039056949267f, 0.1050603075647f, 0.1062226143295f,
  0.1073926200494f, 0.1085703291582f, 0.1097557456936f, 0.1109488732953f,
  0.1121497152040f, 0.1133582742591f, 0.1145745528977f, 0.1157985531527f,
  0.1170302766517f, 0.1182697246151f, 0.1195168978550f, 0.1207717967738f,
  0.1220344213626f, 0.1233047712001f, 0.1245828454510f, 0.1258686428652f,
  0.1271621617762f, 0.1284634000997f, 0.1297723553331f, 0.1310890245537f,
  0.1324134044180f, 0.1337454911603f, 0.1350852805917f, 0.1364327680996f,
  0.1377879486460f, 0.1391508167668f, 0.1405213665711f, 0.1418995917403f,
  0.1432854855267f, 0.1446790407535f, 0.1460802498134f, 0.1474891046680f,
  0.1489055968475f, 0.1503297174493f, 0.1517614571379f, 0.1532008061441f,
  0.1546477542646f, 0.1561022908610f, 0.1575644048599f, 0.1590340847519f,
  0.1605113185917f, 0.1619960939971f, 0.1634883981492f, 0.1649882177916f,
  0.1664955392304f, 0.1680103483340f, 0.1695326305327f, 0.1710623708184f,
  0.1725995537448f, 0.1741441634271f, 0.1756961835419f, 0.1772555973272f,
  0.1788223875824f, 0.1803965366683f, 0.1819780265073f, 0.1835668385834f,
  0.1851629539423f, 0.1867663531917f, 0.1883770165015f, 0.1899949236038f,
  0.1916200537938f, 0.1932523859294f, 0.1948918984321f, 0.1965385692871f,
  0.1981923760441f, 0.1998532958172f, 0.2015213052861f, 0.2031963806959f,
  0.2048784978587f, 0.2065676321530f, 0.2082637585254f, 0.2099668514908f,
  0.2116768851333f, 0.2133938331068f, 0.2151176686360f, 0.2168483645172f,
  0.2185858931192f, 0.2203302263843f, 0.2220813358289f, 0.2238391925450f,
  0.2256037672011f, 0.2273750300430f, 0.2291529508952f, 0.2309374991619f,
  0.2327286438284f, 0.2345263534620f, 0.2363305962136f, 0.2381413398188f,
  0.2399585515993f, 0.2417821984640f, 0.2436122469111f, 0.2454486630289f,
  0.2472914124974f, 0.2491404605901f, 0.2509957721752f, 0.2528573117176f,
  0.2547250432802f, 0.2565989305256f, 0.2584789367179f, 0.2603650247245f,
  0.2622571570178f, 0.2641552956769f, 0.2660594023897f, 0.2679694384544f,
  0.2698853647819f, 0.2718071418974f, 0.2737347299426f, 0.2756680886774f,
  0.2776071774824f, 0.2795519553607f, 0.2815023809402f, 0.2834584124756f,
  0.2854200078506f, 0.2873871245804f, 0.2893597198136f, 0.2913377503349f,
  0.2933211725670f, 0.2953099425734f, 0.2973040160603f, 0.2993033483797f,
  0.3013078945313f, 0.3033176091652f, 0.3053324465845f, 0.3073523607479f,
  0.3093773052720f, 0.3114072334343f, 0.3134420981758f, 0.3154818521036f,
  0.3175264474934f, 0.3195758362929f, 0.3216299701239f, 0.3236888002857f,
  0.3257522777574f, 0.3278203532013f, 0.3298929769657f, 0.3319700990874f,
  0.3340516692952f, 0.3361376370130f, 0.3382279513620f, 0.3403225611650f,
  0.3424214149482f, 0.3445244609455f, 0.3466316471007f, 0.3487429210714f,
  0.3508582302318f, 0.3529775216760f, 0.3551007422213f, 0.3572278384116f,
  0.3593587565206f, 0.3614934425551f, 0.3636318422586f, 0.3657739011144f,
  0.3679195643493f, 0.3700687769368f, 0.3722214836007f, 0.3743776288188f,
  0.3765371568260f, 0.3787000116183f, 0.3808661369561f, 0.3830354763677f,
  0.3852079731532f, 0.3873835703882f, 0.3895622109271f, 0.3917438374070f,
  0.3939283922516f, 0.3961158176745f, 0.3983060556834f, 0.4004990480837f,
  0.4026947364822f, 0.4048930622910f, 0.4070939667315f, 0.4092973908381f,
  0.4115032754620f, 0.4137115612752f, 0.4159221887747f, 0.4181350982859f,
  0.4203502299670f, 0.4225675238127f, 0.4247869196585f, 0.4270083571842f,
  0.4292317759187f, 0.4314571152431f, 0.4336843143958f, 0.4359133124756f,
  0.4381440484466f, 0.4403764611416f, 0.4426104892669f, 0.4448460714059f,
  0.4470831460236f, 0.4493216514706f, 0.4515615259873f, 0.4538027077081f,
  0.4560451346658f, 0.4582887447954f, 0.4605334759388f, 0.4627792658486f,
  0.4650260521928f, 0.4672737725586f, 0.4695223644572f, 0.4717717653275f,
  0.4740219125410f, 0.4762727434056f, 0.4785241951701f, 0.4807762050287f,
  0.4830287101251f, 0.4852816475567f, 0.4875349543796f, 0.4897885676121f,
  0.4920424242397f, 0.4942964612190f, 0.4965506154825f, 0.4988048239427f,
  0.5010590234966f, 0.5033131510300f, 0.5055671434219f, 0.5078209375490f,
  0.5100744702899f, 0.5123276785297f, 0.5145804991643f, 0.5168328691049f,
  0.5190847252821f, 0.5213360046508f, 0.5235866441942f, 0.5258365809283f,
  0.5280857519065f, 0.5303340942237f, 0.5325815450209f, 0.5348280414897f,
  0.5370735208765f, 0.5393179204869f, 0.5415611776902f, 0.5438032299239f,
  0.5460440146977f, 0.5482834695983f, 0.5505215322938f, 0.5527581405377f,
  0.5549932321734f, 0.5572267451388f, 0.5594586174706f, 0.5616887873084f,
  0.5639171928993f, 0.5661437726021f, 0.5683684648919f, 0.5705912083639f,
  0.5728119417384f, 0.5750306038644f, 0.5772471337246f, 0.5794614704391f,
  0.5816735532701f, 0.5838833216259f, 0.5860907150653f, 0.5882956733017f,
  0.5904981362077f, 0.5926980438188f, 0.5948953363380f, 0.5970899541400f,
  0.5992818377750f, 0.6014709279733f, 0.6036571656494f, 0.6058404919058f,
  0.6080208480376f, 0.6101981755363f, 0.6123724160939f, 0.6145435116072f,
  0.6167114041816f, 0.6188760361353f, 0.6210373500034f, 0.6231952885417f,
  0.6253497947309f, 0.6275008117804f, 0.6296482831325f, 0.6317921524660f,
  0.6339323637003f, 0.6360688609995f, 0.6382015887758f, 0.6403304916938f,
  0.6424555146741f, 0.6445766028973f, 0.6466937018074f, 0.6488067571161f,
  0.6509157148060f, 0.6530205211349f, 0.6551211226391f, 0.6572174661369f,
  0.6593094987329f, 0.6613971678210f, 0.6634804210884f, 0.6655592065189f,
  0.6676334723966f, 0.6697031673095f, 0.6717682401526f, 0.6738286401320f,
  0.6758843167677f, 0.6779352198975f, 0.6799812996802f, 0.6820225065988f,
  0.6840587914640f, 0.6860901054177f, 0.6881163999359f, 0.6901376268320f,
  0.6921537382601f, 0.6941646867185f, 0.6961704250521f, 0.6981709064563f,
  0.7001660844796f, 0.7021559130266f, 0.7041403463616f, 0.7061193391110f,
  0.7080928462663f, 0.7100608231875f, 0.7120232256055f, 0.7139800096253f,
  0.7159311317284f, 0.7178765487761f, 0.7198162180119f, 0.7217500970645f,
  0.7236781439499f, 0.7256003170750f, 0.7275165752393f, 0.7294268776380f,
  0.7313311838646f, 0.7332294539128f, 0.7351216481798f, 0.7370077274680f,
  0.7388876529879f, 0.7407613863602f, 0.7426288896183f, 0.7444901252103f,
  0.7463450560015f, 0.7481936452766f, 0.7500358567418f, 0.7518716545266f,
  0.7537010031867f, 0.7555238677052f, 0.7573402134950f, 0.7591500064009f,
  0.7609532127014f, 0.7627497991102f, 0.7645397327787f, 0.7663229812976f,
  0.7680995126982f, 0.7698692954548f, 0.7716322984860f, 0.7733884911565f,
  0.7751378432785f, 0.7768803251134f, 0.7786159073734f, 0.7803445612228f,
  0.7820662582796f, 0.7837809706167f, 0.7854886707633f, 0.7871893317064f,
  0.7888829268919f, 0.7905694302256f, 0.7922488160749f, 0.7939210592695f,
  0.7955861351025f, 0.7972440193317f, 0.7988946881805f, 0.8005381183386f,
  0.8021742869633f, 0.8038031716803f, 0.8054247505841f, 0.8070390022392f,
  0.8086459056809f, 0.8102454404156f, 0.8118375864217f, 0.8134223241503f,
  0.8149996345254f, 0.8165694989447f, 0.8181318992799f, 0.8196868178774f,
  0.8212342375582f, 0.8227741416187f, 0.8243065138308f, 0.8258313384418f,
  0.8273486001753f, 0.8288582842307f, 0.8303603762837f, 0.8318548624861f,
  0.8333417294660f, 0.8348209643276f, 0.8362925546513f, 0.8377564884934f,
  0.8392127543861f, 0.8406613413372f, 0.8421022388295f, 0.8435354368213f,
  0.8449609257452f, 0.8463786965083f, 0.8477887404914f, 0.8491910495485f,
  0.8505856160068f, 0.8519724326652f, 0.8533514927946f, 0.8547227901365f,
  0.8560863189029f, 0.8574420737751f, 0.8587900499030f, 0.8601302429042f,
  0.8614626488635f, 0.8627872643312f, 0.8641040863231f, 0.8654131123184f,
  0.8667143402595f, 0.8680077685505f, 0.8692933960559f, 0.8705712220998f,
  0.8718412464643f, 0.8731034693884f, 0.8743578915665f, 0.8756045141472f,
  0.8768433387317f, 0.8780743673726f, 0.8792976025720f, 0.8805130472804f,
  0.8817207048946f, 0.8829205792564f, 0.8841126746512f, 0.8852969958054f,
  0.8864735478854f, 0.8876423364958f, 0.8888033676769f, 0.8899566479035f,
  0.8911021840826f, 0.8922399835515f, 0.8933700540760f, 0.8944924038479f,
  0.8956070414835f, 0.8967139760207f, 0.8978132169179f, 0.8989047740505f,
  0.8999886577099f, 0.9010648786003f, 0.9021334478369f, 0.9031943769431f,
  0.9042476778487f, 0.9052933628869f, 0.9063314447920f, 0.9073619366971f,
  0.9083848521312f, 0.9094002050169f, 0.9104080096678f, 0.9114082807853f,
  0.9124010334568f, 0.9133862831523f, 0.9143640457217f, 0.9153343373924f,
  0.9162971747659f, 0.9172525748156f, 0.9182005548833f, 0.9191411326766f,
  0.9200743262659f, 0.9210001540812f, 0.9219186349094f, 0.9228297878911f,
  0.9237336325174f, 0.9246301886269f, 0.9255194764025f, 0.9264015163682f,
  0.9272763293862f, 0.9281439366532f, 0.9290043596973f, 0.9298576203748f,
  0.9307037408668f, 0.9315427436761f, 0.9323746516233f, 0.9331994878438f,
  0.9340172757844f, 0.9348280391997f, 0.9356318021484f, 0.9364285889904f,
  0.9372184243828f, 0.9380013332764f, 0.9387773409122f, 0.9395464728181f,
  0.9403087548046f, 0.9410642129618f, 0.9418128736556f, 0.9425547635236f,
  0.9432899094721f, 0.9440183386718f, 0.9447400785544f, 0.9454551568085f,
  0.9461636013764f, 0.9468654404497f, 0.9475607024659f, 0.9482494161043f,
  0.9489316102825f, 0.9496073141521f, 0.9502765570953f, 0.9509393687206f,
  0.9515957788592f, 0.9522458175612f, 0.9528895150910f, 0.9535269019242f,
  0.9541580087431f, 0.9547828664332f, 0.9554015060786f, 0.9560139589587f,
  0.9566202565437f, 0.9572204304910f, 0.9578145126408f, 0.9584025350126f,
  0.9589845298006f, 0.9595605293701f, 0.9601305662534f, 0.9606946731456f,
  0.9612528829007f, 0.9618052285277f, 0.9623517431862f, 0.9628924601826f,
  0.9634274129660f, 0.9639566351242f, 0.9644801603796f, 0.9649980225850f,
  0.9655102557199f, 0.9660168938860f, 0.9665179713038f, 0.9670135223077f,
  0.9675035813427f, 0.9679881829600f, 0.9684673618130f, 0.9689411526533f,
  0.9694095903267f, 0.9698727097691f, 0.9703305460027f, 0.9707831341316f,
  0.9712305093382f, 0.9716727068789f, 0.9721097620803f, 0.9725417103353f,
  0.9729685870987f, 0.9733904278839f, 0.9738072682584f, 0.9742191438402f,
  0.9746260902935f, 0.9750281433253f, 0.9754253386813f, 0.9758177121416f,
  0.9762052995176f, 0.9765881366475f, 0.9769662593928f, 0.9773397036345f,
  0.9777085052688f, 0.9780727002043f, 0.9784323243570f, 0.9787874136477f,
  0.9791380039974f, 0.9794841313241f, 0.9798258315389f, 0.9801631405424f,
  0.9804960942210f, 0.9808247284431f, 0.9811490790561f, 0.9814691818820f,
  0.9817850727144f, 0.9820967873148f, 0.9824043614090f, 0.9827078306839f,
  0.9830072307834f, 0.9833025973059f, 0.9835939657999f, 0.9838813717615f,
  0.9841648506303f, 0.9844444377865f, 0.9847201685475f, 0.9849920781646f,
  0.9852602018198f, 0.9855245746224f, 0.9857852316061f, 0.9860422077256f,
  0.9862955378536f, 0.9865452567777f, 0.9867913991973f, 0.9870339997204f,
  0.9872730928609f, 0.9875087130356f, 0.9877408945609f, 0.9879696716504f,
  0.9881950784115f, 0.9884171488432f, 0.9886359168327f, 0.9888514161528f,
  0.9890636804596f, 0.9892727432890f, 0.9894786380547f, 0.9896813980455f,
  0.9898810564224f, 0.9900776462162f, 0.9902712003250f, 0.9904617515119f,
  0.9906493324021f, 0.9908339754810f, 0.9910157130915f, 0.9911945774319f,
  0.9913706005534f, 0.9915438143578f, 0.9917142505958f, 0.9918819408641f,
  0.9920469166039f, 0.9922092090982f, 0.9923688494705f, 0.9925258686819f,
  0.9926802975299f, 0.9928321666461f, 0.9929815064942f, 0.9931283473685f,
  0.9932727193917f, 0.9934146525134f, 0.9935541765082f, 0.9936913209743f,
  0.9938261153313f, 0.9939585888191f, 0.9940887704961f, 0.9942166892378f,
  0.9943423737350f, 0.9944658524929f, 0.9945871538291f, 0.9947063058725f,
  0.9948233365623f, 0.9949382736460f, 0.9950511446788f, 0.9951619770220f,
  0.9952707978421f, 0.9953776341096f, 0.9954825125978f, 0.9955854598818f,
  0.9956865023377f, 0.9957856661414f, 0.9958829772678f, 0.9959784614900f,
  0.9960721443783f, 0.9961640512995f, 0.9962542074160f, 0.9963426376853f,
  0.9964293668593f, 0.9965144194835f, 0.9965978198966f, 0.9966795922298f,
  0.9967597604062f, 0.9968383481406f, 0.9969153789389f, 0.9969908760977f,
  0.9970648627039f, 0.9971373616344f, 0.9972083955559f, 0.9972779869246f,
  0.9973461579859f, 0.9974129307743f, 0.9974783271134f, 0.9975423686154f,
  0.9976050766816f, 0.9976664725018f, 0.9977265770548f, 0.9977854111080f,
  0.9978429952179f, 0.9978993497298f, 0.9979544947783f, 0.9980084502873f,
  0.9980612359703f, 0.9981128713304f, 0.9981633756611f, 0.9982127680460f,
  0.9982610673595f, 0.9983082922673f, 0.9983544612265f, 0.9983995924861f,
  0.9984437040877f, 0.9984868138657f, 0.9985289394480f, 0.9985700982568f,
  0.9986103075087f, 0.9986495842155f, 0.9986879451850f, 0.9987254070218f,
  0.9987619861274f, 0.9987976987016f, 0.9988325607430f, 0.9988665880495f,
  0.9988997962198f, 0.9989322006536f, 0.9989638165525f, 0.9989946589215f,
  0.9990247425692f, 0.9990540821092f, 0.9990826919606f, 0.9991105863495f,
  0.9991377793099f, 0.9991642846842f, 0.9991901161251f, 0.9992152870958f,
  0.9992398108717f, 0.9992637005415f, 0.9992869690078f, 0.9993096289888f,
  0.9993316930191f, 0.9993531734513f, 0.9993740824566f, 0.9993944320267f,
  0.9994142339746f, 0.9994334999357f, 0.9994522413697f, 0.9994704695613f,
  0.9994881956217f, 0.9995054304900f, 0.9995221849344f, 0.9995384695535f,
  0.9995542947780f, 0.9995696708715f, 0.9995846079324f, 0.9995991158949f,
  0.9996132045308f, 0.9996268834504f, 0.9996401621043f, 0.9996530497850f,
  0.9996655556277f, 0.9996776886123f, 0.9996894575647f, 0.9997008711583f,
  0.9997119379151f, 0.9997226662079f, 0.9997330642612f, 0.9997431401529f,
  0.9997529018157f, 0.9997623570388f, 0.9997715134691f, 0.9997803786133f,
  0.9997889598384f, 0.9997972643745f, 0.9998052993151f, 0.9998130716194f,
  0.9998205881138f, 0.9998278554928f, 0.9998348803214f, 0.9998416690360f,
  0.9998482279461f, 0.9998545632358f, 0.9998606809657f, 0.9998665870739f,
  0.9998722873776f, 0.9998777875752f, 0.9998830932472f, 0.9998882098578f,
  0.9998931427568f, 0.9998978971807f, 0.9999024782547f, 0.9999068909936f,
  0.9999111403038f, 0.9999152309846f, 0.9999191677297f, 0.9999229551289f,
  0.9999265976693f, 0.9999300997369f, 0.9999334656182f, 0.9999366995016f,
  0.9999398054787f, 0.9999427875460f, 0.9999456496064f, 0.9999483954703f,
  0.9999510288575f, 0.9999535533981f, 0.9999559726345f, 0.9999582900225f,
  0.9999605089326f, 0.9999626326518f, 0.9999646643846f, 0.9999666072545f,
  0.9999684643056f, 0.9999702385036f, 0.9999719327374f, 0.9999735498204f,
  0.9999750924918f, 0.9999765634181f, 0.9999779651940f, 0.9999793003441f,
  0.9999805713242f, 0.9999817805222f, 0.9999829302598f, 0.9999840227934f,
  0.9999850603157f, 0.9999860449569f, 0.9999869787854f, 0.9999878638097f,
  0.9999887019792f, 0.9999894951857f, 0.9999902452641f, 0.9999909539940f,
  0.9999916231008f, 0.9999922542565f, 0.9999928490813f, 0.9999934091443f,
  0.9999939359651f, 0.9999944310142f, 0.9999948957147f, 0.9999953314431f,
  0.9999957395304f, 0.9999961212630f, 0.9999964778839f, 0.9999968105938f,
  0.9999971205518f, 0.9999974088765f, 0.9999976766471f, 0.9999979249043f,
  0.9999981546512f, 0.9999983668543f, 0.9999985624441f, 0.9999987423168f,
  0.9999989073340f, 0.9999990583249f, 0.9999991960861f, 0.9999993213830f,
  0.9999994349506f, 0.9999995374939f, 0.9999996296895f, 0.9999997121856f,
  0.9999997856034f, 0.9999998505373f, 0.9999999075562f, 0.9999999572039f
};

Ipp32f KBD_long_ssr[] = {
  0.0005851230124f, 0.0009642149851f, 0.0013558207535f, 0.0017771849644f,
  0.0022352533850f, 0.0027342299070f, 0.0032773001022f, 0.0038671998069f,
  0.0045064443384f, 0.0051974336885f, 0.0059425050016f, 0.0067439602523f,
  0.0076040812645f, 0.0085251378136f, 0.0095093917383f, 0.0105590986429f,
  0.0116765080854f, 0.0128638627793f, 0.0141233971319f, 0.0154573353235f,
  0.0168678890601f, 0.0183572550877f, 0.0199276125320f, 0.0215811201042f,
  0.0233199132077f, 0.0251461009667f, 0.0270617631982f, 0.0290689473406f,
  0.0311696653516f, 0.0333658905864f, 0.0356595546648f, 0.0380525443366f,
  0.0405466983507f, 0.0431438043377f, 0.0458455957105f, 0.0486537485902f,
  0.0515698787635f, 0.0545955386770f, 0.0577322144744f, 0.0609813230826f,
  0.0643442093521f, 0.0678221432559f, 0.0714163171547f, 0.0751278431308f,
  0.0789577503983f, 0.0829069827919f, 0.0869763963425f, 0.0911667569411f,
  0.0954787380973f, 0.0999129187978f, 0.1044697814663f, 0.1091497100326f,
  0.1139529881123f, 0.1188797973021f, 0.1239302155952f, 0.1291042159182f,
  0.1344016647958f, 0.1398223211441f, 0.1453658351972f, 0.1510317475687f,
  0.1568194884519f, 0.1627283769610f, 0.1687576206144f, 0.1749063149635f,
  0.1811734433685f, 0.1875578769225f, 0.1940583745251f, 0.2006735831074f,
  0.2074020380087f, 0.2142421635060f, 0.2211922734957f, 0.2282505723294f,
  0.2354151558022f, 0.2426840122942f, 0.2500550240636f, 0.2575259686922f,
  0.2650945206802f, 0.2727582531908f, 0.2805146399424f, 0.2883610572461f,
  0.2962947861868f, 0.3043130149467f, 0.3124128412664f, 0.3205912750432f,
  0.3288452410620f, 0.3371715818563f, 0.3455670606954f, 0.3540283646950f,
  0.3625521080463f, 0.3711348353597f, 0.3797730251194f, 0.3884630932439f,
  0.3972013967476f, 0.4059842374987f, 0.4148078660690f, 0.4236684856688f,
  0.4325622561632f, 0.4414852981631f, 0.4504336971855f, 0.4594035078775f,
  0.4683907582974f, 0.4773914542473f, 0.4864015836507f, 0.4954171209690f,
  0.5044340316502f, 0.5134482766032f, 0.5224558166913f, 0.5314526172383f,
  0.5404346525404f, 0.5493979103767f, 0.5583383965124f, 0.5672521391870f,
  0.5761351935809f, 0.5849836462541f, 0.5937936195493f, 0.6025612759530f,
  0.6112828224084f, 0.6199545145721f, 0.6285726610089f, 0.6371336273176f,
  0.6456338401820f, 0.6540697913389f, 0.6624380414593f, 0.6707352239341f,
  0.6789580485595f, 0.6871033051160f, 0.6951678668346f, 0.7031486937450f,
  0.7110428359000f, 0.7188474364708f, 0.7265597347078f, 0.7341770687622f,
  0.7416968783634f, 0.7491167073478f, 0.7564342060337f, 0.7636471334405f,
  0.7707533593447f, 0.7777508661726f, 0.7846377507243f, 0.7914122257259f,
  0.7980726212081f, 0.8046173857074f, 0.8110450872888f, 0.8173544143867f,
  0.8235441764640f, 0.8296133044858f, 0.8355608512094f, 0.8413859912867f,
  0.8470880211823f, 0.8526663589033f, 0.8581205435445f, 0.8634502346477f,
  0.8686552113761f, 0.8737353715068f, 0.8786907302411f, 0.8835214188358f,
  0.8882276830576f, 0.8928098814640f, 0.8972684835131f, 0.9016040675058f,
  0.9058173183657f, 0.9099090252587f, 0.9138800790599f, 0.9177314696695f,
  0.9214642831859f, 0.9250796989404f, 0.9285789863994f, 0.9319635019416f,
  0.9352346855156f, 0.9383940571862f, 0.9414432135761f, 0.9443838242107f,
  0.9472176277742f, 0.9499464282852f, 0.9525720912005f, 0.9550965394548f,
  0.9575217494469f, 0.9598497469802f, 0.9620826031669f, 0.9642224303061f,
  0.9662713777450f, 0.9682316277320f, 0.9701053912729f, 0.9718949039987f,
  0.9736024220550f, 0.9752302180233f, 0.9767805768832f, 0.9782557920247f,
  0.9796581613210f, 0.9809899832703f, 0.9822535532154f, 0.9834511596505f,
  0.9845850806233f, 0.9856575802400f, 0.9866709052828f, 0.9876272819448f,
  0.9885289126912f, 0.9893779732526f, 0.9901766097570f, 0.9909269360049f,
  0.9916310308941f, 0.9922909359974f, 0.9929086532977f, 0.9934861430842f,
  0.9940253220114f, 0.9945280613238f, 0.9949961852476f, 0.9954314695504f,
  0.9958356402684f, 0.9962103726017f, 0.9965572899760f, 0.9968779632693f,
  0.9971739102015f, 0.9974465948832f, 0.9976974275221f, 0.9979277642810f,
  0.9981389072845f, 0.9983321047687f, 0.9985085513688f, 0.9986693885387f,
  0.9988157050969f, 0.9989485378907f, 0.9990688725745f, 0.9991776444921f,
  0.9992757396582f, 0.9993639958299f, 0.9994432036616f, 0.9995141079354f,
  0.9995774088586f, 0.9996337634217f, 0.9996837868077f, 0.9997280538466f,
  0.9997671005064f, 0.9998014254135f, 0.9998314913952f, 0.9998577270385f,
  0.9998805282556f, 0.9999002598527f, 0.9999172570940f, 0.9999318272557f,
  0.9999442511640f, 0.9999547847122f, 0.9999636603523f, 0.9999710885561f,
  0.9999772592415f, 0.9999823431613f, 0.9999864932503f, 0.9999898459282f,
  0.9999925223549f, 0.9999946296376f, 0.9999962619864f, 0.9999975018180f,
  0.9999984208056f, 0.9999990808746f, 0.9999995351446f, 0.9999998288155f
};

Ipp32f KBD_short[] = {
  0.0000437957029f, 0.0001186738427f, 0.0002307165764f, 0.0003894728276f,
  0.0006058127229f, 0.0008919969517f, 0.0012617254423f, 0.0017301724373f,
  0.0023140071937f, 0.0030313989666f, 0.0039020049736f, 0.0049469401816f,
  0.0061887279335f, 0.0076512306365f, 0.0093595599563f, 0.0113399662084f,
  0.0136197068917f, 0.0162268945863f, 0.0191903247173f, 0.0225392839760f,
  0.0263033404805f, 0.0305121170466f, 0.0351950492237f, 0.0403811300219f,
  0.0460986435187f, 0.0523748897687f, 0.0592359036608f, 0.0667061705563f,
  0.0748083417034f, 0.0835629525487f, 0.0929881471593f, 0.1030994120217f,
  0.1139093224941f, 0.1254273051615f, 0.1376594192678f, 0.1506081602865f,
  0.1642722885311f, 0.1786466855099f, 0.1937222404868f, 0.2094857694366f,
  0.2259199682674f, 0.2430034018413f, 0.2607105299507f, 0.2790117710137f,
  0.2978736038363f, 0.3172587073594f, 0.3371261378740f, 0.3574315427429f,
  0.3781274092336f, 0.3991633466320f, 0.4204863993919f, 0.4420413886774f,
  0.4637712792815f, 0.4856175685594f, 0.5075206937077f, 0.5294204534480f,
  0.5512564399468f, 0.5729684766207f, 0.5944970573441f, 0.6157837824951f,
  0.6367717872471f, 0.6574061575416f, 0.6776343292566f, 0.6974064662255f,
  0.7166758129495f, 0.7353990180935f, 0.7535364251490f, 0.7710523269961f,
  0.7879151814860f, 0.8040977856015f, 0.8195774062277f, 0.8343358660738f,
  0.8483595838269f, 0.8616395681829f, 0.8741713659841f, 0.8859549652852f,
  0.8969946547757f, 0.9072988415767f, 0.9168798300244f, 0.9257535646090f,
  0.9339393407778f, 0.9414594877966f, 0.9483390283040f, 0.9546053195628f,
  0.9602876817057f, 0.9654170184810f, 0.9700254361065f, 0.9741458658425f,
  0.9778116957797f, 0.9810564171039f, 0.9839132897549f, 0.9864150319317f,
  0.9885935373323f, 0.9904796233577f, 0.9921028127769f, 0.9934911505640f,
  0.9946710568026f, 0.9956672157342f, 0.9965025002283f, 0.9971979302082f,
  0.9977726628896f, 0.9982440121120f, 0.9986274935739f, 0.9989368924340f,
  0.9991843495262f, 0.9993804623416f, 0.9995343969636f, 0.9996540072843f,
  0.9997459580703f, 0.9998158487628f, 0.9998683352782f, 0.9999072474906f,
  0.9999357005160f, 0.9999561983594f, 0.9999707289065f, 0.9999808496399f,
  0.9999877638166f, 0.9999923871496f, 0.9999954052996f, 0.9999973226818f,
  0.9999985032505f, 0.9999992040241f, 0.9999996021706f, 0.9999998164955f,
  0.9999999241555f, 0.9999999733849f, 0.9999999929583f, 0.9999999990410f
};

Ipp32f KBD_short_ssr[] = {
  0.0000875914060f, 0.0009321760265f, 0.0032114611467f, 0.0081009893217f,
  0.0171240286619f, 0.0320720743528f, 0.0548307856029f, 0.0871361822565f,
  0.1302923415175f, 0.1848955425508f, 0.2506163195332f, 0.3260874142923f,
  0.4089316830907f, 0.4959414909424f, 0.5833939894959f, 0.6674601983218f,
  0.7446454751465f, 0.8121892962974f, 0.8683559394407f, 0.9125649996382f,
  0.9453396205810f, 0.9680864942678f, 0.9827581789763f, 0.9914756203467f,
  0.9961964092195f, 0.9984956609571f, 0.9994855586984f, 0.9998533730715f,
  0.9999671864476f, 0.9999948432454f, 0.9999995655238f, 0.9999999961639f
};

#else

#ifdef _MSVC_LANG
#pragma warning( disable: 4206 )
#endif

#endif //UMC_ENABLE_XXX
