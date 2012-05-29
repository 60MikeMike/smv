// $Date$ 
// $Revision$
// $Author$

// svn revision character string
char IOslice_revision[]="$Revision$";

#include "options.h"
#ifdef pp_GLEW
#include "glew.h"
#endif
#include <stdio.h>
#ifdef WIN32
#include <share.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef pp_OSX
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "compress.h"
#include "string_util.h"
#include "smv_endian.h"
#include "update.h"
#include "interp.h"
#include "smokeviewvars.h"

#define HEADER_SIZE 4
#define TRAILER_SIZE 4
#define FORTSLICEREAD(var,size) FSEEK(SLICEFILE,HEADER_SIZE,SEEK_CUR);\
                           returncode=fread(var,4,size,SLICEFILE);\
                           if(endianswitch==1)endian_switch(var,size);\
                           FSEEK(SLICEFILE,TRAILER_SIZE,SEEK_CUR)

int endianswitch;
float gslice_valmin, gslice_valmax, *gslicedata;
mesh *gslice_valmesh;


float get_texture_index(float *xyz);
void draw_triangle(float *v1, float *v2, float *v3, 
                   float t1, float t2, float t3,
                   float del, int level);
void draw_triangle_outline(float *v1, float *v2, float *v3, 
                   float del, int level);
int getslicezlibdata(char *file,
                            int set_tmin, int set_tmax, float tmin, float tmax, int ncompressed, int sliceskip, int nsliceframes,
                            float *times, unsigned char *compressed_data, compinfo *compindex, float *valmin, float *valmax);
int average_slice_data(float *data_out, float *data_in, int ndata, int data_per_timestep, float *times, int ntimes, float average_time);
int getsliceheader(char *comp_file, char *size_file, int compression_type, 
                   int framestep, int set_tmin, int set_tmax, float tmin, float tmax,
                   int *nx, int *ny, int *nz, int *nsteps, int *ntotal, float *valmin, float *valmax);
int getsliceheader0(char *comp_file, char *size_file, int compression_type, int *i1, int *i2, int *j1, int *j2, int *k1, int *k2, int *slice3d);
int getslicecompresseddata(char *file,
                            int set_tmin, int set_tmax, float tmin, float tmax, int ncompressed, int sliceskip, int nsliceframes,
                            float *times, unsigned char *compressed_data, compinfo *compindex, float *valmin, float *valmax);

int makeslicesizefile(char *file, char *sizefile, int compression_type);

#ifdef WIN32
#define FOPEN(file,mode) _fsopen(file,mode,_SH_DENYNO)
#else
#define FOPEN(file,mode) fopen(file,mode)
#endif

#define FORTRLESLICEREAD(var,size) FSEEK(RLESLICEFILE,4,SEEK_CUR);\
                           returncode=fread(var,4,size,RLESLICEFILE);\
                           if(endianswitch==1)endian_switch(var,size);\
                           FSEEK(RLESLICEFILE,4,SEEK_CUR)

#define GET_SLICE_COLOR(color,index) \
	   if(sd->constant_color==NULL){\
	     int i11;\
         i11 = sd->iqsliceframe[(index)];\
         color = rgb_slice + 4*i11;\
       }\
       else{\
         color = sd->constant_color;\
       }

#define GET_VAL(U,VAL,n) \
         VAL=0.0;           \
         if(U!=NULL){       \
           if(U->compression_type==1){\
             VAL = U->qval256[U->iqsliceframe[(n)]];\
           }                                  \
           else{                              \
             VAL = U->qslice[(n)];               \
           }                                  \
         }

#define GET_VEC_DXYZ(U,DU,n) \
         DU=0.0;           \
         if(U!=NULL){       \
           int i11a;  \
           if(U->compression_type==1){\
             i11a=U->iqsliceframe[(n)];\
             DU=U->qval256[i11a];\
           }                                  \
           else{                              \
             DU = U->qslice[(n)];               \
           }                                  \
         }                                   \
         DU *= 0.05*vecfactor/vrange

#define GET_VEC_DXYZ_TERRAIN(U,DU) \
         DU=0.0;           \
         if(U!=NULL){       \
           int i11a;  \
           if(U->compression_type==1){\
             i11a=f1*U->iqsliceframe[n1]+f2*U->iqsliceframe[n2];\
             DU=U->qval256[i11a];\
           }                                  \
           else{                              \
             DU=f1*U->qslice[n1]+f2*U->qslice[n2];               \
           }                                  \
         }                                   \
         DU *= 0.05*vecfactor/vrange

/* ------------------ out_slice ------------------------ */

void out_slicefile(slicedata *sd){
  int file_unit=20,slicefilelen;

  slicefilelen=strlen(sd->file);
  FORTwriteslicedata(&file_unit,sd->file,
    &sd->is1,&sd->is2,&sd->js1,&sd->js2,&sd->ks1,&sd->ks2,
    sd->qslicedata,sd->times,&sd->ntimes,slicefilelen);
}

/* ------------------ Creadslice_frame ------------------------ */

int Creadslice_frame(int frame_index,int sd_index,int flag){
  slicedata *sd;
  int slicefilelen;
  int headersize,framesize;
  int frame_size;
  long int skip_local;
  int returncode;
  FILE *SLICEFILE;
  float *time_local,*slicevals;
  int error;

  sd = sliceinfo + sd_index;
  if(sd->loaded==1)readslice(sd->file,sd_index,UNLOAD,&error);
  if(flag==UNLOAD){
    FREEMEMORY(sd->qslicedata);
    FREEMEMORY(sd->times);
    return 0;
  }
  slicefilelen = strlen(sd->file);
  if(frame_index==0){
    if(sd->compression_type==0){

      FORTgetslicesizes(sd->file, &sd->nslicei, &sd->nslicej, &sd->nslicek, &sd->ntimes, &sliceframestep, &endian_smv,&error,
        &settmin_s, &settmax_s, &tmin_s, &tmax_s, &headersize, &framesize,
        slicefilelen);
    }
    else if(sd->compression_type==1){
      if(
        getsliceheader(sd->comp_file,sd->size_file,sd->compression_type,
                       sliceframestep,settmin_s,settmax_s,tmin_s,tmax_s,
                       &sd->nslicei, &sd->nslicej, &sd->nslicek, &sd->ntimes, &sd->ncompressed, &sd->valmin, &sd->valmax)==0){
        readslice("",sd_index,UNLOAD,&error);
        return -1;
      }
    }
  }
  skip_local =           (HEADER_SIZE+30        +TRAILER_SIZE); // long label
  skip_local +=          (HEADER_SIZE+30        +TRAILER_SIZE); // short label
  skip_local +=          (HEADER_SIZE+30        +TRAILER_SIZE); // unit label
  skip_local +=          (HEADER_SIZE+6*4        +TRAILER_SIZE); // is1, is2, js1, js2, ks1, ks2
  
  frame_size = sd->nslicei*sd->nslicej*sd->nslicek;
  skip_local += frame_index*(HEADER_SIZE + 4 + TRAILER_SIZE); // 
  skip_local += frame_index*(HEADER_SIZE + frame_size*4 + TRAILER_SIZE); // 

  SLICEFILE=fopen(sd->file,"rb");
  if(SLICEFILE==NULL){
    return -1;
  }

  returncode=FSEEK(SLICEFILE,skip_local,SEEK_SET); // skip from beginning of file

  if(frame_index==0){
    if(NewMemory((void **)&sd->qslicedata,2*frame_size*sizeof(float))==0||
       NewMemory((void **)&sd->times,sizeof(float))==0){
      return -1;
    }
  }
  slicevals=sd->qslicedata;
  if(frame_index%2!=0){
    slicevals+=frame_size;
  }
  time_local=sd->times;

  FORTSLICEREAD(time_local,1);
  FORTSLICEREAD(slicevals,frame_size);
  fclose(SLICEFILE);
  return 0;
}

/* ------------------ output_mfed_csv ------------------------ */

void output_mfed_csv(multislicedata *mslicei){
  FILE *AREA_STREAM=NULL;
  char fed_area_file[1024],*ext;
  slicedata *slice0;
  int nslices;
  float *areas;
  int i;

  nslices = mslicei->nslices;
  if(nslices<=0)return;
  areas=mslicei->contour_areas;
  if(areas==NULL)return;

  slice0 = sliceinfo + mslicei->islices[0];

  strcpy(fed_area_file,slice0->file);
  ext=strrchr(fed_area_file,'.');
  if(ext!=NULL){
    *ext=0;
    strcat(fed_area_file,"_marea.csv");
    AREA_STREAM=fopen(fed_area_file,"w");
  }
  if(AREA_STREAM==NULL)return;

  fprintf(AREA_STREAM,"\"time\",\"0.0->0.3\",\"0.3->1.0\",\"1.0->3.0\",\"3.0->\"\n");
  for(i=0;i<slice0->ntimes;i++){
    fprintf(AREA_STREAM,"%f,%f,%f,%f,%f\n",
    slice0->times[i],areas[0],areas[1],areas[2],areas[3]);
    areas+=4;
  }
  fclose(AREA_STREAM);
}

/* ------------------ readfed ------------------------ */

void readfed(int file_index, int flag, int file_type, int *errorcode){
  feddata *fedi;
  slicedata *fed_slice,*o2,*co2,*co;
  isodata *fed_iso;
  int error_local;
  contour *fed_contours=NULL;
  mesh *meshi;
  float *xgrid=NULL, *ygrid=NULL;
  char *iblank;
  int nx, ny;
  float levels[6]={-0.00001,0.3,1.0,3.0};
  int nlevels=5; // 2 extra levels for below 0.0 and above 3.0
  float *areas;
  int ibar, jbar, kbar;
  int nxy;

#define FEDCO(CO) ( 4.607*pow(1000000.0*CLAMP(CO,0.0,0.1),1.036)/10000000.0 )
#define FEDO2(O2)  ( exp( -(8.13-0.54*(20.9-100.0*CLAMP(O2,0.0,0.2))) )/60.0 ) 
#define HVCO2(CO2) (exp(0.1930*CLAMP(CO2,0.0,0.1)*100.0+2.0004)/7.1)

  ASSERT(fedinfo!=NULL);
  ASSERT(file_index>=0);
  if(file_type==FED_SLICE){
    slicedata *slicei;

    ASSERT(file_index<nsliceinfo);
    slicei = sliceinfo + file_index;
    fedi = slicei->fedptr;
  }
  else if(file_type==FED_ISO){
    isodata *isoi;

    ASSERT(file_index<nisoinfo);
    isoi = isoinfo + file_index;
    fedi = isoi->fedptr;
  }
  else{
    return;
  }
  o2=fedi->o2;
  co2=fedi->co2;
  co=fedi->co;
  fed_slice=fedi->fed_slice;
  fed_iso=fedi->fed_iso;
  meshi = meshinfo + fed_slice->blocknumber;
  ibar = meshi->ibar;
  jbar = meshi->jbar;
  kbar = meshi->kbar;

  switch (fed_slice->idir){
    case 1:
      xgrid = meshi->yplt;
      ygrid = meshi->zplt;
      nx = meshi->jbar+1;
      ny = meshi->kbar+1;
      nxy = nx*ny;
      break;
    case 2:
      xgrid = meshi->xplt;
      ygrid = meshi->zplt;
      nx = meshi->ibar+1;
      ny = meshi->kbar+1;
      nxy = nx*ny;
      break;
    case 3:
      xgrid = meshi->xplt;
      ygrid = meshi->yplt;
      nx = meshi->ibar+1;
      ny = meshi->jbar+1;
      nxy = nx*ny;
      break;
    default:
      ASSERT(0);
      break;
  }

  if(file_type==FED_SLICE){
    readslice(fed_slice->file,fedi->fed_index,UNLOAD,&error_local);
  }
  else if(file_type==FED_ISO){
    readiso_orig(fed_iso->file,file_index,UNLOAD,&error_local);
  }

  if(flag==UNLOAD)return;

  // regenerate if either the FED slice or isosurface file does not exist or is older than
  // either the CO, CO2 or O2 slice files
  
  if(regenerate_fed==1||
     (file_type==FED_SLICE&&(is_file_newer(fed_slice->file,o2->file)!=1||
                             is_file_newer(fed_slice->file,co2->file)!=1||
                             is_file_newer(fed_slice->file,co->file)!=1))||
     (file_type==FED_ISO&&(is_file_newer(fed_iso->file,o2->file)!=1||
                           is_file_newer(fed_iso->file,co2->file)!=1||
                           is_file_newer(fed_iso->file,co->file)!=1))){
    int i,j,k;
    int frame_size;
    float *fed_frame,*fed_framem1;
    float *fed_o2_frame=NULL, *fed_co_frame=NULL;
    float *o2_frame1,*o2_frame2;
    float *co2_frame1,*co2_frame2;
    float *co_frame1,*co_frame2;
    float *times;
    char fed_area_file[1024],*ext;
    FILE *AREA_STREAM=NULL;
    float area_factor;
    float *contour_areas,*mslice_contour_areas;
    multislicedata *mslicei;

    switch (fed_slice->idir){
      case 1:
        NewMemory((void **)&iblank,jbar*kbar*sizeof(char));
        for(j=0;j<jbar;j++){
          for(k=0;k<kbar;k++){
            iblank[k+j*kbar]=meshi->c_iblank_x[IJKNODE(fed_slice->is1,j,k)];
          }
        }
        break;
      case 2:
        NewMemory((void **)&iblank,ibar*kbar*sizeof(char));
        for(i=0;i<ibar;i++){
          for(k=0;k<kbar;k++){
            iblank[k+i*kbar]=meshi->c_iblank_y[IJKNODE(i,fed_slice->js1,k)];
          }
        }
        break;
      case 3:
        NewMemory((void **)&iblank,ibar*jbar*sizeof(char));
        for(i=0;i<ibar;i++){
          for(j=0;j<jbar;j++){
            iblank[j+i*jbar]=meshi->c_iblank_z[IJKNODE(i,j,fed_slice->ks1)];
          }
        }
        break;
      default:
        ASSERT(0);
        break;
    }
    printf("\n");
    printf("generating FED slice data\n");
    strcpy(fed_area_file,fed_slice->file);
    ext=strrchr(fed_area_file,'.');
    if(ext!=NULL){
      *ext=0;
      strcat(fed_area_file,"_area.csv");
      AREA_STREAM=fopen(fed_area_file,"w");
      area_factor=xyzmaxdiff*xyzmaxdiff;
    }
    if(Creadslice_frame(0,fedi->o2_index,LOAD)<0||
       Creadslice_frame(0,fedi->co2_index,LOAD)<0||
       Creadslice_frame(0,fedi->co_index,LOAD)<0){

       readfed(file_index,UNLOAD, file_type, errorcode);
       return;
    }

    fed_slice->is1=co->is1; // nx = is2 + 1 - is1
    fed_slice->is2=co->is2;
    fed_slice->js1=co->js1;
    fed_slice->js2=co->js2;
    fed_slice->ks1=co->ks1;
    fed_slice->ks2=co->ks2;
    fed_slice->nslicei=co->nslicei;
    fed_slice->nslicej=co->nslicej;
    fed_slice->nslicek=co->nslicek;
    fed_slice->volslice=co->volslice;
    if(fed_slice->volslice==1){
      if(fed_slice->nslicei!=fed_slice->is2+1-fed_slice->is1)fed_slice->is2=fed_slice->nslicei+fed_slice->is1-1;
      if(fed_slice->nslicej!=fed_slice->js2+1-fed_slice->js1)fed_slice->js2=fed_slice->nslicej+fed_slice->js1-1;
      if(fed_slice->nslicek!=fed_slice->ks2+1-fed_slice->ks1)fed_slice->ks2=fed_slice->nslicek+fed_slice->ks1-1;
    }
    fed_slice->ntimes=MIN(co->ntimes,co2->ntimes);
    fed_slice->ntimes=MIN(fed_slice->ntimes,o2->ntimes);
    frame_size = fed_slice->nslicei*fed_slice->nslicej*fed_slice->nslicek;
    fed_slice->nslicetotal=frame_size*fed_slice->ntimes;

    mslicei = fed_slice->mslice;
    if(mslicei->contour_areas==NULL){
      NewMemory((void **)&mslicei->contour_areas,4*sizeof(float)*fed_slice->ntimes);
      for(i=0;i<4*fed_slice->ntimes;i++){
        mslicei->contour_areas[i]=0.0;
      }
    }
    mslice_contour_areas=mslicei->contour_areas;

    if(NewMemory((void **)&fed_slice->qslicedata,sizeof(float)*frame_size*fed_slice->ntimes)==0||
       NewMemory((void **)&fed_slice->times,sizeof(float)*fed_slice->ntimes)==0||
       NewMemory((void **)&fed_slice->contour_areas,4*sizeof(float)*fed_slice->ntimes)==0
       ){
       readfed(file_index,UNLOAD, file_type, errorcode);
      *errorcode=-1;
    }
    contour_areas = fed_slice->contour_areas;

    times=fed_slice->times;
    fed_frame=fed_slice->qslicedata;

    NewMemory((void **)&fed_o2_frame,frame_size*sizeof(float));
    NewMemory((void **)&fed_co_frame,frame_size*sizeof(float));
    for(i=0;i<frame_size;i++){
      fed_o2_frame[i]=0.0;
      fed_co_frame[i]=0.0;
    }

    o2_frame1=o2->qslicedata;
    o2_frame2=o2_frame1+frame_size;

    co2_frame1=co2->qslicedata;
    co2_frame2=co2_frame1+frame_size;

    co_frame1=co->qslicedata;
    co_frame2=co_frame1+frame_size;

    times[0]=co2->times[0];
    for(i=0;i<frame_size;i++){
      fed_frame[i]=0.0;
    }
    if(AREA_STREAM!=NULL){
      if(fed_contours==NULL){
        NewMemory((void **)&fed_contours,sizeof(contour));
      }
      else{
        freecontour(fed_contours);
      }
      initcontour(fed_contours,NULL,nlevels);
      getcontours(xgrid, ygrid, nx, ny, fed_frame, iblank, levels, GET_AREAS, fed_contours);
      areas = fed_contours->areas;
      fprintf(AREA_STREAM,"\"time\",\"0.0->0.3\",\"0.3->1.0\",\"1.0->3.0\",\"3.0->\"\n");
      fprintf(AREA_STREAM,"%f,%f,%f,%f,%f\n",
        times[0],(areas[3]+areas[0])*area_factor,areas[1]*area_factor,areas[2]*area_factor,areas[4]*area_factor);
      contour_areas[0]=(areas[0]+areas[3])*area_factor;
      contour_areas[1]=areas[1]*area_factor;
      contour_areas[2]=areas[2]*area_factor;
      contour_areas[3]=areas[4]*area_factor;
      mslice_contour_areas[0]+=contour_areas[0];
      mslice_contour_areas[1]+=contour_areas[1];
      mslice_contour_areas[2]+=contour_areas[2];
      mslice_contour_areas[3]+=contour_areas[3];
      mslice_contour_areas+=4;
      contour_areas+=4;
    }
    for(i=1;i<fed_slice->ntimes;i++){
      int jj;
      float dt;

      if(Creadslice_frame(i,fedi->o2_index,LOAD)<0||
         Creadslice_frame(i,fedi->co2_index,LOAD)<0||
         Creadslice_frame(i,fedi->co_index,LOAD)){
         readfed(file_index, UNLOAD, file_type,errorcode);
         return;
      }

      times[i]=co2->times[0];
      printf("generating FED time=%.2f\n",times[i]);
      dt = (times[i]-times[i-1]); 

      fed_framem1=fed_frame;
      fed_frame+=frame_size;
      for(jj=0;jj<frame_size;jj++){
        float val1, val2;

        val1=FEDCO(co_frame1[jj])*HVCO2(co2_frame1[jj]);
        val2=FEDCO(co_frame2[jj])*HVCO2(co2_frame2[jj]);
        fed_co_frame[jj] += fed_co_frame[jj] + (val1+val2)*dt/2.0;
        
        val1=FEDO2(o2_frame1[jj]);
        val2=FEDO2(o2_frame2[jj]);
        fed_o2_frame[jj] += (val1+val2)*dt/2.0;

        fed_frame[jj] = fed_framem1[jj] + fed_co_frame[jj] + fed_o2_frame[jj];
      }
      if(fed_slice->volslice==0){

      // compute fed areas 

        if(fed_contours==NULL){
          NewMemory((void **)&fed_contours,sizeof(contour));
        }
        else{
          freecontour(fed_contours);
        }
        initcontour(fed_contours,NULL,nlevels);
        getcontours(xgrid, ygrid, nx, ny, fed_frame, iblank, levels, GET_AREAS, fed_contours);
        areas = fed_contours->areas;
        if(AREA_STREAM!=NULL){
          fprintf(AREA_STREAM,"%f,%f,%f,%f,%f\n",
            times[i],(areas[3]+areas[0])*area_factor,areas[1]*area_factor,areas[2]*area_factor,areas[4]*area_factor);
          contour_areas[0]=(areas[0]+areas[3])*area_factor;
          contour_areas[1]=areas[1]*area_factor;
          contour_areas[2]=areas[2]*area_factor;
          contour_areas[3]=areas[4]*area_factor;
          mslice_contour_areas[0]+=contour_areas[0];
          mslice_contour_areas[1]+=contour_areas[1];
          mslice_contour_areas[2]+=contour_areas[2];
          mslice_contour_areas[3]+=contour_areas[3];
          mslice_contour_areas+=4;
          contour_areas+=4;
        }
      }
    }
    FREEMEMORY(iblank);
    if(AREA_STREAM!=NULL)fclose(AREA_STREAM);
    if(fed_contours!=NULL){
      freecontour(fed_contours);
      FREEMEMORY(fed_contours);
    }
    out_slicefile(fed_slice);
    if(fed_slice->volslice==1){
      float *xplt, *yplt, *zplt;
      char *iblank_cell;
      char longlabel[50],shortlabel[50],unitlabel[50];
      char *isofile;
      int error_local2;
      int reduce_triangles=1;
      int nz;

      strcpy(longlabel,"Fractional effective dose");
      strcpy(shortlabel,"FED");
      strcpy(unitlabel," ");

      xplt = meshi->xplt;
      yplt = meshi->yplt;
      zplt = meshi->zplt;
      ibar = meshi->ibar;
      jbar = meshi->jbar;
      kbar = meshi->kbar;
      nx = ibar + 1;
      ny = jbar + 1;
      nz = kbar + 1;
      isofile=fed_iso->file;

      iblank_cell = meshi->c_iblank_cell;
  
      CCisoheader(isofile,longlabel,shortlabel,unitlabel,fed_iso->levels,&fed_iso->nlevels,&error_local2);
      printf("generating FED isosurface\n");
      for(i=0;i<fed_slice->ntimes;i++){
        float *vals;

        vals = fed_slice->qslicedata + i*frame_size;
        printf("outputting isotime time=%.2f\n",times[i]);

//    C_val(i,j,k) = i*nj*nk + j*nk + k
// Fort_val(i,j,k) = i + j*ni + k*ni*nj

        CCisosurface2file(isofile, times+i, vals, iblank_cell, 
		              fed_iso->levels, &fed_iso->nlevels,
                  xplt, &nx,  yplt, &ny, zplt, &nz, 
                  &reduce_triangles, &error_local2);
      }
    }
    FREEMEMORY(fed_o2_frame);
    FREEMEMORY(fed_co_frame);
    FREEMEMORY(fed_slice->qslicedata);
    FREEMEMORY(fed_slice->times);
    Creadslice_frame(0,fedi->o2_index,UNLOAD);
    Creadslice_frame(0,fedi->co2_index,UNLOAD);
    Creadslice_frame(0,fedi->co_index,UNLOAD);
  }
  if(file_type==FED_SLICE){
    readslice(fed_slice->file,fedi->fed_index,flag,&error_local);
  }
  else{
    readiso_orig(fed_iso->file,file_index,flag,&error_local);
  }
  printf("completed\n");
}

/* ------------------ readvslice ------------------------ */

