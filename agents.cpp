#include <stdlib.h>
#include <time.h>

#include "agents.h"


AutoPilot::AutoPilot()
{

}

void AutoPilot::AutoControl(MovableObject *ob)
{
	Terrain* teren = ob->terrain;  // wskaŸnik do terenu
	Item* przedmioty = teren->p;   // wskaŸnik do  tablicy przedmiotów

	Vector3 vect_local_forward = ob->state.qOrient.rotate_vector(Vector3(1, 0, 0));
	Vector3 vect_local_right = ob->state.qOrient.rotate_vector(Vector3(0, 0, 1));

	// parametry sterowania:
	ob->breaking_degree = 0;             // si³a hamowania
	ob->F = ob->F_max;                           // si³a napêdowa
	ob->state.wheel_turn_angle = 0;      // k¹t skrêtu kierownicy - mo¿na ustaiwaæ go bezpoœrednio zak³adaj¹c, ¿e robot mo¿e krêciæ kierownic¹ dowolnie szybko,
										 // jednaj gwa³towna zmiana po³o¿enia kierownicy (i tym samym kó³) mo¿e skutkowaæ poœlizgiem pojazdu
	// parametry sterowania daj¹ce wiêkszy realizm zamiast state.wheel_turn_angle:
	ob->wheel_turn_speed = 0;            // prêdkoœæ skrêtu kierownicy (dodatnia - w lewo)
	ob->if_keep_steer_wheel = 0;         // czy kierownica zablokowana (jeœli nie, to wraca do po³o¿enia standardowego)


	// TUTAJ NALE¯Y UMIEŒCIÆ ALGORYTM AUTONOMICZNEGO STEROWANIA POJAZDEM

	const float LOW_FUEL = 20.0f;   // próg niskiego paliwa

	// --- wybór celu (system regu³owy) ---
	int target_idx = -1;
	float best_score = -1e30f;

	for (long i = 0; i < teren->number_of_items; i++)
	{
		Item& prz = przedmioty[i];
		if (!prz.to_take || prz.if_taken_by_me) continue;

		bool want_fuel = (ob->state.amount_of_fuel < LOW_FUEL);
		bool is_barrel = (prz.type == ITEM_BARREL);
		bool is_coin   = (prz.type == ITEM_COIN);

		if (want_fuel && !is_barrel) continue;   // ma³o paliwa -> tylko beczki
		if (!want_fuel && !is_coin)  continue;   // du¿o paliwa -> tylko monety

		Vector3 diff = prz.vPos - ob->state.vPos;
		float dist = diff.length();
		if (dist < 0.001f) dist = 0.001f;

		// wynik: wartoœæ / odleg³oœæ (im bli¿ej i cenniejszy, tym lepszy)
		float score = prz.value / dist;
		if (score > best_score)
		{
			best_score = score;
			target_idx = (int)i;
		}
	}

	// --- sterowanie w kierunku celu ---
	if (target_idx >= 0)
	{
		Vector3 diff = przedmioty[target_idx].vPos - ob->state.vPos;
		float dist = diff.length();

		float dot_f = diff ^ vect_local_forward;   // iloczyn skalarny
		float dot_r = diff ^ vect_local_right;

		float alfa = acosf(dot_f / dist);           // k¹t [0, PI]

		// znak: przedmiot z prawej -> skrêt w prawo (ujemny)
		if (dot_r > 0.0f) alfa = -alfa;

		ob->state.wheel_turn_angle = alfa;

		// hamuj gdy bardzo blisko celu
		if (dist < 3.0f)
		{
			ob->F = 0;
			ob->breaking_degree = 1.0f;
		}
	}


}

