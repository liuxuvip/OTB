/*
 * Copyright (C) 2005-2020 Centre National d'Etudes Spatiales (CNES)
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

#ifndef otbMorphologicalOpeningProfileFilter_h
#define otbMorphologicalOpeningProfileFilter_h

#include "otbImageToProfileFilter.h"
#include "itkOpeningByReconstructionImageFilter.h"

namespace otb
{
/** \class MorphologicalOpeningProfileFilter
 *  \brief This filter compute the morphological opening profile.
 *
 * This algorithm is based on the following publication:
 * \par
 * Martino Pesaresi and Jon Alti Benediktsson, Member, IEEE:
 * A new approach for the morphological segmentation of high resolution
 * satellite imagery. IEEE Transactions on geoscience and remote sensing, vol. 39,
 * NO. 2, February 2001, p. 309-320.
 * \par
 *
 * The opening profile is a set of images being the result of a geodesic morphological
 * opening by reconstruction with an increasing range of structuring element sizes.
 *
 * For more information on profiles please refer to the documentation of the otb::ImageToProfileFilter
 * class.
 *
 * \sa ImageToProfileFilter
 * \sa itk::OpeningByReconstructionImageFilter
 *
 * \ingroup OTBMorphologicalProfiles
 */
template <class TInputImage, class TOutputImage, class TStructuringElement>
class ITK_EXPORT MorphologicalOpeningProfileFilter
    : public ImageToProfileFilter<TInputImage, TOutputImage, itk::OpeningByReconstructionImageFilter<TInputImage, TOutputImage, TStructuringElement>,
                                  unsigned int>
{
public:
  /** Standard typedefs */
  typedef MorphologicalOpeningProfileFilter Self;
  typedef ImageToProfileFilter<TInputImage, TOutputImage, itk::OpeningByReconstructionImageFilter<TInputImage, TOutputImage, TStructuringElement>, unsigned int>
                                        Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  /** Type macro */
  itkNewMacro(Self);

  /** Creation through object factory macro */
  itkTypeMacro(MorphologicalOpeningProfileFilter, ImageToProfileFilter);

  typedef TStructuringElement                StructuringElementType;
  typedef typename Superclass::ParameterType ParameterType;

protected:
  /** Set the profile parameter */
  void SetProfileParameter(ParameterType param) override
  {
    StructuringElementType se;
    se.SetRadius(param);
    se.CreateStructuringElement();
    this->GetFilter()->SetKernel(se);
  }

  /** Constructor */
  MorphologicalOpeningProfileFilter(){};

  /** Destructor */
  ~MorphologicalOpeningProfileFilter() override = default;

  /**PrintSelf method */
  void PrintSelf(std::ostream& os, itk::Indent indent) const override
  {
    Superclass::PrintSelf(os, indent);
  }

private:
  MorphologicalOpeningProfileFilter(const Self&) = delete;
  void operator=(const Self&) = delete;
};
} // End namespace otb
#endif