void readvslice(int ivslice, int flag, int *errorcode){
  vslicedata *vd,*vslicei;
  slicedata *u=NULL,*v=NULL,*w=NULL,*val=NULL;
  float valmin, valmax;
  int display;
  int i;

  valmin = 1000000000.0;
  valmax = -valmin;
  vd = vsliceinfo + ivslice;
  vd->u=NULL;
  vd->v=NULL;
  vd->w=NULL;
  vd->val=NULL;
  if(flag==UNLOAD){
    if(vd->loaded==0)return;
    if(vd->iu!=-1){
      u = sliceinfo + vd->iu;
      display=u->display;
      if(u->loaded==1)readslice(u->file,vd->iu,UNLOAD,errorcode);
      u->display=display;
      u->vloaded=0;
    }
    if(vd->iv!=-1){
      v = sliceinfo + vd->iv;
      display=v->display;
      if(v->loaded==1)readslice(v->file,vd->iv,UNLOAD,errorcode);
      v->display=display;
      v->vloaded=0;
    }
    if(vd->iw!=-1){
      w = sliceinfo + vd->iw;
      display=w->display;
      if(w->loaded==1)readslice(w->file,vd->iw,UNLOAD,errorcode);
      w->display=display;
      w->vloaded=0;
    }
    if(vd->ival!=-1){
      val = sliceinfo + vd->ival;
      display=val->display;
      if(val->loaded==1)readslice(val->file,vd->ival,UNLOAD,errorcode);
      val->display=display;
      val->vloaded=0;
    }
    remove_vslice_loadstack(ivslice);
    vd->loaded=0;
    vd->display=0;
    showvslice=0;
    updatemenu=1;
    plotstate=getplotstate(DYNAMIC_PLOTS);
    return;
  }
  if(vd->iu!=-1){
    u = sliceinfo + vd->iu;
    vd->u=u;
    readslice(u->file,vd->iu,LOAD,errorcode);
    if(*errorcode!=0){
      vd->loaded=1;
      printf("error loading %s, vector load aborted\n",u->file);
      readvslice(ivslice,UNLOAD,errorcode);
      *errorcode=1;
      return;
    }
    if(u->valmin<valmin)valmin=u->valmin;
    if(u->valmax>valmax)valmax=u->valmax;
    u->display=0;
    u->reload=0;
    u->vloaded=1;
  }
  if(vd->iv!=-1){
    v = sliceinfo + vd->iv;
    vd->v=v;
    readslice(v->file,vd->iv,LOAD,errorcode);
    if(*errorcode!=0){
      printf("error loading %s, vector load aborted\n",v->file);
      vd->loaded=1;
      readvslice(ivslice,UNLOAD,errorcode);
      *errorcode=1;
      return;
    }

    if(v->valmin<valmin)valmin=v->valmin;
    if(v->valmax>valmax)valmax=v->valmax;
    v->display=0;
    v->reload=0;
    v->vloaded=1;
  }
  if(vd->iw!=-1){
    w = sliceinfo + vd->iw;
    vd->w=w;
    readslice(w->file,vd->iw,LOAD,errorcode);
    if(*errorcode!=0){
      printf("error loading %s, vector load aborted\n",w->file);
      vd->loaded=1;
      readvslice(ivslice,UNLOAD,errorcode);
      *errorcode=1;
      return;
    }

    if(w->valmin<valmin)valmin=w->valmin;
    if(w->valmax>valmax)valmax=w->valmax;
    w->display=0;
    w->reload=0;
    w->vloaded=1;
  }
  vd->type=-1;
  if(vd->ival!=-1){
    val = sliceinfo + vd->ival;
    vd->val=val;
    readslice(val->file,vd->ival,LOAD,errorcode);
    if(*errorcode!=0){
      printf("error loading %s, vector load aborted\n",val->file);
      vd->loaded=1;
      readvslice(ivslice,UNLOAD,errorcode);
      *errorcode=1;
      return;
    }
    islicetype=getslicetype(val);
    vd->type=val->type;
    vd->valmin=valmin;
    vd->valmax=valmax;
    val->display=0;
    val->vloaded=1;
    val->reload=0;
  }
  vd->display=1;
  vd->loaded=1;
  plotstate=getplotstate(DYNAMIC_PLOTS);
  updatemenu=1;
  updatetimes();

  valmax=-100000.0;
  valmin=100000.0;
  for(i=0;i<nvsliceinfo;i++){
    vslicei = vsliceinfo + i;
    if(vslicei->loaded==0)continue;
    if(vslicei->iu!=-1){
      u=sliceinfo + vslicei->iu;
      if(u->valmin<valmin)valmin=u->valmin;
      if(u->valmax>valmax)valmax=u->valmax;
    }
    if(vslicei->iv!=-1){
      v=sliceinfo + vslicei->iv;
      if(v->valmin<valmin)valmin=v->valmin;
      if(v->valmax>valmax)valmax=v->valmax;
    }
    if(vslicei->iw!=-1){
      w=sliceinfo + vslicei->iw;
      if(w->valmin<valmin)valmin=w->valmin;
      if(w->valmax>valmax)valmax=w->valmax;
    }
  }
  velocity_range = valmax - valmin;
  push_vslice_loadstack(ivslice);
  
#ifdef _DEBUG
  printf("After vslice load: ");
  PrintMemoryInfo;
#endif
  Idle_CB();
}

/* ------------------ readslice ------------------------ */

void readslice(char *file, int ifile, int flag, int *errorcode){
  FILE_SIZE slicefilelen;
  float *xplt_local, *yplt_local, *zplt_local;
  int blocknumber;
  int error;
  float offset;
  int i;
  int nx, ny, nxy;
  int ii;
  float qmin, qmax;
  int headersize, framesize;
  char slicelonglabels[31], sliceshortlabels[31], sliceunits[31];
  slicedata *sd;
  vslicedata *vd;
  int flag2=0;
  mesh *meshi;
  int local_starttime=0, local_stoptime=0;
  FILE_SIZE file_size=0;
  int local_starttime0=0, local_stoptime0=0;  
  float delta_time, delta_time0;
#ifdef _DEBUG
  int num_memblocks_load,num_memblocks_unload;
#endif
#ifdef pp_memstatus
  unsigned int availmemory;
#endif

  CheckMemory;
  local_starttime0 = glutGet(GLUT_ELAPSED_TIME);  
  *errorcode=0;
  error=0;
  show_slice_average=0;
  blocknumber = sliceinfo[ifile].blocknumber;
  meshi=meshinfo+blocknumber;

  slicefilenumber = ifile;
  slicefilenum=ifile;

  ASSERT(slicefilenumber>=0&&slicefilenumber<nsliceinfo);
  sd = sliceinfo + slicefilenumber;
  CountMemoryBlocks(num_memblocks_load,0);
  if(flag!=RESETBOUNDS){
    if(sd->loaded==0&&flag==UNLOAD)return;
    sd->display=0;
#ifdef pp_MEMDEBUG    
    if(sd->qslicedata!=NULL){
      ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicetotal));
    }
#endif
    if(sd->qslicedata!=NULL){
      FreeMemory(sd->qslicedata);
      sd->qslicedata=NULL;
    }
    FREEMEMORY(sd->times  );
    FREEMEMORY(sd->slicelevel  );
    FREEMEMORY(sd->compindex);
    FREEMEMORY(sd->qslicedata_compressed);
    FREEMEMORY(sd->slicecomplevel);
    slicefilenum=ifile;

    if(flag==UNLOAD){

      sd->ntimes=0;
      updatemenu=1;
      sd->loaded=0;
      sd->vloaded=0;
      sd->display=0;
      plotstate = getplotstate(DYNAMIC_PLOTS);
      ReadVolSlice=0;
      for(ii=0;ii<nslice_loaded;ii++){
        slicedata *sdi;

        i = slice_loaded_list[ii];
        sdi = sliceinfo+i;
        if(sdi->volslice==1)ReadVolSlice=1;
      }
      for(ii=0;ii<nslice_loaded;ii++){
        slicedata *sdi;

        i = slice_loaded_list[ii];
        sdi = sliceinfo+i;
        if(sdi->type==islicetype){
          slicefilenum=i;
          flag2=1;
          break;
        }
      }
      if(flag2==0){
        for(ii=0;ii<nslice_loaded;ii++){
          slicedata *sdi;

          i = slice_loaded_list[ii];
          sdi = sliceinfo+i;
          if(sdi->type!=islicetype){
            slicefilenum=i;
            flag2=1;
            break;
          }
        }
      }
      if(flag2==0){
        slicefilenum=0;
        islicetype=0;
      }

      for(i=0;i<nvsliceinfo;i++){
        vd = vsliceinfo + i;
        if(vd->iu==ifile)vd->u=NULL;
        if(vd->iv==ifile)vd->v=NULL;
        if(vd->iw==ifile)vd->w=NULL;
        if(vd->u==NULL&&vd->v==NULL&&vd->w==NULL){
          vd->loaded=0;
          vd->display=0;
        }
        if(vd->ival==ifile){
          vd->val=NULL;
          vd->loaded=0;
          vd->display=0;
        }
      }
      if(sd->compression_type==0){
        updateslicebounds();
        list_slice_index=islicetype;
        setslicebounds(islicetype);
        updateallslicecolors(islicetype,errorcode);
      }
      else{
        updateallslicelabels(islicetype,errorcode);
      }

      updateglui();
      update_unit_defs();
      updatetimes();
#ifdef pp_MEMDEBUG
      printf("After slice unload: ");
      PrintMemoryInfo;
      CountMemoryBlocks(num_memblocks_unload,num_memblocks_load);
#endif
      remove_slice_loadstack(slicefilenumber);
      return;
    }
    CountMemoryBlocks(num_memblocks_load,0);
    file_size=get_filesize(file);

    slicefilelen = strlen(file);
    if(sd->compression_type==0){
      FORTgetslicesizes(file, &sd->nslicei, &sd->nslicej, &sd->nslicek, &sd->ntimes, &sliceframestep, &endian_smv,&error,
        &settmin_s, &settmax_s, &tmin_s, &tmax_s, &headersize, &framesize,
        slicefilelen);
    }
    else if(sd->compression_type==1){
      if(
        getsliceheader(sd->comp_file,sd->size_file,sd->compression_type,
                       sliceframestep,settmin_s,settmax_s,tmin_s,tmax_s,
                       &sd->nslicei, &sd->nslicej, &sd->nslicek, &sd->ntimes, &sd->ncompressed, &sd->valmin, &sd->valmax)==0){
        readslice("",ifile,UNLOAD,&error);
        *errorcode=1;
        return;
      }
    }
    if(sd->nslicei!=1&&sd->nslicej!=1&&sd->nslicek!=1){
      sd->volslice=1;
      ReadVolSlice=1;
    }
    if(error!=0){
      readslice("",ifile,UNLOAD,&error);
      *errorcode=1;
      return;
    }
    if(settmax_s==0&&settmin_s==0&&sd->compression_type==0){
      sd->ntimes = (getfilesize(file)-headersize)/framesize;
      if(sliceframestep>1)sd->ntimes/=sliceframestep;
    }
    if(error!=0||sd->ntimes<1){
      readslice("",ifile,UNLOAD,&error);
      *errorcode=1;
      return;
    }
    printf("Loading slice data: %s\n",file);
    MEMSTATUS(1,&availmemory,NULL,NULL);
    local_starttime = glutGet(GLUT_ELAPSED_TIME);
    if(sd->compression_type==1){
      char *datafile;

      if(NewMemory((void **)&sd->qslicedata_compressed,sd->ncompressed)==0||
         NewMemory((void **)&sd->times,sizeof(float)*sd->ntimes)==0||
         NewMemory((void **)&sd->compindex,sizeof(compinfo)*(1+sd->ntimes))==0
         ){
        readslice("",ifile,UNLOAD,&error);
        *errorcode=1;
        return;
      }
      datafile = sd->comp_file;
      if(getslicecompresseddata(datafile,
        settmin_s,settmax_s,tmin_s,tmax_s,sd->ncompressed,sliceframestep,sd->ntimes,
        sd->times,sd->qslicedata_compressed,sd->compindex,&sd->globalmin,&sd->globalmax)==0){
        readslice("",ifile,UNLOAD,&error);
        *errorcode=1;
        return;
      }
    }
    else{
      FILE_SIZE labellen=LABELLEN;
      int file_unit=15;

      if(NewMemory((void **)&sd->qslicedata,sizeof(float)*sd->nslicei*sd->nslicej*sd->nslicek*sd->ntimes)==0||
         NewMemory((void **)&sd->times,sizeof(float)*sd->ntimes)==0){
        *errorcode=1;
        readslice("",ifile,UNLOAD,&error);
        return;
      }
#ifdef pp_MEMDEBUG      
      ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicei*sd->nslicej*sd->nslicek*sd->ntimes));
#endif      
      FORTget_file_unit(&file_unit,&file_unit);
      FORTgetslicedata(&file_unit,file,slicelonglabels,sliceshortlabels,sliceunits,
                   &sd->is1,&sd->is2,&sd->js1,&sd->js2,&sd->ks1,&sd->ks2,&sd->idir,
                   &qmin,&qmax,sd->qslicedata,sd->times,&sd->ntimes,&sliceframestep, &endian_smv,
                   &settmin_s,&settmax_s,&tmin_s,&tmax_s,
                   slicefilelen,labellen,labellen,labellen);
#ifdef pp_MEMDEBUG                   
      ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicei*sd->nslicej*sd->nslicek*sd->ntimes));
#endif      
    }
    local_stoptime = glutGet(GLUT_ELAPSED_TIME);
    delta_time = (local_stoptime-local_starttime)/1000.0;    

    if(slice_average_flag==1){
      int data_per_timestep;
      int ndata;
      int ntimes_local;

      data_per_timestep=sd->nslicei*sd->nslicej*sd->nslicek;
      ntimes_local=sd->ntimes;
      ndata = data_per_timestep*ntimes_local;
      show_slice_average=1;

      if(
        sd->compression_type==1||
        sd->compression_type==2||
        average_slice_data(sd->qslicedata,sd->qslicedata,ndata,data_per_timestep,sd->times,ntimes_local,slice_average_interval)==1
        ){
        show_slice_average=0; // averaging failed
      }
    }

  /*  initialize slice data */

    sd->nslicetotal=0;
    sd->nsliceii = 0;
    if(sd->ntimes==0)return;

  /* estimate the slice offset, the distance to move a slice so
     that it does not "interfere" with an adjacent block */

    blocknumber = sliceinfo[ifile].blocknumber;
    xplt_local=meshinfo[blocknumber].xplt;
    yplt_local=meshinfo[blocknumber].yplt;
    zplt_local=meshinfo[blocknumber].zplt;

    xslicemid = (xplt_local[sd->is1]+xplt_local[sd->is2])/2.0;
    yslicemid = (yplt_local[sd->js1]+yplt_local[sd->js2])/2.0;
    zslicemid = (zplt_local[sd->ks1]+zplt_local[sd->ks2])/2.0;

    sd->sliceoffset=0.0;

    switch (sd->idir) {
     case 1:
      offset=sliceoffset_factor*(xplt_local[1]-xplt_local[0]);
      if(inblockage(meshi,xslicemid-offset,yslicemid,zslicemid)==1){
        sd->sliceoffset=offset;
      }
      if(inblockage(meshi,xslicemid+offset,yslicemid,zslicemid)==1){
        sd->sliceoffset=-offset;
      }
      sd->nslicex=sd->js2+1-sd->js1;
      sd->nslicey=sd->ks2+1-sd->ks1;
      break;
     case 2:
      offset = sliceoffset_factor*(yplt_local[1]-yplt_local[0]);
      if(inblockage(meshi,xslicemid,yslicemid-offset,zslicemid)==1){
        sd->sliceoffset=offset;
      }
      if(inblockage(meshi,xslicemid,yslicemid+offset,zslicemid)==1){
        sd->sliceoffset=-offset;
      }
      sd->nslicex=sd->is2+1-sd->is1;
      sd->nslicey=sd->ks2+1-sd->ks1;
      break;
     case 3:
      offset=sliceoffset_factor*(zplt_local[1]-zplt_local[0]);
      if(inblockage(meshi,xslicemid,yslicemid,zslicemid-offset)==1){
        sd->sliceoffset=offset;
      }
      if(inblockage(meshi,xslicemid,yslicemid,zslicemid+offset)==1){
        sd->sliceoffset=-offset;
      }
      sd->nslicex=sd->is2+1-sd->is1;
      sd->nslicey=sd->js2+1-sd->js1;
      break;
     default:
       ASSERT(FFALSE);
       break;
    }

    sd->nsliceii = sd->nslicei*sd->nslicej*sd->nslicek;
    sd->nslicetotal=sd->ntimes*sd->nsliceii;
    if(sd->compression_type==1||sd->compression_type==2){
      if(NewMemory((void **)&sd->slicecomplevel,sd->nsliceii*sizeof(unsigned char))==0){
        readslice("",ifile,UNLOAD,&error);
        *errorcode=1;
        return;
      }
    }
    else{
      if(NewMemory((void **)&sd->slicelevel,sd->nslicetotal*sizeof(int))==0){
        readslice("",ifile,UNLOAD,&error);
        *errorcode=1;
        return;
      }
    }

    nx = meshi->ibar + 1;
    ny = meshi->jbar + 1;
    nxy = nx*ny;

#ifdef pp_MEMDEBUG
    if(sd->compression_type==0){
      ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicetotal));
    }
#endif
  }  /* RESETBOUNDS */

 /* convert slice points into integers pointing to an rgb color table */
  if(sd->compression_type==0){
    getslicedatabounds(sd,&qmin,&qmax);
  }
  else{
    qmin=sd->valmin;
    qmax=sd->valmax;
  }
  sd->globalmin=qmin;
  sd->globalmax=qmax;
  if(sd->compression_type==0){
    adjustslicebounds(sd,&qmin,&qmax);
  }
  sd->valmin=qmin;
  sd->valmax=qmax;
  sd->valmin_data=qmin;
  sd->valmax_data=qmax;
  for(i=0;i<256;i++){
    sd->qval256[i] = (qmin*(255-i) + qmax*i)/255;
  }
  CheckMemory;

  if(sd->slicetype==SLICE_CENTER){
    usetexturebar=0;
  }
  sd->loaded=1;
  if(sd->vloaded==0)sd->display=1;
  islicetype=getslicetype(sd);
  plotstate=getplotstate(DYNAMIC_PLOTS);
  update_unit_defs();
  updatetimes();
  CheckMemory;

  if(sd->compression_type==0){
    updateslicebounds();
    updateallslicecolors(islicetype,errorcode);
    list_slice_index=islicetype;
    setslicebounds(islicetype);
  }
  else{
    slicebounds[islicetype].valmin_data=qmin;
    slicebounds[islicetype].valmax_data=qmax;
    updateallslicelabels(islicetype,errorcode);
  }
  CheckMemory;

  updateslicelist(list_slice_index);
  CheckMemory;
  updateslicelistindex(slicefilenum);
  CheckMemory;
  updateglui();
  CheckMemory;
#ifdef pp_MEMDEBUG
  if(sd->compression_type==0){
    ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicei*sd->nslicej*sd->nslicek*sd->ntimes));
  }
  CheckMemory;
  printf("After slice file load: ");
  CountMemoryBlocks(sd->num_memblocks,num_memblocks_load);
  PrintMemoryInfo;
#endif
  Idle_CB();

  exportdata=1;
  if(exportdata==0){
    FREEMEMORY(sd->qslicedata);
  }
  
  local_stoptime0 = glutGet(GLUT_ELAPSED_TIME);
  delta_time0=(local_stoptime0-local_starttime0)/1000.0;

  if(flag!=RESETBOUNDS){
    if(file_size!=0&&delta_time>0.0){
      float loadrate;

      loadrate = ((float)file_size*8.0/1000000.0)/delta_time;
      printf(" %.1f MB loaded in %.2f s - rate: %.1f Mb/s (overhead: %.2f s)\n",
      (float)file_size/1000000.,delta_time,loadrate,delta_time0-delta_time);
    }
    else{
      printf(" %.1f MB downloaded in %.2f s (overhead: %.2f s)",
      (float)file_size/1000000.,delta_time,delta_time0-delta_time);
    }
  }

  if(update_fire_line==0&&strcmp(sd->label.shortlabel,"Fire line")==0){
    update_fire_line=1;
  }

  if(colorbartype_ini==-1){
    if(strcmp(sd->label.shortlabel,"thick")==0){
      ColorBarMenu(wallthickness_colorbar);
    }
    if(strcmp(sd->label.shortlabel,"phifield")==0){
      ColorBarMenu(levelset_colorbar);
    }
  }
  push_slice_loadstack(slicefilenumber);

    if(sd->volslice==1){
      mesh *meshj;

      meshj = meshinfo + sd->blocknumber;
      show_gslice_data=1;
#ifdef pp_GPU
      if(gpuactive==1){
        init_slice3d_texture(meshj);
      }
#endif
    }

  glutPostRedisplay();
}
/* ------------------ outputslicebounds ------------------------ */
/*
void outputslicebounds(void){
  int i;
  slicedata *sd;
  databounds *db;

  for(i=0;i<nslice;i++){
    sd = sliceinfo +i;
    printf("i=%i %i %f %i %f \n",
      i,sd->setvalmin,sd->valmin,
        sd->setvalmax,sd->valmax);
  }
  printf("\n");
  for(i=0;i<nslice2;i++){
    db = slicebounds +i;
    printf("i=%i %i %f %i %f \n",
      i,db->setvalmin,db->valmin,
        db->setvalmax,db->valmax);
  }
  printf("slicefilenum=%i\n",slicefilenum);
  printf("list_slice_index=%i\n",list_slice_index);
  printf("setslicemin=%i slicemin=%f setslicemax=%i slicemax=%f\n",
    setslicemin,slicemin,setslicemax,slicemax);
}
*/

/* ------------------ updateslicefilenum ------------------------ */

void updateslicefilenum(void){
  slicedata *sd;
  int i;
  int ii;

  for(ii=0;ii<nslice_loaded;ii++){
    i = slice_loaded_list[ii];
    sd = sliceinfo+i;
    if(sd->display==0||islicetype!=sd->type)continue;
    slicefilenum=i;
    break;
  }
}

/* ------------------ updateslicebounds ------------------------ */

void updateslicebounds(void){
  int i, j;
  float valmin, valmax;
  float valmin_data, valmax_data;
  int minflag, maxflag;
  int minflag2, maxflag2;
  int jj;

  for(i=0;i<nslice2;i++){
    minflag=0; maxflag=0;
    minflag2=0; maxflag2=0;
    for(jj=0;jj<nslice_loaded;jj++){
      j = slice_loaded_list[jj];
      if(sliceinfo[j].type!=i)continue;
      if(slicebounds[i].setvalmin!=SET_MIN){
        if(minflag==0){
          valmin=sliceinfo[j].valmin;
          minflag=1;
        }
        else{
          if(sliceinfo[j].valmin<valmin)valmin=sliceinfo[j].valmin;
        }
      }
      if(minflag2==0){
        valmin_data=sliceinfo[j].valmin_data;
        minflag2=1;
      }
      else{
        if(sliceinfo[j].valmin_data<valmin_data)valmin_data=sliceinfo[j].valmin_data;
      }
    }
    for(jj=0;jj<nslice_loaded;jj++){
      j = slice_loaded_list[jj];
      if(sliceinfo[j].type!=i)continue;
      if(slicebounds[i].setvalmax!=SET_MAX){
        if(maxflag==0){
          valmax=sliceinfo[j].valmax;
          maxflag=1;
        }
        else{
          if(sliceinfo[j].valmax>valmax)valmax=sliceinfo[j].valmax;
        }
      }
      if(maxflag2==0){
        valmax_data=sliceinfo[j].valmax_data;
        maxflag2=1;
      }
      else{
        if(sliceinfo[j].valmax_data>valmax_data)valmax_data=sliceinfo[j].valmax_data;
      }
    }
    if(minflag==1)slicebounds[i].valmin=valmin;
    if(maxflag==1)slicebounds[i].valmax=valmax;
    if(minflag2==1)slicebounds[i].valmin_data=valmin_data;
    if(maxflag2==1)slicebounds[i].valmax_data=valmax_data;
  }
}

/* ------------------ updateallslicelabels ------------------------ */

void updateallslicelabels(int slicetype, int *errorcode){
  int i;
  float valmin, valmax;
  int setvalmin, setvalmax;
  int ii;
  slicedata *sd;

  *errorcode=0;

  setvalmin=slicebounds[slicetype].setvalmin;
  setvalmax=slicebounds[slicetype].setvalmax;
  if(setvalmin==1){
    valmin=slicebounds[slicetype].valmin;
  }
  else{
    valmin=slicebounds[slicetype].valmin_data;
    slicebounds[slicetype].valmin=valmin;
  }
  if(setvalmax==1){
    valmax=slicebounds[slicetype].valmax;
  }
  else{
    valmax=slicebounds[slicetype].valmax_data;
    slicebounds[slicetype].valmax=valmax;
  }
  for(ii=0;ii<nslice_loaded;ii++){
    i = slice_loaded_list[ii];
    sd = sliceinfo + i;
    if(sd->type!=slicetype)continue;
    setslicelabels(valmin,valmax,sd,errorcode);
    if(*errorcode!=0)return;
  }
  setslicebounds(slicetype);
  updateglui();
}

/* ------------------ updateallslicecolors ------------------------ */

void updateallslicecolors(int slicetype, int *errorcode){
  int i;
  float valmin, valmax;
  int setvalmin, setvalmax;
  int ii;
  slicedata *sd;

  *errorcode=0;

  setvalmin=slicebounds[slicetype].setvalmin;
  setvalmax=slicebounds[slicetype].setvalmax;
  if(setvalmin==1){
    valmin=slicebounds[slicetype].valmin;
  }
  else{
    valmin=slicebounds[slicetype].valmin_data;
    slicebounds[slicetype].valmin=valmin;
  }
  if(setvalmax==1){
    valmax=slicebounds[slicetype].valmax;
  }
  else{
    valmax=slicebounds[slicetype].valmax_data;
    slicebounds[slicetype].valmax=valmax;
  }
  for(ii=0;ii<nslice_loaded;ii++){
    i = slice_loaded_list[ii];
    sd = sliceinfo + i;
    if(sd->type!=slicetype)continue;
    setslicecolors(valmin,valmax,sd,errorcode);
    if(*errorcode!=0)return;
  }
  setslicebounds(slicetype);
    updateglui();
}

/* ------------------ slicecompare ------------------------ */

int slicecompare( const void *arg1, const void *arg2 ){
  slicedata *slicei, *slicej;
  float delta_orig;

  slicei = sliceinfo + *(int *)arg1;
  slicej = sliceinfo + *(int *)arg2;

  if(slicei->menu_show>slicej->menu_show)return -1;
  if(slicei->menu_show<slicej->menu_show)return 1;
  if(slicei->mesh_type<slicej->mesh_type)return -1;
  if(slicei->mesh_type>slicej->mesh_type)return 1;
  if(slicei->slicetype<slicej->slicetype)return -1;
  if(slicei->slicetype>slicej->slicetype)return 1;

  delta_orig = MAX(slicei->delta_orig,slicej->delta_orig);

  if( strncmp(slicei->label.longlabel,"VE",2)==0){
    if(
      strncmp(slicej->label.longlabel,"U-",2)==0||
      strncmp(slicej->label.longlabel,"V-",2)==0||
      strncmp(slicej->label.longlabel,"W-",2)==0){
      return -1;
    }
  }
  if(strncmp(slicej->label.longlabel,"VE",2)==0){
    if(
      strncmp(slicei->label.longlabel,"U-",2)==0||
      strncmp(slicei->label.longlabel,"V-",2)==0||
      strncmp(slicei->label.longlabel,"W-",2)==0){
      return 1;
    }
  }
  if(strcmp(slicei->label.longlabel,slicej->label.longlabel)<0)return -1;
  if(strcmp(slicei->label.longlabel,slicej->label.longlabel)>0)return 1;
  if(slicei->volslice>slicej->volslice)return -1;
  if(slicei->volslice<slicej->volslice)return 1;

  if(slicei->idir<slicej->idir)return -1;
  if(slicei->idir>slicej->idir)return 1;
  if(slicei->position_orig+delta_orig<slicej->position_orig)return -1;
  if(slicei->position_orig-delta_orig>slicej->position_orig)return 1;
  if(slicei->blocknumber<slicej->blocknumber)return -1;
  if(slicei->blocknumber>slicej->blocknumber)return 1;
  return 0;
}

