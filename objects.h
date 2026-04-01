#include <stdio.h>
#include "quaternion.h"

#define PI 3.1415926

struct ObjectState
{
	Vector3 vPos;              // polozenie obiektu (œrodka geometrycznego obiektu) 
	quaternion qOrient;        // orientacja (polozenie katowe)
	Vector3 vV, vA;            // predkosc, przyspiesznie liniowe
	Vector3 vV_ang, vA_ang;    // predkosc i przyspieszenie liniowe
	float wheel_angle;         // kat skretu kol w radianach (w lewo - dodatni)
};

// Klasa opisuj¹ca obiekty ruchome
class MovableObject
{
public:
	int iID;                  // identyfikator obiektu

	ObjectState state;

	float F, Fb;                    // si³y dzia³aj¹ce na obiekt: F - pchajaca do przodu, Fb - w prawo (silnik rakietowy)
	float breaking_factor;          // stopieñ hamowania Fh_max = friction*Fy*ham
	float turning_speed;        // prêdkoœæ krêcenia kierownic¹
	bool if_keep_steer_wheel;       // czy kierownica jest trzymana (prêdkoœæ mo¿e byæ zerowa, a kierownica trzymana w pewnym po³o¿eniu nie wraca do po³. zerowego)

	float mass_own;				    // masa w³asna obiektu	
	float length, width, height;    // rozmiary w kierunku lokalnych osi x,y,z
	float clearance;                // wysokoœæ na której znajduje siê podstawa obiektu
	float front_axis_dist;          // odleg³oœæ od przedniej osi do przedniego zderzaka 
	float back_axis_dist;           // odleg³oœæ od tylniej osi do tylniego zderzaka	
	float steer_wheel_ret_speed;    // prêdkoœæ powrotu kierownicy po puszczeniu
	float friction_linear;          // wspó³czynnik tarcia obiektu o pod³o¿e 
	float friction_rot;             // tarcie obrotowe obrotowe opon po pod³o¿u (w szczególnych przypadkach mo¿e byæ inne ni¿ liniowe)
	float friction_roll;            // wspó³czynnik tarcia tocznego
	float friction_air;             // wspó³czynnik oporu powietrza (si³a zale¿y od kwadratu prêdkoœci)
	float elasticity;               // wspó³czynnik sprê¿ystoœci (0-brak sprê¿ystoœci, 1-doskona³a sprê¿ystoœæ) 
	float wheel_angle_max;          // maksymalny skrêt kó³
	float F_max;                    // maksymalna si³a wytwarzana przez silnik

public:
	MovableObject();          // konstruktor
	~MovableObject();
	void StateUpdate(ObjectState state);          // zmiana stateu obiektu
	ObjectState State();        // metoda zwracajaca state obiektu
	void Simulation(float dt);  // symulacja ruchu obiektu w oparciu o biezacy state, przylozone sily
	// oraz czas dzialania sil. Efektem symulacji jest nowy state obiektu 
	void DrawObject();			   // odrysowanie obiektu					
};

// Klasa opisuj¹ca planet_terrain, po którym poruszaj¹ siê obiekty
class Terrain
{
public:
	float **height_map;          // wysokoœci naro¿ników oraz œrodków pól
	float ***d;            // wartoœci wyrazu wolnego z równania p³aszczyzny dla ka¿dego trójk¹ta
	Vector3 ***Norm;       // normalne do p³aszczyzn trójk¹tów
	float field_size;    // length boku kwadratowego pola na mapie
	long number_of_rows, number_of_columns; // liczba wierszy i kolumn mapy (kwadratów na wysokoœæ i szerokoœæ)     
	Terrain();
	~Terrain();
	float DistFromGround(float x, float z);      // okreœlanie wysokoœci dla punktu o wsp. (x,z) 
	void Draw();	                      // odrysowywanie planet_terrainu   
	void DrawInitialisation();               // tworzenie listy wyœwietlania
	int ReadMap(char filename[128]);
};

// k¹t pomiêdzy pojazdami na podstawie kwaternionów orientacji   
float AngleBetweenQuats(quaternion q1, quaternion q2);

// odleg³oœæ pomiêdzy punktami w œwiecie toroidalnym (wymaga uwzglêdnienia przeskoków pomiêdzy koñcem i pocz¹tkiem)
float DistanceBetweenPointsOnTetraMap(Vector3 p1, Vector3 p2);

// realizacja kroku scenariusza dla podanego obiektu, scenariusza i czasu od pocz¹tku
// zwraca informacjê czy scenariusz dobieg³ koñca, umieszcza w obj parametry sterowania (si³a, predkoœæ skrêtu kó³, stopieñ ham.)
bool test_scenario_step(MovableObject *obj, float test_scenario[][4], int number_of_actions, float __time);