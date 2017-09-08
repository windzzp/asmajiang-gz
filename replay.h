#ifndef _RE_PLAY_H_
#define _RE_PLAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string> 

#ifndef byte
typedef unsigned char byte;
#endif

#define MBYTE(x) ((x) & 0xFF)
#define SBYTE(x) (((x) & 0xFF) + 0x80)

#ifdef __cplusplus
extern "C"{
#endif

    enum record_action_mask
    {
        RECORD_GUO                 = 0x01,
        RECORD_CHI 				   = 0x02,
        RECORD_PENG                = 0x03,
        RECORD_GANG	               = 0x04, 
        RECORD_TING		           = 0x05, 
        RECORD_HU				   = 0x06,	
        RECORD_TUOGUAN             = 0x07,
        RECORD_CANCEL_TUOGUAN      = 0x08,
        RECORD_MO                  = 0x09,
        RECORD_CHU                 = 0x0a,
		RECORD_NOTICE			   = 0x0b,
    };
    
	enum notice
	{
		RECORD_NOTICE_CHI          = 0x01,
		RECORD_NOTICE_PENG         = 0x01 << 1,
		RECORD_NOTICE_GANG         = 0x01 << 2,
		RECORD_NOTICE_HU           = 0x01 << 3,
		RECORD_NOTICE_GUO          = 0x01 << 4,
	};
#pragma pack(1)
    typedef struct tagConfig
    {
        byte horse_num;
        byte table_type;
        byte max_play_count;
        byte fang_pao;
		byte dead_double;
		byte forbid_same_ip;
		byte forbid_same_place;
    }Config;

    typedef struct tagStart
    {
        int uids[4];
        int tid;
		int owner_uid;
		char name[4][64];
        byte dealer;
        byte type;
        byte holes[4][13];   
        byte config[64];
		byte play_count;
		byte horse_count;
    }StartRecord;

    typedef struct tagPlay
    {
        byte seats[2];
        byte action;
        byte holes[3];
        byte flag; 
    }PlayRecord;

	typedef struct taghorse
	{
		byte horse[11];
		byte zhong[11];
		byte horse_number;
		byte seatid;
	}HorseRecord;
#pragma pack()


// typedef struct tagSnapShot
// {   
//     int tid;
//     int ttid;
//     int uids[4];
//     int owner_uid;
//     int state;
//     int dealer;
//     int type;
//     int holes[4][14];
//     int absorb[4][14];   
//     int cur_seat;
//     int action;
//     int cards[144];
//     int config[64];
// }SnapShot;
#ifdef __cplusplus
}
#endif


static inline bool is_base64(unsigned char c) {  
  return (isalnum(c) || (c == '+') || (c == '/'));  
} 

class Replay
{
public:
    Replay();
    virtual ~Replay();
	StartRecord  start_record;
	std::vector< PlayRecord > play_record;
	std::vector< HorseRecord > horse_record;

	byte * record_data;
	int data_len;
public:
    int init(int ts, int ttid);
    int record_start(StartRecord& record);
    int record_next(PlayRecord& record);
    int record_horse(HorseRecord& record);
	int record_write();
	int save(int ts, int ttid);

	std::string base64_encode(byte const* , unsigned int len);  
	std::string base64_decode(std::string const& s); 


private:
	FILE * pFile;  
};

// class CSnapShot
// {
// public:
//     CSnapShot();
//     virtual ~CSnapShot();

// public:
//     int init(int tid);
//     void gen_snapshot();
//     SnapShot* get_snapshot();

// private:
//     SnapShot snapshot;
//     FILE* pFile;
// };


#endif
