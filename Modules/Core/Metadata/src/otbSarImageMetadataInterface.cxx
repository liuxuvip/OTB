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


#include "otbSarImageMetadataInterface.h"

#include "otbMath.h"
#include "itkMetaDataObject.h"
#include "otbImageKeywordlist.h"
#include "otbSARMetadata.h"

namespace otb
{

SarImageMetadataInterface::SarImageMetadataInterface()
{
}

const std::string SarImageMetadataInterface::GetProductType() const
{
  const MetaDataDictionaryType& dict = this->GetMetaDataDictionary();
  if (!this->CanRead())
  {
    itkExceptionMacro(<< "Invalid Metadata");
  }

  ImageKeywordlistType imageKeywordlist;

  if (dict.HasKey(MetaDataKey::OSSIMKeywordlistKey))
  {
    itk::ExposeMetaData<ImageKeywordlistType>(dict, MetaDataKey::OSSIMKeywordlistKey, imageKeywordlist);
  }

  if (imageKeywordlist.HasKey("support_data.product_type"))
  {
    const std::string product_type = imageKeywordlist.GetMetadataByKey("support_data.product_type");
    return product_type;
  }
  return "";
}

const std::string SarImageMetadataInterface::GetAcquisitionMode() const
{
  const MetaDataDictionaryType& dict = this->GetMetaDataDictionary();
  if (!this->CanRead())
  {
    itkExceptionMacro(<< "Invalid Metadata");
  }

  ImageKeywordlistType imageKeywordlist;

  if (dict.HasKey(MetaDataKey::OSSIMKeywordlistKey))
  {
    itk::ExposeMetaData<ImageKeywordlistType>(dict, MetaDataKey::OSSIMKeywordlistKey, imageKeywordlist);
  }

  if (imageKeywordlist.HasKey("support_data.acquisition_mode"))
  {
    const std::string acquisition_mode = imageKeywordlist.GetMetadataByKey("support_data.acquisition_mode");
    return acquisition_mode;
  }
  return "";
}

bool SarImageMetadataInterface::CreateCalibrationLookupData(SARCalib& sarCalib, const ImageMetadata&, const MetadataSupplierInterface &mds, const bool) const
{
  sarCalib.calibrationLookupFlag = HasCalibrationLookupDataFlag(mds);
  if (!sarCalib.calibrationLookupFlag)
  {
    sarCalib.calibrationLookupData[SarCalibrationLookupData::SIGMA] = SarCalibrationLookupData::New();
    sarCalib.calibrationLookupData[SarCalibrationLookupData::BETA] = SarCalibrationLookupData::New();
    sarCalib.calibrationLookupData[SarCalibrationLookupData::GAMMA] = SarCalibrationLookupData::New();
    sarCalib.calibrationLookupData[SarCalibrationLookupData::DN] = SarCalibrationLookupData::New();
    return true;
  }
  return false;
}

bool SarImageMetadataInterface::HasCalibrationLookupDataFlag(const MetadataSupplierInterface&) const
{
  return false;
}

SarImageMetadataInterface::RealType SarImageMetadataInterface::GetRadiometricCalibrationScale() const
{
  return static_cast<SarImageMetadataInterface::RealType>(1.0);
}

SarImageMetadataInterface::PointSetPointer SarImageMetadataInterface::GetConstantValuePointSet(const RealType& value) const
{
  PointSetPointer pointSet = PointSetType::New();

  PointType p0;

  pointSet->Initialize();

  p0[0] = static_cast<unsigned int>(0);
  p0[1] = static_cast<unsigned int>(0);
  pointSet->SetPoint(0, p0);
  pointSet->SetPointData(0, value);

  return pointSet;
}

SarImageMetadataInterface::PointSetPointer
SarImageMetadataInterface::GetRadiometricCalibrationNoise(const MetadataSupplierInterface &, const ImageMetadata &, const std::string&) const
{
  return SarImageMetadataInterface::GetConstantValuePointSet(0.0);
}

SarImageMetadataInterface::PointSetPointer SarImageMetadataInterface::GetRadiometricCalibrationAntennaPatternNewGain() const
{
  return SarImageMetadataInterface::GetConstantValuePointSet(1.0);
}


SarImageMetadataInterface::PointSetPointer SarImageMetadataInterface::GetRadiometricCalibrationAntennaPatternOldGain() const
{
  return SarImageMetadataInterface::GetConstantValuePointSet(1.0);
}


SarImageMetadataInterface::PointSetPointer
SarImageMetadataInterface::GetRadiometricCalibrationIncidenceAngle(const MetadataSupplierInterface&) const
{
  return SarImageMetadataInterface::GetConstantValuePointSet(CONST_PI_2);
}


SarImageMetadataInterface::PointSetPointer SarImageMetadataInterface::GetRadiometricCalibrationRangeSpreadLoss() const
{
  return SarImageMetadataInterface::GetConstantValuePointSet(1.0);
}


SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetConstantPolynomialDegree() const
{
  return {0, 0};
}

double SarImageMetadataInterface::GetRescalingFactor() const
{
  return 1.0;
}
SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetRadiometricCalibrationNoisePolynomialDegree() const
{
  return SarImageMetadataInterface::GetConstantPolynomialDegree();
}

SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetRadiometricCalibrationAntennaPatternNewGainPolynomialDegree() const
{
  return SarImageMetadataInterface::GetConstantPolynomialDegree();
}

SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetRadiometricCalibrationAntennaPatternOldGainPolynomialDegree() const
{
  return SarImageMetadataInterface::GetConstantPolynomialDegree();
}

SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetRadiometricCalibrationIncidenceAnglePolynomialDegree() const
{
  return SarImageMetadataInterface::GetConstantPolynomialDegree();
}

SarImageMetadataInterface::ArrayIndexType SarImageMetadataInterface::GetRadiometricCalibrationRangeSpreadLossPolynomialDegree() const
{
  return SarImageMetadataInterface::GetConstantPolynomialDegree();
}

std::vector<AzimuthFmRate> SarImageMetadataInterface::GetAzimuthFmRateGeom() const
{
  std::vector<AzimuthFmRate> azimuthFmRateVector;
  // Number of entries in the vector
  int listCount = m_MetadataSupplierInterface->GetAs<int>(0, "azimuthFmRate.azi_fm_rate_coef_nb_list");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "azimuthFmRate.azi_fm_rate_coef_list" + oss.str();
    AzimuthFmRate afr;
    std::istringstream(m_MetadataSupplierInterface->GetAs<std::string>(path_root + ".azi_fm_rate_coef_time")) >> afr.azimuthTime;
    afr.t0 = m_MetadataSupplierInterface->GetAs<double>(path_root + ".slant_range_time");
    std::vector<double> polynom(3);
    for (int polyId = 1 ; polyId < 4 ; ++polyId)
      polynom.push_back(m_MetadataSupplierInterface->GetAs<double>(path_root+"."+std::to_string(polyId)+".azi_fm_rate_coef"));
    afr.azimuthFmRatePolynomial = std::move(polynom);
    azimuthFmRateVector.push_back(std::move(afr));
  }
  return azimuthFmRateVector;
}

