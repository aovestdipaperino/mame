// license:CC0-1.0
// copyright-holders: Enzo Lombardi

// Netlist for Model Racing Dribbling: derived from the schematics in the Dribbling manual, located here:
// https://archive.org/details/ArcadeGameManualDribbling
// Known issues:
// In the PARATA netlist the JFET emulation based on MOSFET doesn't work well. The whole netlist has been
// replaced with a quite similar one, at least to my ears. The real netlist is commented behind the FAKE_PARATA
// define.

#include "netlist/devices/net_lib.h"

#define TTL_74LS86_DIP TTL_7486_DIP
#define TTL_74LS107_DIP TTL_74107_DIP
#define TTL_74LS161_DIP TTL_74161_DIP
#define TTL_74LS164_DIP TTL_74164_DIP
#define ATTENUATE_FOLLA 200
#define USE_SIMPLIFIED_LM339
#define USE_FAKE_PARATA

// The schematic incorrectly labels two JFETS in the PARATA circuit as 2N3812.  They're actually 2N3819.
// JFET transistors not supported, but this should do the trick; but not for this game.
#define Q_2N3819(name) MOSFET(name, "NMOS(VTO=-3 KP=0.001 CAPMOD=0)")
// Setting KP slows down the simulation. Not required for this JFET.
#define Q_2N3819B(name) MOSFET(name, "NMOS(VTO=-3)")

#ifdef USE_SIMPLIFIED_LM339
// Simplified LM339 model - uses high-level simulation of differential input stage.
static NETLIST_START(LM339)
{
	// * CONNECTIONS:   NON-INVERTING INPUT
	// *                | INVERTING INPUT
	// *                | | POSITIVE POWER SUPPLY
	// *                | | | NEGATIVE POWER SUPPLY
	// *                | | | | OPEN COLLECTOR OUTPUT
	// *                | | | | |
	// *                1 2 3 4 5
	NET_MODEL("LM339_QO NPN(BF=200)")

	AFUNC(CMP, 2, "min(max(A1 - A0, 0), 1e-6)")
	VCCS(QB, 80)
	QBJT_EB(QO, "LM339_QO")
	ANALOG_INPUT(XGND, 0)
	RES(RDUMMY, RES_M(1000))

	NET_C(CMP.Q, QB.IP)
	NET_C(QB.OP, QO.B)
	NET_C(XGND, QB.IN)
	NET_C(QB.ON, QO.E, RDUMMY.2)

	ALIAS(1, CMP.A0)
	ALIAS(2, CMP.A1)
	ALIAS(3, RDUMMY.1)
	ALIAS(4, QO.E)
	ALIAS(5, QO.C)
}
#else // USE_SIMPLIFIED_LM339
// LM339 model adapted from here: https://github.com/evenator/LTSpice-Libraries/blob/master/sub/LM339.sub
// It's too slow for the circuits emulated in this netlist, but might be useful in the future.
static NETLIST_START(LM339)
{
	// * CONNECTIONS:   NON-INVERTING INPUT
	// *                | INVERTING INPUT
	// *                | | POSITIVE POWER SUPPLY
	// *                | | | NEGATIVE POWER SUPPLY
	// *                | | | | OPEN COLLECTOR OUTPUT
	// *                | | | | |
	// *                1 2 3 4 5
	NET_MODEL("LM339A_TI_QIN PNP(IS=800.0E-18 BF=2.000E3)")
	NET_MODEL("LM339A_TI_QMI NPN(IS=800.0E-18 BF=1002)")
	NET_MODEL("LM339A_TI_QMO NPN(IS=800.0E-18 BF=1000 CJC=1E-15 TR=807.4E-9)")
	NET_MODEL("LM339A_TI_QOC NPN(IS=800.0E-18 BF=20.29E3 CJC=1E-15 TF=942.6E-12 TR=543.8E-9)")
	NET_MODEL("LM339A_TI_DX D(IS=800.0E-18)")
	ALIAS(1, VI1.2)
	ALIAS(2, VI2.2)
	ALIAS(3, F10.ON)
	ALIAS(4, Q3.E)
	ALIAS(5, Q5.C)
	DIODE(DP, "LM339A_TI_DX")
	VCVS(E1, 1)
	CCCS(F10, 1)
	CS(IEE, 0.0001)
	QBJT_EB(Q1, "LM339A_TI_QIN")
	QBJT_EB(Q2, "LM339A_TI_QIN")
	QBJT_EB(Q3, "LM339A_TI_QMO")
	QBJT_EB(Q4, "LM339A_TI_QMI")
	QBJT_EB(Q5, "LM339A_TI_QOC")
	RES(RP, 46.300000)
	VS(V1, 0)
	VS(VI1, 0.75)
	VS(VI2, 0.75)
	NET_C(E1.OP, F10.IP)
	NET_C(Q2.C, Q3.B, Q4.C, Q4.B)
	NET_C(VI1.1, Q1.B)
	NET_C(VI2.1, Q2.B)
	NET_C(IEE.2, Q1.E, Q2.E)
	NET_C(F10.IN, V1.1)
	NET_C(F10.ON, IEE.1, DP.K, RP.1)
	NET_C(F10.OP, Q1.C, Q3.C, E1.IP)
	NET_C(V1.2, Q5.B)
	NET_C(Q3.E, Q4.E, E1.ON, E1.IN, Q5.E, DP.A, RP.2)
}
#endif // USE_SIMPLIFIED_LM339

