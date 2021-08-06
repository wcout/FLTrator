#include <FL/Fl_Image.H>

#if !(FLTK_HAS_IMAGE_SCALING)

#include <string.h>
#include <stdint.h> // uint64_t

// Written (c) by Roman Kantor
// The code is Free software distributed under LGPL license with FLTK exceptions, see http://www.fltk.org/COPYING.php

// TWO-dimensional scaling is performend in two steps by rescaling first in one direction
// and then in the other - which at the end has the same effect as direct two-dimensional rescaling...
// All routines are performed with "integer" types for the speed, precision is not lost because the result is scaled up
// and only after resampling in both directions the values are normalised (scaled back) to the range 0-255.
// The macro routines are written to be general and can be used with different types, number of channels,
// sample padding (stride), row padding, can encode/decode the channels from the original data etc...


// ONE-DIMENSIONAL ROUTINES

// a) DOWNSAMPLING (w2<w1):
// Following method seems to work better than classic (bi)linear interpolation as all the original pixels contribute to the
// resulting resampled images with the same weight - that is no pixels are lost or suppressed.
//  Downsampling is performed by "weighted" pixel combinations:
// 1) If the whole original pixel lies within the area of a new pixel, its whole value contributes to that particular new pixel.
// 2) If an original pixel lies on the boundary of two new pixels, its contribution is split to these two pixels with
//    a ratio corresponding to the areas within these pixels. The sum of these two contributions is equal to the whole
//    contribution as in case 1)
// All the contributions are then summed up, the sum of weights gives the normalization (scaling) factor equal to f = w1.
// This factor should be used later to divide all result values thus scaling them back to its original range
// (usually to range 0-255 for 8-bit channels for RGB(A) images)


// b) UPSAMPLING:
// For upsampling linear combination is performed where resulting pixel value is  P = a2 * P1 + a1 * P2
// The contributing factors a2, a1 are proportional to the (swapped) distances from  the surrounding original pixels.
// The normalizarion factor is equal to f = a2 + a1 = 2 * w2



// Macro parameters:
// T_SOURCE: type of source data (eg unsigned char)
// T_TARGET: type of ratrget tata (eg unsigned int)
// T_ACU type of intermediate data. This type hould be a type "big enough" to hold the scaled values so it does not overflow after all pixel combination.
//       The maximum value is equal to maximum pixel value times normalization factor. For instance for downscaling if T_SOURCE type is 8-bit
//       "unsigned char", T_ACU can be 32-bit "unsigned int" without overflow for all images with original width w1 < 2^24.  However if you combine
//       row/collumn resampling, the scale factor is multiplication of both and could easily oferflow 32 bitif scaling down images with sizes above 4096x4096.
//       For that reason we use 64-bit type for T_ACU for the second scaling during second dimension resampling.
// source - (const) pointer to source data array
// target - pointer to target data array
// source_stride - pointer shift between samples (normaly for packed data it is equal to number of channels)
// target_stride - pinter shift between resulting data
// PREPROCESS - pre-processing sequence (code) for raw data stored in "input" variable (of type T_ACU). This is useful ie if multi-channels are encoded
//       within single sample (like when ARGB channels are encoded within single "int" sample), getting red channel can be performed by  putting sequence
//           val >>=16; val &= 0xFF;
//       in the place of PREPROCESS parameter.
// POSTPROCESS_STORE - post-processing sequence (code), can be used eg for rescaling back the data or channel encoding where symbol "output" holds
//       raw interpolated data and "tg" is the pointer where the result should be stored. This pointer can already have an initial value.
//       For instance for multi-channel encoding and "red channel" you can write something like
//           *tg |= (((output + (scale_factor>>1)/scale_factor) & 0xFF) << 16;
//        If you do not perform additional rescaling (*tg = output;) the value in "output" are scaled by the normalization factor
//       (f = w1    for downscaling and    f = 2 * w2)

