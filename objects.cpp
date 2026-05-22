/*********************************************************************
Simulation obiektów fizycznych ruchomych np. samochody, statki, roboty, itd.
+ obs³uga obiektów statycznych np. terrain.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <time.h>
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <iterator> 
#include <map>
using namespace std;
#ifndef _OBJECTS__H
#include "objects.h"
#endif
#include "graphics.h"


//#include "vector3D.h"
extern FILE *f;
extern HWND main_window;

extern ViewParameters par_view;
extern map<int, MovableObject*> network_vehicles;
extern CRITICAL_SECTION m_cs;


extern bool terrain_edition_mode;
//extern Terrain terrain;
//extern int iLiczbaCudzychOb;
//extern MovableObject *CudzeObiekty[1000]; 

//enum ItemTypes { ITEM_COIN, ITEM_BARREL, ITEM_TREE, ITEM_BUILDING, ITEM_POINT, ITEM_EDGE };
char *PRZ_nazwy[32] = { "moneta", "beczka", "drzewo", "punkt", "krawedz" };
//enum TreeSubtypes { TREE_POPLAR, TREE_SPRUCE, TREE_BAOBAB, TREE_FANTAZJA };
char *DRZ_nazwy[32] = { "topola", "swierk", "baobab", "fantazja" };

unsigned long __log2(unsigned long x)  // w starszej wersji Visuala (< 2013) nie ma funkcji log2() w bibliotece math.h  !!!
{
	long i = -1;
	long y = x;
	while (y > 0){
	 	y = y >> 1;
		i++;
	}
	return i;
}

MovableObject::MovableObject(Terrain *t)             // konstruktor                   
{

	terrain = t;

	//iID = (unsigned int)(clock() % 1000);  // identyfikator obiektu
	iID = (unsigned int)(100+rand() % 900);  // identyfikator obiektu
	fprintf(f, "Nowy obiekt: iID = %d\n", iID);
	state.iID_owner = iID;           // identyfikator w³aœciciela obiektu
	state.if_autonomous = 0;

	state.money = 1000;    // np. dolarów
	state.amount_of_fuel = 10.0;   // np. kilogramów paliwa 

	time_of_simulation = 0;    // symulowany czas rzeczywisty od pocz¹tku istnienia obiektu 

	F = 0;	      // si³y dzia³aj¹ce na obiekt 
	breaking_degree = 0;			      // stopieñ hamowania
	if_keep_steer_wheel = false;
	wheel_turn_speed = 0;

	F_max = 35000;
	alpha_max = PI*25.0 / 180;
	mass_own = 1500.0;     // masa w³asna obiektu [kg] (bez paliwa)
	state.mass_total = mass_own + state.amount_of_fuel;  // masa ca³kowita
	Fy = mass_own*9.81;    // si³a nacisku na podstawê obiektu (na ko³a pojazdu)
	length = 7.0;
	width = 4.2;
	height = 1.1;
	clearance = 0.0;           // wysokoœæ na której znajduje siê podstawa obiektu
	front_axis_dist = 1.0;           // odleg³oœæ od przedniej osi do przedniego zderzaka 
	back_axis_dist = 0.2;             // odleg³oœæ od tylniej osi do tylniego zderzaka
	wheel_ret_speed = 1;



	iID_collider = -1;           // na razie brak kolizji 

	// wybór po³o¿enia startowego:
	bool if_point_of_vehicle = false;
	for (long i = 0; i < t->number_of_items; i++)    
	{
		if (t->p[i].subtype == POINT_OF_VEHICLE)
		{
			state.vPos = t->p[i].vPos;
			state.qOrient = t->p[i].qOrient;
			if_point_of_vehicle = true;
			break;
		}
	}

	state.vPos = Vector3(-1651.177368, 10.905829, -22.069324);
	state.qOrient = quaternion(0.000000, -0.438572, 0.000000, -0.898689);


	state.wheel_turn_angle = 0;
	radius = sqrt(length*length + width*width + height*height) / 2 / 1.15;
	//vV_angular = Vector3(0,1,0)*40;  // pocz¹tkowa prêdkoœæ k¹towa (w celach testowych)

	//moment_wziecia = 0;            // czas ostatniego wziecia przedmiotu
	//czas_oczekiwania = 1000000000; // 
	//item_number = -1;

	number_of_taking_item = -1;
	taking_value = 0;
	number_of_renewed_item = -1;

	

	// losowanie umiejêtnoœci tak by nie by³o bardzo s³abych i bardzo silnych:
	planting_skills = 0.2 + (float)(rand() % 5) / 5;
	fuel_collection_skills = 0.2 + (float)(rand() % 5) / 5;
	money_collection_skills = 0.2 + (float)(rand() % 5) / 5;
	//float suma_um = planting_skills + fuel_collection_skills + money_collection_skills;
	//float suma_um_los = 0.7 + 0.8*(float)rand()/RAND_MAX;    // losuje umiejetnoœæ sumaryczn¹
	//planting_skills *= suma_um_los/suma_um;
	//fuel_collection_skills *= suma_um_los/suma_um;
	//money_collection_skills *= suma_um_los/suma_um;

	if_selected = false;
}

MovableObject::~MovableObject()            // destruktor
{
}

void MovableObject::ChangeState(ObjectState __state)  // przepisanie podanego stanu 
{   
	state = __state;
}

ObjectState MovableObject::State()                // metoda zwracaj¹ca state obiektu ³¹cznie z iID
{
	return state;
}


void MovableObject::Simulation(float dt)          // obliczenie nowego stanu na podstawie dotychczasowego,
{                                                // dzia³aj¹cych si³ i czasu, jaki up³yn¹³ od ostatniej symulacji
	if (dt == 0) return;

	time_of_simulation += dt;          // sumaryczny czas wszystkich symulacji obiektu od jego powstania

	float friction_ground = 0.8;            // wspó³czynnik tarcia obiektu o pod³o¿e 
	float friction_rot = friction_ground;     // friction_ground obrotowe (w szczególnych przypadkach mo¿e byæ inne ni¿ liniowe)
	float friction_rolling = 0.35;    // wspó³czynnik tarcia tocznego
	float friction_of_water = 0.2;   // opór wody w N*s2/m4
	float elasticity = 0.8;       // wspó³czynnik sprê¿ystoœci (0-brak sprê¿ystoœci, 1-doskona³a sprê¿ystoœæ) 
	float g = 9.81;                // przyspieszenie grawitacyjne
	float m = mass_own + state.amount_of_fuel;   // masa calkowita
	float closed_volume = 0.1;     // zamkniêta komora powietrzna do której nie dostaje siê woda po zanurzeniu 

	Vector3 vPos_prev = state.vPos;       // zapisanie poprzedniego po³o¿enia

	// obracam uk³ad wspó³rzêdnych lokalnych wed³ug kwaterniona orientacji:
	Vector3 vect_local_forward = state.qOrient.rotate_vector(Vector3(1, 0, 0)); // na razie oœ obiektu pokrywa siê z osi¹ x globalnego uk³adu wspó³rzêdnych (lokalna oœ x)
	Vector3 vect_local_up = state.qOrient.rotate_vector(Vector3(0, 1, 0));  // wektor skierowany pionowo w górê od podstawy obiektu (lokalna oœ y)
	Vector3 vect_local_right = state.qOrient.rotate_vector(Vector3(0, 0, 1)); // wektor skierowany w prawo (lokalna oœ z)

	//fprintf(f,"vect_local_forward = (%f, %f, %f)\n",vect_local_forward.x,vect_local_forward.y,vect_local_forward.z);
	//fprintf(f,"vect_local_up = (%f, %f, %f)\n",vect_local_up.x,vect_local_up.y,vect_local_up.z);
	//fprintf(f,"vect_local_right = (%f, %f, %f)\n",vect_local_right.x,vect_local_right.y,vect_local_right.z);

	//fprintf(f,"|vect_local_forward|=%f,|vect_local_up|=%f,|vect_local_right|=%f\n",vect_local_forward.length(),vect_local_up.length(),vect_local_right.length()  );
	//fprintf(f,"ilo skalar = %f,%f,%f\n",vect_local_forward^vect_local_right,vect_local_forward^vect_local_up,vect_local_up^vect_local_right  );
	//fprintf(f,"vect_local_forward = (%f, %f, %f) vect_local_up = (%f, %f, %f) vect_local_right = (%f, %f, %f)\n",
	//           vect_local_forward.x,vect_local_forward.y,vect_local_forward.z,vect_local_up.x,vect_local_up.y,vect_local_up.z,vect_local_right.x,vect_local_right.y,vect_local_right.z);


	// rzutujemy vV na sk³adow¹ w kierunku przodu i pozosta³e 2 sk³adowe
	// sk³adowa w bok jest zmniejszana przez si³ê tarcia, sk³adowa do przodu
	// przez si³ê tarcia tocznego
	Vector3 vV_forward = vect_local_forward*(state.vV^vect_local_forward),
		vV_right = vect_local_right*(state.vV^vect_local_right),
		vV_up = vect_local_up*(state.vV^vect_local_up);

	// dodatkowa normalizacja likwidujaca blad numeryczny:
	if (state.vV.length() > 0)
	{
		float error_of_length = (vV_forward + vV_right + vV_up).length() / state.vV.length();
		vV_forward = vV_forward / error_of_length;
		vV_right = vV_right / error_of_length;
		vV_up = vV_up / error_of_length;
	}

	// rzutujemy prêdkoœæ k¹tow¹ vV_angular na sk³adow¹ w kierunku przodu i pozosta³e 2 sk³adowe
	Vector3 vV_ang_forward = vect_local_forward*(state.vV_angular^vect_local_forward),
		vV_ang_right = vect_local_right*(state.vV_angular^vect_local_right),
		vV_ang_up = vect_local_up*(state.vV_angular^vect_local_up);


	// ograniczenia 
	if (F > F_max) F = F_max;
	if (F < -F_max / 2) F = -F_max / 2;
	// ruch kó³ na skutek krêcenia lub puszczenia kierownicy:  
	if (wheel_turn_speed != 0)
		state.wheel_turn_angle += wheel_turn_speed * dt;
	else
		if (state.wheel_turn_angle > 0)
		{
			if (!if_keep_steer_wheel)
				state.wheel_turn_angle -= wheel_ret_speed * dt;
			if (state.wheel_turn_angle < 0) state.wheel_turn_angle = 0;
		}
		else if (state.wheel_turn_angle < 0)
		{
			if (!if_keep_steer_wheel)
				state.wheel_turn_angle += wheel_ret_speed * dt;
			if (state.wheel_turn_angle > 0) state.wheel_turn_angle = 0;
		}
	// ograniczenia: 
	if (state.wheel_turn_angle > alpha_max) state.wheel_turn_angle = alpha_max;
	if (state.wheel_turn_angle < -alpha_max) state.wheel_turn_angle = -alpha_max;



	// obliczam promieñ skrêtu pojazdu na podstawie k¹ta skrêtu kó³, a nastêpnie na podstawie promienia skrêtu
	// obliczam prêdkoœæ k¹tow¹ (UPROSZCZENIE! pomijam przyspieszenie k¹towe oraz w³aœciw¹ trajektoriê ruchu)
	if (Fy > 0)
	{
		float V_turn_angle = 0;
		if (state.wheel_turn_angle != 0)
		{
			float Rs = sqrt(length*length / 4 + (fabs(length / tan(state.wheel_turn_angle)) + width / 2)*(fabs(length / tan(state.wheel_turn_angle)) + width / 2));
			V_turn_angle = vV_forward.length()*(1.0 / Rs);
		}
		Vector3 vV_turn_angle = vect_local_up*V_turn_angle*(state.wheel_turn_angle > 0 ? 1 : -1);
		Vector3 vV_angle_to_up2 = vV_ang_up + vV_turn_angle;
		if (vV_angle_to_up2.length() <= vV_ang_up.length()) // skrêt przeciwdzia³a obrotowi
		{
			if (vV_angle_to_up2.length() > V_turn_angle)
				vV_ang_up = vV_angle_to_up2;
			else
				vV_ang_up = vV_turn_angle;
		}
		else
		{
			if (vV_ang_up.length() < V_turn_angle)
				vV_ang_up = vV_turn_angle;

		}

		// friction_ground zmniejsza prêdkoœæ obrotow¹ (UPROSZCZENIE! zamiast masy winienem wykorzystaæ moment bezw³adnoœci)     
		float V_angle_friction = Fy*friction_rot*dt / m / 1.0;      // zmiana pr. k¹towej spowodowana tarciem
		float V_angle_up = vV_ang_up.length() - V_angle_friction;
		if (V_angle_up < V_turn_angle) V_angle_up = V_turn_angle;        // friction_ground nie mo¿e spowodowaæ zmiany zwrotu wektora pr. k¹towej
		vV_ang_up = vV_ang_up.znorm()*V_angle_up;
	}


	Fy = m*g*vect_local_up.y;                      // si³a docisku do pod³o¿a 
	if (Fy < 0) Fy = 0;
	// ... trzeba j¹ jeszcze uzale¿niæ od tego, czy obiekt styka siê z pod³o¿em!
	float Fh = Fy*friction_ground*breaking_degree;                  // si³a hamowania (UP: bez uwzglêdnienia poœlizgu)

	float V_forward = vV_forward.length();// - dt*Fh/m - dt*friction_rolling*Fy/m;
	if (V_forward < 0) V_forward = 0;

	float V_right = vV_right.length();// - dt*friction_ground*Fy/m;
	if (V_right < 0) V_right = 0;


	// wjazd lub zjazd: 
	//vPos.y = terrain.GroundHeight(vPos.x,vPos.z);   // najprostsze rozwi¹zanie - obiekt zmienia wysokoœæ bez zmiany orientacji

	// 1. gdy wjazd na wklês³oœæ: wyznaczam wysokoœci terrainu pod naro¿nikami obiektu (ko³ami), 
	// sprawdzam która trójka
	// naro¿ników odpowiada najni¿ej po³o¿onemu œrodkowi ciê¿koœci, gdy przylega do terrainu
	// wyznaczam prêdkoœæ podbicia (wznoszenia œrodka pojazdu spowodowanego wklês³oœci¹) 
	// oraz prêdkoœæ k¹tow¹
	// 2. gdy wjazd na wypuk³oœæ to si³a ciê¿koœci wywo³uje obrót przy du¿ej prêdkoœci liniowej

	// punkty zaczepienia kó³ (na wysokoœci pod³ogi pojazdu):
	Vector3 P = state.vPos + vect_local_forward*(length / 2 - front_axis_dist) - vect_local_right*width / 2 - vect_local_up*height / 2,
		Q = state.vPos + vect_local_forward*(length / 2 - front_axis_dist) + vect_local_right*width / 2 - vect_local_up*height / 2,
		R = state.vPos + vect_local_forward*(-length / 2 + back_axis_dist) - vect_local_right*width / 2 - vect_local_up*height / 2,
		S = state.vPos + vect_local_forward*(-length / 2 + back_axis_dist) + vect_local_right*width / 2 - vect_local_up*height / 2;

	// pionowe rzuty punktów zacz. kó³ pojazdu na powierzchniê terrainu:  
	Vector3 Pt = P, Qt = Q, Rt = R, St = S;
	//Pt.y = terrain->GroundHeight(P.x, P.z); Qt.y = terrain->GroundHeight(Q.x, Q.z);
	//Rt.y = terrain->GroundHeight(R.x, R.z); St.y = terrain->GroundHeight(S.x, S.z);
	Pt.y = terrain->GeneralHeight(P); Qt.y = terrain->GeneralHeight(Q);
	Rt.y = terrain->GeneralHeight(R); St.y = terrain->GeneralHeight(S);
	Vector3 normPQR = normal_vector(Pt, Rt, Qt), normPRS = normal_vector(Pt, Rt, St), normPQS = normal_vector(Pt, St, Qt),
		normQRS = normal_vector(Qt, Rt, St);   // normalne do p³aszczyzn wyznaczonych przez trójk¹ty

	//fprintf(f,"P.y = %f, Pt.y = %f, Q.y = %f, Qt.y = %f, R.y = %f, Rt.y = %f, S.y = %f, St.y = %f\n",
	//  P.y, Pt.y, Q.y, Qt.y, R.y,Rt.y, S.y, St.y);

	float sryPQR = ((Qt^normPQR) - normPQR.x*state.vPos.x - normPQR.z*state.vPos.z) / normPQR.y, // wys. œrodka pojazdu
		sryPRS = ((Pt^normPRS) - normPRS.x*state.vPos.x - normPRS.z*state.vPos.z) / normPRS.y, // po najechaniu na skarpê 
		sryPQS = ((Pt^normPQS) - normPQS.x*state.vPos.x - normPQS.z*state.vPos.z) / normPQS.y, // dla 4 trójek kó³
		sryQRS = ((Qt^normQRS) - normQRS.x*state.vPos.x - normQRS.z*state.vPos.z) / normQRS.y;
	float sry = sryPQR; Vector3 normal_vectors = normPQR;
	if (sry > sryPRS) { sry = sryPRS; normal_vectors = normPRS; }
	if (sry > sryPQS) { sry = sryPQS; normal_vectors = normPQS; }
	if (sry > sryQRS) { sry = sryQRS; normal_vectors = normQRS; }  // wybór trójk¹ta o œrodku najni¿ej po³o¿onym    



	Vector3 vV_angle_horiz = Vector3(0, 0, 0);
	// jesli któreœ z kó³ jest poni¿ej powierzchni terrainu
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))
	{
		// obliczam powsta³¹ prêdkoœæ k¹tow¹ w lokalnym uk³adzie wspó³rzêdnych:      
		Vector3 v_rotation = -normal_vectors.znorm()*vect_local_up*0.6;
		vV_angle_horiz = v_rotation / dt;
	}

	Vector3 wAg = Vector3(0, -1, 0)*g;    // przyspieszenie grawitacyjne

	// jesli wiecej niz 2 kola sa na ziemi, to przyspieszenie grawitacyjne jest rownowazone przez opor gruntu:
	if ((P.y <= Pt.y + height / 2 + clearance) + (Q.y <= Qt.y + height / 2 + clearance) +
		(R.y <= Rt.y + height / 2 + clearance) + (S.y <= St.y + height / 2 + clearance) > 2)
	{
		wAg = wAg +
			vect_local_up*(vect_local_up^wAg)*-1; //przyspieszenie wynikaj¹ce z si³y oporu gruntu
	}
	else   // w przeciwnym wypadku brak sily docisku 
		Fy = 0;



	// sk³adam z powrotem wektor prêdkoœci k¹towej: 
	//vV_angular = vV_ang_up + vV_ang_right + vV_ang_forward;  
	state.vV_angular = vV_ang_up + vV_angle_horiz;


	float h = sry + height / 2 + clearance - state.vPos.y;  // ró¿nica wysokoœci jak¹ trzeba pokonaæ  
	float V_jump = 0;
	if ((h > 0) && (state.vV.y <= 0.01))
		V_jump = 0.5*sqrt(2 * g*h);  // prêdkoœæ spowodowana podbiciem pojazdu przy wje¿d¿aniu na skarpê 
	if (h > 0) state.vPos.y = sry + height / 2 + clearance;

	// lub  w przypadku zag³êbienia siê 
	//fprintf(f,"sry = %f, vPos.y = %f, dt = %f\n",sry,vPos.y,dt);  
	//fprintf(f,"normPQR.y = %f, normPRS.y = %f, normPQS.y = %f, normQRS.y = %f\n",normPQR.y,normPRS.y,normPQS.y,normQRS.y); 


	Vector3 dwPos = state.vV*dt;//vA*dt*dt/2; // czynnik bardzo ma³y - im wiêksza czêstotliwoœæ symulacji, tym mniejsze znaczenie 
	state.vPos = state.vPos + dwPos;

	// korekta po³o¿enia w przypadku terrainu cyklicznego (toroidalnego) z uwzglêdnieniem granic:
	if (terrain->if_toroidal_world)
	{
		if (terrain->border_x > 0)
			if (state.vPos.x < -terrain->border_x) state.vPos.x += terrain->border_x*2;
			else if (state.vPos.x > terrain->border_x) state.vPos.x -= terrain->border_x*2;
		if (terrain->border_z > 0)
			if (state.vPos.z < -terrain->border_z) state.vPos.z += terrain->border_z*2;
			else if (state.vPos.z > terrain->border_z) state.vPos.z -= terrain->border_z*2;
	}
	else 
	{
		if (terrain->border_x > 0)
			if (state.vPos.x < -terrain->border_x) state.vPos.x = -terrain->border_x;
			else if (state.vPos.x > terrain->border_x) state.vPos.x = terrain->border_x;
		if (terrain->border_z > 0)
			if (state.vPos.z < -terrain->border_z) state.vPos.z = -terrain->border_z;
			else if (state.vPos.z > terrain->border_z) state.vPos.z = terrain->border_z;
	}

	// Sprawdzenie czy obiekt mo¿e siê przemieœciæ w zadane miejsce: Jeœli nie, to 
	// przemieszczam obiekt do miejsca zetkniêcia, wyznaczam nowe wektory prêdkoœci
	// i prêdkoœci k¹towej, a nastêpne obliczam nowe po³o¿enie na podstawie nowych
	// prêdkoœci i pozosta³ego czasu. Wszystko powtarzam w pêtli (pojazd znowu mo¿e 
	// wjechaæ na przeszkodê). Problem z zaokr¹glonymi przeszkodami - konieczne 
	// wyznaczenie minimalnego kroku.


	Vector3 wV_pop = state.vV;

	// sk³adam prêdkoœci w ró¿nych kierunkach oraz efekt przyspieszenia w jeden wektor:    (problem z przyspieszeniem od si³y tarcia -> to przyspieszenie 
	//      mo¿e dzia³aæ krócej ni¿ dt -> trzeba to jakoœ uwzglêdniæ, inaczej poazd bêdzie wê¿ykowa³)
	state.vV = vV_forward.znorm()*V_forward + vV_right.znorm()*V_right + vV_up +
		Vector3(0, 1, 0)*V_jump + state.vA*dt;
	// usuwam te sk³adowe wektora prêdkoœci w których kierunku jazda nie jest mo¿liwa z powodu
	// przeskód:
	// np. jeœli pojazd styka siê 3 ko³ami z nawierzchni¹ lub dwoma ko³ami i œrodkiem ciê¿koœci to
	// nie mo¿e mieæ prêdkoœci w dó³ pod³ogi
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))    // jeœli pojazd styka siê co najm. jednym ko³em
	{
		Vector3 dwV = vV_up + vect_local_up*(state.vA^vect_local_up)*dt;
		if ((vect_local_up.znorm() - dwV.znorm()).length() > 1)  // jeœli wektor skierowany w dó³ pod³ogi
			state.vV = state.vV - dwV;
	}

	/*fprintf(f," |vV_forward| %f -> %f, |vV_right| %f -> %f, |vV_up| %f -> %f |vV| %f -> %f\n",
	vV_forward.length(), (vV_forward.znorm()*V_forward).length(),
	vV_right.length(), (vV_right.znorm()*V_right).length(),
	vV_up.length(), (vV_up.znorm()*vV_up.length()).length(),
	wV_pop.length(), vV.length()); */


	// obliczenie przyspieszenia od wody (opór + wypór (buoyancy )):
	// zanurzenie przy za³o¿eniu, ¿e p³aszczyzna pjazdu jest równoleg³a do wody:
	float dipping = terrain->LevelOfWater(state.vPos.x, state.vPos.z) - (state.vPos.y - height / 2);
	if (dipping > height) dipping = height;
	else if (dipping < 0) dipping = 0;
	float A_water = friction_of_water*(dipping / height)*(state.vV.length()*state.vV.length());

	float A_buoyancy = (dipping / height)*closed_volume*1000*g/m;


	// sk³adam przyspieszenia liniowe od si³ napêdzaj¹cych i od si³ oporu: 
	state.vA = (vect_local_forward*F) / m*(Fy > 0)*(state.amount_of_fuel > 0)  // od si³ napêdzaj¹cych
		- vV_forward.znorm()*(Fh / m + friction_rolling*Fy / m)*(V_forward > 0.01) // od hamowania i tarcia tocznego (w kierunku ruchu)
		- vV_right.znorm()*friction_ground*Fy / m*(V_right > 0.01)                 // od tarcia w kierunku prost. do kier. ruchu
		- state.vV.znorm()*A_water                                                 // od oporu wody w kierunku ruchu 
		+ Vector3(0, 1, 0)*A_buoyancy                                              // od wyporu wody
		+ wAg;           // od grawitacji


	// utrata paliwa:
	state.amount_of_fuel -= (fabs(F))*(Fy > 0)*dt / 100000;
	if (state.amount_of_fuel < 0)state.amount_of_fuel = 0;
	state.mass_total = mass_own + state.amount_of_fuel;


	// obliczenie nowej orientacji:
	Vector3 v_rotation_ = state.vV_angular*dt;// + vA_angular*dt*dt/2;    
	quaternion q_rotation_ = AsixToQuat(v_rotation_.znorm(), v_rotation_.length());
	//fprintf(f,"v_rotation_ = (x=%f, y=%f, z=%f) \n",v_rotation_.x, v_rotation_.y, v_rotation_.z );
	//fprintf(f,"q_rotation_ = (w=%f, x=%f, y=%f, z=%f) \n",q_rotation_.w, q_rotation_.x, q_rotation_.y, q_rotation_.z );
	state.qOrient = q_rotation_* state.qOrient;
	//fprintf(f,"Pol = (%f, %f, %f) V = (%f, %f, %f) A = (%f, %f, %f) V_kat = (%f, %f, %f) ID = %d\n",
	//  vPos.x,vPos.y,vPos.z,vV.x,vV.y,vV.z,vA.x,vA.y,vA.z,vV_angular.x,vV_angular.y,vV_angular.z,iID);

	Item **item_tab_pointer = NULL;
	long number_of_items_in_radius = terrain->ItemsInRadius(&item_tab_pointer, state.vPos, this->radius * 2);
	// Generujê listê wskaŸników przedmiotów w s¹siedztwie symulowanego o radiusiu 2.2, gdy¿ 
	// w przypadku obiektów mniejszych wystarczy 2.0 (zak³adaj¹c, ¿e another obiekt ma promieñ co najwy¿ej taki sam),
	// a w przypadku wiêkszych to symulator tego wiêkszego powinien wczeœniej wykryæ kolizjê 
	MovableObject **object_tab_pointer = NULL;
	long number_of_objects_in_radius = terrain->ObjectsInRadius(&object_tab_pointer, state.vPos, this->radius * 2.2 + state.vV.length()*dt);

	// wykrywanie kolizji z drzewami:
	// wykrywanie kolizji z drzewami z u¿yciem tablicy obszarów:
	Vector3 wWR = state.vPos - vPos_prev;  // wektor jaki przemierzy³ pojazd w tym cyklu
	Vector3 wWR_zn = wWR.znorm();
	float fWR = wWR.length();
	for (long i = 0; i < number_of_items_in_radius; i++)
	{
		Item *prz = item_tab_pointer[i];
		if (prz->type == ITEM_TREE)
		{
			// bardzo duze uproszczenie -> traktuje pojazd jako kulê
			Vector3 vPos_tree = prz->vPos;
			vPos_tree.y = (vPos_tree.y + prz->value > state.vPos.y ? state.vPos.y : vPos_tree.y + prz->value);
			float radius_of_tree = prz->param_f[0] * prz->diameter / 2;
			if ((vPos_tree - state.vPos).length() < radius*0.8 + radius_of_tree)  // jesli kolizja 
			{
				// od wektora predkosci odejmujemy jego rzut na direction od punktu styku do osi drzewa:
				// jesli pojazd juz wjechal w ITEM_TREE, to nieco zwiekszamy poprawke     
				// punkt styku znajdujemy laczac krawedz pojazdu z osia drzewa odcinkiem
				// do obu prostopadlym      
				Vector3 dP = (vPos_tree - state.vPos).znorm();   // wektor, w ktorego kierunku ruch jest niemozliwy
				float k = state.vV^dP;
				if (k > 0)     // jesli jest skladowa predkosci w strone drzewa
				{
					Vector3 wV_pocz = state.vV;
					//vV = wV_pocz - dP*k*(1 + elasticity);  // odjecie skladowej + odbicie sprezyste 
					


					// Uwzglêdnienie obrotu:
					// pocz¹tkowa energia kinetyczna ruchu zamienia siê w energiê kinetyczn¹ ruchu postêpowego po 
					// kolizji i energiê ruchu obrotowego:
					float cos_alpha = state.vV.znorm() ^ dP;       // kosinus k¹ta pomiêdzy kierunkiem pojazda-drzewo a wektorem prêdkoœci poj.  
					
					// przyjmujê, ¿e im wiêkszy k¹t, tym wiêcej energii idzie na obrót
					state.vV = wV_pocz - dP*k*(1 + elasticity)*cos_alpha;
					Vector3 wV_spr = wV_pocz - dP*k * 2*cos_alpha;                          // wektor prêdkoœci po odbiciu w pe³ni sprê¿ystym
					float fV_spr = wV_spr.length(), fV_pocz = wV_pocz.length();
					//float fV = vV.length();
					float dE = (fV_pocz*fV_pocz - fV_spr*fV_spr)*state.mass_total / 2;
					if (dE > 0)     // w³aœciwie nie powinno byæ inaczej!
					{
						float I = (length*length + width*width)*state.mass_total / 12;  // moment bezw³adnoœci prostok¹ta
						float omega = sqrt(2 * dE / I);     // modu³ prêdkoœci k¹towej wynikaj¹cy z reszty energii
						state.vV_angular = state.vV_angular + dP.znorm()*wV_pocz.znorm()*omega*elasticity;
					}
					
				} // jeœli wektor prêdkoœci w kierunku drzewa
			} // jeœli kolizja
		} // jeœli drzewo
		else if (prz->type == ITEM_WALL)
		{			
			// bardzo duze uproszczenie -> traktuje pojazd jako kulê
			Vector3 A = terrain->p[prz->param_i[0]].vPos, B = terrain->p[prz->param_i[1]].vPos;       // punkty tworz¹ce krawêdŸ
			A.y += terrain->p[prz->param_i[0]].value;
			B.y += terrain->p[prz->param_i[1]].value;

			Vector3 AB = B - A;                  // wektor wyznaczaj¹cy oœ podstawy
			//Vector3 AB_zn = AB.znorm();		
			Vector3 m_pion = Vector3(0,1,0);     // vertical muru
			float m_wysokosc = prz->value;       // od prostej AB do góry jest polowa wysokoœci, druga polowa idzie w dó³ 

			// zanim policzymy to co poni¿ej, mo¿na wykonaæ prostszy test np. odleg³oœci pomiêdzy spoziomowanymi odcinkami AB i odcinkiem vPos_prev,vPos
			// jeœli ta odleg³oœæ nie bêdzie wiêksza ni¿ szerokoœæ muru/2+radius to mo¿liwa kolizja:







			int liczba_scian = 4;     // œciany boczne
			Vector3 m_wlewo;          // wektor prostopad³y do œciany wg³¹b muru
			Vector3 m_wprzod;         // wektor po d³ugoœci œciany (ApBp) 
			float m_dlugosc;          // d³ugoœæ œciany
			float m_szerokosc;        // szerokoœæ muru poprzecznie do œciany  
			Vector3 Ap, Bp;           // pocz¹tek i koniec œciany
			float Ap_y, Bp_y;           // wysokoœci w punktach pocz¹tkowym i koñcowym œciany
			bool if_collision_handled = false;   // czy kolizja zosta³a zrealizowana 

			for (int sciana = 0; sciana < liczba_scian; sciana++)
			{
				switch (sciana)
				{
				case 0:   // œciana z prawej strony patrz¹c od punktu A do B
					m_wlewo = (m_pion*AB).znorm();          // wektor jednostkowy w lewo od wektora AB i prostopadle do pionu
					m_wprzod = m_wlewo*m_pion;
					m_dlugosc = AB^m_wprzod;
					m_szerokosc = prz->param_f[0];

					Ap = A - m_wlewo*m_szerokosc / 2;
					Bp = B - m_wlewo*m_szerokosc / 2; // rzut odcinka AB na praw¹ œcianê muru
					Ap_y = A.y;
					Bp_y = B.y;
					break;
				case 1:  // œciana kolejna w kier. przeciwnym do ruchu wskazówek zegara (prostopad³a do AB, przechodz¹ca przez punkt B)
					m_wlewo = -AB.znorm();          // wektor jednostkowy w lewo od wektora AB i prostopadle do pionu
					m_wprzod = m_wlewo*m_pion;
					m_dlugosc = prz->param_f[0];
					m_szerokosc = -AB^m_wlewo;

					Ap = B - m_wprzod*m_dlugosc / 2;
					Bp = B + m_wprzod*m_dlugosc / 2; 
					Ap_y = Bp_y = B.y;

					break;
				case 2:  
					m_wlewo = -(m_pion*AB).znorm();          // wektor jednostkowy w lewo od wektora AB i prostopadle do pionu
					m_wprzod = m_wlewo*m_pion;
					m_dlugosc = -AB^m_wprzod;
					m_szerokosc = prz->param_f[0];

					Ap = B - m_wlewo*m_szerokosc / 2;
					Bp = A - m_wlewo*m_szerokosc / 2; 
					Ap_y = B.y;
					Bp_y = A.y;
					break;
				case 3:
					m_wlewo = AB.znorm();          // wektor jednostkowy w lewo od wektora AB i prostopadle do pionu
					m_wprzod = m_wlewo*m_pion;
					m_dlugosc = prz->param_f[0];
					m_szerokosc = AB^m_wlewo;

					Ap = A - m_wprzod*m_dlugosc / 2;
					Bp = A + m_wprzod*m_dlugosc / 2;
					Ap_y = Bp_y = A.y;
					break;
				}

				//Vector3 Al = A + m_wlewo*m_szerokosc / 2, Bl = B + m_wlewo*m_szerokosc / 2; // rzut odcinka AB na lew¹ œcianê muru
				Vector3 RR = intersection_point_between_line_and_plane(vPos_prev, state.vPos, -m_wlewo, Ap);  // punkt przeciêcia wektora ruchu ze œcian¹ praw¹
				Vector3 QQ = projection_of_point_on_plain(state.vPos, m_wlewo, Ap);  // rzut prostopad³y œrodka pojazdu na p³aszczyznê prawej œciany
				float odl_zezn_prawa = (QQ - state.vPos) ^ m_wlewo;        // odleg³oœæ ze znakiem œr.pojazdu od p³. prawej: dodatnia jeœli vPos nie przekracza œciany
				//float odl_dokladna = distance_from_point_to_segment(vPos, Ap, Bp);
				float cos_kata_pad = wWR_zn^m_wlewo;       // kosinus k¹ta wekt.ruchu w stos. do normalnej œciany (dodatni, gdy pojazd zmierza do œciany od prawej strony)
				//Vector3 Ap_granica_kol = Ap - m_wprzod*radius / cos_kata_pad, Bp_granica_kol = Bp + m_wprzod*radius / cos_kata_pad; // odcinek ApBp powiêkszony z obu stron 

				bool czy_poprzednio_obiekt_przed_sciana = (((RR - vPos_prev) ^ m_wlewo) > 0); // czy w poprzednim cyklu œrodek poj. znajdowa³ siê przed œcian¹
				bool czy_QQ_na_scianie = ((QQ - Ap).x*(QQ - Bp).x + (QQ - Ap).z*(QQ - Bp).z < 0);
				bool czy_RR_na_scianie = ((RR - Ap).x*(RR - Bp).x + (RR - Ap).z*(RR - Bp).z < 0);

				if ((czy_poprzednio_obiekt_przed_sciana) && (cos_kata_pad > 0) &&
					((odl_zezn_prawa >= 0) && (odl_zezn_prawa < radius*0.8) && (czy_QQ_na_scianie) ||   // gdy obiekt lekko nachodzi na œcianê
					(odl_zezn_prawa < 0) && (czy_RR_na_scianie))                                     // gdy obiekt ,,przelecia³'' przez œcianê w ci¹gu cyklu
					)
				{
					float czy_w_przod = ((wWR_zn^m_wprzod) > 0);                    // czy sk³¹dowa pozioma wektora ruchu skierowana w przód 
					Vector3 SS = RR - m_wprzod*(2 * czy_w_przod - 1)*radius / cos_kata_pad;   // punkt styku obiektu ze œcian¹
					float odl_od_A = fabs((SS - Ap) ^ m_wprzod);     // odleg³oœæ od punktu A
					float wysokosc_muru_wS = Ap_y + (Bp_y - Ap_y)*odl_od_A / m_dlugosc + m_wysokosc / 2;  // wysokoœæ muru w punkcie styku
					//bool czy_wysokosc_kol = ((SS - Ap).y - this->height / 2 < wysokosc_muru_wS);  // test wysokoœci - czy kolizja
					bool czy_wysokosc_kol = ((SS.y - this->height / 2 < wysokosc_muru_wS) &&  // test wysokoœci od góry
						(SS.y + this->height / 2 > wysokosc_muru_wS - m_wysokosc));  // od do³u
					if (czy_wysokosc_kol)
					{
						float V_prost_pop = state.vV^m_wlewo;
						state.vV = state.vV - m_wlewo*V_prost_pop*(1 + elasticity);       // zamiana kierunku sk³adowej prêdkoœci prostopad³ej do œciany 

						// mo¿emy te¿ czêœæ energii kinetycznej 

						// cofamy te¿ obiekt przed œcianê, tak jakby siê od niej odbi³ z ustalon¹ sprê¿ystoœci¹:
						float position_correction = radius*0.8 - odl_zezn_prawa;
						state.vPos = state.vPos - m_wlewo* position_correction*(1 + elasticity);

						//fprintf(f, "poj. w scianie, cos_kat = %f, w miejscu %f, Vpop = %f, V = %f\n", cos_kata_pad, odl_od_A / m_dlugosc,
						//	V_prost_pop, vV^m_wlewo);
						if_collision_handled = true;
						break;    // wyjœcie z pêtli po œcianach, by nie traciæ czasu na dalsze sprawdzanie
					}
					else
					{
						int x = 1;
					}
				} // jeœli wykryto kolizjê ze œcian¹ w przestrzeni 2D
			} // po œcianach

			if (if_collision_handled == false)  // jeœli kolizja nie zosta³a jeszcze zrealizowana, sprawdzam kolizjê ze œcian¹ doln¹
			{
				m_wlewo = (m_pion*AB).znorm();
				m_szerokosc = prz->param_f[0];
				Vector3 N = (AB*m_wlewo).znorm();    // normal_vector do dolnej œciany (zwrot do wewn¹trz)
				Vector3 Ad = A - m_pion*(m_wysokosc / 2), Bd = B - m_pion*(m_wysokosc / 2); // rzuty pionowe punktów A i B na doln¹ p³aszczyznê 
				Vector3 RR = intersection_point_between_line_and_plane(vPos_prev, state.vPos, -N, Ad);  // punkt przeciêcia wektora ruchu ze œcian¹ praw¹
				Vector3 RRR = projection_of_point_on_line(RR, Ad, Bd);
				float odl_RR_od_RRR = (RR - RRR).length();
				Vector3 QQ = projection_of_point_on_plain(state.vPos, -N, Ad);  // rzut prostopad³y œrodka pojazdu na p³aszczyznê prawej œciany
				float odl_zezn_prawa = (QQ - state.vPos) ^ N;        // odleg³oœæ ze znakiem œr.pojazdu od p³aszczyzny: dodatnia jeœli vPos nie przekracza œciany
				float odl_praw = (QQ - state.vPos).length();
				float x = (Ad - state.vPos) ^ N;
				float xx = (RR - state.vPos) ^ wWR_zn;
				Vector3 QQQ = projection_of_point_on_line(QQ, Ad, Bd);  
				float odl_QQ_od_QQQ = (QQ - QQQ).length();
				//float odl_dokladna = distance_from_point_to_segment(vPos, Ap, Bp);
				float cos_kata_pad = wWR_zn^N;       // kosinus k¹ta wekt.ruchu w stos. do normalnej œciany (dodatni, gdy pojazd zmierza do œciany od prawej strony)
				//Vector3 Ap_granica_kol = Ap - m_wprzod*radius / cos_kata_pad, Bp_granica_kol = Bp + m_wprzod*radius / cos_kata_pad; // odcinek ApBp powiêkszony z obu stron 

				bool czy_poprzednio_obiekt_przed_sciana = (((RR - vPos_prev) ^ N) > 0); // czy w poprzednim cyklu œrodek poj. znajdowa³ siê przed œcian¹

				bool czy_QQ_na_scianie = ((QQ - Ad).x*(QQ - Bd).x + (QQ - Ad).z*(QQ - Bd).z < 0) && (odl_QQ_od_QQQ < m_szerokosc / 2);
				bool czy_RR_na_scianie = ((RR - Ad).x*(RR - Bd).x + (RR - Ad).z*(RR - Bd).z < 0) && (odl_RR_od_RRR < m_szerokosc / 2);


				//fprintf(f, "nr. cyklu = %d, odl_zezn_prawa = %f, x = %f, QQ = (%f,%f,%f), N = (%f,%f,%f), Ad = (%f,%f,%f), vPos = (%f,%f,%f)\n", 
				//	terrain->number_of_displays, odl_zezn_prawa, x, QQ.x, QQ.y, QQ.z, N.x, N.y, N.z, Ad.x, Ad.y, Ad.z, vPos.x, vPos.y, vPos.z);
				//fprintf(f, "nr. cyklu = %d, odl_zezn_prawa = %f, odl do RR = %f, cos_kata_pad = %f\n",
				//	terrain->number_of_displays, odl_zezn_prawa, xx, cos_kata_pad);
				//fprintf(f, "vPos.y = %f, Ad.y = %f\n", vPos.y, Ad.y);


				if ((czy_poprzednio_obiekt_przed_sciana) && (cos_kata_pad > 0) &&
					((odl_zezn_prawa >= 0) && (odl_zezn_prawa < radius*0.8) && (czy_QQ_na_scianie) ||   // gdy obiekt lekko nachodzi na œcianê
					(odl_zezn_prawa < 0) && (czy_RR_na_scianie))                                     // gdy obiekt ,,przelecia³'' przez œcianê w ci¹gu cyklu
					)
				{
					float V_prost_pop = state.vV^N;
					state.vV = state.vV - N*V_prost_pop*(1 + elasticity);       // zamiana kierunku sk³adowej prêdkoœci prostopad³ej do œciany 
					float position_correction = radius*0.8 - odl_zezn_prawa;
					state.vPos = state.vPos - N* position_correction*(1 + elasticity);
					//fclose(f);
					if_collision_handled = true;
				}
				

			}

		}  // jeœli mur
	} // for po przedmiotach w radiusiu


	// sprawdzam, czy nie najecha³em na monetê lub beczkê z paliwem. 
	// zak³adam, ¿e w jednym cylku symulacji mogê wzi¹æ maksymalnie jeden przedmiot
	for (long i = 0; i < number_of_items_in_radius; i++)
	{
		Item *prz = item_tab_pointer[i];

		if ((prz->to_take == 1) &&
			((prz->vPos - state.vPos + Vector3(0, state.vPos.y - prz->vPos.y, 0)).length() < radius))
		{
			float odl_nasza = (prz->vPos - state.vPos + Vector3(0, state.vPos.y - prz->vPos.y, 0)).length();

			long value = prz->value;
			taking_value = -1;

			if (prz->type == ITEM_COIN)
			{
				bool if_can_take = false;
				// przy du¿ej wartoœci nie mogê samodzielnie podnieœæ pieni¹¿ka bez bratniej pomocy innego pojazdu
				// odleg³oœæ bratniego pojazdu od pieni¹dza nie mo¿e byæ mniejsza od naszej odleg³oœci, gdy¿ wtedy
				// to ten another pojazd zgarnie monetê:
				if (value >= 1000)
				{
					bool helpmate = false;
					int who_helped = -1;
					for (long k = 0; k < number_of_objects_in_radius; k++)
					{
						MovableObject *another = object_tab_pointer[k];
						float mate_dist = (another->state.vPos - prz->vPos).length();
						if ((another->iID != iID) && (mate_dist < another->radius * 2) && (odl_nasza < mate_dist))
						{
							helpmate = true;
							who_helped = another->iID;
						}
					}

					if (!helpmate)
					{
						sprintf(par_view.inscription1, "nie_mozna_wziac_tak_ciezkiego_pieniazka,_chyba_ze_ktos_podjedzie_i_pomoze.");
						if_can_take = false;
					}
					else
					{
						sprintf(par_view.inscription1, "pojazd_o_ID_%d_pomogl_wziac_monete_o_wartosci_%d", who_helped, value);
						if_can_take = true;
					}
				}
				else
					if_can_take = true;

				if (if_can_take)
				{
					taking_value = (float)value*money_collection_skills;
					state.money += (long)taking_value;
				}

				//sprintf(inscription2,"Wziecie_gotowki_o_wartosci_ %d",value);
			} // jeœli to ITEM_COIN
			else if (prz->type == ITEM_BARREL)
			{
				taking_value = (float)value*fuel_collection_skills;
				state.amount_of_fuel += taking_value;
				//sprintf(inscription2,"Wziecie_paliwa_w_ilosci_ %d",value);
			}

			if (taking_value > 0)
			{
				prz->to_take = 0;
				prz->if_taken_by_me = 1;
				prz->taking_time = time_of_simulation;

				// SaveMapToFile informacji, by przekazaæ j¹ innym aplikacjom:
				number_of_taking_item = prz->index;
			}
		}
	} // po przedmiotach w radiusiu


	// Zamiast listowaæ ca³¹ tablicê przedmiotów mo¿na by zrobiæ listê wziêtych przedmiotów
	for (long i = 0; i < terrain->number_of_items; i++)
	{
		Item *prz = &terrain->p[i];
		if ((prz->to_take == 0) && (prz->if_taken_by_me) && (prz->if_renewable) &&
			(time_of_simulation - prz->taking_time >= terrain->time_of_item_renewing))
		{                              // jeœli min¹³ pewnien okres czasu przedmiot mo¿e zostaæ przywrócony
			prz->to_take = 1;
			prz->if_taken_by_me = 0;
			number_of_renewed_item = i;
		}
	}



	// kolizje z innymi obiektami
	if (iID_collider == iID) // ktoœ o numerze iID_collider wykry³ kolizjê z naszym pojazdem i poinformowa³ nas o tym

	{
		//fprintf(f,"ktos wykryl kolizje - modyf. predkosci\n",iID_collider);
		state.vV = state.vV + vdV_collision;   // modyfikuje prêdkoœæ o wektor obliczony od drugiego (¿yczliwego) uczestnika
		iID_collider = -1;

	}
	else
	{
		for (long i = 0; i < number_of_objects_in_radius; i++)
		{
			MovableObject *another = object_tab_pointer[i];

			if ((state.vPos - another->state.vPos).length() < radius + another->radius)  // jeœli kolizja 
			{
				// zderzenie takie jak w symulacji kul 
				Vector3 norm_pl_st = (state.vPos - another->state.vPos).znorm();    // normal_vector do p³aszczyzny stycznej - direction odbicia
				float m1 = state.mass_total, m2 = another->state.mass_total;          // masy obiektów
				float W1 = state.vV^norm_pl_st, W2 = another->state.vV^norm_pl_st;  // wartosci prêdkoœci
				if (W2 > W1)      // jeœli obiekty siê przybli¿aj¹
				{

					float Wns = (m1*W1 + m2*W2) / (m1 + m2);        // prêdkoœæ po zderzeniu ca³kowicie niesprê¿ystym
					float W1s = ((m1 - m2)*W1 + 2 * m2*W2) / (m1 + m2), // prêdkoœæ po zderzeniu ca³kowicie sprê¿ystym
						W2s = ((m2 - m1)*W2 + 2 * m1*W1) / (m1 + m2);
					float W1sp = Wns + (W1s - Wns)*elasticity;    // prêdkoœæ po zderzeniu sprê¿ysto-plastycznym
					float W2sp = Wns + (W2s - Wns)*elasticity;

					state.vV = state.vV + norm_pl_st*(W1sp - W1);    // poprawka prêdkoœci (zak³adam, ¿e another w przypadku drugiego obiektu zrobi to jego w³asny symulator) 
					iID_collider = another->iID;
					vdV_collision = norm_pl_st*(W2sp - W2);
					//fprintf(f,"wykryto i zreal. kolizje z %d W1=%f,W2=%f,W1s=%f,W2s=%f,m1=%f,m2=%f\n",iID_collider,W1,W2,W1s,W2s,m1,m2); 
				}
				//if (fabs(W2 - W1)*dt < (vPos - another->vPos).length() < 2*radius) vV = vV + norm_pl_st*(W1sp-W1)*2;
			}
		}
	} // do else
	delete item_tab_pointer;
	delete object_tab_pointer;

}



