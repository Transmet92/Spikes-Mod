/*
	TRANSMET 2017

	GTA V Script - SPIKES MOD 3.0

	https://los-santos-multiplayer.com
*/


#include "script.h"
#include "keyboard.h"
#include <string>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <list>
#include <tmmintrin.h>

using namespace std;

#define VEH_POOL_SIZE 2048

bool inline get_key_pressed(int nVirtKey) { return (GetAsyncKeyState(nVirtKey) & 0x8000) != 0; }
void draw_text_shadow(char* caption, float x, float y, int text_col[4], int font = 0, float text_scale = 0.35)
{
	UI::SET_TEXT_FONT(font);
	UI::SET_TEXT_SCALE(0.0, text_scale);
	UI::SET_TEXT_COLOUR(text_col[0], text_col[1], text_col[2], text_col[3]);
	UI::SET_TEXT_CENTRE(0);
	UI::SET_TEXT_DROPSHADOW(5, 0, 0, 0, 255);
	UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
	UI::_SET_TEXT_ENTRY("STRING");
	UI::_ADD_TEXT_COMPONENT_STRING(caption);
	UI::_DRAW_TEXT(x, y);

	UI::SET_TEXT_FONT(font);
	UI::SET_TEXT_SCALE(0.0, text_scale);
	UI::SET_TEXT_COLOUR(text_col[0], text_col[1], text_col[2], text_col[3]);
	UI::SET_TEXT_CENTRE(0);
	UI::SET_TEXT_DROPSHADOW(5, 0, 0, 0, 255);
	UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
	UI::_SET_TEXT_GXT_ENTRY("STRING");
	UI::_ADD_TEXT_COMPONENT_STRING(caption);

	UI::_0x9040DFB09BE75706(x, y);
}

struct Spike {
	int handle;
	int blip;
	Vector3 pos;
};

struct Tyre {
	int id;
	const char* name;
};

static const unsigned char tyresCount = 6;
static const Tyre tyres[] = {
	{ 0, "wheel_lf" },
	{ 1, "wheel_rf" },
	{ 2, "wheel_lm" },
	{ 3, "wheel_rm" },
	{ 4, "wheel_lr" },
	{ 5, "wheel_rr" },
};

// YES ... It's not good VDIST ( but it's faster and more precise for this mod )
static inline float __VDIST(Vector3 &One, Vector3 &Two) {
	return fabsf(One.x - Two.x) + fabsf(One.y - Two.y) + fabsf(One.z - Two.z);
}

static bool equality(char* buf, const char* determ, int sizeBuf, int sizeDeterm) {
	bool bypass = true;
	for (unsigned short it = 0; it < sizeDeterm; it++) {
		if (buf[it] != determ[it])
			bypass = false;
	}
	return bypass;
}

static int parseData(char* buf, const char* pattern, int limit, int size, bool boolean) {
	if (equality(buf, pattern, limit, size) == true)
	{
		buf += size;
		char* back = buf;
		for (; *buf != ';'; buf++) continue;
		*buf = 0x00;
		return (boolean ? (*back == '1' ? -1 : -2) : strtol(back, NULL, 16));
	}
	return 0;
}

