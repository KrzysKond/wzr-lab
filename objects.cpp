/*********************************************************************
	Simulation obiektów fizycznych ruchomych np. samochody, statki, roboty, itd.
	+ obs³uga obiektów statycznych np. env.
	**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include "objects.h"
#include "graphics.h"
#include <random>
#include <iostream>

extern FILE *f;
extern Environment env;

extern bool if_ID_visible;



MovableObject::MovableObject()             // konstruktor                   
{
	iID = (unsigned int)(rand() % 2000);  // identyfikator obiektu
	fprintf(f, "my_car->iID = %d\n", iID);

	// zmienne zwi¹zame z akcjami kierowcy
	F = Fb = 0;	// si³y dzia³aj¹ce na obiekt 
	breaking_factor = 0;	    // stopieñ hamowania
	steer_wheel_speed = 0;      // prêdkoœæ krêcenia kierownic¹ w rad/s
	if_keep_steer_wheel = 0;    // informacja czy kieronica jest trzymana

	// sta³e samochodu
	mass_own = 14.0;			// masa obiektu [kg]
	//Fy = mass_own*9.81;        // si³a nacisku na podstawê obiektu (na ko³a pojazdu)
	length = 9.0;
	width = 3.0;
	height = 1.3;
	clearance = 0.0;     // wysokoœæ na której znajduje siê podstawa obiektu
	front_axis_dist = 1.0;     // odleg³oœæ od przedniej osi do przedniego zderzaka 
	back_axis_dist = 0.2;       // odleg³oœæ od tylniej osi do tylniego zderzaka
	steer_wheel_ret_speed = 0.5; // prêdkoœæ powrotu kierownicy w rad/s (gdy zostateie puszczona)

	// parametry stateu auta:
	state.steering_angle = 0;
	state.vPos.y = clearance + height / 2 + 20; // wysokoœæ œrodka ciê¿koœci w osi pionowej pojazdu
	state.vPos.x = 200;
	state.vPos.z = -300;
	quaternion qObr = AsixToQuat(Vector3(0, 1, 0), 1.1*PI / 180.0); // obrót obiektu o k¹t 30 stopni wzglêdem osi y:
	state.qOrient = qObr*state.qOrient;
}

MovableObject::~MovableObject()            // destruktor
{
}

void MovableObject::ChangeState(ObjectState __state)  // przepisanie podanego stateu 
{                                                // w przypadku obiektów, które nie s¹ symulowane
	state = __state;
}

void MovableObject::FindPosition(MovableObject* other_cars, int car_count)
{
	Vector3 furthest_pos = { 0.f, 0.f, 0.f };


}

float GetRandomPos(float map_bounds)
{
	return fmodf((float)rand(), map_bounds);
}

void MovableObject::FindPosition(std::map<int, MovableObject*>& other_cars)
{
	Vector3 furthest_pos = { 0.f, 0.f, 0.f };
	float best_length{FLT_MIN};



	for (int i = 0; i < 100; ++i)
	{

		Vector3 random_pos{GetRandomPos(env.max_bounds.x), GetRandomPos(env.max_bounds.y), GetRandomPos(env.max_bounds.z) };

		uint32_t car_count{ (uint32_t)other_cars.size() };
		for (auto& car : other_cars)
		{
			//if (car.first == this->iID) continue;
			Vector3 pos{ car.second->state.vPos };

			Vector3 diff{ pos - random_pos };ds
			if (diff.length() > best_length)
			{
				furthest_pos = random_pos;
				best_length = diff.length();
			}
		}
	}

	this->state.vPos = furthest_pos;
	char buffer[256]{};
	snprintf(buffer, 256, "FOund pos: %f %f %f\n", furthest_pos.x, furthest_pos.y, furthest_pos.z);
	OutputDebugString(buffer);
}

ObjectState MovableObject::State()                // metoda zwracaj¹ca state obiektu ³¹cznie z iID
{
	return state;
}



void MovableObject::Simulation(float dt)          // obliczenie nowego stateu na podstawie dotychczasowego,
{                                                // dzia³aj¹cych si³ i czasu, jaki up³yn¹³ od ostatniej symulacji

	if (dt == 0) return;

	float friction = 3.0;            // wspó³czynnik tarcia obiektu o pod³o¿e 
	float friction_rot = friction;     // friction obrotowe (w szczególnych przypadkach mo¿e byæ inne ni¿ liniowe)
	float friction_roll = 0.15;    // wspó³czynnik tarcia tocznego
	float elasticity = 0.5;       // wspó³czynnik sprê¿ystoœci (0-brak sprê¿ystoœci, 1-doskona³a sprê¿ystoœæ) 
	float g = 9.81;                // przyspieszenie grawitacyjne
	float Fy = mass_own*9.81;        // si³a nacisku na podstawê obiektu (na ko³a pojazdu)

	// obracam uk³ad wspó³rzêdnych lokalnych wed³ug quaterniona orientacji:
	Vector3 dir_forward = state.qOrient.rotate_vector(Vector3(1, 0, 0)); // na razie oœ obiektu pokrywa siê z osi¹ x globalnego uk³adu wspó³rzêdnych (lokalna oœ x)
	Vector3 dir_up = state.qOrient.rotate_vector(Vector3(0, 1, 0));  // wektor skierowany pionowo w górê od podstawy obiektu (lokalna oœ y)
	Vector3 dir_right = state.qOrient.rotate_vector(Vector3(0, 0, 1)); // wektor skierowany w prawo (lokalna oœ z)


	// rzutujemy vV na sk³adow¹ w kierunku przodu i pozosta³e 2 sk³adowe
	// sk³adowa w bok jest zmniejszana przez si³ê tarcia, sk³adowa do przodu
	// przez si³ê tarcia tocznego
	Vector3 vV_forward = dir_forward*(state.vV^dir_forward),
		vV_right = dir_right*(state.vV^dir_right),
		vV_up = dir_up*(state.vV^dir_up);

	// rzutujemy prêdkoœæ k¹tow¹ vV_ang na sk³adow¹ w kierunku przodu i pozosta³e 2 sk³adowe
	Vector3 vV_ang_forward = dir_forward*(state.vV_ang^dir_forward),
		vV_ang_right = dir_right*(state.vV_ang^dir_right),
		vV_ang_up = dir_up*(state.vV_ang^dir_up);


	// ruch kó³ na skutek krêcenia lub puszczenia kierownicy:  

	if (steer_wheel_speed != 0)
		state.steering_angle += steer_wheel_speed*dt;
	else
		if (state.steering_angle > 0)
		{
			if (!if_keep_steer_wheel)
				state.steering_angle -= steer_wheel_ret_speed*dt;
			if (state.steering_angle < 0) state.steering_angle = 0;
		}
		else if (state.steering_angle < 0)
		{
			if (!if_keep_steer_wheel)
				state.steering_angle += steer_wheel_ret_speed*dt;
			if (state.steering_angle > 0) state.steering_angle = 0;
		}
	// ograniczenia: 
	if (state.steering_angle > PI*60.0 / 180) state.steering_angle = PI*60.0 / 180;
	if (state.steering_angle < -PI*60.0 / 180) state.steering_angle = -PI*60.0 / 180;

	// obliczam promien skrêtu pojazdu na podstawie k¹ta skrêtu kó³, a nastêpnie na podstawie promienia skrêtu
	// obliczam prêdkoœæ k¹tow¹ (UPROSZCZENIE! pomijam przyspieszenie k¹towe oraz w³aœciw¹ trajektoriê ruchu)
	if (Fy > 0)
	{
		float V_ang_turn = 0;
		if (state.steering_angle != 0)
		{
			float Rs = sqrt(length*length / 4 + (fabs(length / tan(state.steering_angle)) + width / 2)*(fabs(length / tan(state.steering_angle)) + width / 2));
			V_ang_turn = vV_forward.length()*(1.0 / Rs);
		}
		Vector3 vV_ang_turn = dir_up*V_ang_turn*(state.steering_angle > 0 ? 1 : -1);
		Vector3 vV_ang_up2 = vV_ang_up + vV_ang_turn;
		if (vV_ang_up2.length() <= vV_ang_up.length()) // skrêt przeciwdzia³a obrotowi
		{
			if (vV_ang_up2.length() > V_ang_turn)
				vV_ang_up = vV_ang_up2;
			else
				vV_ang_up = vV_ang_turn;
		}
		else
		{
			if (vV_ang_up.length() < V_ang_turn)
				vV_ang_up = vV_ang_turn;
		}

		// friction zmniejsza prêdkoœæ obrotow¹ (UPROSZCZENIE! zamiast masy winienem wykorzystaæ moment bezw³adnoœci)     
		float V_ang_friction = Fy*friction_rot*dt / mass_own / 1.0;      // zmiana pr. k¹towej spowodowana frictionm
		float V_ang_up = vV_ang_up.length() - V_ang_friction;
		if (V_ang_up < V_ang_turn) V_ang_up = V_ang_turn;        // friction nie mo¿e spowodowaæ zmiany zwrotu wektora pr. k¹towej
		vV_ang_up = vV_ang_up.znorm()*V_ang_up;
	}


	Fy = mass_own*g*dir_up.y;                      // si³a docisku do pod³o¿a 
	if (Fy < 0) Fy = 0;
	// ... trzeba j¹ jeszcze uzale¿niæ od tego, czy obiekt styka siê z pod³o¿em!
	float Fh = Fy*friction*breaking_factor;                  // si³a hamowania (UP: bez uwzglêdnienia poœlizgu)

	float V_up = vV_forward.length();// - dt*Fh/m - dt*friction_roll*Fy/m;
	if (V_up < 0) V_up = 0;

	float V_right = vV_right.length();// - dt*friction*Fy/m;
	if (V_right < 0) V_right = 0;


	// wjazd lub zjazd: 
	//vPos.y = env.DistFromGround(vPos.x,vPos.z);   // najprostsze rozwi¹zanie - obiekt zmienia wysokoœæ bez zmiany orientacji

	// 1. gdy wjazd na wklês³oœæ: wyznaczam wysokoœci envu pod naro¿nikami obiektu (ko³ami), 
	// sprawdzam która trójka
	// naro¿ników odpowiada najni¿ej po³o¿onemu œrodkowi ciê¿koœci, gdy przylega do envu
	// wyznaczam prêdkoœæ podbicia (wznoszenia œrodka pojazdu spowodowanego wklês³oœci¹) 
	// oraz prêdkoœæ k¹tow¹
	// 2. gdy wjazd na wypuk³oœæ to si³a ciê¿koœci wywo³uje obrót przy du¿ej prêdkoœci liniowej

	// punkty zaczepienia kó³ (na wysokoœci pod³ogi pojazdu):
	Vector3 P = state.vPos + dir_forward*(length / 2 - front_axis_dist) - dir_right*width / 2 - dir_up*height / 2,
		Q = state.vPos + dir_forward*(length / 2 - front_axis_dist) + dir_right*width / 2 - dir_up*height / 2,
		R = state.vPos + dir_forward*(-length / 2 + back_axis_dist) - dir_right*width / 2 - dir_up*height / 2,
		S = state.vPos + dir_forward*(-length / 2 + back_axis_dist) + dir_right*width / 2 - dir_up*height / 2;

	// pionowe rzuty punktów zacz. kó³ pojazdu na powierzchniê envu:  
	Vector3 Pt = P, Qt = Q, Rt = R, St = S;
	Pt.y = env.DistFromGround(P.x, P.z); Qt.y = env.DistFromGround(Q.x, Q.z);
	Rt.y = env.DistFromGround(R.x, R.z); St.y = env.DistFromGround(S.x, S.z);
	Vector3 normPQR = normal_vector(Pt, Rt, Qt), normPRS = normal_vector(Pt, Rt, St), normPQS = normal_vector(Pt, St, Qt),
		normQRS = normal_vector(Qt, Rt, St);   // normalne do p³aszczyzn wyznaczonych przez trójk¹ty

	//fprintf(f, "P.y = %f, Pt.y = %f, Q.y = %f, Qt.y = %f, R.y = %f, Rt.y = %f, S.y = %f, St.y = %f\n",
	//	P.y, Pt.y, Q.y, Qt.y, R.y, Rt.y, S.y, St.y);

	float sryPQR = ((Qt^normPQR) - normPQR.x*state.vPos.x - normPQR.z*state.vPos.z) / normPQR.y, // wys. œrodka pojazdu
		sryPRS = ((Pt^normPRS) - normPRS.x*state.vPos.x - normPRS.z*state.vPos.z) / normPRS.y, // po najechaniu na skarpê 
		sryPQS = ((Pt^normPQS) - normPQS.x*state.vPos.x - normPQS.z*state.vPos.z) / normPQS.y, // dla 4 trójek kó³
		sryQRS = ((Qt^normQRS) - normQRS.x*state.vPos.x - normQRS.z*state.vPos.z) / normQRS.y;
	float sry = sryPQR; Vector3 norm = normPQR;
	if (sry > sryPRS) { sry = sryPRS; norm = normPRS; }
	if (sry > sryPQS) { sry = sryPQS; norm = normPQS; }
	if (sry > sryQRS) { sry = sryQRS; norm = normQRS; }  // wybór trójk¹ta o œrodku najni¿ej po³o¿onym    

	Vector3 vV_ang_horizontal = Vector3(0, 0, 0);
	// jesli któreœ z kó³ jest poni¿ej powierzchni envu
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))
	{
		// obliczam powsta³¹ prêdkoœæ k¹tow¹ w lokalnym uk³adzie wspó³rzêdnych:      
		Vector3 v_rotation = -norm.znorm()*dir_up*0.6;
		vV_ang_horizontal = v_rotation / dt;
	}

	Vector3 vAg = Vector3(0, -1, 0)*g;    // przyspieszenie grawitacyjne

	// jesli wiecej niz 2 kola sa na ziemi, to przyspieszenie grawitacyjne jest rownowazone przez opor gruntu:
	if ((P.y <= Pt.y + height / 2 + clearance) + (Q.y <= Qt.y + height / 2 + clearance) +
		(R.y <= Rt.y + height / 2 + clearance) + (S.y <= St.y + height / 2 + clearance) > 2)
		vAg = vAg + dir_up*(dir_up^vAg)*-1; //przyspieszenie resultaj¹ce z si³y oporu gruntu
	else   // w przeciwnym wypadku brak sily docisku 
		Fy = 0;


	// sk³adam z powrotem wektor prêdkoœci k¹towej: 
	//state.vV_ang = vV_ang_up + vV_ang_right + vV_ang_forward;  
	state.vV_ang = vV_ang_up + vV_ang_horizontal;


	float h = sry + height / 2 + clearance - state.vPos.y;  // ró¿nica wysokoœci jak¹ trzeba pokonaæ  
	float V_podbicia = 0;
	if ((h > 0) && (state.vV.y <= 0.01))
		V_podbicia = 0.5*sqrt(2 * g*h);  // prêdkoœæ spowodowana podbiciem pojazdu przy wje¿d¿aniu na skarpê 
	if (h > 0) state.vPos.y = sry + height / 2 + clearance;

	// lub  w przypadku zag³êbienia siê 
	Vector3 dvPos = state.vV*dt + state.vA*dt*dt / 2; // czynnik bardzo ma³y - im wiêksza czêstotliwoœæ symulacji, tym mniejsze znaczenie 
	state.vPos = state.vPos + dvPos;

	// korekta po³o¿enia w przypadku envu cyklicznego:
	if (state.vPos.x < -env.field_size*env.number_of_columns / 2) state.vPos.x += env.field_size*env.number_of_columns;
	else if (state.vPos.x > env.field_size*(env.number_of_columns - env.number_of_columns / 2)) state.vPos.x -= env.field_size*env.number_of_columns;
	if (state.vPos.z < -env.field_size*env.number_of_rows / 2) state.vPos.z += env.field_size*env.number_of_rows;
	else if (state.vPos.z > env.field_size*(env.number_of_rows - env.number_of_rows / 2)) state.vPos.z -= env.field_size*env.number_of_rows;

	// Sprawdzenie czy obiekt mo¿e siê przemieœciæ w zadane miejsce: Jeœli nie, to 
	// przemieszczam obiekt do miejsca zetkniêcia, wyznaczam nowe wektory prêdkoœci
	// i prêdkoœci k¹towej, a nastêpne obliczam nowe po³o¿enie na podstawie nowych
	// prêdkoœci i pozosta³ego czasu. Wszystko powtarzam w pêtli (pojazd znowu mo¿e 
	// wjechaæ na przeszkodê). Problem z zaokr¹glonymi przeszkodami - konieczne 
	// wyznaczenie minimalnego kroku.


	Vector3 vV_pop = state.vV;

	// sk³adam prêdkoœci w ró¿nych kierunkach oraz efekt przyspieszenia w jeden wektor:    (problem z przyspieszeniem od si³y tarcia -> to przyspieszenie 
	//      mo¿e dzia³aæ krócej ni¿ dt -> trzeba to jakoœ uwzglêdniæ, inaczej pojazd bêdzie wê¿ykowa³)
	state.vV = vV_forward.znorm()*V_up + vV_right.znorm()*V_right + vV_up +
		Vector3(0, 1, 0)*V_podbicia + state.vA*dt;
	// usuwam te sk³adowe wektora prêdkoœci w których kierunku jazda nie jest mo¿liwa z powodu
	// przeskód:
	// np. jeœli pojazd styka siê 3 ko³ami z nawierzchni¹ lub dwoma ko³ami i œrodkiem ciê¿koœci to
	// nie mo¿e mieæ prêdkoœci w dó³ pod³ogi
	if ((P.y <= Pt.y + height / 2 + clearance) || (Q.y <= Qt.y + height / 2 + clearance) ||
		(R.y <= Rt.y + height / 2 + clearance) || (S.y <= St.y + height / 2 + clearance))    // jeœli pojazd styka siê co najm. jednym ko³em
	{
		Vector3 dvV = vV_up + dir_up*(state.vA^dir_up)*dt;
		if ((dir_up.znorm() - dvV.znorm()).length() > 1)  // jeœli wektor skierowany w dó³ pod³ogi
			state.vV = state.vV - dvV;
	}

	// sk³adam przyspieszenia liniowe od si³ napêdzaj¹cych i od si³ oporu: 
	state.vA = (dir_forward*F + dir_right*Fb) / mass_own*(Fy > 0)  // od si³ napêdzaj¹cych
		- vV_forward.znorm()*(Fh / mass_own + friction_roll*Fy / mass_own)*(V_up > 0.01) // od hamowania i tarcia tocznego (w kierunku ruchu)
		- vV_right.znorm()*friction*Fy / mass_own*(V_right > 0.01)    // od tarcia w kierunku prost. do kier. ruchu
		+ vAg;           // od grawitacji


	// obliczenie nowej orientacji:
	Vector3 w_obrot = state.vV_ang*dt + state.vA_ang*dt*dt / 2;
	quaternion q_obrot = AsixToQuat(w_obrot.znorm(), w_obrot.length());
	state.qOrient = q_obrot*state.qOrient;
}

void MovableObject::DrawObject()
{
	glPushMatrix();


	glTranslatef(state.vPos.x, state.vPos.y + clearance, state.vPos.z);

	Vector3 k = state.qOrient.AsixAngle();     // reprezentacja k¹towo-osiowa quaterniona

	Vector3 k_znorm = k.znorm();

	glRotatef(k.length()*180.0 / PI, k_znorm.x, k_znorm.y, k_znorm.z);
	glTranslatef(-length / 2, -height / 2, -width / 2);
	glScalef(length, height, width);

	glCallList(Auto);
	GLfloat Surface[] = { 2.0f, 2.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
	if (if_ID_visible){
		glRasterPos2f(0.30, 1.20);
		glPrint("%d", iID);
	}
	glPopMatrix();
}




//**********************
//   Obiekty nieruchome
//**********************
Environment::Environment()
{
   field_size = 35;         // d³ugoœæ boku kwadratu w [m]           

   int wynik = ReadMap("map.txt");
   if (wynik == 0)
	   wynik = ReadMap("..//map.txt");
   if (wynik == 0)
	   fprintf(f, "Cannot open map.txt file. Check if this file exists in project directory!\n");




   d = new float**[number_of_rows];
   for (long i=0;i<number_of_rows;i++) {
       d[i] = new float*[number_of_columns];
       for (long j=0;j<number_of_columns;j++) d[i][j] = new float[4];
   }    
   Norm = new Vector3**[number_of_rows];
   for (long i=0;i<number_of_rows;i++) {
       Norm[i] = new Vector3*[number_of_columns];
	   for (long j = 0; j < number_of_columns; j++)
	   {
		   Norm[i][j] = new Vector3[4];
	   }
   }    
       
   fprintf(f,"height_map env: number_of_rows = %d, number_of_columns = %d\n",number_of_rows,number_of_columns);
}

Environment::~Environment()
{
  for (long i = 0;i< number_of_rows*2+1;i++) delete height_map[i];             
  delete height_map;   
  for (long i=0;i<number_of_rows;i++)  {
      for (long j=0;j<number_of_columns;j++) delete d[i][j];
      delete d[i];
  }
  delete d;  
  for (long i=0;i<number_of_rows;i++)  {
      for (long j=0;j<number_of_columns;j++) delete Norm[i][j];
      delete Norm[i];
  }
  delete Norm;  

         
}

float Environment::DistFromGround(float x,float z)      // okreœlanie wysokoœci dla punktu o wsp. (x,z) 
{
  
  float x_begin = -field_size*number_of_columns/2,     // wspó³rzêdne lewego górnego krañca envu
        z_begin = -field_size*number_of_rows/2;        
  
  long k = (long)((x - x_begin)/field_size), // wyznaczenie wspó³rzêdnych (w,k) kwadratu
       w = (long)((z - z_begin)/field_size);
  //if ((k < 0)||(k >= number_of_rows)||(w < 0)||(w >= number_of_columns)) return -1e10;  // jeœli poza map¹

  // korekta numeru kolumny lub wiersza w przypadku envu cyklicznego
  if (k<0) while (k<0) k += number_of_columns;
  else if (k > number_of_columns - 1) while (k > number_of_columns - 1) k -= number_of_columns;
  if (w<0) while (w<0) w += number_of_rows;
  else if (w > number_of_rows - 1) while (w > number_of_rows - 1) w -= number_of_rows;
  
  // wyznaczam punkt B - œrodek kwadratu oraz trójk¹t, w którym znajduje siê punkt
  // (rysunek w Environment::DrawInitialisation())
  Vector3 B = Vector3(x_begin + (k+0.5)*field_size, height_map[w*2+1][k], z_begin + (w+0.5)*field_size); 
  enum tr{ABC=0,ADB=1,BDE=2,CBE=3};       // trójk¹t w którym znajduje siê punkt 
  int triangle=0; 
  if ((B.x > x)&&(fabs(B.z - z) < fabs(B.x - x))) triangle = ADB;
  else if ((B.x < x)&&(fabs(B.z - z) < fabs(B.x - x))) triangle = CBE;
  else if ((B.z > z)&&(fabs(B.z - z) > fabs(B.x - x))) triangle = ABC;
  else triangle = BDE;
  
  // wyznaczam normaln¹ do p³aszczyzny a nastêpnie wspó³czynnik d z równania p³aszczyzny
  float dd = d[w][k][triangle];
  Vector3 N = Norm[w][k][triangle];
  float y;
  if (N.y > 0) y = (-dd - N.x*x - N.z*z)/N.y;
  else y = 0;
  
  return y;    
}

void Environment::DrawInitialisation()
{
  // tworze listê wyœwietlania rysuj¹c poszczególne pola mapy za pomoc¹ trójk¹tów 
  // (po 4 trójk¹ty na ka¿de pole):
  enum tr{ABC=0,ADB=1,BDE=2,CBE=3};       
  float x_begin = -field_size*number_of_columns/2,     // wspó³rzêdne lewego górnego krañca envu
        z_begin = -field_size*number_of_rows/2;        
  Vector3 A,B,C,D,E,N;      
  glNewList(EnvironmentMap,GL_COMPILE);
  glBegin(GL_TRIANGLES);
    
    for (long w=0;w<number_of_rows;w++) 
      for (long k=0;k<number_of_columns;k++) 
      {
          A = Vector3(x_begin + k*field_size, height_map[w*2][k], z_begin + w*field_size);
          B = Vector3(x_begin + (k+0.5)*field_size, height_map[w*2+1][k], z_begin + (w+0.5)*field_size);            
          C = Vector3(x_begin + (k+1)*field_size, height_map[w*2][k+1], z_begin + w*field_size); 
          D = Vector3(x_begin + k*field_size, height_map[(w+1)*2][k], z_begin + (w+1)*field_size);       
          E = Vector3(x_begin + (k+1)*field_size, height_map[(w+1)*2][k+1], z_begin + (w+1)*field_size); 
          // tworzê trójk¹t ABC w górnej czêœci kwadratu: 
          //  A o_________o C
          //    |.       .|
          //    |  .   .  | 
          //    |    o B  | 
          //    |  .   .  |
          //    |._______.|
          //  D o         o E
          
          Vector3 AB = B-A;
          Vector3 BC = C-B;
          N = (AB*BC).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( A.x, A.y, A.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( C.x, C.y, C.z);
          d[w][k][ABC] = -(B^N);          // dodatkowo wyznaczam wyraz wolny z równania plaszyzny trójk¹ta
          Norm[w][k][ABC] = N;          // dodatkowo zapisujê normaln¹ do p³aszczyzny trójk¹ta
          // trójk¹t ADB:
          Vector3 AD = D-A;
          N = (AD*AB).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( A.x, A.y, A.z);
		  glVertex3f( D.x, D.y, D.z);
		  glVertex3f( B.x, B.y, B.z);
		  d[w][k][ADB] = -(B^N);       
          Norm[w][k][ADB] = N;
		  // trójk¹t BDE:
          Vector3 BD = D-B;
          Vector3 DE = E-D;
          N = (BD*DE).znorm();          
          glNormal3f( N.x, N.y, N.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( D.x, D.y, D.z);     
          glVertex3f( E.x, E.y, E.z);  
          d[w][k][BDE] = -(B^N);        
          Norm[w][k][BDE] = N;  
          // trójk¹t CBE:
          Vector3 CB = B-C;
          Vector3 BE = E-B;
          N = (CB*BE).znorm();          
          glNormal3f( N.x, N.y, N.z);
          glVertex3f( C.x, C.y, C.z);
		  glVertex3f( B.x, B.y, B.z);
          glVertex3f( E.x, E.y, E.z);      
          d[w][k][CBE] = -(B^N);        
          Norm[w][k][CBE] = N;

		  min_bounds.x = (float)min(Norm[w][k]->x * field_size*number_of_columns/2, min_bounds.x);
		  min_bounds.y = (float)min(Norm[w][k]->y, min_bounds.y);
		  min_bounds.z = (float)min(Norm[w][k]->z * field_size * number_of_rows / 2, min_bounds.z);
		  max_bounds.x = (float)max(Norm[w][k]->x * field_size*number_of_columns/2, max_bounds.x);
		  max_bounds.y = (float)max(Norm[w][k]->y, max_bounds.y);
		  max_bounds.z = (float)max(Norm[w][k]->z * field_size * number_of_rows / 2, max_bounds.z);
      }		


  glEnd();
  glEndList(); 
                 
}

// wczytanie powierzchni terenu (mapy wysokoœci) oraz przedmiotów  
int Environment::ReadMap(char filename[128])
{
	int mode_reading_things = 0, mode_reading_map = 0, mode_reading_row = 0,
		nr_of_row_point = -1, nr_of_column_point = -1;   // liczby wierszy i kolumn punktów 
	height_map = NULL;
   
	this->number_of_rows = this->number_of_columns = 0;  // liczby wierszy i kolumn czwórek trójk¹tów

	FILE *pl = fopen(filename, "r");

	if (pl)
	{
		char line[1024], writing[128];
		long long_number;
		Vector3 wektor;
		quaternion kw;
		float float_number;
		while (fgets(line, 1024, pl))
		{
			sscanf(line, "%s", &writing);
			
			if (strcmp(writing, "<mapa>") == 0)
			{
				mode_reading_map = 1;
			}
			
			if (mode_reading_map)
			{
				if (strcmp(writing, "<liczba_wierszy") == 0)
				{
					sscanf(line, "%s %d ", &writing, &long_number);
					this->number_of_rows = long_number;
				}
				else if (strcmp(writing, "<liczba_kolumn") == 0)
				{
					sscanf(line, "%s %d ", &writing, &long_number);
					this->number_of_columns = long_number;
				}
				else if (strcmp(writing, "<wiersz_punktow") == 0)
				{
					mode_reading_row = 1;
					sscanf(line, "%s %d ", &writing, &long_number);
					nr_of_row_point = long_number;
					nr_of_column_point = 0;
				}
				else if (strcmp(writing, "</mapa>") == 0)
				{
					mode_reading_map = 0;
				}

				if (mode_reading_row)
				{
					if (strcmp(writing, "<w") == 0)
					{
						sscanf(line, "%s %f ", &writing, &float_number);
						height_map[nr_of_row_point][nr_of_column_point] = float_number;
						nr_of_column_point++;
					}
					else if (strcmp(writing, "</wiersz_punktow>") == 0)
					{
						mode_reading_row = 0;
					}
				}

			} // tryb odczytu mapy wierzcho³ków

			// pamiêæ dla mapy terenu:
			if ((this->number_of_rows > 0) && (this->number_of_columns > 0) && (height_map == NULL))
			{
				height_map = new float*[number_of_rows * 2 + 1];
				for (long i = 0; i<number_of_rows * 2 + 1; i++) {
					height_map[i] = new float[number_of_columns + 1];
					for (long j = 0; j<number_of_columns + 1; j++) height_map[i][j] = 0;
				}
			}

		}
		fclose(pl);
	}
	else return 0;

	return 1;
}


void Environment::Draw()
{
  glCallList(EnvironmentMap);                  
}

   