void MovableObject::DrawObject()
{
	glPushMatrix();

	glTranslatef(state.vPos.x, state.vPos.y + clearance, state.vPos.z);

	quaternion k = state.qOrient.AsixAngle();
	//fprintf(f,"quaternion = [%f, %f, %f], w = %f\n",qOrient.x,qOrient.y,qOrient.z,qOrient.w);
	//fprintf(f,"os obrotu = [%f, %f, %f], kat = %f\n",k.x,k.y,k.z,k.w*180.0/PI);

	glRotatef(k.w*180.0 / PI, k.x, k.y, k.z);
	glPushMatrix();
	glTranslatef(-length / 2, -height / 2, -width / 2);

	glScalef(length, height, width);
	glCallList(Cube);
	glPopMatrix();
	if (this->if_selected)
	{
		float w = 1.1;
		glTranslatef(-length / 2 * w, -height / 2 * w, -width / 2 * w);
		glScalef(length*w, height*w, width*w);
		GLfloat Surface[] = { 2.0f, 0.0f, 0.0f, 1.0f };
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
		glCallList(Cube_skel);
	}

	GLfloat Surface[] = { 2.0f, 2.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
	glRasterPos2f(0.30, 1.20);
	glPrint("%d", iID);
	glPopMatrix();
}





Sector::Sector(int _numcells, long _w, long _k, bool if_map)
{
	w = _w; k = _k;
	number_of_movable_objects = 0;
	number_of_movable_objects_max = 10;
	mov_obj = new MovableObject*[number_of_movable_objects_max];
	number_of_items = 0;
	number_of_items_max = 10;
	wp = new Item*[number_of_items_max];
	number_of_cells = _numcells;
	type_of_surface = NULL;
	map_of_heights = NULL;
	normal_vectors = NULL;
	level_of_water = NULL;
	if (if_map)
	{
		memory_for_map(number_of_cells,false,true);  // mapa wysokoœci + normalne + type_of_surfaceierzchni
		this->number_of_cells_displayed = number_of_cells;
		number_of_cells_displayed_prev = number_of_cells;
	}
	else
	{
		this->number_of_cells_displayed = 1;
		number_of_cells_displayed_prev = 1;
	}
	default_type_of_surface = 0;              // parametry standardowe dla ca³ego sektora brane pod uwagê gdy nie ma dok³adnej mapy
	default_height = 0;
	default_level_of_water = -1e10;

	map_of_heights_in_edit = NULL;
	normal_vectors_in_edit = NULL;
	type_of_surface_in_edit = NULL;
	level_of_water_in_edit = NULL;
	//fprintf(f, "Konstruktor: utworzono Sector w = %d, k = %d, mapa = %d\n", w, k, map_of_heights);
	this->height_max = -1e10;
}

Sector::~Sector()
{
	delete mov_obj;  // to zawsze jest
	delete wp;   // to te¿

	// natomiast to ju¿ nie zawsze:
	if (map_of_heights)
		memory_release ();
	//fprintf(f, "Destruktor: Usunieto Sector w = %d, k = %d\n", w, k);
}

// pamiêæ dla mapy. na wejœciu liczba komórek mapy (potêga 2) oraz informacja czy mapa jest w trybie edycji
void Sector::memory_for_map(int __number_of_cells, bool if_edit, bool if_map)
{
	float **__map_of_heights = NULL;
	//int ***__type_of_surface = NULL;
	//float ***__level_of_water = NULL;
	Vector3**** __Norm = NULL;

	if (if_map)
	{
		__map_of_heights = new float*[__number_of_cells * 2 + 1];
		for (int n = 0; n < __number_of_cells * 2 + 1; n++)
		{
			__map_of_heights[n] = new float[__number_of_cells + 1];
		}
		int liczba_rozdzielczosci = 1 + log2(__number_of_cells);         // liczba map o ró¿nych rozdzielczoœciach a¿ do mapy 1x1 (jedno oczko, 4 normalne)
		__Norm = new Vector3***[liczba_rozdzielczosci];
		for (int rozdz = 0; rozdz < liczba_rozdzielczosci; rozdz++)
		{
			long loczek = __number_of_cells / (1 << rozdz);
			__Norm[rozdz] = new Vector3**[loczek];
			for (int k = 0; k < loczek; k++)
			{
				__Norm[rozdz][k] = new Vector3*[loczek];
				for (int n = 0; n < loczek; n++)
					__Norm[rozdz][k][n] = new Vector3[4];
			}
		}
	}

	if (if_edit)
	{
		map_of_heights_in_edit = __map_of_heights;
		normal_vectors_in_edit = __Norm;
		number_of_cells_in_edit = __number_of_cells;
	}
	else
	{
		map_of_heights = __map_of_heights;
		normal_vectors = __Norm;
		number_of_cells = __number_of_cells;
	}
}

void Sector::memory_for_surf(bool if_edit)
{
	int ***__type_of_surface = NULL;
	__type_of_surface = new int**[number_of_cells];
	for (int k = 0; k < number_of_cells; k++)
	{
		__type_of_surface[k] = new int*[number_of_cells];
		for (int n = 0; n < number_of_cells; n++)
		{
			__type_of_surface[k][n] = new int[4];
			for (int m = 0; m < 4; m++)
				__type_of_surface[k][n][m] = 0;
		}
	}

	if (if_edit)
	{
		type_of_surface_in_edit = __type_of_surface;
	}
	else
	{
		type_of_surface = __type_of_surface;
	}
}

void Sector::memory_for_water(bool if_edit)
{
	float ***__level_of_water = NULL;

	__level_of_water = new float**[number_of_cells];
	for (int k = 0; k < number_of_cells; k++)
	{
		__level_of_water[k] = new float*[number_of_cells];
		for (int n = 0; n < number_of_cells; n++)
		{
			__level_of_water[k][n] = new float[4];   // 4 trójk¹ty w komórce
			for (int m = 0; m < 4; m++)
				__level_of_water[k][n][m] = -1e10;
		}
	}

	if (if_edit)
	{
		level_of_water_in_edit = __level_of_water;
	}
	else
	{
		level_of_water = __level_of_water;
	}
}


// uwolnienie pamiêci dla sektora
void Sector::memory_release (bool if_edit)
{
	float **mapa = map_of_heights;
	Vector3 ****__N = normal_vectors;
	int ***__type_of_surface = type_of_surface;
	float ***__level_of_water = level_of_water;
	long loczek = number_of_cells;

	if (if_edit)
	{
		mapa = map_of_heights_in_edit;
		__N = normal_vectors_in_edit;
		__type_of_surface = type_of_surface_in_edit;
		__level_of_water = level_of_water_in_edit;
		loczek = number_of_cells_in_edit;
		map_of_heights_in_edit = NULL;
		normal_vectors_in_edit = NULL;
		type_of_surface_in_edit = NULL;
		level_of_water_in_edit = NULL;
	}
	else
	{
		map_of_heights = NULL;
		normal_vectors = NULL;
		type_of_surface = NULL;
		level_of_water = NULL;
	}

	if (mapa)
	{
		for (int ww = 0; ww < loczek * 2 + 1; ww++)
			delete mapa[ww];
		delete mapa;

		long liczba_rozdzielczosci = (loczek > 0 ? 1 + log2(loczek) : 1);
		for (int rozdz = 0; rozdz < liczba_rozdzielczosci; rozdz++)
		{
			for (int i = 0; i < loczek / (1 << rozdz); i++)
			{
				for (int j = 0; j < loczek / (1 << rozdz); j++)
					delete __N[rozdz][i][j];
				delete __N[rozdz][i];
			}
			delete __N[rozdz];
		}
		delete __N;
	}

	if (__type_of_surface)
	{
		for (int i = 0; i < loczek; i++)
		{
			for (int j = 0; j < loczek; j++)
				delete __type_of_surface[i][j];
			delete __type_of_surface[i];
		}
		delete __type_of_surface;
	}

	if (__level_of_water)
	{

		for (int i = 0; i < loczek; i++)
		{
			for (int j = 0; j < loczek; j++)
				delete __level_of_water[i][j];
			delete __level_of_water[i];
		}
		delete __level_of_water;	
	}
}

void Sector::insert_item(Item *p)
{
	if (number_of_items == number_of_items_max) // powiekszenie tablicy
	{
		Item **wp_nowe = new Item*[2 * number_of_items_max];
		for (long i = 0; i < number_of_items; i++) wp_nowe[i] = wp[i];
		delete wp;
		wp = wp_nowe;
		number_of_items_max = 2 * number_of_items_max;
	}
	wp[number_of_items] = p;
	number_of_items++;
}

void Sector::remove_item(Item *p)
{
	for (long i = 0; i < this->number_of_items;i++)
		if (this->wp[i] == p){
			wp[i] = wp[number_of_items-1];
			number_of_items--;
			break;
		}
}

void Sector::insert_movable_object(MovableObject *o)
{
	if (number_of_movable_objects == number_of_movable_objects_max)
	{
		MovableObject **wob_nowe = new MovableObject*[2 * number_of_movable_objects_max];
		for (long i = 0; i < number_of_movable_objects; i++) wob_nowe[i] = mov_obj[i];
		delete mov_obj;
		mov_obj = wob_nowe;
		number_of_movable_objects_max = 2 * number_of_movable_objects_max;
	}
	mov_obj[number_of_movable_objects] = o;
	number_of_movable_objects++;
}
void Sector::release_movable_object(MovableObject *o)
{
	for (long i = number_of_movable_objects - 1; i >= 0; i--)
		if (mov_obj[i] == o){
			mov_obj[i] = mov_obj[number_of_movable_objects - 1];
			number_of_movable_objects--;
			break;
		}
}
// obliczenie wektorów normalnych N do p³aszczyzn trójk¹tów, by nie robiæ tego ka¿dorazowo przy odrysowywaniu
void Sector::calculate_normal_vectors(float sector_size, bool if_edit)
{
	float **mapa = map_of_heights;
	Vector3 ****__Norm = normal_vectors;
	long loczek = number_of_cells;

	if (if_edit){
		mapa = map_of_heights_in_edit;
		__Norm = normal_vectors_in_edit;
		loczek = number_of_cells_in_edit;
	}

	if (mapa)
	{
		long liczba_rozdzielczosci = (loczek > 0 ? 1 + log2(loczek) : 1);              // gdy podst. rozdzielczoœæ 16x16, mamy 5 rozdzielczoœci:
		                                                            // 16x16, 8x8, 4x4, 2x2, 1x1
		for (int rozdz = 0; rozdz < liczba_rozdzielczosci; rozdz++)
		{

			//fprintf(f, "znaleziono Sector o w = %d, k = %d\n", sek->w, sek->k);
			long zmn = (1 << rozdz);             // zmniejszenie rozdzielczoœci (potêga dwójki)
			long loczek_rozdz = loczek / zmn;    // liczba oczek w danym wariancie rozdzielczoœci

			float rozmiar_pola = (loczek > 0 ? sector_size / loczek : 1);        // size pola w danym wariancie rozdzielczoœci

			// tworze listê wyœwietlania rysuj¹c poszczególne pola mapy za pomoc¹ trójk¹tów 
			// (po 4 trójk¹ty na ka¿de pole):
			enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };

			Vector3 A, B, C, D, E, N;

			for (long w = 0; w < loczek_rozdz; w++)
				for (long k = 0; k < loczek_rozdz; k++)
				{
					A = Vector3(k*rozmiar_pola, mapa[w * 2 * zmn][k*zmn], w*rozmiar_pola);
					B = Vector3((k + 0.5)*rozmiar_pola, mapa[(w * 2 + 1)*zmn][k*zmn+(zmn>1)*zmn/2], (w + 0.5)*rozmiar_pola);
					C = Vector3((k + 1)*rozmiar_pola, mapa[w * 2 * zmn][(k + 1)*zmn], w*rozmiar_pola);
					D = Vector3(k*rozmiar_pola, mapa[(w + 1) * 2 * zmn][k*zmn], (w + 1)*rozmiar_pola);
					E = Vector3((k + 1)*rozmiar_pola, mapa[(w + 1) * 2 * zmn][(k + 1)*zmn], (w + 1)*rozmiar_pola);

					// tworzê trójk¹t ABC w górnej czêœci kwadratu: 
					//  A o_________o C
					//    |.       .|
					//    |  .   .  | 
					//    |    o B  | 
					//    |  .   .  |
					//    |._______.|
					//  D o         o E

					Vector3 AB = B - A;
					Vector3 BC = C - B;
					N = (AB*BC).znorm();
					//d[w][k][ABC] = -(B^N);          // dodatkowo wyznaczam wyraz wolny z równania plaszyzny trójk¹ta
					__Norm[rozdz][w][k][ABC] = N;          // dodatkowo zapisujê normaln¹ do p³aszczyzny trójk¹ta
					// trójk¹t ADB:
					Vector3 AD = D - A;
					N = (AD*AB).znorm();
					//d[w][k][ADB] = -(B^N);
					__Norm[rozdz][w][k][ADB] = N;
					// trójk¹t BDE:
					Vector3 BD = D - B;
					Vector3 DE = E - D;
					N = (BD*DE).znorm();
					//d[w][k][BDE] = -(B^N);
					__Norm[rozdz][w][k][BDE] = N;
					// trójk¹t CBE:
					Vector3 CB = B - C;
					Vector3 BE = E - B;
					N = (CB*BE).znorm();
					//d[w][k][CBE] = -(B^N);
					__Norm[rozdz][w][k][CBE] = N;
				}
		} // po rozdzielczoœciach (od najwiêkszej do najmniiejszej
	} // jeœli znaleziono mapê sektora


	// dodatkowo wyznaczana jest maksymalna wysokoœæ:
	if (mapa)
	{
		height_max = -1e10;
		for (long w = 0; w < loczek; w++)
			for (long k = 0; k < loczek; k++)
				if (mapa[w * 2][k] > height_max)
					height_max = mapa[w * 2][k];
	}
	else
		height_max = 0;
}