void ScriptMain() // EntryPoint
{
	int DEPLOY_KEY = VK_ADD;
	int CLEAR_KEY = VK_SUBTRACT;
	int TOGGLE_CAM_KEY = 0x4F;
	// int TOGGLE_ME_KEY = 0x4B;
	bool USE_CONTROLLER = false;

	char buffer[512];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	unsigned short lastRN = 0;
	for (unsigned short i = 0; i < 512; i++)
		if (buffer[i] == '\\')
		{
			buffer[i] = '/';
			lastRN = i + 1;
		}
	buffer[lastRN] = 0x0;
	strcat_s(buffer, "Spikes.ini");

	FILE* fstr = nullptr;
	if (fopen_s(&fstr, buffer, "r") == 0)
	{
		fseek(fstr, 0, SEEK_SET);
		size_t read = fread(buffer, 1, 512, fstr);

		for (unsigned short i = 0; i < read; i++)
		{
			int cur = parseData(buffer + i, "DEPLOY=0x", (int)read, 9, false);
			if (cur != 0x00) DEPLOY_KEY = cur;

			cur = parseData(buffer + i, "CLEAR=0x", (int)read, 8, false);
			if (cur != 0x00) CLEAR_KEY = cur;

			cur = parseData(buffer + i, "TOGGLE_CAM=0x", (int)read, 13, false);
			if (cur != 0x00) TOGGLE_CAM_KEY = cur;

			cur = parseData(buffer + i, "USE_CONTROLLER=", (int)read, 15, true);
			if (cur != 0x00) USE_CONTROLLER = cur == -1 ? true : false;

			// cur = parseData(buffer + i, "TOGGLE_ME=0x", read, 12);
			// if (cur != 0x00) TOGGLE_ME_KEY = cur;
		}
		fclose(fstr);
	}


	int textColor[4] = { 255, 255, 255, 255 };
	bool useCam = true;
	// bool includeMe = false;

	vector<Spike> spikes = vector<Spike>();
	unsigned short spikesCount = 0;
	int lastSpike = 0;

	Hash herseHash = GAMEPLAY::GET_HASH_KEY("p_ld_stinger_s");

	unsigned char step = 0;
	unsigned int TickStep = 0;

	char victimesText[64];
	int victimCount = 0;
	bool showVictims = false;
	
	Cam camScripted = CAM::CREATE_CAM("DEFAULT_SCRIPTED_CAMERA", 1);
	CAM::SET_CAM_PARAMS(camScripted, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 30.0f, 3000, 3, 3, 2);

	unsigned int pressTick = 0;

	int* PoolVehHeap = new int[VEH_POOL_SIZE]; // PREFER STACK FOR POOL NOT HEAVY
	unsigned int TickCheck = GetTickCount();
	for (;;)
	{
		int myPed = PLAYER::PLAYER_PED_ID();
		int myVeh = PED::GET_VEHICLE_PED_IS_USING(myPed);
		bool inVeh = PED::IS_PED_IN_ANY_VEHICLE(myPed, true) == 1;

		if (((USE_CONTROLLER &&
			CONTROLS::IS_CONTROL_PRESSED(2, 85) &&
			CONTROLS::IS_CONTROL_PRESSED(2, 258))
			|| get_key_pressed(DEPLOY_KEY)) &&
			GetTickCount() > pressTick && step == 0)
		{
			pressTick = GetTickCount() + 1000;

			showVictims = true;
			Vector3 modelPos;
			Vector3 modelRot;
			if (inVeh) {
				Vector3 bone = ENTITY::GET_WORLD_POSITION_OF_ENTITY_BONE(
					myVeh, ENTITY::GET_ENTITY_BONE_INDEX_BY_NAME(myVeh, "wheel_lr"));
				modelPos = ENTITY::GET_ENTITY_COORDS(myVeh, 1);

				if (GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(modelPos.x, modelPos.y, bone.z, &(modelPos.z), 0) == 0)
					modelPos.z = bone.z - 0.2f;
				
				modelRot = ENTITY::GET_ENTITY_ROTATION(myVeh, false);
			}
			else {
				modelPos = ENTITY::GET_ENTITY_COORDS(myPed, 1);
				Vector3 forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(myPed);
				modelPos.x += forward.x;
				modelPos.y += forward.y;

				if (GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(modelPos.x, modelPos.y, modelPos.z, &(modelPos.z), 0) == 0)
					modelPos.z -= 0.6f;

				modelRot = ENTITY::GET_ENTITY_ROTATION(myPed, false);
			}

			AUDIO::REQUEST_SCRIPT_AUDIO_BANK("BIG_SCORE_HIJACK_01", 0);
			STREAMING::REQUEST_MODEL(herseHash);
			STREAMING::REQUEST_ANIM_DICT("P_ld_stinger_s");
			while (!STREAMING::HAS_MODEL_LOADED(herseHash)) WAIT(0);
			while (!STREAMING::HAS_ANIM_DICT_LOADED("P_ld_stinger_s")) WAIT(0);

			int curSpike = OBJECT::CREATE_OBJECT(herseHash, modelPos.x, modelPos.y, modelPos.z, 1, 1, 1);
			int curBlip = UI::ADD_BLIP_FOR_COORD(modelPos.x, modelPos.y, modelPos.z);
			spikes.push_back({ curSpike, curBlip, modelPos });
			UI::SET_BLIP_COLOUR(curBlip, 3);
			UI::SET_BLIP_SCALE(curBlip, 0.7f);
			ENTITY::SET_ENTITY_ROTATION(curSpike, modelRot.x, modelRot.y, modelRot.z - 90.0f, 2, 1);

			if (useCam) {
				CAM::POINT_CAM_AT_COORD(camScripted, modelPos.x, modelPos.y, modelPos.z + 0.2f);

				CAM::SET_CAM_COORD(camScripted, modelPos.x - 6.0f, modelPos.y - 6.0f, modelPos.z + 1.5f);
				bool bypassAttach = true;
				for (unsigned short i = 0; i < worldGetAllVehicles(PoolVehHeap, VEH_POOL_SIZE); i++)
				{
					if (PoolVehHeap[i] != myVeh)
					{
						Vector3 curVehPos = ENTITY::GET_ENTITY_COORDS(PoolVehHeap[i], 1);
						if (__VDIST(curVehPos, modelPos) <= 25.0f)
						{
							Vector3 max, min;
							GAMEPLAY::GET_MODEL_DIMENSIONS(ENTITY::GET_ENTITY_MODEL(PoolVehHeap[i]), &min, &max);
							CAM::ATTACH_CAM_TO_ENTITY(camScripted, PoolVehHeap[i], 0.0f, 0.0f, max.z + 2.0f, 1);
							bypassAttach = false;
							break;
						}
					}
				}
				if (bypassAttach && inVeh)
				{
					Vector3 max, min;
					GAMEPLAY::GET_MODEL_DIMENSIONS(ENTITY::GET_ENTITY_MODEL(myVeh), &min, &max);
					CAM::ATTACH_CAM_TO_ENTITY(camScripted, myVeh, 0.0f, 0.0f, max.z + 2.0f, 1);
				}

				CAM::RENDER_SCRIPT_CAMS(1, 0, 0, 1, 1);
			}

			AUDIO::PLAY_SOUND_FROM_ENTITY(-1, "DROP_STINGER", curSpike, "BIG_SCORE_3A_SOUNDS", 0, 0);
			ENTITY::PLAY_ENTITY_ANIM(curSpike, "P_Stinger_S_deploy", "P_ld_stinger_s", 2000.0f, 0, 1, 0, 0.0f, 0);
			
			spikesCount++;
			step = 16;
			TickStep = GetTickCount() + 1200;
			lastSpike = curSpike;

		}

		if (((USE_CONTROLLER &&
			CONTROLS::IS_CONTROL_PRESSED(2, 74) &&
			CONTROLS::IS_CONTROL_PRESSED(2, 258))
			|| get_key_pressed(CLEAR_KEY)) &&
			GetTickCount() > pressTick)
		{
			pressTick = GetTickCount() + 1000;

			for (unsigned short i = 0; i < spikesCount; i++)
			{
				if (ENTITY::DOES_ENTITY_EXIST(spikes[i].handle))
					OBJECT::DELETE_OBJECT(&(spikes[i].handle));
				UI::REMOVE_BLIP(&(spikes[i].blip));
				victimCount = 0;
			}
			spikes.clear();
			spikesCount = 0;

			step = 64;
			TickStep = GetTickCount() + 1200;
		}


		if (get_key_pressed(TOGGLE_CAM_KEY) && GetTickCount() > pressTick)
		{
			useCam ? useCam = false : useCam = true;
			pressTick = GetTickCount() + 200;
		}
		/*else if (get_key_pressed(TOGGLE_ME_KEY) && GetTickCount() > pressTick)
		{
			includeMe ? includeMe = false : includeMe = true;
			pressTick = GetTickCount() + 200;
		}*/

		switch (step)
		{
		case 16:
			if (GetTickCount() > TickStep)
			{
				CAM::RENDER_SCRIPT_CAMS(0, 0, 0, 1, 1);

				if (useCam) {
					CAM::DETACH_CAM(camScripted);
					CAM::STOP_CAM_POINTING(camScripted);
				}
				TickStep = GetTickCount() + 1200;
				step = 32;
			}
			break;
		case 32:
		{
			if (TickStep > GetTickCount())
				draw_text_shadow("~w~Spikes ~g~deployed ~w~!", 0.47f, 0.95f, textColor, 1, 0.50f);
			else
			{
				ENTITY::FREEZE_ENTITY_POSITION(lastSpike, true);
				step = 0;
			}
		}
		break;
		case 64:
		{
			if (TickStep > GetTickCount())
				draw_text_shadow("~w~Spikes ~r~removed ~w~!", 0.47f, 0.95f, textColor, 1, 0.50f);
			else
				step = 0;
		}
		break;
		}

		if (spikesCount != 0)
		{
			if (GetTickCount() > TickCheck) // 17 Hz
			{
				for (unsigned short i = 0; i < worldGetAllVehicles(PoolVehHeap, VEH_POOL_SIZE); i++)
					if (PoolVehHeap[i] != myVeh) // (includeMe ? true : PoolVehStck[i] != myVeh)
					{
						int victimVeh = PoolVehHeap[i];

						Vector3 curVehPos = ENTITY::GET_ENTITY_COORDS(victimVeh, 1);
						for (unsigned short it = 0; it < spikesCount; it++)
						{
							Vector3 spikePos = spikes[it].pos;

							for (unsigned char it1 = 0; it1 < tyresCount; it1++)
							{
								Vector3 wheelPos = ENTITY::GET_WORLD_POSITION_OF_ENTITY_BONE(
									victimVeh,
									ENTITY::GET_ENTITY_BONE_INDEX_BY_NAME(victimVeh, (char*)tyres[it1].name));

								if (__VDIST(wheelPos, spikePos) <= 2.6f &&
									!VEHICLE::IS_VEHICLE_TYRE_BURST(victimVeh, tyres[it1].id, 0))
								{
									VEHICLE::SET_VEHICLE_TYRE_BURST(victimVeh, tyres[it1].id, 1, 1.0f);
									victimCount++;
								}
							}
						}
					}
				TickCheck = GetTickCount() + 58;
			}

			if (showVictims == true)
			{
				sprintf_s(victimesText, "Tires Burst : ~b~%d    ~w~%s", victimCount,
					useCam ? "~g~CAM" : "~r~CAM"); //, includeMe ? "~g~ME" : "~r~ME");
				draw_text_shadow(victimesText, 0.47f, 0.01f, textColor, 6, 0.5f);
			}
		}

		WAIT(0);
	}

	delete[] PoolVehHeap;
}