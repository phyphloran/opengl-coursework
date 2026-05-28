// +++ main.cpp +++

//////////////////////////////////////////
//                                      //
// Учебные приложения по графике OpenGL //
// Цилиндр и тор. Метод концентрических //
// сфер                                //
//                                      //
//////////////////////////////////////////

#define STRICT
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <assert.h>

#include <gl\gl.h>
#include <gl\glu.h>

#pragma comment(linker, "/defaultlib:opengl32.lib")
#pragma comment(linker, "/defaultlib:glu32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HINSTANCE g_hApp = nullptr;
LPCTSTR g_szAppName = _T("Цилиндр и тор. Метод концентрических сфер");
LPCTSTR g_szWndClass = _T("WcOglCylinderTorusSpheres");

HWND g_hWindow = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hGLRC = nullptr;
GLUquadricObj* g_pGluQuadObj = nullptr;

int g_wndWidth = 1000;
int g_wndHeight = 700;
double g_angle = 0.0;

const double g_torusMajorRadius = 7.0;
const double g_torusMinorRadius = 2.45;
const double g_cylinderRadius = 2.85;
const double g_cylinderLength = 20.0;

LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);
BOOL InitApp();
void UninitApp();
void Draw();

BOOL SetupPixelFormat(HDC dc)
{
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(pfd));

	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(dc, &pfd);
	if (!pf)
		return FALSE;

	return SetPixelFormat(dc, pf, &pfd);
}

void SetMaterial(float r, float g, float b, float shininess)
{
	GLfloat ambient[] = { r * 0.25f, g * 0.25f, b * 0.25f, 1.0f };
	GLfloat diffuse[] = { r, g, b, 1.0f };
	GLfloat specular[] = { 0.55f, 0.55f, 0.55f, 1.0f };

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

void DrawAxes(double size)
{
	glDisable(GL_LIGHTING);
	glLineWidth(2.0f);
	glBegin(GL_LINES);
	glColor3d(1.0, 0.15, 0.15);
	glVertex3d(-size, 0.0, 0.0);
	glVertex3d(size, 0.0, 0.0);
	glColor3d(0.1, 0.85, 0.1);
	glVertex3d(0.0, -size, 0.0);
	glVertex3d(0.0, size, 0.0);
	glColor3d(0.15, 0.25, 1.0);
	glVertex3d(0.0, 0.0, -size);
	glVertex3d(0.0, 0.0, size);
	glEnd();
	glLineWidth(1.0f);
	glEnable(GL_LIGHTING);
}

void DrawTorus()
{
	const int uSteps = 96;
	const int vSteps = 32;
	const double R = g_torusMajorRadius;
	const double r = g_torusMinorRadius;

	SetMaterial(0.02f, 0.55f, 0.18f, 64.0f);

	for (int i = 0; i < uSteps; ++i)
	{
		double u0 = 2.0 * M_PI * i / uSteps;
		double u1 = 2.0 * M_PI * (i + 1) / uSteps;

		glBegin(GL_QUAD_STRIP);
		for (int j = 0; j <= vSteps; ++j)
		{
			double v = 2.0 * M_PI * j / vSteps;
			double cv = cos(v);
			double sv = sin(v);

			double cu0 = cos(u0), su0 = sin(u0);
			double cu1 = cos(u1), su1 = sin(u1);

			glNormal3d(cv * cu0, sv, cv * su0);
			glVertex3d((R + r * cv) * cu0, r * sv, (R + r * cv) * su0);

			glNormal3d(cv * cu1, sv, cv * su1);
			glVertex3d((R + r * cv) * cu1, r * sv, (R + r * cv) * su1);
		}
		glEnd();
	}
}

void DrawCylinder()
{
	const double halfLength = g_cylinderLength / 2.0;

	SetMaterial(0.55f, 0.08f, 0.05f, 42.0f);

	glPushMatrix();
	glTranslated(-halfLength, 0.0, 0.0);
	glRotated(90.0, 0.0, 1.0, 0.0);
	gluCylinder(g_pGluQuadObj, g_cylinderRadius, g_cylinderRadius, g_cylinderLength, 64, 10);
	gluDisk(g_pGluQuadObj, 0.0, g_cylinderRadius, 64, 1);
	glTranslated(0.0, 0.0, g_cylinderLength);
	gluDisk(g_pGluQuadObj, 0.0, g_cylinderRadius, 64, 1);
	glPopMatrix();
}

void DrawAuxiliarySphere(double radius)
{
	glDisable(GL_LIGHTING);
	glColor4d(0.95, 0.95, 0.95, 0.32);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	gluSphere(g_pGluQuadObj, radius, 48, 24);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_LIGHTING);
}

void DrawConcentricSpheres()
{
	// Метод концентрических сфер: все вспомогательные сферы имеют общий
	// центр в точке пересечения осей тора и цилиндра. Их радиусы подобраны
	// так, чтобы каждая сфера пересекала обе поверхности и давала точки
	// искомой линии пересечения.
	const double minRadius = sqrt(g_torusMajorRadius * g_torusMajorRadius
		+ g_torusMinorRadius * g_torusMinorRadius
		- 2.0 * g_torusMajorRadius * g_torusMinorRadius);
	const double maxRadius = sqrt(g_torusMajorRadius * g_torusMajorRadius
		+ g_torusMinorRadius * g_torusMinorRadius
		+ 2.0 * g_torusMajorRadius * g_torusMinorRadius);

	for (int i = 1; i <= 5; ++i)
	{
		double k = i / 6.0;
		DrawAuxiliarySphere(minRadius + (maxRadius - minRadius) * k);
	}
}

