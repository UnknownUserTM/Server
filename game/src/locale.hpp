#ifndef __INC_METIN2_GAME_LOCALE_H__
#define __INC_METIN2_GAME_LOCALE_H__
#include <common.h>
extern "C"
{
	extern int g_iUseLocale;

	#define LC_TEXT(str) locale_find(str, g_stDefaultLanguage)
	#define LC_TEXT_CONVERT_LANGUAGE(language, str) locale_find(str, language)
};

#endif