/* ------------------ vslicecompare ------------------------ */

int vslicecompare( const void *arg1, const void *arg2 ){
  slicedata *slicei, *slicej;
  vslicedata *vslicei, *vslicej;
  float delta_orig;

  vslicei = vsliceinfo + *(int *)arg1;
  vslicej = vsliceinfo + *(int *)arg2;
  slicei = sliceinfo + vslicei->ival;
  slicej = sliceinfo + vslicej->ival;

  if(slicei->mesh_type<slicej->mesh_type)return -1;
  if(slicei->mesh_type>slicej->mesh_type)return 1;

  if( strncmp(slicei->label.longlabel,"VE",2)==0){
    if(
      strncmp(slicej->label.longlabel,"U-",2)==0||
      strncmp(slicej->label.longlabel,"V-",2)==0||
      strncmp(slicej->label.longlabel,"W-",2)==0){
      return -1;
    }
  }
  if(strncmp(slicej->label.longlabel,"VE",2)==0){
    if(
      strncmp(slicei->label.longlabel,"U-",2)==0||
      strncmp(slicei->label.longlabel,"V-",2)==0||
      strncmp(slicei->label.longlabel,"W-",2)==0){
      return 1;
    }
  }
  if(strcmp(slicei->label.longlabel,slicej->label.longlabel)<0)return -1;
  if(strcmp(slicei->label.longlabel,slicej->label.longlabel)>0)return 1;
  if(slicei->volslice<slicej->volslice)return -1;
  if(slicei->volslice>slicej->volslice)return 1;
  if(slicei->idir<slicej->idir)return -1;
  if(slicei->idir>slicej->idir)return 1;
  delta_orig = MAX(slicei->delta_orig,slicej->delta_orig);
  if(slicei->position_orig+delta_orig<slicej->position_orig)return -1;
  if(slicei->position_orig-delta_orig>slicej->position_orig)return 1;
  if(slicei->blocknumber<slicej->blocknumber)return -1;
  if(slicei->blocknumber>slicej->blocknumber)return 1;
  return 0;
}

/* ------------------ update_slice_menu_show ------------------------ */

void update_slice_menu_show(void){
  int i;

  for(i=0;i<nsliceinfo;i++){
    mesh *slicemesh;
    slicedata *sd;

    sd = sliceinfo + i;
    slicemesh = meshinfo + sd->blocknumber;
    sd->menu_show=1;
    if(sd->slicetype==SLICE_CENTER){
      flowlabels *label;

      label = &sd->label;
      if(strcmp(label->shortlabel,"U-VEL")==0||strcmp(label->shortlabel,"V-VEL")==0||strcmp(label->shortlabel,"W-VEL")==0){
        sd->menu_show=0;
      }
    }
    if(show_evac_slices==0&&slicemesh->mesh_type!=0){
      sd->menu_show=0;
    }
    if(strcmp(sd->label.longlabel,"Direction")==0&&constant_evac_coloring==1){
      sd->constant_color=direction_color_ptr;
    }
    else{
      sd->constant_color=NULL;
    }
  }
}

/* ------------------ updateslicemenulabels ------------------------ */

void updateslicemenulabels(void){
  int i;
  char label[128];
  multislicedata *mslicei;
  slicedata *sd,*sdold;

  update_slice_menu_show();
  if(nsliceinfo>0){
    mslicei = multisliceinfo;
    sd = sliceinfo + sliceorderindex[0];
    STRCPY(mslicei->menulabel,sd->slicedir);
    STRCPY(sd->menulabel,mslicei->menulabel);

    STRCPY(mslicei->menulabel2,sd->label.longlabel);
    STRCAT(mslicei->menulabel2,", ");
    STRCAT(mslicei->menulabel2,sd->menulabel);

    if(nmeshes>1){
      mesh *meshi;
      mesh *slicemesh;
      
      slicemesh = meshinfo + sd->blocknumber;
      sprintf(label,", %s",slicemesh->label);
      STRCAT(sd->menulabel,label);
      meshi = meshinfo + sd->blocknumber;
      if(nevac>0){
        if(meshi->mesh_type==0){
          strcpy(label,", FDS mesh");
        }
        else{
          strcpy(label,", Evacuation mesh");
        }
        STRCAT(mslicei->menulabel2,label);
        STRCAT(mslicei->menulabel,label);
      }
    }
    if(showfiles==1){
      STRCAT(sd->menulabel,", ");
      STRCAT(sd->menulabel,sd->file);
    }
    if(sd->compression_type==1){
      STRCAT(sd->menulabel," (ZLIB)");
    }
    else if(sd->compression_type==2){
      STRCAT(sd->menulabel," (RLE)");
    }
    for(i=1;i<nsliceinfo;i++){
      mesh *meshi;

      sdold = sliceinfo + sliceorderindex[i - 1];
      sd = sliceinfo + sliceorderindex[i];
      STRCPY(sd->menulabel,sd->slicedir);
      if(new_multi_slice(sdold,sd)==1){
        mslicei++;
        STRCPY(mslicei->menulabel,sd->menulabel);
        STRCPY(mslicei->menulabel2,sd->label.longlabel);
        STRCAT(mslicei->menulabel2,", ");
        STRCAT(mslicei->menulabel2,sd->menulabel);
        meshi = meshinfo + sd->blocknumber;
        if(nevac>0){
          if(meshi->mesh_type==0){
            strcpy(label,", FDS mesh");
          }
          else{
            strcpy(label,", Evacuation mesh");
          }
          STRCAT(mslicei->menulabel2,label);
          STRCAT(mslicei->menulabel,label);
        }
      }
      if(nmeshes>1){
        mesh *slicemesh;

        slicemesh = meshinfo + sd->blocknumber;
        sprintf(label,", %s",slicemesh->label);
        STRCAT(sd->menulabel,label);
      }
      if(showfiles==1){
        STRCAT(sd->menulabel,", ");
        STRCAT(sd->menulabel,sd->file);
      }
      if(sd->compression_type==1){
        STRCAT(sd->menulabel," (ZLIB)");
      }
      else if(sd->compression_type==2){
        STRCAT(sd->menulabel," (RLE)");
      }
    } 
    for(i=0;i<nsliceinfo;i++){
      sd = sliceinfo + i;
      STRCPY(sd->menulabel2,sd->label.longlabel);
      STRCAT(sd->menulabel2,", ");
      STRCAT(sd->menulabel2,sd->menulabel);
    }
  }
}

/* ------------------ updatevslicemenulabels ------------------------ */

void updatevslicemenulabels(void){
  int i;
  slicedata *sd, *sdold;
  vslicedata *vsd, *vsdold;
  multivslicedata *mvslicei;
  char label[128];


  if(nvsliceinfo>0){
    mvslicei = multivsliceinfo;
    vsd = vsliceinfo + vsliceorderindex[0];
    sd = sliceinfo + vsd->ival;

    STRCPY(mvslicei->menulabel,sd->slicedir);
    STRCPY(mvslicei->menulabel2,sd->label.longlabel);
    STRCAT(mvslicei->menulabel2,", ");
    STRCAT(mvslicei->menulabel2,sd->slicedir);

    STRCPY(vsd->menulabel,mvslicei->menulabel);
    STRCPY(vsd->menulabel2,mvslicei->menulabel2);
    if(nmeshes>1){
      mesh *slicemesh;

	  slicemesh = meshinfo + sd->blocknumber;
      sprintf(label,", %s",slicemesh->label);
      STRCAT(vsd->menulabel,label);
    }
    if(showfiles==1){
      STRCAT(vsd->menulabel,", ");
      STRCAT(vsd->menulabel,sd->file);
    }
    for(i=1;i<nvsliceinfo;i++){
      vsdold = vsliceinfo + vsliceorderindex[i - 1];
      sdold = sliceinfo + vsdold->ival;
      vsd = vsliceinfo + vsliceorderindex[i];
      sd = sliceinfo + vsd->ival;
      STRCPY(vsd->menulabel,sd->slicedir);
      if(new_multi_slice(sdold,sd)==1){
        mvslicei++;
        STRCPY(mvslicei->menulabel,vsd->menulabel);
        STRCPY(mvslicei->menulabel2,sd->label.longlabel);
        STRCAT(mvslicei->menulabel2,", ");
        STRCAT(mvslicei->menulabel2,mvslicei->menulabel);
      }
      if(nmeshes>1){
	    mesh *slicemesh;

		slicemesh = meshinfo + sd->blocknumber;
        sprintf(label,", %s",slicemesh->label);
        STRCAT(vsd->menulabel,label);
      }
      if(showfiles==1){
        STRCAT(vsd->menulabel,", ");
        STRCAT(vsd->menulabel,sd->file);
      }
    } 
    for(i=0;i<nvsliceinfo;i++){
      vsd = vsliceinfo + vsliceorderindex[i];
      sd = sliceinfo + vsd->ival;
      STRCPY(vsd->menulabel2,sd->label.longlabel);
      STRCAT(vsd->menulabel2,", ");
      STRCAT(vsd->menulabel2,vsd->menulabel);
    }
  }
}

/* ------------------ hide_slice ------------------------ */

int hide_slice2(slicedata *sdi,slicedata *sdj){
  float delta_orig;
  float dx, dy, dz, aslice, aintersect;

  if(sdi->volslice==1||sdj->volslice==1)return 0;
  delta_orig = MAX(sdi->delta_orig,sdj->delta_orig);
  if(strcmp(sdj->label.shortlabel,sdi->label.shortlabel)!=0
      ||sdj->idir!=sdi->idir
      ||sdj->position_orig+delta_orig<sdi->position_orig
      ||sdj->position_orig-delta_orig>sdi->position_orig
      ||sdj->mesh_type!=sdi->mesh_type
        ){
      return 0;
  }
  dx = MIN(sdi->xmax,sdj->xmax) - MAX(sdi->xmin,sdj->xmin);
  dy = MIN(sdi->ymax,sdj->ymax) - MAX(sdi->ymin,sdj->ymin);
  dz = MIN(sdi->zmax,sdj->zmax) - MAX(sdi->zmin,sdj->zmin);
  if(sdi->idir==1){
    dx=1.0;
    aslice=(sdi->ymax-sdi->ymin)*(sdi->zmax-sdi->zmin);
  }
  if(sdi->idir==2){
    dy=1.0;
    aslice=(sdi->xmax-sdi->xmin)*(sdi->zmax-sdi->zmin);
  }
  if(sdi->idir==3){
    dz=1.0;
    aslice=(sdi->xmax-sdi->xmin)*(sdi->ymax-sdi->ymin);
  }
  aintersect=dx*dy*dz;
  if(dx<=0.0||dy<=0.0||dz<=0.0||sdj->blocknumber<=sdi->blocknumber)return 0;
  if(aintersect<0.1*aslice)return 0;
  return 1;
}

/* ------------------ new_multi_slice ------------------------ */

int new_multi_slice(slicedata *sdold,slicedata *sd){

  if(sdold->volslice!=sd->volslice)return 1;
  if(sd->volslice==0){
    float delta_orig;
    float delta_scaled;

  // sd->delta is in FDS physical units
  // sd->xmin/xmax etc are in Smokeview scaled units
  // convert from physical to scaled units using xyzmaxdiff 
    delta_orig = MAX(sdold->delta_orig,sd->delta_orig);
    delta_scaled = delta_orig/xyzmaxdiff;
    if(ABS(sd->xmin-sdold->xmin)<delta_scaled&&ABS(sd->xmax-sdold->xmax)<delta_scaled // test whether two slices are identical
	   &&ABS(sd->ymin-sdold->ymin)<delta_scaled&&ABS(sd->ymax-sdold->ymax)<delta_scaled
	   &&ABS(sd->zmin-sdold->zmin)<delta_scaled&&ABS(sd->zmax-sdold->zmax)<delta_scaled
     &&sd->blocknumber==sdold->blocknumber
        ){
	    return 1;
	  }
	  
    if(strcmp(sd->label.shortlabel,sdold->label.shortlabel)!=0
      ||sd->idir!=sdold->idir
      ||ABS(sd->position_orig-sdold->position_orig)>delta_orig
      ||sd->mesh_type!=sdold->mesh_type
        ){
      return 1;
    }
  }
  else{
    if(strcmp(sd->label.shortlabel,sdold->label.shortlabel)!=0
      ||sd->mesh_type!=sdold->mesh_type||sd->slicetype!=sdold->slicetype
        ){
      return 1;
    }
  }
  return 0;
}

/* ------------------ getsliceparams ------------------------ */

void getsliceparams(void){
  int i;
  char *file;
  int error;
  FILE_SIZE  lenfile;
  int build_cache=0;
  FILE *stream;

  if(is_file_newer(sliceinfofilename,smvfilename)!=1){
    build_cache=1;
    stream=fopen(sliceinfofilename,"w");
  }
  else{
    build_cache=0;
    stream=fopen(sliceinfofilename,"r");
  }

  for(i=0;i<nsliceinfo;i++){
    slicedata *sd;
    int is1, is2, js1, js2, ks1, ks2;
    int ni, nj, nk;

    sd = sliceinfo + i;

    if(nsliceinfo>100&&(i%100==0||i==nsliceinfo-1)){
      if(i==10){
        printf("    obtaining parameters from %i'th slice file\n",i+1);
      }
      else{
        printf("    obtaining parameters from %i'st slice file\n",i+1);
      }
    }

    file = sd->file;
    lenfile = strlen(file);
    if(sd->compression_type==0){
      int doit_anyway;
      
      doit_anyway=0;
      if(build_cache==0&&stream!=NULL){
        int seq=-1;

        while(seq!=sd->seq_id){
          char buffer[255];

          if(fgets(buffer,255,stream)==NULL){
            doit_anyway=1;
            break;
          }
          sscanf(buffer,"%i %i %i %i %i %i %i %i %i %i %i",&seq,&is1,&is2,&js1,&js2,&ks1,&ks2,&ni,&nj,&nk,&sd->volslice);
        }
        error=0;
      }
      if(build_cache==1||stream==NULL||doit_anyway==1){
        is1=sd->is1;
        is2=sd->is2;
        js1=sd->js1;
        js2=sd->js2;
        ks1=sd->ks1;
        ks2=sd->ks2;
        FORTgetsliceparms(file,&endian_smv,
          &is1,&is2,&js1,&js2,&ks1,&ks2,&ni,&nj,&nk,&sd->volslice,&error,lenfile);
        if(stream!=NULL&&doit_anyway==0)fprintf(stream,"%i %i %i %i %i %i %i %i %i %i %i\n",sd->seq_id,is1,is2,js1,js2,ks1,ks2,ni,nj,nk,sd->volslice);
      }
    }
    else if(sd->compression_type==1){
      error=0;
      if(getsliceheader0(sd->comp_file,sd->size_file,sd->compression_type,&is1,&is2,&js1,&js2,&ks1,&ks2, &sd->volslice)==0)error=1;
      ni = is2 + 1 - is1;
      nj = js2 + 1 - js1;
      nk = ks2 + 1 - ks1;
    }
    if(error==0){
      sd->is1=is1;
      sd->is2=is2;
      sd->js1=js1;
      sd->js2=js2;
      sd->ks1=ks1;
      sd->ks2=ks2;
      sd->nslicei=ni;
      sd->nslicej=nj;
      sd->nslicek=nk;
    }
  }
  update_fedinfo();
  for(i=0;i<nsliceinfo;i++){
    slicedata *sd;
    int is1, is2, js1, js2, ks1, ks2;
    int ni, nj, nk;

    sd = sliceinfo + i;
    is1=sd->is1;
    is2=sd->is2;
    js1=sd->js1;
    js2=sd->js2;
    ks1=sd->ks1;
    ks2=sd->ks2;
    ni=sd->nslicei;
    nj=sd->nslicej;
    nk=sd->nslicek;
    if(error==0){
      int iblock;
      mesh *meshi;
      float position;


      sd->idir=-1;
      iblock = sd->blocknumber;
      meshi = meshinfo + iblock;

      strcpy(sd->slicedir,"");
      position=-999.0;
      if(sd->is1==sd->is2
        ||(sd->js1!=sd->js2&&sd->ks1!=sd->ks2)
        ){
        sd->idir=1;
        position = meshi->xplt_orig[is1];
        if(sd->slicetype==SLICE_CENTER){
          float *xp;

          is2=is1-1;
          if(is2<0)is2=0;
          xp = meshi->xplt_orig;
          position = (xp[is1]+xp[is2])/2.0;
        }
        if(is1>0){
          sd->delta_orig=(meshi->xplt_orig[is1]-meshi->xplt_orig[is1-1])/2.0;
        }
        else{
          sd->delta_orig=(meshi->xplt_orig[is1+1]-meshi->xplt_orig[is1])/2.0;
        }
        if(sd->volslice==0){
          sprintf(sd->slicedir,"X=%f",position);
        }
        else{
          sprintf(sd->slicedir,"3D slice");
        }
      }
      if(sd->js1==sd->js2){
        sd->idir=2;
        position = meshi->yplt_orig[js1];
        if(sd->slicetype==SLICE_CENTER){
          float *yp;

          js2=js1-1;
          if(js2<0)js2=0;
          yp = meshi->yplt_orig;
          position = (yp[js1]+yp[js2])/2.0;
        }
        if(js1>0){
          sd->delta_orig=(meshi->yplt_orig[js1]-meshi->yplt_orig[js1-1])/2.0;
        }
        else{
          sd->delta_orig=(meshi->yplt_orig[js1+1]-meshi->yplt_orig[js1])/2.0;
        }
        sprintf(sd->slicedir,"Y=%f",position);
      }
      if(sd->ks1==sd->ks2){
        sd->idir=3;
        position = meshi->zplt_orig[ks1];
        if(sd->slicetype==SLICE_CENTER){
          float *zp;

          ks2=ks1-1;
          if(ks2<0)ks2=0;
          zp = meshi->zplt_orig;
          position = (zp[ks1]+zp[ks2])/2.0;
        }
        if(ks1>0){
          sd->delta_orig=(meshi->zplt_orig[ks1]-meshi->zplt_orig[ks1-1])/2.0;
        }
        else{
          sd->delta_orig=(meshi->zplt_orig[ks1+1]-meshi->zplt_orig[ks1])/2.0;
        }
        if(sd->slicetype==SLICE_TERRAIN){
          position=sd->above_ground_level;
          sprintf(sd->slicedir,"AGL=%f",position);
        }
        else{
          sprintf(sd->slicedir,"Z=%f",position);
        }
      }
      sd->position_orig=position;
      trimzeros(sd->slicedir);
    }
    {
      float *xplt, *yplt, *zplt;
      mesh *meshi;

      meshi = meshinfo + sd->blocknumber;
      sd->mesh_type=meshi->mesh_type;
      xplt = meshi->xplt;
      yplt = meshi->yplt;
      zplt = meshi->zplt;
      sd->xmin = xplt[sd->is1];
      sd->xmax = xplt[sd->is2];
      sd->ymin = yplt[sd->js1];
      sd->ymax = yplt[sd->js2];
      sd->zmin = zplt[sd->ks1];
      sd->zmax = zplt[sd->ks2];
    }
  }
  if(stream!=NULL)fclose(stream);
  if(nsliceinfo>0){
    FREEMEMORY(sliceorderindex);
    NewMemory((void **)&sliceorderindex,sizeof(int)*nsliceinfo);
    for(i=0;i<nsliceinfo;i++){
      sliceorderindex[i]=i;
    }
    qsort( (int *)sliceorderindex, (size_t)nsliceinfo, sizeof(int), slicecompare );

    for(i=0;i<nmultislices;i++){
      multislicedata *mslicei;

      mslicei = multisliceinfo + i;
      FREEMEMORY(mslicei->islices);
    }
    FREEMEMORY(multisliceinfo);
    nmultislices=0;

    NewMemory((void **)&multisliceinfo,sizeof(multislicedata)*nsliceinfo);

    {
      multislicedata *mslicei;
      slicedata *sd;
      
      nmultislices=1;
      mslicei = multisliceinfo;
      mslicei->islices=NULL;
      NewMemory((void **)&mslicei->islices,sizeof(int)*nsliceinfo);
      mslicei->nslices=1;
      sd = sliceinfo + sliceorderindex[0];
      mslicei->islices[0] = sliceorderindex[0];
      mslicei->type=sd->type;
      for(i=1;i<nsliceinfo;i++){
        slicedata *sdold;

        sdold = sliceinfo + sliceorderindex[i - 1];
        sd = sliceinfo + sliceorderindex[i];
        mslicei->autoload=0;
        if(new_multi_slice(sdold,sd)==1){
          nmultislices++;
          mslicei++;
          mslicei->nslices=0;
          mslicei->type=sd->type;
          mslicei->mesh_type=sd->mesh_type;
          mslicei->islices=NULL;
          NewMemory((void **)&mslicei->islices,sizeof(int)*nsliceinfo);
        }
        mslicei->nslices++;
        mslicei->islices[mslicei->nslices-1]=sliceorderindex[i];
      }
    }
  }
#ifdef pp_HIDEMULTI
  for(iii=0;iii<nmultislices;iii++){
    int ii;

    mslicei = multisliceinfo + iii;
    for(i=0;i<mslicei->nslices;i++){
      slicedata *sdi, *sdj;
      int j;

      if(mslicei->islices[i]==-1)continue;
      sdi = sliceinfo + mslicei->islices[i];
      for(j=0;j<mslicei->nslices;j++){
        if(i==j||mslicei->islices[j]==-1)continue;
        sdj = sliceinfo + mslicei->islices[j];
        if(hide_slice2(sdi,sdj)==1){
          mslicei->islices[j]=-1;
        }
      }
    }
    ii=0;
    for(i=0;i<mslicei->nslices;i++){
      if(mslicei->islices[i]==-1)continue;
      mslicei->islices[ii]=mslicei->islices[i];
      ii++;
    }
    mslicei->nslices=ii;
  }
#endif
  for(i=0;i<nsliceinfo;i++){
    slicedata *slicei;

    slicei = sliceinfo + i;
    slicei->mslice=NULL;
  }
  for(i=0;i<nmultislices;i++){
    int ii;
    multislicedata *mslicei;

    mslicei = multisliceinfo + i;
    mslicei->contour_areas=NULL;
    for(ii=0;ii<mslicei->nslices;ii++){
      slicedata *slicei;

      slicei = sliceinfo + mslicei->islices[ii];
      ASSERT(slicei->mslice==NULL);
      slicei->mslice=mslicei;
    }
  }
  updateslicemenulabels();
  update_slicedir_count();
}

/* ------------------ getsliceparams2 ------------------------ */

void getsliceparams2(void){
  int i;
  
  trainer_temp_n=0;
  trainer_oxy_n=0;
  if(nmultislices>0){
    FREEMEMORY(trainer_temp_indexes);
    FREEMEMORY(trainer_oxy_indexes);
    NewMemory((void **)&trainer_temp_indexes,nmultislices*sizeof(int));
    NewMemory((void **)&trainer_oxy_indexes,nmultislices*sizeof(int));
  }
  for(i=0;i<nmultislices;i++){
    multislicedata *mslicei;

    mslicei = multisliceinfo + i;
    if(mslicei->autoload==1){
      slicedata *slicei;
      char *longlabel;

      slicei = sliceinfo + mslicei->islices[0];
      longlabel = slicei->label.longlabel;

      if(STRCMP(longlabel,"TEMPERATURE")==0){
        trainer_temp_indexes[trainer_temp_n++]=i;
      }
      if(STRCMP(longlabel,"OXYGEN")==0||STRCMP(longlabel,"OXYGEN VOLUME FRACTION")==0){
        trainer_oxy_indexes[trainer_oxy_n++]=i;
      }
    }
  }
}

/* ------------------ update_fedinfo ------------------------ */