// CALCIO_A and CALCIO_B differ only for the parameters of 3 capacitors.
static NETLIST_START(calcio)
{
	DEFPARAM(CAP1, 33)
	DEFPARAM(CAP2, 68)
	ANALOG_INPUT(I_V5, 5)
	ANALOG_INPUT(I_V6, 6)
	ANALOG_INPUT(I_V12, 12)
	QBJT_EB(Q8, "BC239C")
	QBJT_EB(Q9, "BC239C")
	ALIAS(INPUT, R45.2)
	RES(R45, 420)
	RES(R46, RES_K(8.2))
	RES(R48, RES_K(220))
	RES(R49, RES_K(56))
	RES(R50, RES_K(56))
	RES(R51, RES_M(1))
	RES(R52, RES_K(68))
	RES(R53, RES_K(82))
	RES(R54, RES_K(1.9))  // WAS POT
	RES(R55, RES_K(10))
	RES(R56, RES_K(100))
	RES(R57, RES_K(10))
	RES(R58, RES_K(3.9))
	CAP(C29, CAP_U(4.2))
	CAP(C30, CAP_N($(@.CAP2)))
	CAP(C31, CAP_N(150))
	CAP(C32, CAP_U(4.7))
	CAP(C33, CAP_N($(@.CAP1)))
	CAP(C34, CAP_N($(@.CAP1)))
	NET_C(INPUT, R46.1)
	ALIAS(GND, R45.1)
	NET_C(R46.2, C33.2, C34.1)
	NET_C(C33.1, R49.1, C29.1)
	NET_C(C34.2, R50.2, R51.1, Q8.B)
	NET_C(R49.2, C30.2, R50.1)
	NET_C(C30.1, GND)
	NET_C(C29.2, R52.1, R51.2, R53.1, Q8.C)
	NET_C(R52.2, I_V5)
	NET_C(Q8.E, R54.2)
	NET_C(R54.1, GND)
	NET_C(R53.2, C31.1)
	NET_C(C31.2, R48.1, R56.2, Q9.B)
	NET_C(R48.2, I_V5)
	NET_C(R56.1, GND)
	NET_C(Q9.C, R55.1)
	ALIAS(OUTPUT, Q9.C)
	NET_C(R55.2, I_V5)
	NET_C(Q9.E, R57.2, C32.2)
	NET_C(R57.1, GND)
	NET_C(C32.1, R58.2)
	NET_C(R58.1, GND)
}

static NETLIST_START(stop_palla)
{
	ANALOG_INPUT(I_V5, 5)
	QBJT_EB(Q1, "BC309")
	RES(R5, RES_K(1))
	RES(R6, RES_K(160))
	RES(R7, RES_K(150))
	RES(R8, RES_K(10))
	RES(R9, RES_K(2.2))
	CAP(C5, CAP_U(1))
	CAP(C6, CAP_U(1))
	DIODE(D1, "1N914")
	SUBMODEL(LM339, IC_B9)
	ALIAS(GND, IC_B9.4)
	ALIAS(INPUT, R5.1)
	NET_C(R5.2, Q1.B)
	NET_C(Q1.E, I_V5)
	NET_C(Q1.C, C5.2, R7.2, R6.1)
	NET_C(C5.1, GND)
	NET_C(R7.1, GND)
	NET_C(R6.2, IC_B9.1, D1.A)
	NET_C(D1.K, R9.1, IC_B9.5, R8.2)
	NET_C(IC_B9.2, C6.2, R8.1)
	NET_C(C6.1, GND)
	NET_C(R9.2, I_V5)
	NET_C(IC_B9.3, I_V5)

	ALIAS(OUTPUT, IC_B9.5)
}

