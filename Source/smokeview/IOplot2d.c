#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "datadefs.h"
#include "smokeviewvars.h"

/* ------------------ GetCurrentCsv ------------------------ */

csvdata *GetCsv(int file_index, int col_index, csvfiledata **csvf_ptr){
  csvfiledata *csvfi;
  csvdata *csvi;

  csvfi = csvfileinfo    + file_index;
  csvi  = csvfi->csvinfo + col_index;
  if(csvf_ptr != NULL)*csvf_ptr = csvfi;
  return csvi;
}

/* ------------------ GetCurrentCsv ------------------------ */

csvdata *GetCurrentCsv(int col_index, csvfiledata **csvf_ptr){
  return GetCsv(glui_csv_file_index, col_index, csvf_ptr);
}

/* ------------------ HaveGenDevShow ------------------------ */

int GenDevShow(void){
  int i;

  for(i = 0; i < nplot2dinfo; i++){
    plot2ddata *plot2di;

    plot2di = plot2dinfo + i;
    if(plot2di->show == 1){
      int j;

      for(j = 0; j < plot2di->ncurves; j++){
        if(plot2di->curve[j].csv_col_index<ndeviceinfo)return 1;
      }
    }
  }
  return 0;
}

/* ------------------ HaveGenHrrShow ------------------------ */

int GenHrrShow(void){
  int i;

  for(i = 0; i < nplot2dinfo; i++){
    plot2ddata *plot2di;

    plot2di = plot2dinfo + i;
    if(plot2di->show == 1){
      int j;

      for(j = 0; j < plot2di->ncurves; j++){
        if(plot2di->curve[j].csv_col_index>=ndeviceinfo)return 1;
      }
    }
  }
  return 0;
}

/* ------------------ HaveGenDev ------------------------ */

int HaveGenDev(void){
  int i;

  for(i = 0; i<glui_plot2dinfo->ncurves; i++){
    if(glui_plot2dinfo->curve[i].csv_col_index<ndeviceinfo)return 1;
  }
  return 0;
}

/* ------------------ HaveGenHrr ------------------------ */

int HaveGenHrr(void){
  int i;

  for(i = 0; i<glui_plot2dinfo->ncurves; i++){
    if(glui_plot2dinfo->curve[i].csv_col_index>=ndeviceinfo)return 1;
  }
  return 0;
}

