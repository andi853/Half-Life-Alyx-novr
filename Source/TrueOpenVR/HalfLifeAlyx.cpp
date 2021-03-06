#include <windows.h>
#include <atlstr.h> 
#include <math.h>
#include "IniReader\IniReader.h"

#define DLLEXPORT extern "C" __declspec(dllexport)

typedef struct _HMDData
{
	double	X;
	double	Y;
	double	Z;
	double	Yaw;
	double	Pitch;
	double	Roll;
} THMD, *PHMD;

typedef struct _Controller
{
	double	X;
	double	Y;
	double	Z;
	double	Yaw;
	double	Pitch;
	double	Roll;
	unsigned short	Buttons;
	float	Trigger;
	float	AxisX;
	float	AxisY;
} TController, *PController;

#define TOVR_SUCCESS 0
#define TOVR_FAILURE 1

#define GRIP_BTN	0x0001
#define THUMB_BTN	0x0002
#define A_BTN		0x0004
#define B_BTN		0x0008
#define MENU_BTN	0x0010
#define SYS_BTN		0x0020

//Key bindings
int KEY_ID_LEFT_TRIGGER;
int KEY_ID_LEFT_MENU;
int KEY_ID_LEFT_GRIP;
int KEY_ID_LEFT_SYSTEM;
int KEY_ID_RIGHT_TRIGGER;
int KEY_ID_RIGHT_TRIGGER2;
int KEY_ID_RIGHT_MENU;
int KEY_ID_RIGHT_GRIP;
int KEY_ID_CHANGE_WEAPON;
int KEY_ID_TELEPORT;
int KEY_ID_BACKPACK;
int KEY_ID_FIRST_AID_SYRINGE;
int KEY_ID_FIXING_LEFT_CONTROLLER;
int KEY_ID_THROW_ITEMS;
int KEY_ID_HEALTH_AND_AMMO_DISPLAY;
int KEY_ID_MOVE_CONTROLLERS_FORWARD;
int KEY_ID_MOVE_CONTROLLERS_BACK;
int KEY_ID_PLAYER_RISE_HIGHER;
int KEY_ID_PLAYER_RISE_LOWER;
int KEY_ID_PLAYER_RISE_RESET;
int KEY_ID_CROUCH;
int KEY_ID_COVER_MOUTH;
int KEY_ID_LASER_MODE;
int KEY_ID_THROW_ENERGY_BALL;
int KEY_ID_AIMING_MODE;
int KEY_ID_AIMING;
int KEY_ID_LEFT_HAND_CELL;
int KEY_ID_LEFT_CTRL_MOTION;

int KEY_ID_ROTATION_CONTROLLERS_UP;
int KEY_ID_ROTATION_CONTROLLERS_DOWN;
int KEY_ID_ROTATION_CONTROLLERS_LEFT;
int KEY_ID_ROTATION_CONTROLLERS_RIGHT;
int KEY_ID_ROTATION_CONTROLLERS_RESET;

int KEY_ID_MOTION_MODE_MOVE_UP;
int KEY_ID_MOTION_MODE_MOVE_DOWN;

int KEY_ID_TURN_LEFT;
int KEY_ID_TURN_RIGHT;

int KEY_ID_UP;
int KEY_ID_DOWN;
int KEY_ID_LEFT;
int KEY_ID_RIGHT;
//end key bindings


#define StepPos 0.01; //0.0033;
#define StepRot 0.4;
double HMDPosZ = 0;

double FirstCtrlPos[3] = { 0, 0, 0 }, SecondCtrlPos[3] = { 0, 0, 0 };
double CtrlsRoll = 0, CtrlsPitch = 0;
float CrouchOffsetZ = 0;

double OffsetYPR(float f, float f2)
{
	f -= f2;
	if (f < -180)
		f += 360;
	else if (f > 180)
		f -= 360;

	return f;
}

double DegToRad(double f) {
	return f * (3.14159265358979323846 / 180);
}

THMD mouseHMD;
bool firstCP = true;
int m_HalfWidth = 1920 / 2;
int m_HalfHeight = 1080 / 2;
float mouseSensetivePitch = 0.04;
float mouseSensetiveYaw = 0.05;
float mouseYaw = 0, mousePitch = 0; 

HWND SteamVRHeadsetWindow = 0;
HWND HalfLifeAlyxWindow = 0;
bool HeadsetWindowFocused = false;
bool HalfLifeAlyxFocused = false;
bool VRMode = false;