void update_fedinfo(void){
  int i;
  int nfediso=0,ifediso=0;

  nfedinfo=0;
  for(i=0;i<nsliceinfo;i++){
    slicedata *slicei;
    feddata *fedi;
    int j;

    fedi = fedinfo + nfedinfo;
    slicei = sliceinfo + i;

    fedi->co=NULL;
    fedi->co2=NULL;
    fedi->o2=NULL;
    fedi->fed_slice=NULL;
    fedi->fed_iso=NULL;
    fedi->co_index=-1;
    fedi->co2_index=-1;
    fedi->o2_index=-1;
    fedi->fed_index=-1;
    fedi->loaded=0;
    fedi->display=0;
    if(strcmp(slicei->label.longlabel,"CARBON DIOXIDE VOLUME FRACTION")!=0)continue;
    fedi->co2_index=i;
    for(j=0;j<nsliceinfo;j++){
      slicedata *slicej;

      slicej = sliceinfo + j;
      if(strcmp(slicej->label.longlabel,"CARBON MONOXIDE VOLUME FRACTION")!=0)continue;
      if(slicei->blocknumber!=slicej->blocknumber)continue;
      if(slicei->is1!=slicej->is1||slicei->is2!=slicej->is2)continue;
      if(slicei->js1!=slicej->js1||slicei->js2!=slicej->js2)continue;
      if(slicei->ks1!=slicej->ks1||slicei->ks2!=slicej->ks2)continue; // skip if not the same size and in the same mesh
      fedi->co_index=j;
      break;
    }
    if(fedi->co_index==-1)continue;
    for(j=0;j<nsliceinfo;j++){
      slicedata *slicej;

      slicej = sliceinfo + j;
      if(strcmp(slicej->label.longlabel,"OXYGEN VOLUME FRACTION")!=0)continue;
      if(slicei->blocknumber!=slicej->blocknumber)continue;
      if(slicei->is1!=slicej->is1||slicei->is2!=slicej->is2)continue;
      if(slicei->js1!=slicej->js1||slicei->js2!=slicej->js2)continue;
      if(slicei->ks1!=slicej->ks1||slicei->ks2!=slicej->ks2)continue; // skip if not the same size and in the same mesh
      fedi->o2_index=j;
      break;
    }
    if(fedi->o2_index==-1)continue;
    fedi->fed_index=nsliceinfo+nfedinfo;
    if(sliceinfo[fedi->co_index].volslice==1)nfediso++;
    nfedinfo++;
  }
  if(nfedinfo==0){
    FREEMEMORY(fedinfo);
    return;
  }
  else{
    nsliceinfo+=nfedinfo;
    ResizeMemory((void **)&fedinfo,nfedinfo*sizeof(feddata));
    ResizeMemory((void **)&sliceinfo,nsliceinfo*sizeof(slicedata));
    ResizeMemory( (void **)&vsliceinfo, 3*nsliceinfo*sizeof(vslicedata));
    ResizeMemory( (void **)&sliceinfo,  nsliceinfo*sizeof(slicedata));
    ResizeMemory( (void **)&fedinfo,  nsliceinfo*sizeof(feddata));
    ResizeMemory( (void **)&slicetypes, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&slice_loadstack, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&vslice_loadstack, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&subslice_menuindex, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&subvslice_menuindex, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&mslice_loadstack, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&mvslice_loadstack, nsliceinfo*sizeof(int));
    ResizeMemory( (void **)&vslicetypes,3*nsliceinfo*sizeof(int));
    if(nfediso>0){
      nisoinfo+=nfediso;
      if(nisoinfo==nfediso){
        NewMemory((void **)&isoinfo,nisoinfo*sizeof(isodata));
        NewMemory((void **)&isotypes,nisoinfo*sizeof(int));
      }
      else{
        ResizeMemory((void **)&isoinfo,nisoinfo*sizeof(isodata));
        ResizeMemory((void **)&isotypes,nisoinfo*sizeof(int));
      }
    }
  }
  for(i=0;i<nfedinfo;i++){ // define sliceinfo for fed slices
    slicedata *sd;
    int len;
    slicedata *co2;
    int nn_slice;
    feddata *fedi;
    char filename[256],*ext;

    sd = sliceinfo + nsliceinfo + (i- nfedinfo);
    fedi = fedinfo + i;
    fedi->co=sliceinfo+fedi->co_index;
    fedi->o2=sliceinfo+fedi->o2_index;
    fedi->co2=sliceinfo+fedi->co2_index;
    fedi->fed_slice=sliceinfo+fedi->fed_index;
    fedi->fed_iso=NULL;

    setlabels(&(sd->label),"Fractional effective dose","FED"," ");

    co2 = fedi->co2;
    sd->file=NULL;
    sd->is1=co2->is1;
    sd->is2=co2->is2;
    sd->js1=co2->js1;
    sd->js2=co2->js2;
    sd->ks1=co2->ks1;
    sd->ks2=co2->ks2;

    nn_slice=nsliceinfo+i;

    sd->is_fed=1;
    sd->fedptr=fedi;
    sd->slicetype=co2->slicetype;
    sd->reg_file=NULL;
    sd->comp_file=NULL;
    sd->compression_type=co2->compression_type;
    sd->vol_file=co2->vol_file;
    sd->size_file=NULL;
    sd->slicelabel=NULL;
    sd->above_ground_level=co2->above_ground_level;
    sd->seq_id=nn_slice;
    sd->autoload=0;
    sd->display=0;
    sd->loaded=0;
    sd->qslicedata=NULL;
    sd->compindex=NULL;
    sd->slicecomplevel=NULL;
    sd->qslicedata_compressed=NULL;
    sd->volslice=fedi->co->volslice;
    sd->times=NULL;
    sd->slicelevel=NULL;
    sd->iqsliceframe=NULL;
    sd->qsliceframe=NULL;
    sd->timeslist=NULL;
    sd->blocknumber=co2->blocknumber;
    sd->vloaded=0;
    sd->reload=0;
    sd->nline_contours=0;
    sd->line_contours=NULL;
    sd->contour_areas=NULL;
    sd->ncontour_areas=0;
    sd->menu_show=1;
    sd->constant_color=NULL;
    sd->mesh_type=co2->mesh_type;

    strcpy(filename,fedi->co->file);
    ext=strrchr(filename,'.');
    *ext=0;
    strcat(filename,"_fed.sf");
    len=strlen(filename);
    NewMemory((void **)&fedi->fed_slice->reg_file,len+1);
    strcpy(sd->reg_file,filename);
    sd->file=sd->reg_file;
    
    if(sd->volslice==1){
      isodata *isoi;
      int nn_iso,ii;
      float **colorlevels;

      isoi = isoinfo + nisoinfo - nfediso + ifediso;
      fedi->fed_iso=isoi;
      isoi->tfile=NULL;
      isoi->is_fed=1;
      isoi->fedptr=fedi;
      nn_iso = nisoinfo - nfediso + ifediso + 1;
      isoi->seq_id=nn_iso;
      isoi->autoload=0;
      isoi->blocknumber=sd->blocknumber;
      isoi->loaded=0;
      isoi->display=0;
      isoi->dataflag=0;
      isoi->geomflag=0;
      isoi->levels=NULL;
      setlabels(&(isoi->surface_label),"Fractional effective dose","FED"," ");

      isoi->nlevels=3;
      NewMemory((void **)&(isoi->levels),3*sizeof(float));
      NewMemory((void **)&colorlevels,3*sizeof(float *));
      isoi->colorlevels=colorlevels;
      for(ii=0;ii<3;ii++){
        colorlevels[ii]=NULL;
      }
      isoi->levels[0]=0.3;
      isoi->levels[1]=1.0;
      isoi->levels[2]=3.0;
      setlabels_iso(&(isoi->surface_label),"Fractional effective dose","FED"," ",isoi->levels,isoi->nlevels);
      isoi->normaltable=NULL;
      isoi->color_label.longlabel=NULL;
      isoi->color_label.shortlabel=NULL;
      isoi->color_label.unit=NULL;
      isoi->geominfo=NULL;

      strcpy(filename,fedi->co->file);
      ext=strrchr(filename,'.');
      *ext=0;
      strcat(filename,"_fed.iso");
      len=strlen(filename);
      NewMemory((void **)&isoi->reg_file,len+1);
      strcpy(isoi->reg_file,filename);
      isoi->file=isoi->reg_file;

      NewMemory((void **)&isoi->size_file,strlen(isoi->file)+3+1);
      STRCPY(isoi->size_file,isoi->file);
      STRCAT(isoi->size_file,".sz");

      ifediso++;
    }
  }
  if(nfediso>0)updateisomenulabels();

}

/* ------------------ updatevslices ------------------------ */

void updatevslices(void){
  int i;

  printf("  updating vector slices\n");
  getsliceparams();

  /* update vector slices */

  nvsliceinfo=0;
  for(i=0;i<nsliceinfo;i++){
    slicedata *sdi;

    sdi = sliceinfo+i;
    sdi->vec_comp=0;
    if(strncmp(sdi->label.shortlabel,"U-VEL",5)==0){
       sdi->vec_comp=1;
       continue;
    }
    if(strncmp(sdi->label.shortlabel,"V-VEL",5)==0){
      sdi->vec_comp=2;
      continue;
    }
    if(strncmp(sdi->label.shortlabel,"W-VEL",5)==0){
      sdi->vec_comp=3;
      continue;
    }
  }
  for(i=0;i<nsliceinfo;i++){
    slicedata *sdi;
    vslicedata *vd;
    int j;

    if(nsliceinfo>100&&(i%100==0||i==nsliceinfo-1)){
      if(i==10){
        printf("    examining %i'th slice file for vectors\n",i+1);
      }
      else{
        printf("    examining %i'st slice file for vectors\n",i+1);
      }
    }
    vd = vsliceinfo + nvsliceinfo;
    sdi = sliceinfo+i;
    vd->iu=-1;
    vd->iv=-1;
    vd->iw=-1;
    vd->ival=i;
    vd->type=sliceinfo[i].type;
    vd->slicetype=sliceinfo[i].slicetype;
    if(vd->slicetype==SLICE_CENTER){
      for(j=0;j<nsliceinfo;j++){
        slicedata *sdj;

        sdj = sliceinfo+j;
        if(sdj->slicetype!=SLICE_CENTER)continue;
        if(sdi->blocknumber!=sdj->blocknumber)continue;
        if(sdi->is1!=sdj->is1||sdi->is2!=sdj->is2||sdi->js1!=sdj->js1)continue;
        if(sdi->js2!=sdj->js2||sdi->ks1!=sdj->ks1||sdi->ks2!=sdj->ks2)continue;
        if(sdj->vec_comp==1)vd->iu=j;
        if(sdj->vec_comp==2)vd->iv=j;
        if(sdj->vec_comp==3)vd->iw=j;
      }
    }
    else{
      for(j=0;j<nsliceinfo;j++){
        slicedata *sdj;

        sdj = sliceinfo+j;
        if(sdj->slicetype==SLICE_CENTER)continue;
        if(sdi->blocknumber!=sdj->blocknumber)continue;
        if(sdi->is1!=sdj->is1||sdi->is2!=sdj->is2||sdi->js1!=sdj->js1)continue;
        if(sdi->js2!=sdj->js2||sdi->ks1!=sdj->ks1||sdi->ks2!=sdj->ks2)continue;
        if(sdj->vec_comp==1)vd->iu=j;
        if(sdj->vec_comp==2)vd->iv=j;
        if(sdj->vec_comp==3)vd->iw=j;
      }
    }
    if(vd->iu!=-1||vd->iv!=-1||vd->iw!=-1){
      vd->display=0;
      vd->loaded=0;
      vd->vec_type=0;
      vd->volslice=sdi->volslice;
      nvsliceinfo++;
    }
  }
  printf("    %i vector slices found\n",nvsliceinfo);
  if(nvsliceinfo>0){
    vslicedata *vsd;
    multivslicedata *mvslicei;

    FREEMEMORY(vsliceorderindex);
    NewMemory((void **)&vsliceorderindex,sizeof(int)*nvsliceinfo);
    for(i=0;i<nvsliceinfo;i++){
      vsliceorderindex[i]=i;
    }
    qsort( (int *)vsliceorderindex, (size_t)nvsliceinfo, sizeof(int), vslicecompare );

    for(i=0;i<nmultivslices;i++){
      mvslicei = multivsliceinfo + i;
      FREEMEMORY(mvslicei->ivslices);
    }
    FREEMEMORY(multivsliceinfo);
    nmultivslices=0;

    NewMemory((void **)&multivsliceinfo,sizeof(multislicedata)*nvsliceinfo);

    nmultivslices=1;
    mvslicei = multivsliceinfo;
    mvslicei->ivslices=NULL;
    NewMemory((void **)&mvslicei->ivslices,sizeof(int)*nvsliceinfo);
    mvslicei->nvslices=1;
    vsd = vsliceinfo + vsliceorderindex[0];
    mvslicei->ivslices[0] = vsliceorderindex[0];
    mvslicei->type=sliceinfo[vsd->ival].type;
    for(i=1;i<nvsliceinfo;i++){
      slicedata *sd, *sdold;
      vslicedata *vsdold;

      vsdold = vsliceinfo + vsliceorderindex[i - 1];
      sdold = sliceinfo + vsdold->ival;
      vsd = vsliceinfo + vsliceorderindex[i];
      sd = sliceinfo + vsd->ival;
      if(new_multi_slice(sdold,sd)==1){
        nmultivslices++;
        mvslicei++;
        mvslicei->nvslices=0;
        mvslicei->type=sd->type;
        mvslicei->ivslices=NULL;
        NewMemory((void **)&mvslicei->ivslices,sizeof(int)*nvsliceinfo);
      }
      mvslicei->nvslices++;
      mvslicei->ivslices[mvslicei->nvslices-1]=vsliceorderindex[i];
    } 

    // define sequence id's for auto file loading

    for(i=0;i<nvsliceinfo;i++){
      vslicedata *vslicei;
      slicedata *sliceval;
      int seq_id;

      vslicei = vsliceinfo + i;
      sliceval = sliceinfo + vslicei->ival;
      seq_id=-1;
      if(vslicei->ival>=0)seq_id = sliceval->seq_id;
      vslicei->seq_id=seq_id;
      vslicei->autoload=0;
    }
  }
  if(nvsliceinfo>0)printf("    updating vector slice menus\n");
  updatevslicemenulabels();
  printf("  vector slices update completed\n\n");
  
}

/* ------------------ updatevslicetypes ------------------------ */
void updatevslicetypes(void){
  int i;
  vslicedata *vd;

  nvslicetypes = 0;
  for(i=0;i<nvsliceinfo;i++){
    vd = vsliceinfo+i;
    if(getvsliceindex(vd)==-1)vslicetypes[nvslicetypes++]=i;
  }
  for(i=0;i<nvsliceinfo;i++){
    vd = vsliceinfo+i;
    vd->type=getvslicetype(vd);
  }
}

/* ------------------ update_slice_contours ------------------------ */

void update_slice_contours(int slice_type_index, float line_min, float line_max, int nline_values){
  int i,j;
  int nx, ny, nz;
  mesh *meshi;
  int ibar, jbar, kbar;
  float *xplt, *yplt, *zplt;
  float constval;
  databounds *sb;
  int slice_type_j;
  int contours_gen=0;
  float dval;

  dval=0.0;
  if(nline_values>1&&line_max!=line_min){
    dval=(line_max-line_min)/(float)(nline_values-1);
  }

  sb = slicebounds + slice_type_index;
  for(j=0;j<nsliceinfo;j++){
    slicedata *sd;

    sd = sliceinfo + j;
    if(sd->loaded==0)continue;

    slice_type_j = getslicetype(sd);
    if(slice_type_j!=slice_type_index)continue;
    if(sd->qslicedata==NULL){
      printf("*** warning: data not available from %s to generate contours\n",sd->reg_file);
      continue;
    }
    printf("generating contours for %s\n",sd->file);
    contours_gen=1;

    for(i=0;i<nline_values;i++){
      int val_index;
      float val;
      float valmin, valmax;
      
      valmin = sb->levels256[0];
      valmax = sb->levels256[255];

      val=line_min + i*dval;
      val_index=255;
      if(val<valmin){
        val_index=0;
      }
      else if(valmax>valmin&&val>=valmin&&val<=valmax){
        val_index=255*(val-valmin)/(valmax-valmin);
      }
      else if(val>valmax){
        val_index=255;
      }
      if(val_index<0)val_index=0;
      if(val_index>255)val_index=255;
      sd->rgb_slice_ptr[i]=&rgb_full[val_index][0];
    }
    meshi = meshinfo + sd->blocknumber;

    xplt=meshi->xplt;
    yplt=meshi->yplt;
    zplt=meshi->zplt;
    ibar=meshi->ibar;
    jbar=meshi->jbar;
    kbar=meshi->kbar;
    nx = ibar + 1;
    ny = jbar + 1;
    nz = kbar + 1;

    switch (sd->idir){
      case 1:
      constval = xplt[sd->is1]+offset_slice*sd->sliceoffset;
      break;
      case 2:
      constval = yplt[sd->js1]+offset_slice*sd->sliceoffset;
      break;
      case 3:
      constval = zplt[sd->ks1]+offset_slice*sd->sliceoffset;
      break;
      default:
        ASSERT(0);
        break;
    }

    freecontours(sd->line_contours,sd->nline_contours);
    sd->nline_contours=sd->ntimes;
    initcontours(&sd->line_contours,sd->rgb_slice_ptr,sd->nline_contours,constval,sd->idir,line_min,line_max,nline_values);
    for(i=0;i<sd->nline_contours;i++){
      float *vals;
      contour *ci;

      vals = sd->qslicedata + i*sd->nsliceii;
      ci = sd->line_contours+i;
      switch (sd->idir){
        case 1:
        getlinecontours(yplt,zplt,ny,nz,vals,NULL,line_min, line_max,ci);
        break;
        case 2:
        getlinecontours(xplt,zplt,nx,nz,vals,NULL,line_min,line_max,ci);
        break;
        case 3:
        getlinecontours(xplt,yplt,nx,ny,vals,NULL,line_min,line_max,ci);
        break;
        default:
          ASSERT(0);
          break;
      }
    }
  }
  if(contours_gen==0){
    printf("*** warning: no slice files of type %s are currently loaded\n",sb->datalabel);
  }
}

/* ------------------ updateslicetypes ------------------------ */

void updateslicetypes(void){
  int i;
  slicedata *sd;

  nslicetypes = 0;
  for(i=0;i<nsliceinfo;i++){
    sd = sliceinfo+i;
    if(getsliceindex(sd)==-1)slicetypes[nslicetypes++]=i;
  }
  for(i=0;i<nsliceinfo;i++){
    sd = sliceinfo+i;
    sd->type=getslicetype(sd);
  }
}

/* ------------------ getvsliceindex ------------------------ */

int getvsliceindex(const vslicedata *vd){
  vslicedata *vd2;
  int j;

  for(j=0;j<nvslicetypes;j++){
    vd2 = vsliceinfo+vslicetypes[j];
    if(strcmp(sliceinfo[vd->ival].label.shortlabel,sliceinfo[vd2->ival].label.shortlabel)==0)return vslicetypes[j];
  }
  return -1;
}

/* ------------------ getvsliceindex ------------------------ */

int getvslicetype(const vslicedata *vd){
  vslicedata *vd2;
  int j;

  for(j=0;j<nvslicetypes;j++){
    vd2 = vsliceinfo+vslicetypes[j];
    if(strcmp(sliceinfo[vd->ival].label.shortlabel,sliceinfo[vd2->ival].label.shortlabel)==0)return j;
  }
  return -1;
}


/* ------------------ getsliceindex ------------------------ */

int getsliceindex(const slicedata *sd){
  slicedata *sd2;
  int j;

  for(j=0;j<nslicetypes;j++){
    sd2 = sliceinfo+slicetypes[j];
    if(strcmp(sd->label.shortlabel,sd2->label.shortlabel)==0)return slicetypes[j];
  }
  return -1;
}

/* ------------------ getslicetype ------------------------ */

int getslicetype(const slicedata *sd){
  slicedata *sd2;
  int j;

  for(j=0;j<nslicetypes;j++){
    sd2 = sliceinfo+slicetypes[j];
    if(strcmp(sd->label.shortlabel,sd2->label.shortlabel)==0)return j;
  }
  return -1;
}

/* ------------------ getslicetype_fromlabel ------------------------ */

int getslicetype_fromlabel(char *label){
  slicedata *sd2;
  int j;

  for(j=0;j<nslicetypes;j++){
    sd2 = sliceinfo+slicetypes[j];
    if(strcmp(label,sd2->label.shortlabel)==0)return j;
  }
  return -1;
}


/* ------------------ updatesliceboundlabels ------------------------ */

void updatesliceboundlabels(){
  int i,j;
  databounds *sb;
  slicedata *sd;

  for(i=0;i<nsliceinfo;i++){
    sd = sliceinfo + i;
    j = getslicetype(sd);
    sb = slicebounds + j;
    sb->label=&(sd->label);
  }
}

/* ------------------ setslicecolors ------------------------ */

void setslicecolors(float smin, float smax, 
                    slicedata *sd, int *errorcode){
  char *scale;
  int slicetype;
  databounds *sb;

  slicetype=getslicetype(sd);
  sb = slicebounds + slicetype;
  sb->label=&(sd->label);


  *errorcode=0;
  printf("computing slice color levels \n");
  scale=sb->scale;
  if(sd->qslicedata==NULL)return;
  getSliceColors(sd->qslicedata,sd->nslicetotal,sd->slicelevel,
                smin,smax,
                nrgb_full,nrgb,
                sb->colorlabels,&scale,&sb->fscale,sb->levels256,
                &sd->extreme_min,&sd->extreme_max
                );
}

/* ------------------ setslicelabels ------------------------ */

void setslicelabels(float smin, float smax, 
                    slicedata *sd, int *errorcode){
  char *scale;
  int slicetype;
  databounds *sb;

  slicetype=getslicetype(sd);
  sb = slicebounds + slicetype;
  sb->label=&(sd->label);

  *errorcode=0;
  printf("setting up slice labels \n");
  scale=sb->scale;
  getSliceLabels(smin,smax,nrgb,
                sb->colorlabels,&scale,&sb->fscale,sb->levels256);
}

/* ------------------ setslicebounds ------------------------ */

void setslicebounds(int slicetype){
  if(slicetype>=0&&slicetype<nslice2){
#ifdef pp_SLICECONTOURS
    slice_line_contour_min=slicebounds[slicetype].line_contour_min;
    slice_line_contour_max=slicebounds[slicetype].line_contour_max;
    slice_line_contour_num=slicebounds[slicetype].line_contour_num;
#endif
    slicemin=slicebounds[slicetype].valmin;
    slicemax=slicebounds[slicetype].valmax;
    setslicemin=slicebounds[slicetype].setvalmin;
    setslicemax=slicebounds[slicetype].setvalmax;
    slicechopmin=slicebounds[slicetype].chopmin;
    slicechopmax=slicebounds[slicetype].chopmax;
    setslicechopmin=slicebounds[slicetype].setchopmin;
    setslicechopmax=slicebounds[slicetype].setchopmax;
  }
}

/* ------------------ getslicedatabounds ------------------------ */

void getslicedatabounds(const slicedata *sd, float *pmin, float *pmax){

  float *pdata;
  int ndata;
  int n;
  int first=1;
  int i,j,k,nn;
  int nx, ny, nxy, ibar, jbar;
  int ntimes;
  char *iblank_node, *iblank_cell;
  mesh *meshi;

  meshi = meshinfo + sd->blocknumber;
  iblank_node = meshi->c_iblank_node;
  iblank_cell = meshi->c_iblank_cell;

  ibar = meshi->ibar;
  jbar = meshi->jbar;
  nx = ibar+1;
  ny = jbar+1;
  nxy = nx*ny;

  pdata = sd->qslicedata;
  ndata = sd->nslicetotal;

  n=-1;
  ntimes = ndata/sd->nsliceii;
  for(nn=0;nn<ntimes;nn++){
    for(i=0;i<sd->nslicei;i++){
      for(j=0;j<sd->nslicej;j++){
        for(k=0;k<sd->nslicek;k++){
          n++;
          if(sd->slicetype==SLICE_CENTER&&((k==0&&sd->nslicek!=1)||(j==0&&sd->nslicej!=1)||(i==0&&sd->nslicei!=1)))continue;
        // 0 blocked
        // 1 partially blocked
        // 2 unblocked
          if(sd->slicetype!=SLICE_CENTER&&iblank_node[IJKNODE(sd->is1+i,sd->js1+j,sd->ks1+k)]==0){
//            if(nn==0){
//              printf("%i %i %i %f blocked\n",i,j,k,pdata[n]);
//            }
            continue;
          }
          if(sd->slicetype==SLICE_CENTER&&iblank_cell[IJKCELL(sd->is1+i-1,sd->js1+j-1,sd->ks1+k-1)]==0){
            continue;
          }
//          if(nn==0)printf("%i %i %i %f not blocked\n",i,j,k,pdata[n]);
          if(first==1){
            *pmin=pdata[n];
            *pmax=pdata[n];
            first=0;
          }
          else{
            if(pdata[n]<*pmin)*pmin=pdata[n];
            if(pdata[n]>*pmax)*pmax=pdata[n];
          }
        }
      }
    }
    if(first==1){
      *pmin=0.0;
      *pmax=1.0;
    }
  }
  printf(" global min (slice file): %f\n",*pmin);
  printf(" global max (slice file): %f\n",*pmax);
}

/* ------------------ adjustslicebounds ------------------------ */

void adjustslicebounds(const slicedata *sd, float *pmin, float *pmax){

    int nsmall, nbig, *buckets=NULL, n, level, total, alpha05;
    float dp;
    float *pdata;
    int ndata;
    float ppmin;

    pdata = sd->qslicedata;
    ndata = sd->nslicetotal;

    if(setslicemin==PERCENTILE_MIN||setslicemax==PERCENTILE_MAX){
      dp = (*pmax - *pmin)/NBUCKETS;
      nsmall=0;
      nbig=NBUCKETS;
      if(NewMemory((void **)&buckets,NBUCKETS*sizeof(int))==0){
        printf("*** Warning: Unable to allocate memory in getdatabounds\n");
        return;
      }

      for (n=0;n<NBUCKETS;n++){
        buckets[n]=0;
      }
      for (n=0;n<ndata;n++){
        level=0;
        if(dp!=0.0f){
          level = (int)((pdata[n] - *pmin)/dp);
        }
        if(level<0){
          level=0;
        }
        if(level>NBUCKETS-1){
          level=NBUCKETS-1;
        }
        buckets[level]++;
      }
      alpha05 = (int)(.01f*ndata);
      total = 0;
      for (n=0;n<NBUCKETS;n++){
        total += buckets[n];
        if(total>alpha05){
          nsmall=n;
          break;
        }
      }
      total = 0;
      for (n=NBUCKETS;n>0;n--){
        total += buckets[n-1];
        if(total>alpha05){
          nbig=n;
          break;
        }
      }
      FreeMemory(buckets);
      ppmin = *pmin;
      if(setslicemin==PERCENTILE_MIN)*pmin = ppmin + nsmall*dp;
      if(setslicemax==PERCENTILE_MAX)*pmax = ppmin + (nbig+1)*dp;
      
    }
    if(axislabels_smooth==1){
      smoothlabel(pmin,pmax,nrgb);
    }

}

/* ------------------ drawslice_frame ------------------------ */