#define RESAMPLE_DOWN(T_SOURCE, T_TARGET, T_ACU, w1, w2, source, target, source_stride, target_stride, PREPROCESS, POSTPROCESS_STORE) \
  { \
    const T_SOURCE * src = source; \
    T_TARGET * tg = target; \
    T_ACU pre = 0; \
    T_ACU input; \
    T_ACU output; \
    unsigned rest = w1; \
    for(unsigned int i = w2 - 1; i; i--) { \
      T_ACU full = 0; \
      while(rest>=w2) { \
        input = *src; \
        PREPROCESS \
        src += source_stride; \
        full += input; \
        rest -= w2; \
      } \
      input = *src; \
      PREPROCESS \
      src += source_stride; \
      output = pre + w2 * full + rest * input; \
      POSTPROCESS_STORE \
      tg += target_stride; \
      pre = (w2 - rest) * input; \
      rest += w1 - w2; \
    } \
    output = 0; \
    while(rest>=w2) { \
      input = *src;\
      PREPROCESS \
      output += input;\
      src += source_stride; \
      rest -= w2; \
    } \
    output *= w2; \
    output += pre; \
    POSTPROCESS_STORE \
  }

// Resampling by linear interpolation
// This is similar to RESAMPLE_DOWN however this time the result is scaled by factor  f = 2 * w2  - unless modified by POSTPROCESS_STORE sequence.
#define RESAMPLE_UP(T_SOURCE, T_TARGET, T_ACU, w1, w2, source, target, source_stride, target_stride, PREPROCESS, POSTPROCESS_STORE) \
  { \
    unsigned  W2 = w2 * 2; \
    unsigned W1 = w1 * 2; \
    const T_SOURCE * src = source; \
    T_TARGET * tg = target; \
    T_ACU input = *src; \
    PREPROCESS \
    T_ACU output = input * W2; \
    T_ACU tmp = output; \
    unsigned i = 0; \
    unsigned mod = w1; \
    while(mod<=w2){ \
      POSTPROCESS_STORE \
      output = tmp; \
      mod += W1; \
      tg += target_stride; \
      i++; \
    } \
    mod -= w2; \
    T_ACU pre_input = input; \
    src += source_stride; \
    input = *src; \
    PREPROCESS \
    for(unsigned no = w2 - 2 * i; no; no--) { \
      if(mod>=W2){ \
        mod -=W2; \
        pre_input = input; \
        src += source_stride;\
        input = *src; \
        PREPROCESS \
      } \
      T_ACU output = input * mod + (W2 - mod) * pre_input; \
      POSTPROCESS_STORE \
      tg += target_stride; \
      mod += W1;\
    } \
    output = W2 * input; \
    tmp = output; \
    while(i){\
      POSTPROCESS_STORE \
      tg += target_stride; \
      output = tmp; \
      i--; \
    } \
 }