void MouseToYawPitch()
{
	POINT mousePos;

	if (firstCP) {
		SetCursorPos(m_HalfWidth, m_HalfHeight);
		firstCP = false;
	}

	GetCursorPos(&mousePos);

	mouseYaw += (mousePos.x - m_HalfWidth) * mouseSensetiveYaw * -1;
	if (mouseYaw > 360)
		mouseYaw = 0;
	if (mouseYaw < 0)
		mouseYaw = 360;
	mouseHMD.Yaw = OffsetYPR(mouseYaw - 180, 180);

	mousePitch += (mousePos.y - m_HalfHeight) * mouseSensetivePitch * -1;
	if (mousePitch > 90)
		mousePitch = 90;
	else if (mousePitch < -90)
		mousePitch = -90;
	mouseHMD.Pitch = mousePitch;

	SetCursorPos(m_HalfWidth, m_HalfHeight);
}

DLLEXPORT DWORD __stdcall GetHMDData(__out THMD *HMD)
{
	if ((GetAsyncKeyState(KEY_ID_PLAYER_RISE_HIGHER) & 0x8000) != 0) HMDPosZ += StepPos;
	if ((GetAsyncKeyState(KEY_ID_PLAYER_RISE_LOWER) & 0x8000) != 0) HMDPosZ -= StepPos;
	if ((GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_PLAYER_RISE_RESET) & 0x8000) != 0) HMDPosZ = 0; //Minus numpad or -

	HMD->X = 0;
	HMD->Y = 0;
	HMD->Z = HMDPosZ - CrouchOffsetZ;

	HMD->Yaw = mouseHMD.Yaw * -1;
	HMD->Pitch = mouseHMD.Pitch * -1;
	HMD->Roll = 0;

	return TOVR_SUCCESS;
}


float oldPitch = 0, deltaPitch = 0;
float oldRoll = 0, deltaRoll = 0;
double CtrlPosOffset[3] = { 0, 0, 0 };

//For fix left controller
double FirstCtrlLastXYZ[3] = { 0, 0, 0 };
double FirstCtrlLastYPR[3] = { 0, 0, 0 };

double SecondCtrlYPROffset[3] = { 0, 0, 0 };
double SecondCtrlLastYPR[3] = { 0, 0, 0 };


float LeftHandYaw = 0, LeftHandPitch = 0, LeftAngleDist = 0, LeftRadDist = 0, LeftYOffset = 0;
float RightAngleDist = 0; //Граната, немного раздвигаем руки
float RightHandYaw = 0, RightHandPitch = 0, RightRadDist = 0;

float HandRadDist = 0;

//Motion
float MotionLeftRadDist = 0, MotionLeftAngleDist = 0;


int GranadeTick = 0;
bool UseGranade = false;

bool Aiming = false;
bool CoverMouth = false;
bool LaserMode = false;
float LaserPos = 0, LaserYaw = 0;

bool UsedEnergyBall = false;

#define OffsetPitchByDefault -45
float ZPosSensetive = 0.009;

