EESchema Schematic File Version 2  date 23-04-2012 16:35:19
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:special
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
EELAYER 25  0
EELAYER END
$Descr A4 11700 8267
encoding utf-8
Sheet 1 1
Title "AISentinel"
Date "23 apr 2012"
Rev "0"
Comp "© MadsenSoft"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Wire Line
	5700 3600 5700 4250
Wire Wire Line
	5500 3600 5500 4050
Wire Wire Line
	6550 3700 6550 3850
Wire Wire Line
	6550 3850 5900 3850
Wire Wire Line
	5900 3850 5900 4650
Wire Wire Line
	6800 3550 6800 3200
Wire Wire Line
	5500 4950 5500 5900
Wire Wire Line
	6800 5300 6800 6000
Wire Wire Line
	3550 2800 3550 2700
Wire Wire Line
	2750 3200 4750 3200
Wire Wire Line
	2750 3200 2750 3000
Wire Wire Line
	2750 3000 2500 3000
Wire Wire Line
	5700 4250 5300 4250
Wire Wire Line
	5300 4150 5600 4150
Wire Wire Line
	5900 4650 5300 4650
Wire Wire Line
	8350 4050 8350 3950
Wire Wire Line
	7550 4250 6100 4250
Wire Wire Line
	5300 4450 7550 4450
Wire Wire Line
	4750 2700 4750 2800
Wire Wire Line
	5300 5150 6100 5150
Wire Wire Line
	6100 5150 6100 4250
Connection ~ 8350 4350
Wire Wire Line
	5500 4950 5300 4950
Connection ~ 6400 5300
Wire Wire Line
	5500 4050 5300 4050
Connection ~ 4150 3200
Wire Wire Line
	4150 3100 4150 3200
Connection ~ 3550 2800
Wire Wire Line
	2500 2800 3750 2800
Wire Wire Line
	4750 3200 4750 3300
Wire Wire Line
	3100 3900 3100 3950
Wire Wire Line
	3100 3950 3200 3950
Connection ~ 4750 3200
Connection ~ 3550 3200
Wire Wire Line
	4750 2800 4550 2800
Connection ~ 4750 2800
Wire Wire Line
	3200 5150 3100 5150
Wire Wire Line
	3100 5150 3100 5250
Wire Wire Line
	5300 3950 5400 3950
Connection ~ 8350 4250
Wire Wire Line
	5300 4550 6400 4550
Wire Wire Line
	6400 4550 6400 4050
Wire Wire Line
	6400 4050 7550 4050
Wire Wire Line
	8350 4600 8350 4150
Connection ~ 8350 4450
Connection ~ 6800 5900
Wire Wire Line
	7550 4350 5300 4350
Connection ~ 3200 3200
Connection ~ 3200 2800
Wire Wire Line
	5500 5900 6400 5900
Wire Wire Line
	6400 5300 5600 5300
Wire Wire Line
	5600 5300 5600 4850
Wire Wire Line
	5600 4850 5300 4850
Wire Wire Line
	6550 3200 6550 3300
Wire Wire Line
	5300 5050 6000 5050
Wire Wire Line
	6000 5050 6000 3950
Wire Wire Line
	6000 3950 6800 3950
Wire Wire Line
	5400 3950 5400 3600
Wire Wire Line
	5600 4150 5600 3600
$Comp
L CONN_4 P2
U 1 1 4F956674
P 5550 3250
F 0 "P2" V 5500 3250 50  0000 C CNN
F 1 "CONN_4" V 5600 3250 50  0000 C CNN
	1    5550 3250
	0    -1   -1   0   
$EndComp
$Comp
L ATTINY24-P IC1
U 1 1 4F800B0F
P 4250 4550
F 0 "IC1" H 3500 5300 60  0000 C CNN
F 1 "ATTINY24-P" H 4800 3800 60  0000 C CNN
F 2 "DIP14" H 3550 3800 60  0001 C CNN
	1    4250 4550
	1    0    0    -1  
$EndComp
$Comp
L PWR_FLAG #FLG01
U 1 1 4F82C1DF
P 3200 3200
F 0 "#FLG01" H 3200 3295 30  0001 C CNN
F 1 "PWR_FLAG" H 3200 3380 30  0000 C CNN
	1    3200 3200
	1    0    0    -1  
$EndComp
$Comp
L PWR_FLAG #FLG02
U 1 1 4F82C08F
P 3200 2800
F 0 "#FLG02" H 3200 2895 30  0001 C CNN
F 1 "PWR_FLAG" H 3200 2980 30  0000 C CNN
	1    3200 2800
	1    0    0    -1  
$EndComp
$Comp
L +12V #PWR03
U 1 1 4F82BE17
P 3550 2700
F 0 "#PWR03" H 3550 2650 20  0001 C CNN
F 1 "+12V" H 3550 2800 30  0000 C CNN
	1    3550 2700
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR04
U 1 1 4F82BDF8
P 4750 3300
F 0 "#PWR04" H 4750 3300 30  0001 C CNN
F 1 "GND" H 4750 3230 30  0001 C CNN
	1    4750 3300
	1    0    0    -1  
$EndComp
$Comp
L CONN_2 P1
U 1 1 4F7FFBF7
P 2150 2900
F 0 "P1" V 2100 2900 40  0000 C CNN
F 1 "12 V" V 2200 2900 40  0000 C CNN
	1    2150 2900
	-1   0    0    -1  
