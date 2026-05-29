// +++ main.cpp +++

//////////////////////////////////////////
//                                      //
// Учебные приложения по графике OpenGL //
// Цилиндр и тор. Метод пересечения     //
// плоскостей                           //
//                                      //
//////////////////////////////////////////

#define STRICT
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>

#include <gl\gl.h>
#include <gl\glu.h>

#pragma execution_character_set("utf-8")
#pragma comment(linker, "/defaultlib:opengl32.lib")
#pragma comment(linker, "/defaultlib:glu32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct SceneParams
{
	double torusMajorRadius;
	double torusMinorRadius;
	double cylinderRadius;
	double cylinderLength;
	double cylinderOffsetZ;
	int planeCount;
	bool showAuxiliaryPlanes;
	bool showSectionContours;
	bool showIntersectionCurve;
};

HINSTANCE g_hApp = nullptr;
LPCTSTR g_szAppName = L"\u0426\u0438\u043b\u0438\u043d\u0434\u0440 \u0438 \u0442\u043e\u0440. "
	L"\u041c\u0435\u0442\u043e\u0434 \u043f\u0435\u0440\u0435\u0441\u0435\u0447\u0435\u043d\u0438\u044f "
	L"\u043f\u043b\u043e\u0441\u043a\u043e\u0441\u0442\u0435\u0439";
LPCTSTR g_szWndClass = _T("WcOglCylinderTorusPlanes");
LPCTSTR g_szControlWndClass = _T("WcOglCylinderTorusControls");

HWND g_hWindow = nullptr;
HWND g_hControlWindow = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hGLRC = nullptr;
GLUquadricObj* g_pGluQuadObj = nullptr;

int g_wndWidth = 1000;
int g_wndHeight = 700;
bool g_isLeftMouseDown = false;
POINT g_lastMousePos = { 0, 0 };

double g_cameraYaw = -38.0;
double g_cameraPitch = 24.0;
double g_cameraDistance = 30.0;

SceneParams g_defaultScene =
{
	7.0,
	2.45,
	2.85,
	20.0,
	7.3,
	5,
	true,
	true,
	true
};

SceneParams g_scene = g_defaultScene;

enum ControlIds
{
	IDC_EDIT_TORUS_MAJOR = 101,
	IDC_EDIT_TORUS_MINOR = 102,
	IDC_EDIT_CYL_RADIUS = 103,
	IDC_EDIT_CYL_LENGTH = 104,
	IDC_EDIT_CYL_OFFSET = 105,
	IDC_EDIT_PLANE_COUNT = 106,
	IDC_CHECK_SHOW_PLANES = 107,
	IDC_CHECK_SHOW_SECTIONS = 108,
	IDC_CHECK_SHOW_INTERSECTION = 109,
	IDC_BUTTON_APPLY = 110,
	IDC_BUTTON_RESET = 111
};

LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ControlWindowProc(HWND, UINT, WPARAM, LPARAM);
BOOL InitApp();
void UninitApp();
void Draw();
void UpdateControlValuesFromScene();
void ApplySceneFromControls();

double Clamp(double value, double minValue, double maxValue)
{
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;
	return value;
}

int ClampInt(int value, int minValue, int maxValue)
{
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;
	return value;
}

double MinDouble(double a, double b)
{
	return a < b ? a : b;
}

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
	GLfloat ambient[] = { r * 0.58f, g * 0.58f, b * 0.58f, 1.0f };
	GLfloat diffuse[] = { r, g, b, 1.0f };
	GLfloat specular[] = { 0.28f, 0.28f, 0.28f, 1.0f };

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

double GetPlaneHalfSize()
{
	double maxRadius = g_scene.torusMajorRadius + g_scene.torusMinorRadius + 1.5;
	double cylinderReach = fabs(g_scene.cylinderOffsetZ) + g_scene.cylinderRadius + 1.5;
	return maxRadius > cylinderReach ? maxRadius : cylinderReach;
}

double GetSectionPlaneExtent()
{
	double limit = MinDouble(g_scene.torusMinorRadius, g_scene.cylinderRadius);
	return limit > 0.05 ? limit * 0.95 : 0.0;
}