/* ------------------ DrawPlot ------------------------ */
#define AXIS_LEFT  0
#define AXIS_RIGHT 1
#define AXIS_NONE  2
void DrawGenCurve(int option, plot2ddata *plot2di, curvedata *curve, float size_factor,
              float *x, float *z, int n, float x_cur, float z_cur, float zmin, float zmax,
              int axis_side, int position, char *label, char *unit, float pad_length){
  float xmin, xmax, dx, dz;
  float xscale = 1.0, zscale = 1.0;
  int i, ndigits = 3;

  float *xyz0, linewidth_arg;
  int *plot_color, show_title, show_curve_labels, show_curve_values;
  char *title;
  float fplot_color[3];
  float curve_factor;
  int apply_curve_factor;
  int update_average;;
  float *vals;

  SNIFF_ERRORS("after DrawGenCurve 1 - beginning");
  xyz0               = plot2di->xyz;
  plot_color         = curve->color;
  linewidth_arg      = curve->linewidth;
  title              = plot2di->plot_label;
  show_title         = plot2di->show_title;
  show_curve_labels  = plot2di->show_curve_labels;
  show_curve_values  = plot2di->show_curve_values;
  fplot_color[0]     = (float)plot_color[0] / 255.0;
  fplot_color[1]     = (float)plot_color[1] / 255.0;
  fplot_color[2]     = (float)plot_color[2] / 255.0;
  curve_factor       = curve->curve_factor;
  apply_curve_factor = curve->apply_curve_factor;
  update_average     = curve->update_avg;
  vals               = curve->vals;

  xmin = x[0];
  xmax = xmin;
  for(i = 1; i < n; i++){
    xmin = MIN(xmin, x[i]);
    xmax = MAX(xmax, x[i]);
    }
  if(xmax == xmin)xmax = xmin + 1.0;
  if(xmax > xmin)xscale = 1.0 / (xmax - xmin);

  if(zmax == zmin)zmax = zmin + 1.0;
  if(zmax > zmin)zscale = 1.0 / (zmax - zmin);

  dx = (xmax - xmin) / 20.0;
  dz = (zmax - zmin) / 20.0;

  glPushMatrix();
  glScalef(SCALE2SMV(1.0), SCALE2SMV(1.0), SCALE2SMV(1.0));
  glTranslatef(SCALE2FDS(plot2d_xyz_offset[0]), SCALE2FDS(plot2d_xyz_offset[1]), SCALE2FDS(plot2d_xyz_offset[2]));

  glTranslatef(xyz0[0], xyz0[1], xyz0[2]);

  float az = camera_current->az_elev[0];
  glRotatef(-az, 0.0, 0.0, 1.0);

  float elev = camera_current->az_elev[1];
  glRotatef(-elev, 1.0, 0.0, 0.0);

  glScalef(SCALE2FDS(size_factor), SCALE2FDS(size_factor), SCALE2FDS(size_factor));
  glScalef(xscale, 1.0, zscale);
  glTranslatef(-xmin, 0.0, -zmin);
  glColor3ub((unsigned char)plot_color[0], (unsigned char)plot_color[1], (unsigned char)plot_color[2] );
  glLineWidth(linewidth_arg);
  SNIFF_ERRORS("after DrawGenCurve 1 - before");
  if(apply_curve_factor==1){
    glPushMatrix();
    glScalef(1.0, 1.0, curve_factor);
  }
  glBegin(GL_LINES);
  for(i = 0; i < n - 1; i++){
    glVertex3f(x[i],     0.0, z[i]);
    glVertex3f(x[i + 1], 0.0, z[i + 1]);
  }
  glEnd();
  if(apply_curve_factor==1)glPopMatrix();
  SNIFF_ERRORS("after DrawGenCurve 1 - after");
  if(option == PLOT_ALL){
    glColor3fv(foregroundcolor);
    glLineWidth(plot2d_line_width);
    glBegin(GL_LINES);
    glVertex3f(xmin, 0.0, zmin);
    glVertex3f(xmax, 0.0, zmin);

    glVertex3f(xmax, 0.0, zmin);
    glVertex3f(xmax, 0.0, zmax);

    glVertex3f(xmax, 0.0, zmax);
    glVertex3f(xmin, 0.0, zmax);

    glVertex3f(xmin, 0.0, zmax);
    glVertex3f(xmin, 0.0, zmin);

    glVertex3f(xmax, 0.0, zmax);
    glVertex3f(xmax, 0.0, zmax);

    glVertex3f(xmax, 0.0, zmin);
    glVertex3f(xmax, 0.0, zmin);
    glEnd();
    SNIFF_ERRORS("after DrawGenCurve 2");
  }

  glColor3f(1.0, 0.0, 0.0);
  glPointSize(plot2d_point_size);
  if(apply_curve_factor==1){
    glPushMatrix();
    glScalef(1.0, 1.0, curve_factor);
  }
  glBegin(GL_POINTS);
  glVertex3f(x_cur, 0.0, z_cur);
  glEnd();
  if(apply_curve_factor==1)glPopMatrix();
  SNIFF_ERRORS("after DrawGenCurve 3");

  if(showd_plot2d_labels == 1){
    float dfont = (float)GetFontHeight() / ((float)screenHeight * zscale * SCALE2FDS(size_factor) * SCALE2SMV(1.0));

    if(option == PLOT_ALL){
      char c_tmin[32], c_tmax[32];

      Float2String(c_tmin, x[0], ndigits, force_fixedpoint);
      Output3Text(foregroundcolor, xmin, 0.0, zmin - dz - dfont, c_tmin);

      Float2String(c_tmax, x[n - 1], ndigits, force_fixedpoint);
      Output3Text(foregroundcolor, xmax, 0.0, zmin - dz - dfont, c_tmax);
      if(show_title==1){
        Output3Text(foregroundcolor, xmin, 0.0, zmax + 1.5*dz, title);
      }
      SNIFF_ERRORS("after DrawGenCurve 4");
    }
    {
      char c_zmin[32], c_zmax[32], c_zcur[32];
      char label2[256];

      Float2String(c_zmin, zmin,  ndigits, force_fixedpoint);
      Float2String(c_zmax, zmax,  ndigits, force_fixedpoint);
      if(show_curve_values==1)Float2String(c_zcur, z_cur, ndigits, force_fixedpoint);
      strcpy(label2, "");
      if(show_curve_labels==1){
        strcat(label2, label);
        if(show_curve_values==1)strcat(label2, "/");
      }
      int pad_length_val;
      if(show_curve_values==1){
        strcat(label2, c_zcur);
        pad_length_val = GetStringLength(label2);
      }
      if(axis_side == AXIS_LEFT){
        if(show_curve_labels==1 || show_curve_values==1){
          Output3Text(fplot_color,     xmax + 2.0 * dx,
                      0.0, zmax - (0.5 + plot2d_font_spacing * (float)position) * dfont, label2);
        }
        Output3Text(foregroundcolor, xmax + 2.0 * dx, 0.0, zmin,  c_zmin);
        Output3Text(foregroundcolor, xmax + 2.0 * dx, 0.0, zmax , c_zmax);
        }
      else{
        if(show_curve_labels==1 || show_curve_values==1){
          if(show_curve_values==1){
            Output3TextRight(fplot_color,      xmin - dx,
                             0.0, zmax - (0.5 + plot2d_font_spacing * (float)position) * dfont, label2, pad_length_val);
          }
          else{
            Output3TextRight(fplot_color,      xmin - dx,
                             0.0, zmax - (0.5 + plot2d_font_spacing * (float)position) * dfont, label2, pad_length);
          }
        }
        Output3TextRight(foregroundcolor, xmin - dx, 0.0, zmin,  c_zmin, pad_length);
        Output3TextRight(foregroundcolor, xmin - dx, 0.0, zmax , c_zmax, pad_length);
      }
      SNIFF_ERRORS("after DrawGenCurve 5");
    }
    if(unit!=NULL){
      if(axis_side == AXIS_LEFT){
        Output3Text(foregroundcolor, xmax + 2.0 * dx, 0.0, zmax - (0.5 + plot2d_font_spacing*(float)(position + 1))*dfont, unit);
      }
      else{
        Output3TextRight(foregroundcolor, xmin - dx, 0.0, zmax - (0.5 + plot2d_font_spacing*(float)(position + 1))*dfont, unit, pad_length);
      }
    }
  }
  glPopMatrix();
  SNIFF_ERRORS("after DrawGenCurve end");
}

