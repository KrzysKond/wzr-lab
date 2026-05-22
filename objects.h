#define _OBJECTS__H
#include <stdio.h>
#include "quaternion.h"

#define PI 3.1416
extern class Terrain;

struct ObjectState
{
	int iID;                  // identyfikator obiektu
	Vector3 vPos;             // position obiektu (œrodka geometrycznego obiektu) 
	quaternion qOrient;       // orientacja (position katowe)
	Vector3 vV, vA;            // predkosc, przyspiesznie liniowe
	Vector3 vV_angular, vA_angular;   // predkosc i przyspeszenie liniowe
	float wheel_turn_angle;     // front wheels rotation angle  (positive - to the left)

	float mass_total;          // masa ca³kowita (wa¿na przy kolizjach)
	long money;
	float amount_of_fuel;
	int iID_owner;
	int if_autonomous;
};


class MovableObject
{
public:
	int iID;                  // identyfikator obiektu

	ObjectState state;

	// parametry akcji:
	float F;                  // F - si³a pchajaca do przodu (do ty³u ujemna)
	float breaking_degree;                // stopieñ hamowania Fh_max = friction_ground*Fy*breaking_degree
	//float wheel_turn_angle;               // kat skretu kol w radianach (w lewo - dodatni)
	float wheel_turn_speed;      // steering wheel speed [rad/s]
	bool if_keep_steer_wheel;    // czy kierownica zablokowana (jeœli nie, to wraca do po³o¿enia standardowego)

	// parametry sta³e:
	float planting_skills,    // umiejêtnoœæ sadzenia drzew (1-pe³na, 0- brak)
		fuel_collection_skills,   // umiejêtnoœæ zbierania paliwa
		money_collection_skills;

	bool if_selected;            // czy obiekt zaznaczony  

	// parametry fizyczne pojazdu:
	float F_max;
	float alpha_max;
	float Fy;                 // si³a nacisku na podstawê pojazdu - gdy obiekt styka siê z pod³o¿em (od niej zale¿y si³a hamowania)
	float mass_own;		  // masa w³asna obiektu (bez paliwa)	
	//float mass_total;          // masa ca³kowita (wa¿na przy kolizjach)
	float length, width, height; // rozmiary w kierunku lokalnych osi x,y,z
	float radius;            // radius minimalnej sfery, w ktora wpisany jest obiekt
	float clearance;           // wysokoœæ na której znajduje siê podstawa obiektu (przeœwit)
	float front_axis_dist;           // odleg³oœæ od przedniej osi do przedniego zderzaka 
	float back_axis_dist;             // odleg³oœæ od tylniej osi do tylniego zderzaka 
	float wheel_ret_speed;  // steering wheel return speed[rad / s]
	float tank_capacity;    // amount of fuel which can be tanked 

	// inne: 
	int iID_collider;            // identyfikator pojazdu z którym nast¹pi³a kolizja (-1  brak kolizji)
	Vector3 vdV_collision;        // poprawka prêdkoœci pojazdu koliduj¹cego
	//int iID_owner;
	//bool if_autonomous;         // czy obiekt jest autonomiczny
	long number_of_taking_item;   // numer wzietego przedmiotu
	float taking_value;   // value wzietego przedmiotu
	long number_of_renewed_item;


	float time_of_simulation;     // czas sumaryczny symulacji obiektu   
	Terrain *terrain;             // wskaŸnik do terrainu, do którego przypisany jest obiekt

public:
	MovableObject(Terrain *t);          // konstruktor
	~MovableObject();
	void ChangeState(ObjectState __state);          // zmiana stanu obiektu
	ObjectState State();        // metoda zwracajaca state obiektu
	void Simulation(float dt);  // symulacja ruchu obiektu w oparciu o biezacy state, przylozone sily
	// oraz czas dzialania sil. Efektem symulacji jest nowy state obiektu 
	void DrawObject();			   // odrysowanie obiektu					
};

