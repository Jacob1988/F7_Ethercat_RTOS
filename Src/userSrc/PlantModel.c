#include "PlantModel.h"

struct MYJOINT myjoint={
		.rotaryPlate={
			.r=0.06,
			.m=0.943,//0.523,
			.com=0.11,
			.I=0.034566,//8.29e-4,
			.b=0,
			.fm=0.005,
			.fc=0.2,
			.kv=0.01},
		.shelinder1={
			.lmax=0.1,
			.l0=0.2,
			.v0=5e-6,
			.A=2.2e-3,
			.m=0.025,
			.k=800,
			.b=1,
			.fm=8,
			.fc=8,
			.kv=0.02
			},
		.shelinder2={
			.lmax=0.1,
			.l0=0.2,
			.v0=5e-6,
			.A=2.2e-3,
			.m=0.025,
			.k=800,
			.b=1,
			.fm=8,
			.fc=8,
			.kv=0.02},
};




struct YIJUAN_ROTARY_PLANT YiJuanPlant;

void Init_YiJuanPlant()
{

  YiJuanPlant.Psource=100000+101325;
  YiJuanPlant.rA=6.9e-5;
  YiJuanPlant.x0=3.1415926;
  YiJuanPlant.v0=2*YiJuanPlant.x0*YiJuanPlant.rA;
  YiJuanPlant.I = 0.00001; //approx
  YiJuanPlant.K = 7; //3.283048 ;//(Nm)  0.0573(Nm/degree)
  YiJuanPlant.F = 0;
  YiJuanPlant.fm = 0;
  YiJuanPlant.fc = 0;
  YiJuanPlant.kv = 0;
}


void Init_myjoint()
{
	myjoint.I=myjoint.rotaryPlate.I+(myjoint.shelinder1.m+myjoint.shelinder2.m)*myjoint.rotaryPlate.r*myjoint.rotaryPlate.r;
	myjoint.B=myjoint.rotaryPlate.b+(myjoint.shelinder1.b+myjoint.shelinder2.b)*myjoint.rotaryPlate.r*myjoint.rotaryPlate.r;
    myjoint.K=(myjoint.shelinder1.k+myjoint.shelinder2.k)*myjoint.rotaryPlate.r*myjoint.rotaryPlate.r;
    myjoint.x0 =  myjoint.shelinder1.l0 / myjoint.rotaryPlate.r;
}
