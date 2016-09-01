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
#ifndef otbStreamingManager_h
#define otbStreamingManager_h

#include "otbMacro.h"

#include "itkDataObject.h"
#include "itkImageRegionSplitterBase.h"
#include "otbPipelineMemoryPrintCalculator.h"

namespace otb
{

/** \class StreamingManager
 *  \brief This class handles the streaming process used in the writers implementation
 *
 *  The streaming mode can be chosen with either SetStrippedRAMStreamingMode, SetStrippedNumberOfLinesStreamingMode,
 *  SetTiledRAMStreamingMode, or SetTiledTileDimensionStreamingMode.
 *
 *  Then, PrepareStreaming must be called so that the stream type and dimensions are computed
 *  This involves passing the actual DataObject who will be written, since it will be used
 *  during memory estimation for some specific streaming modes.
 *
 *  After PrepareStreaming has been called, the actual number of splits and streaming mode which will be used
 *  can be retrieved with GetStreamingMode and GetNumberOfSplits.
 *  The different splits can be retrieved with GetSplit
 *
 * \sa ImageFileWriter
 * \sa StreamingImageVirtualFileWriter
 *
 * \ingroup OTBStreaming
 */
template<class TImage>
class ITK_EXPORT StreamingManager : public itk::Object
{
public:
  /** Standard class typedefs. */
  typedef StreamingManager              Self;
  typedef itk::LightObject              Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  typedef TImage                                ImageType;
  typedef typename ImageType::Pointer           ImagePointerType;
  typedef typename ImageType::RegionType        RegionType;
  typedef typename RegionType::IndexType        IndexType;
  typedef typename RegionType::SizeType         SizeType;
  typedef typename ImageType::InternalPixelType PixelType;

  typedef otb::PipelineMemoryPrintCalculator::MemoryPrintType MemoryPrintType;

  /** Type macro */
  itkTypeMacro(StreamingManager, itk::LightObject);

  /** Dimension of input image. */
  itkStaticConstMacro(ImageDimension, unsigned int, ImageType::ImageDimension);

  /** Actually computes the stream divisions, according to the specified streaming mode,
   * eventually using the input parameter to estimate memory consumption */
  virtual void PrepareStreaming(itk::DataObject * input, const RegionType &region) = 0;

  /** Returns the actual number of pieces that will be used to process the image.
   * PrepareStreaming() must have been called before.
   * This can be different than the requested number */
  virtual unsigned int GetNumberOfSplits();

  /** Get a region definition that represents the ith piece a specified region.
   * The "numberOfPieces" must be equal to what
   * GetNumberOfSplits() returns. */
  virtual RegionType GetSplit(unsigned int i);

protected:
  StreamingManager();
  ~StreamingManager() ITK_OVERRIDE;

  virtual unsigned int EstimateOptimalNumberOfDivisions(itk::DataObject * input, const RegionType &region,
                                                        MemoryPrintType availableRAMInMB,
                                                        double bias = 1.0);

  /** The number of splits generated by the splitter */
  unsigned int m_ComputedNumberOfSplits;

  /** The region to stream */
  RegionType m_Region;

  /** The splitter used to compute the different strips */
  typedef itk::ImageRegionSplitterBase           AbstractSplitterType;
  typedef typename AbstractSplitterType::Pointer AbstractSplitterPointerType;
  AbstractSplitterPointerType m_Splitter;

private:
  StreamingManager(const StreamingManager &); //purposely not implemented
  void operator =(const StreamingManager&);   //purposely not implemented

  /* Compute the available RAM from configuration settings if the input parameter is 0,
   * otherwise, simply returns the input parameter */
  MemoryPrintType GetActualAvailableRAMInBytes(MemoryPrintType availableRAMInMB);


};

} // End namespace otb

#ifndef OTB_MANUAL_INSTANTIATION
#include "otbStreamingManager.txx"
#endif

#endif