void drawslice_frame(){
    int ii;

    for(ii=0;ii<nslice_loaded;ii++){
      slicedata *sd;
      int i;

      i=slice_loaded_list[ii];
      sd = sliceinfo + i;
      if(sd->display==0||sd->type!=islicetype)continue;
      if(sd->times[0]>global_times[itimes])continue;
      if(sd->compression_type==1||sd->compression_type==2){
#ifdef USE_ZLIB
        uncompress_slicedataframe(sd,sd->itime);
#endif
        sd->iqsliceframe=sd->slicecomplevel;
      }
      else{
        sd->iqsliceframe = sd->slicelevel + sd->itime*sd->nsliceii;
        sd->qslice       = sd->qslicedata + sd->itime*sd->nsliceii;
      }
      sd->qsliceframe=NULL;
#ifdef pp_MEMDEBUG
      if(sd->compression_type==0){
        ASSERT(ValidPointer(sd->qslicedata,sizeof(float)*sd->nslicetotal));
      }
#endif

      if(sd->qslicedata!=NULL)sd->qsliceframe = sd->qslicedata + sd->itime*sd->nsliceii;
#ifdef pp_SLICECONTOURS
      if(vis_slice_contours==1&&sd->line_contours!=NULL){
        DrawLineContours(sd->line_contours+sd->itime, 3.0);
        SNIFF_ERRORS("after DrawLineContours");
        continue;
      }
#endif
      switch (sd->slicetype){
        case SLICE_NODE:
          if(usetexturebar!=0){
            drawvolslice_texture(sd);
            SNIFF_ERRORS("after drawvolslice_texture");
          }
          else{
            drawvolslice(sd);
            SNIFF_ERRORS("after drawvolslice");
          }
#ifdef pp_GPU
          if(sd->volslice==1&&(show_gslice_data==1)){
            if(usegpu==1){
              Load3DSliceShaders();
              SNIFF_ERRORS("after Load3DSliceShaders");
              drawgslice_dataGPU(sd);
              SNIFF_ERRORS("after drawgslice_dataGPU");
              UnLoadShaders();
              SNIFF_ERRORS("after UnLoad3DSliceShaders");
            }
            else{
              drawgslice_data(sd);
              SNIFF_ERRORS("after drawgslice_data");
            }
          }
#else
          if(sd->volslice==1&&show_gslice_data==1){
            drawgslice_data(sd);
          }
#endif
          break;
        case SLICE_CENTER:
          drawvolslice_cellcenter(sd);
          SNIFF_ERRORS("after drawvolslice_cellcenter");
          break;
        case SLICE_TERRAIN:
          drawvolslice_terrain(sd);
          SNIFF_ERRORS("after drawvolslice_terrain");
          break;
      }
  }
}

/* ------------------ drawvslice_frame ------------------------ */

void drawvslice_frame(void){
  int i;

  for(i=0;i<nvsliceinfo;i++){
    vslicedata *vd;
    slicedata *u, *v, *w, *val;

    vd = vsliceinfo + i;
    if(vd->loaded==0||vd->display==0||sliceinfo[vd->ival].type!=islicetype)continue;
    val = vd->val;
    if(val==NULL)continue;
    u = vd->u;
    v = vd->v;
    w = vd->w;
    if(u==NULL&&v==NULL&&w==NULL)continue;
    if(sliceinfo[vd->ival].times[0]>global_times[itimes])continue;
#define VAL val
    if(VAL->compression_type==1){
#ifdef USE_ZLIB
      uncompress_slicedataframe(VAL,VAL->itime);
#endif
      VAL->iqsliceframe=VAL->slicecomplevel;
    }
    else{
      if(VAL!=NULL)VAL->iqsliceframe = VAL->slicelevel + VAL->itime*VAL->nsliceii;
    }
#undef VAL
#define VAL u
    if(VAL!=NULL){
      if(VAL->compression_type==1){
#ifdef USE_ZLIB
        uncompress_slicedataframe(VAL,VAL->itime);
#endif
        VAL->iqsliceframe=VAL->slicecomplevel;
      }
      else{
        if(VAL!=NULL)VAL->iqsliceframe = VAL->slicelevel + VAL->itime*VAL->nsliceii;
      }
    }
#undef VAL
#define VAL v
    if(VAL!=NULL){
      if(VAL->compression_type==1){
#ifdef USE_ZLIB
        uncompress_slicedataframe(VAL,VAL->itime);
#endif
        VAL->iqsliceframe=VAL->slicecomplevel;
      }
      else{
        if(VAL!=NULL)VAL->iqsliceframe = VAL->slicelevel + VAL->itime*VAL->nsliceii;
      }
    }
#undef VAL
#define VAL w
    if(VAL!=NULL){
      if(VAL->compression_type==1){
#ifdef USE_ZLIB
        uncompress_slicedataframe(VAL,VAL->itime);
#endif
        VAL->iqsliceframe=VAL->slicecomplevel;
      }
      else{
        if(VAL!=NULL)VAL->iqsliceframe = VAL->slicelevel + VAL->itime*VAL->nsliceii;
      }
    }
    if(u!=NULL&&u->compression_type==0){
      u->qslice = u->qslicedata + u->itime*u->nsliceii;
    }
    if(v!=NULL&&v->compression_type==0){
      v->qslice = v->qslicedata + v->itime*v->nsliceii;
    }
    if(w!=NULL&&w->compression_type==0){
      w->qslice = w->qslicedata + w->itime*w->nsliceii;
    }
    if(vd->slicetype==SLICE_TERRAIN){
      drawvvolslice_terrain(vd);
    }
    else if(vd->slicetype==SLICE_CENTER){
        drawvvolslice_cellcenter(vd);
    }
    else{
      drawvvolslice(vd);
    }
  }
}

#ifdef pp_GSLICE

/* ------------------ plane_dist ------------------------ */

float plane_dist(float *norm, float *xyz0, float *xyz){
  float dist=0.0;
  float dx;
  int i;

  for(i=0;i<3;i++){
    dx = xyz[i]-xyz0[i];
    dist += dx*norm[i];
  }
  return dist;
}

/* ------------------ update_gslice_planes ------------------------ */

void update_gslice_planes(void){
  int i;
  float *norm;
  float *xyz0;

  /* stuff min and max grid data into a more convenient form 
  assuming the following grid numbering scheme

       5-------6
     / |      /| 
   /   |     / | 
  4 -------7   |
  |    |   |   |  
  Z    1---|---2
  |  Y     |  /
  |/       |/
  0--X-----3     

  */
  int ix[8]={0,0,1,1,0,0,1,1};
  int iy[8]={0,1,1,0,0,1,1,0};
  int iz[8]={0,0,0,0,1,1,1,1};

// plane equation: (x-xyz0) .dot. norm = 0
  norm=gslice_norm;
  xyz0 = gslice_xyz;


  for(i=0;i<nmeshes;i++){
    mesh *meshi;
    int j;
    float vals[8],xx[2],yy[2],zz[2];
    float *xyzmin, *xyzmax;
    float level;

    meshi = meshinfo + i;

    xyzmin = meshi->boxmin;
    xyzmax = meshi->boxmax;
    xx[0]=xyzmin[0];
    yy[0]=xyzmin[1];
    zz[0]=xyzmin[2];
    xx[1]=xyzmax[0];
    yy[1]=xyzmax[1];
    zz[1]=xyzmax[2];
    for(j=0;j<8;j++){
      float xyz[3];

      xyz[0] = xx[ix[j]];
      xyz[1] = yy[iy[j]];
      xyz[2] = zz[iz[j]];
      vals[j] = plane_dist(norm,xyz0,xyz);
    }
    level=0.0;
    getisobox(xx,yy,zz,vals,level,
      meshi->gslice_verts,&meshi->gslice_nverts,meshi->gslice_triangles,&meshi->gslice_ntriangles);
      meshi->gslice_ntriangles/=3;
  }
}

/* ------------------ drawgslice_outline ------------------------ */

void getNormal(float *v1, float *v2, float *v3, float *area, float *normal);
void drawgslice_outline(void){
  int i;
  float zero[3]={0.0,0.0,0.0};

  glPushMatrix();
  glScalef(1.0/xyzmaxdiff,1.0/xyzmaxdiff,1.0/xyzmaxdiff);
  glTranslatef(-xbar0,-ybar0,-zbar0);

  glColor3fv(foregroundcolor);

  if(show_gslice_triangles==1){
    glBegin(GL_LINES);
    for(i=0;i<nmeshes;i++){
      mesh *meshi;
      int j;

      meshi = meshinfo + i;
      if(meshi->gslice_nverts==0||meshi->gslice_ntriangles==0)continue;
      for(j=0;j<meshi->gslice_ntriangles;j++){
        float *xyz1, *xyz2, *xyz3;

        xyz1 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j];
        xyz2 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+1];
        xyz3 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+2];

        glVertex3fv(xyz1);
        glVertex3fv(xyz2);
        glVertex3fv(xyz2);
        glVertex3fv(xyz3);
        glVertex3fv(xyz3);
        glVertex3fv(xyz1);
      }
    }
    glEnd();
  }

  if(show_gslice_triangulation==1){
    for(i=0;i<nmeshes;i++){
      mesh *meshi;
      int j;
      float del;

      meshi = meshinfo + i;
      del = meshi->cellsize;
      del *= del;
      del /= 4.0;
      if(meshi->gslice_nverts==0||meshi->gslice_ntriangles==0)continue;
      for(j=0;j<meshi->gslice_ntriangles;j++){
        float *xyz1, *xyz2, *xyz3;

        xyz1 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j];
        xyz2 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+1];
        xyz3 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+2];
        draw_triangle_outline(xyz1,xyz2,xyz3,del,0);
      }
    }
  }
  // draw normal vector

  if(show_gslice_normal==1||show_gslice_normal_keyboard==1){
    glColor3f(1.0,0.0,0.0);
    glPushMatrix();
    glTranslatef(gslice_xyz[0],gslice_xyz[1],gslice_xyz[2]);
    glBegin(GL_LINES);
    glVertex3fv(zero);
    glVertex3fv(gslice_norm);
    glEnd();
    glPointSize(20.0);
    glBegin(GL_POINTS);
    glVertex3fv(zero);
    glVertex3fv(gslice_norm);
    glEnd();
    glPopMatrix();
  }

  glPopMatrix();
}

#ifdef pp_GPU

/* ------------------ init_volsmoke_texture ------------------------ */

void init_slice3d_texture(mesh *meshi){
  GLint border_size=0;
  GLsizei nx, ny, nz;

  printf("Defining 3d slice textures for %s ...",meshi->label);
  fflush(stdout);

  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1,&meshi->slice3d_texture_id);
  glBindTexture(GL_TEXTURE_3D,meshi->smoke_texture_id);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  nx = meshi->ibar+1;
  ny = meshi->jbar+1;
  nz = meshi->kbar+1;
  if(meshi->slice3d_texture_buffer==NULL){
    int i;

    NewMemory((void **)&meshi->slice3d_texture_buffer,nx*ny*nz*sizeof(float));
    for(i=0;i<nx*ny*nz;i++){
      meshi->slice3d_texture_buffer[i]=0.0;
    }
  }
  if(meshi->slice3d_c_buffer==NULL){
    NewMemory((void **)&meshi->slice3d_c_buffer,nx*ny*nz*sizeof(float));
  }
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, 
    nx, ny, nz, border_size, 
    GL_RED, GL_FLOAT, meshi->slice3d_texture_buffer);


  if(slice3d_colormap_id_defined==-1){
    slice3d_colormap_id_defined=1;
    glActiveTexture(GL_TEXTURE4);
    glGenTextures(1,&slice3d_colormap_id);
    glBindTexture(GL_TEXTURE_1D,slice3d_colormap_id);
    glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA,256,0,GL_RGBA,GL_FLOAT,rgb_slice);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_slice);
  }

  glActiveTexture(GL_TEXTURE0);
  printf("completed\n");
  fflush(stdout);
}
#endif

/* ------------------ update_slice3d_texture ------------------------ */

void update_slice3d_texture(mesh *meshi, float *valdata){
  GLint xoffset=0,yoffset=0,zoffset=0;
  GLsizei nx, ny, nz, nxy;
  int i, j, k;
  float *cbuffer;

//  glGetIntegerv(GL_MAX_TEXTURE_COORDS,&ntextures);
  nx = meshi->ibar+1;
  ny = meshi->jbar+1;
  nz = meshi->kbar+1;
  nxy = nx*ny;
  cbuffer = meshi->slice3d_c_buffer;
  for(i=0;i<nx;i++){
    for(j=0;j<ny;j++){
      for(k=0;k<nz;k++){
        cbuffer[IJKNODE(i,j,k)]=valdata[k+j*nz+i*nz*ny];
      }
    }
  }

  glActiveTexture(GL_TEXTURE0);
  glTexSubImage3D(GL_TEXTURE_3D,0,
    xoffset,yoffset,zoffset,
    nx, ny, nz,
    GL_RED, GL_FLOAT, cbuffer);
}

/* ------------------ drawgslice_dataGPU ------------------------ */

void drawgslice_dataGPU(slicedata *slicei){
  mesh *meshi;
  int j;
  databounds *sb;
  float valmin, valmax;
  float *boxmin, *boxmax;

  if(slicei->loaded==0||slicei->display==0||slicei->volslice==0)return;

  meshi = meshinfo + slicei->blocknumber;
  if(meshi->gslice_nverts==0||meshi->gslice_ntriangles==0)return;

  update_slice3d_texture(meshi, slicei->qsliceframe);
  glPushMatrix();
  glScalef(1.0/xyzmaxdiff,1.0/xyzmaxdiff,1.0/xyzmaxdiff);
  glTranslatef(-xbar0,-ybar0,-zbar0);

  if(cullfaces==1)glDisable(GL_CULL_FACE);
  if(use_transparency_data==1)transparenton();


  sb=slicebounds+islicetype;
  valmin = sb->levels256[0];
  valmax = sb->levels256[255];
  boxmin=meshi->boxmin;
  boxmax=meshi->boxmax;

  glUniform1i(GPU3dslice_valtexture,0);
  glUniform1i(GPU3dslice_colormap,4);
  glUniform1f(GPU3dslice_val_min,valmin);
  glUniform1f(GPU3dslice_val_max,valmax);
  glUniform1f(GPU3dslice_transparent_level,transparent_level);
  glUniform3f(GPU3dslice_boxmin,boxmin[0],boxmin[1],boxmin[2]);
  glUniform3f(GPU3dslice_boxmax,boxmax[0],boxmax[1],boxmax[2]);
  glBegin(GL_TRIANGLES);

  for(j=0;j<meshi->gslice_ntriangles;j++){
    float *xyz1, *xyz2, *xyz3;

    xyz1 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j];
    xyz2 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+1];
    xyz3 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+2];
    glVertex3fv(xyz1);
    glVertex3fv(xyz2);
    glVertex3fv(xyz3);
  }
  glEnd();
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);
  glPopMatrix();
}
#endif

/* ------------------ drawgslice_data ------------------------ */

void drawgslice_data(slicedata *slicei){
  mesh *meshi;
  int j;
  databounds *sb;
  float valmin, valmax;
  float del;
  float dval;

  if(slicei->loaded==0||slicei->display==0||slicei->volslice==0)return;

  meshi = meshinfo + slicei->blocknumber;
  if(meshi->gslice_nverts==0||meshi->gslice_ntriangles==0)return;
  del = meshi->cellsize;
  del *= del;
  del /= 4.0;

  glPushMatrix();
  glScalef(1.0/xyzmaxdiff,1.0/xyzmaxdiff,1.0/xyzmaxdiff);
  glTranslatef(-xbar0,-ybar0,-zbar0);

  if(cullfaces==1)glDisable(GL_CULL_FACE);
  if(use_transparency_data==1)transparenton();

  sb=slicebounds+islicetype;
  valmin = sb->levels256[0];
  valmax = sb->levels256[255];
  dval = (valmax-valmin)/255.0;
  
  gslicedata=slicei->qsliceframe;
  gslice_valmin=valmin;
  gslice_valmax=valmax;
  gslice_valmesh=meshi;

  for(j=0;j<meshi->gslice_ntriangles;j++){
    float *xyz1, *xyz2, *xyz3;
    float t1, t2, t3;

    xyz1 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j];
    xyz2 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+1];
    xyz3 = meshi->gslice_verts + 3*meshi->gslice_triangles[3*j+2];
    t1 = get_texture_index(xyz1);
    t2 = get_texture_index(xyz2);
    t3 = get_texture_index(xyz3);
  
    draw_triangle(xyz1,xyz2,xyz3,t1,t2,t3,del,0);
  }
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);
  glPopMatrix();
}

/* ------------------ drawslice_terrain ------------------------ */

#define ijnode2(i,j) ((nycell+1)*(i) + (j))


/* ------------------ drawvolslice_texture ------------------------ */

