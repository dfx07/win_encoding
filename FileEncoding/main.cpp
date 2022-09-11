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

#define FREE_DATA_BUFF(p) { delete[] p; p = NULL; }
#define FREE_DATA_FILE(p) { free(p)   ; p = NULL; }


/***************************************************************************
* @Brief : Check system endian is using                                     
* @Author: thuong.nv - [Date] :09/07/2022                                   
* @Param : void                                                             
* @Return: 0 =little endian | 1 =big endian                                 
***************************************************************************/
int get_system_endian()
{
    // little endian if true
    int n = 1;
    return (*(char *)&n == 1) ? 0 : 1;
}

void swap_endian_16(wchar_t* ch)
{
    *ch = ((*ch) >> 8) | (((*ch) & 0xFF) << 8);
}


/***************************************************************************
* @Brief : Convert from little to big endian and vice versa                 
* @Author: thuong.nv - [Date] :09/07/2022                                   
* @Param :                                                                  
*         [in] str   : wchar_t string                                       
*         [in] nsize : n charactor not bytes                                
* @Return: void                                                             
***************************************************************************/
void reverse_endian(wchar_t* str, const int& nsize)
{
    for (int i = 0; i < nsize; i++)
    {
        swap_endian_16(&(str)[i]);
    }
}

int utf8_to_utf16(const char* str_utf8, const int& nsize, wchar_t** str_utf16)
{
    *str_utf16 = NULL; int nwsize = 0; // reset
    if (!str_utf8) return nwsize;

    nwsize = ::MultiByteToWideChar(CP_UTF8, 0, &str_utf8[0], nsize, NULL, 0);
    if (nwsize <= 0) return nwsize;
    *str_utf16 = new wchar_t[nwsize + 1];
    (*str_utf16)[nwsize] = 0;

    return ::MultiByteToWideChar(CP_UTF8, 0, &str_utf8[0], nsize, *str_utf16, nwsize);
}

int utf16_to_utf8(const wchar_t* str_utf16, const int& nwsize, char** str_utf8)
{
    *str_utf8 = NULL; int nsize = 0; // reset
    if (!str_utf16) return nsize;

    nsize = ::WideCharToMultiByte(CP_UTF8, 0, &str_utf16[0], nwsize, NULL, 0, NULL, NULL);
    if (nsize <= 0) return nsize;
    *str_utf8 = new char[nsize + 1];
    (*str_utf8)[nsize] = 0;

    return ::WideCharToMultiByte(CP_UTF8, 0, &str_utf16[0], nwsize, *str_utf8, nsize, NULL, NULL);
}

int acp_to_utf16(const char* str_acp, const int& nsize, wchar_t** str_utf16)
{
    *str_utf16 = NULL; int nwsize = 0; // reset
    if (!str_acp) return nwsize;

    nwsize = ::MultiByteToWideChar(CP_ACP, 0, &str_acp[0], nsize, NULL, 0);
    if (nwsize <= 0) return nwsize;
    *str_utf16 = new wchar_t[nwsize + 1];
    (*str_utf16)[nwsize] = 0;

    return ::MultiByteToWideChar(CP_ACP, 0, &str_acp[0], nsize, *str_utf16, nwsize);
}

