#include <iostream>
#include <Windows.h>
#include "Encoding.h"

//======================================
//00 00 FE FF   UTF-32, big-endian      
//FF FE 00 00   UTF-32, little-endian   
//FE FF         UTF-16, big-endian      
//FF FE         UTF-16, little-endian   
//EF BB BF      UTF-8                   
//======================================

void utf8_to_utf16(const char* str, const int& n_size, wchar_t*& utf16, int& n_wsize)
{
    utf16 = NULL; n_wsize = 0; // reset
    if (!str) return;

    n_wsize = ::MultiByteToWideChar(CP_UTF8, 0, &str[0], n_size, NULL, 0);
    utf16 = new wchar_t[n_wsize];
    ::MultiByteToWideChar(CP_UTF8, 0, &str[0], n_size, &utf16[0], n_wsize);
}

void utf16_to_utf8(const wchar_t* str, const int& n_wsize, char*& utf8, int& n_size)
{
    utf8 = NULL; n_size = 0; // reset
    if (!str) return;

    n_size = ::WideCharToMultiByte(CP_UTF8, 0, &str[0], n_wsize, NULL, 0, NULL, NULL);
    utf8 = new char[n_size + 1];
    memset(utf8, '\0', n_size + 1);
    ::WideCharToMultiByte(CP_UTF8, 0, &str[0], n_wsize, &utf8[0], n_size, NULL, NULL);
}

//@brief  : get encoding use byte bom
//@return : ANSI=0 | UTF-8=1 | UTF-16LE=2 | UTF-16BE=3 | UTF-32LE=4 | UTF-32BE=5
int get_encoding_bytes_bom(unsigned char* _bytes, const int _size)
{
    unsigned char utf8[]    = { 0xEF, 0xBB, 0xBF      };
    unsigned char utf16le[] = { 0xFF, 0xFE            };
    unsigned char utf16be[] = { 0xFE, 0xFF            };
    unsigned char utf32le[] = { 0xFF, 0xFE, 0x00, 0x00};
    unsigned char utf32be[] = { 0x00, 0x00, 0xFE, 0xFF};

    if (_size >= 3)
    {
        // utf-8
        if (_bytes[0] == utf8[0] && _bytes[1] == utf8[1] &&
            _bytes[2] == utf8[2])
            return 1;
    }
    else if(_size >= 4)
    {
        // utf-32le
        if (_bytes[0] == utf32le[0] && _bytes[1] == utf32le[1] &&
            _bytes[2] == utf32le[2] && _bytes[3] == utf32le[3])
            return 4;

        // utf-32be
        if (_bytes[0] == utf32be[0] && _bytes[1] == utf32be[1] &&
            _bytes[2] == utf32be[2] && _bytes[3] == utf32be[3])
            return 5;
    }
    else if (_size >= 2)
    {
        // utf-16le
        if (_bytes[0] == utf16le[0] && _bytes[1] == utf16le[1])
            return 2;

        // utf-16be
        if (_bytes[0] == utf16be[0] && _bytes[1] == utf16be[1])
            return 3;
    }

    return 0; // ansi
}

//@brief  : get encoding multiple bytes
//@ref    : https://unicodebook.readthedocs.io/guess_encoding.html
//@return : 0: utf8 | 1 != utf8
size_t get_file_size(FILE*& file)
{
    fseek(file, 0L, SEEK_END);
    size_t n_bytes_size = static_cast<size_t>(ftell(file));
    fseek(file, 0L, SEEK_SET);

    return n_bytes_size;
}

//@brief  : get encoding use byte bom
//@return : ANSI=0 | UTF-8=1 | UTF-16LE=2 | UTF-16BE=3 | UTF-32LE=4 | UTF-32BE=5
size_t read_data_file(const char* path, char** buff)
{
    // reset data
    size_t nbyte = 0; *buff = NULL;

    FILE* file = NULL;
    fopen_s(&file, path, "r");

    if (!file) return 0;

    // read length bytes file size
    fseek(file, 0L, SEEK_END);
    nbyte = static_cast<size_t>(ftell(file));
    fseek(file, 0L, SEEK_SET);

    // read content bytes file
    *buff = new char[nbyte + 1];
    buff[nbyte] = '\0';
    fread_s(buff, nbyte, sizeof(char), nbyte, file);

    return nbyte;
}