// The actual PARATA schematics requires JFETs (2N3819) and using 'MOSFET(Q21, "NMOS(VTO=-1.0)")' as proposed in the FAQ
// doesn't work. So using the same circuit STOP_PALLA with different values to emulate the sound which has a higher pitch.

static NETLIST_START(parata)
{
	ANALOG_INPUT(I_V5, 5)

	QBJT_EB(Q1, "BC309")
	Q_2N3819(Q2)
	Q_2N3819B(Q3)
	RES(R5, RES_K(1))
	RES(R6, RES_K(150))
	RES(R7, RES_K(220))
	RES(R9, RES_K(2.2))
	CAP(C5, CAP_U(2.2))
	CAP(C6, CAP_U(1))
	DIODE(D1, "1N914")
	DIODE(D2, "1N914")
	DIODE(D3, "1N914")

	SUBMODEL(LM339, IC_B9)
	ALIAS(GND, IC_B9.4)
	ALIAS(INPUT, R5.1)
	NET_C(R5.2, Q1.B)
	NET_C(Q1.E, I_V5)
	NET_C(Q1.C, C5.2, R7.2, R6.1)
	NET_C(C5.1, GND)
	NET_C(R7.1, GND)
	NET_C(R6.2, IC_B9.1, D1.A)
	NET_C(D1.K, R9.1, IC_B9.5, Q2.G, Q2.S)
	NET_C(IC_B9.2, C6.2, Q2.D)
	NET_C(C6.1, GND)
	NET_C(R9.2, I_V5)
	NET_C(IC_B9.3, I_V5)
	NET_C(D2.A, Q2.G)
	NET_C(D2.K, Q2.S)

	NET_C(Q2.D, Q3.G)
	NET_C(Q3.D, I_V5)
	NET_C(D3.A, Q3.G)
	NET_C(D3.K, Q3.S)	
	ALIAS(OUTPUT, Q3.S)
	OPTIMIZE_FRONTIER(Q3.S, RES_K(100), 1)
}

// Sallen-Key approximation of a third-order Butterworth filter with 15KHz cutoff frequency.
// Values computed using http://sim.okawa-denshi.jp/en/Sallen3tool.php .
// This is because the tone generator outputs a 40KHz square wave at idle, and this is to avoid aliasing when outputing at 48KHz.
static NETLIST_START(output_filter)
{
	OPAMP(AMP, "OPAMP(TYPE=1 FPF=5 RI=1M RO=50 UGF=1M SLEW=1M VLH=0.5 VLL=0.03 DAB=0.0015)")
	RES(R1, RES_K(11))
	RES(R2, RES_K(110))
	RES(R3, RES_K(33))
	CAP(C1, CAP_U(0.001))
	CAP(C2, CAP_P(470))
	CAP(C3, CAP_P(68))
	ANALOG_INPUT(VPLUS, 12)
	ANALOG_INPUT(VMINUS, -12)
	NET_C(VPLUS, AMP.VCC)
	NET_C(VMINUS, AMP.GND)
	ALIAS(INPUT, R1.1)
	ALIAS(OUTPUT, AMP.OUT)
	ALIAS(GND, C1.2)

	NET_C(GND, C3.2)
	NET_C(R1.2, R2.1, C1.1)
	NET_C(R2.2, R3.1, C2.1)
	NET_C(R3.2, C3.1, AMP.PLUS)
	NET_C(OUTPUT, C2.2, AMP.MINUS)
}

