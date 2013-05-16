// VimMake: LDFLAGS=(-lpng -O3)

/* Copyright (c) 2012, Liu Lukai (liulukai@gmail.com)
All rights reserved.

Redistribution and use in source and binary forms, with or
without modification, are permitted provided that the
following conditions are met: 

1. Redistributions of source code must retain the above
copyright notice, this list of conditions and the following
disclaimer.
2. Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials
provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and
documentation are those of the authors and should not be
interpreted as representing official policies, either
expressed or implied, of the FreeBSD Project. */

#include <stdlib.h>
#include <string.h>
#include </usr/include/png.h>

typedef enum{false=0,true}bool;
typedef struct{
   int width,height, nPass;
   png_byte color_type, bit_depth, channels;
   png_structp png_ptr;
   png_infop info_ptr;
   png_bytepp rows;
   bool loaded;	// ready to call writeImage()
}Png;
int marginH=10, marginV=10, /* horizontal/vertical margins AFTER CROPPING in pixel */
    tol=20,	 /* Binarize threshold */ accFactor=2, /* accFactor<min(margins)/2 */
    perc=50;/* percentile of horizontal/vertical line-scan to
		    find margins, between 0 and 100 exclusively */
bool WhiteBG=true;

int writeImage(const char* filename, Png* ppng);
int read_png_file(const char* file_name, Png*);
int crop(const Png* src, Png* dest, const int dim[]);
int procPng(const Png* psrc, int[]);
int shortcut(const char*,const char*);
inline void freePng(Png* ppng);
inline void suffix(char* dest,const char* src);	   // prepend "O"
inline short percArray(unsigned short*,const int);

int main(int argc, char *argv[]) {
   if (argc!=2)
	return fprintf(stderr, "Usage: %s src-image\n", argv[0]);
   Png Ipng;
   if(read_png_file(argv[1], &Ipng))
	return printf("Error caught in read_png_file\n");
   int dims[] = {480,480,450,450}, ret;
   /* Manual control of margins
    * left/right/upper/bottom margins TO CROP in pixel
    * if(crop(&Ipng, &Opng, dims)) */
   /* Simple thresholding of consecutive dark blotches */
   if(procPng(&Ipng, dims))
	fprintf(stderr, "Error in \"%s\": No output image generated.\n", argv[1]);
   else{
	char Oname[strlen(argv[1])+2];
	suffix(Oname,argv[1]);
	Png Opng;
	if((ret=crop(&Ipng, &Opng, dims))>0)
	   fprintf(stderr, "Error in \"%s\": No output image generated.\n", argv[1]);
	else if(ret==-1)return shortcut(argv[1],Oname);
	else writeImage(Oname, &Opng);
	freePng(&Opng);
   }
   freePng(&Ipng);
   return 0;
}

int writeImage(const char* filename, Png* ppng){
   int code=0;
   FILE *fp;
   fp = fopen(filename, "wb");
   if(!fp){
	code=fprintf(stderr, "Could not open file %s for writing\n", filename);
	goto finalise;
   }
   ppng->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
   if(!ppng->png_ptr){
	code=fprintf(stderr, "Could not allocate write struct\n");
	goto finalise;
   }
   png_init_io(ppng->png_ptr, fp);
   png_set_IHDR(ppng->png_ptr, ppng->info_ptr, ppng->width,
	   ppng->height, 8, ppng->color_type, PNG_INTERLACE_NONE,
	   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
   png_write_info(ppng->png_ptr, ppng->info_ptr);
   png_write_rows(ppng->png_ptr, ppng->rows, ppng->height);
   png_write_end(ppng->png_ptr, 0);
finalise:
   if(fp) fclose(fp);
   return code;
}

int read_png_file(const char* file_name, Png* ppng){
   int code=0, headMax=8,y;
   char header[headMax];
   FILE *fp = fopen(file_name, "rb");
   if(!fp){
	code=fprintf(stderr, "[read_png_file] File %s could not be opened for reading", file_name);
	goto finalise;
   }
   fread(header, 1, headMax, fp);
   if(png_sig_cmp(header, 0, headMax)){
	code=fprintf(stderr,"[read_png_file] File %s is not recognized as a PNG file", file_name);
	goto finalise;
   }
   ppng->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	   0, 0, 0);
   if(!ppng->png_ptr){
	code=fprintf(stderr,"[read_png_file] png_create_read_struct failed");
	goto finalise;
   }
   ppng->info_ptr = png_create_info_struct(ppng->png_ptr);
   if(!ppng->info_ptr){
	code=fprintf(stderr,"[read_png_file] png_create_info_struct failed");
	goto finalise;
   }
   if(setjmp(png_jmpbuf(ppng->png_ptr))){
	code=fprintf(stderr,"[read_png_file] Error during init_io");
	goto finalise;
   }
   png_init_io(ppng->png_ptr, fp);
   png_set_sig_bytes(ppng->png_ptr, 8);
   png_read_info(ppng->png_ptr, ppng->info_ptr);
   ppng->width = png_get_image_width(ppng->png_ptr, ppng->info_ptr);
   ppng->height= png_get_image_height(ppng->png_ptr, ppng->info_ptr);
   ppng->color_type= png_get_color_type(ppng->png_ptr, ppng->info_ptr);
   ppng->bit_depth = png_get_bit_depth(ppng->png_ptr, ppng->info_ptr);
   ppng->channels = png_get_channels(ppng->png_ptr, ppng->info_ptr);
   ppng->nPass = png_set_interlace_handling(ppng->png_ptr);
   png_read_update_info(ppng->png_ptr, ppng->info_ptr);
   if(setjmp(png_jmpbuf(ppng->png_ptr))){
	code=fprintf(stderr,"[read_png_file] local Error during read_image");
	goto finalise;
   }
   ppng->rows = (png_bytep*)malloc(sizeof(png_bytep)*ppng->height);
   for(y=0; y<ppng->height; ++y)
	ppng->rows[y] = (png_bytep)malloc(png_get_rowbytes(
		   ppng->png_ptr, ppng->info_ptr));
   png_read_image(ppng->png_ptr, ppng->rows);
   ppng->loaded = true;
finalise:
   if(fp) fclose(fp);
   return code;
}