SectorsHashTable::SectorsHashTable()
{
	number_of_cells = 1000;
	general_number_of_sectors = 0;
	cells = new HashTableCell[number_of_cells];
	for (long i = 0; i < number_of_cells; i++)
	{
		cells[i].number_of_sectors = 0;
		cells[i].number_of_sectors_max = 0;
		cells[i].sectors = NULL;
	}
	// zainicjowanie indeksów skrajnych sektorów (w celu u³atwienia wyszukiwania istniej¹cych sektorów): 
	w_min = 1000000;
	w_max = -1000000;
	k_min = 1000000;
	k_max = -1000000;
}

SectorsHashTable::~SectorsHashTable()
{
	for (long i = 0; i < number_of_cells; i++)
		delete cells[i].sectors;
	delete cells;
}

 // wyznaczanie indeksu komórki na podstawie wspó³rzêdnych sektora
unsigned long SectorsHashTable::create_key(long w, long k)                 
{
	unsigned long kl = abs((w^k + w*(w + 1)*k + k*(k - 1)*w)) % number_of_cells;
	//if (kl < 0) kl = -kl;

	return kl;
}

// znajdowanie sektora (zwraca NULL jeœli nie znaleziono)
Sector *SectorsHashTable::find(long w, long k)       
{
	unsigned long klucz = create_key(w, k);
	Sector *sector = NULL;
	//fprintf(f, "  znajdowanie sektora - klucz = %d, jest to %d sektorow\n", klucz, cells[klucz].number_of_sectors);
	for (long i = 0; i < cells[klucz].number_of_sectors; i++)
	{
		Sector *s = cells[klucz].sectors[i];
		//fprintf(f, "    i=%d, w=%d, k=%d\n", i, s->w, s->k);
		if ((s->w == w) && (s->k == k)) {
			sector = s;
			break;
		}
	}
	return sector;
}

// wstawianie sektora do tablicy
Sector *SectorsHashTable::insert(Sector *s)      
{
	general_number_of_sectors++;

	if (general_number_of_sectors > number_of_cells) // reorganizacja tablicy (by nie by³o zbyt du¿o konfliktów)
	{
		long stara_liczba_komorek = number_of_cells;
		HashTableCell *stare_komorki = cells;
		number_of_cells = stara_liczba_komorek * 2;
		cells = new HashTableCell[number_of_cells];
		for (long i = 0; i < number_of_cells; i++)
		{
			cells[i].number_of_sectors = 0;
			cells[i].number_of_sectors_max = 0;
			cells[i].sectors = NULL;
		}
		for (long i = 0; i < stara_liczba_komorek; i++)
		{
			for (long j = 0; j < stare_komorki[i].number_of_sectors; j++)
			{
				general_number_of_sectors--;
				insert(stare_komorki[i].sectors[j]);
			}
			delete stare_komorki[i].sectors;
		}
		delete stare_komorki;
	}

	long w = s->w, k = s->k;
	long klucz = create_key(w, k);
	long liczba_sekt = cells[klucz].number_of_sectors;
	if (liczba_sekt >= cells[klucz].number_of_sectors_max)     // jesli brak pamieci, to nale¿y j¹ powiêkszyæ
	{
		long nowy_rozmiar = (cells[klucz].number_of_sectors_max == 0 ? 1 : cells[klucz].number_of_sectors_max * 2);
		Sector **sektory2 = new Sector*[nowy_rozmiar];
		for (long i = 0; i < cells[klucz].number_of_sectors; i++) sektory2[i] = cells[klucz].sectors[i];
		delete cells[klucz].sectors;
		cells[klucz].sectors = sektory2;
		cells[klucz].number_of_sectors_max = nowy_rozmiar;
	}
	cells[klucz].sectors[liczba_sekt] = s;
	cells[klucz].number_of_sectors++;

	if (w_min > w) w_min = w;
	if (w_max < w) w_max = w;
	if (k_min > k) k_min = k;
	if (k_max < k) k_max = k;

	return cells[klucz].sectors[liczba_sekt];           // zwraca wskaŸnik do nowoutworzonego sektora
}

void SectorsHashTable::remove(Sector *s)
{
	long w = s->w, k = s->k;
	long klucz = create_key(w, k);
	long liczba_sekt = cells[klucz].number_of_sectors;
	long index = -1;
	for (long i = 0; i < liczba_sekt; i++)
	{
		Sector *ss = cells[klucz].sectors[i];
		if ((ss->w == w) && (ss->k == k)) {
			index = i;
			break;
		}
	}
	if (index > -1){
		cells[klucz].sectors[index] = cells[klucz].sectors[liczba_sekt - 1];
		cells[klucz].number_of_sectors--;
	}
}
//**********************
//   Obiekty nieruchome
//**********************
Terrain::Terrain()
{
	bool czy_z_pliku = 1;
    detail_level = 0.7;                 // stopieñ szczegó³owoœci wyœwietlania przedmiotów i powierzchni terrainu (1 - pe³na, 0 - minimalna)
	number_of_displays = 0;
	ts = new SectorsHashTable();
	p = NULL;

	number_of_selected_items_max = 100;  // to nie oznacza, ¿e tyle tylko mo¿na zaznaczyæ przedmiotów, ale ¿e tyle jest miejsca w tablicy, która jest automatycznie powiêkszana
	selected_items = new long[number_of_selected_items_max];     // tablica zaznaczonych przedmiotów (dodatkowo ka¿dy przedmiot posiada pole z informacj¹ o zaznaczeniu)
	number_of_selected_items = 0;
	
	if (czy_z_pliku)
	{
		//char filename[] = "wyspa3.map";
		//char filename[] = "bramki_01.map";
		char filename[] = "stozki_5.map";
		//char filename[] = "teren.map";
		//char filename[] = "dolek_1.map";
		//char filename[] = "paralaksa3_3.map";
		//char filename[] = "dla_zyczliwych.map";
		int result = OpenMapFromFile(filename);
		if (result == -1)
		{
			char path[1024];
			sprintf(path, "..\\%s", filename);
			int result = OpenMapFromFile(path);
			if (result == -1)
			{
				sprintf(path, "..\\..\\%s", filename);
				int result = OpenMapFromFile(path);
				if (result == -1)
				{
					fprintf(f, "Cannot open terrain map!");
				}
			}
		}
		//SaveMapToFile(filename);
		//OpenMapFromFile("terrain.map");
	}
	else
	{
		sector_size = 480;         // d³ugoœæ boku kwadratu w [m]
		time_of_item_renewing = 120;
		if_toroidal_world = false;
		border_x = -1;
		border_z = -1;
		number_of_items = 0;
		number_of_items_max = 10;

		height_std = 0;
		level_of_water_std = -1e10;   
		
		p = new Item[number_of_items_max];
	}
	for (long i = 0; i < number_of_items; i++)
		if (p[i].type == ITEM_TREE)
		{
			//p[i].value = 20;
			//this->PlaceItemInTerrain(&p[i]);
		}

	//SaveMapToFile("mapa4");
	//time_of_item_renewing = 120;   // czas [s] po jakim przedmiot jest odnawiany
}



