/*=========================================================================

Program:   ORFEO Toolbox
Language:  C++
Date:      $Date$
Version:   $Revision$


Copyright (c) Centre National d'Etudes Spatiales. All rights reserved.
See OTBCopyright.txt for details.


This software is distributed WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef _otbImportGeoInformationImageFilter_txx
#define _otbImportGeoInformationImageFilter_txx

#include "itkDataObject.h"
#include "otbImportGeoInformationImageFilter.h"
#include "itkMetaDataObject.h"
#include "itkMetaDataDictionary.h"
#include "otbMetaDataKey.h"

namespace otb
{
/**
 * Constructor
 */
template <class TImage, class TSourceImage>
ImportGeoInformationImageFilter<TImage,TSourceImage>
::ImportGeoInformationImageFilter()
{
  this->SetNumberOfRequiredInputs(2);
  this->SetNthInput(1,SourceImageType::New().GetPointer());
  this->SetInPlace(true);
}

template <class TImage, class TSourceImage>
void
ImportGeoInformationImageFilter<TImage,TSourceImage>
::SetSource(const TSourceImage * source)
{
  this->SetNthInput(1,const_cast<TSourceImage *>(source));
}
template <class TImage, class TSourceImage>
const TSourceImage *
ImportGeoInformationImageFilter<TImage,TSourceImage>
::GetSource()
{
  return static_cast<const TSourceImage *>(this->itk::ProcessObject::GetInput(1));
}

template <class TImage, class TSourceImage>
void
 ImportGeoInformationImageFilter<TImage,TSourceImage>
::GenerateInputRequestedRegion(void)
{
  Superclass::GenerateInputRequestedRegion();

  typename SourceImageType::RegionType region;
  typename SourceImageType::SizeType size;
  typename SourceImageType::IndexType index;

  size.Fill(0);
  index.Fill(0);
  region.SetSize(size);
  region.SetIndex(index);

  SourceImageType * sourcePtr = const_cast<SourceImageType *>(this->GetSource());
  sourcePtr->SetRequestedRegion(region);
}

template <class TImage, class TSourceImage>
void
 ImportGeoInformationImageFilter<TImage,TSourceImage>
::GenerateOutputInformation(void)
{
  Superclass::GenerateOutputInformation();
  // Get output and source pointer
  ImagePointerType outputPtr = this->GetOutput();
  SourceImageType * sourcePtr =const_cast<SourceImageType *>(this->GetSource());
  // Import metdata
  outputPtr->CopyInformation(sourcePtr);
}
/**
 * Main computation method.
 */
template <class TImage, class TSourceImage>
void
 ImportGeoInformationImageFilter<TImage,TSourceImage>
::GenerateData(void)
{
  // Generate data is empty, since the filter runs inplace.
}
/**
 * PrintSelf Method
 */
template <class TImage, class TSourceImage>
void
ImportGeoInformationImageFilter<TImage,TSourceImage>
::PrintSelf(std::ostream& os, itk::Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}
} // End namespace otb
#endif