void AutoPilot::ControlTest(MovableObject *_ob, float krok_czasowy, float czas_proby)
{
	bool koniec = false;
	float _czas = 0;               // czas liczony od pocz¹tku testu
	//FILE *pl = fopen("test_sterowania.txt","w");
	while (!koniec)
	{
		_ob->Simulation(krok_czasowy);
		AutoControl(_ob);
		_czas += krok_czasowy;
		if (_czas >= czas_proby) koniec = true;
		//fprintf(pl,"czas %f, vPos[%f %f %f], got %d, pal %f, F %f, wheel_turn_angle %f, breaking_degree %f\n",_czas,_ob->vPos.x,_ob->vPos.y,_ob->vPos.z,_ob->money,_ob->amount_of_fuel,_ob->F,_ob->wheel_turn_angle,_ob->breaking_degree);
	}
	//fclose(pl);
}

// losowanie liczby z rozkladu normalnego o zadanej sredniej i wariancji
float Randn(float srednia, float wariancja, long liczba_iter)
{
	//long liczba_iter = 10;  // im wiecej iteracji tym rozklad lepiej przyblizony
	float suma = 0;
	for (long i = 0; i < liczba_iter; i++)
		suma += (float)rand() / RAND_MAX;
	return (suma - (float)liczba_iter / 2)*sqrt(12 * wariancja / liczba_iter) + srednia;
}

void AutoPilot::ParametersSimAnnealing(long number_of_epochs, float krok_czasowy, float czas_proby)
{
	float T = 0.02,//100,
		wT = 0.99,
		c = 100000.0;

	float pz = 0.1;   // prawdopodobieñstwo zmiany parametru (¿eby nie wszystkie siê zmienia³y ka¿dorazowo) 

	//for (long p=0;p<number_of_params;p++) par[p] = 0.9;
	long gotowka_pop = 0;

	float delta_par[100];
	FILE *f = fopen("wyzarz_log.txt", "w");

	fprintf(f, "Start optymalizacji %d parametrow z wykorzystaniem symulowanego wyzarzania\n", number_of_params);
	for (long ep = 0; ep < number_of_epochs; ep++)
	{
		// losuje poprawki dla czêœci parametrów:
		for (long p = 0; p < number_of_params; p++)
			if ((float)rand() / RAND_MAX < pz)
				delta_par[p] = Randn(0, T, 10);
			else
				delta_par[p] = 0;

		if (ep > 0)
			for (long p = 0; p < number_of_params; p++)
				par[p] += delta_par[p];
		for (long i = 0; i < number_of_params; i++)
			fprintf(f, "par[%d] = %3.10f;\n", i, par[i]);
		Terrain t2;
		MovableObject *Obiekt = new MovableObject(&t2);
		Obiekt->planting_skills = 1.0;
		Obiekt->money_collection_skills = 1.0;
		Obiekt->fuel_collection_skills = 1.0;
		long gotowka_pocz = Obiekt->state.money;

		ControlTest(Obiekt, krok_czasowy, czas_proby);

		long gotowka = Obiekt->state.money - gotowka_pocz;

		float dE = gotowka - gotowka_pop;
		float p_akc = 1.0 / (1 + exp(-dE / (c*T)));
		fprintf(f, "epoka %d: T = %f, gotowka = %d, dE = %f, p_akc = %f\n", ep, T, gotowka, dE, p_akc);
		//if (gotowka > 15000) break;
		// akceptujemy lub odrzucamy
		if (((float)rand() / RAND_MAX < p_akc) || (ep == 0))
		{
			gotowka_pop = gotowka;

			fprintf(f, "sym.wyz-akceptacja, %d epoka: T=%f, gotowka = %d\n", ep, T, gotowka);
			char lanc[256];
			sprintf(lanc, "sym.wyz-akceptacja, %d epoka: T=%f, gotowka = %d", ep, T, gotowka);
			//SetWindowText(main_window, lanc);

		}
		else
		{
			for (long p = 0; p < number_of_params; p++) par[p] -= delta_par[p];
		}
		delete Obiekt;
		fclose(f);
		f = fopen("wyzarz_log.txt", "a");

		T *= wT;

	} // po epokach

	fprintf(f, "Koniec wyzarzania, koncowy wynik to %d gotowki\nOto koncowe wartosci:\n", gotowka_pop);
	for (long i = 0; i < number_of_params; i++)
		fprintf(f, "par[%d] = %3.10f;\n", i, par[i]);
	fclose(f);
} 