std::vector<DopplerCentroid> SarImageMetadataInterface::GetDopplerCentroidGeom() const
{
  std::vector<DopplerCentroid> dopplerCentroidVector;
  // Path: dopplerCentroid.dop_coef_list<listId>.{dop_coef_time,slant_range_time,{1,2,3}.dop_coef}
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ;
       m_MetadataSupplierInterface->GetAs<std::string>("", std::string("dopplerCentroid.dop_coef_list")+std::to_string(listId)+std::string(".slant_range_time")) != "" ;
       ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "dopplerCentroid.dop_coef_list" + oss.str();
    DopplerCentroid dopplerCent;
    std::istringstream(m_MetadataSupplierInterface->GetAs<std::string>(path_root + ".dop_coef_time")) >> dopplerCent.azimuthTime;
    dopplerCent.t0 = m_MetadataSupplierInterface->GetAs<double>(path_root + ".slant_range_time");
    dopplerCentroidVector.push_back(std::move(dopplerCent));
  }
  return dopplerCentroidVector;
}

std::vector<Orbit> SarImageMetadataInterface::GetOrbitsGeom() const
{
  std::vector<Orbit> orbitVector;
  // Number of entries in the vector
  int listCount = m_MetadataSupplierInterface->GetAs<int>("orbitList.nb_orbits");
  // This streams will hold the iteration number
  std::ostringstream oss;

  for (int listId = 0 ; listId <= listCount - 1 ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "orbitList.orbit[" + oss.str() + "]";
    Orbit orbit;

    orbit.time = MetaData::ReadFormattedDate(m_MetadataSupplierInterface->GetAs<std::string>(path_root + ".time"));

    orbit.position[0] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".x_pos");
    orbit.position[1] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".y_pos");
    orbit.position[2] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".z_pos");
    orbit.velocity[0] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".x_vel");
    orbit.velocity[1] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".y_vel");
    orbit.velocity[2] = m_MetadataSupplierInterface->GetAs<double>(path_root + ".z_vel");
    orbitVector.push_back(std::move(orbit));
  }
  return orbitVector;
}