void DrawIntersectionCurveBranch(int branch)
{
	const double R = g_torusMajorRadius;
	const double r = g_torusMinorRadius;
	const double rc = g_cylinderRadius;
	const int samples = 360;
	bool inStrip = false;

	glDisable(GL_LIGHTING);
	glColor3d(1.0, 0.92, 0.12);
	glLineWidth(4.0f);

	for (int i = 0; i <= samples; ++i)
	{
		double v = 2.0 * M_PI * i / samples;
		double y = r * sin(v);
		double tubeRadius = R + r * cos(v);
		double z2 = rc * rc - y * y;
		bool valid = z2 >= 0.0 && tubeRadius > 0.0;

		if (valid)
		{
			double zAbs = sqrt(z2);
			double s = zAbs / tubeRadius;
			valid = s <= 1.0;
			if (valid)
			{
				double uBase = asin(s);
				double u = 0.0;
				switch (branch)
				{
				case 0: u = uBase; break;
				case 1: u = M_PI - uBase; break;
				case 2: u = M_PI + uBase; break;
				default: u = 2.0 * M_PI - uBase; break;
				}

				double x = tubeRadius * cos(u);
				double z = tubeRadius * sin(u);

				if (!inStrip)
				{
					glBegin(GL_LINE_STRIP);
					inStrip = true;
				}
				glVertex3d(x, y, z);
			}
		}

		if (!valid && inStrip)
		{
			glEnd();
			inStrip = false;
		}
	}

	if (inStrip)
		glEnd();

	glLineWidth(1.0f);
	glEnable(GL_LIGHTING);
}

void DrawIntersectionCurves()
{
	for (int branch = 0; branch < 4; ++branch)
		DrawIntersectionCurveBranch(branch);
}

void SetupLighting()
{
	GLfloat light0Pos[] = { -12.0f, 12.0f, 14.0f, 1.0f };
	GLfloat light1Pos[] = { 10.0f, -8.0f, -6.0f, 1.0f };
	GLfloat light0Diffuse[] = { 1.0f, 0.96f, 0.9f, 1.0f };
	GLfloat light1Diffuse[] = { 0.35f, 0.45f, 0.65f, 1.0f };
	GLfloat ambient[] = { 0.18f, 0.18f, 0.18f, 1.0f };

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHT1);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
	glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}

void Draw()
{
	int side = g_wndWidth < g_wndHeight ? g_wndWidth : g_wndHeight;
	glViewport((g_wndWidth - side) / 2, (g_wndHeight - side) / 2, side, side);

	glClearColor(0.93f, 0.95f, 0.95f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(42.0, 1.0, 1.0, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0, 10.5, 27.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

	SetupLighting();

	glRotated(18.0, 1.0, 0.0, 0.0);
	glRotated(g_angle, 0.0, 1.0, 0.0);
	glRotated(-28.0, 0.0, 0.0, 1.0);

	DrawConcentricSpheres();
	DrawTorus();
	DrawCylinder();
	DrawIntersectionCurves();
	DrawAxes(11.0);

	glFinish();
	SwapBuffers(g_hDC);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		g_hDC = GetDC(hwnd);
		if (!SetupPixelFormat(g_hDC))
			return -1;

		g_hGLRC = wglCreateContext(g_hDC);
		wglMakeCurrent(g_hDC, g_hGLRC);

		g_pGluQuadObj = gluNewQuadric();
		assert(g_pGluQuadObj);
		gluQuadricNormals(g_pGluQuadObj, GLU_SMOOTH);
		gluQuadricDrawStyle(g_pGluQuadObj, GLU_FILL);

		SetTimer(hwnd, 1, 30, nullptr);
		return 0;

	case WM_SIZE:
		g_wndWidth = LOWORD(lParam);
		g_wndHeight = HIWORD(lParam);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		Draw();
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_TIMER:
		g_angle += 0.8;
		if (g_angle >= 360.0)
			g_angle -= 360.0;
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		if (g_pGluQuadObj)
		{
			gluDeleteQuadric(g_pGluQuadObj);
			g_pGluQuadObj = nullptr;
		}
		wglMakeCurrent(nullptr, nullptr);
		if (g_hGLRC)
		{
			wglDeleteContext(g_hGLRC);
			g_hGLRC = nullptr;
		}
		if (g_hDC)
		{
			ReleaseDC(hwnd, g_hDC);
			g_hDC = nullptr;
		}
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL InitApp()
{
	WNDCLASSEX wce;
	ZeroMemory(&wce, sizeof(wce));
	wce.cbSize = sizeof(wce);
	wce.hInstance = g_hApp;
	wce.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wce.lpfnWndProc = MainWindowProc;
	wce.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wce.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wce.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wce.lpszClassName = g_szWndClass;

	if (!RegisterClassEx(&wce))
		return FALSE;

	g_hWindow = CreateWindow(
		g_szWndClass,
		g_szAppName,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1000, 700,
		nullptr, nullptr, g_hApp, nullptr);

	if (!g_hWindow)
		return FALSE;

	ShowWindow(g_hWindow, SW_SHOW);
	UpdateWindow(g_hWindow);
	return TRUE;
}

void UninitApp()
{
	UnregisterClass(g_szWndClass, g_hApp);
}

int APIENTRY WinMain(HINSTANCE hApp, HINSTANCE, LPSTR, int)
{
	g_hApp = hApp;

	if (InitApp())
	{
		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UninitApp();
	return 0;
}

// --- main.cpp ---
