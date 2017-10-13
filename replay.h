#ifndef _RE_PLAY_H_
#define _RE_PLAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include "jpacket.h"

using namespace std;
#define MAX_SMSG 2048


typedef struct tagMsg
{
    int ttid;
    int ts;
    char* data;
}Msg;

class Replay
{
public:
    Replay();
    virtual ~Replay();

public:
    int init(int my_ts, int my_ttid);
    static int compress_data(const char *src, int srcLen, char *dest, int destLen);
    int save();
    int save(int ts, int ttid);
    int async_save(int ts, int ttid);

    int append_record(Json::Value &val);
    const char* get_compress_data();
    void free_compress_data();

    bool send_msg(int ts, int ttid, char* data);

    static int init_handler();
    static Msg msgs[MAX_SMSG];
    static int head;
    static int tail;
    static void* msg_handler(void* args);

private:
    Jpacket packet;
    int ttid;
    int ts;
    char *c_data;
    int c_data_len;
};
#endif