//@brief  : get encoding multiple bytes
//@ref    : https://unicodebook.readthedocs.io/guess_encoding.html
//@return : 0: utf8 | 1 != utf8
int is_utf8(const char* _bytes, size_t _size)
{
    const unsigned char *str = (unsigned char*)_bytes;
    const unsigned char *end = str + _size;
    unsigned char byte;
    unsigned int code_length, i;
    uint32_t ch;
    while (str != end) 
    {
        byte = *str;
        /* 1 byte sequence: U+0000..U+007F */
        if (byte <= 0x7F) 
        {
            str += 1;
            continue;
        }

        /* 0b110xxxxx: 2 bytes sequence */
        if (0xC2 <= byte && byte <= 0xDF)
            code_length = 2;
        /* 0b1110xxxx: 3 bytes sequence */
        else if (0xE0 <= byte && byte <= 0xEF)
            code_length = 3;
        /* 0b11110xxx: 4 bytes sequence */
        else if (0xF0 <= byte && byte <= 0xF4)
            code_length = 4;
        else {
            /* invalid first byte of a multibyte character */
            return 0;
        }

        if (str + (code_length - 1) >= end)
        {
            /* truncated string or invalid byte sequence */
            return 0;
        }

        /* Check continuation bytes: bit 7 should be set, bit 6 should be
        * unset (b10xxxxxx). */
        for (i = 1; i < code_length; i++) 
        {
            if ((str[i] & 0xC0) != 0x80)
                return 0;
        }

        /* 2 bytes sequence: U+0080..U+07FF */
        if (code_length == 2)
        {
            ch = ((str[0] & 0x1f) << 6) + (str[1] & 0x3f);
            /* str[0] >= 0xC2, so ch >= 0x0080.
            str[0] <= 0xDF, (str[1] & 0x3f) <= 0x3f, so ch <= 0x07ff */
        }
        /* 3 bytes sequence: U+0800..U+FFFF */
        else if (code_length == 3)
        {
            ch = ((str[0] & 0x0f) << 12) + ((str[1] & 0x3f) << 6) +
                (str[2] & 0x3f);
            /* (0xff & 0x0f) << 12 | (0xff & 0x3f) << 6 | (0xff & 0x3f) = 0xffff,
            so ch <= 0xffff */
            if (ch < 0x0800)
                return 0;

            /* surrogates (U+D800-U+DFFF) are invalid in UTF-8:
            test if (0xD800 <= ch && ch <= 0xDFFF) */
            if ((ch >> 11) == 0x1b)
                return 0;
        }
        /* 4 bytes sequence: U+10000..U+10FFFF */
        else if (code_length == 4)
        {
            ch = ((str[0] & 0x07) << 18) + ((str[1] & 0x3f) << 12) +
                ((str[2] & 0x3f) << 6) + (str[3] & 0x3f);
            if ((ch < 0x10000) || (0x10FFFF < ch))
                return 0;
        }
        str += code_length;
    }
    return 1;
}

//@return :Unkown: -1| ANSI=0 | UTF-8=1 | UTF-16LE=2 | UTF-16BE=3 | UTF-32LE=4 | UTF-32BE=5
int get_encoding_file(const char* path)
{
    FILE* file = NULL;
    fopen_s(&file, path, "r");

    if (!file) return -1;
    // read byte order mark 
    unsigned char bytes_mark[4] = { 0x00 };
    int bytes_mark_size = fread_s(bytes_mark, 4, sizeof(char), 4, file);

    int encoding = get_encoding_bytes_bom(bytes_mark, bytes_mark_size);

    // number of bytes file size 
    fseek(file, 0L, SEEK_END);
    size_t n_bytes_size = static_cast<size_t>(ftell(file));
    fseek(file, 0L, SEEK_SET);

    // read content bytes file
    unsigned char* buff = new unsigned char[n_bytes_size + 1];
    buff[n_bytes_size] = '\0';
    fread_s(buff, n_bytes_size, sizeof(char), n_bytes_size, file);

    if (encoding == 0) // ansi -> utf8 (content unicode)
    {
        if (is_utf8((const char*)buff, n_bytes_size))
        {
            encoding = 1;
        }
    }

    delete[] buff;
    fclose(file);

    return encoding;
}