int Terrain::SaveMapToFile(char filename[])
{
	FILE *pl = fopen(filename, "wb");
	if (!pl)
	{
		printf("Nie dalo sie otworzyc pliku %s do zapisu!\n", filename);
		return 0;
	}
	long liczba_sekt = 0;
	for (long i = 0; i < ts->number_of_cells; i++)
	{
		for (long j = 0; j < ts->cells[i].number_of_sectors; j++)
			if (ts->cells[i].sectors[j]->map_of_heights) liczba_sekt++;
	}

	fwrite(&sector_size, sizeof(float), 1, pl);
	fwrite(&time_of_item_renewing, sizeof(float), 1, pl);
	fwrite(&if_toroidal_world, sizeof(bool), 1, pl);
	fwrite(&border_x, sizeof(float), 1, pl);
	fwrite(&border_z, sizeof(float), 1, pl);
	fwrite(&liczba_sekt, sizeof(long), 1, pl);
	fwrite(&height_std, sizeof(float), 1, pl);
	fwrite(&level_of_water_std, sizeof(float), 1, pl);
	fprintf(f, "jest %d sektorow z mapami wysokosciowych + opisem nawierzchni + poz.wody lub innymi informacjami \n", liczba_sekt);
	for (long i = 0; i < ts->number_of_cells; i++)
	{
		fprintf(f, "  w komorce %d tab.hash jest %d sektorow:\n", i, ts->cells[i].number_of_sectors);
		for (long j = 0; j < ts->cells[i].number_of_sectors; j++)
		{
			Sector *s = ts->cells[i].sectors[j];
			int if_map = (s->map_of_heights != 0 ? 1 : 0);
			int if_surf = 0;// (s->type_of_surface != 0 ? 1 : 0);
			//int if_water = 0;// (s->level_of_water != 0 ? 1 : 0);
			bool czy_ogolne_wartosci = (s->default_type_of_surface != 0) && (s->default_height != 0) && (s->default_level_of_water > -1e10);
			if (if_map || czy_ogolne_wartosci)
			{
				fwrite(&ts->cells[i].sectors[j]->w, sizeof(long), 1, pl);
				fwrite(&ts->cells[i].sectors[j]->k, sizeof(long), 1, pl);
				int loczek = ts->cells[i].sectors[j]->number_of_cells;
				fwrite(&loczek, sizeof(int), 1, pl);
				fwrite(&if_map, sizeof(int), 1, pl);
				fwrite(&if_surf, sizeof(int), 1, pl);
				//fwrite(&if_water, sizeof(int), 1, pl);
				if (if_map)
				{
					for (int wx = 0; wx < loczek * 2 + 1; wx++)
						fwrite(ts->cells[i].sectors[j]->map_of_heights[wx], sizeof(float), loczek + 1, pl);
					fprintf(f, "    sekt.%d z mapa, w = %d, k = %d, size = %d\n", j, ts->cells[i].sectors[j]->w,
						ts->cells[i].sectors[j]->k, loczek);
				}
				else
					fwrite(&ts->cells[i].sectors[j]->default_height, sizeof(float), 1, pl);
					

				if (if_surf)
				{
					for (int w = 0; w < loczek; w++)
						for (int k = 0; k < loczek; k++)
							fwrite(ts->cells[i].sectors[j]->type_of_surface[w][k], sizeof(int), 4, pl);
				}
				else 
					fwrite(&ts->cells[i].sectors[j]->default_type_of_surface, sizeof(int), 1, pl);
					
				/*if (if_water)
				{
					for (int w = 0; w < loczek; w++)
						for (int k = 0; k < loczek; k++)
							fwrite(ts->cells[i].sectors[j]->level_of_water[w][k], sizeof(int), 4, pl);
				}
				else
					fwrite(&ts->cells[i].sectors[j]->default_level_of_water, sizeof(float), 1, pl);
					*/  // zapis wody poprzez rozlewanie z przedmiotu typu POINT_OF_WATER

			} // jeœli jakaœ informacja jest w sektorze poza przedmiotami i obiektami ruchomymi
		} // po sektorach
	} // po komórkach tabl.hash
	fwrite(&number_of_items, sizeof(long), 1, pl);
	for (long i = 0; i < number_of_items; i++)
		fwrite(&p[i], sizeof(Item), 1, pl);
	long liczba_par_edycji_fald = 10;
	fwrite(&liczba_par_edycji_fald, sizeof(long), 1, pl);
	for (long i = 0; i < liczba_par_edycji_fald; i++)
		fwrite(&pf_rej[i], sizeof(FoldParams), 1,pl);

	fclose(pl);


	//fstream fbin;
	//fbin.open(filename.c_str(), ios::binary | ios::in | ios::out);

	return 1;
}



int Terrain::OpenMapFromFile(char filename[])
{
	FILE *pl = fopen(filename, "rb");
	if (!pl)
	{
		printf("Nie dalo sie otworzyc pliku %s do odczytu!\n", filename);
		fprintf(f, "Nie dalo sie otworzyc pliku %s do odczytu!\n", filename);
		return -1;
	}

	fread(&sector_size, sizeof(float), 1, pl);
	fread(&time_of_item_renewing, sizeof(float), 1, pl);
	fread(&if_toroidal_world, sizeof(bool), 1, pl);
	fread(&border_x, sizeof(float), 1, pl);
	fread(&border_z, sizeof(float), 1, pl);

	long _liczba_sekt = 0;        // czyli liczba sektorów 
	fread(&_liczba_sekt, sizeof(long), 1, pl);
	fread(&height_std, sizeof(float), 1, pl);
	fread(&level_of_water_std, sizeof(float), 1, pl);

	fprintf(f, "\nOdczyt danych o terrainie z pliku %s\n\n", filename);
	fprintf(f, "  size sektora = %f\n", sector_size);
	fprintf(f, "  czas odnowy przedmiotu = %f\n", time_of_item_renewing);
	fprintf(f, "  if_toroidal_world = %d\n", if_toroidal_world);
	fprintf(f, "  border_x = %f\n", border_x);
	fprintf(f, "  border_z = %f\n", border_z);
	fprintf(f, "  liczba sekt. = %d\n", _liczba_sekt);
	fprintf(f, "  std poziom wody = %f\n", level_of_water_std);

	for (long i = 0; i < _liczba_sekt; i++)
	{
		long w, k;
		int loczek, if_map = false, if_surf = false, if_water = false;
		fread(&w, sizeof(long), 1, pl);
		fread(&k, sizeof(long), 1, pl);
		fread(&loczek, sizeof(int), 1, pl);
		fread(&if_map, sizeof(int), 1, pl);      // czy dok³adna mapa
		fread(&if_surf, sizeof(int), 1, pl);     // czy typy powierzchni
		//fread(&if_water, sizeof(int), 1, pl);    // czy poziomy wody w poszczeólnych trójk¹tach
		//fprintf(f, "    mapa w=%d, k=%d, l.oczek = %d\n", w, k, loczek);

		Sector *o = new Sector(loczek, w, k, if_map);
		ts->insert(o);
		if (if_map)
		{
			for (int wx = 0; wx < loczek * 2 + 1; wx++)
				fread(o->map_of_heights[wx], sizeof(float), loczek + 1, pl);
			o->calculate_normal_vectors(sector_size);
		}
		else
			fread(&o->default_type_of_surface, sizeof(int), 1, pl);

		if (if_surf)
			for (int w = 0; w < loczek; w++)           // tak powinno byæ
				for (int k = 0; k < loczek; k++)
					fread(o->type_of_surface[w][k], sizeof(int), 4, pl);
		else
			fread(&o->default_type_of_surface, sizeof(int), 1, pl);

		//if (if_water)
		//	for (int w = 0; w < loczek; w++)
		//		for (int k = 0; k < loczek; k++)
		//			fread(o->level_of_water[w][k], sizeof(float), 4, pl);
		//else
		//	fread(&o->default_level_of_water, sizeof(float), 1, pl);
	}
	long _liczba_przedm = 0;
	fread(&_liczba_przedm, sizeof(long), 1, pl);
	fprintf(f, "Znaleziono w pliku %s przedmioty w liczbie %d\n", filename, _liczba_przedm);
	fclose(f);
	f = fopen("wzr_log.txt", "a");
	if (p) delete p;
	p = new Item[_liczba_przedm+10];

	for (long i = 0; i < _liczba_przedm; i++)
	{
		fread(&p[i], sizeof(Item), 1, pl);
		//fprintf(f, "  przedm.%d, type = %d, value = %f\n", i,p[i].type,p[i].value);
		fclose(f);
		f = fopen("wzr_log.txt", "a");
		p[i].index = i;
		InsertItemIntoSectors(&p[i]);
		PlaceItemInTerrain(&p[i]);
		if (p[i].if_selected) 
		{
			p[i].if_selected = 0;
			SelectUnselectItemOrGroup(i);
		}
		if ((p[i].type == ITEM_POINT) && (p[i].subtype == POINT_OF_WATER))
			InsertWater(p[i].vPos.x, p[i].vPos.z, p[i].value);

	}
	this->number_of_items = _liczba_przedm;
	this->number_of_items_max = _liczba_przedm + 10;

	long liczba_par_edycji_fald;
	fread(&liczba_par_edycji_fald, sizeof(long), 1, pl);
	for (long i = 0; i < liczba_par_edycji_fald; i++)
		fread(&pf_rej[i], sizeof(FoldParams), 1, pl);

	fprintf(f, "Koniec wczytywania danych o terrainie.\n");
	fclose(pl);
	return 1;
}

void Terrain::SectorCoordinates(long *w, long *k, float x, float z)  // na podstawie wsp. po³o¿enia punktu (x,z) zwraca wsp. sektora
{
	// punkt (x,z) = (0,0) odpowiada œrodkowi sektora (w,k) = (0,0)
	long _k, _w;
	if (x > 0)
		_k = (long)(x / sector_size + 0.5);
	else
	{
		float _kf = x / sector_size - 0.5;
		_k = (long)(_kf); // przy dodatnich rzutowanie obcina w dó³, przy ujemnych w górê!
		if ((long)_kf == _kf) _k++;
	}
	if (z > 0)
		_w = (long)(z / sector_size + 0.5);
	else
	{
		float _wf = z / sector_size - 0.5;
		_w = (long)(_wf);
		if ((long)_wf == _wf) _w++;
	}

	*w = _w; *k = _k;
}

void Terrain::SectorBeginPosition(float *x, float *z, long w, long k) // na podstawie wspó³rzêdnych sektora (w,k) 
{												              // zwraca po³o¿enie punktu pocz¹tkowego sektora (x,z)
	float _x = (k - 0.5)*sector_size,
		_z = (w - 0.5)*sector_size;

	(*x) = _x;
	(*z) = _z;
}

Terrain::~Terrain()
{
	for (long i = 0; i < ts->number_of_cells; i++)
		for (long j = 0; j < ts->cells[i].number_of_sectors; j++)
			delete ts->cells[i].sectors[j];

	delete ts;
	delete p;
}