int shortcut(const char* fname,const char* dname){/* copy to destination */
   FILE* inf=fopen(fname,"r"), *outf=fopen(dname,"w");
   if(!inf || !outf)return fprintf(stderr,
	   "Cannot open files \"%s\" and \"%s\" for piping\n", inf,outf);
   char c;
   while(c=fgetc(inf))fputc(c,outf);
   fclose(inf); fclose(outf);
   return 0;
}

int crop(const Png* src, Png* dest, const int dims[]){
   /* dims: left-margin, right-margin, upper-margin, lower-margin */
   if(!src->loaded || !src->rows) return -1;
   if(dims[0]+dims[1]>src->width)
	return fprintf(stderr, "crop(): Left/right margins too "
		"large: [%d %d]>Width=%d\n", dims[0], dims[1], src->width);
   else if(dims[2]+dims[3]>src->height)
	return fprintf(stderr, "crop(): Upper/lower margins too "
		"large: [%d %d]>Height=%d\n", dims[2], dims[3], src->height);
   int y, startCol=dims[0], endCol=src->width-dims[1]-1, /*NOTE: modified*/
	   startRow=dims[2], endRow=src->height-dims[3]-1,
	   chan=src->channels;
   if(!startRow&&!startCol && endRow==src->height-1&&endCol==src->width-1)
	return -1;   /* no need to crop */
   dest->width = endCol-startCol+1;
   dest->height = endRow-startRow+1;
   dest->rows = (png_bytepp)malloc(sizeof(png_bytep)*dest->height);
   for(y=0; y < dest->height; ++y){
	dest->rows[y] = (png_bytep)malloc(chan*dest->width*
		sizeof(png_byte));
	png_bytep tmp = src->rows[y+startRow];
	memcpy(dest->rows[y], tmp+startCol*chan,
		chan*sizeof(png_byte)*dest->width);
   }
   dest->png_ptr = src->png_ptr; dest->info_ptr = src->info_ptr;
   dest->color_type = src->color_type; dest->bit_depth = src->bit_depth;
   dest->nPass = src->nPass; dest->channels = chan;
   dest->loaded = true;
   return 0;
}

inline void freePng(Png* ppng){
   if(!ppng)return;
   int y;
   if(ppng->rows)
	for(y=0; y<ppng->height; ++y)free(ppng->rows[y]);
}

inline void suffix(char* dest,const char* src){
   short last_slash=strlen(src);
   while(--last_slash && src[last_slash]!='/');	/* *nix directory */
   dest[last_slash?++last_slash:0]='O';
   if(last_slash)strncpy(dest,src,last_slash);
   strcpy(dest+last_slash+1, src+last_slash); 
}

inline int belowTol(const png_bytep first){
   return WhiteBG==true ? *first<tol : *first>255-tol;
}