bool save_file_endcoding(const wchar_t* fpath, const char* stream, const int& n_size, const int& encoding =0)
{
    FILE* file = NULL;
    _wfopen_s(&file, fpath, L"w");

    if (!file) return false;

    //======================================
    //00 00 FE FF   UTF-32, big-endian      
    //FF FE 00 00   UTF-32, little-endian   
    //FE FF         UTF-16, big-endian      
    //FF FE         UTF-16, little-endian   
    //EF BB BF      UTF-8                   
    //======================================
    int encode = encoding;

    unsigned char bytes_mark_utf8   [] = {0xEF, 0xBB, 0xBF      };
    unsigned char bytes_mark_utf16le[] = {0xFF, 0xFE            };
    unsigned char bytes_mark_utf16be[] = {0xFE, 0xFF,           };

    if (encode == 0) // UTF-8
    {
        //memcpy_s(file, 3, bytes_mark_utf8, 3);
        for (size_t i = 0; i < sizeof(bytes_mark_utf8) / sizeof(bytes_mark_utf8[0]); i++)
            fputc(bytes_mark_utf8[i], file);
        fputs(stream, file);
        //memcpy_s(file + 3, n_size, stream, n_size);
    }
    else if (encode == 1) //UTF-16, little-endian
    {
        for (size_t i = 0; i < sizeof(bytes_mark_utf16le) / sizeof(bytes_mark_utf16le[0]); i++)
            fputc(bytes_mark_utf16le[i], file);
        fputs(stream, file);
        //memcpy_s(file + 2, n_size, stream, n_size);
    }
    else if (encode == 2)
    {
        for (size_t i = 0; i < sizeof(bytes_mark_utf16be) / sizeof(bytes_mark_utf16be[0]); i++)
            fputc(bytes_mark_utf16be[i], file);
        fputs(stream, file);
        //memcpy_s(file, 2, bytes_mark_utf16be, 2);
        //memcpy_s(file + 2, n_size, stream, n_size);
    }
    fclose(file);
}


bool utf8_to_jis(const char* string)
{

    return true;
}

bool is_utf8(const char* string)
{
    if (!string)
        return true;

    const unsigned char * bytes = (const unsigned char *)string;
    unsigned int cp;
    int num;

    while (*bytes != 0x00)
    {
        if ((*bytes & 0x80) == 0x00)
        {
            // U+0000 to U+007F 
            cp = (*bytes & 0x7F);
            num = 1;
        }
        else if ((*bytes & 0xE0) == 0xC0)
        {
            // U+0080 to U+07FF 
            cp = (*bytes & 0x1F);
            num = 2;
        }
        else if ((*bytes & 0xF0) == 0xE0)
        {
            // U+0800 to U+FFFF 
            cp = (*bytes & 0x0F);
            num = 3;
        }
        else if ((*bytes & 0xF8) == 0xF0)
        {
            // U+10000 to U+10FFFF 
            cp = (*bytes & 0x07);
            num = 4;
        }
        else
            return false;

        bytes += 1;
        for (int i = 1; i < num; ++i)
        {
            if ((*bytes & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (*bytes & 0x3F);
            bytes += 1;
        }

        if  ((cp > 0x10FFFF) ||
            ((cp >= 0xD800) && (cp <= 0xDFFF)) ||
            ((cp <= 0x007F) && (num != 1)) ||
            ((cp >= 0x0080) && (cp <= 0x07FF) && (num != 2)) ||
            ((cp >= 0x0800) && (cp <= 0xFFFF) && (num != 3)) ||
            ((cp >= 0x10000) && (cp <= 0x1FFFFF) && (num != 4)))
            return false;
    }

    return true;
}

char* ReadFileStream(const char* fpath, int& encoding)
{
    FILE* file;
    file = fopen(fpath, "r");

    char* buff = NULL;

    if (file)
    {
        fseek(file, 0L, SEEK_END);
        int f_nsize = ftell(file);

        buff = new char[f_nsize + 1];
        memset(buff, '\0', f_nsize + 1);

        fseek(file, 0L, SEEK_SET);

        fread(buff, sizeof(char), f_nsize, file);

        encoding = is_utf8(buff);

        fclose(file);
    }

    return buff;
}

bool SaveFileStream(const char* fpath, const int& encoding)
{
    //======================================
    //00 00 FE FF   UTF-32, big-endian      
    //FF FE 00 00   UTF-32, little-endian   
    //FE FF         UTF-16, big-endian      
    //FF FE         UTF-16, little-endian   
    //EF BB BF      UTF-8                   
    //======================================

    return true;
}


int main()
{
    int encoding = 0;
    //char* textfile = ReadFileStream("data.txt", encoding);

    std::wstring wa = L"thuong 한국어 키보드";

    char* a = NULL;

    int _size = 0;
    utf16_to_utf8(wa.c_str(), wa.size(), a, _size);


    //std::string  a = "fsadfasdf";

    save_file_endcoding(L"output.txt", a, _size, 2);
    //save_file_endcoding(L"output.txt", a.c_str(), a.size(), 0);
}