/* ------------------ GetPlotUnit ------------------------ */

char *GetPlotUnit(plot2ddata *plot2di, int curv_index){
  csvdata *csvi;

  csvi = GetCsv(plot2di->curve[curv_index].csv_file_index, plot2di->curve[curv_index].csv_col_index, NULL);
  return csvi->label.unit;
}

/* ------------------ GetPlotShortLabel ------------------------ */

char *GetPlotShortLabel(plot2ddata *plot2di, int curv_index){
  csvdata *csvi;

  csvi = GetCsv(plot2di->curve[curv_index].csv_file_index, plot2di->curve[curv_index].csv_col_index, NULL);
  return csvi->label.shortlabel;
}

/* ------------------ GetCSVVal ------------------------ */

float GetCSVVal(float t, float *times, float *vals, int nvals){
  int beg, mid, end;

  if(t<times[0])return vals[0];
  if(t>times[nvals-1])return vals[nvals-1];
  beg = 0;
  end = nvals-1;
  mid = (beg+end)/2;
  while(end-beg>1){
    mid = (beg+end)/2;
    if(t<times[mid]){
      end = mid;
    }
    else{
      beg = mid;
    }
  }
  return vals[mid];
}

/* ------------------ UpdateCurveBounds ------------------------ */

void UpdateCurveBounds(plot2ddata *plot2di, int option){
  int i;

  if(option==1){
    for(i = 0; i<PLOT2D_MAX_CURVES; i++){
      curvedata *curve;

      curve = plot2di->curve+i;
      curve->color[0]           = 0;
      curve->color[1]           = 0;
      curve->color[2]           = 0;
      curve->linewidth          = 1.0;
      curve->apply_curve_factor = 0;
      curve->curve_factor       = 1.0;
      curve->update_avg         = 0;
      curve->vals               = NULL;
    }
  }
  for(i = 0; i<ncsvfileinfo; i++){
    int j;
    csvfiledata *csvfi;

    csvfi = csvfileinfo+i;
    for(j = 0; j<csvfi->ncsvinfo; j++){
      csvdata *csvi;
      float valmin, valmax;
      int k;

      csvi = csvfi->csvinfo+j;
      valmin = csvi->vals[0];
      valmax = valmin;
      for(k = 1; k<csvi->nvals; k++){
        valmin = MIN(valmin, csvi->vals[k]);
        valmax = MAX(valmax, csvi->vals[k]);
      }
      csvi->valmin = valmin;
      csvi->valmax = valmax;
    }
  }

  if(plot2di->ncurves==0){
    plot2di->bounds_defined = 0;
  }
  else{
    plot2di->bounds_defined = 1;
  }
  for(i = 0; i<plot2di->ncurves; i++){
    curvedata *curve;
    csvdata *csvi;

    curve = plot2di->curve+i;
    csvi = GetCsv(curve->csv_file_index, curve->csv_col_index, NULL);
    curve->vmin = csvi->valmin;
    curve->vmax = csvi->valmax;
  }
  for(i = plot2di->ncurves; i<PLOT2D_MAX_CURVES; i++){
    curvedata *curve;

    curve = plot2di->curve+i;
    curve->vmin = 0.0;
    curve->vmax = 1.0;
  }
}