DLLEXPORT DWORD __stdcall GetControllersData(__out TController *FirstController, __out TController *SecondController)
{
	SteamVRHeadsetWindow = FindWindow(NULL, "Headset Window");
	HalfLifeAlyxWindow = FindWindow(NULL, "Half-Life: Alyx");

	if ((GetAsyncKeyState(VK_F1) & 0x8000) != 0 || (GetAsyncKeyState(VK_F7) & 0x8000) != 0)
		SetForegroundWindow(SteamVRHeadsetWindow);

	//if ((GetAsyncKeyState(VK_F7) & 0x8000) != 0)
	HeadsetWindowFocused = SteamVRHeadsetWindow != 0 && SteamVRHeadsetWindow == GetForegroundWindow() && IsWindowVisible(SteamVRHeadsetWindow);
	HalfLifeAlyxFocused = HalfLifeAlyxWindow != 0 && HalfLifeAlyxWindow == GetForegroundWindow() && IsWindowVisible(HalfLifeAlyxWindow);

	if (HeadsetWindowFocused || HalfLifeAlyxFocused)
		MouseToYawPitch();

	//Controllers default state 
	FirstController->AxisX = 0;
	FirstController->AxisY = 0;
	FirstController->Buttons = 0;
	FirstController->Trigger = 0;

	SecondController->AxisX = 0;
	SecondController->AxisY = 0;
	SecondController->Buttons = 0;
	SecondController->Trigger = 0;

	SecondCtrlYPROffset[0] = 0;
	SecondCtrlYPROffset[1] = 0;
	SecondCtrlYPROffset[2] = 0;

	if (HeadsetWindowFocused || HalfLifeAlyxFocused) //Don't press buttons when window is not focused / Не нажимать кнопки когда окно не в фокусе
	{
		if ((GetAsyncKeyState(KEY_ID_ROTATION_CONTROLLERS_UP) & 0x8000) != 0) CtrlsPitch -= StepRot; //U
		if ((GetAsyncKeyState(KEY_ID_ROTATION_CONTROLLERS_DOWN) & 0x8000) != 0) CtrlsPitch += StepRot; //J

		if (CtrlsPitch < -180)
			CtrlsPitch += 360;
		else if (CtrlsPitch > 180)
			CtrlsPitch -= 360;

		if ((GetAsyncKeyState(KEY_ID_ROTATION_CONTROLLERS_LEFT) & 0x8000) != 0) CtrlsRoll += StepRot; //H
		if ((GetAsyncKeyState(KEY_ID_ROTATION_CONTROLLERS_RIGHT) & 0x8000) != 0) CtrlsRoll -= StepRot; //K

		if (CtrlsRoll < -180)
			CtrlsRoll += 360;
		else if (CtrlsRoll > 180)
			CtrlsRoll -= 360;

		if ((GetAsyncKeyState(KEY_ID_ROTATION_CONTROLLERS_RESET) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_CHANGE_WEAPON) & 0x8000) != 0) { //Y
			CtrlsRoll = 0;
			CtrlsPitch = 0;
		}
	}

	//Default
	LeftHandYaw = 0;
	LeftHandPitch = 0;
	LeftAngleDist = 0;
	LeftRadDist = 0;

	RightHandYaw = 0;
	RightHandPitch = 0;
	RightAngleDist = 0;
	RightRadDist = 0;

	//Gestures / Жесты
	//First aid / Шприц
	if ((GetAsyncKeyState(KEY_ID_FIRST_AID_SYRINGE) & 0x8000) != 0) //"\" 
	{
		LeftHandYaw = -85;
		LeftAngleDist = 13;
		LeftRadDist = 0.2;
		FirstController->Buttons |= MENU_BTN;
	}

	//Left hand HUD / Информация на левой руке
	if ((GetAsyncKeyState(KEY_ID_HEALTH_AND_AMMO_DISPLAY) & 0x8000) != 0)
	{
		LeftHandYaw = -120; //-120
		LeftHandPitch = -160; //предыдущее -150
		LeftAngleDist = 5; //предыдущее 4
		LeftRadDist = 0.1;
	}

	//Left cell for items / Левая ячейка для предметов
	if ((GetAsyncKeyState(KEY_ID_LEFT_HAND_CELL) & 0x8000) != 0)
	{
		RightHandYaw = -45;
		RightRadDist = 0.17; //0.15
		RightAngleDist = -39.5;
	}

	//Crouch / Присесть
	if ((GetAsyncKeyState(KEY_ID_CROUCH) & 0x8000) != 0) {
		HMDPosZ = 0;
		CrouchOffsetZ = 0.8;
	} else
		CrouchOffsetZ = 0;

	//Backpack / Рюкзак (PART 1)
	if ((GetAsyncKeyState(KEY_ID_BACKPACK) & 0x8000) != 0) //"?" 
	{
		LeftHandYaw = -15;
		LeftAngleDist = -120;
		LeftRadDist = 0.22;
		LeftHandPitch = 50;
	}

	//Granades / Гранаты
	if (UseGranade) {
		if (GranadeTick < 15) {
			GranadeTick++;
			RightAngleDist = 40;
			LeftAngleDist = 17;
		}
		else {
			GranadeTick = 0;
			UseGranade = false;
		}
	}

	//Throw / Бросок
	if ((GetAsyncKeyState(KEY_ID_THROW_ITEMS) & 0x8000) != 0) //186 is ";"
	{
		LeftAngleDist = -120;
		RightAngleDist = 40;
		LeftRadDist = 0.25;
		LeftHandPitch = -75;
		FirstController->Buttons |= MENU_BTN;
		FirstController->Trigger = 1;
		UseGranade = true;
		GranadeTick = 0;
	}

	//Energy ball (Part 1)
	if (UsedEnergyBall)
	{
		LeftAngleDist = -40;
		LeftRadDist = 0.17;
		UsedEnergyBall = false;
	}
	if ((GetAsyncKeyState(KEY_ID_THROW_ENERGY_BALL) & 0x8000) != 0)
	{
		LeftAngleDist = -70;
		LeftRadDist = 0.17;
		UsedEnergyBall = true;
		FirstController->Trigger = 1;
	}

	//Cover mouth / Прикрыть рот
	if ((GetAsyncKeyState(KEY_ID_COVER_MOUTH) & 0x8000) != 0) CoverMouth = true;
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_CHANGE_WEAPON) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_AIMING) & 0x8000) != 0) CoverMouth = false;
	if (CoverMouth)
	{
		LeftHandYaw = 90;
		LeftAngleDist = 4;
		LeftRadDist = 0.34;
	}

	//Motion left controller on right click mode
	deltaPitch = mouseHMD.Yaw - oldPitch;
	oldPitch = mouseHMD.Yaw;

	deltaRoll = mouseHMD.Pitch - oldRoll;
	oldRoll = mouseHMD.Pitch;
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) != 0)
	{
		MotionLeftRadDist -= deltaRoll * 0.01;
		MotionLeftAngleDist -= deltaPitch;
		if (LeftAngleDist < -180)
			LeftAngleDist = -180;
		if (LeftAngleDist > 180)
			LeftAngleDist = 180;
		if ((GetAsyncKeyState(KEY_ID_MOTION_MODE_MOVE_DOWN) & 0x8000) != 0)
			LeftYOffset -= 0.003;
		if ((GetAsyncKeyState(KEY_ID_MOTION_MODE_MOVE_UP) & 0x8000) != 0)
			LeftYOffset += 0.003;
		LeftHandYaw = 25;
		//if (KEY_ID_LEFT_CTRL_MOTION == VK_RBUTTON) //Нажимаем триггер на левую мышку только если motion режим на правой кнопке, как бы переключение контроллеров
		if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0) FirstController->Trigger = 1;
		SecondCtrlYPROffset[1] = 20; 
		LeftRadDist = MotionLeftRadDist;
		LeftAngleDist = MotionLeftAngleDist;
	}
	else
	{
		MotionLeftRadDist = 0;
		MotionLeftAngleDist = 0;
		LeftYOffset = 0;
	}

	//Movement controllers
	if ((GetAsyncKeyState(KEY_ID_MOVE_CONTROLLERS_FORWARD) & 0x8000) != 0) HandRadDist += 0.005;
	if ((GetAsyncKeyState(KEY_ID_MOVE_CONTROLLERS_BACK) & 0x8000) != 0) HandRadDist -= 0.005;
	if ((GetAsyncKeyState(KEY_ID_MOVE_CONTROLLERS_FORWARD) & 0x8000) == 0 && (GetAsyncKeyState(KEY_ID_MOVE_CONTROLLERS_BACK) & 0x8000) == 0)
		HandRadDist = 0;

	FirstCtrlPos[0] = (0.4 - LeftRadDist + HandRadDist) * sin(DegToRad(OffsetYPR(OffsetYPR(mouseHMD.Yaw + 180, -17), LeftAngleDist))); //Первый OffsetYPR это расстояние между контроллерами, а второй жесты
	FirstCtrlPos[1] = (0.4 - LeftRadDist + HandRadDist) * cos(DegToRad(OffsetYPR(OffsetYPR(mouseHMD.Yaw + 180, -17), LeftAngleDist)));
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) == 0)
		FirstCtrlPos[2] = mouseHMD.Pitch * ZPosSensetive - CrouchOffsetZ + HMDPosZ;

	//Height coordinates for gestures / Координаты высоты для жестов
	//Backpack / Рюкзак (PART 2)
	if ((GetAsyncKeyState(KEY_ID_BACKPACK) & 0x8000) != 0) //"?"  
	{
		FirstCtrlPos[2] = 0.15 - CrouchOffsetZ + HMDPosZ; //0.1 - just above head / чуть выше головы
	}

	//Granade first state / Первая позиция гранаты (PART 2)
	if ((GetAsyncKeyState(KEY_ID_THROW_ITEMS) & 0x8000) != 0)
	{
		FirstCtrlPos[2] = FirstCtrlPos[2] - 0.15;
	}

	if (CoverMouth) //PART 2
	{
		FirstCtrlPos[2] = 0.12 - CrouchOffsetZ + HMDPosZ;
	}

	FirstController->X = FirstCtrlPos[0];
	FirstController->Y = FirstCtrlPos[1];
	FirstController->Z = FirstCtrlPos[2] + LeftYOffset - 0.15;

	if (LaserMode)
		ZPosSensetive = 0.006;
	else
		ZPosSensetive = 0.009;

	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) == 0)
	{
		SecondCtrlPos[0] = (0.4 - RightRadDist + HandRadDist) * sin(DegToRad(OffsetYPR( OffsetYPR(mouseHMD.Yaw + 180, 17), RightAngleDist) ));
		SecondCtrlPos[1] = (0.4 - RightRadDist + HandRadDist) * cos(DegToRad(OffsetYPR( OffsetYPR(mouseHMD.Yaw + 180, 17), RightAngleDist) ));
		SecondCtrlPos[2] = mouseHMD.Pitch * ZPosSensetive - CrouchOffsetZ + HMDPosZ;
	}

	SecondController->X = SecondCtrlPos[0];
	SecondController->Y = SecondCtrlPos[1];
	SecondController->Z = SecondCtrlPos[2] - 0.15;

	//Rotation controllers
	FirstController->Roll = CtrlsRoll;
	FirstController->Yaw = OffsetYPR(mouseHMD.Yaw, LeftHandYaw);
	FirstController->Pitch = OffsetYPR(OffsetYPR(OffsetYPR(mouseHMD.Pitch, OffsetPitchByDefault), LeftHandPitch), CtrlsPitch);

	//Backpack / Рюкзак (PART 3)
	if ((GetAsyncKeyState(191) & 0x8000) != 0) { 
		FirstController->Roll = 0;
		FirstController->Pitch = -175;
	}

	if (CoverMouth) //Прикрытие рта (PART 3)
		FirstController->Pitch = 45;

	//Energy ball 3
	if ((GetAsyncKeyState(KEY_ID_THROW_ENERGY_BALL) & 0x8000) != 0)
		FirstController->Pitch = -175;
	if (UsedEnergyBall)
		FirstController->Pitch = 160;

	SecondController->Yaw = OffsetYPR( OffsetYPR(mouseHMD.Yaw, SecondCtrlYPROffset[0]), RightHandYaw);
	SecondController->Pitch = OffsetYPR( OffsetYPR(OffsetYPR(OffsetYPR(mouseHMD.Pitch, OffsetPitchByDefault), CtrlsPitch), SecondCtrlYPROffset[1]), RightHandPitch);
	SecondController->Roll = CtrlsRoll;

	//Right hand mode
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) == 0) {

		//SecondController->Roll = OffsetYPR(CtrlYaw, SecondCtrlYPROffset[2]);
		
		SecondCtrlLastYPR[0] = SecondController->Yaw;
		SecondCtrlLastYPR[1] = SecondController->Pitch;
		SecondCtrlLastYPR[2] = SecondController->Roll;
	} else {
		SecondController->Yaw = OffsetYPR(SecondCtrlLastYPR[0], SecondCtrlYPROffset[0]);
		SecondController->Pitch = OffsetYPR(SecondCtrlLastYPR[1], SecondCtrlYPROffset[1]);
		SecondController->Roll = OffsetYPR(SecondCtrlLastYPR[2], SecondCtrlYPROffset[2]);
	}

	//Fix left controller (chapter 3: shotgun, chapter 5: Northern Star)
	if ((GetAsyncKeyState(KEY_ID_FIXING_LEFT_CONTROLLER) & 0x8000) != 0) { //Key "
		FirstController->Yaw = FirstCtrlLastYPR[0];
		FirstController->Pitch = FirstCtrlLastYPR[1];
		FirstController->Roll = FirstCtrlLastYPR[2];

		FirstController->X = FirstCtrlLastXYZ[0];
		FirstController->Y = FirstCtrlLastXYZ[1];
		FirstController->Z = FirstCtrlLastXYZ[2];
	}
	else {
		//Save last rotation and position for fixing left controller
		FirstCtrlLastYPR[0] = FirstController->Yaw;
		FirstCtrlLastYPR[1] = FirstController->Pitch;
		FirstCtrlLastYPR[2] = FirstController->Roll;

		FirstCtrlLastXYZ[0] = FirstController->X;
		FirstCtrlLastXYZ[1] = FirstController->Y;
		FirstCtrlLastXYZ[2] = FirstController->Z;
	}
	
	//Aiming mode
	if ((GetAsyncKeyState(KEY_ID_AIMING_MODE) & 0x8000) != 0) Aiming = true;
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_CHANGE_WEAPON) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_AIMING) & 0x8000) != 0) Aiming = false;
	if (Aiming || (GetAsyncKeyState(KEY_ID_AIMING) & 0x8000) != 0) {
		double a = DegToRad(OffsetYPR(mouseHMD.Yaw, -97) * -1); //-98
		if (VRMode)
			a = DegToRad(OffsetYPR(mouseHMD.Yaw, -91) * -1);

		double b = DegToRad(OffsetYPR(mouseHMD.Pitch, 90) * -1);

		SecondController->X = 0.30 * cos(a) * sin(b);
		SecondController->Y = 0.30 * sin(a) * sin(b);
		SecondController->Z = 0.30 * cos(b * -1) - 0.1 - CrouchOffsetZ + HMDPosZ;

		SecondController->Yaw = mouseHMD.Yaw;
		if (VRMode)
			SecondController->Yaw = OffsetYPR(SecondController->Yaw, -6);
		SecondController->Pitch = OffsetYPR(SecondCtrlLastYPR[1] * 1.1, 5);
		if (VRMode)
			SecondController->Pitch = OffsetYPR(SecondController->Pitch, 2);

		//First hand under weapon / Первая рука под оружием
		FirstController->X = 0.22 * cos(a) * sin(b); //0.18
		FirstController->Y = 0.22 * sin(a) * sin(b); //0.18
		FirstController->Z = SecondController->Z - 0.10;
	}

	//Lasers mode
	if ((GetAsyncKeyState(KEY_ID_LASER_MODE) & 0x8000) != 0) LaserMode = true;
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_CHANGE_WEAPON) & 0x8000) != 0 || (GetAsyncKeyState(KEY_ID_AIMING) & 0x8000) != 0) LaserMode = false;
	if (LaserMode) {
		double a = DegToRad(OffsetYPR(mouseHMD.Yaw, -97) * -1); //-98
		if (VRMode)
			a = DegToRad(OffsetYPR(mouseHMD.Yaw, -91) * -1);
		double b = DegToRad(OffsetYPR(mouseHMD.Pitch, 90) * -1);

		if ((GetAsyncKeyState(KEY_ID_UP) & 0x8000) != 0) LaserPos += 0.0013;
		if ((GetAsyncKeyState(KEY_ID_DOWN) & 0x8000) != 0) LaserPos -= 0.0013;
		if ((GetAsyncKeyState(KEY_ID_LEFT) & 0x8000) != 0) LaserYaw += 2;
		if ((GetAsyncKeyState(KEY_ID_RIGHT) & 0x8000) != 0) LaserYaw -= 2;
		if ((GetAsyncKeyState(KEY_ID_TURN_LEFT) & 0x8000) != 0) LaserYaw += 2;
		if ((GetAsyncKeyState(KEY_ID_TURN_RIGHT) & 0x8000) != 0) LaserYaw -= 2;
		if (LaserYaw < -180)
			LaserYaw += 360;
		else if (LaserYaw > 180)
			LaserYaw -= 360;

		SecondController->X = LaserPos * cos(a) * sin(b);
		SecondController->Y = LaserPos * sin(a) * sin(b);

		SecondController->Yaw = OffsetYPR( OffsetYPR(mouseHMD.Yaw, 0), LaserYaw);
		if (VRMode)
			SecondController->Yaw = OffsetYPR(SecondController->Yaw, -6);
		SecondController->Pitch = OffsetYPR(SecondCtrlLastYPR[1] * 1.1, 5);

		//First hand under weapon / Первая рука под оружием
		FirstController->X = 0.05 * cos(a) * sin(b);
		FirstController->Y = 0.05 * sin(a) * sin(b);
		FirstController->Z = 0.05 * cos(b * -1) - 0.3 - CrouchOffsetZ + HMDPosZ;
	}
	else {
		LaserPos = 0.3;
		LaserYaw = 0;
	}

	//Buttons
	if ((GetAsyncKeyState(KEY_ID_LEFT_GRIP) & 0x8000) != 0) FirstController->Buttons |= GRIP_BTN;
	if ((GetAsyncKeyState(KEY_ID_LEFT_MENU) & 0x8000) != 0) FirstController->Buttons |= MENU_BTN;
	if (UseGranade == false) //Гранаты
		if ((GetAsyncKeyState(KEY_ID_LEFT_TRIGGER) & 0x8000) != 0) FirstController->Trigger = 1;
	if ((GetAsyncKeyState(KEY_ID_LEFT_SYSTEM) & 0x8000) != 0) FirstController->Buttons |= SYS_BTN;

	//Movement
	if (LaserMode == false) //Don't move in laser mode / Не двигаемся в режиме лазера
	{
		if ((GetAsyncKeyState(KEY_ID_UP) & 0x8000) != 0) { FirstController->AxisY = 1; FirstController->Buttons |= THUMB_BTN; }
		if ((GetAsyncKeyState(KEY_ID_DOWN) & 0x8000) != 0) { FirstController->AxisY = -1; FirstController->Buttons |= THUMB_BTN; }
		if ((GetAsyncKeyState(KEY_ID_LEFT) & 0x8000) != 0) { FirstController->AxisX = -1; FirstController->Buttons |= THUMB_BTN; }
		if ((GetAsyncKeyState(KEY_ID_RIGHT) & 0x8000) != 0) { FirstController->AxisX = 1; FirstController->Buttons |= THUMB_BTN; }

		if ((GetAsyncKeyState(KEY_ID_TURN_LEFT) & 0x8000) != 0) { SecondController->AxisX = -1; SecondController->Buttons |= THUMB_BTN; }
		if ((GetAsyncKeyState(KEY_ID_TURN_RIGHT) & 0x8000) != 0) { SecondController->AxisX = 1; SecondController->Buttons |= THUMB_BTN; }
	}

	if ((GetAsyncKeyState(KEY_ID_RIGHT_MENU) & 0x8000) != 0) SecondController->Buttons |= MENU_BTN;
	if ((GetAsyncKeyState(KEY_ID_RIGHT_GRIP) & 0x8000) != 0) SecondController->Buttons |= GRIP_BTN;
	if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER2) & 0x8000) != 0) SecondController->Trigger = 1;

	//Teleport
	if ((GetAsyncKeyState(KEY_ID_TELEPORT) & 0x8000) != 0) {
		SecondController->AxisY = -1;
		SecondController->Buttons |= THUMB_BTN;
	}
	//Select weapon
	if ((GetAsyncKeyState(KEY_ID_LEFT_CTRL_MOTION) & 0x8000) == 0) { //Only in right controller mode
		if ((GetAsyncKeyState(KEY_ID_CHANGE_WEAPON) & 0x8000) != 0) {
			SecondController->AxisY = 1;
			SecondController->Buttons |= THUMB_BTN;
		}
		if ((GetAsyncKeyState(KEY_ID_RIGHT_TRIGGER) & 0x8000) != 0) SecondController->Trigger = 1;
	}
	
	if (HeadsetWindowFocused == false && HalfLifeAlyxFocused == false) //Don't press buttons when window is not focused / Не нажимать кнопки когда окно не в фокусе
	{
		FirstController->Buttons = 0;
		FirstController->Trigger = 0;
		FirstController->AxisX = 0;
		FirstController->AxisY = 0;
		SecondController->Buttons = 0;
		SecondController->Trigger = 0;
		SecondController->AxisX = 0;
		SecondController->AxisY = 0;
	}

	//New version of SteamVR driver with correct axes / Новая версия SteamVR драйвер, с правильными осями
	FirstController->Yaw = FirstController->Yaw * -1;
	FirstController->Pitch = FirstController->Pitch * -1;

	SecondController->Yaw = SecondController->Yaw * -1;
	SecondController->Pitch = SecondController->Pitch * -1;

	return TOVR_SUCCESS;
}

