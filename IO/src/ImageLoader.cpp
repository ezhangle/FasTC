/* FasTC
 * Copyright (c) 2012 University of North Carolina at Chapel Hill. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its documentation for educational, 
 * research, and non-profit purposes, without fee, and without a written agreement is hereby granted, 
 * provided that the above copyright notice, this paragraph, and the following four paragraphs appear 
 * in all copies.
 *
 * Permission to incorporate this software into commercial products may be obtained by contacting the 
 * authors or the Office of Technology Development at the University of North Carolina at Chapel Hill <otd@unc.edu>.
 *
 * This software program and documentation are copyrighted by the University of North Carolina at Chapel Hill. 
 * The software program and documentation are supplied "as is," without any accompanying services from the 
 * University of North Carolina at Chapel Hill or the authors. The University of North Carolina at Chapel Hill 
 * and the authors do not warrant that the operation of the program will be uninterrupted or error-free. The 
 * end-user understands that the program was developed for research purposes and is advised not to rely 
 * exclusively on the program for any reason.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL OR THE AUTHORS BE LIABLE TO ANY PARTY FOR 
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING OUT OF THE 
 * USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL OR THE 
 * AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL AND THE AUTHORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, 
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND ANY 
 * STATUTORY WARRANTY OF NON-INFRINGEMENT. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY 
 * OF NORTH CAROLINA AT CHAPEL HILL AND THE AUTHORS HAVE NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, 
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Please send all BUG REPORTS to <pavel@cs.unc.edu>.
 *
 * The authors may be contacted via:
 *
 * Pavel Krajcevski
 * Dept of Computer Science
 * 201 S Columbia St
 * Frederick P. Brooks, Jr. Computer Science Bldg
 * Chapel Hill, NC 27599-3175
 * USA
 * 
 * <http://gamma.cs.unc.edu/FasTC/>
 */

#include "ImageLoader.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////
//
// Static helper functions
//
///////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline T min(const T &a, const T &b) {
  return (a > b)? b : a;
}

template <typename T>
static inline T abs(const T &a) {
  return (a > 0)? a : -a;
}

template <typename T>
static inline T sad(const T &a, const T &b) {
  return (a > b)? a - b : b - a;
}

void ReportError(const char *str) {
  fprintf(stderr, "ImageLoader.cpp -- ERROR: %s\n", str);
}

unsigned int ImageLoader::GetChannelForPixel(uint32 x, uint32 y, uint32 ch) {
  uint32 prec;
  const uint8 *data;

  switch(ch) {
  case 0:
    prec = GetRedChannelPrecision();
    data = GetRedPixelData();
    break;

  case 1:
    prec = GetGreenChannelPrecision();
    data = GetGreenPixelData();
    break;

  case 2:
    prec = GetBlueChannelPrecision();
    data = GetBluePixelData();
    break;

  case 3:
    prec = GetAlphaChannelPrecision();
    data = GetAlphaPixelData();
    break;

  default:
    ReportError("Unspecified channel");
    return INT_MAX;
  }

  if(0 == prec)
    return 0;

  assert(x < GetWidth());
  assert(y < GetHeight());

  uint32 pixelIdx = y * GetWidth() + x;
  const uint32 val = data[pixelIdx];
  
  if(prec < 8) {
    int32 ret = 0;
    for(uint32 precLeft = 8; precLeft > 0; precLeft -= min(prec, sad(prec, precLeft))) {
      
      if(prec > precLeft) {
        const int toShift = prec - precLeft;
        ret = ret << precLeft;
        ret |= val >> toShift;
      }
      else {
        ret = ret << prec;
        ret |= val;
      }
    }

    return ret;
  }
  else if(prec > 8) {
    const int32 toShift = prec - 8;
    return val >> toShift;
  }

  return val;
}

bool ImageLoader::LoadFromPixelBuffer(uint32 *data) {
  m_RedChannelPrecision = 8;
  m_GreenChannelPrecision = 8;
  m_BlueChannelPrecision = 8;
  m_AlphaChannelPrecision = 8;

  const int nPixels = m_Width * m_Height;
  m_RedData = new uint8[nPixels];
  m_GreenData = new uint8[nPixels];
  m_BlueData = new uint8[nPixels];
  m_AlphaData = new uint8[nPixels];

  for (uint32 i = 0; i < m_Width; i++) {
    for (uint32 j = 0; j < m_Height; j++) {
      uint32 idx = j*m_Height + i;
      uint32 pixel = data[idx];
      m_RedData[idx] = pixel & 0xFF;
      m_GreenData[idx] = (pixel >> 8) & 0xFF;
      m_BlueData[idx] = (pixel >> 16) & 0xFF;
      m_AlphaData[idx] = (pixel >> 24) & 0xFF;
    }
  }

  return true;
}

bool ImageLoader::LoadImage() {

  // Do we already have pixel data?
  if(m_PixelData)
    return true;

  // Read the image data!
  if(!ReadData())
    return false;

  m_Width = GetWidth();
  m_Height = GetHeight();

  // Create RGBA buffer 
  const unsigned int dataSz = 4 * m_Width * m_Height;

  m_PixelData = new unsigned char[dataSz];

  // Populate buffer in block stream order... make 
  // sure that width and height are aligned to multiples of four.
  const unsigned int aw = ((m_Width + 3) >> 2) << 2;
  const unsigned int ah = ((m_Height + 3) >> 2) << 2;

#ifndef NDEBUG
  if(aw != m_Width || ah != m_Height)
    fprintf(stderr, "Warning: Image dimension not multiple of four. "
                    "Space will be filled with black.\n");
#endif

  int byteIdx = 0;
  for(uint32 j = 0; j < ah; j++) {
    for(uint32 i = 0; i < aw; i++) {

      unsigned int redVal = GetChannelForPixel(i, j, 0);
      if(redVal == INT_MAX)
        return false;

      unsigned int greenVal = redVal;
      unsigned int blueVal = redVal;

      if(GetGreenChannelPrecision() > 0) {
        greenVal = GetChannelForPixel(i, j, 1);
        if(greenVal == INT_MAX)
          return false;
      }

      if(GetBlueChannelPrecision() > 0) {
        blueVal = GetChannelForPixel(i, j, 2);
        if(blueVal == INT_MAX)
          return false;
      }

      unsigned int alphaVal = 0xFF;
      if(GetAlphaChannelPrecision() > 0) {
        alphaVal = GetChannelForPixel(i, j, 3);
        if(alphaVal == INT_MAX)
          return false;
      }

      // Red channel
      m_PixelData[byteIdx++] = redVal & 0xFF;

      // Green channel
      m_PixelData[byteIdx++] = greenVal & 0xFF;

      // Blue channel
      m_PixelData[byteIdx++] = blueVal & 0xFF;

      // Alpha channel
      m_PixelData[byteIdx++] = alphaVal & 0xFF;
    }
  }

  return true;
}