void drawvolslice_texture(const slicedata *sd){
  int i,j,k,n,n2;
  float r11, r31, r13, r33;
  float constval,x1,x3,yy1,y3,z1,z3;
  int maxj;

  float *xplt, *yplt, *zplt;
  int ibar,jbar;
  int nx,ny,nxy;
  char *iblank_x, *iblank_y, *iblank_z;
  char *iblank_embed;
  int plotx, ploty, plotz;

  mesh *meshi;

  if(sd->volslice==1&&visx_all==0&&visy_all==0&&visz_all==0)return;
  meshi = meshinfo + sd->blocknumber;

  xplt=meshi->xplt;
  yplt=meshi->yplt;
  zplt=meshi->zplt;
  if(sd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }
  ibar=meshi->ibar;
  jbar=meshi->jbar;
  iblank_x=meshi->c_iblank_x;
  iblank_y=meshi->c_iblank_y;
  iblank_z=meshi->c_iblank_z;
  iblank_embed = meshi->c_iblank_embed;
  nx = ibar + 1;
  ny = jbar + 1;
  nxy = nx*ny;

  if(cullfaces==1)glDisable(GL_CULL_FACE);
  if(use_transparency_data==1)transparenton();
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  glEnable(GL_TEXTURE_1D);
  glBindTexture(GL_TEXTURE_1D,texture_slice_colorbar_id);

  if((sd->volslice==1&&plotx>=0&&visx_all==1)||(sd->volslice==0&&sd->idir==1)){
   constval = xplt[plotx]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   maxj = sd->js2;
   if(sd->js1+1>maxj){
     maxj=sd->js1+1;
   }
   for(j=sd->js1; j<maxj; j++){
     float ymid;

     n = (j-sd->js1)*sd->nslicek -1;
     n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
     n2 = n + sd->nslicek;
     yy1 = yplt[j];
     y3 = yplt[j+1];
     ymid = (yy1+y3)/2.0;

     // val(i,j,k) = di*nj*nk + dj*nk + dk
     for(k=sd->ks1; k<sd->ks2; k++){
       float rmid, zmid;

       n++; n2++; 
       if(iblank_x[IJK(plotx,j,k)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(plotx,j,k)]==0)continue;
       r11 = (float)sd->iqsliceframe[n]/255.0;
       r31 = (float)sd->iqsliceframe[n2]/255.0;
       r13 = (float)sd->iqsliceframe[n+1]/255.0;
       r33 = (float)sd->iqsliceframe[n2+1]/255.0;
       rmid = (r11+r31+r13+r33)/4.0;

       z1 = zplt[k];
       z3 = zplt[k+1];
       zmid = (z1+z3)/2.0;

       /*
       n+1 (y1,z3) n2+1 (y3,z3)
         n (y1,z1)     n2 (y3,z1)
       */
       //  (yy1,z3,r13)                    (y3,z3,r33)
       //                (ymid,zmid,rmid)
       //  (yy1,z1,r11)                    (y3,z1,r31)
       glTexCoord1f( r11); glVertex3f(constval, yy1,  z1);
       glTexCoord1f( r31); glVertex3f(constval,  y3,  z1);
       glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
       glTexCoord1f( r31); glVertex3f(constval,  y3,  z1);
       glTexCoord1f( r33); glVertex3f(constval,  y3,  z3);
       glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
       glTexCoord1f( r33); glVertex3f(constval,  y3,  z3);
       glTexCoord1f( r13); glVertex3f(constval, yy1,  z3);
       glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
       glTexCoord1f( r13); glVertex3f(constval, yy1,  z3);
       glTexCoord1f( r11); glVertex3f(constval, yy1,  z1);
       glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
     }
   }
   glEnd();
  }
  if((sd->volslice==1&&ploty>=0&&visy_all==1)||(sd->volslice==0&&sd->idir==2)){
   constval = yplt[ploty]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
     float xmid;

     n = (i-sd->is1)*sd->nslicej*sd->nslicek -1;
     n += (ploty-sd->js1)*sd->nslicek;
     n2 = n + sd->nslicej*sd->nslicek;

     x1 = xplt[i];
     x3 = xplt[i+1];
     xmid = (x1+x3)/2.0;

     for(k=sd->ks1; k<sd->ks2; k++){
       float rmid, zmid;

       n++; n2++; 
       if(iblank_y[IJK(i,ploty,k)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,ploty,k)]==0)continue;
       r11 = (float)sd->iqsliceframe[n]/255.0;
       r31 = (float)sd->iqsliceframe[n2]/255.0;
       r13 = (float)sd->iqsliceframe[n+1]/255.0;
       r33 = (float)sd->iqsliceframe[n2+1]/255.0;
       rmid = (r11+r31+r13+r33)/4.0;

       z1 = zplt[k];
       z3 = zplt[k+1];
       zmid = (z1+z3)/2.0;

       /*
       n+1 (x1,z3)   n2+1 (x3,z3)
         n (x1,z1)     n2 (x3,z1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       //  (x1,z3,r13)                    (x3,z3,r33)
       //                (xmid,zmid,rmid)
       //  (x1,z1,r11)                    (x3,z1,r31)
       glTexCoord1f( r11); glVertex3f(  x1,constval,  z1);
       glTexCoord1f( r31); glVertex3f(  x3,constval,  z1);
       glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
       glTexCoord1f( r31); glVertex3f(  x3,constval,  z1);
       glTexCoord1f( r33); glVertex3f(  x3,constval,  z3);
       glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
       glTexCoord1f( r33); glVertex3f(  x3,constval,  z3);
       glTexCoord1f( r13); glVertex3f(  x1,constval,  z3);
       glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
       glTexCoord1f( r13); glVertex3f(  x1,constval,  z3);
       glTexCoord1f( r11); glVertex3f(  x1,constval,  z1);
       glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
     }
   }
   glEnd();
  }
  if((sd->volslice==1&&plotz>=0&&visz_all==1)||(sd->volslice==0&&sd->idir==3)){
   constval = zplt[plotz]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
     float xmid;

     n = (i-sd->is1)*sd->nslicej*sd->nslicek -sd->nslicek;
     n += (plotz-sd->ks1);
     n2 = n + sd->nslicej*sd->nslicek;

     x1 = xplt[i];
     x3 = xplt[i+1];
     xmid = (x1+x3)/2.0;

     for(j=sd->js1; j<sd->js2; j++){
       float ymid, rmid;

        n+=sd->nslicek; 
       n2+=sd->nslicek; 
       if(iblank_z[IJK(i,j,plotz)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,j,plotz)]==0)continue;
       r11 = (float)sd->iqsliceframe[n]/255.0;
       r31 = (float)sd->iqsliceframe[n2]/255.0;
       r13 = (float)sd->iqsliceframe[ n+sd->nslicek]/255.0;
       r33 = (float)sd->iqsliceframe[n2+sd->nslicek]/255.0;
       rmid = (r11+r31+r13+r33)/4.0;

       yy1 = yplt[j];
       y3 = yplt[j+1];
       ymid = (yy1+y3)/2.0;

       /*
       n+nk (x1,y3)   n2+nk (x3,y3)
          n (x1,y1)      n2 (x3,y1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       //  (x1,y3,r13)                    (x3,y3,r33)
       //                (xmid,ymid,rmid)
       //  (x1,yy1,r11)                    (x3,yy1,r31)
       glTexCoord1f( r11); glVertex3f(  x1,  yy1, constval);
       glTexCoord1f( r31); glVertex3f(  x3,  yy1, constval);
       glTexCoord1f(rmid); glVertex3f(xmid,ymid, constval);
       glTexCoord1f( r31); glVertex3f(  x3,  yy1, constval);
       glTexCoord1f( r33); glVertex3f(  x3,  y3, constval);
       glTexCoord1f(rmid); glVertex3f(xmid,ymid, constval);
       glTexCoord1f( r33); glVertex3f(  x3,  y3, constval);
       glTexCoord1f( r13); glVertex3f(  x1,  y3, constval);
       glTexCoord1f(rmid); glVertex3f(xmid,ymid, constval);
       glTexCoord1f( r13); glVertex3f(  x1,  y3, constval);
       glTexCoord1f( r11); glVertex3f(  x1,  yy1, constval);
       glTexCoord1f(rmid); glVertex3f(xmid,ymid, constval);
     }
   }
   glEnd();
  }
  glDisable(GL_TEXTURE_1D);
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);
}

/* ------------------ drawvolslice_terrain ------------------------ */

void drawvolslice_terrain(const slicedata *sd){
  int i,j,k,n,n2;
  float r11, r31, r13, r33;
  float constval,x1,x3,yy1,y3,z1,z3;
  int maxj;

  float *xplt, *yplt, *zplt;
  int plotx, ploty, plotz;
  int ibar,jbar;
  int nx,ny,nxy;
  char *iblank_x, *iblank_y, *iblank_z;
  terraindata *terri;
  float *znode;
  int nycell;
  char *iblank_embed;

  mesh *meshi;

  meshi = meshinfo + sd->blocknumber;

  terri = meshi->terrain;
  if(terri==NULL)return;
  znode = terri->znode_scaled;
  nycell = terri->ny;

  xplt=meshi->xplt;
  yplt=meshi->yplt;
  zplt=meshi->zplt;
  if(sd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }
  ibar=meshi->ibar;
  jbar=meshi->jbar;
  iblank_x=meshi->c_iblank_x;
  iblank_y=meshi->c_iblank_y;
  iblank_z=meshi->c_iblank_z;
  iblank_embed = meshi->c_iblank_embed;
  nx = ibar + 1;
  ny = jbar + 1;
  nxy = nx*ny;

  if(cullfaces==1)glDisable(GL_CULL_FACE);

  if(use_transparency_data==1)transparenton();
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  glEnable(GL_TEXTURE_1D);
  glBindTexture(GL_TEXTURE_1D,texture_slice_colorbar_id);
  if((sd->volslice==1&&plotx>=0&&visx_all==1)||(sd->volslice==0&&sd->idir==1)){
    constval = xplt[plotx]+offset_slice*sd->sliceoffset;
    glBegin(GL_TRIANGLES);
    maxj = sd->js2;
    if(sd->js1+1>maxj){
      maxj=sd->js1+1;
    }
    for(j=sd->js1; j<maxj; j++){
      float ymid;

      n = (j-sd->js1)*sd->nslicek -1;
      n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
      n2 = n + sd->nslicek;
      yy1 = yplt[j];
      y3 = yplt[j+1];
      ymid = (yy1+y3)/2.0;

     // val(i,j,k) = di*nj*nk + dj*nk + dk
      for(k=sd->ks1; k<sd->ks2; k++){
        float rmid, zmid;

        n++; n2++; 
        if(iblank_x[IJK(plotx,j,k)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(plotx,j,k)]==0)continue;
        r11 = (float)sd->iqsliceframe[n]/255.0;
        r31 = (float)sd->iqsliceframe[n2]/255.0;
        r13 = (float)sd->iqsliceframe[n+1]/255.0;
        r33 = (float)sd->iqsliceframe[n2+1]/255.0;
        rmid = (r11+r31+r13+r33)/4.0;

        z1 = zplt[k];
        z3 = zplt[k+1];
        zmid = (z1+z3)/2.0;

       /*
       n+1 (y1,z3) n2+1 (y3,z3)
         n (y1,z1)     n2 (y3,z1)
       */
       //  (yy1,z3,r13)                    (y3,z3,r33)
       //                (ymid,zmid,rmid)
       //  (yy1,z1,r11)                    (y3,z1,r31)
        glTexCoord1f( r11); glVertex3f(constval, yy1,  z1);
        glTexCoord1f( r31); glVertex3f(constval,  y3,  z1);
        glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
        glTexCoord1f( r31); glVertex3f(constval,  y3,  z1);
        glTexCoord1f( r33); glVertex3f(constval,  y3,  z3);
        glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
        glTexCoord1f( r33); glVertex3f(constval,  y3,  z3);
        glTexCoord1f( r13); glVertex3f(constval, yy1,  z3);
        glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
        glTexCoord1f( r13); glVertex3f(constval, yy1,  z3);
        glTexCoord1f( r11); glVertex3f(constval, yy1,  z1);
        glTexCoord1f(rmid); glVertex3f(constval,ymid,zmid);
      }
    }
    glEnd();
  }
  if((sd->volslice==1&&ploty>=0&&visy_all==1)||(sd->volslice==0&&sd->idir==2)){
    constval = yplt[ploty]+offset_slice*sd->sliceoffset;
    glBegin(GL_TRIANGLES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float xmid;

      n = (i-sd->is1)*sd->nslicej*sd->nslicek -1;
      n += (ploty-sd->js1)*sd->nslicek;
      n2 = n + sd->nslicej*sd->nslicek;

      x1 = xplt[i];
      x3 = xplt[i+1];
      xmid = (x1+x3)/2.0;

      for(k=sd->ks1; k<sd->ks2; k++){
        float rmid, zmid;

        n++; n2++; 
        if(iblank_y[IJK(i,ploty,k)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,ploty,k)]==0)continue;
        r11 = (float)sd->iqsliceframe[n]/255.0;
        r31 = (float)sd->iqsliceframe[n2]/255.0;
        r13 = (float)sd->iqsliceframe[n+1]/255.0;
        r33 = (float)sd->iqsliceframe[n2+1]/255.0;
        rmid = (r11+r31+r13+r33)/4.0;

        z1 = zplt[k];
        z3 = zplt[k+1];
        zmid = (z1+z3)/2.0;

       /*
       n+1 (x1,z3)   n2+1 (x3,z3)
         n (x1,z1)     n2 (x3,z1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       //  (x1,z3,r13)                    (x3,z3,r33)
       //                (xmid,zmid,rmid)
       //  (x1,z1,r11)                    (x3,z1,r31)
        glTexCoord1f( r11); glVertex3f(  x1,constval,  z1);
        glTexCoord1f( r31); glVertex3f(  x3,constval,  z1);
        glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
        glTexCoord1f( r31); glVertex3f(  x3,constval,  z1);
        glTexCoord1f( r33); glVertex3f(  x3,constval,  z3);
        glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
        glTexCoord1f( r33); glVertex3f(  x3,constval,  z3);
        glTexCoord1f( r13); glVertex3f(  x1,constval,  z3);
        glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
        glTexCoord1f( r13); glVertex3f(  x1,constval,  z3);
        glTexCoord1f( r11); glVertex3f(  x1,constval,  z1);
        glTexCoord1f(rmid); glVertex3f(xmid,constval,zmid);
      }
    }
    glEnd();
  }
  if((sd->volslice==1&&plotz>=0&&visz_all==1)||(sd->volslice==0&&sd->idir==3)){
    float z11, z31, z13, z33, zmid, zmax;

    constval = zplt[plotz]+offset_slice*sd->sliceoffset-znode[0];
    zmax = zplt[meshi->kbar];
    glBegin(GL_TRIANGLES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float xmid;

      if(plotz<sd->ks1)break;
      if(plotz>=sd->ks1+sd->nslicek)break;
      x1 = xplt[i];
      x3 = xplt[i+1];
      xmid = (x1+x3)/2.0;

      for(j=sd->js1; j<sd->js2; j++){
        float ymid, rmid;
        int n11, n31, n13, n33;

        z11 = constval + znode[ijnode2(i,j)];
        if(z11>zmax)z11=zmax;
        z31 = constval + znode[ijnode2(i+1,j)];
        if(z31>zmax)z31=zmax;
        z13 = constval + znode[ijnode2(i,j+1)];
        if(z13>zmax)z13=zmax;
        z33 = constval + znode[ijnode2(i+1,j+1)];
        if(z33>zmax)z33=zmax;
        zmid = (z11 + z31 + z13 + z33)/4.0;

        if(iblank_z[IJK(i,j,plotz)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,j,plotz)]==0)continue;

        n11=i*sd->nslicej*sd->nslicek+j*sd->nslicek;
        r11 = interp3dsliceindex(sd->iqsliceframe,zplt,meshi->kbar,n11,z11)/255.0;

        n31=n11 + sd->nslicej*sd->nslicek;
        r31 = interp3dsliceindex(sd->iqsliceframe,zplt,meshi->kbar,n31,z31)/255.0;

        n13=n11 + sd->nslicek;
        r13 = interp3dsliceindex(sd->iqsliceframe,zplt,meshi->kbar,n13,z13)/255.0;

        n33=n13 + sd->nslicej*sd->nslicek;
        r33 = interp3dsliceindex(sd->iqsliceframe,zplt,meshi->kbar,n33,z33)/255.0;

        rmid = (r11+r31+r13+r33)/4.0;


        yy1 = yplt[j];
        y3 = yplt[j+1];
        ymid = (yy1+y3)/2.0;

       /*
       n+nk (x1,y3)   n2+nk (x3,y3)
          n (x1,y1)      n2 (x3,y1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       //  (x1,y3,r13,z13)                    (x3,y3,r33,z33)
       //                (xmid,ymid,rmid,zmid)
       //  (x1,yy1,r11,z11)                    (x3,yy1,r31,z31)

        glTexCoord1f( r11); glVertex3f(  x1,  yy1, z11);
        glTexCoord1f( r31); glVertex3f(  x3,  yy1, z31);
        glTexCoord1f(rmid); glVertex3f(xmid,ymid, zmid);

        glTexCoord1f( r31); glVertex3f(  x3,  yy1, z31);
        glTexCoord1f( r33); glVertex3f(  x3,  y3, z33);
        glTexCoord1f(rmid); glVertex3f(xmid,ymid, zmid);

        glTexCoord1f( r33); glVertex3f(  x3,  y3, z33);
        glTexCoord1f( r13); glVertex3f(  x1,  y3, z13);
        glTexCoord1f(rmid); glVertex3f(xmid,ymid, zmid);

        glTexCoord1f( r13); glVertex3f(  x1,  y3, z13);
        glTexCoord1f( r11); glVertex3f(  x1,  yy1, z11);
        glTexCoord1f(rmid); glVertex3f(xmid,ymid, zmid);
      }
    }
    glEnd();
  }
  glDisable(GL_TEXTURE_1D);
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);

}

/* ------------------ drawvolslice ------------------------ */

void drawvolslice_cellcenter(const slicedata *sd){
  float *xplt, *yplt, *zplt;
  int plotx, ploty, plotz;
  int ibar,jbar;
  int nx,ny,nxy;
  char *iblank_x, *iblank_y, *iblank_z;
  char *iblank_embed;
  int incx=0, incy=0, incz=0;

  mesh *meshi;

  float *rgb_ptr;

  rgb_ptr = rgb_slice;

  meshi = meshinfo + sd->blocknumber;

  xplt=meshi->xplt;
  yplt=meshi->yplt;
  zplt=meshi->zplt;
  if(sd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
    incx=1;
    incy=1;
    incz=1;
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }
  if(plotx<=1)plotx=1;
  if(ploty<=1)ploty=1;
  if(plotz<=1)plotz=1;

  ibar=meshi->ibar;
  jbar=meshi->jbar;
  iblank_x=meshi->c_iblank_x;
  iblank_y=meshi->c_iblank_y;
  iblank_z=meshi->c_iblank_z;
  iblank_embed = meshi->c_iblank_embed;
  nx = ibar + 1;
  ny = jbar + 1;
  nxy = nx*ny;

  if(cullfaces==1)glDisable(GL_CULL_FACE);

  if(use_transparency_data==1)transparenton();
  if((sd->volslice==1&&plotx>=0&&visx_all==1)||(sd->volslice==0&&sd->idir==1)){
    float constval;
    int maxj;
    int j;

    constval = (xplt[plotx]+xplt[plotx-1])/2.0;
    glBegin(GL_TRIANGLES);
    maxj = sd->js2;
    if(sd->js1+1>maxj){
      maxj=sd->js1+1;
    }
    for(j=sd->js1; j<maxj; j++){
      float yy1;
      int k;
      float y3;

      yy1 = yplt[j];
      y3 = yplt[j+1];
     // val(i,j,k) = di*nj*nk + dj*nk + dk
      for(k=sd->ks1; k<sd->ks2; k++){
        int index_cell;
        int i33;
        float z1,z3;

        if(sd->slicetype!=SLICE_CENTER&&iblank_x[IJK(plotx,j,k)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(plotx,j,k)]==0)continue;

        index_cell = (plotx + 1 -incx-sd->is1)*sd->nslicej*sd->nslicek + (j+1-sd->js1)*sd->nslicek + k + 1 - sd->ks1;

        i33 = 4*sd->iqsliceframe[index_cell];
        z1 = zplt[k];
        z3 = zplt[k+1];
       /*
       n+1 (y1,z3) n2+1 (y3,z3)
         n (y1,z1)     n2 (y3,z1)
       */

        glColor4fv(&rgb_ptr[i33]);
        glVertex3f(constval,yy1,z1);
        glVertex3f(constval,y3,z1);
        glVertex3f(constval,y3,z3);

        glVertex3f(constval,yy1,z1);
        glVertex3f(constval,y3,z3);
        glVertex3f(constval,yy1,z3);
      }
    }
    glEnd();
    if(cell_center_text==1){
      for(j=sd->js1; j<maxj; j++){
        float yy1,y3;
        int k;

        yy1 = yplt[j];
        y3 = yplt[j+1];
     // val(i,j,k) = di*nj*nk + dj*nk + dk
        for(k=sd->ks1; k<sd->ks2; k++){
          float val;
          int index_cell;
          float z1, z3;

          if(sd->slicetype!=SLICE_CENTER&&iblank_x[IJK(plotx,j,k)]!=2)continue;
          if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(plotx,j,k)]==0)continue;
          z1 = zplt[k];
          z3 = zplt[k+1];
       /*
       n+1 (y1,z3) n2+1 (y3,z3)
         n (y1,z1)     n2 (y3,z1)
       */
          index_cell = (plotx + 1 -incx-sd->is1)*sd->nslicej*sd->nslicek + (j+1-sd->js1)*sd->nslicek + k + 1 - sd->ks1;

          GET_VAL(sd,val,index_cell);
          output3Val(constval,(yy1+y3)/2.0,(z1+z3)/2.0,val);
        }
      }
    }
  }
  if((sd->volslice==1&&ploty>=0&&visy_all==1)||(sd->volslice==0&&sd->idir==2)){
    float constval;
    int i;

    constval = (yplt[ploty]+yplt[ploty-1])/2.0;
    glBegin(GL_TRIANGLES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      int index_cell;
      float x1, x3;
      int k;

      x1 = xplt[i];
      x3 = xplt[i+1];
      for(k=sd->ks1; k<sd->ks2; k++){
        int i33;
        float z1, z3;

        if(iblank_y[IJK(i,ploty,k)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,ploty,k)]==0)continue;
        index_cell = (i+incx-sd->is1)*sd->nslicej*sd->nslicek + (ploty+1-incy-sd->js1)*sd->nslicek + k+1 - sd->ks1;
        i33 = 4*sd->iqsliceframe[index_cell];
        z1 = zplt[k];
        z3 = zplt[k+1];
       /*
       n+1 (x1,z3)   n2+1 (x3,z3)
         n (x1,z1)     n2 (x3,z1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
        glColor4fv(&rgb_ptr[i33]);
        glVertex3f(x1,constval,z1);
        glVertex3f(x3,constval,z1);
        glVertex3f(x3,constval,z3);

        glVertex3f(x1,constval,z1);
        glVertex3f(x3,constval,z3);
        glVertex3f(x1,constval,z3);
      }
    }
    glEnd();
    if(cell_center_text==1){
      for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
        float x1, x3;
        int k;

        x1 = xplt[i];
        x3 = xplt[i+1];
        for(k=sd->ks1; k<sd->ks2; k++){
          float val;
          int index_cell;
          int i33;
          float z1, z3;

          if(iblank_y[IJK(i,ploty,k)]!=2)continue;
          if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,ploty,k)]==0)continue;
          index_cell = (i+incx-sd->is1)*sd->nslicej*sd->nslicek + (ploty+1-incy-sd->js1)*sd->nslicek + k+1 - sd->ks1;
          i33 = 4*sd->iqsliceframe[index_cell];
          z1 = zplt[k];
          z3 = zplt[k+1];
       /*
       n+1 (x1,z3)   n2+1 (x3,z3)
         n (x1,z1)     n2 (x3,z1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
          GET_VAL(sd,val,index_cell);
          output3Val((x1+x3)/2.0,constval,(z1+z3)/2.0,val);
        }
      }
    }
  }
  if((sd->volslice==1&&plotz>=0&&visz_all==1)||(sd->volslice==0&&sd->idir==3)){
    float constval;
    int i;

    constval = (zplt[plotz]+zplt[plotz-1])/2.0;
    glBegin(GL_TRIANGLES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float x1, x3;
      int j;

      x1 = xplt[i];
      x3 = xplt[i+1];
      for(j=sd->js1; j<sd->js2; j++){
        int index_cell;
        int i33;
        float yy1, y3;

        if(iblank_z[IJK(i,j,plotz)]!=2)continue;
        if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,j,plotz)]==0)continue;
        index_cell = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (j+incy-sd->js1)*sd->nslicek + plotz + 1 -incz- sd->ks1;
        i33 = 4*sd->iqsliceframe[index_cell];
        yy1 = yplt[j];
        y3 = yplt[j+1];
       /*
       n+nk (x1,y3)   n2+nk (x3,y3)
          n (x1,y1)      n2 (x3,y1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
        glColor4fv(&rgb_ptr[i33]);
        glVertex3f(x1,yy1,constval);
        glVertex3f(x3,yy1,constval);
        glVertex3f(x3,y3,constval);

        glVertex3f(x1,yy1,constval);
        glVertex3f(x3,y3,constval);
        glVertex3f(x1,y3,constval);
      }
    }
    glEnd();
    if(cell_center_text==1){
      for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
        float x1, x3;
        int j;

        x1 = xplt[i];
        x3 = xplt[i+1];
        for(j=sd->js1; j<sd->js2; j++){
          float val;
          int index_cell;
          int i33;
          float yy1, y3;

          index_cell = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (j+incy-sd->js1)*sd->nslicek + plotz + 1 -incz- sd->ks1;
          if(iblank_z[IJK(i,j,plotz)]!=2)continue;
          if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,j,plotz)]==0)continue;
          i33 = 4*sd->iqsliceframe[index_cell];
          yy1 = yplt[j];
          y3 = yplt[j+1];
       /*
       n+nk (x1,y3)   n2+nk (x3,y3)
          n (x1,y1)      n2 (x3,y1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
          GET_VAL(sd,val,index_cell);
          output3Val((x1+x3)/2.0,(yy1+y3)/2.0,constval,val);
        }
      }
    }
  }
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);

}

/* ------------------ drawvolslice ------------------------ */

void drawvolslice(const slicedata *sd){
  int i,j,k,n,n2;
  int i11, i31, i13, i33;
  float constval,x1,x3,yy1,y3,z1,z3;
  int maxj;

  float *xplt, *yplt, *zplt;
  int plotx, ploty, plotz;
  int ibar,jbar;
  int nx,ny,nxy;
  char *iblank_x, *iblank_y, *iblank_z;
  char *iblank_embed;

  mesh *meshi;

  float *rgb_ptr;

  rgb_ptr = rgb_slice;

  meshi = meshinfo + sd->blocknumber;

  xplt=meshi->xplt;
  yplt=meshi->yplt;
  zplt=meshi->zplt;
  if(sd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }
  ibar=meshi->ibar;
  jbar=meshi->jbar;
  iblank_x=meshi->c_iblank_x;
  iblank_y=meshi->c_iblank_y;
  iblank_z=meshi->c_iblank_z;
  iblank_embed = meshi->c_iblank_embed;
  nx = ibar + 1;
  ny = jbar + 1;
  nxy = nx*ny;

  if(cullfaces==1)glDisable(GL_CULL_FACE);

  if(use_transparency_data==1)transparenton();
  if((sd->volslice==1&&plotx>=0&&visx_all==1)||(sd->volslice==0&&sd->idir==1)){
   constval = xplt[plotx]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   maxj = sd->js2;
   if(sd->js1+1>maxj){
     maxj=sd->js1+1;
   }
   for(j=sd->js1; j<maxj; j++){
     n = (j-sd->js1)*sd->nslicek -1;
     n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
     n2 = n + sd->nslicek;
     yy1 = yplt[j];
     y3 = yplt[j+1];
     // val(i,j,k) = di*nj*nk + dj*nk + dk
     for(k=sd->ks1; k<sd->ks2; k++){
       n++; n2++; 
       if(iblank_x[IJK(plotx,j,k)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(plotx,j,k)]==0)continue;
       i11 = 4*sd->iqsliceframe[n];
       i31 = 4*sd->iqsliceframe[n2];
       i13 = 4*sd->iqsliceframe[n+1];
       i33 = 4*sd->iqsliceframe[n2+1];
       z1 = zplt[k];
       z3 = zplt[k+1];
       /*
       n+1 (y1,z3)     n2+1 (y3,z3)
         n (y1,z1)     n2   (y3,z1)
       */
       if(ABS(i11-i33)<ABS(i13-i31)){
         glColor4fv(&rgb_ptr[i11]);glVertex3f(constval,yy1,z1);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(constval,y3,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(constval,y3,z3);

         glColor4fv(&rgb_ptr[i11]);glVertex3f(constval,yy1,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(constval,y3,z3);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(constval,yy1,z3);
       }
       else{
         glColor4fv(&rgb_ptr[i11]);glVertex3f(constval,yy1,z1);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(constval,y3,z1);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(constval,yy1,z3);

         glColor4fv(&rgb_ptr[i31]);glVertex3f(constval,y3,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(constval,y3,z3);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(constval,yy1,z3);
       }
     }
   }
   glEnd();
  }
  if((sd->volslice==1&&ploty>=0&&visy_all==1)||(sd->volslice==0&&sd->idir==2)){
   constval = yplt[ploty]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
     n = (i-sd->is1)*sd->nslicej*sd->nslicek -1;
     n += (ploty-sd->js1)*sd->nslicek;
     n2 = n + sd->nslicej*sd->nslicek;

     x1 = xplt[i];
     x3 = xplt[i+1];
     for(k=sd->ks1; k<sd->ks2; k++){
       n++; n2++; 
       if(iblank_y[IJK(i,ploty,k)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,sd->js1,k)]==0)continue;
       i11 = 4*sd->iqsliceframe[n];
       i31 = 4*sd->iqsliceframe[n2];
       i13 = 4*sd->iqsliceframe[n+1];
       i33 = 4*sd->iqsliceframe[n2+1];
       z1 = zplt[k];
       z3 = zplt[k+1];
       /*
       n+1 (x1,z3)   n2+1 (x3,z3)
         n (x1,z1)     n2 (x3,z1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       if(ABS(i11-i33)<ABS(i13-i31)){
         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,constval,z1);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,constval,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,constval,z3);

         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,constval,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,constval,z3);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,constval,z3);
       }
       else{
         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,constval,z1);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,constval,z1);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,constval,z3);

         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,constval,z1);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,constval,z3);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,constval,z3);
       }
     }
   }
   glEnd();
  }
  // i*nj*nk + j*nk + k
  if((sd->volslice==1&&plotz>=0&&visz_all==1)||(sd->volslice==0&&sd->idir==3)){
   constval = zplt[plotz]+offset_slice*sd->sliceoffset;
   glBegin(GL_TRIANGLES);
   for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
     n = (i-sd->is1)*sd->nslicej*sd->nslicek -sd->nslicek;
     n += (plotz-sd->ks1);
     n2 = n + sd->nslicej*sd->nslicek;

     x1 = xplt[i];
     x3 = xplt[i+1];
     for(j=sd->js1; j<sd->js2; j++){
        n+=sd->nslicek; 
       n2+=sd->nslicek; 
       if(iblank_z[IJK(i,j,plotz)]!=2)continue;
       if(skip_slice_in_embedded_mesh==1&&iblank_embed!=NULL&&iblank_embed[IJK(i,j,plotz)]==0)continue;
       i11 = 4*sd->iqsliceframe[n];
       i31 = 4*sd->iqsliceframe[n2];
       i13 = 4*sd->iqsliceframe[ n+sd->nslicek];
       i33 = 4*sd->iqsliceframe[n2+sd->nslicek];
       yy1 = yplt[j];
       y3 = yplt[j+1];
       /*
       n+nk (x1,y3)   n2+nk (x3,y3)
          n (x1,y1)      n2 (x3,y1)

        val(i,j,k) = di*nj*nk + dj*nk + dk
       */
       if(ABS(i11-i33)<ABS(i13-i31)){
         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,yy1,constval);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,yy1,constval);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,y3,constval);

         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,yy1,constval);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,y3,constval);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,y3,constval);
       }
       else{
         glColor4fv(&rgb_ptr[i11]);glVertex3f(x1,yy1,constval);
         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,yy1,constval);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,y3,constval);

         glColor4fv(&rgb_ptr[i31]);glVertex3f(x3,yy1,constval);
         glColor4fv(&rgb_ptr[i33]);glVertex3f(x3,y3,constval);
         glColor4fv(&rgb_ptr[i13]);glVertex3f(x1,y3,constval);
       }
     }
   }
   glEnd();
  }
  if(use_transparency_data==1)transparentoff();
  if(cullfaces==1)glEnable(GL_CULL_FACE);

}

/* ------------------ drawvvolslice ------------------------ */