$EndComp
Text Label 7250 4450 0    60   ~ 0
MISO
Text Label 7300 4350 0    60   ~ 0
SCK
Text Label 7050 4250 0    60   ~ 0
/RST dW
Text Label 7250 4050 0    60   ~ 0
MOSI
NoConn ~ 7550 4150
$Comp
L VCC #PWR05
U 1 1 4F801688
P 6800 2700
F 0 "#PWR05" H 6800 2800 30  0001 C CNN
F 1 "VCC" H 6800 2800 30  0000 C CNN
	1    6800 2700
	1    0    0    -1  
$EndComp
$Comp
L VCC #PWR06
U 1 1 4F801680
P 6550 2700
F 0 "#PWR06" H 6550 2800 30  0001 C CNN
F 1 "VCC" H 6550 2800 30  0000 C CNN
	1    6550 2700
	1    0    0    -1  
$EndComp
$Comp
L R R2
U 1 1 4F801677
P 6800 2950
F 0 "R2" V 6880 2950 50  0000 C CNN
F 1 "470" V 6800 2950 50  0000 C CNN
	1    6800 2950
	1    0    0    -1  
$EndComp
$Comp
L R R1
U 1 1 4F801655
P 6550 2950
F 0 "R1" V 6630 2950 50  0000 C CNN
F 1 "470" V 6550 2950 50  0000 C CNN
	1    6550 2950
	1    0    0    -1  
$EndComp
$Comp
L LED D2
U 1 1 4F8015EA
P 6800 3750
F 0 "D2" H 6800 3850 50  0000 C CNN
F 1 "ALARM" H 6800 3650 50  0000 C CNN
	1    6800 3750
	0    1    1    0   
$EndComp
$Comp
L LED D1
U 1 1 4F8015C8
P 6550 3500
F 0 "D1" H 6550 3600 50  0000 C CNN
F 1 "ACT" H 6550 3400 50  0000 C CNN
	1    6550 3500
	0    1    1    0   
$EndComp
$Comp
L VCC #PWR07
U 1 1 4F801434
P 8350 3950
F 0 "#PWR07" H 8350 4050 30  0001 C CNN
F 1 "VCC" H 8350 4050 30  0000 C CNN
	1    8350 3950
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR08
U 1 1 4F801218
P 8350 4600
F 0 "#PWR08" H 8350 4600 30  0001 C CNN
F 1 "GND" H 8350 4530 30  0001 C CNN
	1    8350 4600
	1    0    0    -1  
$EndComp
$Comp
L CONN_5X2 P3
U 1 1 4F8011FD
P 7950 4250
F 0 "P3" H 7950 4550 60  0000 C CNN
F 1 "ISP / dW" V 7950 4250 50  0000 C CNN
	1    7950 4250
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR09
U 1 1 4F800F47
P 6800 6000
F 0 "#PWR09" H 6800 6000 30  0001 C CNN
F 1 "GND" H 6800 5930 30  0001 C CNN
	1    6800 6000
	1    0    0    -1  
$EndComp
$Comp
L C C4
U 1 1 4F800EFF
P 6600 5900
F 0 "C4" H 6650 6000 50  0000 L CNN
F 1 "22 pF" H 6650 5800 50  0000 L CNN
	1    6600 5900
	0    -1   -1   0   
$EndComp
$Comp
L C C3
U 1 1 4F800ED9
P 6600 5300
F 0 "C3" H 6650 5400 50  0000 L CNN
F 1 "22 pF" H 6650 5200 50  0000 L CNN
	1    6600 5300
	0    -1   -1   0   
$EndComp
$Comp
L CRYSTAL X1
U 1 1 4F800E6D
P 6400 5600
F 0 "X1" H 6400 5750 60  0000 C CNN
F 1 "12 MHz" H 6400 5450 60  0000 C CNN
	1    6400 5600
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR010
U 1 1 4F7F0E4B
P 3100 5250
F 0 "#PWR010" H 3100 5250 30  0001 C CNN
F 1 "GND" H 3100 5180 30  0001 C CNN
	1    3100 5250
	1    0    0    -1  
$EndComp
$Comp
L VCC #PWR011
U 1 1 4F7F0E2B
P 3100 3900
F 0 "#PWR011" H 3100 4000 30  0001 C CNN
F 1 "VCC" H 3100 4000 30  0000 C CNN
	1    3100 3900
	1    0    0    -1  
$EndComp
$Comp
L VCC #PWR012
U 1 1 4F7F0E20
P 4750 2700
F 0 "#PWR012" H 4750 2800 30  0001 C CNN
F 1 "VCC" H 4750 2800 30  0000 C CNN
	1    4750 2700
	1    0    0    -1  
$EndComp
$Comp
L CAPAPOL C2
U 1 1 4F7F0CB0
P 4750 3000
F 0 "C2" H 4800 3100 50  0000 L CNN
F 1 "100 uF" H 4800 2900 50  0000 L CNN
	1    4750 3000
	1    0    0    -1  
$EndComp
$Comp
L LM7805 U1
U 1 1 4F7F0C97
P 4150 2850
F 0 "U1" H 4300 2654 60  0000 C CNN
F 1 "LM7805" H 4150 3050 60  0000 C CNN
	1    4150 2850
	1    0    0    -1  
$EndComp
$Comp
L CAPAPOL C1
U 1 1 4F7E1001
P 3550 3000
F 0 "C1" H 3600 3100 50  0000 L CNN
F 1 "100 uF" H 3600 2900 50  0000 L CNN
	1    3550 3000
	1    0    0    -1  
$EndComp
$EndSCHEMATC
