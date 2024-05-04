#pragma once
#include <Windows.h>
#include <stdio.h>
#include <ctime>
#pragma warning (disable : 26495)

#ifdef PROFILE
#define ProfileBegin(num)\
static bool isFirst##num = true;\
static int test_Index##num = profiler.CURRENT_PROFILE_SAMPLE_LENGTH;\
static char tagName##num[_countof(__FUNCDNAME__) + 1] = __FUNCDNAME__;\
	if(isFirst##num)\
	{\
		++profiler.CURRENT_PROFILE_SAMPLE_LENGTH;\
		tagName##num[_countof(__FUNCDNAME__) - 1] = #num[0];\
		tagName##num[_countof(__FUNCDNAME__)] = 0;\
		isFirst##num = false;\
	}\
PROFILE_REQUEST pr##num{test_Index##num,tagName##num};\
do{\
}while(0)\

constexpr DWORD kMICRO = 1000000;

struct PROFILE_SAMPLE
{
	char				szName[64];			// 프로파일 샘플 이름.
	static constexpr int MINMAX_COUNT = 2;
	LONGLONG iTotalTime;			// 전체 사용시간 카운터 Time.
	LONGLONG iMin[MINMAX_COUNT];			// 최소 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최소 [1] 다음 최소)
	LONGLONG iMax[MINMAX_COUNT];			// 최대 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최대 [1] 다음 최대)
	LONGLONG			iCall;				// 누적 호출 횟수.
	bool				isUsed = false;		// 해당 PROFILE_SAMPLE 객체가 최초로 사용되는것인지 여부 표시

	__forceinline PROFILE_SAMPLE()
		:iTotalTime{ 0 }, iCall{ 0 }, isUsed{ false }
	{
		iTotalTime = 0;
		for (int i = 0; i < MINMAX_COUNT; ++i)
		{
			iMax[i] = LLONG_MIN;
			iMin[i] = LLONG_MAX;
		}
	}
};


class PROFILER
{
public:
	static constexpr int PROFILE_SAMPLE_LENGTH = 100;
	int CURRENT_PROFILE_SAMPLE_LENGTH;
	PROFILE_SAMPLE _sampleArr[PROFILE_SAMPLE_LENGTH];
	LARGE_INTEGER _freq;
	PROFILER()
		:CURRENT_PROFILE_SAMPLE_LENGTH{ 0 }
	{
		QueryPerformanceFrequency(&_freq);
	}
};

PROFILER profiler;

class PROFILE_REQUEST
{
	LARGE_INTEGER _start;
	LARGE_INTEGER _end;
	int _test_index;

public:
	__forceinline PROFILE_REQUEST(const int test_index, char* tagName)
		:_test_index(test_index)
	{
		auto& temp = profiler._sampleArr[_test_index];
		if (!temp.isUsed)
		{
			strcpy_s(temp.szName, tagName);
			temp.isUsed = true;
		}
		QueryPerformanceCounter(&_start);
	}
	__forceinline ~PROFILE_REQUEST()
	{
		QueryPerformanceCounter(&_end);
		LONGLONG elapsed_time = _end.QuadPart - _start.QuadPart;
		auto& temp = profiler._sampleArr[_test_index];
		++temp.iCall;
		temp.iTotalTime += elapsed_time;

		LONGLONG swapping;

		if (temp.iMax[0] < elapsed_time)
		{
			temp.iMax[1] = temp.iMax[0];
			temp.iMax[0] = elapsed_time;
		}

		if (temp.iMin[0] > elapsed_time)
		{
			temp.iMin[1] = temp.iMin[0];
			temp.iMin[0] = elapsed_time;
		}
	}
};


#define PROFILlNG(num) ProfileBegin(num)
/*
PROFILE_SAMPLE_NUMBER는 profiling되는 함수의 갯수와 인덱스의 의미를 동시에 지닙니다.
따라서 최초호출시에 PROFILE_SAMPLE_NUMBER가 인덱스가 되고 해당변수를 1 증가시킨후
다음 호출부터는 증가되지 않습니다.
*/
__inline void ProfileReset(void)
{
	new (profiler._sampleArr)PROFILE_SAMPLE[profiler.CURRENT_PROFILE_SAMPLE_LENGTH];
}

__inline void tempToFileBuffer(char* tempBuffer,char** startPoint)
{
	size_t flag = strlen(tempBuffer) + 1;
	snprintf(*startPoint, flag, tempBuffer);
	*startPoint += (flag - 1);
}

__inline void ProfileDataOutText(void)
{
	constexpr int size = 200;
	char szFileName[MAX_PATH];
	__time64_t now;
	_time64(&now);

	tm LocalTime;
	_localtime64_s(&LocalTime, &now);
	sprintf_s(szFileName, "%4d%02d%02d%02d%02d - ProfileLog.txt", LocalTime.tm_year + 1900, LocalTime.tm_mon, LocalTime.tm_mday, LocalTime.tm_hour, LocalTime.tm_min);

	FILE* pFile;
	fopen_s(&pFile, szFileName, "a+");
	if (!pFile)
		throw;

	char* fileBuffer = new char[size * 150];
	fileBuffer[0] = 0;
	char tempBuffer[size] = { 0 };

	
	char* startPoint = fileBuffer;

	sprintf_s(tempBuffer, size,"%4d%02d%02d:%02d:%02d:%02d\n", LocalTime.tm_year + 1900, LocalTime.tm_mon, LocalTime.tm_mday, LocalTime.tm_hour, LocalTime.tm_min,LocalTime.tm_sec);
	tempToFileBuffer(tempBuffer, &startPoint);
	
	sprintf_s(tempBuffer, size, "-------------------------------------------------------------------------------\n");
	tempToFileBuffer(tempBuffer, &startPoint);

	sprintf_s(tempBuffer, size, "%15s |%15s |%15s |%15s |%15s\n", "NAME", "AVERAGE", "MIN", "MAX", "CALL");
	tempToFileBuffer(tempBuffer, &startPoint);

	sprintf_s(tempBuffer, size, "-------------------------------------------------------------------------------\n");
	tempToFileBuffer(tempBuffer, &startPoint);
	
	
	float average; float min; float max;
	for (int i = 0; i < profiler.CURRENT_PROFILE_SAMPLE_LENGTH; ++i)
	{
		auto& temp = profiler._sampleArr[i];
		int num = 0;
		// 호출횟수가 4회 이하인경우는 그냥평균냄
		if (temp.iCall < temp.MINMAX_COUNT * 2)
		{
			memset(temp.iMax, 0, sizeof(temp.iMax[0]) * temp.MINMAX_COUNT);
			memset(temp.iMin, 0, sizeof(temp.iMin[0]) * temp.MINMAX_COUNT);
			average = temp.iTotalTime * kMICRO / ((float)profiler._freq.QuadPart * temp.iCall);
			sprintf_s(tempBuffer, size, "%10s | %10fus, | %10lld", temp.szName, average, temp.iCall);
			tempToFileBuffer(tempBuffer, &startPoint);
		}
		else // 4회이상인 경우는 최대 최소 뺴고 평균냄
		{
			for (int i = 0; i < temp.MINMAX_COUNT; ++i)
			{
				if (temp.iMax[i] != LLONG_MIN)
				{
					temp.iTotalTime -= temp.iMax[i];
					++num;
				}
			}

			for (int i = 0; i < temp.MINMAX_COUNT; ++i)
			{
				if (temp.iMin[i] != LLONG_MAX)
				{
					temp.iTotalTime -= temp.iMin[i];
					++num;
				}
			}
			average = temp.iTotalTime * kMICRO / ((float)profiler._freq.QuadPart * (temp.iCall - num));
			min = temp.iMin[0] * kMICRO / (float)profiler._freq.QuadPart;
			max = temp.iMax[0] * kMICRO / (float)profiler._freq.QuadPart;
			sprintf_s(tempBuffer,size,"%10s |%10fus | %10fus | %10fus | %10lld\n", temp.szName, average, min, max, temp.iCall);
		}
		tempToFileBuffer(tempBuffer, &startPoint);
	}
	snprintf(tempBuffer, size, "-------------------------------------------------------------------------------\n");
	tempToFileBuffer(tempBuffer, &startPoint);

	snprintf(tempBuffer, size, "\n\n\n");
	tempToFileBuffer(tempBuffer, &startPoint);
	fwrite(fileBuffer, startPoint - fileBuffer, 1, pFile);
	fclose(pFile);
	delete[] fileBuffer;
}
#else
#define PROFILING(num)
#endif
