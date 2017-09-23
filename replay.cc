#include "replay.h"
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "zjh.h"
#include "game.h"
#include "log.h"

extern ZJH zjh;
extern Log mjlog;

// static int count = 0;

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

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
    snprintf(buff, 128, "/data/logs/ll_log/%d.txt", ttid);
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
    get_compress_data();
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

const char* Replay::get_compress_data()
{
    string str = packet.val.toStyledString();
    int len = str.length();

    char* dst = NULL;
    // int dstlen = len;

    if (len <= 0)
    {
        return NULL;
    }

    dst = (char*)malloc(len);
    int ret = compress_data(str.c_str(), len, dst, len);
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

        if(deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,   
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
            if(deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
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