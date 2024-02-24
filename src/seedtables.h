#ifndef SEEDTABLES_H
#define SEEDTABLES_H

#include <inttypes.h>

// Quad monument bases are too expensive to generate on the fly and there are
// so few of them that they can be hard coded, rather than loading from a file.
static const uint64_t g_qm_90[] = {
        35634735275ULL,
       775390004760ULL,
      3752034493314ULL,
      6745625093360ULL,
      8462966022953ULL,
      9735142779473ULL,
     10800310675623ULL,
     11692781183913ULL,
     15412128852232ULL,
     17507542501908ULL,
     20824655119255ULL,
     22102712328997ULL,
     23762468444321ULL,
     25706129919032ULL,
     30283004217073ULL,
     35236458175272ULL,
     36751575034122ULL,
     36982464340515ULL,
     40643008242473ULL,
     40847772584050ULL,
     42617143633137ULL,
     43070165094018ULL,
     45369104426317ULL,
     46388621658318ULL,
     49815561472240ULL,
     55209394201513ULL,
     60038078292777ULL,
     62013264258290ULL,
     64801907598210ULL,
     64967875136377ULL,
     65164413867914ULL,
     69458426726932ULL,
     69968838232145ULL,
     73925657823492ULL,
     75345282835555ULL,
     75897475957225ULL,
     75947399088753ULL,
     77139067906027ULL,
     80473750076038ULL,
     80869462923905ULL,
     85241165147001ULL,
     85336468993784ULL,
     85712034064849ULL,
     88230721056511ULL,
     89435905378306ULL,
     91999539429616ULL,
     96363295408208ULL,
     96666172090448ULL,
     97326737469569ULL,
    108818308997907ULL,
    110070523643664ULL,
    110929723321216ULL,
    113209246256383ULL,
    117558666299491ULL,
    121197818285311ULL,
    141209514904082ULL,
    141849622061865ULL,
    143737608964985ULL,
    152637010423035ULL,
    157050661340383ULL,
    170156314248098ULL,
    177801560427026ULL,
    183906223213433ULL,
    184103120915339ULL,
    185417015970809ULL,
    195760082985897ULL,
    197672678105184ULL,
    201305970722015ULL,
    206145729365528ULL,
    208212283032595ULL,
    210644041809166ULL,
    211691066673180ULL,
    211760287392362ULL,
    214621993657585ULL,
    215210467388591ULL,
    215223275916543ULL,
    218746505276081ULL,
    220178926586908ULL,
    220411725309680ULL,
    222407767385304ULL,
    222506989413161ULL,
    223366727763152ULL,
    226043537443714ULL,
    226089485745383ULL,
    226837069851090ULL,
    228023683672163ULL,
    230531739894864ULL,
    233072899009401ULL,
    233864998978601ULL,
    235857107438457ULL,
    236329873695639ULL,
    240806186862061ULL,
    241664450767537ULL,
    244715407566485ULL,
    248444978127746ULL,
    249746467672705ULL,
    252133692983502ULL,
    254891659986992ULL,
    256867224807089ULL,
    257374513735944ULL,
    257985210845650ULL,
    258999812908248ULL,
    260070455016529ULL,
    260286388529265ULL,
    261039958084216ULL,
    264768543640500ULL,
    265956699301296ULL,
    0
};

static const uint64_t g_qm_95[] = {
       775390004760ULL,
     40643008242473ULL,
     75345282835555ULL,
     85241165147001ULL,
    117558666299491ULL,
    141849622061865ULL,
    157050661340383ULL,
    184103120915339ULL,
    197672678105184ULL,
    201305970722015ULL,
    220178926586908ULL,
    206145729365528ULL,
    226043537443714ULL,
    143737608964985ULL,
    0
};