// funkcja wstawia wodê o okreœlonym maksymalnym poziomie w zag³êbienie okreœlone punktem (x,z)
// poszukiwana jest poziomica o wysokoœci poziom_max przybli¿ona wielok¹tem. Jeœli taka poziomica
// nie tworzy cyklu wokó³ punktu (x,z), to szukamy poziomicy o jak najwy¿szym poziomie, która
// taki cykl tworzy i wype³niamy wod¹ wszystkie oczka le¿¹ce wewn¹trz wielok¹ta 
//
// Trzeba to przerobiæ na wersjê nierekurencyjn¹ w zwi¹zku z przepe³nieniami stosu przy bardziej skomplikowanych mapach 
void Terrain::InsertWater(float x, float z, float poziom_maks)
{
	struct CheckPoint
	{
		//long w, k;             // sector coordinates
		//long w_lok, k_lok;     // cell coordinates in sector
		//int triangle;          // nr of triangle to begin flowing water
		float x, z;
	};

	CheckPoint *curr_pts = new CheckPoint[1], *next_pts = new CheckPoint[1000]; // points of triangles (cells) which can be under water
	curr_pts[0].x = x; curr_pts[0].z = z;
	long curr_num = 1, next_num = 0;               // numbers of points in current and next iteration
	long next_mem_num = 1000;                      // amount of memory for new points



	while (curr_num > 0)         // jeœli s¹ trójk¹ty (komórki), w których trzeba sprawdziæ poziom wody
	{
		for (long ce = 0; ce < curr_num; ce++)  //po punktach w trójk¹tach(komórkach) w które mo¿e wlaæ siê woda
		{
			long w, k;                                   // wspó³rzêdne sektora 
			SectorCoordinates(&w, &k, curr_pts[ce].x, curr_pts[ce].z);     // wyznaczenie tych wspó³rzêdnych
			// wyszukanie sektora:
			Sector *s = ts->find(w, k);
			if (s && s->map_of_heights)
			{
				float **mapa = s->map_of_heights;

				long loczek = s->number_of_cells;
				float rozmiar_pola = (loczek > 0 ? (float)sector_size / loczek : 1);
				float x_pocz_sek, z_pocz_sek;             // wspó³rzêdne po³o¿enia pocz¹tku sektora
				SectorBeginPosition(&x_pocz_sek, &z_pocz_sek, w, k);
				float x_lok = curr_pts[ce].x - x_pocz_sek, z_lok = curr_pts[ce].z - z_pocz_sek;   // wspó³rzêdne lokalne wewn¹trz sektora
				long k_lok = (long)(x_lok / rozmiar_pola), w_lok = (long)(z_lok / rozmiar_pola);  // lokalne wspó³rzêdne pola w sektorze
				//  A o_________o C
				//    |.       .|
				//    |  .   .  | 
				//    |    o B  | 
				//    |  .   .  |
				//    |._______.|
				//  D o         o E
				float wysA = mapa[w_lok * 2][k_lok], wysC = mapa[w_lok * 2][k_lok + 1],
					wysD = mapa[(w_lok + 1) * 2][k_lok], wysE = mapa[(w_lok + 1) * 2][k_lok + 1];
				float wysB = mapa[w_lok * 2 + 1][k_lok];

				// sprawdzamy w który trójk¹t wstawiono wodê:
				Vector3 B = Vector3((k_lok + 0.5)*rozmiar_pola, mapa[w_lok * 2 + 1][k_lok], (w_lok + 0.5)*rozmiar_pola);
				enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };       // trójk¹t w którym znajduje siê punkt 
				int trojkat = 0;
				if ((B.x > x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = ADB;
				else if ((B.x < x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = CBE;
				else if (B.z > z_lok) trojkat = ABC;
				else trojkat = BDE;

				// sprawdzamy,czy przypadkiem nie odwiedziliœmy oczka od tej strony
				// jeœli tak, to przerywamy funkcjê, by nie kr¹¿yæ w pêtli:
				if ((s->level_of_water == NULL) || (s->level_of_water[w_lok][k_lok][trojkat] != poziom_maks))
				{
					//fprintf(f, "InsertWater: sekt(%d, %d), oczko (%d,%d), troj %d\n", w, k, w_lok, k_lok, trojkat);
					//if (s->level_of_water)
					//	fprintf(f, "   obecny poziom = %f\n", s->level_of_water[w_lok][k_lok][trojkat]);
					//fclose(f);
					//f = fopen("wzr_log.txt", "a");



					if (s->level_of_water == NULL)
					{
						s->memory_for_water(false);
					}

					// sprawdzamy przecieki wewnêtrzne w oczku (czy woda wycieka do innych trójk¹tów):
					int wyciek_wew[4];
					for (int ii = 0; ii < 4; ii++) wyciek_wew[ii] = false;

					if (wysB < poziom_maks)
						for (int ii = 0; ii < 4; ii++) wyciek_wew[ii] = true;
					else
					{
						if (trojkat == ABC)
						{
							if (wysA < poziom_maks) wyciek_wew[ADB] = true;
							if (wysC < poziom_maks) wyciek_wew[CBE] = true;
							if ((wysA < poziom_maks) || (wysC < poziom_maks)) wyciek_wew[ABC] = true;
							if (((wyciek_wew[ADB] == true) && (wysD < poziom_maks)) || ((wyciek_wew[CBE] == true) && (wysE < poziom_maks)))
								wyciek_wew[BDE] = true;
						}
						else if (trojkat == ADB)
						{
							if (wysA < poziom_maks) wyciek_wew[ABC] = true;
							if (wysD < poziom_maks) wyciek_wew[BDE] = true;
							if ((wysA < poziom_maks) || (wysD < poziom_maks)) wyciek_wew[ADB] = true;
							if (((wyciek_wew[ABC] == true) && (wysC < poziom_maks)) || ((wyciek_wew[BDE] == true) && (wysE < poziom_maks)))
								wyciek_wew[CBE] = true;
						}
						else if (trojkat == BDE)
						{
							if (wysD < poziom_maks) wyciek_wew[ADB] = true;
							if (wysE < poziom_maks) wyciek_wew[CBE] = true;
							if ((wysD < poziom_maks) || (wysE < poziom_maks)) wyciek_wew[BDE] = true;
							if (((wyciek_wew[ADB] == true) && (wysA < poziom_maks)) || ((wyciek_wew[CBE] == true) && (wysC < poziom_maks)))
								wyciek_wew[ABC] = true;
						}
						else if (trojkat == CBE)
						{
							if (wysC < poziom_maks) wyciek_wew[ABC] = true;
							if (wysE < poziom_maks) wyciek_wew[BDE] = true;
							if ((wysC < poziom_maks) || (wysE < poziom_maks)) wyciek_wew[CBE] = true;
							if (((wyciek_wew[ABC] == true) && (wysA < poziom_maks)) || ((wyciek_wew[BDE] == true) && (wysD < poziom_maks)))
								wyciek_wew[ADB] = true;
						}
					}

					// ustawienie poziomu wody:
					for (int ii = 0; ii < 4; ii++)
						if (wyciek_wew[ii]) s->level_of_water[w_lok][k_lok][ii] = poziom_maks;



					// sprawdzenie czy oczko przecieka na zewn¹trz:
					bool przeciek_gora = (wyciek_wew[ABC] && ((wysA < poziom_maks) || (wysC < poziom_maks)));
					bool przeciek_dol = (wyciek_wew[BDE] && ((wysD < poziom_maks) || (wysE < poziom_maks)));
					bool przeciek_lewo = (wyciek_wew[ADB] && ((wysA < poziom_maks) || (wysD < poziom_maks)));
					bool przeciek_prawo = (wyciek_wew[CBE] && ((wysC < poziom_maks) || (wysE < poziom_maks)));

					/*if ((w == 0) && (k == 1) && (w_lok == 9) && (k_lok == 2))
					{
						fprintf(f, "blok\n");
					}
					else*/
					{
						if (next_mem_num < next_num + 4)
						{
							CheckPoint *__next_pts = new CheckPoint[next_mem_num + 1000];
							for (long i = 0; i < next_num; i++) __next_pts[i] = next_pts[i];
							delete next_pts;
							next_pts = __next_pts;
							next_mem_num += 1000;
						}

						if (przeciek_gora)
						{
							//InsertWater(curr_pts[ce].x, z_pocz_sek + w_lok*rozmiar_pola - 0.0001, poziom_maks);
							next_pts[next_num].x = curr_pts[ce].x;
							next_pts[next_num].z = z_pocz_sek + w_lok*rozmiar_pola - 0.0001;
							next_num++;

							if (w_lok == 0){
								// nale¿y sprawdziæ czy wy¿ej po³o¿ony sektor ma gêstsz¹ siatkê oczek 0 jeœli tak,
								// to nale¿y wlaæ wodê w wiêksz¹ liczbê miejsc!!!
								Sector *sg = ts->find(w - 1, k);
								if (sg && (sg->number_of_cells > s->number_of_cells))
								{
								}
							}
						}
						if (przeciek_dol)
						{
							//InsertWater(curr_pts[ce].x, z_pocz_sek + (w_lok + 1)*rozmiar_pola + 0.0001, poziom_maks);
							next_pts[next_num].x = curr_pts[ce].x;
							next_pts[next_num].z = z_pocz_sek + (w_lok + 1)*rozmiar_pola + 0.0001;
							next_num++;
						}
						if (przeciek_lewo)
						{
							//InsertWater(x_pocz_sek + k_lok*rozmiar_pola - 0.0001, curr_pts[ce].z, poziom_maks);
							next_pts[next_num].x = x_pocz_sek + k_lok*rozmiar_pola - 0.0001;
							next_pts[next_num].z = curr_pts[ce].z;
							next_num++;
						}
						if (przeciek_prawo)
						{
							//InsertWater(x_pocz_sek + (k_lok + 1)*rozmiar_pola + 0.0001, curr_pts[ce].z, poziom_maks);
							next_pts[next_num].x = x_pocz_sek + (k_lok + 1)*rozmiar_pola + 0.0001;
							next_pts[next_num].z = curr_pts[ce].z;
							next_num++;
						}
					}

				} // jeœli komórka nie by³a wczeœniej odwiedzona

				//s->level_of_water[w_lok][k_lok] = poziom_maks;		
			} // jeœli Sector istnieje i ma mapê
			else  // jeœli nie ma mapy terenu, to ustalamy standardowy poziom wody nad terenem ze standardow¹ wysokoœci¹ (bez szczegó³owej mapy)
			{
				if ((height_std < poziom_maks) && (level_of_water_std != poziom_maks)) // zabezpieczamy siê przed zapêtleniem
				{

					//fprintf(f, "InsertWater: woda w obszar bez mapy, akt. poziom = %f\n", level_of_water_std);

					level_of_water_std = poziom_maks;

					// sprawdzamy przelanie do wszystkich sektorów przylegaj¹cych do standardowej powierzchni:
					for (long i = 0; i < ts->number_of_cells; i++)                 // po komórkach w tablicy ts
						for (long j = 0; j < ts->cells[i].number_of_sectors; j++)  // po sektorach
						{
							Sector *ss = ts->cells[i].sectors[j];
							if (ss->map_of_heights)
							{
								long __loczek = ss->number_of_cells;
								float __rozmiar_pola = (__loczek > 0 ? (float)sector_size / __loczek : 1);
								float __x_pocz_sek, __z_pocz_sek;             // wspó³rzêdne po³o¿enia pocz¹tku sektora
								SectorBeginPosition(&__x_pocz_sek, &__z_pocz_sek, ss->w, ss->k);

								if (next_mem_num < next_num + __loczek*4) // by nie zabrak³o pamiêci na punkty
								{
									CheckPoint *__next_pts = new CheckPoint[next_mem_num + __loczek*40];
									for (long i = 0; i < next_num; i++) __next_pts[i] = next_pts[i];
									delete next_pts;
									next_pts = __next_pts;
									next_mem_num += __loczek * 40;
								}

								// sprawdzenie czy w s¹siedztwie tego sektoru s¹ puste miejsca, z których mo¿e przelaæ siê woda:
								Sector *sg = ts->find(ss->w - 1, ss->k);
								if ((sg == NULL) || (sg->map_of_heights == NULL)){           // jest puste miejsce od góry
									for (int oczko = 0; oczko < __loczek; oczko++)
									{
										//InsertWater(__x_pocz_sek + __rozmiar_pola*(oczko + 0.5), __z_pocz_sek + 0.0001, poziom_maks);
										next_pts[next_num].x = __x_pocz_sek + __rozmiar_pola*(oczko + 0.5);
										next_pts[next_num].z = __z_pocz_sek + 0.0001;
										next_num++;
									}
								}
								Sector *sd = ts->find(ss->w + 1, ss->k);
								if ((sd == NULL) || (sd->map_of_heights == NULL)){           // jest puste miejsce od do³u
									for (int oczko = 0; oczko < __loczek; oczko++)
									{
										//InsertWater(__x_pocz_sek + __rozmiar_pola*(oczko + 0.5), __z_pocz_sek + sector_size - 0.0001, poziom_maks);
										next_pts[next_num].x = __x_pocz_sek + __rozmiar_pola*(oczko + 0.5);
										next_pts[next_num].z = __z_pocz_sek + sector_size - 0.0001;
										next_num++;
									}
								}
								Sector *sl = ts->find(ss->w, ss->k - 1);
								if ((sl == NULL) || (sl->map_of_heights == NULL)){           // jest puste miejsce z lewej
									for (int oczko = 0; oczko < __loczek; oczko++)
									{
										//InsertWater(__x_pocz_sek + 0.0001, __z_pocz_sek + __rozmiar_pola*(oczko + 0.5), poziom_maks);
										next_pts[next_num].x = __x_pocz_sek + 0.0001;
										next_pts[next_num].z = __z_pocz_sek + __rozmiar_pola*(oczko + 0.5);
										next_num++;
									}
								}
								Sector *sp = ts->find(ss->w, ss->k + 1);
								if ((sp == NULL) || (sp->map_of_heights == NULL)){           // jest puste miejsce z prawej
									for (int oczko = 0; oczko < __loczek; oczko++)
									{
										//InsertWater(__x_pocz_sek + sector_size - 0.0001, __z_pocz_sek + __rozmiar_pola*(oczko + 0.5), poziom_maks);
										next_pts[next_num].x = __x_pocz_sek + sector_size - 0.0001;
										next_pts[next_num].z = __z_pocz_sek + __rozmiar_pola*(oczko + 0.5);
										next_num++;
									}
								}
							}
						}
				} // po sektorach

			}  // jeœli nie ma mapy terenu
		} // for po punktach w trójk¹tach (komórkach) w które mo¿e wlaæ siê woda

		delete curr_pts;
		curr_pts = next_pts;
		curr_num = next_num;
		next_pts = new CheckPoint[1000];
		next_num = 0;
		next_mem_num = 1000;
	} // while jeœli s¹ komórki, w których trzeba sprawdziæ mo¿liwoœæ wlania siê wody 
	delete curr_pts;
	delete next_pts;
}


float Terrain::LevelOfWater(float x, float z)
{
	float water_level = -1e10;
	long w, k;                                   // wspó³rzêdne sektora 
	SectorCoordinates(&w, &k, x, z);             // wyznaczenie tych wspó³rzêdnych
	// wyszukanie sektora:
	Sector *s = ts->find(w, k);
	if (s && s->level_of_water)
	{
		long loczek = s->number_of_cells;
		float rozmiar_pola = (loczek > 0 ? (float)sector_size / loczek : 1);
		float x_pocz_sek, z_pocz_sek;             // wspó³rzêdne po³o¿enia pocz¹tku sektora
		SectorBeginPosition(&x_pocz_sek, &z_pocz_sek, w, k);
		float x_lok = x - x_pocz_sek, z_lok = z - z_pocz_sek;   // wspó³rzêdne lokalne wewn¹trz sektora
		long k_lok = (long)(x_lok / rozmiar_pola), w_lok = (long)(z_lok / rozmiar_pola);  // lokalne wspó³rzêdne pola w sektorze

		// sprawdzamy w który trójk¹t wstawiono wodê:
		Vector3 B = Vector3((k_lok + 0.5)*rozmiar_pola, 0, (w_lok + 0.5)*rozmiar_pola);
		enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };       // trójk¹t w którym znajduje siê punkt 
		int trojkat = 0;
		if ((B.x > x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = ADB;
		else if ((B.x < x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = CBE;
		else if (B.z > z_lok) trojkat = ABC;
		else trojkat = BDE;

		// sprawdzamy,czy przypadkiem nie odwiedziliœmy oczka od tej strony
		// jeœli tak, to przerywamy funkcjê, by nie kr¹¿yæ w pêtli:
		water_level = s->level_of_water[w_lok][k_lok][trojkat];
	}
	else
	{
		water_level = level_of_water_std;
	}

	return water_level;
}

// punkt przeciêcia prostej pionowej przechodz¹cej przez punkt pol z górn¹ p³aszczyzn¹ najwy¿ej po³o¿onego przedmiotu spoœród
// zaznaczonych. Jeœli ¿aden nie le¿y na linii przeciêcia, to zwracana jest wartoœæ -1e10.  
float Terrain::HighestSelectedItemHeight(Vector3 pol)
{
	// szukamy przedmiotów w radiusiu
	Item **item_tab_pointer = NULL;
	long number_of_items_in_radius = ItemsInRadius(&item_tab_pointer, pol, 0);
	float wysokosc_maks = -1e10;
	Item *wsk_min = NULL;  // wskaŸnik do przedmiotu, od którego liczona jest minimalna wysokoœæ
	for (long i = 0; i < number_of_items_in_radius; i++)
		if (item_tab_pointer[i]->if_selected)
		{
			float wys_na_p = ItemPointHeight(pol, item_tab_pointer[i]);
			//if (item_tab_pointer[i]->type == ITEM_WALL)
			{
				if (wysokosc_maks < wys_na_p)
				{
					wysokosc_maks = wys_na_p;
					wsk_min = item_tab_pointer[i];
				}
			}
		}

	delete item_tab_pointer;
	return wysokosc_maks;
}

// iteracyjne wyznaczenie kursora 3D z uwazglêdnieniem wysokoœci terrainu i zaznaczonych przedmiotów, na których
// mo¿e le¿eæ nowo-tworzony przedmiot
Vector3 Terrain::Cursor3D_CoordinatesWithoutParallax(int X, int Y)
{
	Vector3 w = Vector3(0, 0, 0);
	float wys_na_prz;
	for (int i = 0; i < 7; i++)
	{
		w = Cursor3dCoordinates(X, Y, w.y);
		wys_na_prz = HighestSelectedItemHeight(w);
		if (wys_na_prz > -1e9)
			w.y = wys_na_prz;
		else
			w.y = GroundHeight(w.x, w.z);
	}
	return w;
}

// Przedmioty k³adzione s¹ na teren tak, by le¿a³y na jego powierzchni
void Terrain::PlaceItemInTerrain(Item *prz) // wyzn. par. przedmiotu w odniesieniu do terrainu
{
	// uzupe³nienie wartoœci niektórych parametrów przedmiotu:
	prz->if_renewable = 1;
	float grubosc = 0;
	switch (prz->type)
	{
	case ITEM_COIN:
	{
		prz->diameter = powf(prz->value / 100, 0.4);
		grubosc = 0.2*prz->diameter;
		
		prz->to_take = 1;
		prz->diameter_visual = sqrt(prz->diameter*prz->diameter + grubosc*grubosc);
		break;
	}
	case ITEM_BARREL:
	{
		prz->diameter = powf((float)prz->value / 50, 0.4);
		grubosc = 2 * prz->diameter;
		//prz->vPos.y = grubosc + GroundHeight(prz->vPos.x, prz->vPos.z);
		prz->to_take = 1;
		prz->diameter_visual = sqrt(prz->diameter*prz->diameter + grubosc*grubosc);
		break;
	}
	case ITEM_TREE:
		prz->to_take = 0;

		switch (prz->subtype)
		{
		case TREE_POPLAR:
		{
			prz->diameter = 0.65 + prz->value / 40;
			break;
		}
		case TREE_SPRUCE:
		{
			prz->diameter = prz->value / 10;
			break;
		}
		case TREE_BAOBAB:
		{
			prz->diameter = prz->value / 5;
			break;
		}
		case TREE_FANTAZJA:
		{
			prz->diameter = prz->value / 10;
			break;
		}
		}
		//prz->vPos.y = prz->value + GroundHeight(prz->vPos.x, prz->vPos.z);
		prz->diameter_visual = prz->value;
		grubosc = prz->value;
		break;
	case ITEM_POINT:
	{
		//prz->vPos.y = 0.0 + GroundHeight(prz->vPos.x, prz->vPos.z);
		break;
	}
	} // switch type przedmiotu

	// obliczenie wysokoœci na jakiej le¿y 
	if (prz->type != ITEM_POINT)
	{
		if (prz->param_f[1] > -1e10)      // gdy le¿y on na jakimœ innym przedmiocie prz->param_f[1] jest wysokoœci¹ od której zaczynamy poszukiwania
		{                                 // jeœli schodz¹c w dó³ ¿adnego przedmiotu nie napotkamy, to przedmiot znajdzie siê na gruncie
			prz->vPos.y = grubosc + prz->param_f[1];
			prz->vPos.y = grubosc + GeneralHeight(prz->vPos);
		}
		else                              // gdy le¿y bezpoœrednio na gruncie
			prz->vPos.y = grubosc + GroundHeight(prz->vPos.x, prz->vPos.z);
	}
}

long Terrain::InsertItemToArrays(Item prz)
{
	long ind = number_of_items;
	if (number_of_items >= number_of_items_max) // gdy nowy przedmiot nie mieœci siê w tablicy przedmiotów
	{
		long nowa_liczba_przedmiotow_max = 2 * number_of_items_max;
		Item *p_nowe = new Item[nowa_liczba_przedmiotow_max];
		for (long i = 0; i < number_of_items; i++) {
			p_nowe[i] = p[i];
			DeleteItemFromSectors(&p[i]);   // trzeba to zrobiæ gdy¿ zmieniaj¹ siê wskaŸniki !
			InsertItemIntoSectors(&p_nowe[i]);
		}
		delete p;
		p = p_nowe;
		number_of_items_max = nowa_liczba_przedmiotow_max;
	}
	prz.index = ind;
	p[ind] = prz;
	
	fprintf(f, "Utworzono przedmiot %d index = %d, type = %d (%s)\n", &p[ind], ind, p[ind].type, PRZ_nazwy[p[ind].type]);
	PlaceItemInTerrain(&p[ind]);
	InsertItemIntoSectors(&p[ind]);
	//fprintf(f,"wstawiam przedmiot %d w miejsce (%f, %f, %f)\n",i,terrain.p[ind].vPos.x,terrain.p[ind].vPos.y,terrain.p[ind].vPos.z);
	

	number_of_items++;
	return ind;
}

// Zaznaczanie lub odznaczanie grupy przedmiotów lub pojedyczego przedmiotu
void Terrain::SelectUnselectItemOrGroup(long nr_prz)
{
	char lan[256];
	if (p[nr_prz].group > -1)       // zaznaczanie/ odznaczanie grupy
	{ 
		long group = p[nr_prz].group;
		if (p[nr_prz].if_selected)     // odznaczanie grupy
		{
			long number_of_group_unselected = 0;
			for (long i = 0; i < number_of_items; i++)
				if (p[i].group == group)
				{
					p[i].if_selected = 0;
					for (long j = 0; j < number_of_selected_items; j++)  // przeszukujemy listê zaznaczonych przedmiotów by usun¹æ zaznaczenie
					{
						if (selected_items[j] == i)
						{
							selected_items[j] = selected_items[number_of_selected_items - 1];
							number_of_selected_items--;
						}
					}
					number_of_group_unselected++;
				}
			sprintf(lan, "odznaczono %d przedmiotow grupy %d", number_of_group_unselected, group);
			SetWindowText(main_window, lan);
		}
		else                        // zaznaczanie grupy
		{
			long number_of_grouped_items_selected = 0;
			for (long i = 0; i < number_of_items; i++)
				if (p[i].group == group)
				{
					p[i].if_selected = 1;
					if (number_of_selected_items == number_of_selected_items_max) // trzeba powiêkszyæ tablicê
					{
						long *__zazn_przedm = new long[number_of_selected_items_max * 2];
						for (long j = 0; j < number_of_selected_items; j++) __zazn_przedm[j] = selected_items[j];
						delete selected_items;
						selected_items = __zazn_przedm;
						number_of_selected_items_max *= 2;
					}
					selected_items[number_of_selected_items] = i;   // wstawiam na koniec listy
					number_of_selected_items++;
					number_of_grouped_items_selected++;
				}
			
			sprintf(lan, "zaznaczono %d przedmiotow grupy %d", number_of_grouped_items_selected,group);
			SetWindowText(main_window, lan);
		}
	}
	else                            // zaznaczanie/odznaczanie przedmiotu
	{
		p[nr_prz].if_selected = 1 - p[nr_prz].if_selected;
		char lan[128];
		if (p[nr_prz].if_selected)
			sprintf(lan, "zaznaczono_przedmiot_%d,_razem_%d_zaznaczonych",
			nr_prz, number_of_selected_items + 1);
		else
			sprintf(lan, "odznaczono_przedmiot_%d__wcisnij_SHIFT+RMB_by_odznaczyc_wszystkie", nr_prz);
		SetWindowText(main_window, lan);
		if (p[nr_prz].if_selected)   // dodaje do osobnej listy zaznaczonych przedmiotów
		{
			if (number_of_selected_items == number_of_selected_items_max) // trzeba powiêkszyæ tablicê
			{
				long *__zazn_przedm = new long[number_of_selected_items_max * 2];
				for (long i = 0; i < number_of_selected_items; i++) __zazn_przedm[i] = selected_items[i];
				delete selected_items;
				selected_items = __zazn_przedm;
				number_of_selected_items_max *= 2;
			}
			selected_items[number_of_selected_items] = nr_prz;   // wstawiam na koniec listy
			number_of_selected_items++;
		}
		else                             // usuwam z osobnej listy zaznaczonych przedmiotów
		{
			for (long i = 0; i < number_of_selected_items; i++)  // przeszukujemy listê zaznaczonych przedmiotów by usun¹æ zaznaczenie
			{
				if (selected_items[i] == nr_prz)
				{
					selected_items[i] = selected_items[number_of_selected_items - 1];
					number_of_selected_items--;
				}
			}
		}
	}
}

// UWAGA! Na razie procedura s³aba pod wzglêdem obliczeniowym (pêtla po przedmiotach x po zaznaczonych przedmiotach)
// z powodu wystêpowania krawêdzi, które s¹ zale¿ne od punktów
void Terrain::DeleteSelectItems()
{
	// sprawdzenie czy wœród przeznaczonych do usuniêcia s¹ punkty, które mogê byæ powi¹zane z krawêdziami. 
	// Jeœli tak, to krawêdzie równie¿ nale¿y usun¹æ - mo¿na to zrobiæ poprzez do³¹czenie ich do listy zaznaczonych:
	for (long i = 0; i < number_of_selected_items; i++)
	{
		long nr_prz = selected_items[i];
		if (p[nr_prz].type == ITEM_POINT)
		{
			for (long j = 0; j < number_of_items; j++)
				if (((p[j].type == ITEM_EDGE) || (p[j].type == ITEM_WALL)) && (p[j].if_selected == 0) && ((p[j].param_i[0] == nr_prz) || (p[j].param_i[1] == nr_prz)))
					SelectUnselectItemOrGroup(j);
			if (p[nr_prz].subtype == POINT_OF_WATER)
				this->InsertWater(p[nr_prz].vPos.x, p[nr_prz].vPos.z, -1e10);
		}
	}
	

	fprintf(f, "\n\nUsu.prz.: liczba przedm = %d, liczba zazn = %d\n", number_of_items,number_of_selected_items);
	fprintf(f, "  lista przedmiotow:\n");
	for (long i = 0; i < number_of_items; i++){
		fprintf(f, "%d: %d (%s) o wartosci %f\n", i, p[i].type,PRZ_nazwy[p[i].type], p[i].value);
		if (p[i].type == ITEM_EDGE)
			fprintf(f, "         krawedz pomiedzy punktami %d a %d\n", p[i].param_i[0], p[i].param_i[1]);
	}

	for (long i = 0; i < number_of_selected_items; i++)
	{
		long nr_prz = selected_items[i];

		fprintf(f, "  usuwam prz %d: %d (%s) o wartosci %f\n", nr_prz, p[nr_prz].type,PRZ_nazwy[p[nr_prz].type], p[nr_prz].value);

		DeleteItemFromSectors(&p[nr_prz]);
		
		// usuniêcie przedmiotu z listy przedmiotów:
		if (nr_prz != number_of_items - 1)
		{
			DeleteItemFromSectors(&p[number_of_items - 1]);
			p[nr_prz] = p[number_of_items - 1];         // przepisuje w miejsce usuwanego ostatni przedmiot na liœcie
			p[nr_prz].index = nr_prz;

			// przedmiot ostatni na liœcie uzyskuje nowy index i jeœli jest to punkt, to trzeba poprawiæ te¿ odwo³ania do niego w krawêdziach:
			if (p[number_of_items - 1].type == ITEM_POINT)
				for (long j = 0; j < number_of_items;j++)
					if ((p[j].type == ITEM_EDGE) || (p[j].type == ITEM_WALL))
					{
						if (p[j].param_i[0] == number_of_items - 1) p[j].param_i[0] = nr_prz;
						if (p[j].param_i[1] == number_of_items - 1) p[j].param_i[1] = nr_prz;
					}
				

			InsertItemIntoSectors(&p[nr_prz]);                  // wstawiam go w sectors
			// zamiana numerów na liœcie zaznaczonych jeœli ostatnmi na niej wystêpuje:
			if (p[nr_prz].if_selected)
				for (long k = i + 1; k < number_of_selected_items; k++)
					if (selected_items[k] == number_of_items - 1) selected_items[k] = nr_prz;
		}
		// byæ mo¿e trzeba by jeszcze pozamieniaæ wskaŸniki przedmiotów w sektorach

		number_of_items--;
	}
	number_of_selected_items = 0;

}

void Terrain::NewMap()
{
	for (long i = 0; i < ts->number_of_cells; i++)
		for (long j = 0; j < ts->cells[i].number_of_sectors; j++)
			delete ts->cells[i].sectors[j];

	delete ts;
	delete p;
	this->number_of_items = 0;
	this->number_of_items_max = 10;
	p = new Item[number_of_items_max];
	ts = new SectorsHashTable();

	number_of_selected_items = 0;
	height_std = 0;
	level_of_water_std = -1e10;
}

float Terrain::GroundHeight(float x, float z)      // okreœlanie wysokoœci dla punktu o wsp. (x,z) 
{
	float y = height_std;    // wysokoœæ standardowa;

	long w, k;
	this->SectorCoordinates(&w, &k, x, z);               // w,k - wspó³rzêdne sektora, w którym znajduje siê punkt (x,z)

	Sector *s = ts->find(w, k);

	float **mapa = NULL;
	Vector3 ****__Norm = NULL;
	long loczek = 0;
	if (s)
	{
		if (s->map_of_heights_in_edit)
		{
			mapa = s->map_of_heights_in_edit;
			__Norm = s->normal_vectors_in_edit;
			loczek = s->number_of_cells_in_edit;
		}
		else
		{
			mapa = s->map_of_heights;
			__Norm = s->normal_vectors;
			loczek = s->number_of_cells;
		}
	}

	if (mapa)
	{
		float rozmiar_pola = (loczek > 0 ? (float)this->sector_size / loczek : 1);
		float x_pocz_sek, z_pocz_sek;             // wspó³rzêdne po³o¿enia pocz¹tku sektora
		this->SectorBeginPosition(&x_pocz_sek, &z_pocz_sek, w, k);
		float x_lok = x - x_pocz_sek, z_lok = z - z_pocz_sek;   // wspó³rzêdne lokalne wewn¹trz sektora
		long k_lok = (long)(x_lok / rozmiar_pola), w_lok = (long)(z_lok / rozmiar_pola);  // lokalne wspó³rzêdne pola w sektorze


		if ((k_lok < 0) || (w_lok < 0))
		{
			fprintf(f,"procedura Terrain::height: k i w lokalne nie moga byc ujemne!\n");
			fclose(f);
			exit(1);
		}

		if ((k_lok > loczek - 1) || (w_lok > loczek - 1))
		{
			fprintf(f, "procedura Terrain::height: k i w lokalne nie moga byc wieksze od liczby oczek - 1!\n");
			fclose(f);
			exit(1);
		}

		// tworzê trójk¹t ABC w górnej czêœci kwadratu: 
		//  A o_________o C
		//    |.       .|
		//    |  .   .  | 
		//    |    o B  | 
		//    |  .   .  |
		//    |._______.|
		//  D o         o E
		// wyznaczam punkt B - œrodek kwadratu oraz trójk¹t, w którym znajduje siê punkt
		Vector3 B = Vector3((k_lok + 0.5)*rozmiar_pola, mapa[w_lok * 2 + 1][k_lok], (w_lok + 0.5)*rozmiar_pola);
		enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };       // trójk¹t w którym znajduje siê punkt 
		int trojkat = 0;
		if ((B.x > x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = ADB;
		else if ((B.x < x_lok) && (fabs(B.z - z_lok) < fabs(B.x - x_lok))) trojkat = CBE;
		else if (B.z > z_lok) trojkat = ABC;
		else trojkat = BDE;

		// wyznaczam normaln¹ do p³aszczyzny a nastêpnie wspó³czynnik d z równania p³aszczyzny
		Vector3 N = __Norm[0][w_lok][k_lok][trojkat];
		float dd = -(B^N);          // wyraz wolny z równania plaszyzny trójk¹ta
		//float y;
		if (N.y > 0) y = (-dd - N.x*x_lok - N.z*z_lok) / N.y;
		else y = 0;

	} // jeœli Sector istnieje i zawiera mapê wysokoœci

	return y;
}

// wysokoœæ na jakiej znajdzie siê przeciêcie pionowej linii przech. przez pol na górnej powierzchni przedmiotu
float Terrain::ItemPointHeight(Vector3 pol, Item *prz)
{
	float wys_na_p = -1e10;
	if (prz->type == ITEM_WALL)
	{
		// sprawdzamy, czy rzut pionowy pol mieœci siê w prostok¹cie muru:
		Vector3 A = p[prz->param_i[0]].vPos, B = p[prz->param_i[1]].vPos;       // punkty tworz¹ce krawêdŸ
		A.y += p[prz->param_i[0]].value;
		B.y += p[prz->param_i[1]].value;

		Vector3 AB = B - A;
		Vector3 m_pion = Vector3(0, 1, 0);     // vertical muru
		float m_wysokosc = prz->value;       // od prostej AB do góry jest polowa wysokoœci, druga polowa idzie w dó³ 
		Vector3 m_wlewo = (m_pion*AB).znorm();          // wektor jednostkowy w lewo od wektora AB i prostopadle do pionu
		float m_szerokosc = prz->param_f[0];

		Vector3 N = (AB*m_wlewo).znorm();            // normal_vector do p³aszczyzny górnej muru
		Vector3 R = intersection_point_between_line_and_plane(pol, pol - m_pion, N, A); // punkt spadku punktu pol na górn¹ p³. muru
		Vector3 RR = projection_of_point_on_line(R, A, B);                        // rzut punktu R na prost¹ AB
		bool czy_RR_na_odcAB = (((RR - A) ^ (RR - B)) < 0);                 // czy punkt ten le¿y na odcinku AB (mieœci siê na d³ugoœci muru)
		Vector3 R_RR = RR - R;
		float odl = fabs(R_RR.x*m_wlewo.x + R_RR.z*m_wlewo.z);              // odleg³oœæ R od osi muru (niewymagaj¹ca pierwiastkowania)
		if ((odl < m_szerokosc / 2) && (czy_RR_na_odcAB))  // warunek tego, ¿e rzut pionowy pol mieœci siê w prostok¹cie muru
			wys_na_p = (R.y + m_wysokosc / 2);               // wysokoœæ punktu pol nad górn¹ p³. muru
	} // jeœli mur
	return wys_na_p;
}

// Okreœlenie wysokoœci nad najbli¿szym przedmiotem (obiektem ruchomym) lub gruntem patrz¹c w dó³ od punktu pol
// wysokoœæ mo¿e byæ ujemna, co oznacza zag³êbienie w gruncie lub przedmiocie
float Terrain::GeneralHeight(Vector3 pol)
{
	// szukamy przedmiotów w radiusiu
	Item **item_tab_pointer = NULL;
	long number_of_items_in_radius = ItemsInRadius(&item_tab_pointer, pol, 0);
	
	float wysokosc_gruntu = GroundHeight(pol.x, pol.z);
	float roznica_wys_min = pol.y-wysokosc_gruntu;

	Item *wsk_min = NULL;  // wskaŸnik do przedmiotu, od którego liczona jest minimalna wysokoœæ
	for (long i = 0; i < number_of_items_in_radius; i++)
	{		
		float wys_na_p = this->ItemPointHeight(pol, item_tab_pointer[i]);
		if (wys_na_p > -1e10)
			if (item_tab_pointer[i]->type == ITEM_WALL)
			{
				float roznica_wys = pol.y - wys_na_p;               // wysokoœæ punktu pol nad górn¹ p³. muru
				float m_wysokosc = item_tab_pointer[i]->value;
				//bool czy_wewn_muru = ((roznica_wys < 0) && (-roznica_wys < m_wysokosc));  // czy punkt pol znajduje siê wewn¹trz muru
				if ((roznica_wys < roznica_wys_min) && 
					((roznica_wys > 0) || (-roznica_wys < m_wysokosc)))  // ponad murem lub wewn¹trz muru (nie pod spodem!)
				{
					roznica_wys_min = roznica_wys;
					wsk_min = item_tab_pointer[i];
				}		
			} // jeœli mur
	}
	delete item_tab_pointer;

	// Tutaj nale¿a³oby jeszcze sprawdziæ wysokoœæ nad obiektami ruchomymi


	return pol.y - roznica_wys_min;
}

void Terrain::GraphicsInitialization()
{


}


void Terrain::InsertItemIntoSectors(Item *prz)
{
	// przypisanie przedmniotu do okreœlonego obszaru -> umieszczenie w tablicy wskaŸników 
	// wspó³rzêdne kwadratu opisuj¹cego przedmiot:
	float x1 = prz->vPos.x - prz->diameter / 2, x2 = prz->vPos.x + prz->diameter / 2,
		z1 = prz->vPos.z - prz->diameter / 2, z2 = prz->vPos.z + prz->diameter / 2;

	long k1, w1, k2, w2;
	SectorCoordinates(&w1, &k1, x1, z1);
	SectorCoordinates(&w2, &k2, x2, z2);

	//fprintf(f, "wsp. kwadratu opisujacego przedmiot: w1=%d,w2=%d,k1=%d,k2=%d\n", w1, w2, k1, k2);
	

	// wstawiamy przedmiot w te sectors, w których powinien byæ widoczny:
	for (long w = w1; w <= w2; w++)
		for (long k = k1; k <= k2; k++)
		{
			//fprintf(f, "wstawiam przedmiot %d: %s o wartosci %f i srednicy %f w Sector w = %d, k = %d\n", prz,PRZ_nazwy[prz->type],prz->value, prz->diameter, w, k);
			Sector *ob = ts->find(w, k);
			if (ob == NULL) {                 // tworzymy Sector, gdy nie istnieje (mo¿na nie tworzyæ mapy)
				ob = new Sector(0, w, k, false);
				ts->insert(ob);
			}
			ob->insert_item(prz);

			//fprintf(f, "  wstawiono przedmiot w Sector (%d, %d) \n", w, k);
		}
}

// Wyszukiwanie wszystkich przedmiotow le¿¹cych w okrêgu o œrodku wyznaczonym
// sk³adowymi x,z wekrora pol i zadanym radiusiu. Przedmioty umieszczane s¹ na 
// liœcie wskaŸników item_tab_pointer. PóŸniej nale¿y zwolniæ pamiêæ: 
//     delete item_tab_pointer;
// Funkcja zwraca liczbê znalezionych przedmiotów
long Terrain::ItemsInRadius(Item*** item_tab_pointer, Vector3 pol, float radius)
{
	Vector3 pol2D = pol;
	pol2D.y = 0;
	float x1 = pol2D.x - radius, z1 = pol2D.z - radius, x2 = pol2D.x + radius, z2 = pol2D.z + radius;
	long kp1, wp1, kp2, wp2;
	this->SectorCoordinates(&wp1, &kp1, x1, z1);
	this->SectorCoordinates(&wp2, &kp2, x2, z2);
	//fprintf(f,"kp1 = %d, wp1 = %d, kp2 = %d, wp2 = %d\n",kp1,wp1,kp2,wp2);
	float radius_kw = radius*radius;

	Sector **oby = new Sector*[(wp2 - wp1 + 1)*(kp2 - kp1 + 1)];
	long liczba_obsz = 0;

	// wyznaczenie sektorów w otoczeniu: 
	long liczba_przedm = 0, liczba_prz_max = 0;            // wyznaczenie maks. liczby przedmiotów
	for (long w = wp1; w <= wp2; w++)
		for (long k = kp1; k <= kp2; k++)
		{
			Sector *ob = ts->find(w, k);
			if (ob)
			{
				oby[liczba_obsz++] = ob;   // wpicuje obszar do tablicy by ponownie go nie wyszukiwaæ
				liczba_prz_max += ob->number_of_items;
			}
		}

	//fprintf(f,"liczba_przedm_max = %d\n",liczba_przedm_max);
	//fclose(f);
	//exit(1);

	if (liczba_prz_max > 0)
	{
		(*item_tab_pointer) = new Item*[liczba_prz_max];              // alokacja pamieci dla wskaŸników przedmiotów
		for (long n = 0; n < liczba_obsz; n++)
		{
			Sector *ob = oby[n];
			for (long i = 0; i < ob->number_of_items; i++)
			{
				float odl_kw = 0;
				if ((ob->wp[i]->type == ITEM_EDGE) || (ob->wp[i]->type == ITEM_WALL))   // przedmioty odcinkowe
				{
					Vector3 A = p[ob->wp[i]->param_i[0]].vPos, B = p[ob->wp[i]->param_i[1]].vPos;
					A.y = 0; B.y = 0;
					float odl = distance_from_point_to_segment(pol2D, A, B) - ob->wp[i]->param_f[0] / 2;
					if (odl < 0) odl = 0;
					odl_kw = odl*odl;
				}
				else
					odl_kw = (ob->wp[i]->vPos.x - pol2D.x)*(ob->wp[i]->vPos.x - pol2D.x) +
					(ob->wp[i]->vPos.z - pol2D.z)*(ob->wp[i]->vPos.z - pol2D.z) - ob->wp[i]->diameter*ob->wp[i]->diameter / 4;
				if (odl_kw <= radius_kw)
				{
					(*item_tab_pointer)[liczba_przedm] = ob->wp[i];
					liczba_przedm++;
				}
			}
		}
	} // if maks. liczba przedmiotów > 0
	delete oby;
	return liczba_przedm;
}


void Terrain::InsertObjectIntoSectors(MovableObject *ob)
{
	float x1 = ob->state.vPos.x - ob->radius, x2 = ob->state.vPos.x + ob->radius,
		z1 = ob->state.vPos.z - ob->radius, z2 = ob->state.vPos.z + ob->radius;

	long k1, w1, k2, w2;
	SectorCoordinates(&w1, &k1, x1, z1);
	SectorCoordinates(&w2, &k2, x2, z2);

	//fprintf(f, "wsp. kwadratu opisujacego przedmiot: w1=%d,w2=%d,k1=%d,k2=%d\n", w1, w2, k1, k2);

	// wstawiamy obiekt ruchomy w te sectors, w których powinien byæ widoczny:
	for (long w = w1; w <= w2; w++)
		for (long k = k1; k <= k2; k++)
		{
			Sector *obsz = ts->find(w, k);
			if (obsz == NULL) {                 // tworzymy Sector, gdy nie istnieje (mo¿na nie tworzyæ mapy)
				obsz = new Sector(0, w, k, false);
				ts->insert(obsz);
			}
			obsz->insert_movable_object(ob);

			//fprintf(f, "  wstawiono obiekt ruchomy w Sector (%d, %d) \n", w, k);
		}

}
void Terrain::DeleteObjectsFromSectors(MovableObject *ob)
{
	float x1 = ob->state.vPos.x - ob->radius, x2 = ob->state.vPos.x + ob->radius,
		z1 = ob->state.vPos.z - ob->radius, z2 = ob->state.vPos.z + ob->radius;

	long k1, w1, k2, w2;
	SectorCoordinates(&w1, &k1, x1, z1);
	SectorCoordinates(&w2, &k2, x2, z2);

	for (long w = w1; w <= w2; w++)
		for (long k = k1; k <= k2; k++)
		{
			Sector *obsz = ts->find(w, k);
			if (obsz == NULL) {                 // to nie powinno siê zdarzaæ!

			}
			else
			{
				obsz->release_movable_object(ob);
				//fprintf(f, "  usuniêto obiekt ruchomy z sektora (%d, %d) \n", w, k);

				// mo¿na jeszcze sprawdziæ czy Sector jest pusty (brak mapy, przedmiotów i ob. ruchomych) i usun¹æ Sector:
				if ((obsz->number_of_movable_objects == 0) && (obsz->number_of_items == 0) && (obsz->level_of_water == NULL)&&
					(obsz->type_of_surface == NULL) && (obsz->map_of_heights == NULL) && (obsz->map_of_heights_in_edit == NULL))
				{
					ts->remove(obsz);
					//delete obsz->mov_obj;  // to jest usuwane w destruktorze ( dwie linie dalej!)
					//delete obsz->wp;
					delete obsz;
				}
			}
			//fprintf(f, "  usunieto obiekt ruchomy w sektora (%d, %d) \n", w, k);
		}

}

void Terrain::DeleteItemFromSectors(Item *prz)
{
	float x1 = prz->vPos.x - prz->diameter / 2, x2 = prz->vPos.x + prz->diameter / 2,
		z1 = prz->vPos.z - prz->diameter / 2, z2 = prz->vPos.z + prz->diameter / 2;

	long k1, w1, k2, w2;
	SectorCoordinates(&w1, &k1, x1, z1);
	SectorCoordinates(&w2, &k2, x2, z2);

	for (long w = w1; w <= w2; w++)
		for (long k = k1; k <= k2; k++)
		{
			Sector *sek = ts->find(w, k);
			if (sek == NULL) {                 // to nie powinno siê zdarzaæ!

			}
			else
			{
				sek->remove_item(prz);
				fprintf(f, "  usuniêto przedmiot %d: %s o wartosci %f z sektora (%d, %d) \n", prz, PRZ_nazwy[prz->type],prz->value,w, k);

				// mo¿na jeszcze sprawdziæ czy Sector jest pusty (brak mapy, przedmiotów i ob. ruchomych) i usun¹æ Sector:
				if ((sek->number_of_movable_objects == 0) && (sek->number_of_items == 0) &&
					(sek->type_of_surface == NULL) && (sek->level_of_water == NULL) && (sek->map_of_heights == NULL))
				{
					ts->remove(sek);
					//delete obsz->mov_obj;  // to jest usuwane w destruktorze ( dwie linie dalej!)
					//delete obsz->wp;
					delete sek;
				}
			}
		}
}

long Terrain::ObjectsInRadius(MovableObject*** object_tab_pointer, Vector3 pol, float radius)
{
	float x1 = pol.x - radius, z1 = pol.z - radius, x2 = pol.x + radius, z2 = pol.z + radius;
	long kp1, wp1, kp2, wp2;
	this->SectorCoordinates(&wp1, &kp1, x1, z1);
	this->SectorCoordinates(&wp2, &kp2, x2, z2);
	//fprintf(f,"kp1 = %d, wp1 = %d, kp2 = %d, wp2 = %d\n",kp1,wp1,kp2,wp2);
	float radius_kw = radius*radius;

	Sector **oby = new Sector*[(wp2 - wp1 + 1)*(kp2 - kp1 + 1)];
	long liczba_obsz = 0;

	long liczba_obiektow = 0, liczba_obiektow_max = 0;            // wyznaczenie maks. liczby przedmiotów
	for (long w = wp1; w <= wp2; w++)
		for (long k = kp1; k <= kp2; k++)
		{
			Sector *ob = ts->find(w, k);
			if (ob)
			{
				oby[liczba_obsz++] = ob;   // wpicuje obszar do tablicy by ponownie go nie wyszukiwaæ
				liczba_obiektow_max += ob->number_of_movable_objects;
			}
		}

	//fprintf(f,"liczba_przedm_max = %d\n",liczba_przedm_max);
	//fclose(f);
	//exit(1);

	if (liczba_obiektow_max > 0)
	{
		(*object_tab_pointer) = new MovableObject*[liczba_obiektow_max];              // alokacja pamieci dla wskaŸników obiektów ruchomych
		for (long n = 0; n < liczba_obsz; n++)
		{
			Sector *ob = oby[n];
			for (long i = 0; i < ob->number_of_movable_objects; i++)
			{
				float odl_kw = (ob->mov_obj[i]->state.vPos.x - pol.x)*(ob->mov_obj[i]->state.vPos.x - pol.x) +
					(ob->mov_obj[i]->state.vPos.z - pol.z)*(ob->mov_obj[i]->state.vPos.z - pol.z);
				if (odl_kw <= radius_kw)
				{
					(*object_tab_pointer)[liczba_obiektow] = ob->mov_obj[i];
					liczba_obiektow++;
				}
			}
		}
	} // if maks. liczba przedmiotów > 0
	delete oby;
	return liczba_obiektow;

}



/*
maks. wielkoœæ obiektu w pikselach, widoczna na ekranie, -1 jeœli obiekt w ogóle nie jest widoczny
*/
float NumberOfVisiblePixels(Vector3 WPol, float diameter)
{
	// Sprawdzenie, czy jakikolwiek z kluczowych punktów sfery opisujacej przedmiot nie jest widoczny
	// (rysowanie z pominiêciem niewidocznych przedmiotów jest znacznie szybsze!)
	bool czy_widoczny = 1;// 0;
	RECT clr;
	GetClientRect(main_window, &clr);
	Vector3 pol_kam = par_view.initial_camera_position - par_view.initial_camera_direction*par_view.distance;     // po³o¿enie kamery
	//Vector3 vPos = p.vPos;
	Vector3 pkt_klucz_sfery[] = { Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), // kluczowe punkty sfery widocznoœci
		Vector3(0, -1, 0), Vector3(0, 0, 1), Vector3(0, 0, -1) };
	const int liczba_pkt_klucz = sizeof(pkt_klucz_sfery) / sizeof(Vector3);
	Vector3 ePol[liczba_pkt_klucz];   // wspolrzedne punktów kluczowych na ekranie (tylko skladowe x,y) 

	for (int pkt = 0; pkt<liczba_pkt_klucz; pkt++)
	{
		Vector3 wPol2 = WPol + pkt_klucz_sfery[pkt] * diameter;   // po³o¿enie wybranego punktu pocz¹tkowego  
		ScreenCoordinates(&ePol[pkt].x, &ePol[pkt].y, &ePol[pkt].z, wPol2);        // wyznaczenie wspó³rzêdnych ekranu
		float ilo_skal = (wPol2 - pol_kam) ^ par_view.initial_camera_direction;                      // iloczyn skalarny kierunku od kam do pojazdu i kierunku patrzenia kam
		if ((ePol[pkt].x > clr.left) && (ePol[pkt].x < clr.right) &&               // jeœli wspó³rzêdne ekranu któregokolwiek punktu mieszcz¹ siê
			(ePol[pkt].y > clr.top) && (ePol[pkt].y < clr.bottom) && (ilo_skal > 0))  // w oknie i punkt z przodu kamery to mo¿e on byæ widoczny
			czy_widoczny = 1;                                                        // (mo¿na jeszcze sprawdziæ sto¿ek widocznoœci) 
	}

	// sprawdzenie na ile widoczny jest przedmiot - w tym celu wybierana jest jedna z 3 osi sfery o najwiêkszej 
	// ekranowej odleg³oœci pomiêdzy jej punktami na sferze. Im wiêksza ekranowa odleg³oœæ, tym wiêcej szczegó³ów, 
	// jeœli < 1 piksel, to mo¿na w ogóle przedmiotu nie rysowaæ, jeœli < 10 piks. to mo¿na nie rysowaæ napisu
	float liczba_pix_wid = 0;          // szczegó³owoœæ w pikselach maksymalnie widocznego rozmiaru przedmiotu
	if (czy_widoczny)
	{
		float odl_x = fabs(ePol[0].x - ePol[1].x), odl_y = fabs(ePol[2].y - ePol[3].y),
			odl_z = fabs(ePol[4].z - ePol[5].z);
		liczba_pix_wid = (odl_x > odl_y ? odl_x : odl_y);
		liczba_pix_wid = (odl_z > liczba_pix_wid ? odl_z : liczba_pix_wid);
	}

	if (czy_widoczny == 0) liczba_pix_wid = -1;

	return liczba_pix_wid;
}


void Terrain::DrawObject()
{
	float radius_of_horizont = 1400;

	// rysowanie powierzchni terrainu:
	//glCallList(TerrainSurface);
	GLfloat SelectionSurface[] = { 0.95f, 0.5f, 0.5f, 0.4f };
	GLfloat OrangeSurface[] = { 1.0f, 0.8f, 0.0f, 0.7f };
	GLfloat GreenSurface[] = { 0.39f, 0.36f, 0.5f, 1.0f };
	GLfloat WaterSurface[] = { 0.10f, 0.2f, 1.0f, 0.3f };

	// rysujemy wszystko co jest w okrêgu o srednicy 1000 m:
	
	long liczba_sekt = radius_of_horizont / this->sector_size;  // liczba widocznych sektorów
	long ls_min_x = liczba_sekt, ls_plus_x = liczba_sekt, ls_min_z = liczba_sekt, ls_plus_z = liczba_sekt;  
	long wwx, kkx;                // wspó³rzêdne sektora, w którym znajduje siê kamera
	//Vector3 pol_kam = initial_camera_position - initial_camera_direction*distance;     // po³o¿enie kamery
	Vector3 pol_kam, kierunek_kam, pion_kam;
	CameraSettings(&pol_kam, &kierunek_kam, &pion_kam, par_view);             // wyznaczenie po³o¿enia kamery i innych jej ustawieñ

	this->SectorCoordinates(&wwx, &kkx, pol_kam.x, pol_kam.z);               // wspó³rzêdne sektora w którym znajduje siê kamera
	//fprintf(f, "pol. kamery = (%f,%f,%f), Sector (%d, %d)\n", pol_kam.x, pol_kam.y, pol_kam.z, wwx, kkx);



	// jeœli widok od góry, warto jest sprawdziæ czy sektor jest widoczny i ewentualnie powiêkszyæ obszar widocznoœci 
	// poza zadany widnokr¹g
	float skalar = kierunek_kam^Vector3(0, 1, 0);
	if (skalar < -0.7)  // jeœli kamera patrzy g³ównie w dó³
	{
		RECT r;
		GetClientRect(main_window, &r);
		Vector3 w1 = Cursor3dCoordinates(0, r.bottom - r.top - 0);
		if (w1.x / sector_size < -ls_min_x) ls_min_x = -w1.x / sector_size+1;
		else if (w1.x / sector_size > ls_plus_x) ls_plus_x = w1.x / sector_size+1;
		if (w1.z / sector_size < -ls_min_z) ls_min_z = -w1.z / sector_size+1;
		else if (w1.z / sector_size > ls_plus_z) ls_plus_z = w1.z / sector_size+1;
		w1 = Cursor3dCoordinates(0, 0);
		if (w1.x / sector_size < -ls_min_x) ls_min_x = -w1.x / sector_size + 1;
		else if (w1.x / sector_size > ls_plus_x) ls_plus_x = w1.x / sector_size + 1;
		if (w1.z / sector_size < -ls_min_z) ls_min_z = -w1.z / sector_size + 1;
		else if (w1.z / sector_size > ls_plus_z) ls_plus_z = w1.z / sector_size + 1;
		w1 = Cursor3dCoordinates(r.right-r.left, 0);
		if (w1.x / sector_size < -ls_min_x) ls_min_x = -w1.x / sector_size + 1;
		else if (w1.x / sector_size > ls_plus_x) ls_plus_x = w1.x / sector_size + 1;
		if (w1.z / sector_size < -ls_min_z) ls_min_z = -w1.z / sector_size + 1;
		else if (w1.z / sector_size > ls_plus_z) ls_plus_z = w1.z / sector_size + 1;
		w1 = Cursor3dCoordinates(r.right - r.left, r.bottom - r.top);
		if (w1.x / sector_size < -ls_min_x) ls_min_x = -w1.x / sector_size + 1;
		else if (w1.x / sector_size > ls_plus_x) ls_plus_x = w1.x / sector_size + 1;
		if (w1.z / sector_size < -ls_min_z) ls_min_z = -w1.z / sector_size + 1;
		else if (w1.z / sector_size > ls_plus_z) ls_plus_z = w1.z / sector_size + 1;
	}
	// kwadratury do rysowania przedmiotów:
	GLUquadricObj *Qcyl = gluNewQuadric();
	GLUquadricObj *Qdisk = gluNewQuadric();
	GLUquadricObj *Qsph = gluNewQuadric();


	


	// RYSOWANIE PRZEDMIOTÓW
	for (int ww = wwx - liczba_sekt; ww <= wwx + liczba_sekt; ww++)       // po sektorach w otoczeniu kamery
		for (int kk = kkx - liczba_sekt; kk <= kkx + liczba_sekt; kk++)
		{
			//fprintf(f, "szukam sektora o w =%d, k = %d, liczba_sekt = %d\n", ww, kk,liczba_sekt);
			Sector *sek = ts->find(ww, kk);

			long liczba_prz = 0;
			if (sek) liczba_prz = sek->number_of_items;

			for (long prz = 0; prz < liczba_prz; prz++)
			{
				Item *p = sek->wp[prz];
				if (p->display_number != this->number_of_displays)   // o ile jeszcze nie zosta³ wyœwietlony w tym cyklu rysowania
				{
					p->display_number = number_of_displays;          // by nie wyœwietlaæ wielokrotnie, np. gdy przedmiot jest w wielu sektorach
					float liczba_pix_wid = NumberOfVisiblePixels(p->vPos, p->diameter_visual);
					bool czy_widoczny = (liczba_pix_wid >= 0 ? 1 : 0);

					glPushMatrix();

					glTranslatef(p->vPos.x, p->vPos.y, p->vPos.z);
					//glRotatef(k.w*180.0/PI,k.x,k.y,k.z);
					//glScalef(length,height,width);
					switch (p->type)
					{
					case ITEM_COIN:
						//gluCylinder(Qcyl,radius1,radius2,height,10,20);
						if (p->to_take)
						{
							GLfloat Surface[] = { 2.0f, 2.0f, 1.0f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
							glRotatef(90, 1, 0, 0);
							float grubosc = 0.2*p->diameter;
							//gluDisk(Qdisk,0,p->diameter,10,10);
							//gluCylinder(Qcyl,p->diameter,p->diameter,grubosc,10,20);
							if (liczba_pix_wid < (int)3.0 / (detail_level + 0.001))        // przy ma³ych widocznych rozmiarach rysowanie punktu
							{
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();
							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 3);
								if (liczba < 5) liczba = 5;
								if (liczba > 15) liczba = 15;
								gluDisk(Qdisk, 0, p->diameter, liczba, liczba * 2);
								gluCylinder(Qcyl, p->diameter, p->diameter, grubosc, liczba, liczba * 2);
							}
							if (liczba_pix_wid > 5)
							{
								glRasterPos2f(0.30, 1.20);
								glPrint("%d", (int)p->value);
							}
						}
						break;
					case ITEM_BARREL:
						//gluCylinder(Qcyl,radius1,radius2,height,10,20);
						if (p->to_take)
						{
							GLfloat Surface[] = { 0.50f, 0.45f, 0.0f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
							if (liczba_pix_wid < (int)3.0 / (detail_level + 0.001))        // przy ma³ych widocznych rozmiarach rysowanie punktu
							{
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();
							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 3);
								if (liczba < 5) liczba = 5;
								if (liczba > 15) liczba = 15;
								glRotatef(90, 1, 0, 0);

								float grubosc = 2 * p->diameter;
								gluDisk(Qdisk, 0, p->diameter, liczba, liczba);
								gluCylinder(Qcyl, p->diameter, p->diameter, grubosc, liczba, liczba * 2);
							}
							if (liczba_pix_wid > 5)
							{
								glRasterPos2f(0.30, 1.20);
								glPrint("%d", (int)p->value);
							}
						}
						break;
					case ITEM_TREE:
					{
						float wariancja_ksztaltu = 0.2;     // poprawka wielkoœci i koloru zale¿¹ od po³o¿enia (wsp. x,z)
						float wariancja_koloru = 0.2;        // wszêdzie wiêc (u ró¿nych klientów) drzewa bêd¹ wygl¹da³y tak samo
						float poprawka_kol1 = (2 * fabs(p->vPos.x / 600 - floor(p->vPos.x / 600)) - 1)*wariancja_koloru,
							poprawka_kol2 = (2 * fabs(p->vPos.z / 600 - floor(p->vPos.z / 600)) - 1)*wariancja_koloru;
						switch (p->subtype)
						{
						case TREE_POPLAR:
						{
							GLfloat Surface[] = { 0.5f, 0.5f, 0.0f, 1.0f };
							GLfloat KolorPnia[] = { 0.4 + poprawka_kol1 / 3, 0.6 + poprawka_kol2 / 3, 0.0f, 1.0f };
							GLfloat KolorLisci[] = { 2 * poprawka_kol1, 0.9 + 2 * poprawka_kol2, 0.0f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorPnia);
							if (liczba_pix_wid < (int)10.0 / (detail_level + 0.001))
							{
								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();
							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 20);
								if (liczba < 3) liczba = 3;
								if (liczba > 10) liczba = 10;

								glPushMatrix();
								glRotatef(90, 1, 0, 0);
								float poprawka_ksz1 = (2 * fabs(p->vPos.x / 35 - floor(p->vPos.x / 35)) - 1)*wariancja_ksztaltu;
								float poprawka_ksz2 = (2 * fabs(p->vPos.z / 15 - floor(p->vPos.z / 15)) - 1)*wariancja_ksztaltu;
								float height = p->value;
								float sredn = p->param_f[0] * p->diameter*(1 + poprawka_ksz1);
								gluCylinder(Qcyl, sredn / 2, sredn, height*1.1, liczba, liczba * 2);
								glPopMatrix();

								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								//glTranslatef(0,height,0);

								glScalef(sredn, 3 * (1 + poprawka_ksz2), sredn);

								gluSphere(Qsph, 3.0, liczba * 2, liczba * 2);
							}
							break;
						}
						case TREE_SPRUCE:
						{
							GLfloat KolorPnia[] = { 0.65f, 0.3f, 0.0f, 1.0f };
							GLfloat KolorLisci[] = { 0.0f, 0.70 + poprawka_kol1, 0.2 + poprawka_kol2, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorPnia);
							if (liczba_pix_wid < (int)10.0 / (detail_level + 0.001))
							{
								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();

							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 20);
								if (liczba < 3) liczba = 3;
								if (liczba > 10) liczba = 10;

								glPushMatrix();
								glRotatef(90, 1, 0, 0);
								float height = p->value;
								float radius = p->param_f[0] * p->diameter / 2;
								gluCylinder(Qcyl, radius, radius * 2, height*1.1, liczba, liczba * 2);

								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glTranslatef(0, 0, height * 2 / 4);
								gluCylinder(Qcyl, radius * 2, radius * 8, height / 4, liczba, liczba * 2);
								glTranslatef(0, 0, -height * 1 / 4);
								gluCylinder(Qcyl, radius * 2, radius * 6, height / 4, liczba, liczba * 2);
								glTranslatef(0, 0, -height * 1 / 3);
								gluCylinder(Qcyl, 0, radius * 4, height / 3, liczba, liczba * 2);

								glPopMatrix();
							}
							break;
						}
						case TREE_BAOBAB:
						{
							GLfloat KolorPnia[] = { 0.5f, 0.9f, 0.2f, 1.0f };
							GLfloat KolorLisci[] = { 0.0f, 0.9 + poprawka_kol2, 0.2 + poprawka_kol1, 1.0f };

							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorPnia);

							if (liczba_pix_wid < (int)10.0 / (detail_level + 0.001))
							{
								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();
							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 20);
								if (liczba < 3) liczba = 3;
								if (liczba > 10) liczba = 10;
								glPushMatrix();
								glRotatef(90, 1, 0, 0);
								float height = p->value;
								float sredn = p->param_f[0] * p->diameter;
								gluCylinder(Qcyl, p->diameter / 2, sredn, height*1.1, liczba, liczba * 2);
								glPopMatrix();

								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glPushMatrix();
								float s = 2 + height / 20;
								glScalef(s, s / 2, s);
								gluSphere(Qsph, 3, liczba * 2, liczba * 2);
								glPopMatrix();
								glTranslatef(0, -s / 1.5, 0);
								glScalef(s*2.2, s / 2, s*2.2);
								gluSphere(Qsph, 3, liczba, liczba);
							}
							break;
						}
						case TREE_FANTAZJA:
						{
							GLfloat KolorPnia[] = { 0.65f, 0.3f, 0.0f, 1.0f };                    // kolor pnia i ga³êzi
							GLfloat KolorLisci[] = { 0.5f, 0.65 + poprawka_kol1, 0.2f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorPnia);

							if (liczba_pix_wid < (int)10.0 / (detail_level + 0.001))
							{
								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								glPointSize((int)liczba_pix_wid*0.3);
								glBegin(GL_POINTS);
								glVertex3f(0, 0, 0);
								glEnd();
							}
							else
							{
								int liczba = (int)(liczba_pix_wid*detail_level / 20);
								if (liczba < 3) liczba = 3;
								if (liczba > 10) liczba = 10;
								glPushMatrix();
								glRotatef(90, 1, 0, 0);
								float height = p->value, radius_podst = p->param_f[0] * p->diameter / 2,
									radius_wierzch = p->param_f[0] * p->diameter / 5;
								gluCylinder(Qcyl, radius_wierzch * 2, radius_podst * 2, height*1.1, liczba, liczba * 2);  // rysowanie pnia
								glPopMatrix();
								glPushMatrix();
								// kolor liœci
								if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
								else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);
								//glTranslatef(0,height,0);
								float s = 0.5 + height / 15;    // œrednica listowia      
								glScalef(s, s, s);
								gluSphere(Qsph, 3, liczba * 2, liczba * 2);        // rysowanie pêku liœci na wierzcho³ku
								glPopMatrix();

								int liczba_gal = (int)(height / 3.5) + (int)(height * 10) % 3;             // liczba ga³êzi
								float prop_podz = 0.25 + (float)((int)(height * 13) % 10) / 100;           // proporcja wysokoœci na której znajduje siê ga³¹Ÿ od poprzedniej galezi lub gruntu
								float prop = 1, wys = 0;

								for (int j = 0; j < liczba_gal; j++)
								{
									glPushMatrix();
									prop *= prop_podz;
									wys += (height - wys)*prop_podz;              // wysokoœæ na której znajduje siê i-ta ga³¹Ÿ od poziomu gruntu

									float kat = 3.14 * 2 * (float)((int)(wys * 107) % 10) / 10;    // kat w/g pnia drzewa
									float sredn = (radius_podst * 2 - wys / height*(radius_podst * 2 - radius_wierzch * 2)) / 2;
									float dlug = sredn * 2 + sqrt(height - wys);
									if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
									else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorPnia);

									glTranslatef(0, wys - height, 0);
									glRotatef(kat * 180 / 3.14, 0, 1, 0);
									gluCylinder(Qcyl, sredn, sredn / 2, dlug, liczba, liczba);
									if (p->if_selected) glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
									else glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, KolorLisci);

									float ss = s*sredn / radius_podst * 2 * 1.5;
									glTranslatef(0, 0, dlug + ss * 2);
									glScalef(ss, ss, ss);
									gluSphere(Qsph, 3, liczba, liczba);        // rysowanie pêku liœci na wierzcho³ku

									glPopMatrix();
								}
							}

							break;
						} // case Fantazja

						} // switch po typach drzew
						break;
					} // drzewo
					case ITEM_POINT:
					{
						//gluCylinder(Qcyl,radius1,radius2,height,10,20);
						if (terrain_edition_mode)
						{
							GLfloat Surface[] = { 2.0f, 0.0f, 0.0f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
							//glRotatef(90, 1, 0, 0);
							glTranslatef(0, -p->value, 0);
							glPointSize(3);
							glBegin(GL_POINTS);
							glVertex3f(0, 0, 0);
							glEnd();
							glTranslatef(0, p->value, 0);
							if (p->subtype == POINT_ORDINAL)
							{
								if (liczba_pix_wid > 5)
								{
									glRasterPos2f(0.30, 1.20);
									glPrint("point");
								}
							}
							else if (p->subtype == POINT_OF_WATER)
							{
								glLineWidth(3);

								glBegin(GL_LINES);
								glVertex3f(0, 0, 0);
								glVertex3f(0, 30, 0);
								glEnd();
								if (liczba_pix_wid > 5)
								{
									glRasterPos2f(0.30, 1.20);
									glPrint("water");
								}
							}
							else if (p->subtype == POINT_OF_VEHICLE)
							{
								glLineWidth(3);

								glBegin(GL_LINES);
								glVertex3f(0, 0, 0);
								glVertex3f(0, 30, 0);
								glEnd();
								if (liczba_pix_wid > 5)
								{
									glRasterPos2f(0.30, 1.20);
									glPrint("vehicle");
								}
							}
						}
						break;
					}
					case ITEM_EDGE:
					{
						//gluCylinder(Qcyl,radius1,radius2,height,10,20);
						if (terrain_edition_mode)
						{
							GLfloat Surface[] = { 2.0f, 0.0f, 0.0f, 1.0f };
							if (p->if_selected)
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
							else
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
							//glRotatef(90, 1, 0, 0);
							long nr_punktu1 = p->param_i[0], nr_punktu2 = p->param_i[1];
							Vector3 pol = (this->p[nr_punktu1].vPos + this->p[nr_punktu2].vPos) / 2;
							glTranslatef(-p->vPos.x, -p->vPos.y, -p->vPos.z);
							//glTranslatef(-pol.x, -pol.y, -pol.z);
							glLineWidth(2);

							glBegin(GL_LINES);
							glVertex3f(this->p[nr_punktu1].vPos.x, this->p[nr_punktu1].vPos.y + this->p[nr_punktu1].value, this->p[nr_punktu1].vPos.z);
							glVertex3f(this->p[nr_punktu2].vPos.x, this->p[nr_punktu2].vPos.y + this->p[nr_punktu2].value, this->p[nr_punktu2].vPos.z);
							glEnd();

							//glTranslatef(p->vPos.x, p->vPos.y, p->vPos.z);
							glTranslatef(pol.x, pol.y, pol.z);
							if (liczba_pix_wid > 5)
							{
								glRasterPos2f(0.30, 1.20);
								glPrint("krawedz");
							}
							glTranslatef(-pol.x, -pol.y, -pol.z);
						}
						break;
					}
					case ITEM_GATE:
					{

						GLfloat Surface[] = { 0.3f, 0.3f, 0.9f, 0.8f };
						GLfloat Surface2[] = { 0.8f, 0.8f, 0.0f, 0.8f };
						if (p->if_selected)
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
						else
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
						//glRotatef(90, 1, 0, 0);
						long nr_punktu1 = p->param_i[0], nr_punktu2 = p->param_i[1];
						Vector3 pol = (this->p[nr_punktu1].vPos + this->p[nr_punktu2].vPos) / 2;
						glTranslatef(-p->vPos.x, -p->vPos.y, -p->vPos.z);
						//glTranslatef(-pol.x, -pol.y, -pol.z);
						glLineWidth(2);

						glBegin(GL_QUADS);
						glVertex3f(this->p[nr_punktu1].vPos.x, this->p[nr_punktu1].vPos.y, this->p[nr_punktu1].vPos.z);
						glVertex3f(this->p[nr_punktu2].vPos.x, this->p[nr_punktu2].vPos.y, this->p[nr_punktu2].vPos.z);
						glVertex3f(this->p[nr_punktu2].vPos.x, this->p[nr_punktu2].vPos.y - p->param_f[0], this->p[nr_punktu2].vPos.z);
						glVertex3f(this->p[nr_punktu1].vPos.x, this->p[nr_punktu1].vPos.y - p->param_f[0], this->p[nr_punktu1].vPos.z);
						glEnd();

						//glTranslatef(p->vPos.x, p->vPos.y, p->vPos.z);
						bool czy_w_trasie = false;
						if ((p->group > -1) && (p->param_i[2] > -1)) // bramka w grupie i ma numer w grupie, czyli bramka nale¿y do trasy
						{
							czy_w_trasie = true;
							if (p->param_i[2] == 0) // pierwsza bramka -> rysowanie pola startowego
							{
								Vector3 w = this->p[nr_punktu2].vPos - this->p[nr_punktu1].vPos;                  // wektor pomiêdzy drzewami bramki
								Vector3 wp1 = Vector3(w.z, 0, -w.x), wp2 = Vector3(-w.z, 0, w.x);     // wektory prostopad³e w lewo i w prawo 
								// wyznaczenie punktów kluczowych pola startowego:
								// 
								//           A.--------------.B
								//       wp1  |              |
								//            |       w      |
								//  drzewo 1  o------------->o  drzewo 2
								//            |              |
								//       wp2  |              |
								//           C.--------------.D
								//
								float wsp_dlg = 0.5;  // spó³czynnik d³ugoœci pola
								Vector3 A = this->p[nr_punktu1].vPos + wp1*wsp_dlg, B = this->p[nr_punktu2].vPos + wp1*wsp_dlg,
									C = this->p[nr_punktu1].vPos + wp2*wsp_dlg, D = this->p[nr_punktu2].vPos + wp2*wsp_dlg;
								A.y = this->GeneralHeight(A); B.y = this->GeneralHeight(B);
								C.y = this->GeneralHeight(C); D.y = this->GeneralHeight(D);
								// rysowanie pola:
								glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface2);
								glBegin(GL_QUADS);
								glVertex3f(A.x, A.y + 1, A.z);
								glVertex3f(A.x, A.y - 5, A.z);
								glVertex3f(B.x, B.y - 5, B.z);
								glVertex3f(B.x, B.y + 1, B.z);

								glVertex3f(B.x, B.y - 5, B.z);
								glVertex3f(B.x, B.y + 1, B.z);
								glVertex3f(D.x, D.y + 1, D.z);
								glVertex3f(D.x, D.y - 5, D.z);

								glVertex3f(A.x, A.y + 1, A.z);
								glVertex3f(A.x, A.y - 5, A.z);
								glVertex3f(C.x, C.y - 5, C.z);
								glVertex3f(C.x, C.y + 1, C.z);

								glVertex3f(C.x, C.y - 5, C.z);
								glVertex3f(C.x, C.y + 1, C.z);
								glVertex3f(D.x, D.y + 1, D.z);
								glVertex3f(D.x, D.y - 5, D.z);
								glEnd();

							}
						}
						glTranslatef(pol.x, pol.y, pol.z);
						if ((liczba_pix_wid > 5) && (terrain_edition_mode))
						{
							glRasterPos2f(0.30, 1.20);
							if (czy_w_trasie)
								glPrint("bramka_%d__w_trasie_%d", p->param_i[2], p->group);
							else
								glPrint("bramka");
						}
						else if (liczba_pix_wid > 5)
						{
							glRasterPos2f(0.30, 1.20);
							glPrint("%d", (int)p->value);
						}
						glTranslatef(-pol.x, -pol.y, -pol.z);

						break;
					}
					case ITEM_WALL:
					{
						GLfloat Surface[] = { 0.7f, 0.7f, 0.7f, 1.0f };
						if (p->if_selected)
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, SelectionSurface);
						else
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
						//glRotatef(90, 1, 0, 0);
						long nr_punktu1 = p->param_i[0], nr_punktu2 = p->param_i[1];
						Vector3 pol1 = this->p[nr_punktu1].vPos, pol2 = this->p[nr_punktu2].vPos, vertical = Vector3(0, 1, 0);
						pol1.y += this->p[nr_punktu1].value;
						pol2.y += this->p[nr_punktu2].value;

						Vector3 pol = (pol1 + pol2) / 2;
						Vector3 wprzod = (pol2 - pol1).znorm();             // direction od punktu 1 do punktu 2
						Vector3 wlewo = vertical*wprzod;                        // direction w lewo, gdyby patrzeæ w kierunku wprzod
						float grubosc = p->param_f[0];     // gruboœæ muru
						float height = p->value;       // wysokoœæ muru (tzn. gruboœæ w pionie) niezale¿nie od pionowego umiejscowienia

						//glTranslatef(-pol.x, -pol.y, -pol.z);							
						glTranslatef(-p->vPos.x, -p->vPos.y, -p->vPos.z);

						// punkty kluczowe:
						Vector3 A = pol1 - wlewo*grubosc / 2 + vertical*height / 2;
						Vector3 B = A + wlewo*grubosc;
						Vector3 C = B - vertical*height;
						Vector3 D = A - vertical*height;
						Vector3 Ap = A + pol2 - pol1, Bp = B + pol2 - pol1,
							Cp = C + pol2 - pol1, Dp = D + pol2 - pol1;

						glBegin(GL_QUADS);
						// sciana ABCD:
						Vector3 N = vertical*wlewo;
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(A.x, A.y, A.z);
						glVertex3f(B.x, B.y, B.z);
						glVertex3f(C.x, C.y, C.z);
						glVertex3f(D.x, D.y, D.z);

						glNormal3f(-N.x, -N.y, -N.z);
						glVertex3f(Ap.x, Ap.y, Ap.z);
						glVertex3f(Dp.x, Dp.y, Dp.z);
						glVertex3f(Cp.x, Cp.y, Cp.z);
						glVertex3f(Bp.x, Bp.y, Bp.z);

						glNormal3f(-wlewo.x, -wlewo.y, -wlewo.z);
						glVertex3f(A.x, A.y, A.z);
						glVertex3f(D.x, D.y, D.z);
						glVertex3f(Dp.x, Dp.y, Dp.z);
						glVertex3f(Ap.x, Ap.y, Ap.z);

						glNormal3f(wlewo.x, wlewo.y, wlewo.z);
						glVertex3f(B.x, B.y, B.z);
						glVertex3f(Bp.x, Bp.y, Bp.z);
						glVertex3f(Cp.x, Cp.y, Cp.z);
						glVertex3f(C.x, C.y, C.z);

						N = wprzod*wlewo;
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(A.x, A.y, A.z);
						glVertex3f(Ap.x, Ap.y, Ap.z);
						glVertex3f(Bp.x, Bp.y, Bp.z);
						glVertex3f(B.x, B.y, B.z);

						glNormal3f(-N.x, -N.y, -N.z);
						glVertex3f(D.x, D.y, D.z);
						glVertex3f(C.x, C.y, C.z);
						glVertex3f(Cp.x, Cp.y, Cp.z);
						glVertex3f(Dp.x, Dp.y, Dp.z);

						glEnd();
						//glTranslatef(p->vPos.x, p->vPos.y, p->vPos.z);

						if (terrain_edition_mode)
						{
							glTranslatef(pol.x, pol.y + height / 2, pol.z);
							if (liczba_pix_wid > 5)
							{
								glRasterPos2f(0.30, 1.20);
								glPrint("mur");
							}
							glTranslatef(-pol.x, -pol.y - height / 2, -pol.z);
						}
						//fprintf(f, "Narysowano mur pomiedzy punktami %d (%f,%f,%f) a %d (%f,%f,%f)\n", nr_punktu1, pol1.x, pol1.y, pol1.z,
						//	nr_punktu2, pol2.x, pol2.y, pol2.z);
						break;
					}
					} // switch  po typach przedmiotów

					glPopMatrix();
				} // jeœli przedmiot nie zosta³ ju¿ wyœwietlony w tej funkcji
			} // po przedmiotach z sektora

		} // po sektorach

	// Rysowanie powierzchni terenu oraz wody:
	for (int ww = wwx - ls_min_z; ww <= wwx + ls_plus_z; ww++)       // po sektorach w otoczeniu kamery (w kierunku osi z)
		for (int kk = kkx - ls_min_x; kk <= kkx + ls_plus_x; kk++)  // _||- (w kierunku osi x)
		{
			//fprintf(f, "szukam sektora o w =%d, k = %d, liczba_sekt = %d\n", ww, kk,liczba_sekt);
			Sector *sek = ts->find(ww, kk);
			//float pocz_x = -rozmiar_pola*lkolumn / 2,     // wspó³rzêdne lewego górnego krañca terrainu
			//	pocz_z = -rozmiar_pola*lwierszy / 2;
			float pocz_x = (kk-0.5)*sector_size;
			float pocz_z = (ww-0.5)*sector_size;
			if ((sek) && ((sek->map_of_heights)||(sek->map_of_heights_in_edit)))
			{
				//fprintf(f, "znaleziono Sector o w = %d, k = %d\n", sek->w, sek->k);
				long loczek = 0;
				float **mapa;
				Vector3 ****__Norm;
				if (sek->map_of_heights_in_edit)
				{
					mapa = sek->map_of_heights_in_edit;
					__Norm = sek->normal_vectors_in_edit;
					loczek = sek->number_of_cells_in_edit;
					//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, OrangeSurface);
				}
				else
				{
					mapa = sek->map_of_heights;
					__Norm = sek->normal_vectors;
					loczek = sek->number_of_cells;
					//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);
				}

				// Tutaj nale¿y ustaliæ na ile widoczny (blisko obserwatora) jest Sector i w zal. od tego wybraæ rozdzielczoœæ
				float rozmiar_pola_r0 = 1;
				if (loczek > 0) rozmiar_pola_r0 = sector_size / loczek;
				Vector3 pol = Vector3(kk*sector_size, sek->height_max, ww*sector_size);

				long nr_rozdz = 0;             // nr_rozdzielczoœæ: 0 - podstawowa, kolejne numery odpowiadaj¹ coraz mniejszym rozdzielczoœciom

				float liczba_pix_wid = NumberOfVisiblePixels(pol, rozmiar_pola_r0);
				if (liczba_pix_wid < 20/(detail_level + 0.05) > 0)
					nr_rozdz = 1 + (int)log2(20/(detail_level + 0.05) / liczba_pix_wid);			
				
				long zmn = (1<<nr_rozdz);     // zmiejszenie rozdzielczoœci (jeœli Sector daleko od obserwatora)
				if (zmn > loczek)
				{
					zmn = loczek;
					if (loczek > 0)
						nr_rozdz = log2(loczek);
				}
				//long loczek_nr_rozdz = loczek / zmn;                          // zmniejszona liczba oczek
				
				//sek->number_of_cells_displayed_prev = sek->number_of_cells_displayed;
				sek->number_of_cells_displayed_prev = sek->number_of_cells_displayed;
				sek->number_of_cells_displayed = loczek / zmn;                          // rozdzielczoœæ do wyœwietlenia
				long loczek_nr_rozdz = sek->number_of_cells_displayed_prev;
				float rozmiar_pola = (loczek_nr_rozdz > 0 ? sector_size / loczek_nr_rozdz : 1);       // size pola dla aktualnej rozdzielczoœci
				zmn = (loczek_nr_rozdz > 0 ? loczek / loczek_nr_rozdz : 1);
				if (zmn > 0)
					nr_rozdz = log2(zmn);

				Sector *s_lewy = ts->find(ww, kk - 1), *s_prawy = ts->find(ww, kk + 1),   // sectors s¹siednie
					*s_dolny = ts->find(ww + 1, kk), *s_gorny = ts->find(ww - 1, kk);

				
				// tworze listê wyœwietlania rysuj¹c poszczególne pola mapy za pomoc¹ trójk¹tów 
				// (po 4 trójk¹ty na ka¿de pole):
				enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };

				Vector3 A, B, C, D, E, N;
				//glNewList(TerrainSurface, GL_COMPILE);
				glBegin(GL_TRIANGLES);
				for (long w = 0; w < loczek_nr_rozdz; w++)
					for (long k = 0; k < loczek_nr_rozdz; k++)
					{						
						if (k > 0)
						{
							A = C;     // dziêki temu, ¿e poruszamy siê z oczka na oczko w okreœlonym kierunku, mo¿emy zaoszczêdziæ
							D = E;     // trochê obliczeñ
						}
						else
						{
							A = Vector3(pocz_x + k*rozmiar_pola, mapa[w * 2 * zmn][k*zmn], pocz_z + w*rozmiar_pola);
							D = Vector3(pocz_x + k*rozmiar_pola, mapa[(w + 1) * 2 * zmn][k*zmn], pocz_z + (w + 1)*rozmiar_pola);
						}
						B = Vector3(pocz_x + (k + 0.5)*rozmiar_pola, mapa[(w * 2 + 1)*zmn][k*zmn + (zmn>1)*zmn / 2], pocz_z + (w + 0.5)*rozmiar_pola);
						C = Vector3(pocz_x + (k + 1)*rozmiar_pola, mapa[w * 2 * zmn][(k + 1)*zmn], pocz_z + w*rozmiar_pola);
						E = Vector3(pocz_x + (k + 1)*rozmiar_pola, mapa[(w + 1) * 2 * zmn][(k + 1)*zmn], pocz_z + (w + 1)*rozmiar_pola);
						

						// dopasowanie brzegów sektora do nr_rozdzielczoœci sektorów s¹siednich (o ile maj¹ mniejsz¹ rozdzielczoœæ):
						if ((k==0)&&(s_lewy) && (s_lewy->number_of_cells_displayed < loczek_nr_rozdz))
						{
							//if ((s_lewy->map_of_heights == NULL) && (s_lewy->map_of_heights_in_edit == NULL)) s_lewy->number_of_cells_displayed = 1;
							int iloraz = (s_lewy->number_of_cells_displayed > 0 ? loczek_nr_rozdz / s_lewy->number_of_cells_displayed : 1);
							int reszta = 0;
							if (iloraz > 0) reszta = w % iloraz;
							if (reszta > 0)
								A.y = mapa[(w - reszta) * 2 * zmn][0] + 
								(mapa[(w - reszta + iloraz) * 2 * zmn][0] - mapa[(w - reszta) * 2 * zmn][0])*(float)reszta / iloraz;
							reszta = 0;
							if (iloraz > 0)
								reszta = (w+1) % iloraz;
							if (reszta > 0)
								D.y = mapa[(w + 1 - reszta) * 2 * zmn][0] + 
								(mapa[(w + 1 - reszta + iloraz) * 2 * zmn][0] - mapa[(w + 1 - reszta) * 2 * zmn][0])*(float)reszta / iloraz;
						}
						if ((k == loczek_nr_rozdz - 1) && (s_prawy) && (s_prawy->number_of_cells_displayed < loczek_nr_rozdz))
						{
							int iloraz = loczek_nr_rozdz / s_prawy->number_of_cells_displayed;
							int reszta = 0;
							if (iloraz > 0) reszta = w % iloraz;
							if (reszta > 0)
								C.y = mapa[(w - reszta) * 2 * zmn][loczek_nr_rozdz*zmn] + 
								(mapa[(w - reszta + iloraz) * 2 * zmn][loczek_nr_rozdz*zmn] - mapa[(w - reszta) * 2 * zmn][loczek_nr_rozdz*zmn])*(float)reszta / iloraz;
							reszta = 0;
							if (iloraz > 0)
								reszta = (w + 1) % iloraz;
							if (reszta > 0)
								E.y = mapa[(w + 1 - reszta) * 2 * zmn][loczek_nr_rozdz*zmn] + 
								(mapa[(w + 1 - reszta + iloraz) * 2 * zmn][loczek_nr_rozdz*zmn] - mapa[(w + 1 - reszta) * 2 * zmn][loczek_nr_rozdz*zmn])*(float)reszta / iloraz;
						}
						
						if ((w == 0) && (s_gorny) && (s_gorny->number_of_cells_displayed < loczek_nr_rozdz))
						{
							int iloraz = (s_gorny->number_of_cells_displayed > 0 ? loczek_nr_rozdz / s_gorny->number_of_cells_displayed : 1);
							int reszta = 0;
							if (iloraz > 0) reszta = k % iloraz;
	
							if (reszta > 0)
								A.y = mapa[0][(k - reszta)*zmn] + (mapa[0][(k - reszta + iloraz)*zmn] - mapa[0][(k - reszta)*zmn])*(float)reszta / iloraz;
							reszta = 0;
							if (iloraz > 0)
								reszta = (k + 1) % iloraz;
							if (reszta > 0)
								C.y = mapa[0][(k + 1 - reszta)*zmn] + (mapa[0][(k + 1 - reszta + iloraz)*zmn] - mapa[0][(k + 1 - reszta)*zmn])*(float)reszta / iloraz;
						}
						
						if ((w == loczek_nr_rozdz - 1) && (s_dolny) && (s_dolny->number_of_cells_displayed < loczek_nr_rozdz))
						{
							int iloraz = (s_dolny->number_of_cells_displayed > 0 ? loczek_nr_rozdz / s_dolny->number_of_cells_displayed : 1);
							int reszta = 0;
							if (iloraz > 0) reszta = k % iloraz;
							if (reszta > 0)
								D.y = mapa[loczek_nr_rozdz*zmn*2][(k - reszta)*zmn] + 
								(mapa[loczek_nr_rozdz*zmn * 2][(k - reszta + iloraz)*zmn] - mapa[loczek_nr_rozdz*zmn * 2][(k - reszta)*zmn])*(float)reszta / iloraz;
							reszta = 0;
							if (iloraz > 0)
								reszta = (k + 1) % iloraz;
							if (reszta > 0)
								E.y = mapa[loczek_nr_rozdz*zmn * 2][(k + 1 - reszta)*zmn] + 
								(mapa[loczek_nr_rozdz*zmn * 2][(k + 1 - reszta + iloraz)*zmn] - mapa[loczek_nr_rozdz*zmn * 2][(k + 1 - reszta)*zmn])*(float)reszta / iloraz;
						}
						// tworzê trójk¹t ABC w górnej czêœci kwadratu: 
						//  A o_________o C
						//    |.       .|
						//    |  .   .  | 
						//    |    o B  | 
						//    |  .   .  |
						//    |._______.|
						//  D o         o E
						enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };       // trójk¹t w którym znajduje siê punkt 
						//Vector3 AB = B - A;
						//Vector3 BC = C - B;
						//N = (AB*BC).znorm();
						if (sek->map_of_heights_in_edit)
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, OrangeSurface);
						else
							glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);

						N = __Norm[nr_rozdz][w][k][ABC];
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(A.x, A.y, A.z);
						glVertex3f(B.x, B.y, B.z);
						glVertex3f(C.x, C.y, C.z);
						//d[w][k][ABC] = -(B^N);          // dodatkowo wyznaczam wyraz wolny z równania plaszyzny trójk¹ta
						//normal_vectors[w][k][ABC] = N;          // dodatkowo zapisujê normaln¹ do p³aszczyzny trójk¹ta
						// trójk¹t ADB:
						//Vector3 AD = D - A;
						//N = (AD*AB).znorm();
						N = __Norm[nr_rozdz][w][k][ADB];
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(A.x, A.y, A.z);
						glVertex3f(D.x, D.y, D.z);
						glVertex3f(B.x, B.y, B.z);
						//d[w][k][ADB] = -(B^N);
						//normal_vectors[w][k][ADB] = N;
						// trójk¹t BDE:
						//Vector3 BD = D - B;
						//Vector3 DE = E - D;
						//N = (BD*DE).znorm();
						N = __Norm[nr_rozdz][w][k][BDE];
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(B.x, B.y, B.z);
						glVertex3f(D.x, D.y, D.z);
						glVertex3f(E.x, E.y, E.z);
						//d[w][k][BDE] = -(B^N);
						//normal_vectors[w][k][BDE] = N;
						// trójk¹t CBE:
						//Vector3 CB = B - C;
						//Vector3 BE = E - B;
						//N = (CB*BE).znorm();
						N = __Norm[nr_rozdz][w][k][CBE];
						glNormal3f(N.x, N.y, N.z);
						glVertex3f(C.x, C.y, C.z);
						glVertex3f(B.x, B.y, B.z);
						glVertex3f(E.x, E.y, E.z);
						//d[w][k][CBE] = -(B^N);
						//normal_vectors[w][k][CBE] = N;

						

					}
				glEnd();
			} // jeœli znaleziono mapê sektora
			else if (sek) // jeœli znaleziono sektor ale bez mapy
			{
				// ustawienie tekstury:
				// ....
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);
				glBegin(GL_QUADS);
				glNormal3f(0, 1, 0);
				glVertex3f(pocz_x, height_std, pocz_z);
				glVertex3f(pocz_x, height_std, pocz_z + sector_size);
				glVertex3f(pocz_x + sector_size, height_std, pocz_z + sector_size);
				glVertex3f(pocz_x + sector_size, height_std, pocz_z);

				glEnd();
			} // jeœli nie znaleziono mapy ale znaleziono Sector ze standardow¹ nawierzchni¹
			else
			{
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);
				glBegin(GL_QUADS);
				glNormal3f(0, 1, 0);
				glVertex3f(pocz_x, height_std, pocz_z);
				glVertex3f(pocz_x, height_std, pocz_z + sector_size);
				glVertex3f(pocz_x + sector_size, height_std, pocz_z + sector_size);
				glVertex3f(pocz_x + sector_size, height_std, pocz_z);



				glEnd();

			} // jeœli nie znaleziono sektora
		} // po sektorach



		// Rysowanie wody:
		for (int ww = wwx - ls_min_z; ww <= wwx + ls_plus_z; ww++)       // po sektorach w otoczeniu kamery (w kierunku osi z)
			for (int kk = kkx - ls_min_x; kk <= kkx + ls_plus_x; kk++)  // _||- (w kierunku osi x)
			{
				//fprintf(f, "szukam sektora o w =%d, k = %d, liczba_sekt = %d\n", ww, kk,liczba_sekt);
				Sector *sek = ts->find(ww, kk);
				//float pocz_x = -rozmiar_pola*lkolumn / 2,     // wspó³rzêdne lewego górnego krañca terrainu
				//	pocz_z = -rozmiar_pola*lwierszy / 2;
				float pocz_x = (kk - 0.5)*sector_size;
				float pocz_z = (ww - 0.5)*sector_size;
				if ((sek) && ((sek->map_of_heights) || (sek->map_of_heights_in_edit)))
				{
					//fprintf(f, "znaleziono Sector o w = %d, k = %d\n", sek->w, sek->k);
					long loczek = 0;
					//float **mapa;
					//Vector3 ****__Norm;
					if (sek->map_of_heights_in_edit)
					{
						//mapa = sek->map_of_heights_in_edit;
						//__Norm = sek->normal_vectors_in_edit;
						loczek = sek->number_of_cells_in_edit;
						//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, OrangeSurface);
					}
					else
					{
						//mapa = sek->map_of_heights;
						//__Norm = sek->normal_vectors;
						loczek = sek->number_of_cells;
						//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);
					}

					// Tutaj nale¿y ustaliæ na ile widoczny (blisko obserwatora) jest Sector i w zal. od tego wybraæ rozdzielczoœæ
					float rozmiar_pola_r0 = (loczek > 0 ?  sector_size / loczek : 1);
					Vector3 pol = Vector3(kk*sector_size, sek->height_max, ww*sector_size);

					long nr_rozdz = 0;             // nr_rozdzielczoœæ: 0 - podstawowa, kolejne numery odpowiadaj¹ coraz mniejszym rozdzielczoœciom

					float liczba_pix_wid = NumberOfVisiblePixels(pol, rozmiar_pola_r0);
					if (liczba_pix_wid < 20 / (detail_level + 0.05) > 0)
						nr_rozdz = 1 + (int)log2(20 / (detail_level + 0.05) / liczba_pix_wid);

					long zmn = (1 << nr_rozdz);     // zmiejszenie rozdzielczoœci (jeœli Sector daleko od obserwatora)
					if (zmn > loczek)
					{
						zmn = loczek;
						nr_rozdz = (loczek > 0 ? log2(loczek) : 1);
					}
					//long loczek_nr_rozdz = loczek / zmn;                          // zmniejszona liczba oczek

					//sek->number_of_cells_displayed_prev = sek->number_of_cells_displayed;
					sek->number_of_cells_displayed_prev = sek->number_of_cells_displayed;
					sek->number_of_cells_displayed = (zmn > 0 ? loczek / zmn : 1);                          // rozdzielczoœæ do wyœwietlenia
					long loczek_nr_rozdz = sek->number_of_cells_displayed_prev;
					float rozmiar_pola = (loczek_nr_rozdz > 0 ?  sector_size / loczek_nr_rozdz : 1);       // size pola dla aktualnej rozdzielczoœci
					zmn = (loczek_nr_rozdz > 0 ? loczek / loczek_nr_rozdz : 1);
					if (zmn > 0)
						nr_rozdz = log2(zmn);

					Sector *s_lewy = ts->find(ww, kk - 1), *s_prawy = ts->find(ww, kk + 1),   // sectors s¹siednie
						*s_dolny = ts->find(ww + 1, kk), *s_gorny = ts->find(ww - 1, kk);


					// tworze listê wyœwietlania rysuj¹c poszczególne pola mapy za pomoc¹ trójk¹tów 
					// (po 4 trójk¹ty na ka¿de pole):
					enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };

					Vector3 A, B, C, D, E, N;
					//glNewList(TerrainSurface, GL_COMPILE);
				
					if ((sek->level_of_water))  // rysowanie powierzchni wody
					{
						glBegin(GL_TRIANGLES);
						glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, WaterSurface);
						glNormal3f(0, 1, 0);
						for (long w = 0; w < loczek_nr_rozdz; w++)
							for (long k = 0; k < loczek_nr_rozdz; k++)
							{
								if (k > 0)
								{
									A = C;     // dziêki temu, ¿e poruszamy siê z oczka na oczko w okreœlonym kierunku, mo¿emy zaoszczêdziæ
									D = E;     // trochê obliczeñ
								}
								else
								{
									A = Vector3(pocz_x + k*rozmiar_pola, 0, pocz_z + w*rozmiar_pola);
									D = Vector3(pocz_x + k*rozmiar_pola, 0, pocz_z + (w + 1)*rozmiar_pola);
								}
								B = Vector3(pocz_x + (k + 0.5)*rozmiar_pola, 0, pocz_z + (w + 0.5)*rozmiar_pola);
								C = Vector3(pocz_x + (k + 1)*rozmiar_pola, 0, pocz_z + w*rozmiar_pola);
								E = Vector3(pocz_x + (k + 1)*rozmiar_pola, 0, pocz_z + (w + 1)*rozmiar_pola);

								// tworzê trójk¹t ABC w górnej czêœci kwadratu: 
								//  A o_________o C
								//    |.       .|
								//    |  .   .  | 
								//    |    o B  | 
								//    |  .   .  |
								//    |._______.|
								//  D o         o E
								enum tr{ ABC = 0, ADB = 1, BDE = 2, CBE = 3 };       // trójk¹t w którym znajduje siê punkt 


								float yy = sek->level_of_water[w / zmn][k / zmn][ABC];
								if (yy > -1e10)
								{
									glVertex3f(A.x, yy, A.z);
									glVertex3f(B.x, yy, B.z);
									glVertex3f(C.x, yy, C.z);
								}
								yy = sek->level_of_water[w / zmn][k / zmn][ADB];
								if (yy > -1e10)
								{
									glVertex3f(A.x, yy, A.z);
									glVertex3f(D.x, yy, D.z);
									glVertex3f(B.x, yy, B.z);
								}
								yy = sek->level_of_water[w / zmn][k / zmn][BDE];
								if (yy > -1e10)
								{
									glVertex3f(B.x, yy, B.z);
									glVertex3f(D.x, yy, D.z);
									glVertex3f(E.x, yy, E.z);
								}
								yy = sek->level_of_water[w / zmn][k / zmn][CBE];
								if (yy > -1e10)
								{
									glVertex3f(C.x, yy, C.z);
									glVertex3f(B.x, yy, B.z);
									glVertex3f(E.x, yy, E.z);
								}

							} // po oczkach
						glEnd();
					} // jeœli mapa poziomów wody
					else if (level_of_water_std > -1e10) // jeœli nie ma mapy poziomów wody
					{
						glBegin(GL_QUADS);
						glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, WaterSurface);
						glNormal3f(0, 1, 0);
						glVertex3f(pocz_x, level_of_water_std, pocz_z);
						glVertex3f(pocz_x, level_of_water_std, pocz_z + sector_size);
						glVertex3f(pocz_x + sector_size, level_of_water_std, pocz_z + sector_size);
						glVertex3f(pocz_x + sector_size, level_of_water_std, pocz_z);
						glEnd();
					}

				} // jeœli znaleziono mapê sektora
				else // jeœli sektor jest bez mapy lub nie znaleziono sektora
				{
					// ustawienie tekstury:
					// ....

					glBegin(GL_QUADS);
					glNormal3f(0, 1, 0);
			
					if (this->level_of_water_std > this->height_std)
					{
						glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, WaterSurface);
						glVertex3f(pocz_x, level_of_water_std, pocz_z);
						glVertex3f(pocz_x, level_of_water_std, pocz_z + sector_size);
						glVertex3f(pocz_x + sector_size, level_of_water_std, pocz_z + sector_size);
						glVertex3f(pocz_x + sector_size, level_of_water_std, pocz_z);
					}
					glEnd();
				} // jeœli nie znaleziono mapy ale znaleziono Sector ze standardow¹ nawierzchni¹

			} // po sektorach


	// RYSOWANIE OKULARÓW:
	// okular wodny (gdy kamera jest pod wod¹):
	Vector3 pol_kam2 = pol_kam - kierunek_kam*par_view.distance;
	float level_of_water = LevelOfWater(pol_kam2.x, pol_kam2.z);
	if (pol_kam2.y < level_of_water)
	{
		glPushMatrix();
		glTranslatef(pol_kam2.x, pol_kam2.y, pol_kam2.z);
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, WaterSurface);
		gluSphere(Qsph, 2, 4, 4);
		glPopMatrix();
	}
	
	gluDeleteQuadric(Qcyl); gluDeleteQuadric(Qdisk); gluDeleteQuadric(Qsph);
	//glCallList(Floor);     
	this->number_of_displays++;
}