enum ItemTypes { ITEM_COIN, ITEM_BARREL, ITEM_TREE, ITEM_BUILDING, ITEM_POINT, ITEM_EDGE, ITEM_START_PLACE, ITEM_WALL, ITEM_GATE };
//char *PRZ_nazwy[] = { "moneta", "beczka", "drzewo", "punkt", "krawedz", "miejsce startowe", "mur", "bramka" };
enum TreeSubtypes { TREE_POPLAR, TREE_SPRUCE, TREE_BAOBAB, TREE_FANTAZJA};
//char *DRZ_nazwy[] = { "topola", "swierk", "baobab", "fantazja" };
enum PointSubtypes {POINT_ORDINAL, POINT_OF_EDGE, POINT_OF_WATER, POINT_OF_VEHICLE};

struct Item
{
	Vector3 vPos;                  // po³o¿enie œrodka obiektu
	quaternion qOrient;            // orientacja (po³o¿enie katowe, praktycznie nieu¿ywane)

	int type;
	int subtype;
	long index;                    // identyfikator - numer przedmiotu (jest potrzebny przy przesy³aniu danych z tego wzglêdu,
	//    ¿e wyszukiwanie przedmiotów (np. ItemsInRadius()) zwraca wskaŸniki)

	float value;                   // w zal. od typu nomina³ monety /ilosc paliwa, itd.
	float diameter;                // np. grubosc pnia u podstawy, diameter monety
	float diameter_visual;         // œrednica widocznoœci - sfery, która opisuje przedmiot
	float param_f[3];              // dodatkowe parametry roznych typow
	long param_i[3];
	bool if_selected;              // czy przedmiot jest zaznaczony przez uzytkownika
	long group;                    // numer grupy do której nale¿y przedmiot;

	bool to_take;                  // czy przedmiot mozna wziac
	bool if_taken_by_me;           // czy przedmiot wziety przeze mnie
	bool if_renewable;             // czy mo¿e siê odnawiaæ w tym samym miejscu po pewnym czasie
	long taking_time;              // czas wziêcia (potrzebny do przywrócenia)

	
	unsigned long display_number;  // zgodny z liczba wyœwietleñ po to, by nie powtarzaæ rysowania przedmiotów
};

struct LassoPoint
{
	float x;
	float y;
};

class Sector
{
public:
	long w, k;                    // wiersz i kolumna okreœlaj¹ce po³o¿enie obszaru na nieograniczonej p³aszczyŸnie
	int number_of_cells;             // liczba oczek na mapie w poziomie i pionie (potêga dwójki!) 
	//float sector_size;        // potrzebny do obliczenia normalnych
	Item **wp;               // jednowymiarowa tablica wskaŸników do przedmiotów znajduj¹cych siê w pewnym obszarze
	long number_of_items;      // liczba przedmiotów w obszarze 
	long number_of_items_max;  // size tablicy przedmiotów z przydzielon¹ pamiêci¹

	float **map_of_heights;         // wysokoœci wierzcho³ków
	int ***type_of_surface;                 // typy nawierzchni w ka¿dym trójk¹cie w ka¿dym oczku 
	float ***level_of_water;            // poziom wody w ka¿dym trójk¹cie w ka¿dym oczku 
	Vector3 ****normal_vectors;                // wektory normalne do poszczególnych p³aszczyzn (rozdzielczoœæ x wiersz x kolumna x 4 p³aszczyzny);

	float **map_of_heights_in_edit;    // mapa w trakcie edycji - powinna byæ widoczna ³¹cznie z dotychczasow¹ map¹
	Vector3 ****normal_vectors_in_edit;   
	int ***type_of_surface_in_edit;
	float ***level_of_water_in_edit;
	int number_of_cells_in_edit;

	// podstawowe w³aœciwoœci dla ca³ego sektora, gdyby nie by³o mapy
	int default_type_of_surface;                              // type nawierzchni gdyby nie by³o mapki
	float default_height;                                     // std. poziom gruntu (praktycznie nie u¿ywany)
	float default_level_of_water;                             // (praktycznie nie u¿ywany)

	MovableObject **mov_obj;                          // tablica obiektów ruchomych, które mog¹ znaleŸæ siê w obszarze w ci¹gu
	// kroku czasowego symulacji (np. obiekty b. szybkie mog¹ znaleŸæ siê w wielu obszarach)
	long number_of_movable_objects;
	long number_of_movable_objects_max;

