PCBNEW-LibModule-V1  09-04-2012 19:15:14
# encoding utf-8
$INDEX
BZR
G5LE
LM7805TO
$EndINDEX
$MODULE G5LE
Po 0 0 0 15 4F830EF6 00000000 ~~
Li G5LE
Sc 00000000
AR
Op 0 0 0
T0 0 2480 600 600 0 120 N V 21 N "G5LE"
T1 0 -2441 600 600 0 120 N V 21 N "VAL**"
DS -4134 -3268 -4134 3268 150 21
DS -4134 3268 4646 3268 150 21
DS 4646 3268 4646 -3268 150 21
DS 4646 -3268 -4134 -3268 150 21
$PAD
Sh "3" C 1181 1181 0 0 0
Dr 591 0 0
At STD N 00E0FFFF
Ne 0 ""
Po 2362 -2362
$EndPAD
$PAD
Sh "4" C 1181 1181 0 0 0
Dr 591 0 0
At STD N 00E0FFFF
Ne 0 ""
Po 2362 2362
$EndPAD
$PAD
Sh "1" C 1181 1181 0 0 0
Dr 591 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -3228 0
$EndPAD
$PAD
Sh "2" C 1181 1181 0 0 0
Dr 591 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -2441 -2362
$EndPAD
$PAD
Sh "5" C 1181 1181 0 0 0
Dr 591 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -2441 2362
$EndPAD
$EndMODULE  G5LE
$MODULE LM7805TO
Po 0 0 0 15 4F831236 00000000 ~~
Li LM7805TO
Cd LM7805 in TO92
Kw TR TO92
Sc 00000000
AR
Op 0 0 0
.SolderPasteRatio 9.42934e+297
T0 -500 1500 400 400 0 80 N V 21 N "LM7805TO"
T1 -500 -2000 400 400 0 80 N V 21 N "VAL**"
DS -500 1000 1000 -500 120 21
DS 1000 -500 1000 -1000 120 21
DS 1000 -1000 500 -1500 120 21
DS 500 -1500 -500 -1500 120 21
DS -500 -1500 -1500 -500 120 21
DS -1500 -500 -1500 500 120 21
DS -1500 500 -1000 1000 120 21
DS -1000 1000 -500 1000 120 21
$PAD
Sh "VI" C 550 550 0 0 0
Dr 320 0 0
At STD N 00E0FFFF
Ne 0 ""
Po 500 -500
$EndPAD
$PAD
Sh "GND" R 550 550 0 0 0
Dr 320 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -500 -500
$EndPAD
$PAD
Sh "VO" C 550 550 0 0 0
Dr 320 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -500 500
$EndPAD
$SHAPE3D
Na "discret/to98.wrl"
Sc 1.000000 1.000000 1.000000
Of 0.000000 0.000000 0.000000
Ro 0.000000 0.000000 0.000000
$EndSHAPE3D
$EndMODULE  LM7805TO
$MODULE BZR
Po 0 0 0 15 4F831914 00000000 ~~
Li BZR
Sc 00000000
AR 
Op 0 0 0
T0 0 3858 600 600 0 120 N V 21 N "BZR"
T1 0 -3307 600 600 0 120 N V 21 N "VAL**"
DC 0 0 2362 0 150 21
$PAD
Sh "1" R 550 550 0 0 0
Dr 320 0 0
At STD N 00E0FFFF
Ne 0 ""
Po 1181 0
$EndPAD
$PAD
Sh "2" C 550 550 0 0 0
Dr 320 0 0
At STD N 00E0FFFF
Ne 0 ""
Po -1181 0
$EndPAD
$EndMODULE  BZR
$EndLIBRARY
