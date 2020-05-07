// Author: Michal Oracki
// Title: "EAN-8 decoder"
// Date: 04.05.2020

#include <stdio.h>
#include <stdlib.h>

#define EAN8_NUM_OF_CODES 11
#define START_STOP_MOD_LENGTH 3
#define DIVIDER_MOD_LENGTH 5
#define DIGIT_MOD_LENGTH 7
#define SIDE_NUM_OF_DIGITS 4
#define SAFETY_OFFSET 2          // to read files with bar width less than 3px (1 or 2), leave commented
//#define FILE_INFO             // to get file info, leave uncommented



// in order to compile program properly, following options are obligatory:
// -m32 because program should be 32-bits (int = 4 bytes)
// -fpack-struct  because header of mono-chrome file should be 62 bytes long

// gcc -lm -m32 -fpack-struct ean8_main.c

typedef struct
{
    unsigned short bfType;
    unsigned long  bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned long  bfOffBits;
    unsigned long  biSize;
    long  biWidth;
    long  biHeight;
    short biPlanes;
    short biBitCount;
    unsigned long  biCompression;
    unsigned long  biSizeImage;
    long biXPelsPerMeter;
    long biYPelsPerMeter;
    unsigned long  biClrUsed;
    unsigned long  biClrImportant;
    unsigned long  RGBQuad_0;
    unsigned long  RGBQuad_1;
} bmpHdr;

typedef struct
{
    int width, height;		    // image width and height
    unsigned char* pImg;	    // pointer to beginning of pixel data
    unsigned char* pMiddle;     // pointer to beginning of middle row
    unsigned short barWidth;    // width of the bar in pixels
} imgInfo;


const unsigned int L_CODES_ARR[] = {
        // left codes
        114,    // 0
        102,    // 1
        108,    // 2
        66,     // 3
        92,     // 4
        78,     // 5
        80,     // 6
        68,     // 7
        72,     // 8
        116,    // 9
};
const unsigned int R_CODES_ARR[] = {
        // right codes
        13,     // 0
        25,     // 1
        19,     // ...
        61,
        35,
        49,
        47,
        59,
        55,     // ...
        11,     // 9
};

/**********************************************************************************************************************/

extern unsigned char* getRow(unsigned  char* pImg, unsigned char* buf, unsigned char* lastByte);
extern unsigned char* getCode(char* buf, int* code, const unsigned int* codesArr);


void* freeResources(FILE* pFile, void* pFirst, void* pSnd)
{
    if (pFile != 0)
        fclose(pFile);
    if (pFirst != 0)
        free(pFirst);
    if (pSnd !=0)
        free(pSnd);
    return 0;
}

imgInfo* readBMP(const char* fname)
{
    imgInfo* pInfo = 0;
    FILE* fbmp = 0;
    bmpHdr bmpHead;
    int lineBytes, y;
    unsigned long imageSize = 0;
    unsigned char* ptr;

    pInfo = 0;
    fbmp = fopen(fname, "rb");
    if (fbmp == 0)
        return 0;

    fread((void *) &bmpHead, sizeof(bmpHead), 1, fbmp);
    // some basic checks
    if (bmpHead.bfType != 0x4D42 || bmpHead.biPlanes != 1 ||
        bmpHead.biBitCount != 1 ||
        (pInfo = (imgInfo *) malloc(sizeof(imgInfo))) == 0)
        return (imgInfo*) freeResources(fbmp, pInfo->pImg, pInfo);

    pInfo->width = bmpHead.biWidth;
    pInfo->height = bmpHead.biHeight;
    imageSize = (((pInfo->width + 31) >> 5) << 2) * pInfo->height;

    if ((pInfo->pImg = (unsigned char*) malloc(imageSize)) == 0)
        return (imgInfo*) freeResources(fbmp, pInfo->pImg, pInfo);

    // process height (it can be negative)
    ptr = pInfo->pImg;
    lineBytes = ((pInfo->width + 31) >> 5) << 2; // line size in bytes
    if (pInfo->height > 0)
    {
        // "upside down", bottom of the image first
        ptr += lineBytes * (pInfo->height - 1);
        lineBytes = -lineBytes;
    }
    else
        pInfo->height = -pInfo->height;

    // reading image
    // moving to the proper position in the file
    if (fseek(fbmp, bmpHead.bfOffBits, SEEK_SET) != 0)
        return (imgInfo*) freeResources(fbmp, pInfo->pImg, pInfo);

    for (y=0; y<pInfo->height; ++y)
    {
        fread(ptr, 1, abs(lineBytes), fbmp);
        ptr += lineBytes;
    }
    fclose(fbmp);
    return pInfo;
}

