#ifndef _RE_PLAY_H_
#define _RE_PLAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "jpacket.h"

using namespace std;

#ifndef byte
typedef unsigned char byte;
#endif

class Replay
{
public:
    Replay();
    virtual ~Replay();

public:
    int init(int my_ts, int my_ttid);
    int compress_data(const char *src, int srcLen, char *dest, int destLen);
    int save();
    int save(int ts, int ttid);

    int append_record(Json::Value &val);
    const char* get_compress_data();
    void free_compress_data();
private:
    Jpacket packet;
    int ttid;
    int ts;
    char *c_data;
    int c_data_len;

};
#endif
