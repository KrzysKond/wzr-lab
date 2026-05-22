#include <stdlib.h>
#include <time.h>
#include <map>

#include "agents.h"

extern MovableObject *my_vehicle;
extern std::map<int, MovableObject*> network_vehicles;
extern float TransferSending(int ID_receiver, int transfer_type, float transfer_value);


AutoPilot::AutoPilot()
{

}

static float FuelPrice(float myFuel, float myMoney, float theirFuel, float theirMoney)
{
    constexpr float kBasePrice = 50.0f;

    float buyerRatio  = (myMoney   > 0.0f) ? myFuel   / myMoney   : 0.0f;
    float sellerRatio = (theirMoney > 0.0f) ? theirFuel / theirMoney : 1.0f;

    float factor = 1.0f;
    if (buyerRatio  < 0.01f) factor *= 1.5f;
    else if (buyerRatio < 0.05f) factor *= 1.2f;
    
    if (sellerRatio > 0.05f) factor *= 0.8f;
    else if (sellerRatio > 0.02f) factor *= 0.9f;

    return kBasePrice * factor;
}

void AutoPilot::AutoControl(MovableObject* ob)
{
    constexpr float kLowFuel    = 5.0f;
    constexpr float kHighFuel   = 15.0f;
    constexpr float kTradeFuel  = 3.0f;
    constexpr float kStopDist   = 3.0f;
    constexpr float kMinDist    = 0.001f;

    ob->F                      = ob->F_max;
    ob->breaking_degree        = 0.0f;
    ob->state.wheel_turn_angle = 0.0f;
    ob->wheel_turn_speed       = 0.0f;
    ob->if_keep_steer_wheel    = 0;

    Vector3    pos      = ob->state.vPos;
    const bool wantFuel = ob->state.amount_of_fuel < kLowFuel;

    Vector3 forward = ob->state.qOrient.rotate_vector(Vector3(1, 0, 0));
    Vector3 right   = ob->state.qOrient.rotate_vector(Vector3(0, 0, 1));

    if (!network_vehicles.empty())
    {
        for (auto& kv : network_vehicles)
        {
            MovableObject* neighbor = kv.second;
            if (!neighbor) continue;

            float myFuel      = ob->state.amount_of_fuel;
            float myMoney     = (float)ob->state.money;
            float theirFuel   = neighbor->state.amount_of_fuel;
            float theirMoney  = (float)neighbor->state.money;

            float price = FuelPrice(myFuel, myMoney, theirFuel, theirMoney);

            if (myFuel < kLowFuel && theirFuel > kHighFuel && myMoney >= price * kTradeFuel)
            {
                TransferSending(neighbor->iID, 0, price * kTradeFuel);
                break;
            }
            else if (myFuel > kHighFuel && theirFuel < kLowFuel && theirMoney >= price * kTradeFuel)
            {
                TransferSending(neighbor->iID, 1, kTradeFuel);
                break;
            }
        }
    }

    int   bestIdx   = -1;
    float bestScore = -1.0f;

    Item* items = ob->terrain->p;
    const long count = ob->terrain->number_of_items;

    for (long i = 0; i < count; ++i)
    {
        Item& item = items[i];
        if (!item.to_take || item.if_taken_by_me)     continue;
        if (wantFuel  && item.type != ITEM_BARREL)    continue;
        if (!wantFuel && item.type != ITEM_COIN)      continue;

        Vector3 diff = item.vPos - pos;
        const float dist  = diff.length();
        const float score = item.value / (dist < kMinDist ? kMinDist : dist);

        if (score > bestScore) { bestScore = score; bestIdx = static_cast<int>(i); }
    }

    if (bestIdx < 0) return;

    Vector3 diff = items[bestIdx].vPos - pos;
    float   dist = diff.length();
    if (dist < kMinDist) dist = kMinDist;

    const float cosA  = (diff ^ forward) / dist;
    const float angle = (diff ^ right) > 0.0f ? -acosf(cosA) : acosf(cosA);

    ob->state.wheel_turn_angle = angle;

    if (dist < kStopDist)
    {
        ob->F               = 0.0f;
        ob->breaking_degree = 1.0f;
    }
}
void AutoPilot::ControlTest(MovableObject *_ob, float krok_czasowy, float czas_proby)
{
    bool koniec = false;
    float _czas = 0;               // czas liczony od poczatku testu
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

    float pz = 0.1;   // prawdopodobienstwo zmiany parametru (zeby nie wszystkie sie zmienialy kazdorazowo) 

    //for (long p=0;p<number_of_params;p++) par[p] = 0.9;
    long gotowka_pop = 0;

    float delta_par[100];
    FILE *f = fopen("wyzarz_log.txt", "w");

    fprintf(f, "Start optymalizacji %d parametrow z wykorzystaniem symulowanego wyzarzania\n", number_of_params);
    for (long ep = 0; ep < number_of_epochs; ep++)
    {
        // losuje poprawki dla czesci parametrow:
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