/* ------------------ DrawGenPlot ------------------------ */

void DrawGenPlot(plot2ddata *plot2di){
  int i;
  char *axis_right_unit = NULL, *axis_left_unit = NULL;
  float axis_left_min   = 1.0,  axis_left_max  = 0.0;
  float axis_right_min  = 1.0,  axis_right_max = 0.0;
  int left_position  = 0;
  int right_position = 0;
  int unit_left_index=0, unit_right_index=0;
  float pad_length = 0.0;

  if(plot2di->bounds_defined==0)UpdateCurveBounds(plot2di, 0);
  for(i = 0; i<plot2di->ncurves; i++){
    char *unit;

    unit = GetPlotUnit(plot2di, i);
    if(axis_right_unit == NULL){
      axis_right_unit = unit;
      continue;
    }
    if(strcmp(unit, axis_right_unit) != 0){
      axis_left_unit = unit;
      break;
    }
  }
  for(i = 0; i < plot2di->ncurves; i++){
    char *unit, *label;

    unit  = GetPlotUnit(plot2di, i);
    label = GetPlotShortLabel(plot2di, i);
    if(axis_right_unit != NULL && strcmp(axis_right_unit, unit) == 0){
      unit_right_index = i;
      pad_length = MAX(pad_length, GetStringLength(label));
      continue;
    }
    if(axis_left_unit != NULL && strcmp(axis_left_unit, unit) == 0){
      unit_left_index = i;
      continue;
    }
  }
  for(i = 0; i < plot2di->ncurves; i++){
    float valmin, valmax;
    char *unit;
    curvedata *curve;

    curve = plot2di->curve+i;
    valmin = curve->vmin;
    valmax = curve->vmax;
    if(curve->apply_curve_factor==1){
      valmin *= curve->curve_factor;
      valmax *= curve->curve_factor;
    }
    unit = GetPlotUnit(plot2di, i);
    if(axis_right_unit!=NULL&&strcmp(unit, axis_right_unit) == 0){
      if(axis_right_min>axis_right_max){
        axis_right_min = valmin;
        axis_right_max = valmax;
      }
      else{
        axis_right_min = MIN(axis_right_min,valmin);
        axis_right_max = MAX(axis_right_max,valmax);
      }
    }
    if(axis_left_unit!=NULL&&strcmp(unit, axis_left_unit) == 0){
      if(axis_left_min>axis_left_max){
        axis_left_min = valmin;
        axis_left_max = valmax;
      }
      else{
        axis_left_min = MIN(axis_left_min,valmin);
        axis_left_max = MAX(axis_left_max,valmax);
      }
    }
  }
  for(i = 0; i<plot2di->ncurves; i++){
    float highlight_time, highlight_val;
    char *unit, *shortlabel;
    float valmin, valmax;
    int option, position, side;
    char *unit_display;
    curvedata *curve;

    if(axis_right_unit == NULL)break;
    unit = GetPlotUnit(plot2di, i);
    if(unit==NULL)continue;
    unit_display = NULL;
    if(strcmp(unit, axis_right_unit) == 0){
      right_position++;
      position = right_position;
      valmin   = axis_right_min;
      valmax   = axis_right_max;
      side     = AXIS_RIGHT;
      if(unit_right_index==i)unit_display = unit;
    }
    else{
      if(axis_left_unit == NULL || strcmp(unit, axis_left_unit) != 0)continue;
      left_position++;
      position = left_position;
      valmin   = axis_left_min;
      valmax   = axis_left_max;
      side     = AXIS_LEFT;
      if(unit_left_index==i)unit_display = unit;
    }
    if(position == 1){
      option = PLOT_ALL;
    }
    else{
      option = PLOT_ONLY_DATA;
    }
    if(side == AXIS_LEFT){
      if(plot2di->use_valmax[1] == 1)valmax = plot2di->valmax[1];
      if(plot2di->use_valmin[1] == 1)valmin = plot2di->valmin[1];
    }
    if(side == AXIS_RIGHT){
      if(plot2di->use_valmax[0] == 1)valmax = plot2di->valmax[0];
      if(plot2di->use_valmin[0] == 1)valmin = plot2di->valmin[0];
    }
    csvfiledata *csvfi;
    csvdata *csvi;

    curve = plot2di->curve + i;
    csvi = GetCsv(curve->csv_file_index, curve->csv_col_index, &csvfi);

    if(global_times != NULL){
      highlight_time = global_times[itimes];
      highlight_val = GetCSVVal(global_times[itimes], csvfi->time->vals, csvi->vals, csvi->nvals);
    }
    shortlabel = GetPlotShortLabel(plot2di, i);
    float *vals;
    if(curve->vals==NULL){
      NewMemory((void **)&curve->vals, csvi->nvals * sizeof(devicedata *));
    }
    if(curve->update_avg==1||device_time_average>0.0){
      if(curve->update_avg==1){
        curve->update_avg = 0;
        TimeAveragePlot2DData(csvfi->time->vals, csvi->vals, curve->vals, csvi->nvals);
      }
    }
    else{
      memcpy(curve->vals, csvi->vals, csvi->nvals*sizeof(float));
    }
    DrawGenCurve(option, plot2di, curve, plot2d_size_factor, csvfi->time->vals, curve->vals, csvi->nvals,
                 highlight_time, highlight_val, valmin, valmax, side,
                 position, shortlabel, unit_display, pad_length);
  }
}

  /* ------------------ DrawGenPlots ------------------------ */

