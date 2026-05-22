#ifndef _OBJECTS__H
#include "objects.h"
#endif

class AutoPilot
{       
private:
	float par[100]; // parametry pocz¹tkowe i parametry aktualne
	long number_of_params;
public:
  AutoPilot();
  void AutoControl(MovableObject *ob);                        // pojedynczy krok sterowania
  void ControlTest(MovableObject *_ob,float krok_czasowy, float czas_proby); 
  void ParametersSimAnnealing(long number_of_epochs, float krok_czasowy, float czas_proby);
};