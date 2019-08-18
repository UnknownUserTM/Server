/*********************************************************************
* title_name		: Language System
* date_created		: 2017.10.21
* filename			: MultiLanguage.h
* author			: VegaS
* version_actual	: Version 0.1.0
*/
#ifndef __INC_MULTI_LANGUAGE_H_
#define __INC_MULTI_LANGUAGE_H_
#define LANGUAGE_FILTER_CHAT "language.active_%d"

struct tag_Quiz
{
	char Quiz[256];
	bool answer;
};

enum ELanguageInfoArgs
{
	LANGUAGE_SUB_HEADER_FILTER_CHAT = 0,
	LANGUAGE_SUB_HEADER_GM_ANNOUNCEMENT = 1,
	
	LANGUAGE_UNSELECTED = 0,
	LANGUAGE_SELECTED = 1,
	
	ANNOUNCEMENT_TYPE_NOTICE = 0,
	ANNOUNCEMENT_TYPE_BIG_NOTICE = 1,
	ANNOUNCEMENT_TYPE_MAP_NOTICE = 2,
	ANNOUNCEMENT_TYPE_WHISPER = 3,
	ANNOUNCEMENT_TYPE_MAX = 4,
};

class CLanguageManager : public singleton<CLanguageManager>
{
	public:
		CLanguageManager();
		~CLanguageManager();

		int32_t GetKeyInstanceVectorByLang(const char* pszLanguage);
		void ShoutFilter(LPCHARACTER ch, const char* c_pData);
		void SendLanguageNoticeMap(std::vector<std::vector<tag_Quiz> > m_vec_quiz_language, int32_t iNumberQuiz);
		bool GetVectorInstanceKey(std::vector<std::vector<tag_Quiz> > vector, int32_t keyA);
		const char * GetLanguageStringByIndex(BYTE bIndex);
		
		void SendAnnouncement(BYTE bType, int32_t iMapIndex, const char * szText, const char * szLanguage);
		void AnnouncementP2P(const char * c_pData);
};

enum eLocalization
{
	LC_NOSET = 0,
	LC_YMIR,
	LC_JAPAN,
	LC_ENGLISH,
	LC_HONGKONG,
	LC_NEWCIBN,
	LC_GERMANY,
	LC_KOREA,
	LC_FRANCE,
	LC_ITALY,
	LC_SPAIN,
	LC_UK,
	LC_TURKEY,
	LC_POLAND,
	LC_PORTUGAL,
	LC_CANADA,
	LC_BRAZIL,
	LC_GREEK,
	LC_RUSSIA,
	LC_DENMARK,
	LC_BULGARIA,
	LC_CROATIA,
	LC_MEXICO,
	LC_ARABIA,
	LC_CZECH,
	LC_ROMANIA,
	LC_HUNGARY,
	LC_NETHERLANDS,
	LC_SINGAPORE,
	LC_VIETNAM,
	LC_THAILAND,
	LC_USA,
	LC_WE_KOREA,
	LC_TAIWAN,
	LC_MAX_VALUE
};

const std::string& LocaleService_GetBasePath();
const std::string& LocaleService_GetMapPath();
const std::string& LocaleService_GetQuestPath();
void LocaleService_TransferDefaultSetting();

eLocalization LC_GetLocalType();

bool LC_IsLocale( const eLocalization t );
bool LC_IsYMIR();
bool LC_IsJapan();
bool LC_IsEnglish();
bool LC_IsHongKong();
bool LC_IsNewCIBN();
bool LC_IsGermany();
bool LC_IsKorea();
bool LC_IsEurope();
bool LC_IsCanada();
bool LC_IsBrazil();
bool LC_IsSingapore();
bool LC_IsVietnam();
bool LC_IsThailand();
bool LC_IsWE_Korea();
bool LC_IsTaiwan();
bool LC_IsWorldEdition();

#endif