void DrawGenPlots(void){
  int i;

  for(i = 0; i < nplot2dinfo;i++){
    plot2ddata *plot2di;

    plot2di = plot2dinfo + i;
    if(plot2di->show == 1){
      DrawGenPlot(plot2di);
    }
  }
}

/* ----------------------- SetupPlot2DUnitData ----------------------------- */

void SetupPlot2DUnitData(void){

  //setup deviceunits
  if(ndeviceinfo > 0){
    int i;

    ndeviceunits = 0;
    FREEMEMORY(deviceunits);
    NewMemory((void **)&deviceunits, ndeviceinfo * sizeof(devicedata *));
    for(i = 0; i < ndeviceinfo; i++){
      int j;
      devicedata *devi;
      int skip_dev;

      devi = deviceinfo + i;
      if(devi->nvals == 0 || strlen(devi->quantity) == 0 || strlen(devi->unit) == 0)continue;
      skip_dev = 0;
      for(j = 0; j < ndeviceunits; j++){
        devicedata *devj;

        devj = deviceunits[j];
        if(strcmp(devi->unit, devj->unit) == 0){
          skip_dev = 1;
          break;
        }
      }
      if(skip_dev == 1)continue;
      deviceunits[ndeviceunits++] = devi;
    }
  }
  //setup hrrunits
  if(nhrrinfo > 0){
    int i;

    nhrrunits = 0;
    FREEMEMORY(hrrunits);
    NewMemory((void **)&hrrunits, nhrrinfo * sizeof(hrrdata *));
    for(i = 0; i < nhrrinfo; i++){
      int j;
      hrrdata *hrri;
      int skip_hrr;

      hrri = hrrinfo + i;
      if(hrri->nvals == 0 || strlen(hrri->label.shortlabel) == 0 || strlen(hrri->label.unit) == 0)continue;
      if(STRCMP(hrri->label.shortlabel, "Time") == 0)continue;
      skip_hrr = 0;
      for(j = 0; j < nhrrunits; j++){
        hrrdata *hrrj;

        hrrj = hrrunits[j];
        if(strcmp(hrri->label.unit, hrrj->label.unit) == 0){
          skip_hrr = 1;
          break;
        }
      }
      if(skip_hrr == 1)continue;
      hrrunits[nhrrunits++] = hrri;
    }
  }
}

