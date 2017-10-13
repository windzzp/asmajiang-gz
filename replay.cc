#include "replay.h"
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <pthread.h>

#include "zjh.h"
#include "game.h"
#include "log.h"

extern ZJH zjh;
extern Log mjlog;

int Replay::head = 0;
int Replay::tail = 0;
Msg Replay::msgs[MAX_SMSG];
static pthread_t thid = 0;

Replay::Replay()
{
    c_data = NULL;
    c_data_len = 0;
}

Replay::~Replay()
{
   
}

int Replay::init(int my_ts, int my_ttid)
{
    packet.val.clear();
    ttid = my_ttid;
    ts = my_ts;
    
    return 0;
}

int Replay::save()
{
    char buff[128] = {0};
    snprintf(buff, 128, "/data/logs/asmajiang/%d.txt", ttid);
    FILE* pFile = fopen(buff, "w");
    string str = packet.val.toStyledString();
    fwrite(str.c_str(), str.length(), 1, pFile);
    fclose(pFile);
    return 0;
}

int Replay::save(int ts, int ttid)
{
    char buff[64] = {0, };
    snprintf(buff, 64, "%d_%d",ts, ttid);

    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
    if (get_compress_data() == NULL)
    {
        mjlog.debug("get compress data error\n");
        return -1;
    }
    gettimeofday(&tv_end, NULL);
    int diff = tv_end.tv_usec - tv_begin.tv_usec + (tv_end.tv_sec - tv_begin.tv_sec) * 1000000;
    mjlog.debug("diff %s time is %d us\n", buff, diff);

    if (c_data != NULL)
    {
        int ret = zjh.record_rc->command("SET %s %b", buff, c_data, c_data_len);
        if (ret < 0)
        {   
            mjlog.error("save SET hrecord %s error \n", buff);
            free_compress_data();
            return - 1;
        }

        free_compress_data();
    }

    return 0;
}

int Replay::async_save(int ts, int ttid)
{
    string data = packet.val.toStyledString();
    if (data.length() == 0)
    {
        return 0;
    }

    char* buff = (char*)malloc(data.length() + 1);
    buff[data.length()] = '\0';
    memcpy(buff, data.c_str(), data.length());
    send_msg(ts, ttid, buff);
	packet.val.clear();

    return 0;
}

const char* Replay::get_compress_data()
{
    string str = packet.val.toStyledString();
    int len = str.length();

    char* dst = NULL;
    int dstlen = len;

    if (len <= 0)
    {
        return NULL;
    }

    if (len < 2048)
    {
        dstlen = 2048;
    }

    dst = (char*)malloc(dstlen);

    int ret = compress_data(str.c_str(), len, dst, dstlen);
    if (ret == -1)
    {
        free(dst);
        return NULL;
    }

    c_data = dst;
    c_data_len = ret;
    return c_data;
}

void Replay::free_compress_data()
{
    if (c_data != NULL)
    {
        free(c_data);
        c_data = NULL;
        c_data_len = 0;
    }
}

int Replay::append_record(Json::Value &val)
{
    packet.val.append(val);
    return 0;
}

int Replay::compress_data(const char *src, int srcLen, char *dest, int destLen)
{
    z_stream c_stream;  
    int err = 0;  
    int windowBits = 15;  
    int GZIP_ENCODING = 16;  
  
    if(src && srcLen > 0)  
    {  
        c_stream.zalloc = (alloc_func)0;  
        c_stream.zfree = (free_func)0;  
        c_stream.opaque = (voidpf)0;

        if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,   
                    windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            return -1;
        }

        c_stream.next_in  = (Bytef *)src;  
        c_stream.avail_in  = srcLen;  
        c_stream.next_out = (Bytef *)dest;  
        c_stream.avail_out  = destLen;

        while (c_stream.avail_in != 0 && static_cast<int>(c_stream.total_out) < destLen)   
        {  
            if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
            {
                return -1;
            }
        }

        if (c_stream.avail_in != 0)
        {
            //return c_stream.avail_in;
            return -1;
        }

        for (;;)
        {
            if ((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END)
                break;

            if (err != Z_OK)
                return -1;
        }

        if (deflateEnd(&c_stream) != Z_OK)
        {
            return -1;
        }

        return c_stream.total_out;  
    }

    return -1;  
}
   
int Replay::init_handler()
{
    if (thid != 0)
    {
        return 0;
    }

    int ret = pthread_create(&thid, NULL, msg_handler, (void*)&msgs);
    if (ret < 0)
    {
        mjlog.error("create msg handler thread error");
        return -1;
    }

    pthread_detach(thid);
    return 0;
}

void* Replay::msg_handler(void* args)
{
    Msg* msgs = (Msg*)args;
    while (1)
    {
        int cur_head = __sync_add_and_fetch(&head, 0);

        if (tail != cur_head)
        {     
            Msg& msg = msgs[tail];
            if (msg.data == NULL)
            {
                continue;
            }
  
            int len = strlen(msg.data);
            int dstlen = len;
            int ts = msg.ts;
            int ttid = msg.ttid;

            int next_tail = (tail + 1) % MAX_SMSG;
            if (next_tail > tail)
            {
                tail = __sync_add_and_fetch(&tail, 1);
            }
            else
            {
                tail = __sync_sub_and_fetch(&tail, MAX_SMSG - 1);
            }
            
            if (len <= 0)
            {                
                continue;
            }

            if (len < 2048)
            {
                dstlen = 2048;
            }

            char* dst = (char*)malloc(dstlen);
            dstlen = Replay::compress_data(msg.data, len, dst, dstlen);
            free(msg.data);
            msg.data = NULL;

            if (dstlen == -1)
            {
                free(dst);
                continue;
            }
            
            char buff[64] = {0, };
            snprintf(buff, 64, "%d_%d",ts, ttid);
            int ret = zjh.record_rc->command("SET %s %b", buff, dst, dstlen);
            if (ret < 0)
            {   
                mjlog.error("save SET hrecord %s error \n", buff);
                free(dst);
                continue;
            }

            free(dst);
        }
        else
        {
            sleep(1);
        }
    }

    return NULL;
}

bool Replay::send_msg(int ts, int ttid, char* data)
{
    int next_head = (head + 1) % MAX_SMSG;
    int cur_tail = __sync_add_and_fetch(&tail, 0);
    if (next_head == cur_tail)
    {
        mjlog.error("send msg ts(%d), ttid(%d) fail.\n", ts, ttid);
        return false;
    }

    msgs[head].ts = ts;
    msgs[head].ttid = ttid;
    msgs[head].data = data;

    if (next_head > head)
    {
        head = __sync_add_and_fetch(&head, 1);
    }
    else
    {
        head = __sync_sub_and_fetch(&head, MAX_SMSG - 1);
    }

    mjlog.debug("send_msg update head[%d]\n", head);
    return true;
}