bool TryGetSliceData(double y, double& torusOuterRadius, double& torusInnerRadius, bool& hasInnerCircle, double& cylinderZDelta)
{
	double torusRadicand = g_scene.torusMinorRadius * g_scene.torusMinorRadius - y * y;
	double cylinderRadicand = g_scene.cylinderRadius * g_scene.cylinderRadius - y * y;
	if (torusRadicand < 0.0 || cylinderRadicand < 0.0)
		return false;

	double torusCrossOffset = sqrt(torusRadicand);
	torusOuterRadius = g_scene.torusMajorRadius + torusCrossOffset;
	torusInnerRadius = g_scene.torusMajorRadius - torusCrossOffset;
	hasInnerCircle = torusInnerRadius > 1e-6;
	cylinderZDelta = sqrt(cylinderRadicand);
	return true;
}

void DrawCircleOnHorizontalPlane(double y, double radius)
{
	const int samples = 128;
	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < samples; ++i)
	{
		double angle = 2.0 * M_PI * i / samples;
		glVertex3d(radius * cos(angle), y, radius * sin(angle));
	}
	glEnd();
}

void DrawCylinderSliceLine(double y, double z)
{
	const double lineHalfLength = g_scene.cylinderLength / 2.0;
	glBegin(GL_LINES);
	glVertex3d(-lineHalfLength, y, z);
	glVertex3d(lineHalfLength, y, z);
	glEnd();
}

void DrawTorus()
{
	const int uSteps = 96;
	const int vSteps = 32;
	const double R = g_scene.torusMajorRadius;
	const double r = g_scene.torusMinorRadius;

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
	const double halfLength = g_scene.cylinderLength / 2.0;

	SetMaterial(0.55f, 0.08f, 0.05f, 42.0f);

	glPushMatrix();
	glTranslated(0.0, 0.0, g_scene.cylinderOffsetZ);
	glTranslated(-halfLength, 0.0, 0.0);
	glRotated(90.0, 0.0, 1.0, 0.0);
	gluCylinder(g_pGluQuadObj, g_scene.cylinderRadius, g_scene.cylinderRadius, g_scene.cylinderLength, 64, 10);
	gluDisk(g_pGluQuadObj, 0.0, g_scene.cylinderRadius, 64, 1);
	glTranslated(0.0, 0.0, g_scene.cylinderLength);
	gluDisk(g_pGluQuadObj, 0.0, g_scene.cylinderRadius, 64, 1);
	glPopMatrix();
}

void DrawAuxiliaryPlane(double y)
{
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const double planeHalfSize = GetPlaneHalfSize();
	glColor4d(0.72, 0.78, 0.95, 0.10);
	glBegin(GL_QUADS);
	glVertex3d(-planeHalfSize, y, -planeHalfSize);
	glVertex3d(planeHalfSize, y, -planeHalfSize);
	glVertex3d(planeHalfSize, y, planeHalfSize);
	glVertex3d(-planeHalfSize, y, planeHalfSize);
	glEnd();

	glColor4d(0.75, 0.82, 1.0, 0.45);
	glLineWidth(1.5f);
	glBegin(GL_LINE_LOOP);
	glVertex3d(-planeHalfSize, y, -planeHalfSize);
	glVertex3d(planeHalfSize, y, -planeHalfSize);
	glVertex3d(planeHalfSize, y, planeHalfSize);
	glVertex3d(-planeHalfSize, y, planeHalfSize);
	glEnd();
	glLineWidth(1.0f);

	glDisable(GL_BLEND);
	glEnable(GL_LIGHTING);
}

void DrawPlaneSectionContours(double y)
{
	double torusOuterRadius = 0.0;
	double torusInnerRadius = 0.0;
	double cylinderZDelta = 0.0;
	bool hasInnerCircle = false;
	if (!TryGetSliceData(y, torusOuterRadius, torusInnerRadius, hasInnerCircle, cylinderZDelta))
		return;

	glDisable(GL_LIGHTING);

	glColor3d(0.16, 0.98, 0.48);
	glLineWidth(2.0f);
	DrawCircleOnHorizontalPlane(y, torusOuterRadius);
	if (hasInnerCircle)
		DrawCircleOnHorizontalPlane(y, torusInnerRadius);

	glColor3d(0.98, 0.44, 0.38);
	DrawCylinderSliceLine(y, g_scene.cylinderOffsetZ + cylinderZDelta);
	DrawCylinderSliceLine(y, g_scene.cylinderOffsetZ - cylinderZDelta);

	glLineWidth(1.0f);
	glEnable(GL_LIGHTING);
}