std::vector<BurstRecord> SarImageMetadataInterface::GetBurstRecordsGeom() const
{
  const std::string prefix = "support_data.";
  std::vector<BurstRecord> burstRecords;

  int listCount = m_MetadataSupplierInterface->GetAs<int>(prefix + "geom.bursts.number");

  const int version = m_MetadataSupplierInterface->GetAs<int>("header.version");

  for (int listId = 0 ; listId <= listCount - 1 ; ++listId)
  {
    const std::string burstName = prefix + "geom.bursts.burst[" + std::to_string(listId) + "].";
    BurstRecord record;
    
    record.azimuthStartTime = MetaData::ReadFormattedDate(m_MetadataSupplierInterface->GetAs<std::string>(burstName + "azimuth_start_time"));
    record.azimuthStopTime = MetaData::ReadFormattedDate(m_MetadataSupplierInterface->GetAs<std::string>(burstName + "azimuth_stop_time"));

    record.startLine = m_MetadataSupplierInterface->GetAs<int>(burstName + "start_line");
    record.endLine = m_MetadataSupplierInterface->GetAs<int>(burstName + "end_line");

    if (version >= 4)
    {
      record.azimuthAnxTime = m_MetadataSupplierInterface->GetAs<double>(burstName + "azimuth_anx_time");
    }
    else
    {
      record.azimuthAnxTime = 0.;
    }
    
    if (version >= 3)
    {
      record.startSample = m_MetadataSupplierInterface->GetAs<int>(burstName + "start_sample");
      record.endSample = m_MetadataSupplierInterface->GetAs<int>(burstName + "end_sample");
    }
    else
    {
      record.startSample = 0;
      record.endSample = 0;
    }

    burstRecords.push_back(std::move(record));
  }

  return burstRecords;
}

bool SarImageMetadataInterface::GetSAR(SARParam & sarParam) const
{
  bool hasValue;
  m_MetadataSupplierInterface->GetMetadataValue("calibration.count", hasValue);
  if (!hasValue)
    return false;
  
  sarParam.azimuthFmRates = this->GetAzimuthFmRateGeom();
  sarParam.dopplerCentroids = this->GetDopplerCentroidGeom();
  sarParam.orbits = this->GetOrbitsGeom();
  sarParam.burstRecords = this->GetBurstRecordsGeom();

  const std::string supportDataPrefix = "support_data.";
  sarParam.rangeSamplingRate = m_MetadataSupplierInterface->GetAs<double>(
                                supportDataPrefix + "range_sampling_rate");

  sarParam.nearRangeTime = m_MetadataSupplierInterface->GetAs<double>(
                                supportDataPrefix + "slant_range_to_first_pixel");

  sarParam.rangeResolution = m_MetadataSupplierInterface->GetAs<double>(
                                supportDataPrefix + "range_spacing");

  sarParam.azimuthTimeInterval = MetaData::DurationType::Seconds(m_MetadataSupplierInterface->GetAs<double>(
                                supportDataPrefix + "line_time_interval") );

  if (sarParam.burstRecords.size() > 1 && m_MetadataSupplierInterface->GetAs<int>("header.version") > 2)
  {
    sarParam.numberOfLinesPerBurst = m_MetadataSupplierInterface->GetAs<unsigned long>(
                                  supportDataPrefix + "geom.bursts.number_lines_per_burst");

    sarParam.numberOfSamplesPerBurst = m_MetadataSupplierInterface->GetAs<unsigned long>(
                                  supportDataPrefix + "geom.bursts.number_samples_per_burst");
  }
  return true;
}