void chkBG(const Png* src){
   const int chkN = 32;	   /* #pixels that get checked horizontally / vertically */
   int xJmp=src->width/chkN | 1, yJmp=src->height/chkN | 1, accBlack=0, x,y;
   for(y=0; y<src->height; y+=yJmp)
	for(x=0; x<src->width; x+=xJmp, accBlack+=*(*(src->rows+y)+x)<tol);
   WhiteBG = accBlack*2 < chkN*chkN;
}

int comp(const void*lhs, const void*rhs){
   const unsigned short* lnum=(const unsigned short*)lhs,
	   *rnum=(const unsigned short*)rhs;
   return *lnum>*rnum?1:(*lnum==*rnum?0:-1);
}
short percArray(unsigned short*arr,const int size){
   qsort(arr, size, sizeof(unsigned short),comp);
   return arr[size*perc/100];
}

int procPng(const Png* src, int dim[]){
   /* judge by consecutive strokes (blotches) */
   if(!src->loaded || !src->rows)return fprintf(stderr,
	   "procPng(): source image not loaded.\n");
   //chkBG(src);	   /* on rarity, documents use dark background */
   int x,y,c,startRow,endRow,startCol,endCol, chan=src->channels, consec;
   /* line scans, extract percentile of blotches, set as threshold Th, Tv */
   unsigned short cnt, szH=src->height/accFactor, nBlotchHor[szH],
			szV=src->width/accFactor, nBlotchVert[szV];
   memset(nBlotchHor,0,szH); memset(nBlotchVert,0,szV);
   for(y=cnt=0; y<src->height; y+=accFactor){	/* horizontal scan */
	for(x=c=0, consec=0; x<src->width*chan;
		x+=accFactor*chan,consec=0)
	   if(!consec&&belowTol(*(src->rows+y)+x))consec=++c;
	   else if(consec&&!belowTol(*(src->rows+y)+x))consec=0;
	nBlotchHor[cnt++]=c;
   }
   for(x=cnt=0; x<src->width; x+=accFactor){	/* vertical scan */
	for(y=c=0,consec=0; y<src->height; y+=accFactor)
	   if(!consec&&belowTol(*(src->rows+y)+x*chan))consec=++c;
	   else if(consec&&!belowTol(*(src->rows+y)+x*chan))consec=0;
	nBlotchVert[cnt++]=c;
   }
   int Th=percArray(nBlotchHor,szH), Tv=percArray(nBlotchVert,szV);
   /* find truncation dimension */
   for(y=c=0; y<src->height && c<=Th; y+=accFactor)
	for(x=c=0, consec=0; x<src->width*chan;
		x+=accFactor*chan,consec=0)
	   if(!consec&&belowTol(*(src->rows+y)+x)&&(consec=++c)>Th)break;
	   else if(consec&&!belowTol(*(src->rows+y)+x))consec=0;
   startRow=y;
   for(y=src->height-1,c=0; y>startRow && c<=Th; y-=accFactor)
	for(x=c=0, consec=0; x<src->width*chan;
		x+=accFactor*chan,consec=0)
	   if(!consec&&belowTol(*(src->rows+y)+x)&&(consec=++c)>Th)break;
	   else if(consec&&!belowTol(*(src->rows+y)+x))consec=0;
   endRow=y;
   if(startRow>marginV)startRow-=marginV;
   else startRow=0;  /* leave poor margin already in there alone */
   if(endRow+marginV+1<src->height)endRow+=marginV;
   else endRow=src->height-1;
   for(x=0; x<src->width && c<=Tv; x+=accFactor)
	for(y=startRow,consec=c=0; y<endRow; y+=accFactor)
	   if(!consec&&belowTol(*(src->rows+y)+x*chan)&&(consec=++c)>Tv)break;
	   else if(consec&&!belowTol(*(src->rows+y)+x*chan))consec=0;
   startCol=x;
   for(x=src->width-1,c=0; x>startCol && c<=Tv; x-=accFactor)
	for(y=startRow,consec=c=0; y<endRow; y+=accFactor)
	   if(!consec&&belowTol(*(src->rows+y)+x*chan)&&(consec=++c)>Tv)break;
	   else if(consec&&!belowTol(*(src->rows+y)+x*chan))consec=0;
   endCol=x;
   if(startCol>marginH)startCol-=marginH;
   else startCol=0;
   if(endCol<src->width-marginH-1)endCol+=marginH;
   else endCol=src->width-1;
   dim[0]=startCol; dim[1]=src->width-endCol-1;
   dim[2]=startRow; dim[3]=src->height-endRow-1;
   return 0;
}