/* ------------------ GetPlot2DBounds ------------------------ */

void GetPlot2DBounds(plot2ddata *plot2di, float *valmin, float *valmax){
  int i;
  char *axis_right_unit = NULL, *axis_left_unit = NULL;
  float axis_left_min = 1.0, axis_left_max = 0.0;
  float axis_right_min = 1.0, axis_right_max = 0.0;

  if(plot2di->bounds_defined==0)UpdateCurveBounds(plot2di, 0);
  for(i = 0; i<plot2di->ncurves; i++){
    char *unit;

    unit = GetPlotUnit(plot2di, i);
    if(axis_right_unit==NULL){
      axis_right_unit = unit;
      continue;
    }
    if(strcmp(unit, axis_right_unit)!=0){
      axis_left_unit = unit;
      break;
    }
  }
  for(i = 0; i<plot2di->ncurves; i++){
    float vmin, vmax;
    char *unit;
    curvedata *curve;

    curve = plot2di->curve+i;
    vmin = curve->vmin;
    vmax = curve->vmax;
    unit = GetPlotUnit(plot2di, i);
    if(axis_right_unit!=NULL&&strcmp(unit, axis_right_unit)==0){
      if(axis_right_min>axis_right_max){
        axis_right_min = vmin;
        axis_right_max = vmax;
      }
      else{
        axis_right_min = MIN(axis_right_min, vmin);
        axis_right_max = MAX(axis_right_max, vmax);
      }
    }
    if(axis_left_unit!=NULL&&strcmp(unit, axis_left_unit)==0){
      if(axis_left_min>axis_left_max){
        axis_left_min = vmin;
        axis_left_max = vmax;
      }
      else{
        axis_left_min = MIN(axis_left_min, vmin);
        axis_left_max = MAX(axis_left_max, vmax);
      }
    }
  }
  valmin[0] = axis_right_min;
  valmin[1] = axis_left_min;
  valmax[0] = axis_right_max;
  valmax[1] = axis_left_max;
}

/* ------------------ InitPlot2D ------------------------ */

void InitPlot2D(plot2ddata *plot2di, int plot_index){
  if(ndeviceinfo == 0 && nhrrinfo == 0)return;
  plot2di->ncurves = 0;
  plot2di->ncurves_ini = 0;
  plot2di->show = 0;
  plot2di->show_title = 0;
  plot2di->show_curve_labels = 0;
  plot2di->show_curve_values = 0;
  plot2di->xyz[0] = xbar0FDS;
  plot2di->xyz[1] = ybar0FDS;
  plot2di->xyz[2] = zbar0FDS;
  plot2di->use_valmin[0] = 0;
  plot2di->use_valmin[1] = 0;
  plot2di->use_valmax[0] = 0;
  plot2di->use_valmax[1] = 0;
  plot2di->valmin[0] = 0.0;
  plot2di->valmin[1] = 0.0;
  plot2di->valmax[0] = 1.0;
  plot2di->valmax[1] = 1.0;
  plot2di->plot_index = plot_index;
  sprintf(plot2di->plot_label, "plot %i", plot_index);
  plot2di->curve_index = 0;
  UpdateCurveBounds(plot2di, 1);
  }

  /* ------------------ DrawPlot ------------------------ */

