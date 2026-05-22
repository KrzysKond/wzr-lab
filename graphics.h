#include <gl\gl.h>
#include <gl\glu.h>
#ifndef _VECTOR3D_H
  #include "vector3D.h"
#endif


enum GLDisplayListNames
{
	Wall1 = 1,
	Wall2 = 2,
	Floor = 3,
	Cube = 4,
	Cube_skel = 5,
	TerrainSurface = 6
};

// Stale (wartości początkowe)

struct ViewParameters
{
	Vector3 initial_camera_direction;
	Vector3 initial_camera_position;
	Vector3 initial_camera_vertical;
	bool tracking;                                         // czy śledzenie obiektu (widok z kokpitu, podążanie za pojazdem w widoku z góry)
	bool tempor_tracking;                                  // chwilowa informacja o śledzeniu (np. by ją później przywrócić)
	bool top_view;                                         // czy widok od góry
	float distance;                                        // odległość od śledzonego obiektu
	float zoom;
	float cam_angle_z;
	float shift_to_right;                                  // przesunięcie kamery w prawo (w lewo o wart. ujemnej) - chodzi głównie o tryb edycji
	float shift_to_bottom;                                 // przesunięcie do dołu (w górę o wart. ujemnej)          i widok z góry (klawisz Q)  
	float tempor_shift_to_right, tempor_shift_to_bottom;   // wartości chwilowe (w trakcie przesuwania)
	char inscription1[512], inscription2[512];             // napisy w trybie graficznym
};

void StandardViewParametersSetting(ViewParameters *p);

// Ustwienia kamery w zależności od wartości początkowych (w interakcji), wartości ustawianych
// przez użytkowika oraz stanu obiektu (np. gdy tryb śledzenia)
void CameraSettings(Vector3 *position, Vector3 *direction, Vector3 *vertical, ViewParameters pw);

int GraphicsInitialization(HDC g_context);
void DrawScene();
void WindowSizeChange(int cx,int cy);

Vector3 Cursor3dCoordinates(int x, int y); // współrzędne 3D punktu na obrazie 2D
Vector3 Cursor3dCoordinates(int x, int y, float height); // współrzędne 3D punktu na obrazie 2D
void ScreenCoordinates(float *xx, float *yy, float *zz, Vector3 Point3D); // współrzędne punktu na ekranie na podstawie wsp 3D

void EndOfGraphics();

BOOL SetWindowPixelFormat(HDC hDC);
BOOL CreateViewGLContext(HDC hDC);
GLvoid BuildFont(HDC hDC);
GLvoid glPrint(const char *fmt, ...);