// returns for a >90% quadmonument the number of blocks, by area, in spawn range
__attribute__((const, used))
static int qmonumentQual(uint64_t s48)
{
    switch ((s48) & ((1LL<<48)-1))
    {
    case     35634735275ULL: return 12409;
    case    775390004760ULL: return 12796;
    case   3752034493314ULL: return 12583;
    case   6745625093360ULL: return 12470;
    case   8462966022953ULL: return 12190;
    case   9735142779473ULL: return 12234;
    case  10800310675623ULL: return 12443;
    case  11692781183913ULL: return 12748;
    case  15412128852232ULL: return 12463;
    case  17507542501908ULL: return 12272;
    case  20824655119255ULL: return 12470;
    case  22102712328997ULL: return 12227;
    case  23762468444321ULL: return 12165;
    case  25706129919032ULL: return 12163;
    case  30283004217073ULL: return 12236;
    case  35236458175272ULL: return 12338;
    case  36751575034122ULL: return 12459;
    case  36982464340515ULL: return 12499;
    case  40643008242473ULL: return 12983;
    case  40847772584050ULL: return 12296;
    case  42617143633137ULL: return 12234;
    case  43070165094018ULL: return 12627;
    case  45369104426317ULL: return 12190;
    case  46388621658318ULL: return 12299;
    case  49815561472240ULL: return 12296;
    case  55209394201513ULL: return 12632;
    case  60038078292777ULL: return 12227;
    case  62013264258290ULL: return 12470;
    case  64801907598210ULL: return 12780;
    case  64967875136377ULL: return 12376;
    case  65164413867914ULL: return 12125;
    case  69458426726932ULL: return 12610;
    case  69968838232145ULL: return 12236;
    case  73925657823492ULL: return 12168;
    case  75345282835555ULL: return 12836;
    case  75897475957225ULL: return 12343;
    case  75947399088753ULL: return 12234;
    case  77139067906027ULL: return 12155;
    case  80473750076038ULL: return 12155;
    case  80869462923905ULL: return 12165;
    case  85241165147001ULL: return 12799;
    case  85336468993784ULL: return 12651;
    case  85712034064849ULL: return 12212;
    case  88230721056511ULL: return 12499;
    case  89435905378306ULL: return 12115;
    case  91999539429616ULL: return 12253;
    case  96363295408208ULL: return 12253;
    case  96666172090448ULL: return 12470;
    case  97326737469569ULL: return 12165;
    case 108818308997907ULL: return 12609;
    case 110070523643664ULL: return 12758;
    case 110929723321216ULL: return 12780;
    case 113209246256383ULL: return 12499;
    case 117558666299491ULL: return 12948;
    case 121197818285311ULL: return 12499;
    case 141209514904082ULL: return 12583;
    case 141849622061865ULL: return 12983;
    case 143737608964985ULL: return 13188; // < best
    case 152637010423035ULL: return 12499;
    case 157050661340383ULL: return 12912;
    case 170156314248098ULL: return 12780;
    case 177801560427026ULL: return 12762;
    case 183906223213433ULL: return 12748;
    case 184103120915339ULL: return 12983;
    case 185417015970809ULL: return 12632;
    case 195760082985897ULL: return 12589;
    case 197672678105184ULL: return 12895;
    case 201305970722015ULL: return 12948;
    case 206145729365528ULL: return 12796;
    case 208212283032595ULL: return 12317;
    case 210644041809166ULL: return 12261;
    case 211691066673180ULL: return 12478;
    case 211760287392362ULL: return 12539;
    case 214621993657585ULL: return 12236;
    case 215210467388591ULL: return 12372;
    case 215223275916543ULL: return 12499;
    case 218746505276081ULL: return 12234;
    case 220178926586908ULL: return 12848;
    case 220411725309680ULL: return 12470;
    case 222407767385304ULL: return 12458;
    case 222506989413161ULL: return 12632;
    case 223366727763152ULL: return 12296;
    case 226043537443714ULL: return 13028;
    case 226089485745383ULL: return 12285;
    case 226837069851090ULL: return 12305;
    case 228023683672163ULL: return 12742;
    case 230531739894864ULL: return 12296;
    case 233072899009401ULL: return 12376;
    case 233864998978601ULL: return 12376;
    case 235857107438457ULL: return 12632;
    case 236329873695639ULL: return 12396;
    case 240806186862061ULL: return 12190;
    case 241664450767537ULL: return 12118;
    case 244715407566485ULL: return 12300;
    case 248444978127746ULL: return 12780;
    case 249746467672705ULL: return 12391;
    case 252133692983502ULL: return 12299;
    case 254891659986992ULL: return 12296;
    case 256867224807089ULL: return 12234;
    case 257374513735944ULL: return 12391;
    case 257985210845650ULL: return 12118;
    case 258999812908248ULL: return 12290;
    case 260070455016529ULL: return 12168;
    case 260286388529265ULL: return 12234;
    case 261039958084216ULL: return 12168;
    case 264768543640500ULL: return 12242;
    case 265956699301296ULL: return 12118;

    default: return 0;
    }
}

#endif // SEEDTABLES_H
