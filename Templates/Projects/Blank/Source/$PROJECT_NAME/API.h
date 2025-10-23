#pragma once

#ifdef PRIMARY_GAME_MODULE
#define ${PROJECT_NAME_UPPER}_API __declspec(dllexport)
#else
#define ${PROJECT_NAME_UPPER}_API __declspec(dllimport)
#endif