/***************************************************************************
*! \Brief  : Get encoding use bom bytes                                      
*! \Author : thuong.nv   - [Date] :09/07/2022                                
*! \Return : ANSI=0 | UTF-8=1 | UTF-16LE=2 | UTF-16BE=3 | UTF-32LE=4 | UTF-32BE=5
*! \Note   : N/A                                                             
***************************************************************************/
int get_encoding_bytes_bom(unsigned char* _bytes, const int& _size)
{
    if (!_bytes || !_size) return 0;

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
    if(_size >= 4)
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
    if (_size >= 2)
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

/***************************************************************************
* @Brief : Get encoding multiple bytes                                      
* @Author: thuong.nv - [Date] :09/07/2022                                   
* @Return: 1 = utf8 | 0 = not utf8                                          
* @Note  : https://unicodebook.readthedocs.io/guess_encoding.html           
***************************************************************************/
int is_utf8(const char* _bytes, size_t _size)
{
    if (!_bytes) return 1;

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

/***************************************************************************
* @Brief : save file use encoding file                                      
* @Author: thuong.nv - [Date] :09/07/2022                                   
* @Return: true/false                                                       
* @Note  : N/A                                                              
***************************************************************************/
bool save_file(const char* fpath, void* stream, const int& nsize)
{
    FILE* file = NULL;
    fopen_s(&file, fpath, "w");

    if (!file) return false;

    fwrite(stream, nsize, sizeof(char), file);

    fclose(file);

    return true;
}


/***************************************************************************
* !\ Brief : Read data file -> bytes                                        
* !\ Author: thuong.nv  -[Date] :09/07/2022                                 
* !\ Return: Number of bytes file & buffer & encoding                       
* !\ ANSI=0| UTF-8=1| UTF-16LE=2| UTF-16BE=3| UTF-32LE=4| UTF-32BE=5        
* !\ Note  : free the data return use  FREE_DATA_FILE function              
***************************************************************************/
int read_data_file(const char* fpath, void** buff, int* encoding)
{
    if (buff) { *buff = NULL; } int nbytes = 0;

    // Open and read file share data
    FILE *file = _fsopen(fpath, "rb", _SH_DENYRD);
    if  (!file) return nbytes;

    // Read number of bytes file size 
    fseek(file, 0L, SEEK_END);
    nbytes = static_cast<int>(ftell(file));
    fseek(file, 0L, SEEK_SET);

    // when encoding is utf16 then nstring(wchar_t) = nbtyes/2
    // So, I allocate extra 2 byte to break the buff data
    auto temp_buff = malloc((nbytes + 2)* sizeof(char));
    memset(temp_buff, 0, nbytes + 2);
    nbytes = (int)fread_s(temp_buff, nbytes, 1, nbytes, file);
    fclose(file);

    // Read content bytes file + nbyte read
    if (buff) { *buff = temp_buff; };

    // Read nbyte order mark : 4 byte
    if (encoding)
    {
        int min_byte = (nbytes < 4) ? nbytes : 4;
        *encoding = get_encoding_bytes_bom((PUCHAR)temp_buff, min_byte);

        // Special case : Ansi -> Utf8 (contain unicode)
        if (*encoding == 0 && is_utf8((const char*)(temp_buff), nbytes))
        {
            *encoding = 1;
        }
    }

    // check expection (not bytes, buff zero)
    if (nbytes == 0 && buff) { free(*buff); *buff = NULL;};

    // when encoding is utf16be and system endian is le then convert buff data
    if (*encoding == 3 && nbytes > 0 && get_system_endian() == 0)
    {
        reverse_endian((wchar_t*)*buff, nbytes / 2);
    }

    return nbytes;
}

/***************************************************************************
* @Brief : Read n first bytes in file                                       
* @Author: thuong.nv  -[Date] :09/07/2022                                   
* @Return: void*                                                            
* @Note  : free buff data return use  FREE_DATA_FILE function               
***************************************************************************/
void* read_nbyte_file(const char* fpath, size_t nbyte, size_t* nbyteread)
{
    void *buff = NULL; *nbyteread = 0;

    FILE *file = _fsopen(fpath, "r", _SH_DENYRD);
    if (!file) return NULL;

    // allocate nbyte data content bytes file
    buff = (void*)malloc(nbyte * sizeof(char));
    memset(buff, 0, nbyte);
    *nbyteread = fread_s(buff, nbyte, 1, nbyte, file);

    if (*nbyteread == 0) FREE_DATA_FILE(buff);

    fclose(file);
    return buff;
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

    return true;
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
    //int encoding = 0;
    ////char* textfile = ReadFileStream("data.txt", encoding);

    //std::wstring wa = L"thuong 한국어 키보드";

    //char* a = NULL;

    //int _size = 0;
    //utf16_to_utf8(wa.c_str(), wa.size(), a, _size);


    ////std::string  a = "fsadfasdf";

    //save_file_endcoding(L"output.txt", a, _size, 2);
    //save_file_endcoding(L"output.txt", a.c_str(), a.size(), 0);

    //int ecode1 = get_encoding_file("utf8.txt");
    //int ecode2 = get_encoding_file("utf8bom.txt");
    //int ecode3 = get_encoding_file("utf16le.txt");
    //int ecode4 = get_encoding_file("utf16be.txt");

    size_t bytes_size = 0;
    void* temp  = NULL; int encoding = 0;
    bytes_size = read_data_file("utf16be.txt", &temp, &encoding);
    wchar_t* buff_data = static_cast<wchar_t*>(temp);

    //auto a = acp_to_utf16(buff_data, bytes_size, &utf);
    //auto a = utf16_to_utf8(buff_data, bytes_size, &utf16);
    //
    //wchar_t* utf = NULL;
    //auto b = utf8_to_utf16(buff_data, bytes_size, &utf);

    FREE_DATA_FILE(buff_data);
}