DLLEXPORT DWORD __stdcall SetControllerData(__in int dwIndex, __in unsigned char MotorSpeed)
{
	return TOVR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
		{
			//Key bindings
			CRegKey key;
			TCHAR driversPath[MAX_PATH];
			LONG status = key.Open(HKEY_CURRENT_USER, "Software\\TrueOpenVR");
			if (status == ERROR_SUCCESS)
			{
				ULONG regSize = sizeof(driversPath);
				status = key.QueryStringValue("Drivers", driversPath, &regSize);
			}
			key.Close();

			TCHAR configPath[MAX_PATH] = { 0 };
			_tcscat_s(configPath, sizeof(configPath), driversPath);
			_tcscat_s(configPath, sizeof(configPath), "HalfLifeAlyx.ini");

			if (status == ERROR_SUCCESS) { //&& PathFileExists(configPath)
				CIniReader IniFile((char *)configPath);

				KEY_ID_LEFT_TRIGGER = IniFile.ReadInteger("Keys", "LEFT_TRIGGER", VK_RSHIFT);
				KEY_ID_LEFT_MENU = IniFile.ReadInteger("Keys", "LEFT_MENU", VK_RETURN);
				KEY_ID_LEFT_GRIP = IniFile.ReadInteger("Keys", "LEFT_GRIP", VK_ESCAPE);
				KEY_ID_LEFT_SYSTEM = IniFile.ReadInteger("Keys", "LEFT_SYSTEM", VK_F9);
				KEY_ID_RIGHT_TRIGGER = IniFile.ReadInteger("Keys", "RIGHT_TRIGGER", VK_LBUTTON);
				KEY_ID_RIGHT_TRIGGER2 = IniFile.ReadInteger("Keys", "RIGHT_TRIGGER2", VK_NUMPAD1);
				KEY_ID_RIGHT_MENU = IniFile.ReadInteger("Keys", "RIGHT_MENU", VK_DECIMAL);
				KEY_ID_RIGHT_GRIP = IniFile.ReadInteger("Keys", "RIGHT_GRIP", VK_NUMPAD0);
				KEY_ID_CHANGE_WEAPON = IniFile.ReadInteger("Keys", "CHANGE_WEAPON", VK_MBUTTON);
				KEY_ID_TELEPORT = IniFile.ReadInteger("Keys", "TELEPORT", VK_SPACE);
				KEY_ID_BACKPACK = IniFile.ReadInteger("Keys", "BACKPACK", 191); //?
				KEY_ID_FIRST_AID_SYRINGE = IniFile.ReadInteger("Keys", "FIRST_AID_SYRINGE", 220);
				KEY_ID_FIXING_LEFT_CONTROLLER = IniFile.ReadInteger("Keys", "FIXING_LEFT_CONTROLLER", 222);
				KEY_ID_THROW_ITEMS = IniFile.ReadInteger("Keys", "THROW_ITEMS", 186); //";"
				KEY_ID_HEALTH_AND_AMMO_DISPLAY = IniFile.ReadInteger("Keys", "HEALTH_AND_AMMO_DISPLAY", VK_BACK);
				KEY_ID_MOVE_CONTROLLERS_FORWARD = IniFile.ReadInteger("Keys", "MOVE_CONTROLLERS_FORWARD", VK_INSERT);
				KEY_ID_MOVE_CONTROLLERS_BACK = IniFile.ReadInteger("Keys", "MOVE_CONTROLLERS_BACK", VK_HOME);
				KEY_ID_PLAYER_RISE_HIGHER = IniFile.ReadInteger("Keys", "PLAYER_RISE_HIGHER", VK_PRIOR);
				KEY_ID_PLAYER_RISE_LOWER = IniFile.ReadInteger("Keys", "PLAYER_RISE_LOWER", VK_NEXT);
				KEY_ID_PLAYER_RISE_RESET = IniFile.ReadInteger("Keys", "PLAYER_RISE_RESET", 189); //-
				KEY_ID_CROUCH = IniFile.ReadInteger("Keys", "CROUCH", VK_RCONTROL);

				KEY_ID_COVER_MOUTH = IniFile.ReadInteger("Keys", "COVER_MOUTH", 'P');
				KEY_ID_LASER_MODE = IniFile.ReadInteger("Keys", "LASER_MODE", 'L');
				KEY_ID_THROW_ENERGY_BALL = IniFile.ReadInteger("Keys", "THROW_ENERGY_BALL", VK_END);
				KEY_ID_AIMING_MODE = IniFile.ReadInteger("Keys", "AIMING_MODE", VK_DELETE);
				KEY_ID_AIMING = IniFile.ReadInteger("Keys", "AIMING", VK_XBUTTON1);
				KEY_ID_LEFT_HAND_CELL = IniFile.ReadInteger("Keys", "LEFT_HAND_CELL", 'O');
				KEY_ID_LEFT_CTRL_MOTION = IniFile.ReadInteger("Keys", "LEFT_CTRL_MOTION", VK_RBUTTON);

				KEY_ID_ROTATION_CONTROLLERS_UP = IniFile.ReadInteger("Keys", "ROTATION_CONTROLLERS_UP", 'U');
				KEY_ID_ROTATION_CONTROLLERS_DOWN = IniFile.ReadInteger("Keys", "ROTATION_CONTROLLERS_DOWN", 'J');
				KEY_ID_ROTATION_CONTROLLERS_LEFT = IniFile.ReadInteger("Keys", "ROTATION_CONTROLLERS_LEFT", 'H');
				KEY_ID_ROTATION_CONTROLLERS_RIGHT = IniFile.ReadInteger("Keys", "ROTATION_CONTROLLERS_RIGHT", 'K');
				KEY_ID_ROTATION_CONTROLLERS_RESET = IniFile.ReadInteger("Keys", "ROTATION_CONTROLLERS_RESET", 'Y');

				KEY_ID_MOTION_MODE_MOVE_UP = IniFile.ReadInteger("Keys", "MOTION_MODE_MOVE_UP", 219);
				KEY_ID_MOTION_MODE_MOVE_DOWN = IniFile.ReadInteger("Keys", "MOTION_MODE_MOVE_DOWN", 221);

				KEY_ID_TURN_LEFT = IniFile.ReadInteger("Keys", "TURN_LEFT", 'N');
				KEY_ID_TURN_RIGHT = IniFile.ReadInteger("Keys", "TURN_RIGHT", 'M');

				KEY_ID_UP = IniFile.ReadInteger("Keys", "UP", VK_UP);
				KEY_ID_DOWN = IniFile.ReadInteger("Keys", "DOWN", VK_DOWN);
				KEY_ID_LEFT = IniFile.ReadInteger("Keys", "LEFT", VK_LEFT);
				KEY_ID_RIGHT = IniFile.ReadInteger("Keys", "RIGHT", VK_RIGHT);

				mouseSensetiveYaw = IniFile.ReadFloat("Main", "AxisX", 0.05);
				mouseSensetivePitch = IniFile.ReadFloat("Main", "AxisY", 0.04);
				
				ZPosSensetive = IniFile.ReadFloat("Main", "AxisZ", 0.009);

				VRMode = IniFile.ReadBoolean("Main", "VRMode", false);
			}
			
			m_HalfWidth = GetSystemMetrics(SM_CXSCREEN) / 2;
			m_HalfHeight = GetSystemMetrics(SM_CYSCREEN) / 2;
			break;
		}
	}

	return TRUE;
}