namespace { // anonymous namespace


// Although we could use types from <stdint.h>, there is no assurance that int64_t is available.
// Anyway the types below are reasonable expectations as:
// 1) "int" is assumed by fltk to be at least 32 bit wide (otherwise fltk won't work)
// 2) "long long" is the longest "standard" type available and it is usually 64 bit wide

typedef unsigned int U32; // use type at least 32 bit wide
#ifdef HAVE_LONG_LONG
typedef unsigned long long U64; // use type at least 48 bit wide
#else
typedef uint64_t U64; // use type at least 48 bit wide
#endif


U32 get_resample_scale(unsigned w1, unsigned w2)
{
  if(w2 < w1) return w1;
  return 2 * w2;
}


// resampling from "unsigned char" to intermediate "unsigned int" array. The function returns the scaling factor.
unsigned resample(unsigned w1, unsigned w2, const unsigned char * source, U32 * target,
                  int source_stride, int target_stride,
                  unsigned sets, int source_set_stride, int target_set_stride,
                  int alpha_shift)
{
  if(w2 < w1) {
    for(unsigned j = 0; j < sets; j++) {
      const unsigned char * sou = source + j * source_set_stride;
      U32 * tar = target + j * target_set_stride;
      if(alpha_shift)
        RESAMPLE_DOWN(unsigned char, U32, U32, w1, w2, sou, tar,
                      source_stride, target_stride,
                      input *= src[alpha_shift];, *tg = output;)
      else
        RESAMPLE_DOWN(unsigned char, U32, U32, w1, w2, sou, tar,
                      source_stride, target_stride,
                      ;, *tg = output;)
    }
    return w1;
  }
  for(unsigned j = 0; j < sets; j++) {
    const unsigned char * sou = source + j * source_set_stride;
    U32 * tar = target + j * target_set_stride;
    if(alpha_shift)
      RESAMPLE_UP(unsigned char, U32, U32, w1, w2, sou, tar,
                  source_stride, target_stride,
                  input *= src[alpha_shift];, *tg = output;)
    else
      RESAMPLE_UP(unsigned char, U32, U32, w1, w2, sou, tar,
                  source_stride, target_stride,
                  ;, *tg = output;)
  }
  return 2 * w2;
}



// resampling from intermediate "unsigned" to final "unsigned char" with scaling
void resample_scale(unsigned w1, unsigned w2, const U32 * source, unsigned char * target,
                    int source_stride, int target_stride,
                    unsigned sets, int source_set_stride, int target_set_stride,
                    U64 scale, int alpha_shift)
{
  U64 sc_add = scale / 2;
  U64 alpha;
  if(w2 < w1) {
    for(unsigned j = 0; j < sets; j++) {
      const U32 * sou = source + j * source_set_stride;
      unsigned char * tar = target + j * target_set_stride;
      if(alpha_shift)
        RESAMPLE_DOWN(U32, unsigned char, U64, w1, w2, sou, tar,
                      source_stride, target_stride,
                      ;, alpha = scale * tg[alpha_shift];
                      if(!alpha) alpha = 1;
                      *tg = (output + sc_add + alpha / 2) / (alpha + scale);)
      else
        RESAMPLE_DOWN(U32, unsigned char, U64, w1, w2, sou, tar,
                      source_stride, target_stride,
                      ;, *tg = (output + sc_add) / scale;)
    }
    return;
  }
  for(unsigned j = 0; j < sets; j++) {
    const U32 * sou = source + j * source_set_stride;
    unsigned char * tar = target + j * target_set_stride;
    if(alpha_shift)
      RESAMPLE_UP(U32, unsigned char, U64, w1, w2, sou, tar,
                  source_stride, target_stride,
                  ;, alpha = scale * tg[alpha_shift];
                  if(!alpha) alpha = 1;
                  *tg = (output + sc_add + alpha / 2) / (alpha + scale);)
    else
      RESAMPLE_UP(U32, unsigned char, U64, w1, w2, sou, tar,
                  source_stride, target_stride,
                  ;, *tg = (output + sc_add) / scale;)
  }
}


// Finaly this is synthesis of the above functions to use optimal resizing approach.
// Parameters:
// w2, h2 - new (resampled) image size
// source - pointer to original image data
// w1, h1 - size of the original image
// no_channels - number of adjacent channels (1 for mono images, 3 for RGB and 4 for RGBA or ARGB)
// pixel_stride - pointer shift from pixel to pixel for the original image. If 0, "packed" data are assumed and pixel_stride equals to no_channels
// row_stride - pointer shift between rows for the original image. If 0, row_stride = w1 * pixel_stride.
// target - pointer to array where resampled image data are stored. If 0, a new array is allocated with sufficient size.
// target_pixel_stride - pointer shift from pixel to pixel for the resampled image. If 0, "packed" data are assumed and target_pixel_stride equals to no_channels
// target_row_stride - pointer shift between rows for the resampled image. If 0, target_row_stride = w2 * target_pixel_stride.
// The function returns the "target" parameter or pointer to newly allocated data (if target==0) - in such a case free this memory using operator delete[].


unsigned char * resample(int w2, int h2, const unsigned char * const source,
                         int w1, int h1, int no_channels,
                         int pixel_stride = 0, int row_stride = 0,
                         unsigned char * target = 0, int target_pixel_stride = 0, int target_row_stride = 0,
                         bool alpha_premultiply = false)
{
  if(!pixel_stride) pixel_stride = no_channels;
  if(!row_stride) row_stride = pixel_stride * w1;
  const unsigned char * src = source;
  //alpha_premultiply = 0;
  int n_ch = no_channels - 1;
  int alph_shift_multiplier = 0;
  if(alpha_premultiply)
    alph_shift_multiplier = 1;


  if (pixel_stride < 0) {
    src += pixel_stride * (w1 - 1);
    pixel_stride = -pixel_stride;
  }
  if (row_stride < 0) {
    src +=  row_stride * (h1 - 1);
    row_stride = -row_stride;
  }
  if(!target_pixel_stride) target_pixel_stride = no_channels;
  if(!target_row_stride) target_row_stride = target_pixel_stride * w2;
  int size = target_row_stride * h2;
  if(!target)
    target = new unsigned char[size];

  if(w1 == w2 && h1 == h2) { // direct copy
    if(target_pixel_stride == pixel_stride) {
      if(target_row_stride == row_stride)
        memcpy(target, src, size * sizeof(unsigned char));
      else for(int i = 0; i < h1; i++)
        memcpy(target + i * target_row_stride , src + row_stride , row_stride * sizeof(unsigned char));
    } else {
      for(int j = 0; j < h1; j++) {
        const unsigned char * s = src + j * row_stride;
        unsigned char * t = target + j * target_row_stride;
        int s_addition = pixel_stride - no_channels;
        int t_addition = target_pixel_stride - no_channels;
        for(int i = 0; i < w1; i++) {
          for(int c = no_channels; c; c--)
            *t++ = *s++;
          s += s_addition;
          t += t_addition;
        }
      }
    }
    return target;
  }


  // General rescaling with intermediate U32 data array.
  // First we try to find optimal approach to minimize intermediate data size:
  int s = w2 * h1;
  bool rows_first = 1;
  {
    int s2 = w1 * h2;
    if(s2 < s) {
      s = s2;
      rows_first = 0;
    }
  }

  U32 * buffer = new U32[s]; // intermediate buffer
  U64 scale = ((U64)(get_resample_scale(w1, w2))) * ((U64)(get_resample_scale(h1, h2)));
  if(rows_first) {
    resample(w1, w2, source + n_ch, buffer,
             pixel_stride, 1, h1, row_stride, w2, 0);
    resample_scale(h1, h2, buffer, target + n_ch,
                   w2, target_row_stride, w2, 1, target_pixel_stride,
                   scale, 0);
    for(int ch = 0; ch < n_ch; ch++) {
      resample(w1, w2, source + ch, buffer,
               pixel_stride, 1, h1, row_stride, w2,
               alph_shift_multiplier * (n_ch - ch));
      resample_scale(h1, h2, buffer, target + ch,
                     w2, target_row_stride, w2, 1, target_pixel_stride,
                     scale, alph_shift_multiplier * (n_ch - ch));
    }
  } else {
    resample(h1, h2, source + n_ch, buffer,
             row_stride, w1, w1, pixel_stride, 1, 0);
    resample_scale(w1, w2, buffer, target + n_ch,
                   1, target_pixel_stride, h2, w1, target_row_stride,
                   scale, 0);
    for(int ch = 0; ch < n_ch; ch++) {
      resample(h1, h2, source + ch, buffer,
               row_stride, w1, w1, pixel_stride, 1,
               alph_shift_multiplier * (n_ch - ch));
      resample_scale(w1, w2, buffer, target + ch,
                     1, target_pixel_stride, h2, w1, target_row_stride,
                     scale, alph_shift_multiplier * (n_ch - ch));
    }
  }
  delete[] buffer;
  return target;
}

}  // end of anonymous namespace