unsigned char* GetMiddlePixel(imgInfo* pInfo, int y, int byteWidth)
{
    pInfo->pMiddle = pInfo->pImg;
    pInfo->pMiddle += byteWidth * (y-1);
    return pInfo->pMiddle;
}

const unsigned char* skipMargin(const unsigned char* buf)
{
    while(*buf == 1)
        buf += 1;
    return buf;
}

unsigned short getBarWidth(const unsigned char* buf)
{
    unsigned short barWidth = 0;
    while (*buf == 0)
    {
        buf += 1;
        barWidth += 1;
    }
    buf -= barWidth;
    return barWidth;
}

unsigned char chooseColor(const unsigned char* buf, unsigned short barWidth, unsigned short offset)
{
    if(*buf != *(buf+barWidth-offset-1))   // if two pixels in bar are different, then
    {                                      // we need to decide which color in bar is correct
        const unsigned char* newBuf = buf - offset;
        short i = 0;
        short black = 0;
        short white = 0;
        unsigned char mCode = 0;     // set default to black colour (if there will be same amount of white
        while(i < barWidth)                              // and black pixels, the black would be chosen)
        {
            if(*newBuf == 0) black += 1;
            else white += 1;

            if(white > (barWidth >> 1)) {mCode = 1; break;}     // check if more than half of a bar is white
            else if(black > (barWidth >> 1)) {mCode = 0; break;}  // check if more than half of a bar is black
            i += 1;
        }
        return mCode;
    }
    return *buf;
}

unsigned short decodeFragment(const unsigned char* buf, unsigned short moduleLength, unsigned short barWidth)
{
    unsigned short index = 0;
    unsigned short moduleCode = 0;

    while(index < moduleLength)
    {
        moduleCode = moduleCode << 1;
        #ifdef SAFETY_OFFSET
            moduleCode += chooseColor(buf,barWidth,SAFETY_OFFSET);
        #else
            moduleCode += chooseColor(buf,barWidth,0);
        #endif
        buf += barWidth;
        index += 1;
    }
    return moduleCode;
}

int* getDecodedCodes(int* codes, const unsigned char* buffer, imgInfo* pInfo)
{
    int* temp = codes;
    const unsigned char* buf = skipMargin(buffer);

    pInfo->barWidth = getBarWidth(buf);
    #ifdef SAFETY_OFFSET
        buf += SAFETY_OFFSET;   // (default 2) pixel offset
    #endif


    unsigned short index = 0;

    // decode start module
    *codes = decodeFragment(buf,START_STOP_MOD_LENGTH,pInfo->barWidth);
    codes += 1;
    buf += START_STOP_MOD_LENGTH * pInfo->barWidth;

    // decode left digits
    while(index < SIDE_NUM_OF_DIGITS)
    {
        *codes = decodeFragment(buf,DIGIT_MOD_LENGTH,pInfo->barWidth);
        index += 1;
        codes += 1;
        buf += DIGIT_MOD_LENGTH * pInfo->barWidth;
    }
    index = 0;

    // decode divider module
    *codes = decodeFragment(buf,DIVIDER_MOD_LENGTH,pInfo->barWidth);
    codes += 1;
    buf += DIVIDER_MOD_LENGTH * pInfo->barWidth;

    // decode right digits
    while(index < SIDE_NUM_OF_DIGITS)
    {
        *codes = decodeFragment(buf,DIGIT_MOD_LENGTH,pInfo->barWidth);
        index += 1;
        codes += 1;
        buf += DIGIT_MOD_LENGTH * pInfo->barWidth;
    }

    // decode stop module
    *codes = decodeFragment(buf,START_STOP_MOD_LENGTH,pInfo->barWidth);
    codes = temp;

    return codes;
}