NETLIST_START(dribling)
{
	NET_MODEL("BC239C NPN(IS=1.5813E-15 ISE=4.637E-14 ISC=8.0864E-18 XTI=3 BF=113.32 BR=86.718 IKF=1.4907 IKR=0.03360 XTB=0 VAF=12.331 VAR=31.901 VJE=0.71518 VJC=1.1381 RE=0.22081 RC=0.01636 RB=1.0078 CJE=3.3904E-14 CJC=2.9774E-12 XCJC=0.02899 FC=0.99886 NF=1.0653 NR=1.8047 NE=1.4254 NC=1.8821 MJE=0.36824 MJC=0.31461  TF=2.0691E-11 TR=1.0033E-09 EG=1.11 IRB=0.00083992 RBM=0 XTF=0.31338 VTF=0.10174 ITF=0.0045579 PTF=0 CJS=0 MJS=0 VJS=0.75)") // )")
	NET_MODEL("BC309 PNP(IS=1.0366E-15 ISE=1.6295E-14 ISC=9.4971E-17 XTI=3 BF=80 BR=16.116 IKF=0.47497 IKR=0.012081 XTB=0 VAF=8.486 VAR=9.5149 VJE=0.84456 VJC=1.0282 RE=1.9597 RC=1.1393 RB=3.2133 CJE=2.0636E-14 CJC=2.6904E-12 XCJC=0.075977 FC=0.54298 NF=1.03136 NR=1.2907 NE=1.3702 NC=1.038 MJE=0.68352 MJC=0.5401 TF=5.4303E-11 TR=3.4233E-10 EG=1.11 IRB=0.00046855 RBM=2.0822 XTF=0.27447 VTF=0.19311 ITF=0.0030573 PTF=0 CJS=0 VJS=0.75 MJS=0)")

	SOLVER(Solver, 1000)
	PARAM(Solver.DYNAMIC_TS, 1)
	PARAM(Solver.DYNAMIC_MIN_TIMESTEP, 2e-5)
	PARAM(Solver.NR_LOOPS, 300)
	PARAM(Solver.GS_LOOPS, 10)
	PARAM(Solver.PARALLEL, 2) // More does not help

	CLOCK(clk, 40000) // 40KHz
	ANALOG_INPUT(I_V5, 5)
	ANALOG_INPUT(I_V6, 6)
	ANALOG_INPUT(I_V12, 12)

	TTL_7414_DIP(IC_L9) // SHARED INVERTER, FISCHIO 1-2, TOS 3-9
	NC_PIN(NC)
	NET_C(IC_L9.10, GND.Q)
	NET_C(IC_L9.12, GND.Q)

	LOCAL_SOURCE(LM339)
	LOCAL_SOURCE(calcio)
	LOCAL_SOURCE(stop_palla)
	LOCAL_SOURCE(parata)
	LOCAL_SOURCE(output_filter)

	// TOS Section
	TTL_INPUT(I_PB0, 0)
	TTL_INPUT(I_PB1, 0)
	TTL_INPUT(I_PB2, 0)
	TTL_INPUT(I_PB3, 0)
	TTL_INPUT(I_PB4, 0)
	TTL_INPUT(I_PB5, 0)
	TTL_INPUT(I_PB6, 0)
	TTL_INPUT(I_PB7, 0)
	NET_C(GND, I_PB0.GND, I_PB1.GND, I_PB2.GND, I_PB3.GND, I_PB4.GND, I_PB5.GND, I_PB6.GND, I_PB7.GND, clk.GND)
	NET_C(I_V5, I_PB0.VCC, I_PB1.VCC, I_PB2.VCC, I_PB3.VCC, I_PB4.VCC, I_PB5.VCC, I_PB6.VCC, I_PB7.VCC, clk.VCC)
	CAP(C7_1, CAP_P(330))
	CAP(C7_2, CAP_P(330))
	TTL_74161_DIP(IC_D7)  // 4 bit counter
	TTL_74161_DIP(IC_E7)  // same
	TTL_74LS107_DIP(IC_C7)  // dual JK flip flop
	NET_C(GND, IC_C7.7, IC_D7.8, IC_E7.8, C7_1.2)
	NET_C(I_V5, IC_C7.14, IC_D7.16, IC_E7.16)
	NET_C(I_PB0, IC_D7.3)
	NET_C(I_PB1, IC_D7.4)
	NET_C(I_PB2, IC_D7.5)
	NET_C(I_PB3, IC_D7.6)
	NET_C(I_PB4, IC_E7.3)
	NET_C(I_PB5, IC_E7.4)
	NET_C(I_PB6, IC_E7.5)
	NET_C(I_PB7, IC_E7.6)
	NET_C(IC_E7.1, IC_D7.1, I_V5)  // CLEAR FLAG
	NET_C(IC_D7.7, IC_D7.10, I_V5)
	NET_C(IC_E7.9, IC_D7.9, C7_1.1, IC_L9.4)
	NET_C(IC_D7.15, IC_E7.7, IC_E7.10)
	NET_C(IC_E7.15, IC_L9.3)
	NET_C(IC_L9.6, IC_E7.2, IC_D7.2)
	NET_C(IC_L9.5, C7_2.1, IC_C7.9, IC_L9.8)
	NET_C(GND, C7_2.2)
	NET_C(IC_L9.9, clk)
	NET_C(IC_C7.10, I_V5)
	NET_C(IC_C7.1, IC_C7.13, IC_C7.4, IC_C7.12, GND.Q)
	NET_C(IC_C7.8, IC_E7.15) // FLIP-FLOP
	NET_C(IC_C7.11, IC_E7.15)
	ALIAS(TOS, IC_C7.5)

	// FISCHIO
	TTL_INPUT(I_FISCHIO, 0)
	NET_C(GND, I_FISCHIO.GND)
	NET_C(I_V5, I_FISCHIO.VCC)
	SUBMODEL(NE556_DIP, IC_N9)
	TTL_74393_DIP(IC_M9)
	NET_C(IC_L9.7, GND)
	NET_C(IC_L9.14, I_V5)
	NET_C(IC_M9.7, GND)
	NET_C(IC_M9.14, I_V5)
	NET_C(IC_N9.7, GND)
	NET_C(IC_N9.14, I_V5)
	RES(R1, RES_K(6.8))
	RES(R2, RES_K(39))
	RES(R3, RES_K(100))
	RES(R4, RES_K(47))
	CAP(C42, CAP_U(4.2))
	CAP(C2, CAP_N(2.2))
	NET_C(IC_M9.6, IC_L9.1)
	NET_C(IC_N9.4, IC_L9.2)
	NET_C(IC_N9.10, IC_L9.2)
	NET_C(I_V5, R1.1)
	NET_C(I_V5, R2.1)
	NET_C(R1.2, IC_N9.1, R3.1, IC_N9.2, IC_N9.6, C42.1)
	NET_C(C42.2, GND)
	NET_C(R3.2, R4.2, IC_N9.8, IC_N9.12, C2.1)
	NET_C(R4.1, R2.2, IC_N9.13)
	NET_C(C2.2, GND)
	NET_C(IC_N9.5, IC_M9.1) // Counter tick
	NET_C(I_FISCHIO, IC_M9.2) // Counter CLEAR
	NET_C(I_FISCHIO, IC_M9.12)
	NET_C(GND.Q, IC_L9.11, IC_L9.13 )
	NET_C(IC_M9.6, IC_M9.13)
	HINT(IC_M9.3, NC)
	HINT(IC_M9.4, NC)
	HINT(IC_M9.5, NC)
	HINT(IC_M9.8, NC)
	HINT(IC_M9.9, NC)
	HINT(IC_M9.10, NC)
	HINT(IC_M9.11, NC)
	ALIAS(FISCHIO, IC_N9.9)

	// STOP_PALLA
	TTL_INPUT(I_STOP_PALLA, 0)
	NET_C(GND, I_STOP_PALLA.GND)
	NET_C(I_V6, I_STOP_PALLA.VCC)
	SUBMODEL(stop_palla, STOP_PALLA)
	NET_C(STOP_PALLA.GND, GND)
	NET_C(STOP_PALLA.INPUT, I_STOP_PALLA)

	// PARATA
	TTL_INPUT(I_PARATA, 0)
	NET_C(GND, I_PARATA.GND)
	NET_C(I_V6, I_PARATA.VCC)
	SUBMODEL(parata, PARATA)
	NET_C(PARATA.GND, GND)
	NET_C(PARATA.INPUT, I_PARATA)

	// CALCIO_A and CALCIO_B
	TTL_INPUT(I_CALCIO_A, 0)
	NET_C(GND, I_CALCIO_A.GND)
	NET_C(I_V5, I_CALCIO_A.VCC)
	TTL_INPUT(I_CALCIO_B, 0)
	NET_C(GND, I_CALCIO_B.GND)
	NET_C(I_V5, I_CALCIO_B.VCC)
	SUBMODEL(calcio, CALCIO_A)
	PARAM(CALCIO_A.CAP1, 22)
	PARAM(CALCIO_A.CAP1, 47)
	NET_C(I_CALCIO_A, CALCIO_A.INPUT)
	NET_C(CALCIO_A.GND, GND)
	SUBMODEL(calcio, CALCIO_B)
	PARAM(CALCIO_B.CAP1, 33)
	PARAM(CALCIO_B.CAP1, 68)
	NET_C(I_CALCIO_B, CALCIO_B.INPUT)
	NET_C(CALCIO_B.GND, GND)

	// FOLLA A, M, B
	TTL_INPUT(I_FOLLA_A, 0)
	TTL_INPUT(I_FOLLA_M, 0)
	TTL_INPUT(I_FOLLA_B, 0)
	TTL_INPUT(I_CONTRASTO, 0)
	NET_C(GND, I_FOLLA_A.GND, I_FOLLA_M.GND, I_FOLLA_B.GND, I_CONTRASTO.GND)
	NET_C(I_V5, I_FOLLA_A.VCC, I_FOLLA_M.VCC, I_FOLLA_B.VCC, I_CONTRASTO.VCC)
	NET_C(GND, IC_E8.5, IC_C8.9, IC_C8.6, IC_C8.2, IC_C8.3, IC_8B.12, IC_8B.5, IC_8B.9, IC_7B.12, IC_7B.10, IC_7B.2, IC_7B.8)
	NET_C(GND, IC_8B.4, IC_C8.5, IC_7B.8, IC_8B.10, IC_8B.13, IC_C8.10, IC_E8.4)
	TTL_74LS164_DIP(IC_7A)
	TTL_7414_DIP(IC_7B)
	TTL_74LS164_DIP(IC_8A)
	TTL_74LS86_DIP(IC_8B)
	LM324_DIP(IC_C8)
	TTL_7408_DIP(IC_E8)
	NET_C(I_V5,  IC_7A.14, IC_7B.14, IC_8A.14, IC_8B.14, IC_E8.14)
	NET_C(I_V12, IC_C8.4)
	NET_C(GND,  IC_7A.7, IC_7B.7, IC_8A.7, IC_8B.7, IC_E8.7, IC_C8.11)
	RES(R32, RES_K(1))
	RES(R33, RES_K(220 + ATTENUATE_FOLLA))
	RES(R34, RES_K(100 + ATTENUATE_FOLLA))
	RES(R35, RES_K(1))
	RES(R36, RES_K(56 + ATTENUATE_FOLLA))
	RES(R37, RES_K(10))
	RES(R47, RES_K(330))
	CAP(C20, CAP_N(100))
	CAP(C21, CAP_N(10))
	CAP(C22, CAP_N(22))
	CAP(C24, CAP_U(10))
	NET_C(IC_8B.1, IC_7B.4)
	NET_C(IC_8B.2, IC_8A.3)
	NET_C(IC_8B.3, IC_8A.1, IC_8A.2)
	NET_C(IC_8A.9, IC_7A.9, I_V5)
	NET_C(IC_8A.8, IC_7A.8, R32.1, IC_7B.6)
	NET_C(IC_8A.13, IC_7A.1, IC_7A.2)
	NET_C(IC_7A.12, IC_7B.3)
	NET_C(R32.2, IC_7B.5, C20.2)
	NET_C(C20.1, GND)
	ALIAS(NOISE, IC_7A.13)
	ALIAS(C_IN, IC_8A.8)
	NET_C(I_FOLLA_B, IC_E8.10)
	NET_C(I_FOLLA_M, IC_E8.12)
	NET_C(I_FOLLA_A, IC_E8.1)
	NET_C(IC_E8.9, IC_E8.13, IC_E8.2, NOISE)
	NET_C(IC_E8.8, R33.1)
	NET_C(IC_E8.11, R34.1)
	NET_C(IC_E8.3, R36.1)
	NET_C(R35.1, GND)
	NET_C(R33.2, R34.2, R35.2, R36.2, C22.1, C21.1)
	NET_C(C21.2, R47.2, IC_C8.14)
	ALIAS(FOLLA, IC_C8.14)
	NET_C(R37.2, I_V5)
	NET_C(C22.2, R47.1, IC_C8.13)
	NET_C(R37.1, C24.2, IC_C8.12)
	NET_C(C24.1, GND)

	// CONTRASTO
	SUBMODEL(NE556_DIP, IC_Q9)
	NET_C(IC_Q9.7, GND)
	NET_C(IC_Q9.14, I_V5)
	RES(R38, RES_M(1))
	RES(R39, RES_K(100))
	CAP(C23, CAP_U(0.1))
	CAP(C25, CAP_U(0.1))
	CAP(C26, CAP_U(0.47))
	NET_C(R38.1, I_V5)
	NET_C(R39.1, I_V5)
	NET_C(R38.2, IC_Q9.1, IC_Q9.2, C25.1)
	NET_C(C25.2, C26.2, GND)
	NET_C(R39.2, IC_Q9.13, IC_Q9.12, IC_Q9.8, C26.1)
	NET_C(IC_Q9.5, IC_Q9.10)
	NET_C(IC_Q9.9, C23.1)
	NET_C(IC_Q9.4, I_V5)
	NET_C(I_CONTRASTO, IC_Q9.6)
	ALIAS(CONT_OUT, C23.2)
	NET_C(C23.2, R36.2)



	// OUTPUT SECTION ----------------------
	// TOS
	CAP(C_TOS, CAP_N(100))
	RES(R_TOS, RES_K(220))
	NET_C(TOS, C_TOS.1)
	NET_C(R_TOS.1, C_TOS.2)

	// FISCHIO
	CAP(C_FISCHIO, CAP_N(1000))
	RES(R_FISCHIO, RES_K(220))
	NET_C(FISCHIO, C_FISCHIO.1)
	NET_C(R_FISCHIO.1, C_FISCHIO.2)

	// FOLLA
	CAP(C_FOLLA, CAP_N(10))
	RES(R_FOLLA, RES_K(100))
	NET_C(FOLLA, C_FOLLA.1)
	NET_C(C_FOLLA.2, R_FOLLA.1)

	// CALCIO_A
	CAP(C_CALCIO_A, CAP_U(1))
	RES(R_CALCIO_A, RES_K(10))
	NET_C(CALCIO_A.OUTPUT, C_CALCIO_A.1)
	NET_C(C_CALCIO_A.2, R_CALCIO_A.1)

	// CALCIO_B
	CAP(C_CALCIO_B, CAP_U(1))
	RES(R_CALCIO_B, RES_K(10))
	NET_C(CALCIO_B.OUTPUT, C_CALCIO_B.1)
	NET_C(C_CALCIO_B.2, R_CALCIO_B.1)

	// STOP_PALLA
	CAP(C_STOP_PALLA, CAP_U(1))
	RES(R_STOP_PALLA, RES_K(100))
	NET_C(STOP_PALLA.OUTPUT, C_STOP_PALLA.1)
	NET_C(C_STOP_PALLA.2, R_STOP_PALLA.1)

	// PARATA
	CAP(C_PARATA, CAP_U(1))
	RES(R_PARATA, RES_K(100))
	NET_C(PARATA.OUTPUT, C_PARATA.1)
	NET_C(C_PARATA.2, R_PARATA.1)



	// FINAL MIX, DISABLE CIRCUITRY AND OUTPUT FILTER
	TTL_INPUT(ENABLE_SOUND, 0)
	NET_C(ENABLE_SOUND.VCC, I_V5)
	NET_C(ENABLE_SOUND.GND, GND)
	QBJT_EB(Q_OUT, "BC239C")
	RES(R_OUT, RES_K(2.2))
	RES(R_PU_OUT, RES_K(10))
	NET_C(R_PU_OUT.1, I_V5)
	NET_C(ENABLE_SOUND, R_OUT.1, R_PU_OUT.2)
	NET_C(Q_OUT.B, R_OUT.2)
	NET_C(Q_OUT.C, R_TOS.2, R_FISCHIO.2, R_FOLLA.2, R_CALCIO_A.2, R_CALCIO_B.2, R_STOP_PALLA.2, R_PARATA.2)
	NET_C(Q_OUT.E, GND)
	SUBMODEL(output_filter, OUTPUT_FILTER)
	NET_C(OUTPUT_FILTER.INPUT, Q_OUT.C)
	NET_C(OUTPUT_FILTER.GND, GND)



	ALIAS(OUTPUT, OUTPUT_FILTER.OUTPUT)
}