void DrawPlot(int option, float *xyz0, float factor, float *x, float *z, int n,
              float highlight_x, float highlight_y, int valid,
              float global_valmin, float global_valmax, char *quantity, char *unit){
  float xmin, xmax, zmin, zmax, dx, dz;
  float zmax_display;
  float xscale=1.0, zscale=1.0;
  float origin[3];
  int i;
  char cvalmin[20], cvalmax[20], cval[20];
  int ndigits = 3;

  origin[0] = xyz0[0];
  origin[1] = xyz0[1];
  origin[2] = xyz0[2];

  xmin = x[0];
  xmax = xmin;
  zmin = z[0];
  zmax = zmin;
  for(i = 1; i<n; i++){
    xmin = MIN(xmin, x[i]);
    xmax = MAX(xmax, x[i]);
    zmin = MIN(zmin, z[i]);
    zmax = MAX(zmax, z[i]);
  }
  if(xmax==xmin)xmax=xmin+1.0;
  if(xmax>xmin)xscale = 1.0/(xmax-xmin);

  if(global_valmin<global_valmax){
    zmin = global_valmin;
    zmax = global_valmax;
  }
  zmax_display = zmax;
  if(zmax==zmin)zmax=zmin+1.0;
  if(zmax>zmin)zscale = 1.0/(zmax-zmin);

  Float2String(cvalmin, zmin,         ndigits, force_fixedpoint);
  Float2String(cvalmax, zmax_display, ndigits, force_fixedpoint);
  Float2String(cval,     highlight_y, ndigits, force_fixedpoint);

  dx = (xmax - xmin)/20.0;
  dz = (zmax - zmin)/20.0;

  glPushMatrix();
  glScalef(SCALE2SMV(1.0), SCALE2SMV(1.0), SCALE2SMV(1.0));
  glTranslatef(SCALE2FDS(plot2d_xyz_offset[0]), SCALE2FDS(plot2d_xyz_offset[1]), SCALE2FDS(plot2d_xyz_offset[2]));

  glTranslatef(origin[0], origin[1], origin[2]);

  float az = camera_current->az_elev[0];
  glRotatef(-az, 0.0,0.0,1.0);

  float elev = camera_current->az_elev[1];
  glRotatef(-elev, 1.0, 0.0, 0.0);

  glScalef(SCALE2FDS(factor), SCALE2FDS(factor), SCALE2FDS(factor));
  glScalef(xscale, 1.0, zscale);
  glTranslatef(-xmin, 0.0, -zmin);
  glColor3fv(foregroundcolor);
  glLineWidth(plot2d_line_width);
  glBegin(GL_LINES);
  for(i = 0; i<n-1; i++){
    glVertex3f(x[i],   0.0, z[i]);
    glVertex3f(x[i+1], 0.0, z[i+1]);
  }

  if(option == PLOT_ALL){
    glVertex3f(xmin - dx, 0.0, zmin - dz);
    glVertex3f(xmax + dx, 0.0, zmin - dz);

    glVertex3f(xmax + dx, 0.0, zmin - dz);
    glVertex3f(xmax + dx, 0.0, zmax + dz);

    glVertex3f(xmax + dx, 0.0, zmax + dz);
    glVertex3f(xmin - dx, 0.0, zmax + dz);

    glVertex3f(xmin - dx, 0.0, zmax + dz);
    glVertex3f(xmin - dx, 0.0, zmin - dz);

    glVertex3f(xmax,      0.0, zmax);
    glVertex3f(xmax + dx, 0.0, zmax);

    glVertex3f(xmax,      0.0, zmin);
    glVertex3f(xmax + dx, 0.0, zmin);
  }
  glEnd();

  float dfont = (float)GetFontHeight()/((float)screenHeight*zscale*SCALE2FDS(factor)*SCALE2SMV(1.0));

  if(option == PLOT_ALL && showd_plot2d_labels==1){
    float zmid;

    zmid = (zmax-2.0*dfont+zmin)/2.0;
    Output3Text(foregroundcolor, xmax + 2.0*dx, 0.0, zmax-0.5*dfont, cvalmax);
    Output3Text(foregroundcolor, xmax + 2.0*dx, 0.0, zmax-1.7*dfont, quantity);
    Output3Text(foregroundcolor, xmax + 2.0*dx, 0.0, zmax-2.9*dfont, unit);
    Output3Text(foregroundcolor, xmax + 2.0*dx, 0.0, zmid-0.5*dfont, cval);
    Output3Text(foregroundcolor, xmax + 2.0*dx, 0.0, zmin-0.5*dfont, cvalmin);
  }

  if(valid==1){
    glColor3f(1.0,0.0,0.0);
    glPointSize(plot2d_point_size);
    glBegin(GL_POINTS);
    glVertex3f(highlight_x, 0.0, highlight_y);
    glEnd();
  }

  glPopMatrix();
}

/* ------------------ TimeAveragePlot2DData ------------------------ */