char* getResult(char* result, int *codes)
{
    // start module check
    if(*codes != 2) return "Error Start (Invalid code or SAFETY_OFFSET is enabled,"
                           " when barWidth is lower than 3). Check file info for"
                           " more details (FILE_INFO).";
    codes += 1;

    // result from left side
    result = getCode(result, codes, L_CODES_ARR);
    if(result == NULL) return "Invalid code";   // check if there was an error in code
    result += SIDE_NUM_OF_DIGITS;
    codes += SIDE_NUM_OF_DIGITS;

    // divider module check
    if(*codes != 21) return "Error Divider Module (Invalid code)";
    codes += 1;

    // result from right side
    result = getCode(result, codes, R_CODES_ARR);
    if(result == NULL) return "Invalid code";
    result += SIDE_NUM_OF_DIGITS;
    codes += SIDE_NUM_OF_DIGITS;

    // stop module check
    if(*codes != 2) return "Error Stop Module (Invalid code)";

    *result = '\0';     // add null sign at the end of string
    result -= SIDE_NUM_OF_DIGITS << 1;

    return result;

}

void decodeEAN8(const char* fileName)
{
    imgInfo* pInfo;
    pInfo = readBMP(fileName);

    // moving to middle row
    int y = pInfo->height >> 1;
    int byteWidth = ((pInfo->width + 31) >> 5) << 2;
    pInfo->pMiddle = GetMiddlePixel(pInfo, y, byteWidth);

    // getting row from image
    unsigned int bufSize = byteWidth << 3;
    unsigned char* buf = (unsigned char*) malloc(bufSize);
    unsigned char* lastAddress = pInfo->pMiddle + byteWidth - 1;
    unsigned char* row;
    row = getRow(pInfo->pMiddle,buf,lastAddress);

    // decoding row
    int* codes = (int*) malloc(EAN8_NUM_OF_CODES * sizeof(int));
    int* decimal_codes;
    decimal_codes = getDecodedCodes(codes,row, pInfo);


    // result
    unsigned short numOfDigits = SIDE_NUM_OF_DIGITS << 1;
    char* result_buf = (char*) malloc(numOfDigits);
    char* result;
    result = getResult(result_buf, decimal_codes);

    #ifdef FILE_INFO
        printf("|*************************|\n");
        printf("File name: %s\n", fileName);
        printf("Size of bmpHeader = %d\n", sizeof(bmpHdr));
        if (sizeof(bmpHdr) != 62)
        {
            printf("Change compilation options so as bmpHdr struct size is 62 bytes.\n");

        }
        printf("Image width: %d\n", pInfo->width);
        printf("Image height: %d\n", pInfo->height);
        printf("Bar width: %dpx\n\n", pInfo->barWidth);
        printf("EAN-8 code: %s\n", result);
        printf("|*************************|\n\n");
    #else
        printf("EAN-8 code: %s\n", result);
    #endif

    free(pInfo);
    free(buf);
    free(codes);
    free(result_buf);
}

int main(int argc, char* argv[])
{
    // to decode files with width of bar 1-2 px
    // comment #define SAFETY_OFFSET to disable 2 px offset
    // ----------------------------------------------------
    // to get file info uncomment #define FILE_INFO
    // ----------------------------------------------------

    // test files
    const char* fileName = "Example.bmp";
    decodeEAN8(fileName);

    decodeEAN8("Example1.bmp");
    decodeEAN8("Example2.bmp");
    decodeEAN8("Example3.bmp");

    return 0;
}