void drawvvolslice(const vslicedata *vd){
  int i,j,k,n;
  int i11;
  float constval,x1,yy1,z1;
  int maxj;
  slicedata *u, *v, *w,*sd;
  float dx, dy, dz;
  float vrange;
  mesh *meshi;
  float *xplttemp,*yplttemp,*zplttemp;
  int plotx, ploty, plotz;
  char *iblank;
  int nx, ny, nxy;
  float *rgb_ptr;

  sd = sliceinfo + vd->ival;
  meshi=meshinfo+sd->blocknumber;
  xplttemp=meshi->xplt;
  yplttemp=meshi->yplt;
  zplttemp=meshi->zplt;
  if(vd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }

  iblank = meshi->c_iblank_node;
  nx = meshi->ibar+1;
  ny = meshi->jbar+1;
  nxy = nx*ny;


  vrange = velocity_range;
  if(vrange<=0.0)vrange=1.0;
  u = vd->u;
  v = vd->v;
  w = vd->w;
  if((vd->volslice==1&&plotx>=0&&visx_all==1)||(vd->volslice==0&&sd->idir==1)){
    constval = xplttemp[plotx]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    maxj = sd->js2;
    if(sd->js1+1>maxj)maxj=sd->js1+1;
    for(j=sd->js1; j<maxj+1; j+=vectorskip){
      n = (j-sd->js1)*sd->nslicek - vectorskip;
      n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
      yy1 = yplttemp[j];
      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip; 
        if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(plotx,j,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(constval-dx,yy1-dy,z1-dz);
          glVertex3f(constval+dx,yy1+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice:lines dir=1");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    maxj = sd->js2;
    if(sd->js1+1>maxj)maxj=sd->js1+1;
    for(j=sd->js1; j<maxj+1; j+=vectorskip){
      n = (j-sd->js1)*sd->nslicek - vectorskip;
      n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
      yy1 = yplttemp[j];
      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip;
	      if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(plotx,j,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(constval+dx,yy1+dy,z1+dz);
        }
      }
   }
   glEnd();
   SNIFF_ERRORS("after drawvvolslice:points dir=1");
  }
  if((vd->volslice==1&&ploty>=0&&visy_all==1)||(vd->volslice==0&&sd->idir==2)){
    constval = yplttemp[ploty]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip;
      n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];

      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip; 
	      if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(i,ploty,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1-dx,constval-dy,z1-dz);
          glVertex3f(x1+dx,constval+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice:lines dir=2");
    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip;
      n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];

      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip; 
	      if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(i,ploty,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1+dx,constval+dy,z1+dz);
        }
      }
   }
   glEnd();
   SNIFF_ERRORS("after drawvvolslice:points dir=2");
  }
  if((vd->volslice==1&&plotz>=0&&visz_all==1)||(vd->volslice==0&&sd->idir==3)){
    constval = zplttemp[plotz]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip*sd->nslicek;
      n += (plotz-sd->ks1);

      x1 = xplttemp[i];
      for(j=sd->js1; j<sd->js2+1; j+=vectorskip){
        n+=vectorskip*sd->nslicek; 
	      if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(i,j,plotz)]==1&&rgb_ptr[3]>0.5){
          yy1 = yplttemp[j];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1-dx,yy1-dy,constval-dz);
          glVertex3f(x1+dx,yy1+dy,constval+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice:lines dir=3");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip*sd->nslicek;
      n += (plotz-sd->ks1);

      x1 = xplttemp[i];
      for(j=sd->js1; j<sd->js2+1; j+=vectorskip){
        n+=vectorskip*sd->nslicek; 
	      if(sd->constant_color==NULL){
          i11 = sd->iqsliceframe[n];
          rgb_ptr = rgb_slice + 4*i11;
	      }
	      else{
	        rgb_ptr = sd->constant_color;
	      }
        if(iblank[IJK(i,j,plotz)]==1&&rgb_ptr[3]>0.5){
          yy1 = yplttemp[j];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1+dx,yy1+dy,constval+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice:points dir=3");
  }
}

/* ------------------ drawvvolslice_cellcenter ------------------------ */

void drawvvolslice_cellcenter(const vslicedata *vd){
  int i;
  float constval;
  int maxj;
  slicedata *u, *v, *w,*sd;
  float vrange;
  mesh *meshi;
  float *xplttemp,*yplttemp,*zplttemp;
  int plotx, ploty, plotz;
  char *iblank;
  int nx, ny, nxy;

  sd = sliceinfo + vd->ival;
  meshi=meshinfo+sd->blocknumber;
  xplttemp=meshi->xplt;
  yplttemp=meshi->yplt;
  zplttemp=meshi->zplt;
  if(vd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }

  iblank = meshi->c_iblank_node;
  nx = meshi->ibar+1;
  ny = meshi->jbar+1;
  nxy = nx*ny;


  vrange = velocity_range;
  if(vrange<=0.0)vrange=1.0;
  u = vd->u;
  v = vd->v;
  w = vd->w;
  if((vd->volslice==1&&plotx>=0&&visx_all==1)||(vd->volslice==0&&sd->idir==1)){
    int j;

    constval = xplttemp[plotx]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    maxj = sd->js2;
    if(sd->js1+1>maxj)maxj=sd->js1+1;
    for(j=sd->js1; j<maxj+1; j++){
      float yy1, yhalf;
      int k;

      yy1 = yplttemp[j];
      if(j!=maxj)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;
      for(k=sd->ks1; k<sd->ks2+1; k++){
        float zhalf, z1;

        z1 = zplttemp[k];
        if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

//       n = (j-sd->js1)*sd->nslicek - 1;
//       n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;


        if(k!=sd->ks2){
          int index_v;
          float *color_v;
          float dy;

          index_v = (plotx-sd->is1)*sd->nslicej*sd->nslicek + (j-sd->js1)*sd->nslicek + k + 1 - sd->ks1;
          GET_SLICE_COLOR(color_v,index_v);
          GET_VEC_DXYZ(v,dy,index_v);
          glColor4fv(color_v);
          glVertex3f(constval,yy1-dy,zhalf);
          glVertex3f(constval,yy1+dy,zhalf);
        }
        if(j!=maxj){
          int index_w;
          float *color_w;
          float dz;

          index_w = (plotx-sd->is1)*sd->nslicej*sd->nslicek + (j-sd->js1+1)*sd->nslicek + k - sd->ks1;
          GET_SLICE_COLOR(color_w,index_w);
          GET_VEC_DXYZ(w,dz,index_w);
          glColor4fv(color_w);
          glVertex3f(constval,yhalf,z1-dz);
          glVertex3f(constval,yhalf,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:lines dir=1");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(j=sd->js1; j<maxj+1; j++){
      float yy1, yhalf;
      int k;

      yy1 = yplttemp[j];
      if(j!=maxj)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;
      for(k=sd->ks1; k<sd->ks2+1; k++){
        float zhalf, z1;

        z1 = zplttemp[k];
        if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

        if(k!=sd->ks2){
          int index_v;
          float *color_v;
          float dy;

          index_v = (plotx-sd->is1)*sd->nslicej*sd->nslicek + (j-sd->js1)*sd->nslicek + k - sd->ks1 + 1;
          GET_SLICE_COLOR(color_v,index_v);
          GET_VEC_DXYZ(v,dy,index_v);
          glColor4fv(color_v);
          glVertex3f(constval,yy1+dy,zhalf);
        }
        if(j!=maxj){
          int index_w;
          float *color_w;
          float dz;

          index_w = (plotx-sd->is1)*sd->nslicej*sd->nslicek + (j-sd->js1+1)*sd->nslicek + k-sd->ks1;
          GET_SLICE_COLOR(color_w,index_w);
          GET_VEC_DXYZ(w,dz,index_w);
          glColor4fv(color_w);
          glVertex3f(constval,yhalf,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:points dir=1");
    
    if(cell_center_text==1){
      for(j=sd->js1; j<maxj+1; j++){
        float yy1, yhalf;
        int k;

        yy1 = yplttemp[j];
        if(j!=maxj)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;
        for(k=sd->ks1; k<sd->ks2+1; k++){
          float zhalf, z1;

          z1 = zplttemp[k];
          if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

          if(k!=sd->ks2){
            int index_v;
            float val;

            index_v = (plotx-sd->is1)*sd->nslicej*sd->nslicek + (j-sd->js1)*sd->nslicek + k - sd->ks1 + 1;
            GET_VAL(v,val,index_v);
            output3Val(constval,yy1,zhalf,val);
          }
          if(j!=maxj){
            int index_w;
            float val;

            index_w = (plotx-sd->is1)*sd->nslicej*sd->nslicek;
            index_w += (j+1-sd->js1)*sd->nslicek + k-sd->ks1;
            GET_VAL(w,val,index_w);
            output3Val(constval,yhalf,z1,val);
          }
        }
      }
    }
  }
  if((vd->volslice==1&&ploty>=0&&visy_all==1)||(vd->volslice==0&&sd->idir==2)){
    constval = yplttemp[ploty]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float x1, xhalf;
      int k;

     // n = (i-sd->is1)*sd->nslicej*sd->nslicek - 1;
     // n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];
      if(i!=sd->is2)xhalf=(xplttemp[i]+xplttemp[i+1])/2.0;

      for(k=sd->ks1; k<sd->ks2+1; k++){
        float zhalf, z1;

        z1 = zplttemp[k];
        if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

        if(k!=sd->ks2){
          int index_u;
          float *color_u;
          float dx;
          
          index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k + 1 - sd->ks1;
          GET_SLICE_COLOR(color_u,index_u)
          GET_VEC_DXYZ(u,dx,index_u);
          glColor4fv(color_u);
          glVertex3f(x1-dx,constval,zhalf);
          glVertex3f(x1+dx,constval,zhalf);
        }
        if(i!=sd->is2){
          int index_w;
          float *color_w;
          float dz;
           
          index_w = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k - sd->ks1;
          GET_SLICE_COLOR(color_w,index_w)
          GET_VEC_DXYZ(w,dz,index_w);
          glColor4fv(color_w);
          glVertex3f(xhalf,constval,z1-dz);
          glVertex3f(xhalf,constval,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:lines dir=2");
    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float x1, xhalf;
      int k;

     // n = (i-sd->is1)*sd->nslicej*sd->nslicek - 1;
     // n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];
      if(i!=sd->is2)xhalf=(xplttemp[i]+xplttemp[i+1])/2.0;

      for(k=sd->ks1; k<sd->ks2+1; k++){
        float zhalf, z1;

        z1 = zplttemp[k];
        if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

        if(k!=sd->ks2){
          int index_u;
          float *color_u;
          float dx;
          
          index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k + 1 - sd->ks1;
          GET_SLICE_COLOR(color_u,index_u)
          GET_VEC_DXYZ(u,dx,index_u);
          glColor4fv(color_u);
          glVertex3f(x1+dx,constval,zhalf);
        }
        if(i!=sd->is2){
          int index_w;
          float *color_w;
          float dz;
           
          index_w = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k - sd->ks1;
          GET_SLICE_COLOR(color_w,index_w)
          GET_VEC_DXYZ(w,dz,index_w);
          glColor4fv(color_w);
          glVertex3f(xhalf,constval,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:points dir=2");
    
    if(cell_center_text==1){
      for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
        float x1, xhalf;
        int k;

     // n = (i-sd->is1)*sd->nslicej*sd->nslicek - 1;
     // n += (ploty-sd->js1)*sd->nslicek;

        x1 = xplttemp[i];
        if(i!=sd->is2)xhalf=(xplttemp[i]+xplttemp[i+1])/2.0;

        for(k=sd->ks1; k<sd->ks2+1; k++){
          float zhalf, z1;

          z1 = zplttemp[k];
          if(k!=sd->ks2)zhalf = (zplttemp[k]+zplttemp[k+1])/2.0;

          if(k!=sd->ks2){
            int index_u;
            float val;
          
            index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k + 1 - sd->ks1;
            GET_VAL(u,val,index_u);
            output3Val(x1,constval,zhalf,val);
          }
          if(i!=sd->is2){
            int index_w;
            float val;
           
            index_w = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (ploty-sd->js1)*sd->nslicek + k - sd->ks1;
            GET_VAL(w,val,index_w);
            output3Val(xhalf,constval,z1,val);
          }
        }
      }
    }
  }
  if((vd->volslice==1&&plotz>=0&&visz_all==1)||(vd->volslice==0&&sd->idir==3)){
    constval = zplttemp[plotz]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float xhalf;
      float x1;
      int j;
   
//      n = (i-sd->is1)*sd->nslicej*sd->nslicek - sd->nslicek;
//      n += (plotz-sd->ks1);

      x1 = xplttemp[i];
      if(i!=sd->is2)xhalf = (xplttemp[i]+xplttemp[i+1])/2.0;
      for(j=sd->js1; j<sd->js2+1; j++){
        float yhalf;
        float yy1;

        yy1 = yplttemp[j];
        if(j!=sd->js2)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;

        if(j!=sd->js2){
          int index_u;
          float *color_u;
          float dx;
           
          index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j+1-sd->js1)*sd->nslicek;
          GET_SLICE_COLOR(color_u,index_u)
          GET_VEC_DXYZ(u,dx,index_u);
          glColor4fv(color_u);
          glVertex3f(x1-dx,yhalf,constval);
          glVertex3f(x1+dx,yhalf,constval);
        }
        if(i!=sd->is2){
          int index_v;
          float *color_v;
          float dy;
           
          index_v = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j-sd->js1)*sd->nslicek;
          GET_SLICE_COLOR(color_v,index_v)
          GET_VEC_DXYZ(v,dy,index_v);
          glColor4fv(color_v);
          glVertex3f(xhalf,yy1-dy,constval);
          glVertex3f(xhalf,yy1+dy,constval);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:lines dir=3");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
      float xhalf;
      float x1;
      int j;
   
//      n = (i-sd->is1)*sd->nslicej*sd->nslicek - sd->nslicek;
//      n += (plotz-sd->ks1);

      x1 = xplttemp[i];
      if(i!=sd->is2)xhalf = (xplttemp[i]+xplttemp[i+1])/2.0;
      for(j=sd->js1; j<sd->js2+1; j++){
        float yhalf;
        float yy1;

        yy1 = yplttemp[j];
        if(j!=sd->js2)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;

        if(j!=sd->js2){
          int index_u;
          float *color_u;
          float dx;
           
          index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j+1-sd->js1)*sd->nslicek;
          GET_SLICE_COLOR(color_u,index_u)
          GET_VEC_DXYZ(u,dx,index_u);
          glColor4fv(color_u);
          glVertex3f(x1+dx,yhalf,constval);
        }
        if(i!=sd->is2){
          int index_v;
          float *color_v;
          float dy;
           
          index_v = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j-sd->js1)*sd->nslicek;
          GET_SLICE_COLOR(color_v,index_v)
          GET_VEC_DXYZ(v,dy,index_v);
          glColor4fv(color_v);
          glVertex3f(xhalf,yy1+dy,constval);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_cellcenter:points dir=3");
    
    if(cell_center_text==1){
      for(i=sd->is1; i<sd->is1+sd->nslicei; i++){
        float xhalf;
        float x1;
        int j;
   
//      n = (i-sd->is1)*sd->nslicej*sd->nslicek - sd->nslicek;
//      n += (plotz-sd->ks1);

        x1 = xplttemp[i];
        if(i!=sd->is2)xhalf = (xplttemp[i]+xplttemp[i+1])/2.0;
        for(j=sd->js1; j<sd->js2+1; j++){
          float yhalf;
          float yy1;

          yy1 = yplttemp[j];
          if(j!=sd->js2)yhalf = (yplttemp[j]+yplttemp[j+1])/2.0;

          if(j!=sd->js2){
            int index_u;
            float val;
           
            index_u = (i-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j+1-sd->js1)*sd->nslicek;
            GET_VAL(u,val,index_u);
            output3Val(x1,yhalf,constval,val);
          }
          if(i!=sd->is2){
            int index_v;
            float val;
           
            index_v = (i+1-sd->is1)*sd->nslicej*sd->nslicek + (plotz-sd->ks1)+(j-sd->js1)*sd->nslicek;
            GET_VAL(v,val,index_v);
            output3Val(xhalf,yy1,constval,val);
          }
        }
      }
    }
  }
}

/* ------------------ drawvvolslice_terrain ------------------------ */

void drawvvolslice_terrain(const vslicedata *vd){
  int i,j,k,n;
  int i11;
  float constval,x1,yy1,z1;
  int maxj;
  slicedata *u, *v, *w,*sd;
  float dx, dy, dz;
  float vrange;
  mesh *meshi;
  float *xplttemp,*yplttemp,*zplttemp;
  char *iblank;
  int nx, ny, nxy;
  float *rgb_ptr;
  terraindata *terri;
  float *znode;
  int nycell;
  int plotx, ploty, plotz;

  sd = sliceinfo + vd->ival;
  meshi=meshinfo+sd->blocknumber;
  xplttemp=meshi->xplt;
  yplttemp=meshi->yplt;
  zplttemp=meshi->zplt;
  if(vd->volslice==1){
    plotx = meshi->iplotx_all[iplotx_all];
    ploty = meshi->iploty_all[iploty_all];
    plotz = meshi->iplotz_all[iplotz_all];
  }
  else{
    plotx = sd->is1;
    ploty = sd->js1;
    plotz = sd->ks1;
  }
  iblank = meshi->c_iblank_node;
  nx = meshi->ibar+1;
  ny = meshi->jbar+1;
  nxy = nx*ny;

  terri = meshi->terrain;
  if(terri==NULL)return;
  znode = terri->znode_scaled;
  nycell = terri->ny;

  vrange = velocity_range;
  if(vrange<=0.0)vrange=1.0;
  u = vd->u;
  v = vd->v;
  w = vd->w;
  if((vd->volslice==1&&plotx>=0&&visx_all==1)||(vd->volslice==0&&sd->idir==1)){
    constval = xplttemp[plotx]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    maxj = sd->js2;
    if(sd->js1+1>maxj)maxj=sd->js1+1;
    for(j=sd->js1; j<maxj+1; j+=vectorskip){
      n = (j-sd->js1)*sd->nslicek - vectorskip;
      n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
      yy1 = yplttemp[j];
      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip;
        i11 = sd->iqsliceframe[n];
        rgb_ptr = rgb_slice + 4*i11;
        if(iblank[IJK(plotx,j,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(constval-dx,yy1-dy,z1-dz);
          glVertex3f(constval+dx,yy1+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:lines dir=1");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    maxj = sd->js2;
    if(sd->js1+1>maxj)maxj=sd->js1+1;
    for(j=sd->js1; j<maxj+1; j+=vectorskip){
      n = (j-sd->js1)*sd->nslicek - vectorskip;
      n += (plotx-sd->is1)*sd->nslicej*sd->nslicek;
      yy1 = yplttemp[j];
      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip;
        i11 = sd->iqsliceframe[n];
        rgb_ptr = rgb_slice + 4*i11;
        if(iblank[IJK(plotx,j,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(constval+dx,yy1+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:points dir=1");
  }
  if((vd->volslice==1&&ploty>=0&&visy_all==1)||(vd->volslice==0&&sd->idir==2)){
    constval = yplttemp[ploty]+offset_slice*sd->sliceoffset;
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip;
      n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];

      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip; 
        i11 = sd->iqsliceframe[n];
        rgb_ptr = rgb_slice + 4*i11;
        if(iblank[IJK(i,ploty,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1-dx,constval-dy,z1-dz);
          glVertex3f(x1+dx,constval+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:lines dir=2");
    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip;
      n += (ploty-sd->js1)*sd->nslicek;

      x1 = xplttemp[i];

      for(k=sd->ks1; k<sd->ks2+1; k+=vectorskip){
        n+=vectorskip; 
        i11 = sd->iqsliceframe[n];
        rgb_ptr = rgb_slice + 4*i11;
        if(iblank[IJK(i,ploty,k)]==1&&rgb_ptr[3]>0.5){
          z1 = zplttemp[k];
          GET_VEC_DXYZ(u,dx,n);
          GET_VEC_DXYZ(v,dy,n);
          GET_VEC_DXYZ(w,dz,n);
          glColor4fv(rgb_ptr);
          glVertex3f(x1+dx,constval+dy,z1+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:points dir=2");
  }
  if((vd->volslice==1&&plotz>=0&&visz_all==1)||(vd->volslice==0&&sd->idir==3)){
    float zmax;

    zmax = zplttemp[meshi->kbar];
    constval = zplttemp[plotz]+offset_slice*sd->sliceoffset-znode[0];
    glLineWidth(vectorlinewidth);
    glBegin(GL_LINES);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      x1 = xplttemp[i];
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip*sd->nslicek;
      n += (plotz-sd->ks1);
      for(j=sd->js1; j<sd->js2+1; j+=vectorskip){
        int n11;
        float z11;
        int ij2;

        n+=vectorskip*sd->nslicek; 
        ij2 = ijnode2(i,j);
        z11 = constval + znode[ij2];
        if(z11>zmax)z11=zmax;
        n11=i*sd->nslicej*sd->nslicek+j*sd->nslicek;
        rgb_ptr = rgb_slice + 4*interp3dsliceindex(sd->iqsliceframe,meshi->zplt,meshi->kbar,n11,z11);
        if(rgb_ptr[3]>0.5){
          float f1, f2;
          int k1, k2;
          int n1, n2;

          get_z_interp_factors(meshi->zplt,meshi->kbar,z11,&k1,&k2,&f1,&f2);
          n1 = n11 + k1;
          n2 = n11 + k2;
          yy1 = yplttemp[j];
          GET_VEC_DXYZ_TERRAIN(u,dx);
          GET_VEC_DXYZ_TERRAIN(v,dy);
          GET_VEC_DXYZ_TERRAIN(w,dz);

          glColor4fv(rgb_ptr);
          glVertex3f(x1-dx,yy1-dy,z11-dz);
          glVertex3f(x1+dx,yy1+dy,z11+dz);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:lines dir=3");

    glPointSize(vectorpointsize);
    glBegin(GL_POINTS);
    for(i=sd->is1; i<sd->is1+sd->nslicei; i+=vectorskip){
      n = (i-sd->is1)*sd->nslicej*sd->nslicek - vectorskip*sd->nslicek;
      n += (plotz-sd->ks1);

      x1 = xplttemp[i];
      for(j=sd->js1; j<sd->js2+1; j+=vectorskip){
        int n11;
        float z11;
        int ij2;

        n+=vectorskip*sd->nslicek; 

        ij2 = ijnode2(i,j);
        z11 = constval + znode[ij2];
        if(z11>zmax)z11=zmax;
        n11=i*sd->nslicej*sd->nslicek+j*sd->nslicek;
        rgb_ptr = rgb_slice + 4*interp3dsliceindex(sd->iqsliceframe,meshi->zplt,meshi->kbar,n11,z11);
        if(rgb_ptr[3]>0.5){
          float f1, f2;
          int k1, k2;
          int n1, n2;

          get_z_interp_factors(meshi->zplt,meshi->kbar,z11,&k1,&k2,&f1,&f2);
          n1 = n11 + k1;
          n2 = n11 + k2;
          yy1 = yplttemp[j];
          GET_VEC_DXYZ_TERRAIN(u,dx);
          GET_VEC_DXYZ_TERRAIN(v,dy);
          GET_VEC_DXYZ_TERRAIN(w,dz);
          glColor4fv(rgb_ptr);
          glVertex3f(x1+dx,yy1+dy,z11);
        }
      }
    }
    glEnd();
    SNIFF_ERRORS("after drawvvolslice_terrain:points dir=3");
  }
}

/* ------------------ output_slicedata ------------------------ */

void output_Slicedata(void){
  FILE *fileout;
  char datafile[1024];
  int i, ii, n;
  float *data;
  slicedata *sd;
  int row,col;
  char *ext;
  char flabel[256];

  init_Slicedata();

  for(ii=0;ii<nslice_loaded;ii++){
    i=slice_loaded_list[ii];
    sd = sliceinfo + i;
    if(sd->display==0||sd->type!=islicetype)continue;
    if(sd->times[0]>global_times[itimes])continue;

    if(sd->qslicedata==NULL){
      printf("  Slice data unavailble for output\n");
      continue;
    }
    data = sd->qslicedata + sd->itime*sd->nsliceii;
    strcpy(datafile,sd->file);
    ext = strstr(datafile,".");
    if(ext!=NULL){
      ext[0]=0;
    }
    sprintf(flabel,"%i",itimes);
    trim(flabel);
    strcat(datafile,"_sf_");
    strcat(datafile,flabel);
    strcat(datafile,".csv");
    fileout = fopen(datafile,"a");
    if(fileout==NULL)continue;
    if(global_times!=NULL)fprintf(fileout,"%f\n",global_times[itimes]);
    switch (sd->idir){
      case 1:
        fprintf(fileout,"%i,%i\n",sd->ks2+1-sd->ks1,sd->js2+1-sd->js1);
        for(row=sd->ks1; row<=sd->ks2; row++){
          for(col=sd->js1; col<=sd->js2; col++){
            n = (col-sd->js1)*sd->nslicek + row-sd->ks1;
            if(col!=sd->js2)fprintf(fileout,"%f, ",data[n]);
            if(col==sd->js2)fprintf(fileout,"%f ",data[n]);
          }
          fprintf(fileout,"\n");
        }
       break;
      case 2:
        fprintf(fileout,"%i, %i \n",sd->ks2+1-sd->ks1,sd->is2+1-sd->is1);
        for(row=sd->ks1; row<=sd->ks2; row++){
          for(col=sd->is1; col<=sd->is2; col++){
            n = (col-sd->is1)*sd->nslicek + row-sd->ks1;
            if(col!=sd->is2)fprintf(fileout,"%f, ",data[n]);
            if(col==sd->is2)fprintf(fileout,"%f ",data[n]);
          }
          fprintf(fileout,"\n");
        }
       break;
      case 3:
        fprintf(fileout,"%i, %i \n",sd->js2+1-sd->js1,sd->is2+1-sd->is1);
        for(row=sd->js1; row<=sd->js2; row++){
          for(col=sd->is1; col<=sd->is2; col++){
            n = (col-sd->is1)*sd->nslicej + row-sd->js1;
            if(col!=sd->is2)fprintf(fileout,"%f, ",data[n]);
            if(col==sd->is2)fprintf(fileout,"%f ",data[n]);
          }
          fprintf(fileout,"\n");
        }
       break;
      default:
        ASSERT(FFALSE);
        break;
    }
    fclose(fileout);
    fileout=NULL;

  }
}

/* ------------------ init_slicedata ------------------------ */

void init_Slicedata(void){
  FILE *fileout;
  char datafile[1024];
  int i, j, k, ii;
  float *xplt, *yplt, *zplt;
  slicedata *sd;
  mesh *meshi;
  char *ext;
  char flabel[256];

  for(ii=0;ii<nslice_loaded;ii++){
    i=slice_loaded_list[ii];
    sd = sliceinfo + i;
    if(sd->display==0||sd->type!=islicetype)continue;
    if(sd->times[0]>global_times[itimes])continue;

    strcpy(datafile,sd->file);
    ext = strstr(datafile,".");
    if(ext!=NULL){
      ext[0]=0;
    }
    sprintf(flabel,"%i",itimes);
    trim(flabel);
    strcat(datafile,"_sf_");
    strcat(datafile,flabel);
    strcat(datafile,".csv");
    fileout = fopen(datafile,"w");
    if(fileout==NULL)continue;
    fprintf(fileout,"%s\n",sd->label.longlabel);
    fprintf(fileout,"%s\n",sd->label.unit);
    meshi = meshinfo + sd->blocknumber;

    xplt = meshi->xplt_orig;
    yplt = meshi->yplt_orig;
    zplt = meshi->zplt_orig;
    fprintf(fileout,"%f, %f, %f, %f, %f, %f\n",
      xplt[sd->is1],xplt[sd->is2],
      yplt[sd->js1],yplt[sd->js2],
      zplt[sd->ks1],zplt[sd->ks2]);


    switch (sd->idir){
    case 1:
      fprintf(fileout,"%i\n",sd->ks2+1-sd->ks1);
      for(k=sd->ks1;k<=sd->ks2;k++){
        if(k!=sd->ks2)fprintf(fileout,"%f, ",zplt[k]);
        if(k==sd->ks2)fprintf(fileout,"%f ",zplt[k]);
      }
      fprintf(fileout,"\n");
      fprintf(fileout,"%i\n",sd->js2+1-sd->js1);
      for(j=sd->js1;j<=sd->js2;j++){
        if(j!=sd->js2)fprintf(fileout,"%f, ",yplt[j]);
        if(j==sd->js2)fprintf(fileout,"%f ",yplt[j]);
      }
      fprintf(fileout,"\n");
      break;
    case 2:
      fprintf(fileout,"%i\n",sd->ks2+1-sd->ks1);
      for(k=sd->ks1;k<=sd->ks2;k++){
        if(k!=sd->ks2)fprintf(fileout,"%f, ",zplt[k]);
        if(k==sd->ks2)fprintf(fileout,"%f ",zplt[k]);
      }
      fprintf(fileout,"\n");
      fprintf(fileout,"%i\n",sd->is2+1-sd->is1);
      for(i=sd->is1;i<=sd->is2;i++){
        if(i!=sd->is2)fprintf(fileout,"%f, ",xplt[i]);
        if(i==sd->is2)fprintf(fileout,"%f ",xplt[i]);
      }
      fprintf(fileout,"\n");
      break;
    case 3:
      fprintf(fileout,"%i\n",sd->js2+1-sd->js1);
      for(j=sd->js1;j<=sd->js2;j++){
        if(j!=sd->js2)fprintf(fileout,"%f, ",yplt[j]);
        if(j==sd->js2)fprintf(fileout,"%f ",yplt[j]);
      }
      fprintf(fileout,"\n");
      fprintf(fileout,"%i\n",sd->is2+1-sd->is1);
      for(i=sd->is1;i<=sd->is2;i++){
        if(i!=sd->is2)fprintf(fileout,"%f, ",xplt[i]);
        if(i==sd->is2)fprintf(fileout,"%f ",xplt[i]);
      }
      fprintf(fileout,"\n");
      break;
    default:
      ASSERT(FFALSE);
      break;
    }
    fclose(fileout);
    fileout=NULL;

  }
}

/* ------------------ average_slice_data ------------------------ */

int average_slice_data(float *data_out, float *data_in, int ndata, int data_per_timestep, float *times_local, int ntimes_local, float average_time){

#define IND(itime,ival) ((itime)*data_per_timestep + (ival))
  float *datatemp=NULL;
  int below, above, naverage;
  float average_timed2;
  int i, j, k;

  if(data_in==NULL||data_out==NULL)return 1;
  if(ndata<data_per_timestep||data_per_timestep<1||ntimes_local<1||average_time<0.0)return 1;
  if(ndata!=data_per_timestep*ntimes_local)return 1;

  average_timed2 = average_time/2.0;

  NewMemory((void **)&datatemp,ndata*sizeof(float));
  for(i=0;i<ndata;i++){
    datatemp[i]=0.0;
  }
  for(i=0;i<ntimes_local;i++){
    printf("averaging time=%.2f\n",times_local[i]);
    below=0;
    for(j=i-1;j>=0;j--){
      if(times_local[i]-times_local[j]>average_timed2){
        below=j+1;
        break;
      }
    }
    above=ntimes_local-1;
    for(j=i+1;j<ntimes_local;j++){
      if(times_local[j]-times_local[i]>average_timed2){
        above=j-1;
        break;
      }
    }
    naverage = above + 1 - below;
    for(k=0;k<data_per_timestep;k++){
      for(j=below;j<=above;j++){
        datatemp[IND(i,k)]+=data_in[IND(j,k)];
      }
    }
    for(k=0;k<data_per_timestep;k++){
      datatemp[IND(i,k)]/=(float)naverage;
    }
  }
  for(i=0;i<ndata;i++){
    data_out[i]=datatemp[i];
  }
  FREEMEMORY(datatemp);
  return 0;
}


/* ------------------ getsliceheader ------------------------ */

int getsliceheader0(char *comp_file, char *size_file, int compression_type, int *i1, int *i2, int *j1, int *j2, int *k1, int *k2, int *slice3d){
  FILE *stream;
  char buffer[255];

  stream=FOPEN(size_file,"r");
  if(stream==NULL){
    if(makeslicesizefile(comp_file,size_file, compression_type)==0)return 0;
    stream=FOPEN(size_file,"r");
    if(stream==NULL)return 0;
  }

  if(fgets(buffer,255,stream)==NULL){
    fclose(stream);
    return 0;
  }
  sscanf(buffer,"%i %i %i %i %i %i",i1,i2,j1,j2,k1,k2);
  if(*i1==*i2||*j1==*j2||*k1==*k2){
    *slice3d=0;
  }
  else{
    *slice3d=1;
  }
  fclose(stream);
  return 1;
}
/* ------------------ getsliceheader ------------------------ */

int getsliceheader(char *comp_file, char *size_file, int compression_type,
                   int framestep, int set_tmin, int set_tmax, float tmin_local, float tmax_local,
                   int *nx, int *ny, int *nz, int *nsteps, int *ntotal, float *valmin, float *valmax){
  FILE *stream;
  int i1, i2, j1, j2, k1, k2;
  float time_local;
  int ncompressed;
  int count;
  char buffer[256];
  int ncompressed_rle, ncompressed_zlib;

  stream=FOPEN(size_file,"r");
  if(stream==NULL){
    if(makeslicesizefile(comp_file,size_file,compression_type)==0)return 0;
    stream=fopen(size_file,"r");
    if(stream==NULL)return 0;
  }

  if(fgets(buffer,255,stream)==NULL){
    fclose(stream);
    return 0;
  }
  sscanf(buffer,"%i %i %i %i %i %i",&i1,&i2,&j1,&j2,&k1,&k2);
  *nx = i2 + 1 - i1;
  *ny = j2 + 1 - j1;
  *nz = k2 + 1 - k1;
  if(fgets(buffer,255,stream)==NULL){
    fclose(stream);
    return 0;
  }
  sscanf(buffer,"%f %f",valmin,valmax);

  count=0;
  *nsteps=0;
  *ntotal=0;
  while(!feof(stream)){

    if(fgets(buffer,255,stream)==NULL)break;
    sscanf(buffer,"%f %i %i",&time_local,&ncompressed_zlib, &ncompressed_rle);
    if(compression_type==1){
      ncompressed=ncompressed_zlib;
    }
    else{
      ncompressed=ncompressed_rle;
    }
    if(count++%framestep!=0)continue;
    if(set_tmin==1&&time_local<tmin_local)continue;
    if(set_tmax==1&&time_local>tmax_local)continue;
    (*nsteps)++;
    *ntotal+=ncompressed;
  }
  fclose(stream);
  return 2 + *nsteps;
}

  //*** header
  // endian
  // completion (0/1)
  // fileversion (compressed format)
  // version  (slicef version)
  // global min max (used to perform conversion)
  // i1,i2,j1,j2,k1,k2


  //*** frame
  // time, compressed frame size                        for each frame
  // compressed buffer


/* ------------------ getslicecompresseddata ------------------------ */

int getslicecompresseddata(char *file, 
                            int set_tmin, int set_tmax, float tmin_local, float tmax_local, int ncompressed, int sliceskip, int nsliceframes,
                            float *times_local, unsigned char *compressed_data, compinfo *compindex, float *valmin, float *valmax){
  int returnval;

  returnval=getslicezlibdata(file,set_tmin,set_tmax,tmin_local,tmax_local,ncompressed,sliceskip,nsliceframes,
                            times_local,compressed_data,compindex,valmin,valmax);
  return returnval;
}

/* ------------------ getsliceczlibdata ------------------------ */

int getslicezlibdata(char *file, 
                            int set_tmin, int set_tmax, float tmin_local, float tmax_local, int ncompressed, int sliceskip, int nsliceframes,
                            float *times_local, unsigned char *compressed_data, compinfo *compindex, float *valmin, float *valmax){
  FILE *stream;
  int count, ns;
  unsigned char *cd;
  int endian;
  float minmax[2];
  int fileversion, version; 
  int completion;
  int ijkbar[6];

  cd=compressed_data;
  compindex[0].offset=0;

  stream=FOPEN(file,"rb");
  if(stream==NULL)return 0;
  
  // read header

  fread(&endian,4,1,stream);
  fread(&completion,4,1,stream);
  if(completion==0){
    fclose(stream);
    return 0;
  }

  fread(&fileversion,4,1,stream);
  if(endian!=1)fileversion=int_switch(fileversion);

  fread(&version,4,1,stream);
  if(endian!=1)version=int_switch(version);

  fread(minmax,4,2,stream);
  if(endian!=1){
    minmax[0]=float_switch(minmax[0]);
    minmax[1]=float_switch(minmax[1]);
  }

  fread(ijkbar,4,6,stream);
  if(endian!=1){
    ijkbar[0]=int_switch(ijkbar[0]);
    ijkbar[1]=int_switch(ijkbar[1]);
    ijkbar[2]=int_switch(ijkbar[2]);
    ijkbar[3]=int_switch(ijkbar[3]);
    ijkbar[4]=int_switch(ijkbar[4]);
    ijkbar[5]=int_switch(ijkbar[5]);
  }

  count=0;
  ns=0;
  while(!feof(stream)){
    float ttime;
    int nncomp;

    fread(&ttime,4,1,stream);
    fread(&nncomp,4,1,stream);
    if(count++%sliceskip!=0||set_tmin==1&&ttime<tmin_local||set_tmax==1&&ttime>tmax_local){
      FSEEK(stream,nncomp,SEEK_CUR);
      continue;
    }
    times_local[ns++]=ttime;
    compindex[ns].offset=compindex[ns-1].offset+nncomp;
    compindex[ns-1].size=nncomp;

    printf("slice time=%.2f\n",ttime);
    fread(cd,1,nncomp,stream);
    cd+=nncomp;
    if(ns>=nsliceframes||cd-compressed_data>=ncompressed)break;
  }
  fclose(stream);
  return cd-compressed_data;
}

  //*** header
  // endian
  // completion (0/1)
  // fileversion (compressed format)
  // version  (slicef version)
  // global min max (used to perform conversion)
  // i1,i2,j1,j2,k1,k2


  //*** frame
  // time, compressed frame size                        for each frame
  // compressed buffer

/* ------------------ getslicecompressdata ------------------------ */
int makeslicesizefile(char *file, char *sizefile, int compression_type){
  int endian_fromfile;
  float minmax[2];
  int ijkbar[6];
  FILE *stream, *sizestream, *RLESLICEFILE;
  float time_local;
  int ncompressed;
  int count;
  size_t returncode;

  stream=FOPEN(file,"rb");
  if(stream==NULL)return 0;
  RLESLICEFILE=stream;

  sizestream=fopen(sizefile,"w");
  if(sizestream==NULL){
    fclose(stream);
    return 0;
  }
  count=0;
  if(compression_type==1){
    fread(&endian_fromfile,4,1,stream);
    FSEEK(stream,12,SEEK_CUR);
    fread(minmax,4,2,stream);
    fread(ijkbar,4,6,stream);

    fprintf(sizestream,"%i %i %i %i %i %i\n",ijkbar[0],ijkbar[1],ijkbar[2],ijkbar[3],ijkbar[4],ijkbar[5]);
    fprintf(sizestream,"%f %f\n",minmax[0],minmax[1]);
    count=2;

    while(!feof(stream)){
      fread(&time_local,4,1,stream);
      fread(&ncompressed,4,1,stream);
      fprintf(sizestream,"%f %i\n",time_local,ncompressed);
      count++;
      FSEEK(stream,ncompressed,SEEK_CUR);
    }
  }
  //  endian
  //  fileversion, slice version 
  //  global min max (used to perform conversion)
  //  i1,i2,j1,j2,k1,k2


  //  *** frame
  // time
  //  compressed frame size                        for each frame
  // compressed buffer

  if(compression_type==2){
    int one;

    FSEEK(stream,4,SEEK_CUR);fread(&one,4,1,stream);FSEEK(stream,4,SEEK_CUR);
    
    endianswitch=0;
    if(one!=1)endianswitch=1;

    FSEEK(stream,4*4,SEEK_CUR);
    FORTRLESLICEREAD(minmax,2);
    FORTRLESLICEREAD(ijkbar,6);
    fprintf(sizestream,"%i %i %i %i %i %i\n",ijkbar[0],ijkbar[1],ijkbar[2],ijkbar[3],ijkbar[4],ijkbar[5]);
    fprintf(sizestream,"%f %f\n",minmax[0],minmax[1]);
    count=2;
    while(!feof(stream)){
      FORTRLESLICEREAD(&time_local,1);
      if(returncode==0)break;
      FORTRLESLICEREAD(&ncompressed,1);
      if(returncode==0)break;
      returncode=FSEEK(stream,8+ncompressed,SEEK_CUR);
      if(returncode!=0)break;
      fprintf(sizestream,"%f %i %i\n",time_local,0,ncompressed);
      count++;
    }
  }
  fclose(stream);
  fclose(sizestream);
  return count;

}

#ifdef USE_ZLIB
/* ------------------ uncompress_slicedataframe ------------------------ */

void uncompress_slicedataframe(slicedata *sd,int iframe_local){
  unsigned int countin;
  uLongf countout;
  unsigned char *compressed_data;

  compressed_data = sd->qslicedata_compressed + sd->compindex[iframe_local].offset;
  countin = sd->compindex[iframe_local].size;
  countout=sd->nsliceii;

  if(sd->compression_type==1){
    uncompress(sd->slicecomplevel,&countout,compressed_data,countin);
  }
  else if(sd->compression_type==2){
    irle(compressed_data, countin, sd->slicecomplevel);
  }

}
#endif

/* ------------------ getsliceval ------------------------ */

float getsliceval(slicedata *sd, unsigned char ival){
  float returnval;

  returnval = (sd->valmax*ival + sd->valmin*(255-ival))/255.0;
  return returnval;
}

/* ------------------ push_slice_loadstack ------------------------ */

void push_slice_loadstack(int sliceindex){
  int i;

  if(islice_loadstack<nslice_loadstack){
    for(i=0;i<islice_loadstack;i++){
      if(slice_loadstack[i]==sliceindex)return;
    }
    slice_loadstack[islice_loadstack++]=sliceindex;
  }
}

/* ------------------ remove_slice_loadstack ------------------------ */

void remove_slice_loadstack(int sliceindex){
  int i;

  for(i=islice_loadstack-1;i>=0;i--){
    if(slice_loadstack[i]==sliceindex){
      int j;

      for(j=i;j<islice_loadstack-1;j++){
        slice_loadstack[j]=slice_loadstack[j+1];
      }
      islice_loadstack--;
      break;
    }
  }
}

/* ------------------ last_slice_loadstack ------------------------ */

int last_slice_loadstack(void){
  int return_val;

  if(islice_loadstack-1>=0&&islice_loadstack-1<nslice_loadstack){
    return_val=slice_loadstack[islice_loadstack-1];
  }
  else{
    return_val=-1;
  }
  return return_val;
}

/* ------------------ push_vslice_loadstack ------------------------ */

void push_vslice_loadstack(int vsliceindex){
  int i;

  if(ivslice_loadstack<nvslice_loadstack){
    for(i=0;i<ivslice_loadstack;i++){
      if(vslice_loadstack[i]==vsliceindex)return;
    }
    vslice_loadstack[ivslice_loadstack++]=vsliceindex;
  }
}

/* ------------------ remove_vslice_loadstack ------------------------ */

void remove_vslice_loadstack(int vsliceindex){
  int i;

  for(i=ivslice_loadstack-1;i>=0;i--){
    if(vslice_loadstack[i]==vsliceindex){
      int j;

      for(j=i;j<ivslice_loadstack-1;j++){
        vslice_loadstack[j]=vslice_loadstack[j+1];
      }
      ivslice_loadstack--;
      break;
    }
  }
}

/* ------------------ last_slice_loadstack ------------------------ */

int last_vslice_loadstack(void){
  int return_val;

  if(ivslice_loadstack-1>=0&&ivslice_loadstack-1<nvslice_loadstack){
    return_val=vslice_loadstack[ivslice_loadstack-1];
  }
  else{
    return_val=-1;
  }
  return return_val;
}

/* ------------------ update_slicedir_count ------------------------ */

void update_slicedir_count(void){
  int i,j;

  for(i=0;i<nsliceinfo;i++){
	slicedata *slicei;

    slicei = sliceinfo + i;
	slicei->ndirxyz[0]=0;
	slicei->ndirxyz[1]=0;
	slicei->ndirxyz[2]=0;
	slicei->ndirxyz[3]=0;
  }
  for(i=0;i<nsliceinfo;i++){
    slicedata *slicei, *slicej;

    slicei = sliceinfo + i;
    if(slicei->idir<1)continue;
	  if(slicei->volslice==1)continue;
	  for(j=0;j<nsliceinfo;j++){
	    slicej = sliceinfo + j;
	    if(slicej->idir<1)continue;
	    if(slicej->volslice==1)continue;
      if(strcmp(slicej->label.longlabel,slicei->label.longlabel)!=0)continue;
	    //if(slicej->cellcenter!=slicei->cellcenter)continue;
      if(slicej->slicetype==SLICE_CENTER&&slicei->slicetype!=SLICE_CENTER||slicej->slicetype!=SLICE_CENTER&&slicei->slicetype==SLICE_CENTER)continue;
      slicei->ndirxyz[slicej->idir]++;
  	}
  }
}
#define VERT_AVG(v1,v2,vavg) \
  vavg[0]=(v1[0]+v2[0])/2.0;\
  vavg[1]=(v1[1]+v2[1])/2.0;\
  vavg[2]=(v1[2]+v2[2])/2.0

#define DIST3(v1,v2,dist2) \
  dx=v1[0]-v2[0];\
  dy=v1[1]-v2[1];\
  dz=v1[2]-v2[2];\
  dist2=dx*dx+dy*dy+dz*dz
  
/* ------------------ get_texture_index ------------------------ */

float get_texture_index(float *xyz){
  int i, j, k;
  float *vv;
  float *xplt, *yplt, *zplt;
  float dxbar, dybar, dzbar;
  int ibar, jbar, kbar;
  int nx, ny, nz;
  float dx, dy, dz;
  float val000,val100,val010,val110;
  float val001,val101,val011,val111;
  float val00,val10,val01,val11;
  float val0, val1;
  float val, val_fraction;
  int ijk;
   
  float *slicedata0;
  float valmin, valmax;
  mesh *valmesh;

  slicedata0 = gslicedata;
  valmin = gslice_valmin;
  valmax = gslice_valmax;
  valmesh = gslice_valmesh;

  xplt = valmesh->xplt_orig;
  yplt = valmesh->yplt_orig;
  zplt = valmesh->zplt_orig;
  ibar = valmesh->ibar;
  jbar = valmesh->jbar;
  kbar = valmesh->kbar;
  dxbar = xplt[1]-xplt[0];
  dybar = yplt[1]-yplt[0];
  dzbar = zplt[1]-zplt[0];

  nx = ibar + 1;
  ny = jbar + 1;
  nz = kbar + 1;

  GETINDEX(i,xyz[0],xplt[0],dxbar,nx);
  GETINDEX(j,xyz[1],yplt[0],dybar,ny);
  GETINDEX(k,xyz[2],zplt[0],dzbar,nz);

     // val(i,j,k) = di*nj*nk + dj*nk + dk
  ijk = i*nz*ny + j*nz + k;

  dx = (xyz[0] - xplt[i])/dxbar;
  dx = CLAMP(dx,0.0,1.0);
  dy = (xyz[1] - yplt[j])/dybar;
  dy = CLAMP(dy,0.0,1.0);
  dz = (xyz[2] - zplt[k])/dzbar;
  dz = CLAMP(dz,0.0,1.0);

  vv = slicedata0 + ijk;
  val000 = (float)vv[0]; // i,j,k
  val001 = (float)vv[1]; // i,j,k+1

  vv += nz;
  val010 = (float)vv[0]; // i,j+1,k
  val011 = (float)vv[1]; // i,j+1,k+1

  vv += (nz*ny-nz);
  val100 = (float)vv[0]; // i+1,j,k
  val101 = (float)vv[1]; // i+1,j,k+1

  vv += nz;
  val110 = (float)vv[0]; // i+1,j+1,k
  val111 = (float)vv[1]; // i+1,j+1,k+1

  val00 = MIX(dx,val100,val000);
  val10 = MIX(dx,val110,val010);
  val01 = MIX(dx,val101,val001);
  val11 = MIX(dx,val111,val011);
   val0 = MIX(dy, val10, val00);
   val1 = MIX(dy, val11, val01);
  
  val = MIX(dz,val1,val0);
  val_fraction = (val-valmin)/(valmax-valmin);
  val_fraction = CLAMP(val_fraction,0.0,1.0);
  return val_fraction;
}
  
/* ------------------ draw_quad ------------------------ */

void draw_quad(float *v1, float *v2, float *v3, float *v4,
               float t1, float t2, float t3, float t4,
               float del, int level){
  float d13,d24;
  float dx, dy, dz;

  if(level==0){
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glEnable(GL_TEXTURE_1D);
    glBindTexture(GL_TEXTURE_1D,texture_slice_colorbar_id);
    glBegin(GL_TRIANGLES);
  }
  DIST3(v1,v3,d13);
  DIST3(v2,v4,d24);
  if(d13<d24){
    draw_triangle(v1,v2,v3,t1,t2,t3,del,level+1);
    draw_triangle(v1,v3,v4,t1,t3,t4,del,level+1);
  }
  else{
    draw_triangle(v1,v2,v4,t1,t2,t4,del,level+1);
    draw_triangle(v2,v3,v4,t2,t3,t4,del,level+1);
  }
  if(level==0){
    glEnd();
    glDisable(GL_TEXTURE_1D);
  }
}
  
/* ------------------ draw_triangle ------------------------ */

void draw_triangle(float *v1, float *v2, float *v3, 
                   float t1, float t2, float t3,
                   float del, int level){
  float d12, d13 ,d23;
  float v12[3],v13[3],v23[3];
  float dx, dy, dz;
  float t12,t13,t23;

  if(level==0){
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glEnable(GL_TEXTURE_1D);
    glBindTexture(GL_TEXTURE_1D,texture_slice_colorbar_id);
    glBegin(GL_TRIANGLES);
  }
  DIST3(v1,v2,d12);
  DIST3(v1,v3,d13);
  DIST3(v2,v3,d23);
  if(d12<=del&&d13<=del&&d23<del){
    glTexCoord1f(t1);
    glVertex3fv(v1);

    glTexCoord1f(t2);
    glVertex3fv(v2);

    glTexCoord1f(t3);
    glVertex3fv(v3);
  }
  else{
    if(d12<=MIN(d13,d23)){
      VERT_AVG(v1,v3,v13);
      t13=get_texture_index(v13);
      VERT_AVG(v2,v3,v23);
      t23=get_texture_index(v23);

      draw_triangle(v3,v13,v23,t3,t13,t23,del,level+1);
      draw_quad(v13,v1,v2,v23,t13,t1,t2,t23,del,level+1);
    }
    else if(d13<=MIN(d12,d23)){
      VERT_AVG(v1,v2,v12);
      t12=get_texture_index(v12);
      VERT_AVG(v2,v3,v23);
      t23=get_texture_index(v23);

      draw_triangle(v12,v2,v23,t12,t2,t23,del,level+1);
      draw_quad(v1,v12,v23,v3,t1,t12,t23,t3,del,level+1);
    }
    else{ // d23<=MIN(d12,d13)
      VERT_AVG(v1,v2,v12);
      t12=get_texture_index(v12);
      VERT_AVG(v1,v3,v13);
      t13=get_texture_index(v13);

      draw_triangle(v1,v12,v13,t1,t12,t13,del,level+1);
      draw_quad(v12,v2,v3,v13,t12,t2,t3,t13,del,level+1);
    }
  }
  if(level==0){
    glEnd();
    glDisable(GL_TEXTURE_1D);
  }
}
  
/* ------------------ draw_quad_outline ------------------------ */

void draw_quad_outline(float *v1, float *v2, float *v3, float *v4,
               float del, int level){
  float d13,d24;
  float dx, dy, dz;

  if(level==0){
    glBegin(GL_LINES);
  }
  DIST3(v1,v3,d13);
  DIST3(v2,v4,d24);
  if(d13<d24){
    draw_triangle_outline(v1,v2,v3,del,level+1);
    draw_triangle_outline(v1,v3,v4,del,level+1);
  }
  else{
    draw_triangle_outline(v1,v2,v4,del,level+1);
    draw_triangle_outline(v2,v3,v4,del,level+1);
  }
  if(level==0){
    glEnd();
  }
}
  
/* ------------------ draw_triangle_outline ------------------------ */

void draw_triangle_outline(float *v1, float *v2, float *v3, 
                   float del, int level){
  float d12, d13 ,d23;
  float v12[3],v13[3],v23[3];
  float dx, dy, dz;

  if(level==0){
    glBegin(GL_LINES);
  }
  DIST3(v1,v2,d12);
  DIST3(v1,v3,d13);
  DIST3(v2,v3,d23);
  if(d12<=del&&d13<=del&&d23<del){
    glVertex3fv(v1);
    glVertex3fv(v2);

    glVertex3fv(v2);
    glVertex3fv(v3);

    glVertex3fv(v3);
    glVertex3fv(v1);
  }
  else{
    if(d12<=MIN(d13,d23)){
      VERT_AVG(v1,v3,v13);
      VERT_AVG(v2,v3,v23);

      draw_triangle_outline(v3,v13,v23,del,level+1);
      draw_quad_outline(v13,v1, v2,v23,del,level+1);
    }
    else if(d13<=MIN(d12,d23)){
      VERT_AVG(v1,v2,v12);
      VERT_AVG(v2,v3,v23);

      draw_triangle_outline(v12,v2,v23,del,level+1);
      draw_quad_outline(v1,v12,v23,v3,del,level+1);
    }
    else{ // d23<=MIN(d12,d13)
      VERT_AVG(v1,v2,v12);
      VERT_AVG(v1,v3,v13);

      draw_triangle_outline(v1,v12,v13,del,level+1);
      draw_quad_outline(v12,v2,v3,v13,del,level+1);
    }
  }
  if(level==0){
    glEnd();
  }
}