void TimeAveragePlot2DData(float *times, float *vals, float *vals_avg, int nvals){
  int i;

  if(nvals<=0)return;
  if(times[nvals-1]<=device_time_average){
    float sum = 0.0;

    for(i = 0; i<nvals; i++){
      sum += vals[i];
    }
    sum /= (float)nvals;
    for(i = 0; i<nvals; i++){
      vals_avg[i] = sum;
    }
    return;
  }
  for(i = 0; i<nvals; i++){
    float tlower, tupper;
    int ilower, iupper;
    float sum;
    int j;
    int count;

    if(times[i]>=device_time_average/2.0&&times[i]<=times[nvals-1]-device_time_average/2.0){
      tlower = times[i]-device_time_average/2.0;
      tupper = tlower+device_time_average;
    }
    else if(times[i]<=device_time_average/2.0){
      tlower = times[0];
      tupper = tlower+device_time_average;
    }
    else{
      tupper = times[nvals-1];
      tlower = tupper-device_time_average;
    }
    for(j = i; j>=0; j--){
      ilower = j;
      if(times[j]<=tlower)break;
    }
    for(j = i; j<nvals; j++){
      iupper = j;
      if(times[j]>=tupper)break;
    }
    sum = 0.0;
    count = 0;
    for(j = ilower; j<=iupper;j++){
      if(times[j]>=tlower&&times[j]<=tupper){
        sum += vals[j];
        count++;
      }
    }
    if(count>0){
      vals_avg[i] = sum/(float)(count);
    }
    else{
      vals_avg[i] = vals[i];
    }
  }
}

/* ----------------------- DrawDevicePlots ----------------------------- */

void DrawDevicePlots(void){
  int i;

  if(vis_device_plot!=DEVICE_PLOT_HIDDEN){
    for(i = 0; i<ndeviceinfo; i++){
      devicedata *devicei;

      devicei = deviceinfo+i;
      if(vis_device_plot==DEVICE_PLOT_SHOW_SELECTED&&devicei->selected==0)continue;
      if(devicei->times==NULL||devicei->vals==NULL)continue;
      if(devicei->update_avg==1){
        devicei->update_avg = 0;
        TimeAveragePlot2DData(devicei->times, devicei->vals_orig, devicei->vals, devicei->nvals);
      }
      if(devicei->nvals>1&&devicei->type2==devicetypes_index){
        int valid;
        float highlight_time = 0.0, highlight_val = 0.0;

        valid = 0;
        if(global_times!=NULL){
          highlight_time = global_times[itimes];
          highlight_val = GetDeviceVal(global_times[itimes], devicei, &valid);
        }
        if(devicei->global_valmin>devicei->global_valmax){
          GetGlobalDeviceBounds(devicei->type2);
        }
        DrawPlot(PLOT_ALL, devicei->xyz, plot2d_size_factor, devicei->times, devicei->vals, devicei->nvals,
                 highlight_time, highlight_val, valid, devicei->global_valmin, devicei->global_valmax,
                 devicei->quantity, devicei->unit
        );
      }
    }
  }
}

/* ----------------------- DrawTreePlot ----------------------------- */

void DrawTreePlot(int first, int n){
  int j;
  int drawplot = 0;
  float *xyz = NULL;

  for(j=0;j<n;j++){
    devicedata *devicei;
    int valid, option;
    float highlight_time = 0.0, highlight_val = 0.0;

    devicei = deviceinfo_sortedz[first+j];
    if(devicei->object->visible==0)continue;
    if(devicei->times==NULL||devicei->vals==NULL)continue;
    if(devicei->nvals<=1||devicei->type2!=devicetypes_index)continue;
    drawplot++;

    if(drawplot==1){
      option = PLOT_ALL;
      xyz = devicei->xyz;
    }
    else{
      option = PLOT_ONLY_DATA;
    }
    valid = 0;
    if(global_times!=NULL){
      highlight_time = global_times[itimes];
      highlight_val = GetDeviceVal(global_times[itimes], devicei, &valid);
    }
    if(devicei->global_valmin>devicei->global_valmax){
      GetGlobalDeviceBounds(devicei->type2);
    }
    DrawPlot(option, xyz, plot2d_size_factor, devicei->times, devicei->vals, devicei->nvals,
             highlight_time, highlight_val, valid, devicei->global_valmin, devicei->global_valmax,
             devicei->quantity, devicei->unit
    );
  }
}

/* ----------------------- DrawTreeDevicePlots ----------------------------- */

void DrawTreeDevicePlots(void){
  int i;

  for(i = 0; i<nztreedeviceinfo; i++){
    ztreedevicedata *ztreei;

    ztreei = ztreedeviceinfo+i;
    DrawTreePlot(ztreei->first, ztreei->n);
  }
}