#endif //!(FLTK_HAS_IMAGE_SCALING)

// This is a simple function to replace Fl_RGB_Image::copy().
// Note a clumsy hack to detect Fl_RGB_IMAGE (or subclass) using count() and d() methods.
// RTTI users can modify this using dynamic_cast< >() function.
// If it is Fl_Bitmap or Fl_Pixmap, original resampling methods are used.

Fl_Image * fl_copy_image(Fl_Image * im, int w2, int h2)
{
  if(!im) return 0;
  if(w2 <= 0 || h2 <= 0) return im->copy();
  if(im->w() == w2 && im->h() == h2) // direct copy
    return im->copy();
#if FLTK_HAS_NEW_FUNCTIONS
  if(Fl_Image::RGB_scaling() != FL_RGB_SCALING_BILINEAR)
    return im->copy(w2, h2);
#endif
  if(im->count() != 1 || !im->d()) // bitmap or pixmap
    return im->copy(w2, h2);
#if FLTK_HAS_IMAGE_SCALING
#pragma message( "FLTK_HAS_IMAGE_SCALING" )
  Fl_Image *ni = im->copy();
  ni->scale(w2, h2, 0, 1); // let the hardware do the scaling!
#else
  return im->copy(w2, h2);
  int ld = im->ld();
  int w1 = im->w();
  int pixel_stride = im->d();
  int row_stride = (pixel_stride * w1) + ld;
  const unsigned char * data = (const unsigned char *)(*(im->data()));
  unsigned char * array = resample(w2, h2, data, w1, im->h(), pixel_stride, pixel_stride, row_stride, 0, 0, 0, (im->d() == 4));
  Fl_RGB_Image * ni = new Fl_RGB_Image(array, w2, h2, pixel_stride);
  ni->alloc_array = 1;
#endif
  return ni;
}
