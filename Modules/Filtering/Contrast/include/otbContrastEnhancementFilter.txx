/*
 * Copyright (C) 2005-2017 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef otbContrastEnhancementFilter_txx
#define otbContrastEnhancementFilter_txx

#include "otbContrastEnhancementFilter.h"

#include <limits>

#include "itkThresholdImageFilter.h"

namespace otb
{
typedef itk::Image< float , 2 > ImageGainType;

template <class TInputImage, class TOutputImage , int Tsize >
ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::ContrastEnhancementFilter()
{
	targetHisto.fill(0);
	gainImage = ImageGainType::New();
	gainMultiplyer = MultiplyImageFilterType::New() ;
	wThumbnail = 0;
	hThumbnail = 0;
	threshFactor = INT_MAX;
	lowThresh = 0;
	upThresh = INT_MAX;
}

template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::equalized( const std::array< int , Tsize > & inputHisto,
	           std::array< int , Tsize > & lut)
{
	int countMapValue = 0;
  int countValue = 0;
  lut[countValue] = 0; // Black stays black
  ++countValue;
  int countInput  = inputHisto[ 0 ] + inputHisto[ countValue ];
  lut[lut.size() - 1 ] = lut.size() - 1 ; // White stays white
  int countTarget = this->targetHisto[ countMapValue ];
  while ( countMapValue<Tsize && countValue<Tsize-1)
    {
    if (countInput > countTarget)
      {
      ++countMapValue;
      countTarget += this->targetHisto[countMapValue];
      }
    else
      {	
      lut[countValue] =  countMapValue ;
      ++countValue;
      countInput  += inputHisto[ countValue ];
      }
    }
  for (int i = 0 ; i<lut.size() ; i++)
  	{
  	if (lut[i] == -1)
  		{
  		lut[i] = lut[i-1];	
  		}	
  	}
}

template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::equalized( std::array< int , Tsize > gridHisto[] , 
						 std::array< int , Tsize > gridLut[] ,
						 int nW , 
						 int nH )
{
  for (int i = 0 ; i< nW ; i++)
    {
    for ( int j = 0 ; j<nH ; j++ )
      {
      equalized( gridHisto[i + j * nW ] , gridLut[i+ j * nW ]);
      }
    }
}


template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::computehisto( typename TInputImage::ConstPointer const input ,
								std::array< int , Tsize > gridHisto[] ,
								int nW,
								int nH)
{
  typename TInputImage::IndexType index;
  // int nbBin = this->inputHisto[0].size();
  for (int i = 0 ; i < this->wThumbnail * nW ; i++)
    {
    for (int j = 0 ; j< this->hThumbnail * nH ; j++)
      {
      index[0] = i;
      index[1] = j;
      ++gridHisto[ (i / this->wThumbnail)  + ( j / this->hThumbnail ) * nW][ input->GetPixel( index ) ];
      }
    }
}


template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::createTarget(typename TInputImage::ConstPointer const input )
{
  typename TInputImage::SizeType size = input->GetLargestPossibleRegion().GetSize();
  int nbPixel = size[0] * size[1];
  int height = nbPixel/Tsize ;
  this->targetHisto.fill( height );
}

template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::createTarget( int h , int l )
{
  int nbPixel = h * l;
  int nbBin = this->targetHisto.size();
  int height = nbPixel/nbBin;
  this->targetHisto.fill( height );
}

template <class TInputImage, class TOutputImage , int Tsize >
float ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::interpoleGain( const std::array< int , Tsize > gridLut[] ,
								 int pixelValue ,
                 typename TInputImage::IndexType index ,
                 int nW ,
                 int nH )
{
  int lutX = index[0]/this->wThumbnail;
  int lutY = index[1]/this->hThumbnail;
  float x = static_cast< float >(index[0]%this->wThumbnail) / static_cast< float >(this->wThumbnail);
  float y = static_cast< float >(index[1]%this->hThumbnail) / static_cast< float >(this->hThumbnail);
  float w = (1 - std::abs(x - 0.5) )*(1 - std::abs(y - 0.5) );
  float gain = gridLut[lutX + lutY * nW ][ pixelValue ] * (1 - std::abs(x - 0.5) )*(1 - std::abs(y - 0.5) ) ;
  bool right = x>=0.5 && lutX<(nW - 1) ;
  bool up = y<=0.5 && lutY>0;
  bool left = x<=0.5 && lutX>0;
  bool down = y>=0.5 && lutY<(nH - 1);
  if ( right )
    {
    gain += gridLut[lutX + 1 + lutY * nW ][ pixelValue ] * (1 - std::abs(y - 0.5) ) * (x - 0.5);
    w += (1 - std::abs(y - 0.5) ) * (x - 0.5);
    }
  if ( left )
    {
    gain += gridLut[lutX + lutY * nW - 1 ][ pixelValue ] * (1 - std::abs(y - 0.5) ) * std::abs(x - 0.5);
    w += (1 - std::abs(y - 0.5) ) * std::abs(x - 0.5);
    }
  if ( up )
    {
    gain += gridLut[lutX + (lutY - 1) * nW ][ pixelValue ] * std::abs(y - 0.5) * (1 - std::abs(x - 0.5) );
    w += std::abs(y - 0.5) * (1 - std::abs(x - 0.5) );
    }
  if ( down )
    {
    gain += gridLut[lutX + (lutY + 1)* nW ][ pixelValue ]  * (y - 0.5) * (1 - std::abs(x - 0.5) );
    w += (y - 0.5) * (1 - std::abs(x - 0.5) );
    }
  if ( up && left )
    {
    gain += gridLut[( lutX - 1 ) + (lutY - 1 ) * nW ][ pixelValue ] * std::abs(y - 0.5) * std::abs(x - 0.5);
    w += std::abs(y - 0.5) * std::abs(x - 0.5);
    }
  if ( down && left )
    {
    gain += gridLut[( lutX - 1 ) + (lutY + 1) * nW ][ pixelValue ] * std::abs(y - 0.5) * std::abs(x - 0.5);
    w += std::abs(y - 0.5) * std::abs(x - 0.5);
    }
  if ( up && right )
    {
    gain += gridLut[( lutX + 1 ) + (lutY - 1) * nW ][ pixelValue ] * std::abs(y - 0.5) * std::abs(x - 0.5);
    w += std::abs(y - 0.5) * std::abs(x - 0.5) ;
    }
  if ( down && right )
    {
    gain += gridLut[( lutX + 1 ) + (lutY + 1 ) * nW ][ pixelValue ] * std::abs(y - 0.5) * std::abs(x - 0.5);
    w += std::abs(y - 0.5) * std::abs(x - 0.5);
    }

  return gain/(w * pixelValue);
}

template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::histoLimiteContrast( std::array< int , Tsize > gridHisto[] ,
											 int hThresh,
											 int nW,
											 int nH )
{
  int histoLength = gridHisto[0].size();
  int nbHisto = nW * nH;
  int toThresh = 0;
  int npx = 0;
  int rest = 0;
  for ( int i = 0 ; i < nbHisto ; i++)
    {
    toThresh = 0;
    for (int j = 0 ; j < histoLength ; j++)
      {
      if ( gridHisto[i][j] > hThresh )
        {
          toThresh+= gridHisto[i][j] - hThresh;
          gridHisto[i][j] = hThresh ;
        }
      }
    if (toThresh != 0 )
      {
      npx = toThresh / histoLength;
      rest = toThresh%histoLength;
      for (int j = 0 ; j < histoLength ; j++)
       {
        gridHisto[i][j] += npx ;
        if (rest>0)
          {
          ++gridHisto[i][j];
          --rest;
          }
        }
      }
    }
}


template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::gainLimiteContrast()
{
  typedef itk::ThresholdImageFilter< ImageGainType > ThresholdFilterType;
  ThresholdFilterType::Pointer thresholdFilter ( ThresholdFilterType::New() );
  thresholdFilter->SetInput( this->gainImage );
  thresholdFilter->SetOutsideValue (this->upThresh);
  thresholdFilter->ThresholdAbove( this->upThresh );
  thresholdFilter->Update();
  thresholdFilter->SetInput( thresholdFilter->GetOutput() );
  thresholdFilter->SetOutsideValue (this->lowThresh);
  thresholdFilter->ThresholdBelow( this->lowThresh );
  thresholdFilter->Update();
  this->gainImage = thresholdFilter->GetOutput();
}

template <class TInputImage, class TOutputImage , int Tsize >
void ContrastEnhancementFilter < TInputImage , TOutputImage , Tsize >
::GenerateData()
{
	typename OutputImageType::Pointer     output = this->GetOutput();
  typename InputImageType::ConstPointer input = this->GetInput();

  // output->SetRegions( input->GetLargestPossibleRegion() );
  // output->Allocate();
  // output->SetOrigin( input->GetOrigin() );
  // output->SetSpacing( input->GetSpacing() );
  // typename InputImageType::PixelType Val(1);
	// Val[0];
	// output->FillBuffer(Val);
  createTarget(input);

  if ( input->GetLargestPossibleRegion().GetSize()[1]%this->hThumbnail != 0 )
    {
    std::cout<<"error : hThumbnail = "<<this->hThumbnail<<" is not a divider of the input's width"<<std::endl;
    std::cout<<"Image Height = "<<input->GetLargestPossibleRegion().GetSize()[1]<<std::endl;
    }
  if ( input->GetLargestPossibleRegion().GetSize()[0]%this->wThumbnail != 0 )
    {
    std::cout<<"error : wThumbnail = "<<this->wThumbnail<<"is not a divider of the input's height"<<std::endl;
    std::cout<<"Image Width = "<<input->GetLargestPossibleRegion().GetSize()[0]<<std::endl;
    }

  int nW = input->GetLargestPossibleRegion().GetSize()[0]/this->wThumbnail;
  int nH = input->GetLargestPossibleRegion().GetSize()[1]/this->hThumbnail;
  
  gainImage->SetRegions( input->GetRequestedRegion() );
  gainImage->Allocate();
  gainImage->SetOrigin( input->GetOrigin() );

  std::array< int , Tsize > gridHisto[(nH)*(nW)] = {} ;
 	std::array< int , Tsize > gridLut[(nH)*(nW)] = {} ;



  // Initialize gridLut and gridHisto to zero
  for (int i = 0 ; i<(nH)*(nW) ; i++)
    {
      gridLut[i].fill(-1);
      gridHisto[i].fill(0);
    }

  computehisto( input , gridHisto , nW , nH );

  int histoTresh = this->threshFactor * (this->wThumbnail * this->hThumbnail ) / Tsize;
	histoLimiteContrast( gridHisto , histoTresh , nW , nH );

	equalized( gridHisto , gridLut , nW , nH );
	
	float gainValue = 0.0;
  typename InputImageType::IndexType index;
  for (uint i = 0 ; i < input->GetLargestPossibleRegion().GetSize()[0] ; i++)
    {
    for (uint j = 0 ; j < input->GetLargestPossibleRegion().GetSize()[1] ; j++)
      {
      index[0] = i;
      index[1] = j;
      gainValue = interpoleGain( gridLut , input->GetPixel( index ) , index , nW , nH );
      gainImage->SetPixel( index , gainValue );
      }
    }


  gainLimiteContrast();

  gainMultiplyer->SetInput1( input );
  gainMultiplyer->SetInput2( gainImage );
  gainMultiplyer->Update();
  this-> GraftOutput ( gainMultiplyer -> GetOutput ());
}


}

#endif