#include "replay.h"
#include <stdio.h>
#include <stdlib.h>
#include "zjh.h"
#include "game.h"
#include "log.h"

extern ZJH zjh;
extern Log log;


static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

#define RECORE_LEN 4096

Replay::Replay()
{
    pFile = NULL;
	memset(&start_record, 0, sizeof(StartRecord));
	play_record.clear();
	horse_record.clear();
}

Replay::~Replay()
{
    if (pFile != NULL)
    {
        fclose(pFile);
    }
	record_data = NULL;
}

int Replay::init(int ts, int ttid)
{
	if (record_data == NULL)
	{
		record_data = (byte*)malloc(RECORE_LEN);
	}
	memset(record_data, 0, RECORE_LEN);
	data_len = 0;
    /*if (pFile == NULL)
    {
        char buff[128] = {0, };
        snprintf(buff, sizeof(buff), "/data/logs/replay/%d_%d.txt", ts, ttid);
        //snprintf(buff, sizeof(buff), "../replay/hzmajiang/%d_%d.txt", ts, ttid);

		pFile = fopen(buff, "wb+");
        if (pFile == NULL)
        {
            return -1;
        }

        return 0;
    }
    else
    {
        fclose(pFile);
        pFile = NULL;
    }*/

    return 0;
}

int Replay::record_start(StartRecord &record)
{
	memset(record_data, 0, RECORE_LEN);
	memcpy((void *)record_data, &record, sizeof(record));
	data_len += sizeof(record);
    /*if (pFile == NULL)
    {
        return -1;
    }
	size_t ret = fwrite((const void*)&record, sizeof(StartRecord), 1, pFile);
    if (ret != 1)
    {
        return -1;
    }*/

    return 0;
}

int Replay::record_next(PlayRecord& record)
{

	memcpy((void *)(record_data+data_len) ,&record, sizeof(record));
	data_len += sizeof(record);

    /*if (pFile == NULL)
    {
        return -1;
    }
    size_t ret = fwrite((const void*)&record, sizeof(PlayRecord), 1, pFile);

    if (ret != 1)
    {

		mjlog.debug("fwrite error PlayRecord \n");
        return -1;
    }*/

    return 0;
}

int Replay::record_horse(HorseRecord & record)
{

	memcpy((void *)(record_data+data_len) ,&record, sizeof(record));
	data_len += sizeof(record);

	/*
    if (pFile == NULL)
    {
        return -1;
    }
    size_t ret = fwrite((const void*)&record, sizeof(HorseRecord), 1, pFile);

    if (ret != 1)
    {
		mjlog.debug("fwrite error HorseRecord \n");
        return -1;
    }*/

    return 0;
}

int Replay::record_write()
{
	int play_record_len = play_record.size();
	int horse_record_len = horse_record.size();
	start_record.play_count = play_record_len;
	start_record.horse_count = horse_record_len;
	/*if(pFile == NULL)
	{
		mjlog.debug("fwrite error pFile NULL \n");
		return -1;
	}*/

	int ret = record_start(start_record);
	if (ret == -1)
	{
		return -1;
	}

	for (int i = 0; i < play_record_len; i++)
	{
		ret = record_next(play_record[i]);
		if (ret == -1)
		{
			return -1;
		}
	}

	for (int i = 0; i < horse_record_len; i++)
	{
		ret = record_horse(horse_record[i]);
		if (ret == -1)
		{
			return -1;
		}
	}
	log.debug("record_write play_record_len[%d] horse_record_len[%d] sizeof(pFile) [%d]\n",play_record_len, horse_record_len);
	return 0;
}

int Replay::save(int ts, int ttid)
{
	char buff[32] = {0, };
	snprintf(buff,32, "%d_%d",ts, ttid);
	std::string record_data_64 = base64_encode(record_data, data_len);
	//mjlog.debug("record_data_64 = %s\n", record_data_64.c_str());
	int ret = zjh.record_rc->command("SET %s %s", buff, record_data_64.c_str());
    
	
	/*if (pFile != NULL)
    {
        fclose(pFile);
        pFile = NULL;
    }*/

    if (ret < 0)
    {
        log.error("save SET hrecord error \n");
        return - 1;
    }

	memset(&start_record, 0, sizeof(StartRecord));
	play_record.clear();
	horse_record.clear();
    return 0;
}

std::string Replay::base64_encode(byte const* bytes_to_encode, unsigned int in_len) 
{  
  std::string ret;  
  int i = 0;  
  int j = 0;  
  unsigned char char_array_3[3];  
  unsigned char char_array_4[4];  
  
  while (in_len--) 
  {  
    char_array_3[i++] = *(bytes_to_encode++);  
    if (i == 3) 
	{  
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;  
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);  
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);  
      char_array_4[3] = char_array_3[2] & 0x3f;  
  
      for(i = 0; (i <4) ; i++)  
	  {
        ret += base64_chars[char_array_4[i]];  
	  }
      i = 0;  
    }  
  }  
  
  if (i)  
  {  
    for(j = i; j < 3; j++)  
      char_array_3[j] = '\0';  
  
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;  
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);  
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);  
    char_array_4[3] = char_array_3[2] & 0x3f;  
  
    for (j = 0; (j < i + 1); j++)
	{
      ret += base64_chars[char_array_4[j]];  
	}
  
    while((i++ < 3))  
	{
      ret += '=';  
	}
  
  }  
  
  return ret;  
} 

std::string Replay::base64_decode(std::string const& encoded_string) 
{  
  int in_len = encoded_string.size();  
  int i = 0;  
  int j = 0;  
  int in_ = 0;  
  unsigned char char_array_4[4], char_array_3[3];  
  std::string ret;  
  
  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
   {  
    char_array_4[i++] = encoded_string[in_]; in_++;  
    if (i ==4) 
    {  
      for (i = 0; i <4; i++)  
      {
        char_array_4[i] = base64_chars.find(char_array_4[i]);  
      }
  
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);  
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);  
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];  
  
      for (i = 0; (i < 3); i++)  
      {
        ret += char_array_3[i];  
      }
      i = 0;  
    }  
  }  
  
  if (i) 
  {  
    for (j = i; j <4; j++)  
    {
      char_array_4[j] = 0;  
    }
    for (j = 0; j <4; j++)
    {  
      char_array_4[j] = base64_chars.find(char_array_4[j]);  
    }
    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);  
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);  
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];  
  
    for (j = 0; (j < i - 1); j++) 
    {
      ret += char_array_3[j];
    } 
  }  
  
  return ret;  
}  