void DrawIntersectingPlanes()
{
	const int planeCount = ClampInt(g_scene.planeCount, 1, 20);
	const double yExtent = GetSectionPlaneExtent();
	if (yExtent <= 0.0)
		return;

	for (int i = 0; i < planeCount; ++i)
	{
		double t = planeCount > 1 ? (double)i / (double)(planeCount - 1) : 0.5;
		double y = -yExtent + 2.0 * yExtent * t;

		if (g_scene.showAuxiliaryPlanes)
		{
			DrawAuxiliaryPlane(y);
			if (g_scene.showSectionContours)
				DrawPlaneSectionContours(y);
		}
	}
}

void DrawIntersectionCurveBranch(int branch)
{
	const double R = g_scene.torusMajorRadius;
	const double r = g_scene.torusMinorRadius;
	const double rc = g_scene.cylinderRadius;
	const double zOffset = g_scene.cylinderOffsetZ;
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
			double zLocal = sqrt(z2);
			double z = zOffset;
			double x = 0.0;

			switch (branch)
			{
			case 0: z += zLocal; break;
			case 1: z += zLocal; break;
			case 2: z -= zLocal; break;
			default: z -= zLocal; break;
			}

			double x2 = tubeRadius * tubeRadius - z * z;
			valid = x2 >= 0.0;
			if (valid)
			{
				switch (branch)
				{
				case 0:
				case 2:
					x = sqrt(x2);
					break;
				default:
					x = -sqrt(x2);
					break;
				}

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
	if (!g_scene.showIntersectionCurve)
		return;

	for (int branch = 0; branch < 4; ++branch)
		DrawIntersectionCurveBranch(branch);
}

void SetupLighting()
{
	GLfloat light0Pos[] = { -12.0f, 12.0f, 14.0f, 1.0f };
	GLfloat light1Pos[] = { 10.0f, -8.0f, -6.0f, 1.0f };
	GLfloat light0Diffuse[] = { 1.0f, 0.98f, 0.94f, 1.0f };
	GLfloat light1Diffuse[] = { 0.38f, 0.42f, 0.55f, 1.0f };
	GLfloat ambient[] = { 0.42f, 0.42f, 0.42f, 1.0f };

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
	glViewport(0, 0, g_wndWidth, g_wndHeight);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	double aspect = g_wndHeight > 0 ? (double)g_wndWidth / (double)g_wndHeight : 1.0;
	gluPerspective(42.0, aspect, 1.0, 100.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	double yawRad = g_cameraYaw * M_PI / 180.0;
	double pitchRad = g_cameraPitch * M_PI / 180.0;
	double cameraX = g_cameraDistance * cos(pitchRad) * cos(yawRad);
	double cameraY = g_cameraDistance * sin(pitchRad);
	double cameraZ = g_cameraDistance * cos(pitchRad) * sin(yawRad);
	gluLookAt(cameraX, cameraY, cameraZ, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

	SetupLighting();

	glRotated(16.0, 1.0, 0.0, 0.0);
	glRotated(-26.0, 0.0, 1.0, 0.0);
	glRotated(-34.0, 0.0, 0.0, 1.0);

	DrawIntersectingPlanes();
	DrawTorus();
	DrawCylinder();
	DrawIntersectionCurves();

	glFinish();
	SwapBuffers(g_hDC);
}

void SetWindowTextFromDouble(HWND hwnd, int controlId, double value)
{
	TCHAR buffer[64];
	_stprintf_s(buffer, _T("%.3f"), value);
	SetWindowText(GetDlgItem(hwnd, controlId), buffer);
}

void SetWindowTextFromInt(HWND hwnd, int controlId, int value)
{
	TCHAR buffer[32];
	_stprintf_s(buffer, _T("%d"), value);
	SetWindowText(GetDlgItem(hwnd, controlId), buffer);
}

double ReadDoubleFromControl(HWND hwnd, int controlId, double fallbackValue)
{
	TCHAR buffer[64];
	GetWindowText(GetDlgItem(hwnd, controlId), buffer, 64);
	TCHAR* endPtr = nullptr;
	double value = _tcstod(buffer, &endPtr);
	return endPtr != buffer ? value : fallbackValue;
}

int ReadIntFromControl(HWND hwnd, int controlId, int fallbackValue)
{
	TCHAR buffer[32];
	GetWindowText(GetDlgItem(hwnd, controlId), buffer, 32);
	TCHAR* endPtr = nullptr;
	long value = _tcstol(buffer, &endPtr, 10);
	return endPtr != buffer ? (int)value : fallbackValue;
}

void UpdateControlValuesFromScene()
{
	if (!g_hControlWindow)
		return;

	SetWindowTextFromDouble(g_hControlWindow, IDC_EDIT_TORUS_MAJOR, g_scene.torusMajorRadius);
	SetWindowTextFromDouble(g_hControlWindow, IDC_EDIT_TORUS_MINOR, g_scene.torusMinorRadius);
	SetWindowTextFromDouble(g_hControlWindow, IDC_EDIT_CYL_RADIUS, g_scene.cylinderRadius);
	SetWindowTextFromDouble(g_hControlWindow, IDC_EDIT_CYL_LENGTH, g_scene.cylinderLength);
	SetWindowTextFromDouble(g_hControlWindow, IDC_EDIT_CYL_OFFSET, g_scene.cylinderOffsetZ);
	SetWindowTextFromInt(g_hControlWindow, IDC_EDIT_PLANE_COUNT, g_scene.planeCount);

	CheckDlgButton(g_hControlWindow, IDC_CHECK_SHOW_PLANES, g_scene.showAuxiliaryPlanes ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(g_hControlWindow, IDC_CHECK_SHOW_SECTIONS, g_scene.showSectionContours ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(g_hControlWindow, IDC_CHECK_SHOW_INTERSECTION, g_scene.showIntersectionCurve ? BST_CHECKED : BST_UNCHECKED);
}

void ApplySceneFromControls()
{
	if (!g_hControlWindow)
		return;

	SceneParams nextScene = g_scene;
	nextScene.torusMajorRadius = Clamp(ReadDoubleFromControl(g_hControlWindow, IDC_EDIT_TORUS_MAJOR, g_scene.torusMajorRadius), 1.0, 20.0);
	nextScene.torusMinorRadius = Clamp(ReadDoubleFromControl(g_hControlWindow, IDC_EDIT_TORUS_MINOR, g_scene.torusMinorRadius), 0.2, 10.0);
	nextScene.cylinderRadius = Clamp(ReadDoubleFromControl(g_hControlWindow, IDC_EDIT_CYL_RADIUS, g_scene.cylinderRadius), 0.2, 15.0);
	nextScene.cylinderLength = Clamp(ReadDoubleFromControl(g_hControlWindow, IDC_EDIT_CYL_LENGTH, g_scene.cylinderLength), 2.0, 40.0);
	nextScene.cylinderOffsetZ = Clamp(ReadDoubleFromControl(g_hControlWindow, IDC_EDIT_CYL_OFFSET, g_scene.cylinderOffsetZ), -25.0, 25.0);
	nextScene.planeCount = ClampInt(ReadIntFromControl(g_hControlWindow, IDC_EDIT_PLANE_COUNT, g_scene.planeCount), 1, 20);
	nextScene.showAuxiliaryPlanes = IsDlgButtonChecked(g_hControlWindow, IDC_CHECK_SHOW_PLANES) == BST_CHECKED;
	nextScene.showSectionContours = IsDlgButtonChecked(g_hControlWindow, IDC_CHECK_SHOW_SECTIONS) == BST_CHECKED;
	nextScene.showIntersectionCurve = IsDlgButtonChecked(g_hControlWindow, IDC_CHECK_SHOW_INTERSECTION) == BST_CHECKED;

	if (nextScene.torusMinorRadius >= nextScene.torusMajorRadius)
		nextScene.torusMinorRadius = Clamp(nextScene.torusMajorRadius - 0.2, 0.2, nextScene.torusMinorRadius);

	g_scene = nextScene;
	UpdateControlValuesFromScene();
	InvalidateRect(g_hWindow, nullptr, FALSE);
}

void CreateControlLabel(HWND hwnd, LPCTSTR text, int x, int y, int w, int h)
{
	CreateWindow(_T("STATIC"), text, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd, nullptr, g_hApp, nullptr);
}

HWND CreateControlEdit(HWND hwnd, int controlId, int x, int y, int w, int h)
{
	return CreateWindowEx(
		WS_EX_CLIENTEDGE,
		_T("EDIT"),
		_T(""),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
		x, y, w, h,
		hwnd,
		(HMENU)(INT_PTR)controlId,
		g_hApp,
		nullptr);
}

void CreateControlCheckbox(HWND hwnd, LPCTSTR text, int controlId, int x, int y, int w, int h)
{
	CreateWindow(
		_T("BUTTON"),
		text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
		x, y, w, h,
		hwnd,
		(HMENU)(INT_PTR)controlId,
		g_hApp,
		nullptr);
}

void CreateControlButton(HWND hwnd, LPCTSTR text, int controlId, int x, int y, int w, int h)
{
	CreateWindow(
		_T("BUTTON"),
		text,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		x, y, w, h,
		hwnd,
		(HMENU)(INT_PTR)controlId,
		g_hApp,
		nullptr);
}

void CreateControlWindowContents(HWND hwnd)
{
	const int labelX = 14;
	const int editX = 182;
	const int labelWidth = 160;
	const int editWidth = 92;
	const int rowHeight = 24;
	const int rowGap = 30;
	int y = 16;

	CreateControlLabel(hwnd, L"\u0420\u0430\u0434\u0438\u0443\u0441 \u0442\u043e\u0440\u0430 R", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_TORUS_MAJOR, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlLabel(hwnd, L"\u0422\u043e\u043b\u0449\u0438\u043d\u0430 \u0442\u043e\u0440\u0430 r", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_TORUS_MINOR, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlLabel(hwnd, L"\u0420\u0430\u0434\u0438\u0443\u0441 \u0446\u0438\u043b\u0438\u043d\u0434\u0440\u0430", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_CYL_RADIUS, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlLabel(hwnd, L"\u0414\u043b\u0438\u043d\u0430 \u0446\u0438\u043b\u0438\u043d\u0434\u0440\u0430", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_CYL_LENGTH, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlLabel(hwnd, L"\u0421\u043c\u0435\u0449\u0435\u043d\u0438\u0435 \u0446\u0438\u043b\u0438\u043d\u0434\u0440\u0430 Z", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_CYL_OFFSET, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlLabel(hwnd, L"\u0427\u0438\u0441\u043b\u043e \u043f\u043b\u043e\u0441\u043a\u043e\u0441\u0442\u0435\u0439", labelX, y, labelWidth, rowHeight);
	CreateControlEdit(hwnd, IDC_EDIT_PLANE_COUNT, editX, y - 2, editWidth, rowHeight);
	y += rowGap;

	CreateControlCheckbox(hwnd, L"\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043f\u043b\u043e\u0441\u043a\u043e\u0441\u0442\u0438", IDC_CHECK_SHOW_PLANES, labelX, y, 250, rowHeight);
	y += 28;
	CreateControlCheckbox(hwnd, L"\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u0441\u0435\u0447\u0435\u043d\u0438\u044f \u0444\u0438\u0433\u0443\u0440", IDC_CHECK_SHOW_SECTIONS, labelX, y, 250, rowHeight);
	y += 28;
	CreateControlCheckbox(hwnd, L"\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043b\u0438\u043d\u0438\u044e \u043f\u0435\u0440\u0435\u0441\u0435\u0447\u0435\u043d\u0438\u044f", IDC_CHECK_SHOW_INTERSECTION, labelX, y, 250, rowHeight);
	y += 40;

	CreateControlButton(hwnd, L"\u041f\u0440\u0438\u043c\u0435\u043d\u0438\u0442\u044c", IDC_BUTTON_APPLY, 20, y, 120, 28);
	CreateControlButton(hwnd, L"\u0421\u0431\u0440\u043e\u0441\u0438\u0442\u044c", IDC_BUTTON_RESET, 160, y, 120, 28);

	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	for (HWND child = GetWindow(hwnd, GW_CHILD); child != nullptr; child = GetWindow(child, GW_HWNDNEXT))
		SendMessage(child, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void ShowControlWindow()
{
	if (!g_hControlWindow)
		return;

	ShowWindow(g_hControlWindow, SW_SHOWNORMAL);
	SetForegroundWindow(g_hControlWindow);
	UpdateControlValuesFromScene();
}

LRESULT CALLBACK ControlWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		CreateControlWindowContents(hwnd);
		UpdateControlValuesFromScene();
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_APPLY:
			ApplySceneFromControls();
			return 0;

		case IDC_BUTTON_RESET:
			g_scene = g_defaultScene;
			UpdateControlValuesFromScene();
			InvalidateRect(g_hWindow, nullptr, FALSE);
			return 0;
		}
		break;

	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
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

		return 0;

	case WM_SIZE:
		g_wndWidth = LOWORD(lParam);
		g_wndHeight = HIWORD(lParam);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		Draw();
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_LBUTTONDOWN:
		g_isLeftMouseDown = true;
		g_lastMousePos.x = (short)LOWORD(lParam);
		g_lastMousePos.y = (short)HIWORD(lParam);
		SetCapture(hwnd);
		return 0;

	case WM_MOUSEMOVE:
		if (g_isLeftMouseDown)
		{
			int mouseX = (short)LOWORD(lParam);
			int mouseY = (short)HIWORD(lParam);
			int deltaX = mouseX - g_lastMousePos.x;
			int deltaY = mouseY - g_lastMousePos.y;

			g_cameraYaw += deltaX * 0.45;
			g_cameraPitch = Clamp(g_cameraPitch - deltaY * 0.35, -80.0, 80.0);

			g_lastMousePos.x = mouseX;
			g_lastMousePos.y = mouseY;
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;

	case WM_LBUTTONUP:
		if (g_isLeftMouseDown)
		{
			g_isLeftMouseDown = false;
			ReleaseCapture();
		}
		return 0;

	case WM_KEYDOWN:
		if (wParam == 'E' || wParam == 'e')
		{
			g_scene.showAuxiliaryPlanes = !g_scene.showAuxiliaryPlanes;
			UpdateControlValuesFromScene();
			InvalidateRect(hwnd, nullptr, FALSE);
			return 0;
		}

		if (wParam == 'P' || wParam == 'p')
		{
			ShowControlWindow();
			return 0;
		}

		InvalidateRect(hwnd, nullptr, FALSE);
		break;

	case WM_DESTROY:
		if (g_hControlWindow)
		{
			DestroyWindow(g_hControlWindow);
			g_hControlWindow = nullptr;
		}
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

	WNDCLASSEX controlClass;
	ZeroMemory(&controlClass, sizeof(controlClass));
	controlClass.cbSize = sizeof(controlClass);
	controlClass.hInstance = g_hApp;
	controlClass.style = CS_VREDRAW | CS_HREDRAW;
	controlClass.lpfnWndProc = ControlWindowProc;
	controlClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	controlClass.hIcon = LoadIcon(nullptr, IDI_INFORMATION);
	controlClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	controlClass.lpszClassName = g_szControlWndClass;

	if (!RegisterClassEx(&controlClass))
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

	g_hControlWindow = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		g_szControlWndClass,
		L"\u041f\u0430\u0440\u0430\u043c\u0435\u0442\u0440\u044b \u0444\u0438\u0433\u0443\u0440 \u0438 \u0441\u0435\u0447\u0435\u043d\u0438\u0439",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		120, 120,
		315, 360,
		g_hWindow,
		nullptr,
		g_hApp,
		nullptr);

	if (!g_hControlWindow)
		return FALSE;

	UpdateControlValuesFromScene();
	ShowWindow(g_hWindow, SW_SHOW);
	UpdateWindow(g_hWindow);
	ShowWindow(g_hControlWindow, SW_SHOW);
	UpdateWindow(g_hControlWindow);
	return TRUE;
}

void UninitApp()
{
	UnregisterClass(g_szControlWndClass, g_hApp);
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