	float height_max;             // maksymalna wysokoœæ terrainu w sektorze -> istotna dla wyznaczenia rozdzielczoœci np. przy widoku z góry
	int number_of_cells_displayed;   // liczba oczek (rozdzielczoœæ) aktualnie wyœwietlana, w zal. od niej mo¿na dopasowaæ s¹siednie sectors
	                                // by nie by³o dziur
	int number_of_cells_displayed_prev;  // poprzednia liczba oczek, by uzyskaæ pa³ne dopasowanie rozdzielczoœci s¹siaduj¹cych sektorów 
	//float radius;                // radius sektora zwykle d³ugoœæ po³owy przek¹tnej kwadratu, mo¿e byæ wiêkszy, gdy w
	                              // œrodku du¿e ró¿nice wysokoœci lub du¿e przedmioty lub obiekty

	Sector(){};
	Sector(int _numcells, long _w, long _k, bool if_map);
	~Sector();
	void memory_for_map(int __number_of_cells, bool if_edit, bool if_map);
	void memory_for_water(bool if_edit);
	void memory_for_surf(bool if_edit);
	void memory_release(bool if_edit = 0);
	void insert_item(Item *p);
	void remove_item(Item *p);
	void insert_movable_object(MovableObject *o);
	void release_movable_object(MovableObject *o);
	void calculate_normal_vectors(float sector_size, bool if_edit=0);         // obliczenie wektorów normalnych N do p³aszczyzn trójk¹tów, by nie robiæ tego ka¿dorazowo przy odrysowywaniu   
};

struct HashTableCell  // komórka tablicy rozproszonej (te¿ mog³aby byæ tablic¹ rozproszon¹)
{
	Sector **sectors;  // tablica wskaŸników do sektorów (mog¹ zdarzaæ siê konflikty -> w jednej komórce wiele sektorów) 
	long number_of_sectors; // liczba sektorów w komórce
	long number_of_sectors_max; // liczba sektorów, dla której zarezerwowano pamiêæ
};

class SectorsHashTable      // tablica rozproszona przechowuj¹ca sectors
{
	//private:
public:
	long number_of_cells;                                 // liczba komórek tablicy
	long general_number_of_sectors;
	long w_min, w_max, k_min, k_max;                     // minimalne i maksymalne wspó³rzêdne sektorów (przyspieszaj¹ wyszukiwanie)  
	HashTableCell *cells;
	unsigned long create_key(long w, long k);          // wyznaczanie indeksu komórki

public:

	SectorsHashTable();        // na wejœciu konstruktora liczba komórek tablicy
	~SectorsHashTable();

	Sector *find(long w, long k);       // znajdowanie sektora (zwraca NULL jeœli nie znaleziono)
	Sector *insert(Sector *s);              // wstawianie sektora do tablicy
	void remove(Sector *s);              // usuwanie sektora
};



struct FoldParams
{
	float fold_diameter;
	float fold_height;
	int fold_number_of_cells;           // liczba oczek w ka¿dym sektorze, w którym tworzona jest fa³da 
	bool if_convex;                 // 1-wypuk³oœæ w górê, 0 - w dó³
	float height_variance;        // losowa wariancja wysokoœci ka¿dego wierzcho³ka fa³dy
	int section_shape;
	float internal_diameter;        // pozwala na uzyskanie p³askiego miejsca na górze lub dole fa³dy w zal. od wypuk³oœci
	bool if_constant_descent;        // czy stoki wa³u maj¹ sta³e pochylenie -> wówczas maj¹ one sta³y k¹t nachylenia, niezale¿nie
	// od wysokoœci (np. jak nasyp drogowy) -> nie jest brana pod uwagê œrednica fa³dy ale k¹t nachylenia
	float descent_angle;             // k¹t nachylenia stoku w radianach np. pi/4 == 45 stopni

	int fold_with_terrain_join_type;        // sposób ³¹czenia nowoutworzonej fa³dy z obecn¹ powierzchni¹ terrainu 
};

class Terrain
{
public:
	void InsertItemIntoSectors(Item *prz);
	void DeleteItemFromSectors(Item *prz);
public:

	long number_of_items;      // liczba przedmiotów na planszy
	long number_of_items_max;  // size tablicy przedmiotów