void SarImageMetadataInterface::LoadRadiometricCalibrationData(SARCalib &sarCalib, const MetadataSupplierInterface &mds,
                                                               const ImageMetadata &imd, const std::string& band) const
{
  sarCalib.rescalingFactor = GetRescalingFactor();
  auto coeffs = GetRadiometricCalibrationNoisePolynomialDegree();
  std::copy(coeffs.begin(), coeffs.end(), sarCalib.radiometricCalibrationNoisePolynomialDegree.begin());
  coeffs = GetRadiometricCalibrationAntennaPatternNewGainPolynomialDegree();
  std::copy(coeffs.begin(), coeffs.end(), sarCalib.radiometricCalibrationAntennaPatternNewGainPolynomialDegree.begin());
  coeffs = GetRadiometricCalibrationAntennaPatternOldGainPolynomialDegree();
  std::copy(coeffs.begin(), coeffs.end(), sarCalib.radiometricCalibrationAntennaPatternOldGainPolynomialDegree.begin());
  coeffs = GetRadiometricCalibrationIncidenceAnglePolynomialDegree();
  std::copy(coeffs.begin(), coeffs.end(), sarCalib.radiometricCalibrationIncidenceAnglePolynomialDegree.begin());
  coeffs = GetRadiometricCalibrationRangeSpreadLossPolynomialDegree();
  std::copy(coeffs.begin(), coeffs.end(), sarCalib.radiometricCalibrationRangeSpreadLossPolynomialDegree.begin());
  sarCalib.radiometricCalibrationNoise = GetRadiometricCalibrationNoise(mds, imd, band);
  sarCalib.radiometricCalibrationAntennaPatternNewGain = GetRadiometricCalibrationAntennaPatternNewGain();
  sarCalib.radiometricCalibrationAntennaPatternOldGain = GetRadiometricCalibrationAntennaPatternOldGain();
  sarCalib.radiometricCalibrationIncidenceAngle = GetRadiometricCalibrationIncidenceAngle(mds);
  sarCalib.radiometricCalibrationRangeSpreadLoss = GetRadiometricCalibrationRangeSpreadLoss();
}

void SarImageMetadataInterface::PrintSelf(std::ostream& os, itk::Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  if (this->CanRead())
  {
    os << indent << "GetRadiometricCalibrationScale:                 " << this->GetRadiometricCalibrationScale() << "\n"
//       << indent << "GetRadiometricCalibrationNoise:                 " << this->GetRadiometricCalibrationNoise() << "\n"
       << indent << "GetRadiometricCalibrationAntennaPatternNewGain: " << this->GetRadiometricCalibrationAntennaPatternNewGain() << "\n"
       << indent << "GetRadiometricCalibrationAntennaPatternOldGain: " << this->GetRadiometricCalibrationAntennaPatternOldGain() << "\n"
//       << indent << "GetRadiometricCalibrationIncidenceAngle:        " << this->GetRadiometricCalibrationIncidenceAngle() << "\n"
       << indent << "GetRadiometricCalibrationRangeSpreadLoss:       " << this->GetRadiometricCalibrationRangeSpreadLoss() << "\n"
       << indent << "GetConstantPolynomialDegree:                    ";
    for(const auto& s: this->GetConstantPolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetRadiometricCalibrationNoisePolynomialDegree: ";
    for(const auto& s: this->GetRadiometricCalibrationNoisePolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetRadiometricCalibrationAntennaPatternNewGainPolynomialDegree: ";
    for(const auto& s: this->GetRadiometricCalibrationAntennaPatternNewGainPolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetRadiometricCalibrationAntennaPatternOldGainPolynomialDegree: ";
    for(const auto& s: this->GetRadiometricCalibrationAntennaPatternOldGainPolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetRadiometricCalibrationIncidenceAnglePolynomialDegree:        ";
    for(const auto& s: this->GetRadiometricCalibrationIncidenceAnglePolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetRadiometricCalibrationRangeSpreadLossPolynomialDegree:       ";
    for(const auto& s: this->GetRadiometricCalibrationRangeSpreadLossPolynomialDegree())
      os << s << " ";
    os << "\n"
       << indent << "GetPRF:                  " << this->GetPRF() << "\n"
       << indent << "GetRSF:                  " << this->GetRSF() << "\n"
       << indent << "GetRadarFrequency:       " << this->GetRadarFrequency() << "\n";
//       << indent << "GetCenterIncidenceAngle: " << this->GetCenterIncidenceAngle() << std::endl;
  }
}

bool SarImageMetadataInterface::ConvertImageKeywordlistToImageMetadata(ImageMetadata&)
{
  // TODO
  return false;
}

} // end namespace otb