	SectorsHashTable *ts;

	
	// inne istotne w³aœciwoœci terrainu:
	float sector_size;        // w [m] zamiast rozmiaru pola
	float time_of_item_renewing;     // czas w [s] po którym nastêpuje odnowienie siê przedmiotu 
	bool if_toroidal_world;        // inaczej cyklicznosc -> po dojsciu do granicy przeskakujemy na
	                              // pocz¹tek terrainu, zarówno w pionie, jak i w poziomie (gdy nie ma granicy, terrain nie mo¿e byæ cykliczny!)
	float border_x, border_z;   // granica terrainu (jeœli -1 - terrain nieskoñczenie wielki)

	float height_std;            // standardowa wysokoœæ terenu (na razie nie mo¿na jej zmieniaæ)
	float level_of_water_std;    // standardowy poziom wody (-1e10 oznacza brak wody) 
	

	// parametry wyœwietlania:
	float detail_level;                 // stopieñ szczegó³owoœci w przedziale 0,1 
	unsigned long number_of_displays;   // potrzebne do unikania podwójnego rysowania przedmiotów
	long *selected_items;     // tablica numerów zaznaczonych przedmiotów (dodatkowo ka¿dy przedmiot posiada pole z informacj¹ o zaznaczeniu)
	long number_of_selected_items,      // liczba aktualnie zaznaczonych przedmiotów
		number_of_selected_items_max;   // liczba miejsc w tablicy 

	FoldParams pf_rej[10];        // parametry edycji fa³d (s¹ tu tylko ze wzglêdu na koniecznoœæ zapisu do pliku

	Item *p;          // tablica przedmiotow roznego typu

	Terrain();
	~Terrain();
	float GroundHeight(float x, float z);      // okreœlanie wysokoœci gruntu dla punktu o wsp. (x,z)
	float ItemPointHeight(Vector3 pol, Item *prz); // wysokoœæ na jakiej znajdzie siê przeciêcie pionowej linii przech. przez pol na górnej powierzchni przedmiotu
	float GeneralHeight(Vector3 pol);                 // okreœlenie wysokoœci nad najbli¿szym przedmiotem lub gruntem patrz¹c w dó³ od punktu pol
	void SectorCoordinates(long *w, long *k, float x, float z);  // na podstawie wsp. po³o¿enia punktu (x,z) zwraca wsp. sektora 
	void SectorBeginPosition(float *x, float *z, long w, long k); // na podstawie wspó³rzêdnych sektora (w,k) zwraca po³o¿enie punktu pocz¹tkowego
	void InsertWater(float x, float z, float poziom_maks);        // wstawianie wody w oczko (funkcja rekurencyjna)
	float LevelOfWater(float x, float z);                         // poziom wody w podanym punkcie terenu 
	float HighestSelectedItemHeight(Vector3 pol);
	Vector3 Cursor3D_CoordinatesWithoutParallax(int X, int Y);
	void DrawObject();	      // odrysowywanie terrainu 
	void GraphicsInitialization();               // tworzenie listy wyœwietlania
	int SaveMapToFile(char filename[]);        // SaveMapToFile mapy w pliku o podanej nazwie
	int OpenMapFromFile(char filename[]);       // OpenMapFromFile mapy z pliku
	void PlaceItemInTerrain(Item *prz);     // m.i. ustalenie wysokoœci, na której le¿y przedmiot w zal. od ukszta³t. terrainu
	long InsertItemToArrays(Item prz);   // wstawia przedmiot do tablic, zwraca numer przedmiotu w tablicy p[]
	void SelectUnselectItemOrGroup(long nr_prz);// wpisywanie w pole ¿e jest zaznaczony lub nie, dodawanie/usuwanie do/z listy zaznaczonych 
	void DeleteSelectItems();
	void NewMap();                      // tworzenie nowej mapy (zwolnienie pamiêci dla starej)
	
	long ItemsInRadius(Item*** item_tab_pointer, Vector3 pol, float radius);
	void InsertObjectIntoSectors(MovableObject *ob);
	void DeleteObjectsFromSectors(MovableObject *ob);
	long ObjectsInRadius(MovableObject*** object_tab_pointer, Vector3 pol, float radius);
};