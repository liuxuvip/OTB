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

#include "otbWrapperApplication.h"
#include "otbWrapperApplicationFactory.h"
#include "otbGenericRSTransform.h"
#include "otbOGRDataSourceWrapper.h"
#include "otbCurlHelper.h"

#include <sstream>
#include <iomanip>

namespace otb
{

enum
{
  Mode_Download,
  Mode_List
};

const std::string SRTMServerPath = "https://dds.cr.usgs.gov/srtm/version2_1/SRTM3/";

const std::string HGTZIPExtension = ".hgt.zip";
const std::string HGTExtension    = ".hgt";
const std::string ZIPExtension    = ".zip";
#ifdef _WIN32
const char Sep = '\\';
#else
const char Sep = '/';
#endif
namespace Wrapper
{

class DownloadSRTMTiles : public Application
{
public:
  /** Standard class typedefs. */
  typedef DownloadSRTMTiles             Self;
  typedef Application                   Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  using RSTransformType = otb::GenericRSTransform<>;
  using IndexType       = FloatVectorImageType::IndexType;
  using PointType       = FloatVectorImageType::PointType;
  using SizeType        = FloatVectorImageType::SizeType;
  using SpacingType     = FloatVectorImageType::SpacingType;

  /** Standard macro */
  itkNewMacro(Self);

  itkTypeMacro(DownloadSRTMTiles, otb::Application);

  typedef struct
  {
    int          Lon : 9;
    int          Lat : 8;
    unsigned int Conti : 3;
  } SRTMTileId;

private:
  DownloadSRTMTiles();

  class TileIdComparator
  {
  public:
    bool operator()(const SRTMTileId& a, const SRTMTileId& b) const
    {
      if (a.Lat < b.Lat)
        return true;
      if (a.Lat == b.Lat && a.Lon < b.Lon)
        return true;
      return false;
    }
  };

  typedef std::set<SRTMTileId, TileIdComparator> SRTMTileSet;

  std::string SRTMIdToName(const SRTMTileId& id) const
  {
    std::ostringstream oss;
    oss << (id.Lat < 0 ? 'S' : 'N');
    oss << std::setfill('0') << std::setw(2) << std::abs(id.Lat);
    oss << (id.Lon < 0 ? 'W' : 'E');
    oss << std::setfill('0') << std::setw(3) << std::abs(id.Lon);
    return oss.str();
  }

  std::string SRTMIdToContinent(const SRTMTileId& id) const
  {
    switch (id.Conti)
    {
    case 0:
      // this tile is unknown, return an empty string.
      break;
    case 1:
      return std::string("Africa");
    case 2:
      return std::string("Australia");
    case 3:
      return std::string("Eurasia");
    case 4:
      return std::string("Islands");
    case 5:
      return std::string("North_America");
    case 6:
      return std::string("South_America");
    default:
      break;
    }
    return std::string();
  }

  bool SRTMTileExists(const SRTMTileId& tile, std::string& continent) const
  {
    auto pos = m_SRTMTileList.find(tile);
    if (pos != m_SRTMTileList.end())
    {
      continent = this->SRTMIdToContinent(*pos);
      return true;
    }
    return false;
  }

  bool SRTMTileDownloaded(const std::string& name, const std::string& tileDir) const
  {
    std::string path(tileDir);
    if (!path.empty() && path.back() != Sep)
    {
      path += Sep;
    }
    // try different filenames
    std::string filepath(path + name + HGTExtension);
    bool        exists = itksys::SystemTools::FileExists(filepath);
    if (!exists)
    {
      filepath += ZIPExtension;
      exists = itksys::SystemTools::FileExists(filepath);
    }

    if (!exists)
    {
      std::string lowerName(name);
      std::transform(name.begin(), name.end(), lowerName.begin(), ::tolower);
      filepath = path + lowerName + HGTExtension;
      exists   = itksys::SystemTools::FileExists(filepath);
      if (!exists)
      {
        filepath += ZIPExtension;
        exists = itksys::SystemTools::FileExists(filepath);
      }
    }
    return exists;
  }

  bool CheckPermissions(const std::string& dir) const
  {
    std::string path(dir);
    if (!path.empty() && path.back() != Sep)
    {
      path += Sep;
    }
    path += "foo";
    if (itksys::SystemTools::Touch(path, true))
    {
      itksys::SystemTools::RemoveFile(path);
    }
    else
    {
      return false;
    }
    return true;
  }

  void DoInit() override
  {
    SetName("DownloadSRTMTiles");
    SetDescription("Download or list SRTM tiles");

    // Documentation
    SetDocLongDescription(
        "This application allows selecting the appropriate SRTM tiles that covers a list of images. It builds a list of the required tiles. Two modes are "
        "available: the first one download those tiles from the USGS SRTM3 website (http://dds.cr.usgs.gov/srtm/version2_1/SRTM3/), the second one list those "
        "tiles in a local directory. In both cases, you need to indicate the directory in which directory  tiles will be download or the location of local "
        "SRTM files.");
    SetDocLimitations("None");
    SetDocAuthors("OTB-Team");
    SetDocSeeAlso(" ");

    AddDocTag(Tags::Manip);
    AddDocTag("Utilities");
    AddDocTag(Tags::Deprecated);

    AddParameter(ParameterType_InputImageList, "il", "Input images list");
    SetParameterDescription("il",
                            "List of images on which you want to "
                            "determine corresponding SRTM tiles.");
    MandatoryOff("il");

    AddParameter(ParameterType_InputVectorDataList, "vl", "Input vector data list");
    SetParameterDescription("vl",
                            "List of vector data files on which you want "
                            "to determine corresponding SRTM tiles.");
    MandatoryOff("vl");

    AddParameter(ParameterType_StringList, "names", "Input tile names");
    SetParameterDescription("names",
                            "List of SRTM tile names to download. "
                            "This list is added to the tiles derived from input images or vectors."
                            "The names should follow the SRTM tile naming convention, for instance "
                            "N43E001.");
    MandatoryOff("names");

    AddParameter(ParameterType_Directory, "tiledir", "Tiles directory");
    SetParameterDescription("tiledir",
                            "Directory where SRTM tiles "
                            "are stored. In download mode, the zipped archives will be downloaded to "
                            "this directory. You'll need to unzip all tile files before using them in"
                            " your application. In any case, this directory will be inspected to "
                            "check which tiles are already downloaded.");

    // UserDefined values
    AddParameter(ParameterType_Choice, "mode", "Download/List corresponding SRTM tiles");

    AddChoice("mode.download", "Download");
    SetParameterDescription("mode.download", "Download corresponding tiles on USGE server.");

    AddChoice("mode.list", "List tiles");
    SetParameterDescription("mode.list", "List tiles in an existing local directory.");

    // Doc example parameter settings
    SetDocExampleParameterValue("il", "QB_Toulouse_Ortho_XS.tif");
    SetDocExampleParameterValue("mode", "list");
    SetDocExampleParameterValue("tiledir", "/home/user/srtm_dir/");

    SetOfficialDocLink();
  }

  void DoUpdateParameters() override
  {
    // Nothing to do here : all parameters are independent
  }


  void DoExecute() override
  {
    // Get the mode
    int mode = GetParameterInt("mode");

    // Get the inputs
    auto                     inList = FloatVectorImageListType::New();
    std::vector<std::string> vectorDataList;
    std::vector<std::string> nameList;

    if (IsParameterEnabled("il") && HasValue("il"))
      inList = this->GetParameterImageList("il");
    if (IsParameterEnabled("vl") && HasValue("vl"))
      vectorDataList = this->GetParameterStringList("vl");
    if (IsParameterEnabled("names") && HasValue("names"))
      nameList = this->GetParameterStringList("names");

    std::string tileDir = this->GetParameterString("tiledir");

    if (inList->Size() + vectorDataList.size() + nameList.size() == 0)
    {
      itkExceptionMacro("No input image/vector/name set...");
    }

    SRTMTileSet tiles;
    //--------------------------------------------------------------------------
    // Check input images
    for (unsigned int i = 0; i < inList->Size(); i++)
    {
      auto inImage = inList->GetNthElement(i);

      auto rsTransformToWGS84 = RSTransformType::New();
      rsTransformToWGS84->SetInputImageMetadata(&(inImage->GetImageMetadata()));
      rsTransformToWGS84->SetInputProjectionRef(inImage->GetProjectionRef());
      rsTransformToWGS84->SetOutputProjectionRef(static_cast<std::string>(otb::SpatialReference::FromWGS84().ToWkt()));
      rsTransformToWGS84->InstantiateTransform();

      const SizeType  size  = inImage->GetLargestPossibleRegion().GetSize();
      const IndexType start = inImage->GetLargestPossibleRegion().GetIndex();
      PointType       tmpPoint;
      itk::ContinuousIndex<double, 2> index(start);
      index[0] += -0.5;
      index[1] += -0.5;
      inImage->TransformContinuousIndexToPhysicalPoint(index, tmpPoint);
      PointType ul = rsTransformToWGS84->TransformPoint(tmpPoint);
      index[0] += size[0];
      inImage->TransformContinuousIndexToPhysicalPoint(index, tmpPoint);
      PointType ur = rsTransformToWGS84->TransformPoint(tmpPoint);
      index[1] += size[1];
      inImage->TransformContinuousIndexToPhysicalPoint(index, tmpPoint);
      PointType lr = rsTransformToWGS84->TransformPoint(tmpPoint);
      index[0] -= (double)size[0];
      inImage->TransformContinuousIndexToPhysicalPoint(index, tmpPoint);
      PointType ll = rsTransformToWGS84->TransformPoint(tmpPoint);

      int floorMinLong = std::floor(std::min({ul[0], ur[0], ll[0], lr[0]}));
      int floorMaxLong = std::floor(std::max({ul[0], ur[0], ll[0], lr[0]}));
      int floorMinLat  = std::floor(std::min({ul[1], ur[1], ll[1], lr[1]}));
      int floorMaxLat  = std::floor(std::max({ul[1], ur[1], ll[1], lr[1]}));

      // Construct SRTM tile filename based on min/max lat/long
      for (int k = floorMinLat; k <= floorMaxLat; ++k)
      {
        for (int j = floorMinLong; j <= floorMaxLong; ++j)
        {
          tiles.insert(SRTMTileId({j, k, 0}));
        }
      }
    }
    //--------------------------------------------------------------------------
    // Check input vector files
    for (unsigned int i = 0; i < vectorDataList.size(); i++)
    {
      auto        source = ogr::DataSource::New(vectorDataList[i], ogr::DataSource::Modes::Read);
      OGREnvelope envelope;
      std::string currentWkt("");
      try
      {
        envelope = source->GetGlobalExtent(false, &currentWkt);
      }
      catch (...)
      {
      }
      if (currentWkt.empty())
      {
        try
        {
          envelope = source->GetGlobalExtent(true, &currentWkt);
        }
        catch (...)
        {
          otbAppLogWARNING("Can't get envelope of vector file : " << vectorDataList[i]);
          continue;
        }
      }

      auto rsTransformToWGS84 = RSTransformType::New();
      rsTransformToWGS84->SetInputProjectionRef(currentWkt);
      rsTransformToWGS84->SetOutputProjectionRef(static_cast<std::string>(otb::SpatialReference::FromWGS84().ToWkt()));
      rsTransformToWGS84->InstantiateTransform();
      PointType tmpPoint;
      tmpPoint[0]            = envelope.MinX;
      tmpPoint[1]            = envelope.MinY;
      PointType ll           = rsTransformToWGS84->TransformPoint(tmpPoint);
      tmpPoint[0]            = envelope.MaxX;
      tmpPoint[1]            = envelope.MaxY;
      PointType ur           = rsTransformToWGS84->TransformPoint(tmpPoint);
      int       floorMinLong = std::floor(ll[0]);
      int       floorMaxLong = std::floor(ur[0]);
      int       floorMinLat  = std::floor(ll[1]);
      int       floorMaxLat  = std::floor(ur[1]);

      // Construct SRTM tile filename based on min/max lat/long
      for (int k = floorMinLat; k <= floorMaxLat; ++k)
      {
        for (int j = floorMinLong; j <= floorMaxLong; ++j)
        {
          tiles.insert(SRTMTileId({j, k, 0}));
        }
      }
    }
    //--------------------------------------------------------------------------
    // Check input names
    for (unsigned int i = 0; i < nameList.size(); i++)
    {
      if (nameList[i].size() >= 7)
      {
        int lon = 0, lat = 0;
        try
        {
          lat = boost::lexical_cast<int>(nameList[i].substr(1, 2));
        }
        catch (boost::bad_lexical_cast&)
        {
          otbAppLogWARNING(<< "Wrong longitude in tile name : " << nameList[i]);
          continue;
        }
        try
        {
          lon = boost::lexical_cast<int>(nameList[i].substr(4, 3));
        }
        catch (boost::bad_lexical_cast&)
        {
          otbAppLogWARNING(<< "Wrong latitude in tile name : " << nameList[i]);
          continue;
        }
        if (nameList[i][0] == 's' || nameList[i][0] == 'S')
          lat = -lat;
        else if (nameList[i][0] != 'n' && nameList[i][0] != 'N')
        {
          otbAppLogWARNING(<< "Wrong northing in tile name : " << nameList[i]);
          continue;
        }
        if (nameList[i][3] == 'w' || nameList[i][3] == 'W')
          lon = -lon;
        else if (nameList[i][3] != 'e' && nameList[i][3] != 'E')
        {
          otbAppLogWARNING(<< "Wrong easting in tile name : " << nameList[i]);
          continue;
        }
        tiles.insert(SRTMTileId({lon, lat, 0}));
      }
      else
      {
        otbAppLogWARNING(<< "Tile name should have at least 7 characters : " << nameList[i]);
      }
    }

    if (tiles.empty())
    {
      otbAppLogWARNING(<< "No tile found for the given input(s)!");
      return;
    }

    // iterate over all tiles to build URLs
    std::vector<std::string> nonSRTMTiles;
    std::vector<std::string> localTiles;
    std::vector<std::string> missingTiles;
    std::vector<std::string> continentList;
    for (auto const& tileId : tiles)
    {
      std::string curName(this->SRTMIdToName(tileId));
      std::string continent;
      // Check 1 : does the tile exists in SRTM ? If yes, get the continent
      if (SRTMTileExists(tileId, continent))
      {
        // Check 2 : is the tile already downloaded
        if (SRTMTileDownloaded(curName, tileDir))
        {
          localTiles.push_back(curName);
        }
        else
        {
          missingTiles.push_back(curName);
          continentList.push_back(continent);
        }
      }
      else
      {
        nonSRTMTiles.push_back(curName);
      }
    }

    otbAppLogINFO(<< "Total candidate tiles: " << tiles.size());
    // If download mode : try to get missing tiles
    int srtmHitRatio = (100 * (tiles.size() - nonSRTMTiles.size())) / tiles.size();
    otbAppLogINFO(<< "  --> " << nonSRTMTiles.size() << " missing tiles in SRTM (coverage ratio is " << srtmHitRatio << "%)");
    otbAppLogINFO(<< "  --> " << localTiles.size() << " tiles already in the directory");
    otbAppLogINFO(<< "  --> " << missingTiles.size() << " tiles to download");
    if (mode == Mode_List)
    {
      std::ostringstream oss;
      for (auto const& tileName : nonSRTMTiles)
      {
        oss << "  " << tileName << " = NotSRTM\n";
      }
      for (auto const& tileName : localTiles)
      {
        oss << "  " << tileName << " = Local\n";
      }
      for (auto const& tileName : missingTiles)
      {
        oss << "  " << tileName << " = Missing\n";
      }
      otbAppLogINFO(<< "Status of each tile (NotSRTM/Local/Missing):\n" << oss.str());
    }
    if (mode == Mode_Download)
    {
      // Check permissions first
      if (!CheckPermissions(tileDir))
      {
        otbAppLogFATAL(<< "Can't write into directory : '" << tileDir << "'");
      }
      std::vector<std::string>::const_iterator it, itConti;
      auto                                     curl = CurlHelper::New();
      curl->SetTimeout(0);
      std::string request;
      if (!tileDir.empty() && tileDir.back() != Sep)
      {
        tileDir += Sep;
      }
      for (it = missingTiles.begin(), itConti = continentList.begin(); it != missingTiles.end() && itConti != continentList.end(); ++it, ++itConti)
      {
        otbAppLogINFO(<< "Downloading tile " << *it << " ...");
        request = SRTMServerPath + *itConti + "/" + *it + HGTZIPExtension;
        if (curl->IsCurlReturnHttpError(request))
        {
          otbAppLogWARNING(<< "Can't access tile : " << request);
          continue;
        }
        curl->RetrieveFile(request, std::string(tileDir + *it + HGTZIPExtension));
      }
    }
  }

  SRTMTileSet m_SRTMTileList;
};

} // end of namespace Wrapper
} // end of namespace otb

const otb::Wrapper::DownloadSRTMTiles::SRTMTileId staticTileList[] = {
    {6, 0, 1},      {9, 0, 1},      {10, 0, 1},     {11, 0, 1},     {12, 0, 1},     {13, 0, 1},     {14, 0, 1},     {15, 0, 1},     {16, 0, 1},
    {17, 0, 1},     {18, 0, 1},     {19, 0, 1},     {20, 0, 1},     {21, 0, 1},     {22, 0, 1},     {23, 0, 1},     {24, 0, 1},     {25, 0, 1},
    {26, 0, 1},     {27, 0, 1},     {28, 0, 1},     {29, 0, 1},     {30, 0, 1},     {31, 0, 1},     {32, 0, 1},     {33, 0, 1},     {34, 0, 1},
    {35, 0, 1},     {36, 0, 1},     {37, 0, 1},     {38, 0, 1},     {39, 0, 1},     {40, 0, 1},     {41, 0, 1},     {42, 0, 1},     {43, 0, 1},
    {7, 1, 1},      {9, 1, 1},      {10, 1, 1},     {11, 1, 1},     {12, 1, 1},     {13, 1, 1},     {14, 1, 1},     {15, 1, 1},     {16, 1, 1},
    {17, 1, 1},     {18, 1, 1},     {19, 1, 1},     {20, 1, 1},     {21, 1, 1},     {22, 1, 1},     {23, 1, 1},     {24, 1, 1},     {25, 1, 1},
    {26, 1, 1},     {27, 1, 1},     {28, 1, 1},     {29, 1, 1},     {30, 1, 1},     {31, 1, 1},     {32, 1, 1},     {33, 1, 1},     {34, 1, 1},
    {35, 1, 1},     {36, 1, 1},     {37, 1, 1},     {38, 1, 1},     {39, 1, 1},     {40, 1, 1},     {41, 1, 1},     {42, 1, 1},     {43, 1, 1},
    {44, 1, 1},     {45, 1, 1},     {9, 2, 1},      {10, 2, 1},     {11, 2, 1},     {12, 2, 1},     {13, 2, 1},     {14, 2, 1},     {15, 2, 1},
    {16, 2, 1},     {17, 2, 1},     {18, 2, 1},     {19, 2, 1},     {20, 2, 1},     {21, 2, 1},     {22, 2, 1},     {23, 2, 1},     {24, 2, 1},
    {25, 2, 1},     {26, 2, 1},     {27, 2, 1},     {28, 2, 1},     {29, 2, 1},     {30, 2, 1},     {31, 2, 1},     {32, 2, 1},     {33, 2, 1},
    {34, 2, 1},     {35, 2, 1},     {36, 2, 1},     {37, 2, 1},     {38, 2, 1},     {39, 2, 1},     {40, 2, 1},     {41, 2, 1},     {42, 2, 1},
    {43, 2, 1},     {44, 2, 1},     {45, 2, 1},     {46, 2, 1},     {8, 3, 1},      {9, 3, 1},      {10, 3, 1},     {11, 3, 1},     {12, 3, 1},
    {13, 3, 1},     {14, 3, 1},     {15, 3, 1},     {16, 3, 1},     {17, 3, 1},     {18, 3, 1},     {19, 3, 1},     {20, 3, 1},     {21, 3, 1},
    {22, 3, 1},     {23, 3, 1},     {24, 3, 1},     {25, 3, 1},     {26, 3, 1},     {27, 3, 1},     {28, 3, 1},     {29, 3, 1},     {30, 3, 1},
    {31, 3, 1},     {32, 3, 1},     {33, 3, 1},     {34, 3, 1},     {35, 3, 1},     {36, 3, 1},     {37, 3, 1},     {38, 3, 1},     {39, 3, 1},
    {40, 3, 1},     {41, 3, 1},     {42, 3, 1},     {43, 3, 1},     {44, 3, 1},     {45, 3, 1},     {46, 3, 1},     {47, 3, 1},     {5, 4, 1},
    {6, 4, 1},      {7, 4, 1},      {8, 4, 1},      {9, 4, 1},      {10, 4, 1},     {11, 4, 1},     {12, 4, 1},     {13, 4, 1},     {14, 4, 1},
    {15, 4, 1},     {16, 4, 1},     {17, 4, 1},     {18, 4, 1},     {19, 4, 1},     {20, 4, 1},     {21, 4, 1},     {22, 4, 1},     {23, 4, 1},
    {24, 4, 1},     {25, 4, 1},     {26, 4, 1},     {27, 4, 1},     {28, 4, 1},     {29, 4, 1},     {30, 4, 1},     {31, 4, 1},     {32, 4, 1},
    {33, 4, 1},     {34, 4, 1},     {35, 4, 1},     {36, 4, 1},     {37, 4, 1},     {38, 4, 1},     {39, 4, 1},     {40, 4, 1},     {41, 4, 1},
    {42, 4, 1},     {43, 4, 1},     {44, 4, 1},     {45, 4, 1},     {46, 4, 1},     {47, 4, 1},     {48, 4, 1},     {-2, 4, 1},     {-3, 4, 1},
    {-6, 4, 1},     {-7, 4, 1},     {-8, 4, 1},     {-9, 4, 1},     {-10, 4, 1},    {0, 5, 1},      {1, 5, 1},      {4, 5, 1},      {5, 5, 1},
    {6, 5, 1},      {7, 5, 1},      {8, 5, 1},      {9, 5, 1},      {10, 5, 1},     {11, 5, 1},     {12, 5, 1},     {13, 5, 1},     {14, 5, 1},
    {15, 5, 1},     {16, 5, 1},     {17, 5, 1},     {18, 5, 1},     {19, 5, 1},     {20, 5, 1},     {21, 5, 1},     {22, 5, 1},     {23, 5, 1},
    {24, 5, 1},     {25, 5, 1},     {26, 5, 1},     {27, 5, 1},     {28, 5, 1},     {29, 5, 1},     {30, 5, 1},     {31, 5, 1},     {32, 5, 1},
    {33, 5, 1},     {34, 5, 1},     {35, 5, 1},     {36, 5, 1},     {37, 5, 1},     {38, 5, 1},     {39, 5, 1},     {40, 5, 1},     {41, 5, 1},
    {42, 5, 1},     {43, 5, 1},     {44, 5, 1},     {45, 5, 1},     {46, 5, 1},     {47, 5, 1},     {48, 5, 1},     {-1, 5, 1},     {-2, 5, 1},
    {-3, 5, 1},     {-4, 5, 1},     {-5, 5, 1},     {-6, 5, 1},     {-7, 5, 1},     {-8, 5, 1},     {-9, 5, 1},     {-10, 5, 1},    {-11, 5, 1},
    {0, 6, 1},      {1, 6, 1},      {2, 6, 1},      {3, 6, 1},      {4, 6, 1},      {5, 6, 1},      {6, 6, 1},      {7, 6, 1},      {8, 6, 1},
    {9, 6, 1},      {10, 6, 1},     {11, 6, 1},     {12, 6, 1},     {13, 6, 1},     {14, 6, 1},     {15, 6, 1},     {16, 6, 1},     {17, 6, 1},
    {18, 6, 1},     {19, 6, 1},     {20, 6, 1},     {21, 6, 1},     {22, 6, 1},     {23, 6, 1},     {24, 6, 1},     {25, 6, 1},     {26, 6, 1},
    {27, 6, 1},     {28, 6, 1},     {29, 6, 1},     {30, 6, 1},     {31, 6, 1},     {32, 6, 1},     {33, 6, 1},     {34, 6, 1},     {35, 6, 1},
    {36, 6, 1},     {37, 6, 1},     {38, 6, 1},     {39, 6, 1},     {40, 6, 1},     {41, 6, 1},     {42, 6, 1},     {43, 6, 1},     {44, 6, 1},
    {45, 6, 1},     {46, 6, 1},     {47, 6, 1},     {48, 6, 1},     {49, 6, 1},     {-1, 6, 1},     {-2, 6, 1},     {-3, 6, 1},     {-4, 6, 1},
    {-5, 6, 1},     {-6, 6, 1},     {-7, 6, 1},     {-8, 6, 1},     {-9, 6, 1},     {-10, 6, 1},    {-11, 6, 1},    {-12, 6, 1},    {0, 7, 1},
    {1, 7, 1},      {2, 7, 1},      {3, 7, 1},      {4, 7, 1},      {5, 7, 1},      {6, 7, 1},      {7, 7, 1},      {8, 7, 1},      {9, 7, 1},
    {10, 7, 1},     {11, 7, 1},     {12, 7, 1},     {13, 7, 1},     {14, 7, 1},     {15, 7, 1},     {16, 7, 1},     {17, 7, 1},     {18, 7, 1},
    {19, 7, 1},     {20, 7, 1},     {21, 7, 1},     {22, 7, 1},     {23, 7, 1},     {24, 7, 1},     {25, 7, 1},     {26, 7, 1},     {27, 7, 1},
    {28, 7, 1},     {29, 7, 1},     {30, 7, 1},     {31, 7, 1},     {32, 7, 1},     {33, 7, 1},     {34, 7, 1},     {35, 7, 1},     {36, 7, 1},
    {37, 7, 1},     {38, 7, 1},     {39, 7, 1},     {40, 7, 1},     {41, 7, 1},     {42, 7, 1},     {43, 7, 1},     {44, 7, 1},     {45, 7, 1},
    {46, 7, 1},     {47, 7, 1},     {48, 7, 1},     {49, 7, 1},     {-1, 7, 1},     {-2, 7, 1},     {-3, 7, 1},     {-4, 7, 1},     {-5, 7, 1},
    {-6, 7, 1},     {-7, 7, 1},     {-8, 7, 1},     {-9, 7, 1},     {-10, 7, 1},    {-11, 7, 1},    {-12, 7, 1},    {-13, 7, 1},    {-14, 7, 1},
    {0, 8, 1},      {1, 8, 1},      {2, 8, 1},      {3, 8, 1},      {4, 8, 1},      {5, 8, 1},      {6, 8, 1},      {7, 8, 1},      {8, 8, 1},
    {9, 8, 1},      {10, 8, 1},     {11, 8, 1},     {12, 8, 1},     {13, 8, 1},     {14, 8, 1},     {15, 8, 1},     {16, 8, 1},     {17, 8, 1},
    {18, 8, 1},     {19, 8, 1},     {20, 8, 1},     {21, 8, 1},     {22, 8, 1},     {23, 8, 1},     {24, 8, 1},     {25, 8, 1},     {26, 8, 1},
    {27, 8, 1},     {28, 8, 1},     {29, 8, 1},     {30, 8, 1},     {31, 8, 1},     {32, 8, 1},     {33, 8, 1},     {34, 8, 1},     {35, 8, 1},
    {36, 8, 1},     {37, 8, 1},     {38, 8, 1},     {39, 8, 1},     {40, 8, 1},     {41, 8, 1},     {42, 8, 1},     {43, 8, 1},     {44, 8, 1},
    {45, 8, 1},     {46, 8, 1},     {47, 8, 1},     {48, 8, 1},     {49, 8, 1},     {50, 8, 1},     {-1, 8, 1},     {-2, 8, 1},     {-3, 8, 1},
    {-4, 8, 1},     {-5, 8, 1},     {-6, 8, 1},     {-7, 8, 1},     {-8, 8, 1},     {-9, 8, 1},     {-10, 8, 1},    {-11, 8, 1},    {-12, 8, 1},
    {-13, 8, 1},    {-14, 8, 1},    {0, 9, 1},      {1, 9, 1},      {2, 9, 1},      {3, 9, 1},      {4, 9, 1},      {5, 9, 1},      {6, 9, 1},
    {7, 9, 1},      {8, 9, 1},      {9, 9, 1},      {10, 9, 1},     {11, 9, 1},     {12, 9, 1},     {13, 9, 1},     {14, 9, 1},     {15, 9, 1},
    {16, 9, 1},     {17, 9, 1},     {18, 9, 1},     {19, 9, 1},     {20, 9, 1},     {21, 9, 1},     {22, 9, 1},     {23, 9, 1},     {24, 9, 1},
    {25, 9, 1},     {26, 9, 1},     {27, 9, 1},     {28, 9, 1},     {29, 9, 1},     {30, 9, 1},     {31, 9, 1},     {32, 9, 1},     {33, 9, 1},
    {34, 9, 1},     {35, 9, 1},     {36, 9, 1},     {37, 9, 1},     {38, 9, 1},     {39, 9, 1},     {40, 9, 1},     {41, 9, 1},     {42, 9, 1},
    {43, 9, 1},     {44, 9, 1},     {45, 9, 1},     {46, 9, 1},     {47, 9, 1},     {48, 9, 1},     {49, 9, 1},     {50, 9, 1},     {-1, 9, 1},
    {-2, 9, 1},     {-3, 9, 1},     {-4, 9, 1},     {-5, 9, 1},     {-6, 9, 1},     {-7, 9, 1},     {-8, 9, 1},     {-9, 9, 1},     {-10, 9, 1},
    {-11, 9, 1},    {-12, 9, 1},    {-13, 9, 1},    {-14, 9, 1},    {-15, 9, 1},    {0, 10, 1},     {1, 10, 1},     {2, 10, 1},     {3, 10, 1},
    {4, 10, 1},     {5, 10, 1},     {6, 10, 1},     {7, 10, 1},     {8, 10, 1},     {9, 10, 1},     {10, 10, 1},    {11, 10, 1},    {12, 10, 1},
    {13, 10, 1},    {14, 10, 1},    {15, 10, 1},    {16, 10, 1},    {17, 10, 1},    {18, 10, 1},    {19, 10, 1},    {20, 10, 1},    {21, 10, 1},
    {22, 10, 1},    {23, 10, 1},    {24, 10, 1},    {25, 10, 1},    {26, 10, 1},    {27, 10, 1},    {28, 10, 1},    {29, 10, 1},    {30, 10, 1},
    {31, 10, 1},    {32, 10, 1},    {33, 10, 1},    {34, 10, 1},    {35, 10, 1},    {36, 10, 1},    {37, 10, 1},    {38, 10, 1},    {39, 10, 1},
    {40, 10, 1},    {41, 10, 1},    {42, 10, 1},    {43, 10, 1},    {44, 10, 1},    {45, 10, 1},    {46, 10, 1},    {47, 10, 1},    {48, 10, 1},
    {49, 10, 1},    {50, 10, 1},    {51, 10, 1},    {-1, 10, 1},    {-2, 10, 1},    {-3, 10, 1},    {-4, 10, 1},    {-5, 10, 1},    {-6, 10, 1},
    {-7, 10, 1},    {-8, 10, 1},    {-9, 10, 1},    {-10, 10, 1},   {-11, 10, 1},   {-12, 10, 1},   {-13, 10, 1},   {-14, 10, 1},   {-15, 10, 1},
    {-16, 10, 1},   {0, 11, 1},     {1, 11, 1},     {2, 11, 1},     {3, 11, 1},     {4, 11, 1},     {5, 11, 1},     {6, 11, 1},     {7, 11, 1},
    {8, 11, 1},     {9, 11, 1},     {10, 11, 1},    {11, 11, 1},    {12, 11, 1},    {13, 11, 1},    {14, 11, 1},    {15, 11, 1},    {16, 11, 1},
    {17, 11, 1},    {18, 11, 1},    {19, 11, 1},    {20, 11, 1},    {21, 11, 1},    {22, 11, 1},    {23, 11, 1},    {24, 11, 1},    {25, 11, 1},
    {26, 11, 1},    {27, 11, 1},    {28, 11, 1},    {29, 11, 1},    {30, 11, 1},    {31, 11, 1},    {32, 11, 1},    {33, 11, 1},    {34, 11, 1},
    {35, 11, 1},    {36, 11, 1},    {37, 11, 1},    {38, 11, 1},    {39, 11, 1},    {40, 11, 1},    {41, 11, 1},    {42, 11, 1},    {43, 11, 1},
    {47, 11, 1},    {48, 11, 1},    {49, 11, 1},    {50, 11, 1},    {51, 11, 1},    {-1, 11, 1},    {-2, 11, 1},    {-3, 11, 1},    {-4, 11, 1},
    {-5, 11, 1},    {-6, 11, 1},    {-7, 11, 1},    {-8, 11, 1},    {-9, 11, 1},    {-10, 11, 1},   {-11, 11, 1},   {-12, 11, 1},   {-13, 11, 1},
    {-14, 11, 1},   {-15, 11, 1},   {-16, 11, 1},   {-17, 11, 1},   {0, 12, 1},     {1, 12, 1},     {2, 12, 1},     {3, 12, 1},     {4, 12, 1},
    {5, 12, 1},     {6, 12, 1},     {7, 12, 1},     {8, 12, 1},     {9, 12, 1},     {10, 12, 1},    {11, 12, 1},    {12, 12, 1},    {13, 12, 1},
    {14, 12, 1},    {15, 12, 1},    {16, 12, 1},    {17, 12, 1},    {18, 12, 1},    {19, 12, 1},    {20, 12, 1},    {21, 12, 1},    {22, 12, 1},
    {23, 12, 1},    {24, 12, 1},    {25, 12, 1},    {26, 12, 1},    {27, 12, 1},    {28, 12, 1},    {29, 12, 1},    {30, 12, 1},    {31, 12, 1},
    {32, 12, 1},    {33, 12, 1},    {34, 12, 1},    {35, 12, 1},    {36, 12, 1},    {37, 12, 1},    {38, 12, 1},    {39, 12, 1},    {40, 12, 1},
    {41, 12, 1},    {42, 12, 1},    {43, 12, 1},    {44, 12, 1},    {45, 12, 1},    {52, 12, 1},    {53, 12, 1},    {54, 12, 1},    {-1, 12, 1},
    {-2, 12, 1},    {-3, 12, 1},    {-4, 12, 1},    {-5, 12, 1},    {-6, 12, 1},    {-7, 12, 1},    {-8, 12, 1},    {-9, 12, 1},    {-10, 12, 1},
    {-11, 12, 1},   {-12, 12, 1},   {-13, 12, 1},   {-14, 12, 1},   {-15, 12, 1},   {-16, 12, 1},   {-17, 12, 1},   {0, 13, 1},     {1, 13, 1},
    {2, 13, 1},     {3, 13, 1},     {4, 13, 1},     {5, 13, 1},     {6, 13, 1},     {7, 13, 1},     {8, 13, 1},     {9, 13, 1},     {10, 13, 1},
    {11, 13, 1},    {12, 13, 1},    {13, 13, 1},    {14, 13, 1},    {15, 13, 1},    {16, 13, 1},    {17, 13, 1},    {18, 13, 1},    {19, 13, 1},
    {20, 13, 1},    {21, 13, 1},    {22, 13, 1},    {23, 13, 1},    {24, 13, 1},    {25, 13, 1},    {26, 13, 1},    {27, 13, 1},    {28, 13, 1},
    {29, 13, 1},    {30, 13, 1},    {31, 13, 1},    {32, 13, 1},    {33, 13, 1},    {34, 13, 1},    {35, 13, 1},    {36, 13, 1},    {37, 13, 1},
    {38, 13, 1},    {39, 13, 1},    {40, 13, 1},    {41, 13, 1},    {42, 13, 1},    {43, 13, 1},    {44, 13, 1},    {45, 13, 1},    {46, 13, 1},
    {47, 13, 1},    {48, 13, 1},    {-1, 13, 1},    {-2, 13, 1},    {-3, 13, 1},    {-4, 13, 1},    {-5, 13, 1},    {-6, 13, 1},    {-7, 13, 1},
    {-8, 13, 1},    {-9, 13, 1},    {-10, 13, 1},   {-11, 13, 1},   {-12, 13, 1},   {-13, 13, 1},   {-14, 13, 1},   {-15, 13, 1},   {-16, 13, 1},
    {-17, 13, 1},   {0, 14, 1},     {1, 14, 1},     {2, 14, 1},     {3, 14, 1},     {4, 14, 1},     {5, 14, 1},     {6, 14, 1},     {7, 14, 1},
    {8, 14, 1},     {9, 14, 1},     {10, 14, 1},    {11, 14, 1},    {12, 14, 1},    {13, 14, 1},    {14, 14, 1},    {15, 14, 1},    {16, 14, 1},
    {17, 14, 1},    {18, 14, 1},    {19, 14, 1},    {20, 14, 1},    {21, 14, 1},    {22, 14, 1},    {23, 14, 1},    {24, 14, 1},    {25, 14, 1},
    {26, 14, 1},    {27, 14, 1},    {28, 14, 1},    {29, 14, 1},    {30, 14, 1},    {31, 14, 1},    {32, 14, 1},    {33, 14, 1},    {34, 14, 1},
    {35, 14, 1},    {36, 14, 1},    {37, 14, 1},    {38, 14, 1},    {39, 14, 1},    {40, 14, 1},    {41, 14, 1},    {42, 14, 1},    {43, 14, 1},
    {44, 14, 1},    {45, 14, 1},    {46, 14, 1},    {47, 14, 1},    {48, 14, 1},    {49, 14, 1},    {50, 14, 1},    {-1, 14, 1},    {-2, 14, 1},
    {-3, 14, 1},    {-4, 14, 1},    {-5, 14, 1},    {-6, 14, 1},    {-7, 14, 1},    {-8, 14, 1},    {-9, 14, 1},    {-10, 14, 1},   {-11, 14, 1},
    {-12, 14, 1},   {-13, 14, 1},   {-14, 14, 1},   {-15, 14, 1},   {-16, 14, 1},   {-17, 14, 1},   {-18, 14, 1},   {-24, 14, 1},   {-25, 14, 1},
    {0, 15, 1},     {1, 15, 1},     {2, 15, 1},     {3, 15, 1},     {4, 15, 1},     {5, 15, 1},     {6, 15, 1},     {7, 15, 1},     {8, 15, 1},
    {9, 15, 1},     {10, 15, 1},    {11, 15, 1},    {12, 15, 1},    {13, 15, 1},    {14, 15, 1},    {15, 15, 1},    {16, 15, 1},    {17, 15, 1},
    {18, 15, 1},    {19, 15, 1},    {20, 15, 1},    {21, 15, 1},    {22, 15, 1},    {23, 15, 1},    {24, 15, 1},    {25, 15, 1},    {26, 15, 1},
    {27, 15, 1},    {28, 15, 1},    {29, 15, 1},    {30, 15, 1},    {31, 15, 1},    {32, 15, 1},    {33, 15, 1},    {34, 15, 1},    {35, 15, 1},
    {36, 15, 1},    {37, 15, 1},    {38, 15, 1},    {39, 15, 1},    {40, 15, 1},    {41, 15, 1},    {42, 15, 1},    {43, 15, 1},    {44, 15, 1},
    {45, 15, 1},    {46, 15, 1},    {47, 15, 1},    {48, 15, 1},    {49, 15, 1},    {50, 15, 1},    {51, 15, 1},    {52, 15, 1},    {-1, 15, 1},
    {-2, 15, 1},    {-3, 15, 1},    {-4, 15, 1},    {-5, 15, 1},    {-6, 15, 1},    {-7, 15, 1},    {-8, 15, 1},    {-9, 15, 1},    {-10, 15, 1},
    {-11, 15, 1},   {-12, 15, 1},   {-13, 15, 1},   {-14, 15, 1},   {-15, 15, 1},   {-16, 15, 1},   {-17, 15, 1},   {-18, 15, 1},   {-23, 15, 1},
    {-24, 15, 1},   {-25, 15, 1},   {0, 16, 1},     {1, 16, 1},     {2, 16, 1},     {3, 16, 1},     {4, 16, 1},     {5, 16, 1},     {6, 16, 1},
    {7, 16, 1},     {8, 16, 1},     {9, 16, 1},     {10, 16, 1},    {11, 16, 1},    {12, 16, 1},    {13, 16, 1},    {14, 16, 1},    {15, 16, 1},
    {16, 16, 1},    {17, 16, 1},    {18, 16, 1},    {19, 16, 1},    {20, 16, 1},    {21, 16, 1},    {22, 16, 1},    {23, 16, 1},    {24, 16, 1},
    {25, 16, 1},    {26, 16, 1},    {27, 16, 1},    {28, 16, 1},    {29, 16, 1},    {30, 16, 1},    {31, 16, 1},    {32, 16, 1},    {33, 16, 1},
    {34, 16, 1},    {35, 16, 1},    {36, 16, 1},    {37, 16, 1},    {38, 16, 1},    {39, 16, 1},    {40, 16, 1},    {41, 16, 1},    {42, 16, 1},
    {43, 16, 1},    {44, 16, 1},    {45, 16, 1},    {46, 16, 1},    {47, 16, 1},    {48, 16, 1},    {49, 16, 1},    {50, 16, 1},    {51, 16, 1},
    {52, 16, 1},    {53, 16, 1},    {54, 16, 1},    {55, 16, 1},    {-1, 16, 1},    {-2, 16, 1},    {-3, 16, 1},    {-4, 16, 1},    {-5, 16, 1},
    {-6, 16, 1},    {-7, 16, 1},    {-8, 16, 1},    {-9, 16, 1},    {-10, 16, 1},   {-11, 16, 1},   {-12, 16, 1},   {-13, 16, 1},   {-14, 16, 1},
    {-15, 16, 1},   {-16, 16, 1},   {-17, 16, 1},   {-23, 16, 1},   {-25, 16, 1},   {-26, 16, 1},   {0, 17, 1},     {1, 17, 1},     {2, 17, 1},
    {3, 17, 1},     {4, 17, 1},     {5, 17, 1},     {6, 17, 1},     {7, 17, 1},     {8, 17, 1},     {9, 17, 1},     {10, 17, 1},    {11, 17, 1},
    {12, 17, 1},    {13, 17, 1},    {14, 17, 1},    {15, 17, 1},    {16, 17, 1},    {17, 17, 1},    {18, 17, 1},    {19, 17, 1},    {20, 17, 1},
    {21, 17, 1},    {22, 17, 1},    {23, 17, 1},    {24, 17, 1},    {25, 17, 1},    {26, 17, 1},    {27, 17, 1},    {28, 17, 1},    {29, 17, 1},
    {30, 17, 1},    {31, 17, 1},    {32, 17, 1},    {33, 17, 1},    {34, 17, 1},    {35, 17, 1},    {36, 17, 1},    {37, 17, 1},    {38, 17, 1},
    {39, 17, 1},    {41, 17, 1},    {42, 17, 1},    {43, 17, 1},    {44, 17, 1},    {45, 17, 1},    {46, 17, 1},    {47, 17, 1},    {48, 17, 1},
    {49, 17, 1},    {50, 17, 1},    {51, 17, 1},    {52, 17, 1},    {53, 17, 1},    {54, 17, 1},    {55, 17, 1},    {56, 17, 1},    {-1, 17, 1},
    {-2, 17, 1},    {-3, 17, 1},    {-4, 17, 1},    {-5, 17, 1},    {-6, 17, 1},    {-7, 17, 1},    {-8, 17, 1},    {-9, 17, 1},    {-10, 17, 1},
    {-11, 17, 1},   {-12, 17, 1},   {-13, 17, 1},   {-14, 17, 1},   {-15, 17, 1},   {-16, 17, 1},   {-17, 17, 1},   {-25, 17, 1},   {-26, 17, 1},
    {0, 18, 1},     {1, 18, 1},     {2, 18, 1},     {3, 18, 1},     {4, 18, 1},     {5, 18, 1},     {6, 18, 1},     {7, 18, 1},     {8, 18, 1},
    {9, 18, 1},     {10, 18, 1},    {11, 18, 1},    {12, 18, 1},    {13, 18, 1},    {14, 18, 1},    {15, 18, 1},    {16, 18, 1},    {17, 18, 1},
    {18, 18, 1},    {19, 18, 1},    {20, 18, 1},    {21, 18, 1},    {22, 18, 1},    {23, 18, 1},    {24, 18, 1},    {25, 18, 1},    {26, 18, 1},
    {27, 18, 1},    {28, 18, 1},    {29, 18, 1},    {30, 18, 1},    {31, 18, 1},    {32, 18, 1},    {33, 18, 1},    {34, 18, 1},    {35, 18, 1},
    {36, 18, 1},    {37, 18, 1},    {38, 18, 1},    {40, 18, 1},    {41, 18, 1},    {42, 18, 1},    {43, 18, 1},    {44, 18, 1},    {45, 18, 1},
    {46, 18, 1},    {47, 18, 1},    {48, 18, 1},    {49, 18, 1},    {50, 18, 1},    {51, 18, 1},    {52, 18, 1},    {53, 18, 1},    {54, 18, 1},
    {55, 18, 1},    {56, 18, 1},    {57, 18, 1},    {-1, 18, 1},    {-2, 18, 1},    {-3, 18, 1},    {-4, 18, 1},    {-5, 18, 1},    {-6, 18, 1},
    {-7, 18, 1},    {-8, 18, 1},    {-9, 18, 1},    {-10, 18, 1},   {-11, 18, 1},   {-12, 18, 1},   {-13, 18, 1},   {-14, 18, 1},   {-15, 18, 1},
    {-16, 18, 1},   {-17, 18, 1},   {0, 19, 1},     {1, 19, 1},     {2, 19, 1},     {3, 19, 1},     {4, 19, 1},     {5, 19, 1},     {6, 19, 1},
    {7, 19, 1},     {8, 19, 1},     {9, 19, 1},     {10, 19, 1},    {11, 19, 1},    {12, 19, 1},    {13, 19, 1},    {14, 19, 1},    {15, 19, 1},
    {16, 19, 1},    {17, 19, 1},    {18, 19, 1},    {19, 19, 1},    {20, 19, 1},    {21, 19, 1},    {22, 19, 1},    {23, 19, 1},    {24, 19, 1},
    {25, 19, 1},    {26, 19, 1},    {27, 19, 1},    {28, 19, 1},    {29, 19, 1},    {30, 19, 1},    {31, 19, 1},    {32, 19, 1},    {33, 19, 1},
    {34, 19, 1},    {35, 19, 1},    {36, 19, 1},    {37, 19, 1},    {38, 19, 1},    {39, 19, 1},    {40, 19, 1},    {41, 19, 1},    {42, 19, 1},
    {43, 19, 1},    {44, 19, 1},    {45, 19, 1},    {46, 19, 1},    {47, 19, 1},    {48, 19, 1},    {49, 19, 1},    {50, 19, 1},    {51, 19, 1},
    {52, 19, 1},    {53, 19, 1},    {54, 19, 1},    {55, 19, 1},    {56, 19, 1},    {57, 19, 1},    {-1, 19, 1},    {-2, 19, 1},    {-3, 19, 1},
    {-4, 19, 1},    {-5, 19, 1},    {-6, 19, 1},    {-7, 19, 1},    {-8, 19, 1},    {-9, 19, 1},    {-10, 19, 1},   {-11, 19, 1},   {-12, 19, 1},
    {-13, 19, 1},   {-14, 19, 1},   {-15, 19, 1},   {-16, 19, 1},   {-17, 19, 1},   {0, 20, 1},     {1, 20, 1},     {2, 20, 1},     {3, 20, 1},
    {4, 20, 1},     {5, 20, 1},     {6, 20, 1},     {7, 20, 1},     {8, 20, 1},     {9, 20, 1},     {10, 20, 1},    {11, 20, 1},    {12, 20, 1},
    {13, 20, 1},    {14, 20, 1},    {15, 20, 1},    {16, 20, 1},    {17, 20, 1},    {18, 20, 1},    {19, 20, 1},    {20, 20, 1},    {21, 20, 1},
    {22, 20, 1},    {23, 20, 1},    {24, 20, 1},    {25, 20, 1},    {26, 20, 1},    {27, 20, 1},    {28, 20, 1},    {29, 20, 1},    {30, 20, 1},
    {31, 20, 1},    {32, 20, 1},    {33, 20, 1},    {34, 20, 1},    {35, 20, 1},    {36, 20, 1},    {37, 20, 1},    {39, 20, 1},    {40, 20, 1},
    {41, 20, 1},    {42, 20, 1},    {43, 20, 1},    {44, 20, 1},    {45, 20, 1},    {46, 20, 1},    {47, 20, 1},    {48, 20, 1},    {49, 20, 1},
    {50, 20, 1},    {51, 20, 1},    {52, 20, 1},    {53, 20, 1},    {54, 20, 1},    {55, 20, 1},    {56, 20, 1},    {57, 20, 1},    {58, 20, 1},
    {-1, 20, 1},    {-2, 20, 1},    {-3, 20, 1},    {-4, 20, 1},    {-5, 20, 1},    {-6, 20, 1},    {-7, 20, 1},    {-8, 20, 1},    {-9, 20, 1},
    {-10, 20, 1},   {-11, 20, 1},   {-12, 20, 1},   {-13, 20, 1},   {-14, 20, 1},   {-15, 20, 1},   {-16, 20, 1},   {-17, 20, 1},   {-18, 20, 1},
    {0, 21, 1},     {1, 21, 1},     {2, 21, 1},     {3, 21, 1},     {4, 21, 1},     {5, 21, 1},     {6, 21, 1},     {7, 21, 1},     {8, 21, 1},
    {9, 21, 1},     {10, 21, 1},    {11, 21, 1},    {12, 21, 1},    {13, 21, 1},    {14, 21, 1},    {15, 21, 1},    {16, 21, 1},    {17, 21, 1},
    {18, 21, 1},    {19, 21, 1},    {20, 21, 1},    {21, 21, 1},    {22, 21, 1},    {23, 21, 1},    {24, 21, 1},    {25, 21, 1},    {26, 21, 1},
    {27, 21, 1},    {28, 21, 1},    {29, 21, 1},    {30, 21, 1},    {31, 21, 1},    {32, 21, 1},    {33, 21, 1},    {34, 21, 1},    {35, 21, 1},
    {36, 21, 1},    {37, 21, 1},    {38, 21, 1},    {39, 21, 1},    {40, 21, 1},    {41, 21, 1},    {42, 21, 1},    {43, 21, 1},    {44, 21, 1},
    {45, 21, 1},    {46, 21, 1},    {47, 21, 1},    {48, 21, 1},    {49, 21, 1},    {50, 21, 1},    {51, 21, 1},    {52, 21, 1},    {53, 21, 1},
    {54, 21, 1},    {55, 21, 1},    {56, 21, 1},    {57, 21, 1},    {58, 21, 1},    {59, 21, 1},    {-1, 21, 1},    {-2, 21, 1},    {-3, 21, 1},
    {-4, 21, 1},    {-5, 21, 1},    {-6, 21, 1},    {-7, 21, 1},    {-8, 21, 1},    {-9, 21, 1},    {-10, 21, 1},   {-11, 21, 1},   {-12, 21, 1},
    {-13, 21, 1},   {-14, 21, 1},   {-15, 21, 1},   {-16, 21, 1},   {-17, 21, 1},   {-18, 21, 1},   {0, 22, 1},     {1, 22, 1},     {2, 22, 1},
    {3, 22, 1},     {4, 22, 1},     {5, 22, 1},     {6, 22, 1},     {7, 22, 1},     {8, 22, 1},     {9, 22, 1},     {10, 22, 1},    {11, 22, 1},
    {12, 22, 1},    {13, 22, 1},    {14, 22, 1},    {15, 22, 1},    {16, 22, 1},    {17, 22, 1},    {18, 22, 1},    {19, 22, 1},    {20, 22, 1},
    {21, 22, 1},    {22, 22, 1},    {23, 22, 1},    {24, 22, 1},    {25, 22, 1},    {26, 22, 1},    {27, 22, 1},    {28, 22, 1},    {29, 22, 1},
    {30, 22, 1},    {31, 22, 1},    {32, 22, 1},    {33, 22, 1},    {34, 22, 1},    {35, 22, 1},    {36, 22, 1},    {38, 22, 1},    {39, 22, 1},
    {40, 22, 1},    {41, 22, 1},    {42, 22, 1},    {43, 22, 1},    {44, 22, 1},    {45, 22, 1},    {46, 22, 1},    {47, 22, 1},    {48, 22, 1},
    {49, 22, 1},    {50, 22, 1},    {51, 22, 1},    {52, 22, 1},    {53, 22, 1},    {54, 22, 1},    {55, 22, 1},    {56, 22, 1},    {57, 22, 1},
    {58, 22, 1},    {59, 22, 1},    {-1, 22, 1},    {-2, 22, 1},    {-3, 22, 1},    {-4, 22, 1},    {-5, 22, 1},    {-6, 22, 1},    {-7, 22, 1},
    {-8, 22, 1},    {-9, 22, 1},    {-10, 22, 1},   {-11, 22, 1},   {-12, 22, 1},   {-13, 22, 1},   {-14, 22, 1},   {-15, 22, 1},   {-16, 22, 1},
    {-17, 22, 1},   {0, 23, 1},     {1, 23, 1},     {2, 23, 1},     {3, 23, 1},     {4, 23, 1},     {5, 23, 1},     {6, 23, 1},     {7, 23, 1},
    {8, 23, 1},     {9, 23, 1},     {10, 23, 1},    {11, 23, 1},    {12, 23, 1},    {13, 23, 1},    {14, 23, 1},    {15, 23, 1},    {16, 23, 1},
    {17, 23, 1},    {18, 23, 1},    {19, 23, 1},    {20, 23, 1},    {21, 23, 1},    {22, 23, 1},    {23, 23, 1},    {24, 23, 1},    {25, 23, 1},
    {26, 23, 1},    {27, 23, 1},    {28, 23, 1},    {29, 23, 1},    {30, 23, 1},    {31, 23, 1},    {32, 23, 1},    {33, 23, 1},    {34, 23, 1},
    {35, 23, 1},    {36, 23, 1},    {38, 23, 1},    {39, 23, 1},    {40, 23, 1},    {41, 23, 1},    {42, 23, 1},    {43, 23, 1},    {44, 23, 1},
    {45, 23, 1},    {46, 23, 1},    {47, 23, 1},    {48, 23, 1},    {49, 23, 1},    {50, 23, 1},    {51, 23, 1},    {52, 23, 1},    {53, 23, 1},
    {54, 23, 1},    {55, 23, 1},    {56, 23, 1},    {57, 23, 1},    {58, 23, 1},    {59, 23, 1},    {-1, 23, 1},    {-2, 23, 1},    {-3, 23, 1},
    {-4, 23, 1},    {-5, 23, 1},    {-6, 23, 1},    {-7, 23, 1},    {-8, 23, 1},    {-9, 23, 1},    {-10, 23, 1},   {-11, 23, 1},   {-12, 23, 1},
    {-13, 23, 1},   {-14, 23, 1},   {-15, 23, 1},   {-16, 23, 1},   {-17, 23, 1},   {0, 24, 1},     {1, 24, 1},     {2, 24, 1},     {3, 24, 1},
    {4, 24, 1},     {5, 24, 1},     {6, 24, 1},     {7, 24, 1},     {8, 24, 1},     {9, 24, 1},     {10, 24, 1},    {11, 24, 1},    {12, 24, 1},
    {13, 24, 1},    {14, 24, 1},    {15, 24, 1},    {16, 24, 1},    {17, 24, 1},    {18, 24, 1},    {19, 24, 1},    {20, 24, 1},    {21, 24, 1},
    {22, 24, 1},    {23, 24, 1},    {24, 24, 1},    {25, 24, 1},    {26, 24, 1},    {27, 24, 1},    {28, 24, 1},    {29, 24, 1},    {30, 24, 1},
    {31, 24, 1},    {32, 24, 1},    {33, 24, 1},    {34, 24, 1},    {35, 24, 1},    {37, 24, 1},    {38, 24, 1},    {39, 24, 1},    {40, 24, 1},
    {41, 24, 1},    {42, 24, 1},    {43, 24, 1},    {44, 24, 1},    {45, 24, 1},    {46, 24, 1},    {47, 24, 1},    {48, 24, 1},    {49, 24, 1},
    {50, 24, 1},    {51, 24, 1},    {52, 24, 1},    {53, 24, 1},    {54, 24, 1},    {55, 24, 1},    {56, 24, 1},    {57, 24, 1},    {-1, 24, 1},
    {-2, 24, 1},    {-3, 24, 1},    {-4, 24, 1},    {-5, 24, 1},    {-6, 24, 1},    {-7, 24, 1},    {-8, 24, 1},    {-9, 24, 1},    {-10, 24, 1},
    {-11, 24, 1},   {-12, 24, 1},   {-13, 24, 1},   {-14, 24, 1},   {-15, 24, 1},   {-16, 24, 1},   {0, 25, 1},     {1, 25, 1},     {2, 25, 1},
    {3, 25, 1},     {4, 25, 1},     {5, 25, 1},     {6, 25, 1},     {7, 25, 1},     {8, 25, 1},     {9, 25, 1},     {10, 25, 1},    {11, 25, 1},
    {12, 25, 1},    {13, 25, 1},    {14, 25, 1},    {15, 25, 1},    {16, 25, 1},    {17, 25, 1},    {18, 25, 1},    {19, 25, 1},    {20, 25, 1},
    {21, 25, 1},    {22, 25, 1},    {23, 25, 1},    {24, 25, 1},    {25, 25, 1},    {26, 25, 1},    {27, 25, 1},    {28, 25, 1},    {29, 25, 1},
    {30, 25, 1},    {31, 25, 1},    {32, 25, 1},    {33, 25, 1},    {34, 25, 1},    {36, 25, 1},    {37, 25, 1},    {38, 25, 1},    {39, 25, 1},
    {40, 25, 1},    {41, 25, 1},    {42, 25, 1},    {43, 25, 1},    {44, 25, 1},    {45, 25, 1},    {46, 25, 1},    {47, 25, 1},    {48, 25, 1},
    {49, 25, 1},    {50, 25, 1},    {51, 25, 1},    {52, 25, 1},    {54, 25, 1},    {55, 25, 1},    {56, 25, 1},    {57, 25, 1},    {58, 25, 1},
    {59, 25, 1},    {-1, 25, 1},    {-2, 25, 1},    {-3, 25, 1},    {-4, 25, 1},    {-5, 25, 1},    {-6, 25, 1},    {-7, 25, 1},    {-8, 25, 1},
    {-9, 25, 1},    {-10, 25, 1},   {-11, 25, 1},   {-12, 25, 1},   {-13, 25, 1},   {-14, 25, 1},   {-15, 25, 1},   {0, 26, 1},     {1, 26, 1},
    {2, 26, 1},     {3, 26, 1},     {4, 26, 1},     {5, 26, 1},     {6, 26, 1},     {7, 26, 1},     {8, 26, 1},     {9, 26, 1},     {10, 26, 1},
    {11, 26, 1},    {12, 26, 1},    {13, 26, 1},    {14, 26, 1},    {15, 26, 1},    {16, 26, 1},    {17, 26, 1},    {18, 26, 1},    {19, 26, 1},
    {20, 26, 1},    {21, 26, 1},    {22, 26, 1},    {23, 26, 1},    {24, 26, 1},    {25, 26, 1},    {26, 26, 1},    {27, 26, 1},    {28, 26, 1},
    {29, 26, 1},    {30, 26, 1},    {31, 26, 1},    {32, 26, 1},    {33, 26, 1},    {34, 26, 1},    {35, 26, 1},    {36, 26, 1},    {37, 26, 1},
    {38, 26, 1},    {39, 26, 1},    {40, 26, 1},    {41, 26, 1},    {42, 26, 1},    {43, 26, 1},    {44, 26, 1},    {45, 26, 1},    {46, 26, 1},
    {47, 26, 1},    {48, 26, 1},    {49, 26, 1},    {50, 26, 1},    {51, 26, 1},    {53, 26, 1},    {54, 26, 1},    {55, 26, 1},    {56, 26, 1},
    {57, 26, 1},    {58, 26, 1},    {59, 26, 1},    {-1, 26, 1},    {-2, 26, 1},    {-3, 26, 1},    {-4, 26, 1},    {-5, 26, 1},    {-6, 26, 1},
    {-7, 26, 1},    {-8, 26, 1},    {-9, 26, 1},    {-10, 26, 1},   {-11, 26, 1},   {-12, 26, 1},   {-13, 26, 1},   {-14, 26, 1},   {-15, 26, 1},
    {0, 27, 1},     {1, 27, 1},     {2, 27, 1},     {3, 27, 1},     {4, 27, 1},     {5, 27, 1},     {6, 27, 1},     {7, 27, 1},     {8, 27, 1},
    {9, 27, 1},     {10, 27, 1},    {11, 27, 1},    {12, 27, 1},    {13, 27, 1},    {14, 27, 1},    {15, 27, 1},    {16, 27, 1},    {17, 27, 1},
    {18, 27, 1},    {19, 27, 1},    {20, 27, 1},    {21, 27, 1},    {22, 27, 1},    {23, 27, 1},    {24, 27, 1},    {25, 27, 1},    {26, 27, 1},
    {27, 27, 1},    {28, 27, 1},    {29, 27, 1},    {30, 27, 1},    {31, 27, 1},    {32, 27, 1},    {33, 27, 1},    {34, 27, 1},    {35, 27, 1},
    {36, 27, 1},    {37, 27, 1},    {38, 27, 1},    {39, 27, 1},    {40, 27, 1},    {41, 27, 1},    {42, 27, 1},    {43, 27, 1},    {44, 27, 1},
    {45, 27, 1},    {46, 27, 1},    {47, 27, 1},    {48, 27, 1},    {49, 27, 1},    {50, 27, 1},    {51, 27, 1},    {52, 27, 1},    {53, 27, 1},
    {54, 27, 1},    {55, 27, 1},    {56, 27, 1},    {57, 27, 1},    {58, 27, 1},    {59, 27, 1},    {-1, 27, 1},    {-2, 27, 1},    {-3, 27, 1},
    {-4, 27, 1},    {-5, 27, 1},    {-6, 27, 1},    {-7, 27, 1},    {-8, 27, 1},    {-9, 27, 1},    {-10, 27, 1},   {-11, 27, 1},   {-12, 27, 1},
    {-13, 27, 1},   {-14, 27, 1},   {-16, 27, 1},   {-17, 27, 1},   {-18, 27, 1},   {-19, 27, 1},   {0, 28, 1},     {1, 28, 1},     {2, 28, 1},
    {3, 28, 1},     {4, 28, 1},     {5, 28, 1},     {6, 28, 1},     {7, 28, 1},     {8, 28, 1},     {9, 28, 1},     {10, 28, 1},    {11, 28, 1},
    {12, 28, 1},    {13, 28, 1},    {14, 28, 1},    {15, 28, 1},    {16, 28, 1},    {17, 28, 1},    {18, 28, 1},    {19, 28, 1},    {20, 28, 1},
    {21, 28, 1},    {22, 28, 1},    {23, 28, 1},    {24, 28, 1},    {25, 28, 1},    {26, 28, 1},    {27, 28, 1},    {28, 28, 1},    {29, 28, 1},
    {30, 28, 1},    {31, 28, 1},    {32, 28, 1},    {33, 28, 1},    {34, 28, 1},    {35, 28, 1},    {36, 28, 1},    {37, 28, 1},    {38, 28, 1},
    {39, 28, 1},    {40, 28, 1},    {41, 28, 1},    {42, 28, 1},    {43, 28, 1},    {44, 28, 1},    {45, 28, 1},    {46, 28, 1},    {47, 28, 1},
    {48, 28, 1},    {50, 28, 1},    {51, 28, 1},    {52, 28, 1},    {53, 28, 1},    {54, 28, 1},    {55, 28, 1},    {56, 28, 1},    {57, 28, 1},
    {58, 28, 1},    {59, 28, 1},    {-1, 28, 1},    {-2, 28, 1},    {-3, 28, 1},    {-4, 28, 1},    {-5, 28, 1},    {-6, 28, 1},    {-7, 28, 1},
    {-8, 28, 1},    {-9, 28, 1},    {-10, 28, 1},   {-11, 28, 1},   {-12, 28, 1},   {-13, 28, 1},   {-14, 28, 1},   {-15, 28, 1},   {-16, 28, 1},
    {-17, 28, 1},   {-18, 28, 1},   {-19, 28, 1},   {0, 29, 1},     {1, 29, 1},     {2, 29, 1},     {3, 29, 1},     {4, 29, 1},     {5, 29, 1},
    {6, 29, 1},     {7, 29, 1},     {8, 29, 1},     {9, 29, 1},     {10, 29, 1},    {11, 29, 1},    {12, 29, 1},    {13, 29, 1},    {14, 29, 1},
    {15, 29, 1},    {16, 29, 1},    {17, 29, 1},    {18, 29, 1},    {19, 29, 1},    {20, 29, 1},    {21, 29, 1},    {22, 29, 1},    {23, 29, 1},
    {24, 29, 1},    {25, 29, 1},    {26, 29, 1},    {27, 29, 1},    {28, 29, 1},    {29, 29, 1},    {30, 29, 1},    {31, 29, 1},    {32, 29, 1},
    {33, 29, 1},    {34, 29, 1},    {35, 29, 1},    {36, 29, 1},    {37, 29, 1},    {38, 29, 1},    {39, 29, 1},    {40, 29, 1},    {41, 29, 1},
    {42, 29, 1},    {43, 29, 1},    {44, 29, 1},    {45, 29, 1},    {46, 29, 1},    {47, 29, 1},    {48, 29, 1},    {49, 29, 1},    {50, 29, 1},
    {51, 29, 1},    {52, 29, 1},    {53, 29, 1},    {54, 29, 1},    {55, 29, 1},    {56, 29, 1},    {57, 29, 1},    {58, 29, 1},    {59, 29, 1},
    {-1, 29, 1},    {-2, 29, 1},    {-3, 29, 1},    {-4, 29, 1},    {-5, 29, 1},    {-6, 29, 1},    {-7, 29, 1},    {-8, 29, 1},    {-9, 29, 1},
    {-10, 29, 1},   {-11, 29, 1},   {-14, 29, 1},   {0, 30, 1},     {1, 30, 1},     {2, 30, 1},     {3, 30, 1},     {4, 30, 1},     {5, 30, 1},
    {6, 30, 1},     {7, 30, 1},     {8, 30, 1},     {9, 30, 1},     {10, 30, 1},    {11, 30, 1},    {12, 30, 1},    {13, 30, 1},    {14, 30, 1},
    {15, 30, 1},    {16, 30, 1},    {17, 30, 1},    {18, 30, 1},    {19, 30, 1},    {20, 30, 1},    {21, 30, 1},    {22, 30, 1},    {23, 30, 1},
    {24, 30, 1},    {25, 30, 1},    {26, 30, 1},    {27, 30, 1},    {28, 30, 1},    {29, 30, 1},    {30, 30, 1},    {31, 30, 1},    {32, 30, 1},
    {33, 30, 1},    {34, 30, 1},    {35, 30, 1},    {36, 30, 1},    {37, 30, 1},    {38, 30, 1},    {39, 30, 1},    {40, 30, 1},    {41, 30, 1},
    {42, 30, 1},    {43, 30, 1},    {44, 30, 1},    {45, 30, 1},    {46, 30, 1},    {47, 30, 1},    {48, 30, 1},    {49, 30, 1},    {50, 30, 1},
    {51, 30, 1},    {52, 30, 1},    {53, 30, 1},    {54, 30, 1},    {55, 30, 1},    {56, 30, 1},    {57, 30, 1},    {58, 30, 1},    {59, 30, 1},
    {-1, 30, 1},    {-2, 30, 1},    {-3, 30, 1},    {-4, 30, 1},    {-5, 30, 1},    {-6, 30, 1},    {-7, 30, 1},    {-8, 30, 1},    {-9, 30, 1},
    {-10, 30, 1},   {-16, 30, 1},   {-17, 30, 1},   {0, 31, 1},     {1, 31, 1},     {2, 31, 1},     {3, 31, 1},     {4, 31, 1},     {5, 31, 1},
    {6, 31, 1},     {7, 31, 1},     {8, 31, 1},     {9, 31, 1},     {10, 31, 1},    {11, 31, 1},    {12, 31, 1},    {13, 31, 1},    {14, 31, 1},
    {15, 31, 1},    {16, 31, 1},    {17, 31, 1},    {19, 31, 1},    {20, 31, 1},    {21, 31, 1},    {22, 31, 1},    {23, 31, 1},    {24, 31, 1},
    {25, 31, 1},    {26, 31, 1},    {27, 31, 1},    {28, 31, 1},    {29, 31, 1},    {30, 31, 1},    {31, 31, 1},    {32, 31, 1},    {33, 31, 1},
    {34, 31, 1},    {35, 31, 1},    {36, 31, 1},    {37, 31, 1},    {38, 31, 1},    {39, 31, 1},    {40, 31, 1},    {41, 31, 1},    {42, 31, 1},
    {43, 31, 1},    {44, 31, 1},    {45, 31, 1},    {46, 31, 1},    {47, 31, 1},    {48, 31, 1},    {49, 31, 1},    {50, 31, 1},    {51, 31, 1},
    {52, 31, 1},    {53, 31, 1},    {54, 31, 1},    {55, 31, 1},    {56, 31, 1},    {57, 31, 1},    {58, 31, 1},    {59, 31, 1},    {-1, 31, 1},
    {-2, 31, 1},    {-3, 31, 1},    {-4, 31, 1},    {-5, 31, 1},    {-6, 31, 1},    {-7, 31, 1},    {-8, 31, 1},    {-9, 31, 1},    {-10, 31, 1},
    {0, 32, 1},     {1, 32, 1},     {2, 32, 1},     {3, 32, 1},     {4, 32, 1},     {5, 32, 1},     {6, 32, 1},     {7, 32, 1},     {8, 32, 1},
    {9, 32, 1},     {10, 32, 1},    {11, 32, 1},    {12, 32, 1},    {13, 32, 1},    {14, 32, 1},    {15, 32, 1},    {19, 32, 1},    {20, 32, 1},
    {21, 32, 1},    {22, 32, 1},    {23, 32, 1},    {24, 32, 1},    {34, 32, 1},    {35, 32, 1},    {36, 32, 1},    {37, 32, 1},    {38, 32, 1},
    {39, 32, 1},    {40, 32, 1},    {41, 32, 1},    {42, 32, 1},    {43, 32, 1},    {44, 32, 1},    {45, 32, 1},    {46, 32, 1},    {47, 32, 1},
    {48, 32, 1},    {49, 32, 1},    {50, 32, 1},    {51, 32, 1},    {52, 32, 1},    {53, 32, 1},    {54, 32, 1},    {55, 32, 1},    {56, 32, 1},
    {57, 32, 1},    {58, 32, 1},    {59, 32, 1},    {-1, 32, 1},    {-2, 32, 1},    {-3, 32, 1},    {-4, 32, 1},    {-5, 32, 1},    {-6, 32, 1},
    {-7, 32, 1},    {-8, 32, 1},    {-9, 32, 1},    {-10, 32, 1},   {-17, 32, 1},   {-18, 32, 1},   {0, 33, 1},     {1, 33, 1},     {2, 33, 1},
    {3, 33, 1},     {4, 33, 1},     {5, 33, 1},     {6, 33, 1},     {7, 33, 1},     {8, 33, 1},     {9, 33, 1},     {10, 33, 1},    {11, 33, 1},
    {35, 33, 1},    {36, 33, 1},    {37, 33, 1},    {38, 33, 1},    {39, 33, 1},    {40, 33, 1},    {41, 33, 1},    {42, 33, 1},    {43, 33, 1},
    {44, 33, 1},    {45, 33, 1},    {46, 33, 1},    {47, 33, 1},    {48, 33, 1},    {49, 33, 1},    {50, 33, 1},    {51, 33, 1},    {52, 33, 1},
    {53, 33, 1},    {54, 33, 1},    {55, 33, 1},    {56, 33, 1},    {57, 33, 1},    {58, 33, 1},    {59, 33, 1},    {-1, 33, 1},    {-2, 33, 1},
    {-3, 33, 1},    {-4, 33, 1},    {-5, 33, 1},    {-6, 33, 1},    {-7, 33, 1},    {-8, 33, 1},    {-9, 33, 1},    {-17, 33, 1},   {0, 34, 1},
    {1, 34, 1},     {2, 34, 1},     {3, 34, 1},     {4, 34, 1},     {5, 34, 1},     {6, 34, 1},     {7, 34, 1},     {8, 34, 1},     {9, 34, 1},
    {10, 34, 1},    {11, 34, 1},    {23, 34, 1},    {24, 34, 1},    {25, 34, 1},    {26, 34, 1},    {32, 34, 1},    {33, 34, 1},    {34, 34, 1},
    {35, 34, 1},    {36, 34, 1},    {37, 34, 1},    {38, 34, 1},    {39, 34, 1},    {40, 34, 1},    {41, 34, 1},    {42, 34, 1},    {43, 34, 1},
    {44, 34, 1},    {45, 34, 1},    {46, 34, 1},    {47, 34, 1},    {48, 34, 1},    {49, 34, 1},    {50, 34, 1},    {51, 34, 1},    {52, 34, 1},
    {53, 34, 1},    {54, 34, 1},    {55, 34, 1},    {56, 34, 1},    {57, 34, 1},    {58, 34, 1},    {59, 34, 1},    {-1, 34, 1},    {-2, 34, 1},
    {-3, 34, 1},    {-4, 34, 1},    {-5, 34, 1},    {-6, 34, 1},    {-7, 34, 1},    {-26, 36, 1},   {-25, 37, 1},   {-26, 37, 1},   {-28, 38, 1},
    {-29, 38, 1},   {-28, 39, 1},   {-29, 39, 1},   {-32, 39, 1},   {6, -1, 1},     {8, -1, 1},     {9, -1, 1},     {10, -1, 1},    {11, -1, 1},
    {12, -1, 1},    {13, -1, 1},    {14, -1, 1},    {15, -1, 1},    {16, -1, 1},    {17, -1, 1},    {18, -1, 1},    {19, -1, 1},    {20, -1, 1},
    {21, -1, 1},    {22, -1, 1},    {23, -1, 1},    {24, -1, 1},    {25, -1, 1},    {26, -1, 1},    {27, -1, 1},    {28, -1, 1},    {29, -1, 1},
    {30, -1, 1},    {31, -1, 1},    {32, -1, 1},    {33, -1, 1},    {34, -1, 1},    {35, -1, 1},    {36, -1, 1},    {37, -1, 1},    {38, -1, 1},
    {39, -1, 1},    {40, -1, 1},    {41, -1, 1},    {42, -1, 1},    {5, -2, 1},     {8, -2, 1},     {9, -2, 1},     {10, -2, 1},    {11, -2, 1},
    {12, -2, 1},    {13, -2, 1},    {14, -2, 1},    {15, -2, 1},    {16, -2, 1},    {17, -2, 1},    {18, -2, 1},    {19, -2, 1},    {20, -2, 1},
    {21, -2, 1},    {22, -2, 1},    {23, -2, 1},    {24, -2, 1},    {25, -2, 1},    {26, -2, 1},    {27, -2, 1},    {28, -2, 1},    {29, -2, 1},
    {30, -2, 1},    {31, -2, 1},    {32, -2, 1},    {33, -2, 1},    {34, -2, 1},    {35, -2, 1},    {36, -2, 1},    {37, -2, 1},    {38, -2, 1},
    {39, -2, 1},    {40, -2, 1},    {41, -2, 1},    {42, -2, 1},    {9, -3, 1},     {10, -3, 1},    {11, -3, 1},    {12, -3, 1},    {13, -3, 1},
    {14, -3, 1},    {15, -3, 1},    {16, -3, 1},    {17, -3, 1},    {18, -3, 1},    {19, -3, 1},    {20, -3, 1},    {21, -3, 1},    {22, -3, 1},
    {23, -3, 1},    {24, -3, 1},    {25, -3, 1},    {26, -3, 1},    {27, -3, 1},    {28, -3, 1},    {29, -3, 1},    {30, -3, 1},    {31, -3, 1},
    {32, -3, 1},    {33, -3, 1},    {34, -3, 1},    {35, -3, 1},    {36, -3, 1},    {37, -3, 1},    {38, -3, 1},    {39, -3, 1},    {40, -3, 1},
    {41, -3, 1},    {10, -4, 1},    {11, -4, 1},    {12, -4, 1},    {13, -4, 1},    {14, -4, 1},    {15, -4, 1},    {16, -4, 1},    {17, -4, 1},
    {18, -4, 1},    {19, -4, 1},    {20, -4, 1},    {21, -4, 1},    {22, -4, 1},    {23, -4, 1},    {24, -4, 1},    {25, -4, 1},    {26, -4, 1},
    {27, -4, 1},    {28, -4, 1},    {29, -4, 1},    {30, -4, 1},    {31, -4, 1},    {32, -4, 1},    {33, -4, 1},    {34, -4, 1},    {35, -4, 1},
    {36, -4, 1},    {37, -4, 1},    {38, -4, 1},    {39, -4, 1},    {40, -4, 1},    {55, -4, 1},    {11, -5, 1},    {12, -5, 1},    {13, -5, 1},
    {14, -5, 1},    {15, -5, 1},    {16, -5, 1},    {17, -5, 1},    {18, -5, 1},    {19, -5, 1},    {20, -5, 1},    {21, -5, 1},    {22, -5, 1},
    {23, -5, 1},    {24, -5, 1},    {25, -5, 1},    {26, -5, 1},    {27, -5, 1},    {28, -5, 1},    {29, -5, 1},    {30, -5, 1},    {31, -5, 1},
    {32, -5, 1},    {33, -5, 1},    {34, -5, 1},    {35, -5, 1},    {36, -5, 1},    {37, -5, 1},    {38, -5, 1},    {39, -5, 1},    {53, -5, 1},
    {55, -5, 1},    {11, -6, 1},    {12, -6, 1},    {13, -6, 1},    {14, -6, 1},    {15, -6, 1},    {16, -6, 1},    {17, -6, 1},    {18, -6, 1},
    {19, -6, 1},    {20, -6, 1},    {21, -6, 1},    {22, -6, 1},    {23, -6, 1},    {24, -6, 1},    {25, -6, 1},    {26, -6, 1},    {27, -6, 1},
    {28, -6, 1},    {29, -6, 1},    {30, -6, 1},    {31, -6, 1},    {32, -6, 1},    {33, -6, 1},    {34, -6, 1},    {35, -6, 1},    {36, -6, 1},
    {37, -6, 1},    {38, -6, 1},    {39, -6, 1},    {53, -6, 1},    {55, -6, 1},    {12, -7, 1},    {13, -7, 1},    {14, -7, 1},    {15, -7, 1},
    {16, -7, 1},    {17, -7, 1},    {18, -7, 1},    {19, -7, 1},    {20, -7, 1},    {21, -7, 1},    {22, -7, 1},    {23, -7, 1},    {24, -7, 1},
    {25, -7, 1},    {26, -7, 1},    {27, -7, 1},    {28, -7, 1},    {29, -7, 1},    {30, -7, 1},    {31, -7, 1},    {32, -7, 1},    {33, -7, 1},
    {34, -7, 1},    {35, -7, 1},    {36, -7, 1},    {37, -7, 1},    {38, -7, 1},    {39, -7, 1},    {52, -7, 1},    {53, -7, 1},    {12, -8, 1},
    {13, -8, 1},    {14, -8, 1},    {15, -8, 1},    {16, -8, 1},    {17, -8, 1},    {18, -8, 1},    {19, -8, 1},    {20, -8, 1},    {21, -8, 1},
    {22, -8, 1},    {23, -8, 1},    {24, -8, 1},    {25, -8, 1},    {26, -8, 1},    {27, -8, 1},    {28, -8, 1},    {29, -8, 1},    {30, -8, 1},
    {31, -8, 1},    {32, -8, 1},    {33, -8, 1},    {34, -8, 1},    {35, -8, 1},    {36, -8, 1},    {37, -8, 1},    {38, -8, 1},    {39, -8, 1},
    {52, -8, 1},    {56, -8, 1},    {13, -9, 1},    {14, -9, 1},    {15, -9, 1},    {16, -9, 1},    {17, -9, 1},    {18, -9, 1},    {19, -9, 1},
    {20, -9, 1},    {21, -9, 1},    {22, -9, 1},    {23, -9, 1},    {24, -9, 1},    {25, -9, 1},    {26, -9, 1},    {27, -9, 1},    {28, -9, 1},
    {29, -9, 1},    {30, -9, 1},    {31, -9, 1},    {32, -9, 1},    {33, -9, 1},    {34, -9, 1},    {35, -9, 1},    {36, -9, 1},    {37, -9, 1},
    {38, -9, 1},    {39, -9, 1},    {12, -10, 1},   {13, -10, 1},   {14, -10, 1},   {15, -10, 1},   {16, -10, 1},   {17, -10, 1},   {18, -10, 1},
    {19, -10, 1},   {20, -10, 1},   {21, -10, 1},   {22, -10, 1},   {23, -10, 1},   {24, -10, 1},   {25, -10, 1},   {26, -10, 1},   {27, -10, 1},
    {28, -10, 1},   {29, -10, 1},   {30, -10, 1},   {31, -10, 1},   {32, -10, 1},   {33, -10, 1},   {34, -10, 1},   {35, -10, 1},   {36, -10, 1},
    {37, -10, 1},   {38, -10, 1},   {39, -10, 1},   {46, -10, 1},   {47, -10, 1},   {50, -10, 1},   {51, -10, 1},   {13, -11, 1},   {14, -11, 1},
    {15, -11, 1},   {16, -11, 1},   {17, -11, 1},   {18, -11, 1},   {19, -11, 1},   {20, -11, 1},   {21, -11, 1},   {22, -11, 1},   {23, -11, 1},
    {24, -11, 1},   {25, -11, 1},   {26, -11, 1},   {27, -11, 1},   {28, -11, 1},   {29, -11, 1},   {30, -11, 1},   {31, -11, 1},   {32, -11, 1},
    {33, -11, 1},   {34, -11, 1},   {35, -11, 1},   {36, -11, 1},   {37, -11, 1},   {38, -11, 1},   {39, -11, 1},   {40, -11, 1},   {47, -11, 1},
    {51, -11, 1},   {56, -11, 1},   {13, -12, 1},   {14, -12, 1},   {15, -12, 1},   {16, -12, 1},   {17, -12, 1},   {18, -12, 1},   {19, -12, 1},
    {20, -12, 1},   {21, -12, 1},   {22, -12, 1},   {23, -12, 1},   {24, -12, 1},   {25, -12, 1},   {26, -12, 1},   {27, -12, 1},   {28, -12, 1},
    {29, -12, 1},   {30, -12, 1},   {31, -12, 1},   {32, -12, 1},   {33, -12, 1},   {34, -12, 1},   {35, -12, 1},   {36, -12, 1},   {37, -12, 1},
    {38, -12, 1},   {39, -12, 1},   {40, -12, 1},   {43, -12, 1},   {47, -12, 1},   {49, -12, 1},   {12, -13, 1},   {13, -13, 1},   {14, -13, 1},
    {15, -13, 1},   {16, -13, 1},   {17, -13, 1},   {18, -13, 1},   {19, -13, 1},   {20, -13, 1},   {21, -13, 1},   {22, -13, 1},   {23, -13, 1},
    {24, -13, 1},   {25, -13, 1},   {26, -13, 1},   {27, -13, 1},   {28, -13, 1},   {29, -13, 1},   {30, -13, 1},   {31, -13, 1},   {32, -13, 1},
    {33, -13, 1},   {34, -13, 1},   {35, -13, 1},   {36, -13, 1},   {37, -13, 1},   {38, -13, 1},   {39, -13, 1},   {40, -13, 1},   {43, -13, 1},
    {44, -13, 1},   {45, -13, 1},   {48, -13, 1},   {49, -13, 1},   {12, -14, 1},   {13, -14, 1},   {14, -14, 1},   {15, -14, 1},   {16, -14, 1},
    {17, -14, 1},   {18, -14, 1},   {19, -14, 1},   {20, -14, 1},   {21, -14, 1},   {22, -14, 1},   {23, -14, 1},   {24, -14, 1},   {25, -14, 1},
    {26, -14, 1},   {27, -14, 1},   {28, -14, 1},   {29, -14, 1},   {30, -14, 1},   {31, -14, 1},   {32, -14, 1},   {33, -14, 1},   {34, -14, 1},
    {35, -14, 1},   {36, -14, 1},   {37, -14, 1},   {38, -14, 1},   {39, -14, 1},   {40, -14, 1},   {45, -14, 1},   {47, -14, 1},   {48, -14, 1},
    {49, -14, 1},   {50, -14, 1},   {12, -15, 1},   {13, -15, 1},   {14, -15, 1},   {15, -15, 1},   {16, -15, 1},   {17, -15, 1},   {18, -15, 1},
    {19, -15, 1},   {20, -15, 1},   {21, -15, 1},   {22, -15, 1},   {23, -15, 1},   {24, -15, 1},   {25, -15, 1},   {26, -15, 1},   {27, -15, 1},
    {28, -15, 1},   {29, -15, 1},   {30, -15, 1},   {31, -15, 1},   {32, -15, 1},   {33, -15, 1},   {34, -15, 1},   {35, -15, 1},   {36, -15, 1},
    {37, -15, 1},   {38, -15, 1},   {39, -15, 1},   {40, -15, 1},   {47, -15, 1},   {48, -15, 1},   {49, -15, 1},   {50, -15, 1},   {11, -16, 1},
    {12, -16, 1},   {13, -16, 1},   {14, -16, 1},   {15, -16, 1},   {16, -16, 1},   {17, -16, 1},   {18, -16, 1},   {19, -16, 1},   {20, -16, 1},
    {21, -16, 1},   {22, -16, 1},   {23, -16, 1},   {24, -16, 1},   {25, -16, 1},   {26, -16, 1},   {27, -16, 1},   {28, -16, 1},   {29, -16, 1},
    {30, -16, 1},   {31, -16, 1},   {32, -16, 1},   {33, -16, 1},   {34, -16, 1},   {35, -16, 1},   {36, -16, 1},   {37, -16, 1},   {38, -16, 1},
    {39, -16, 1},   {40, -16, 1},   {45, -16, 1},   {46, -16, 1},   {47, -16, 1},   {48, -16, 1},   {49, -16, 1},   {50, -16, 1},   {54, -16, 1},
    {11, -17, 1},   {12, -17, 1},   {13, -17, 1},   {14, -17, 1},   {15, -17, 1},   {16, -17, 1},   {17, -17, 1},   {18, -17, 1},   {19, -17, 1},
    {20, -17, 1},   {21, -17, 1},   {22, -17, 1},   {23, -17, 1},   {24, -17, 1},   {25, -17, 1},   {26, -17, 1},   {27, -17, 1},   {28, -17, 1},
    {29, -17, 1},   {30, -17, 1},   {31, -17, 1},   {32, -17, 1},   {33, -17, 1},   {34, -17, 1},   {35, -17, 1},   {36, -17, 1},   {37, -17, 1},
    {38, -17, 1},   {39, -17, 1},   {40, -17, 1},   {43, -17, 1},   {44, -17, 1},   {45, -17, 1},   {46, -17, 1},   {47, -17, 1},   {48, -17, 1},
    {49, -17, 1},   {50, -17, 1},   {59, -17, 1},   {11, -18, 1},   {12, -18, 1},   {13, -18, 1},   {14, -18, 1},   {15, -18, 1},   {16, -18, 1},
    {17, -18, 1},   {18, -18, 1},   {19, -18, 1},   {20, -18, 1},   {21, -18, 1},   {22, -18, 1},   {23, -18, 1},   {24, -18, 1},   {25, -18, 1},
    {26, -18, 1},   {27, -18, 1},   {28, -18, 1},   {29, -18, 1},   {30, -18, 1},   {31, -18, 1},   {32, -18, 1},   {33, -18, 1},   {34, -18, 1},
    {35, -18, 1},   {36, -18, 1},   {37, -18, 1},   {38, -18, 1},   {39, -18, 1},   {42, -18, 1},   {43, -18, 1},   {44, -18, 1},   {45, -18, 1},
    {46, -18, 1},   {47, -18, 1},   {48, -18, 1},   {49, -18, 1},   {11, -19, 1},   {12, -19, 1},   {13, -19, 1},   {14, -19, 1},   {15, -19, 1},
    {16, -19, 1},   {17, -19, 1},   {18, -19, 1},   {19, -19, 1},   {20, -19, 1},   {21, -19, 1},   {22, -19, 1},   {23, -19, 1},   {24, -19, 1},
    {25, -19, 1},   {26, -19, 1},   {27, -19, 1},   {28, -19, 1},   {29, -19, 1},   {30, -19, 1},   {31, -19, 1},   {32, -19, 1},   {33, -19, 1},
    {34, -19, 1},   {35, -19, 1},   {36, -19, 1},   {37, -19, 1},   {43, -19, 1},   {44, -19, 1},   {45, -19, 1},   {46, -19, 1},   {47, -19, 1},
    {48, -19, 1},   {49, -19, 1},   {12, -20, 1},   {13, -20, 1},   {14, -20, 1},   {15, -20, 1},   {16, -20, 1},   {17, -20, 1},   {18, -20, 1},
    {19, -20, 1},   {20, -20, 1},   {21, -20, 1},   {22, -20, 1},   {23, -20, 1},   {24, -20, 1},   {25, -20, 1},   {26, -20, 1},   {27, -20, 1},
    {28, -20, 1},   {29, -20, 1},   {30, -20, 1},   {31, -20, 1},   {32, -20, 1},   {33, -20, 1},   {34, -20, 1},   {35, -20, 1},   {44, -20, 1},
    {45, -20, 1},   {46, -20, 1},   {47, -20, 1},   {48, -20, 1},   {49, -20, 1},   {57, -20, 1},   {63, -20, 1},   {13, -21, 1},   {14, -21, 1},
    {15, -21, 1},   {16, -21, 1},   {17, -21, 1},   {18, -21, 1},   {19, -21, 1},   {20, -21, 1},   {21, -21, 1},   {22, -21, 1},   {23, -21, 1},
    {24, -21, 1},   {25, -21, 1},   {26, -21, 1},   {27, -21, 1},   {28, -21, 1},   {29, -21, 1},   {30, -21, 1},   {31, -21, 1},   {32, -21, 1},
    {33, -21, 1},   {34, -21, 1},   {35, -21, 1},   {43, -21, 1},   {44, -21, 1},   {45, -21, 1},   {46, -21, 1},   {47, -21, 1},   {48, -21, 1},
    {55, -21, 1},   {57, -21, 1},   {13, -22, 1},   {14, -22, 1},   {15, -22, 1},   {16, -22, 1},   {17, -22, 1},   {18, -22, 1},   {19, -22, 1},
    {20, -22, 1},   {21, -22, 1},   {22, -22, 1},   {23, -22, 1},   {24, -22, 1},   {25, -22, 1},   {26, -22, 1},   {27, -22, 1},   {28, -22, 1},
    {29, -22, 1},   {30, -22, 1},   {31, -22, 1},   {32, -22, 1},   {33, -22, 1},   {34, -22, 1},   {35, -22, 1},   {43, -22, 1},   {44, -22, 1},
    {45, -22, 1},   {46, -22, 1},   {47, -22, 1},   {48, -22, 1},   {55, -22, 1},   {14, -23, 1},   {15, -23, 1},   {16, -23, 1},   {17, -23, 1},
    {18, -23, 1},   {19, -23, 1},   {20, -23, 1},   {21, -23, 1},   {22, -23, 1},   {23, -23, 1},   {24, -23, 1},   {25, -23, 1},   {26, -23, 1},
    {27, -23, 1},   {28, -23, 1},   {29, -23, 1},   {30, -23, 1},   {31, -23, 1},   {32, -23, 1},   {33, -23, 1},   {34, -23, 1},   {35, -23, 1},
    {40, -23, 1},   {43, -23, 1},   {44, -23, 1},   {45, -23, 1},   {46, -23, 1},   {47, -23, 1},   {48, -23, 1},   {14, -24, 1},   {15, -24, 1},
    {16, -24, 1},   {17, -24, 1},   {18, -24, 1},   {19, -24, 1},   {20, -24, 1},   {21, -24, 1},   {22, -24, 1},   {23, -24, 1},   {24, -24, 1},
    {25, -24, 1},   {26, -24, 1},   {27, -24, 1},   {28, -24, 1},   {29, -24, 1},   {30, -24, 1},   {31, -24, 1},   {32, -24, 1},   {33, -24, 1},
    {34, -24, 1},   {35, -24, 1},   {43, -24, 1},   {44, -24, 1},   {45, -24, 1},   {46, -24, 1},   {47, -24, 1},   {14, -25, 1},   {15, -25, 1},
    {16, -25, 1},   {17, -25, 1},   {18, -25, 1},   {19, -25, 1},   {20, -25, 1},   {21, -25, 1},   {22, -25, 1},   {23, -25, 1},   {24, -25, 1},
    {25, -25, 1},   {26, -25, 1},   {27, -25, 1},   {28, -25, 1},   {29, -25, 1},   {30, -25, 1},   {31, -25, 1},   {32, -25, 1},   {33, -25, 1},
    {34, -25, 1},   {35, -25, 1},   {43, -25, 1},   {44, -25, 1},   {45, -25, 1},   {46, -25, 1},   {47, -25, 1},   {14, -26, 1},   {15, -26, 1},
    {16, -26, 1},   {17, -26, 1},   {18, -26, 1},   {19, -26, 1},   {20, -26, 1},   {21, -26, 1},   {22, -26, 1},   {23, -26, 1},   {24, -26, 1},
    {25, -26, 1},   {26, -26, 1},   {27, -26, 1},   {28, -26, 1},   {29, -26, 1},   {30, -26, 1},   {31, -26, 1},   {32, -26, 1},   {33, -26, 1},
    {34, -26, 1},   {44, -26, 1},   {45, -26, 1},   {46, -26, 1},   {47, -26, 1},   {14, -27, 1},   {15, -27, 1},   {16, -27, 1},   {17, -27, 1},
    {18, -27, 1},   {19, -27, 1},   {20, -27, 1},   {21, -27, 1},   {22, -27, 1},   {23, -27, 1},   {24, -27, 1},   {25, -27, 1},   {26, -27, 1},
    {27, -27, 1},   {28, -27, 1},   {29, -27, 1},   {30, -27, 1},   {31, -27, 1},   {32, -27, 1},   {15, -28, 1},   {16, -28, 1},   {17, -28, 1},
    {18, -28, 1},   {19, -28, 1},   {20, -28, 1},   {21, -28, 1},   {22, -28, 1},   {23, -28, 1},   {24, -28, 1},   {25, -28, 1},   {26, -28, 1},
    {27, -28, 1},   {28, -28, 1},   {29, -28, 1},   {30, -28, 1},   {31, -28, 1},   {32, -28, 1},   {15, -29, 1},   {16, -29, 1},   {17, -29, 1},
    {18, -29, 1},   {19, -29, 1},   {20, -29, 1},   {21, -29, 1},   {22, -29, 1},   {23, -29, 1},   {24, -29, 1},   {25, -29, 1},   {26, -29, 1},
    {27, -29, 1},   {28, -29, 1},   {29, -29, 1},   {30, -29, 1},   {31, -29, 1},   {32, -29, 1},   {16, -30, 1},   {17, -30, 1},   {18, -30, 1},
    {19, -30, 1},   {20, -30, 1},   {21, -30, 1},   {22, -30, 1},   {23, -30, 1},   {24, -30, 1},   {25, -30, 1},   {26, -30, 1},   {27, -30, 1},
    {28, -30, 1},   {29, -30, 1},   {30, -30, 1},   {31, -30, 1},   {17, -31, 1},   {18, -31, 1},   {19, -31, 1},   {20, -31, 1},   {21, -31, 1},
    {22, -31, 1},   {23, -31, 1},   {24, -31, 1},   {25, -31, 1},   {26, -31, 1},   {27, -31, 1},   {28, -31, 1},   {29, -31, 1},   {30, -31, 1},
    {17, -32, 1},   {18, -32, 1},   {19, -32, 1},   {20, -32, 1},   {21, -32, 1},   {22, -32, 1},   {23, -32, 1},   {24, -32, 1},   {25, -32, 1},
    {26, -32, 1},   {27, -32, 1},   {28, -32, 1},   {29, -32, 1},   {30, -32, 1},   {17, -33, 1},   {18, -33, 1},   {19, -33, 1},   {20, -33, 1},
    {21, -33, 1},   {22, -33, 1},   {23, -33, 1},   {24, -33, 1},   {25, -33, 1},   {26, -33, 1},   {27, -33, 1},   {28, -33, 1},   {29, -33, 1},
    {17, -34, 1},   {18, -34, 1},   {19, -34, 1},   {20, -34, 1},   {21, -34, 1},   {22, -34, 1},   {23, -34, 1},   {24, -34, 1},   {25, -34, 1},
    {26, -34, 1},   {27, -34, 1},   {18, -35, 1},   {19, -35, 1},   {20, -35, 1},   {21, -35, 1},   {22, -35, 1},   {23, -35, 1},   {24, -35, 1},
    {25, -35, 1},   {119, -11, 2},  {120, -11, 2},  {121, -11, 2},  {122, -11, 2},  {123, -11, 2},  {124, -11, 2},  {132, -11, 2},  {133, -11, 2},
    {141, -11, 2},  {142, -11, 2},  {143, -11, 2},  {147, -11, 2},  {148, -11, 2},  {149, -11, 2},  {150, -11, 2},  {151, -11, 2},  {152, -11, 2},
    {153, -11, 2},  {161, -11, 2},  {162, -11, 2},  {165, -11, 2},  {166, -11, 2},  {179, -11, 2},  {-140, -11, 2}, {-151, -11, 2}, {-161, -11, 2},
    {-162, -11, 2}, {-166, -11, 2}, {122, -12, 2},  {130, -12, 2},  {131, -12, 2},  {132, -12, 2},  {133, -12, 2},  {134, -12, 2},  {135, -12, 2},
    {136, -12, 2},  {141, -12, 2},  {142, -12, 2},  {143, -12, 2},  {144, -12, 2},  {151, -12, 2},  {152, -12, 2},  {153, -12, 2},  {154, -12, 2},
    {159, -12, 2},  {160, -12, 2},  {166, -12, 2},  {169, -12, 2},  {170, -12, 2},  {-152, -12, 2}, {-166, -12, 2}, {-172, -12, 2}, {122, -13, 2},
    {123, -13, 2},  {130, -13, 2},  {131, -13, 2},  {132, -13, 2},  {133, -13, 2},  {134, -13, 2},  {135, -13, 2},  {136, -13, 2},  {141, -13, 2},
    {142, -13, 2},  {143, -13, 2},  {168, -13, 2},  {176, -13, 2},  {177, -13, 2},  {125, -14, 2},  {126, -14, 2},  {127, -14, 2},  {129, -14, 2},
    {130, -14, 2},  {131, -14, 2},  {132, -14, 2},  {133, -14, 2},  {134, -14, 2},  {135, -14, 2},  {136, -14, 2},  {141, -14, 2},  {142, -14, 2},
    {143, -14, 2},  {144, -14, 2},  {166, -14, 2},  {167, -14, 2},  {-164, -14, 2}, {-172, -14, 2}, {-173, -14, 2}, {-177, -14, 2}, {121, -15, 2},
    {123, -15, 2},  {124, -15, 2},  {125, -15, 2},  {126, -15, 2},  {127, -15, 2},  {128, -15, 2},  {129, -15, 2},  {130, -15, 2},  {131, -15, 2},
    {132, -15, 2},  {133, -15, 2},  {134, -15, 2},  {135, -15, 2},  {136, -15, 2},  {141, -15, 2},  {142, -15, 2},  {143, -15, 2},  {144, -15, 2},
    {145, -15, 2},  {166, -15, 2},  {167, -15, 2},  {168, -15, 2},  {-139, -15, 2}, {-142, -15, 2}, {-145, -15, 2}, {-146, -15, 2}, {-147, -15, 2},
    {-148, -15, 2}, {-149, -15, 2}, {-169, -15, 2}, {-170, -15, 2}, {-171, -15, 2}, {-172, -15, 2}, {-178, -15, 2}, {-179, -15, 2}, {123, -16, 2},
    {124, -16, 2},  {125, -16, 2},  {126, -16, 2},  {127, -16, 2},  {128, -16, 2},  {129, -16, 2},  {130, -16, 2},  {131, -16, 2},  {132, -16, 2},
    {133, -16, 2},  {134, -16, 2},  {135, -16, 2},  {136, -16, 2},  {137, -16, 2},  {141, -16, 2},  {142, -16, 2},  {143, -16, 2},  {144, -16, 2},
    {145, -16, 2},  {166, -16, 2},  {167, -16, 2},  {168, -16, 2},  {-141, -16, 2}, {-143, -16, 2}, {-145, -16, 2}, {-146, -16, 2}, {-147, -16, 2},
    {-148, -16, 2}, {-149, -16, 2}, {-155, -16, 2}, {-174, -16, 2}, {-176, -16, 2}, {-180, -16, 2}, {122, -17, 2},  {123, -17, 2},  {124, -17, 2},
    {125, -17, 2},  {126, -17, 2},  {127, -17, 2},  {128, -17, 2},  {129, -17, 2},  {130, -17, 2},  {131, -17, 2},  {132, -17, 2},  {133, -17, 2},
    {134, -17, 2},  {135, -17, 2},  {136, -17, 2},  {137, -17, 2},  {138, -17, 2},  {139, -17, 2},  {140, -17, 2},  {141, -17, 2},  {142, -17, 2},
    {143, -17, 2},  {144, -17, 2},  {145, -17, 2},  {146, -17, 2},  {149, -17, 2},  {150, -17, 2},  {167, -17, 2},  {168, -17, 2},  {177, -17, 2},
    {178, -17, 2},  {179, -17, 2},  {-141, -17, 2}, {-142, -17, 2}, {-143, -17, 2}, {-144, -17, 2}, {-145, -17, 2}, {-146, -17, 2}, {-147, -17, 2},
    {-150, -17, 2}, {-151, -17, 2}, {-152, -17, 2}, {-153, -17, 2}, {-154, -17, 2}, {-155, -17, 2}, {-180, -17, 2}, {118, -18, 2},  {119, -18, 2},
    {122, -18, 2},  {123, -18, 2},  {124, -18, 2},  {125, -18, 2},  {126, -18, 2},  {127, -18, 2},  {128, -18, 2},  {129, -18, 2},  {130, -18, 2},
    {131, -18, 2},  {132, -18, 2},  {133, -18, 2},  {134, -18, 2},  {135, -18, 2},  {136, -18, 2},  {137, -18, 2},  {138, -18, 2},  {139, -18, 2},
    {140, -18, 2},  {141, -18, 2},  {142, -18, 2},  {143, -18, 2},  {144, -18, 2},  {145, -18, 2},  {146, -18, 2},  {148, -18, 2},  {155, -18, 2},
    {168, -18, 2},  {176, -18, 2},  {177, -18, 2},  {178, -18, 2},  {179, -18, 2},  {-139, -18, 2}, {-141, -18, 2}, {-142, -18, 2}, {-143, -18, 2},
    {-144, -18, 2}, {-145, -18, 2}, {-146, -18, 2}, {-149, -18, 2}, {-150, -18, 2}, {-151, -18, 2}, {-179, -18, 2}, {-180, -18, 2}, {121, -19, 2},
    {122, -19, 2},  {123, -19, 2},  {124, -19, 2},  {125, -19, 2},  {126, -19, 2},  {127, -19, 2},  {128, -19, 2},  {129, -19, 2},  {130, -19, 2},
    {131, -19, 2},  {132, -19, 2},  {133, -19, 2},  {134, -19, 2},  {135, -19, 2},  {136, -19, 2},  {137, -19, 2},  {138, -19, 2},  {139, -19, 2},
    {140, -19, 2},  {141, -19, 2},  {142, -19, 2},  {143, -19, 2},  {144, -19, 2},  {145, -19, 2},  {146, -19, 2},  {162, -19, 2},  {163, -19, 2},
    {168, -19, 2},  {169, -19, 2},  {177, -19, 2},  {178, -19, 2},  {179, -19, 2},  {-137, -19, 2}, {-138, -19, 2}, {-139, -19, 2}, {-140, -19, 2},
    {-141, -19, 2}, {-142, -19, 2}, {-143, -19, 2}, {-160, -19, 2}, {-164, -19, 2}, {-170, -19, 2}, {-174, -19, 2}, {-175, -19, 2}, {-179, -19, 2},
    {-180, -19, 2}, {118, -20, 2},  {119, -20, 2},  {120, -20, 2},  {121, -20, 2},  {122, -20, 2},  {123, -20, 2},  {124, -20, 2},  {125, -20, 2},
    {126, -20, 2},  {127, -20, 2},  {128, -20, 2},  {129, -20, 2},  {130, -20, 2},  {131, -20, 2},  {132, -20, 2},  {133, -20, 2},  {134, -20, 2},
    {135, -20, 2},  {136, -20, 2},  {137, -20, 2},  {138, -20, 2},  {139, -20, 2},  {140, -20, 2},  {141, -20, 2},  {142, -20, 2},  {143, -20, 2},
    {144, -20, 2},  {145, -20, 2},  {146, -20, 2},  {147, -20, 2},  {148, -20, 2},  {158, -20, 2},  {163, -20, 2},  {169, -20, 2},  {170, -20, 2},
    {177, -20, 2},  {178, -20, 2},  {179, -20, 2},  {-139, -20, 2}, {-140, -20, 2}, {-141, -20, 2}, {-142, -20, 2}, {-145, -20, 2}, {-146, -20, 2},
    {-158, -20, 2}, {-159, -20, 2}, {-170, -20, 2}, {-175, -20, 2}, {-176, -20, 2}, {-179, -20, 2}, {-180, -20, 2}, {115, -21, 2},  {116, -21, 2},
    {117, -21, 2},  {118, -21, 2},  {119, -21, 2},  {120, -21, 2},  {121, -21, 2},  {122, -21, 2},  {123, -21, 2},  {124, -21, 2},  {125, -21, 2},
    {126, -21, 2},  {127, -21, 2},  {128, -21, 2},  {129, -21, 2},  {130, -21, 2},  {131, -21, 2},  {132, -21, 2},  {133, -21, 2},  {134, -21, 2},
    {135, -21, 2},  {136, -21, 2},  {137, -21, 2},  {138, -21, 2},  {139, -21, 2},  {140, -21, 2},  {141, -21, 2},  {142, -21, 2},  {143, -21, 2},
    {144, -21, 2},  {145, -21, 2},  {146, -21, 2},  {147, -21, 2},  {148, -21, 2},  {149, -21, 2},  {150, -21, 2},  {154, -21, 2},  {163, -21, 2},
    {164, -21, 2},  {165, -21, 2},  {166, -21, 2},  {167, -21, 2},  {169, -21, 2},  {-139, -21, 2}, {-140, -21, 2}, {-144, -21, 2}, {-158, -21, 2},
    {-159, -21, 2}, {-175, -21, 2}, {-176, -21, 2}, {-179, -21, 2}, {113, -22, 2},  {114, -22, 2},  {115, -22, 2},  {116, -22, 2},  {117, -22, 2},
    {118, -22, 2},  {119, -22, 2},  {120, -22, 2},  {121, -22, 2},  {122, -22, 2},  {123, -22, 2},  {124, -22, 2},  {125, -22, 2},  {126, -22, 2},
    {127, -22, 2},  {128, -22, 2},  {129, -22, 2},  {130, -22, 2},  {131, -22, 2},  {132, -22, 2},  {133, -22, 2},  {134, -22, 2},  {135, -22, 2},
    {136, -22, 2},  {137, -22, 2},  {138, -22, 2},  {139, -22, 2},  {140, -22, 2},  {141, -22, 2},  {142, -22, 2},  {143, -22, 2},  {144, -22, 2},
    {145, -22, 2},  {146, -22, 2},  {147, -22, 2},  {148, -22, 2},  {149, -22, 2},  {150, -22, 2},  {151, -22, 2},  {152, -22, 2},  {153, -22, 2},
    {154, -22, 2},  {155, -22, 2},  {158, -22, 2},  {164, -22, 2},  {165, -22, 2},  {166, -22, 2},  {167, -22, 2},  {168, -22, 2},  {-136, -22, 2},
    {-137, -22, 2}, {-139, -22, 2}, {-140, -22, 2}, {-141, -22, 2}, {-155, -22, 2}, {-158, -22, 2}, {-160, -22, 2}, {-175, -22, 2}, {-176, -22, 2},
    {-179, -22, 2}, {113, -23, 2},  {114, -23, 2},  {115, -23, 2},  {116, -23, 2},  {117, -23, 2},  {118, -23, 2},  {119, -23, 2},  {120, -23, 2},
    {121, -23, 2},  {122, -23, 2},  {123, -23, 2},  {124, -23, 2},  {125, -23, 2},  {126, -23, 2},  {127, -23, 2},  {128, -23, 2},  {129, -23, 2},
    {130, -23, 2},  {131, -23, 2},  {132, -23, 2},  {133, -23, 2},  {134, -23, 2},  {135, -23, 2},  {136, -23, 2},  {137, -23, 2},  {138, -23, 2},
    {139, -23, 2},  {140, -23, 2},  {141, -23, 2},  {142, -23, 2},  {143, -23, 2},  {144, -23, 2},  {145, -23, 2},  {146, -23, 2},  {147, -23, 2},
    {148, -23, 2},  {149, -23, 2},  {150, -23, 2},  {152, -23, 2},  {155, -23, 2},  {165, -23, 2},  {166, -23, 2},  {167, -23, 2},  {168, -23, 2},
    {171, -23, 2},  {172, -23, 2},  {-135, -23, 2}, {-137, -23, 2}, {-139, -23, 2}, {-152, -23, 2}, {-153, -23, 2}, {-177, -23, 2}, {113, -24, 2},
    {114, -24, 2},  {115, -24, 2},  {116, -24, 2},  {117, -24, 2},  {118, -24, 2},  {119, -24, 2},  {120, -24, 2},  {121, -24, 2},  {122, -24, 2},
    {123, -24, 2},  {124, -24, 2},  {125, -24, 2},  {126, -24, 2},  {127, -24, 2},  {128, -24, 2},  {129, -24, 2},  {130, -24, 2},  {131, -24, 2},
    {132, -24, 2},  {133, -24, 2},  {134, -24, 2},  {135, -24, 2},  {136, -24, 2},  {137, -24, 2},  {138, -24, 2},  {139, -24, 2},  {140, -24, 2},
    {141, -24, 2},  {142, -24, 2},  {143, -24, 2},  {144, -24, 2},  {145, -24, 2},  {146, -24, 2},  {147, -24, 2},  {148, -24, 2},  {149, -24, 2},
    {150, -24, 2},  {151, -24, 2},  {152, -24, 2},  {155, -24, 2},  {-131, -24, 2}, {-135, -24, 2}, {-136, -24, 2}, {-138, -24, 2}, {-148, -24, 2},
    {-150, -24, 2}, {113, -25, 2},  {114, -25, 2},  {115, -25, 2},  {116, -25, 2},  {117, -25, 2},  {118, -25, 2},  {119, -25, 2},  {120, -25, 2},
    {121, -25, 2},  {122, -25, 2},  {123, -25, 2},  {124, -25, 2},  {125, -25, 2},  {126, -25, 2},  {127, -25, 2},  {128, -25, 2},  {129, -25, 2},
    {130, -25, 2},  {131, -25, 2},  {132, -25, 2},  {133, -25, 2},  {134, -25, 2},  {135, -25, 2},  {136, -25, 2},  {137, -25, 2},  {138, -25, 2},
    {139, -25, 2},  {140, -25, 2},  {141, -25, 2},  {142, -25, 2},  {143, -25, 2},  {144, -25, 2},  {145, -25, 2},  {146, -25, 2},  {147, -25, 2},
    {148, -25, 2},  {149, -25, 2},  {150, -25, 2},  {151, -25, 2},  {152, -25, 2},  {153, -25, 2},  {-125, -25, 2}, {-129, -25, 2}, {112, -26, 2},
    {113, -26, 2},  {114, -26, 2},  {115, -26, 2},  {116, -26, 2},  {117, -26, 2},  {118, -26, 2},  {119, -26, 2},  {120, -26, 2},  {121, -26, 2},
    {122, -26, 2},  {123, -26, 2},  {124, -26, 2},  {125, -26, 2},  {126, -26, 2},  {127, -26, 2},  {128, -26, 2},  {129, -26, 2},  {130, -26, 2},
    {131, -26, 2},  {132, -26, 2},  {133, -26, 2},  {134, -26, 2},  {135, -26, 2},  {136, -26, 2},  {137, -26, 2},  {138, -26, 2},  {139, -26, 2},
    {140, -26, 2},  {141, -26, 2},  {142, -26, 2},  {143, -26, 2},  {144, -26, 2},  {145, -26, 2},  {146, -26, 2},  {147, -26, 2},  {148, -26, 2},
    {149, -26, 2},  {150, -26, 2},  {151, -26, 2},  {152, -26, 2},  {153, -26, 2},  {-131, -26, 2}, {113, -27, 2},  {114, -27, 2},  {115, -27, 2},
    {116, -27, 2},  {117, -27, 2},  {118, -27, 2},  {119, -27, 2},  {120, -27, 2},  {121, -27, 2},  {122, -27, 2},  {123, -27, 2},  {124, -27, 2},
    {125, -27, 2},  {126, -27, 2},  {127, -27, 2},  {128, -27, 2},  {129, -27, 2},  {130, -27, 2},  {131, -27, 2},  {132, -27, 2},  {133, -27, 2},
    {134, -27, 2},  {135, -27, 2},  {136, -27, 2},  {137, -27, 2},  {138, -27, 2},  {139, -27, 2},  {140, -27, 2},  {141, -27, 2},  {142, -27, 2},
    {143, -27, 2},  {144, -27, 2},  {145, -27, 2},  {146, -27, 2},  {147, -27, 2},  {148, -27, 2},  {149, -27, 2},  {150, -27, 2},  {151, -27, 2},
    {152, -27, 2},  {153, -27, 2},  {-106, -27, 2}, {113, -28, 2},  {114, -28, 2},  {115, -28, 2},  {116, -28, 2},  {117, -28, 2},  {118, -28, 2},
    {119, -28, 2},  {120, -28, 2},  {121, -28, 2},  {122, -28, 2},  {123, -28, 2},  {124, -28, 2},  {125, -28, 2},  {126, -28, 2},  {127, -28, 2},
    {128, -28, 2},  {129, -28, 2},  {130, -28, 2},  {131, -28, 2},  {132, -28, 2},  {133, -28, 2},  {134, -28, 2},  {135, -28, 2},  {136, -28, 2},
    {137, -28, 2},  {138, -28, 2},  {139, -28, 2},  {140, -28, 2},  {141, -28, 2},  {142, -28, 2},  {143, -28, 2},  {144, -28, 2},  {145, -28, 2},
    {146, -28, 2},  {147, -28, 2},  {148, -28, 2},  {149, -28, 2},  {150, -28, 2},  {151, -28, 2},  {152, -28, 2},  {153, -28, 2},  {-110, -28, 2},
    {-144, -28, 2}, {-145, -28, 2}, {113, -29, 2},  {114, -29, 2},  {115, -29, 2},  {116, -29, 2},  {117, -29, 2},  {118, -29, 2},  {119, -29, 2},
    {120, -29, 2},  {121, -29, 2},  {122, -29, 2},  {123, -29, 2},  {124, -29, 2},  {125, -29, 2},  {126, -29, 2},  {127, -29, 2},  {128, -29, 2},
    {129, -29, 2},  {130, -29, 2},  {131, -29, 2},  {132, -29, 2},  {133, -29, 2},  {134, -29, 2},  {135, -29, 2},  {136, -29, 2},  {137, -29, 2},
    {138, -29, 2},  {139, -29, 2},  {140, -29, 2},  {141, -29, 2},  {142, -29, 2},  {143, -29, 2},  {144, -29, 2},  {145, -29, 2},  {146, -29, 2},
    {147, -29, 2},  {148, -29, 2},  {149, -29, 2},  {150, -29, 2},  {151, -29, 2},  {152, -29, 2},  {153, -29, 2},  {114, -30, 2},  {115, -30, 2},
    {116, -30, 2},  {117, -30, 2},  {118, -30, 2},  {119, -30, 2},  {120, -30, 2},  {121, -30, 2},  {122, -30, 2},  {123, -30, 2},  {124, -30, 2},
    {125, -30, 2},  {126, -30, 2},  {127, -30, 2},  {128, -30, 2},  {129, -30, 2},  {130, -30, 2},  {131, -30, 2},  {132, -30, 2},  {133, -30, 2},
    {134, -30, 2},  {135, -30, 2},  {136, -30, 2},  {137, -30, 2},  {138, -30, 2},  {139, -30, 2},  {140, -30, 2},  {141, -30, 2},  {142, -30, 2},
    {143, -30, 2},  {144, -30, 2},  {145, -30, 2},  {146, -30, 2},  {147, -30, 2},  {148, -30, 2},  {149, -30, 2},  {150, -30, 2},  {151, -30, 2},
    {152, -30, 2},  {153, -30, 2},  {114, -31, 2},  {115, -31, 2},  {116, -31, 2},  {117, -31, 2},  {118, -31, 2},  {119, -31, 2},  {120, -31, 2},
    {121, -31, 2},  {122, -31, 2},  {123, -31, 2},  {124, -31, 2},  {125, -31, 2},  {126, -31, 2},  {127, -31, 2},  {128, -31, 2},  {129, -31, 2},
    {130, -31, 2},  {131, -31, 2},  {132, -31, 2},  {133, -31, 2},  {134, -31, 2},  {135, -31, 2},  {136, -31, 2},  {137, -31, 2},  {138, -31, 2},
    {139, -31, 2},  {140, -31, 2},  {141, -31, 2},  {142, -31, 2},  {143, -31, 2},  {144, -31, 2},  {145, -31, 2},  {146, -31, 2},  {147, -31, 2},
    {148, -31, 2},  {149, -31, 2},  {150, -31, 2},  {151, -31, 2},  {152, -31, 2},  {153, -31, 2},  {115, -32, 2},  {116, -32, 2},  {117, -32, 2},
    {118, -32, 2},  {119, -32, 2},  {120, -32, 2},  {121, -32, 2},  {122, -32, 2},  {123, -32, 2},  {124, -32, 2},  {125, -32, 2},  {126, -32, 2},
    {127, -32, 2},  {128, -32, 2},  {129, -32, 2},  {130, -32, 2},  {131, -32, 2},  {132, -32, 2},  {133, -32, 2},  {134, -32, 2},  {135, -32, 2},
    {136, -32, 2},  {137, -32, 2},  {138, -32, 2},  {139, -32, 2},  {140, -32, 2},  {141, -32, 2},  {142, -32, 2},  {143, -32, 2},  {144, -32, 2},
    {145, -32, 2},  {146, -32, 2},  {147, -32, 2},  {148, -32, 2},  {149, -32, 2},  {150, -32, 2},  {151, -32, 2},  {152, -32, 2},  {153, -32, 2},
    {159, -32, 2},  {115, -33, 2},  {116, -33, 2},  {117, -33, 2},  {118, -33, 2},  {119, -33, 2},  {120, -33, 2},  {121, -33, 2},  {122, -33, 2},
    {123, -33, 2},  {124, -33, 2},  {125, -33, 2},  {126, -33, 2},  {127, -33, 2},  {128, -33, 2},  {132, -33, 2},  {133, -33, 2},  {134, -33, 2},
    {135, -33, 2},  {136, -33, 2},  {137, -33, 2},  {138, -33, 2},  {139, -33, 2},  {140, -33, 2},  {141, -33, 2},  {142, -33, 2},  {143, -33, 2},
    {144, -33, 2},  {145, -33, 2},  {146, -33, 2},  {147, -33, 2},  {148, -33, 2},  {149, -33, 2},  {150, -33, 2},  {151, -33, 2},  {152, -33, 2},
    {114, -34, 2},  {115, -34, 2},  {116, -34, 2},  {117, -34, 2},  {118, -34, 2},  {119, -34, 2},  {120, -34, 2},  {121, -34, 2},  {122, -34, 2},
    {123, -34, 2},  {124, -34, 2},  {134, -34, 2},  {135, -34, 2},  {136, -34, 2},  {137, -34, 2},  {138, -34, 2},  {139, -34, 2},  {140, -34, 2},
    {141, -34, 2},  {142, -34, 2},  {143, -34, 2},  {144, -34, 2},  {145, -34, 2},  {146, -34, 2},  {147, -34, 2},  {148, -34, 2},  {149, -34, 2},
    {150, -34, 2},  {151, -34, 2},  {114, -35, 2},  {115, -35, 2},  {116, -35, 2},  {117, -35, 2},  {118, -35, 2},  {119, -35, 2},  {120, -35, 2},
    {121, -35, 2},  {122, -35, 2},  {123, -35, 2},  {134, -35, 2},  {135, -35, 2},  {136, -35, 2},  {137, -35, 2},  {138, -35, 2},  {139, -35, 2},
    {140, -35, 2},  {141, -35, 2},  {142, -35, 2},  {143, -35, 2},  {144, -35, 2},  {145, -35, 2},  {146, -35, 2},  {147, -35, 2},  {148, -35, 2},
    {149, -35, 2},  {150, -35, 2},  {151, -35, 2},  {116, -36, 2},  {117, -36, 2},  {118, -36, 2},  {135, -36, 2},  {136, -36, 2},  {137, -36, 2},
    {138, -36, 2},  {139, -36, 2},  {140, -36, 2},  {141, -36, 2},  {142, -36, 2},  {143, -36, 2},  {144, -36, 2},  {145, -36, 2},  {146, -36, 2},
    {147, -36, 2},  {148, -36, 2},  {149, -36, 2},  {150, -36, 2},  {136, -37, 2},  {137, -37, 2},  {139, -37, 2},  {140, -37, 2},  {141, -37, 2},
    {142, -37, 2},  {143, -37, 2},  {144, -37, 2},  {145, -37, 2},  {146, -37, 2},  {147, -37, 2},  {148, -37, 2},  {149, -37, 2},  {150, -37, 2},
    {139, -38, 2},  {140, -38, 2},  {141, -38, 2},  {142, -38, 2},  {143, -38, 2},  {144, -38, 2},  {145, -38, 2},  {146, -38, 2},  {147, -38, 2},
    {148, -38, 2},  {149, -38, 2},  {150, -38, 2},  {140, -39, 2},  {141, -39, 2},  {142, -39, 2},  {143, -39, 2},  {144, -39, 2},  {145, -39, 2},
    {146, -39, 2},  {147, -39, 2},  {143, -40, 2},  {144, -40, 2},  {146, -40, 2},  {147, -40, 2},  {148, -40, 2},  {143, -41, 2},  {144, -41, 2},
    {145, -41, 2},  {146, -41, 2},  {147, -41, 2},  {148, -41, 2},  {144, -42, 2},  {145, -42, 2},  {146, -42, 2},  {147, -42, 2},  {148, -42, 2},
    {145, -43, 2},  {146, -43, 2},  {147, -43, 2},  {148, -43, 2},  {145, -44, 2},  {146, -44, 2},  {147, -44, 2},  {148, -44, 2},  {72, 0, 3},
    {73, 0, 3},     {97, 0, 3},     {98, 0, 3},     {99, 0, 3},     {100, 0, 3},    {101, 0, 3},    {102, 0, 3},    {103, 0, 3},    {104, 0, 3},
    {106, 0, 3},    {107, 0, 3},    {108, 0, 3},    {109, 0, 3},    {110, 0, 3},    {111, 0, 3},    {112, 0, 3},    {113, 0, 3},    {114, 0, 3},
    {115, 0, 3},    {116, 0, 3},    {117, 0, 3},    {118, 0, 3},    {119, 0, 3},    {120, 0, 3},    {121, 0, 3},    {122, 0, 3},    {123, 0, 3},
    {124, 0, 3},    {126, 0, 3},    {127, 0, 3},    {128, 0, 3},    {129, 0, 3},    {130, 0, 3},    {131, 0, 3},    {134, 0, 3},    {172, 0, 3},
    {173, 0, 3},    {-177, 0, 3},   {73, 1, 3},     {97, 1, 3},     {98, 1, 3},     {99, 1, 3},     {100, 1, 3},    {101, 1, 3},    {102, 1, 3},
    {103, 1, 3},    {104, 1, 3},    {106, 1, 3},    {107, 1, 3},    {108, 1, 3},    {109, 1, 3},    {110, 1, 3},    {111, 1, 3},    {112, 1, 3},
    {113, 1, 3},    {114, 1, 3},    {115, 1, 3},    {116, 1, 3},    {117, 1, 3},    {118, 1, 3},    {119, 1, 3},    {120, 1, 3},    {121, 1, 3},
    {122, 1, 3},    {124, 1, 3},    {125, 1, 3},    {126, 1, 3},    {127, 1, 3},    {128, 1, 3},    {131, 1, 3},    {154, 1, 3},    {172, 1, 3},
    {173, 1, 3},    {-158, 1, 3},   {72, 2, 3},     {73, 2, 3},     {95, 2, 3},     {96, 2, 3},     {97, 2, 3},     {98, 2, 3},     {99, 2, 3},
    {100, 2, 3},    {101, 2, 3},    {102, 2, 3},    {103, 2, 3},    {104, 2, 3},    {105, 2, 3},    {106, 2, 3},    {107, 2, 3},    {108, 2, 3},
    {109, 2, 3},    {111, 2, 3},    {112, 2, 3},    {113, 2, 3},    {114, 2, 3},    {115, 2, 3},    {116, 2, 3},    {117, 2, 3},    {118, 2, 3},
    {125, 2, 3},    {127, 2, 3},    {128, 2, 3},    {131, 2, 3},    {173, 2, 3},    {-158, 2, 3},   {72, 3, 3},     {73, 3, 3},     {95, 3, 3},
    {96, 3, 3},     {97, 3, 3},     {98, 3, 3},     {99, 3, 3},     {100, 3, 3},    {101, 3, 3},    {102, 3, 3},    {103, 3, 3},    {105, 3, 3},
    {106, 3, 3},    {107, 3, 3},    {108, 3, 3},    {112, 3, 3},    {113, 3, 3},    {114, 3, 3},    {115, 3, 3},    {116, 3, 3},    {117, 3, 3},
    {125, 3, 3},    {126, 3, 3},    {131, 3, 3},    {154, 3, 3},    {172, 3, 3},    {173, 3, 3},    {-160, 3, 3},   {72, 4, 3},     {73, 4, 3},
    {95, 4, 3},     {96, 4, 3},     {97, 4, 3},     {98, 4, 3},     {100, 4, 3},    {101, 4, 3},    {102, 4, 3},    {103, 4, 3},    {107, 4, 3},
    {108, 4, 3},    {113, 4, 3},    {114, 4, 3},    {115, 4, 3},    {116, 4, 3},    {117, 4, 3},    {118, 4, 3},    {119, 4, 3},    {125, 4, 3},
    {126, 4, 3},    {127, 4, 3},    {131, 4, 3},    {132, 4, 3},    {168, 4, 3},    {-161, 4, 3},   {72, 5, 3},     {73, 5, 3},     {80, 5, 3},
    {94, 5, 3},     {95, 5, 3},     {96, 5, 3},     {97, 5, 3},     {100, 5, 3},    {101, 5, 3},    {102, 5, 3},    {103, 5, 3},    {114, 5, 3},
    {115, 5, 3},    {116, 5, 3},    {117, 5, 3},    {118, 5, 3},    {119, 5, 3},    {120, 5, 3},    {121, 5, 3},    {124, 5, 3},    {125, 5, 3},
    {126, 5, 3},    {132, 5, 3},    {153, 5, 3},    {157, 5, 3},    {162, 5, 3},    {163, 5, 3},    {168, 5, 3},    {169, 5, 3},    {172, 5, 3},
    {-163, 5, 3},   {72, 6, 3},     {73, 6, 3},     {79, 6, 3},     {80, 6, 3},     {81, 6, 3},     {93, 6, 3},     {95, 6, 3},     {99, 6, 3},
    {100, 6, 3},    {101, 6, 3},    {102, 6, 3},    {115, 6, 3},    {116, 6, 3},    {117, 6, 3},    {118, 6, 3},    {120, 6, 3},    {121, 6, 3},
    {122, 6, 3},    {123, 6, 3},    {124, 6, 3},    {125, 6, 3},    {126, 6, 3},    {134, 6, 3},    {143, 6, 3},    {149, 6, 3},    {151, 6, 3},
    {152, 6, 3},    {157, 6, 3},    {158, 6, 3},    {159, 6, 3},    {160, 6, 3},    {169, 6, 3},    {171, 6, 3},    {172, 6, 3},    {-163, 6, 3},
    {72, 7, 3},     {73, 7, 3},     {79, 7, 3},     {80, 7, 3},     {81, 7, 3},     {93, 7, 3},     {98, 7, 3},     {99, 7, 3},     {100, 7, 3},
    {113, 7, 3},    {116, 7, 3},    {117, 7, 3},    {118, 7, 3},    {121, 7, 3},    {122, 7, 3},    {123, 7, 3},    {124, 7, 3},    {125, 7, 3},
    {126, 7, 3},    {134, 7, 3},    {143, 7, 3},    {144, 7, 3},    {145, 7, 3},    {146, 7, 3},    {147, 7, 3},    {149, 7, 3},    {151, 7, 3},
    {152, 7, 3},    {155, 7, 3},    {157, 7, 3},    {158, 7, 3},    {168, 7, 3},    {171, 7, 3},    {73, 8, 3},     {76, 8, 3},     {77, 8, 3},
    {78, 8, 3},     {79, 8, 3},     {80, 8, 3},     {81, 8, 3},     {92, 8, 3},     {93, 8, 3},     {97, 8, 3},     {98, 8, 3},     {99, 8, 3},
    {100, 8, 3},    {104, 8, 3},    {105, 8, 3},    {106, 8, 3},    {111, 8, 3},    {116, 8, 3},    {117, 8, 3},    {118, 8, 3},    {122, 8, 3},
    {123, 8, 3},    {124, 8, 3},    {125, 8, 3},    {126, 8, 3},    {134, 8, 3},    {137, 8, 3},    {140, 8, 3},    {144, 8, 3},    {146, 8, 3},
    {147, 8, 3},    {149, 8, 3},    {150, 8, 3},    {151, 8, 3},    {152, 8, 3},    {154, 8, 3},    {165, 8, 3},    {166, 8, 3},    {167, 8, 3},
    {168, 8, 3},    {170, 8, 3},    {171, 8, 3},    {76, 9, 3},     {77, 9, 3},     {78, 9, 3},     {79, 9, 3},     {80, 9, 3},     {92, 9, 3},
    {97, 9, 3},     {98, 9, 3},     {99, 9, 3},     {100, 9, 3},    {102, 9, 3},    {103, 9, 3},    {104, 9, 3},    {105, 9, 3},    {106, 9, 3},
    {109, 9, 3},    {117, 9, 3},    {118, 9, 3},    {119, 9, 3},    {120, 9, 3},    {121, 9, 3},    {122, 9, 3},    {123, 9, 3},    {124, 9, 3},
    {125, 9, 3},    {126, 9, 3},    {138, 9, 3},    {139, 9, 3},    {140, 9, 3},    {145, 9, 3},    {160, 9, 3},    {165, 9, 3},    {166, 9, 3},
    {167, 9, 3},    {169, 9, 3},    {170, 9, 3},    {72, 10, 3},    {73, 10, 3},    {75, 10, 3},    {76, 10, 3},    {77, 10, 3},    {78, 10, 3},
    {79, 10, 3},    {92, 10, 3},    {97, 10, 3},    {98, 10, 3},    {99, 10, 3},    {102, 10, 3},   {103, 10, 3},   {104, 10, 3},   {105, 10, 3},
    {106, 10, 3},   {107, 10, 3},   {108, 10, 3},   {114, 10, 3},   {115, 10, 3},   {118, 10, 3},   {119, 10, 3},   {120, 10, 3},   {121, 10, 3},
    {122, 10, 3},   {123, 10, 3},   {124, 10, 3},   {125, 10, 3},   {126, 10, 3},   {139, 10, 3},   {165, 10, 3},   {166, 10, 3},   {168, 10, 3},
    {169, 10, 3},   {170, 10, 3},   {72, 11, 3},    {73, 11, 3},    {75, 11, 3},    {76, 11, 3},    {77, 11, 3},    {78, 11, 3},    {79, 11, 3},
    {92, 11, 3},    {93, 11, 3},    {97, 11, 3},    {98, 11, 3},    {99, 11, 3},    {102, 11, 3},   {103, 11, 3},   {104, 11, 3},   {105, 11, 3},
    {106, 11, 3},   {107, 11, 3},   {108, 11, 3},   {109, 11, 3},   {114, 11, 3},   {115, 11, 3},   {119, 11, 3},   {120, 11, 3},   {121, 11, 3},
    {122, 11, 3},   {123, 11, 3},   {124, 11, 3},   {125, 11, 3},   {162, 11, 3},   {165, 11, 3},   {166, 11, 3},   {167, 11, 3},   {169, 11, 3},
    {74, 12, 3},    {75, 12, 3},    {76, 12, 3},    {77, 12, 3},    {78, 12, 3},    {79, 12, 3},    {80, 12, 3},    {92, 12, 3},    {93, 12, 3},
    {97, 12, 3},    {98, 12, 3},    {99, 12, 3},    {100, 12, 3},   {101, 12, 3},   {102, 12, 3},   {103, 12, 3},   {104, 12, 3},   {105, 12, 3},
    {106, 12, 3},   {107, 12, 3},   {108, 12, 3},   {109, 12, 3},   {119, 12, 3},   {120, 12, 3},   {121, 12, 3},   {122, 12, 3},   {123, 12, 3},
    {124, 12, 3},   {125, 12, 3},   {170, 12, 3},   {74, 13, 3},    {75, 13, 3},    {76, 13, 3},    {77, 13, 3},    {78, 13, 3},    {79, 13, 3},
    {80, 13, 3},    {92, 13, 3},    {93, 13, 3},    {94, 13, 3},    {97, 13, 3},    {98, 13, 3},    {99, 13, 3},    {100, 13, 3},   {101, 13, 3},
    {102, 13, 3},   {103, 13, 3},   {104, 13, 3},   {105, 13, 3},   {106, 13, 3},   {107, 13, 3},   {108, 13, 3},   {109, 13, 3},   {120, 13, 3},
    {121, 13, 3},   {122, 13, 3},   {123, 13, 3},   {124, 13, 3},   {144, 13, 3},   {74, 14, 3},    {75, 14, 3},    {76, 14, 3},    {77, 14, 3},
    {78, 14, 3},    {79, 14, 3},    {80, 14, 3},    {93, 14, 3},    {97, 14, 3},    {98, 14, 3},    {99, 14, 3},    {100, 14, 3},   {101, 14, 3},
    {102, 14, 3},   {103, 14, 3},   {104, 14, 3},   {105, 14, 3},   {106, 14, 3},   {107, 14, 3},   {108, 14, 3},   {109, 14, 3},   {120, 14, 3},
    {121, 14, 3},   {122, 14, 3},   {123, 14, 3},   {124, 14, 3},   {145, 14, 3},   {168, 14, 3},   {169, 14, 3},   {73, 15, 3},    {74, 15, 3},
    {75, 15, 3},    {76, 15, 3},    {77, 15, 3},    {78, 15, 3},    {79, 15, 3},    {80, 15, 3},    {81, 15, 3},    {94, 15, 3},    {95, 15, 3},
    {97, 15, 3},    {98, 15, 3},    {99, 15, 3},    {100, 15, 3},   {101, 15, 3},   {102, 15, 3},   {103, 15, 3},   {104, 15, 3},   {105, 15, 3},
    {106, 15, 3},   {107, 15, 3},   {108, 15, 3},   {109, 15, 3},   {111, 15, 3},   {119, 15, 3},   {120, 15, 3},   {121, 15, 3},   {122, 15, 3},
    {145, 15, 3},   {73, 16, 3},    {74, 16, 3},    {75, 16, 3},    {76, 16, 3},    {77, 16, 3},    {78, 16, 3},    {79, 16, 3},    {80, 16, 3},
    {81, 16, 3},    {82, 16, 3},    {94, 16, 3},    {95, 16, 3},    {96, 16, 3},    {97, 16, 3},    {98, 16, 3},    {99, 16, 3},    {100, 16, 3},
    {101, 16, 3},   {102, 16, 3},   {103, 16, 3},   {104, 16, 3},   {105, 16, 3},   {106, 16, 3},   {107, 16, 3},   {108, 16, 3},   {111, 16, 3},
    {112, 16, 3},   {119, 16, 3},   {120, 16, 3},   {121, 16, 3},   {122, 16, 3},   {145, 16, 3},   {146, 16, 3},   {73, 17, 3},    {74, 17, 3},
    {75, 17, 3},    {76, 17, 3},    {77, 17, 3},    {78, 17, 3},    {79, 17, 3},    {80, 17, 3},    {81, 17, 3},    {82, 17, 3},    {83, 17, 3},
    {94, 17, 3},    {95, 17, 3},    {96, 17, 3},    {97, 17, 3},    {98, 17, 3},    {99, 17, 3},    {100, 17, 3},   {101, 17, 3},   {102, 17, 3},
    {103, 17, 3},   {104, 17, 3},   {105, 17, 3},   {106, 17, 3},   {107, 17, 3},   {120, 17, 3},   {121, 17, 3},   {122, 17, 3},   {145, 17, 3},
    {72, 18, 3},    {73, 18, 3},    {74, 18, 3},    {75, 18, 3},    {76, 18, 3},    {77, 18, 3},    {78, 18, 3},    {79, 18, 3},    {80, 18, 3},
    {81, 18, 3},    {82, 18, 3},    {83, 18, 3},    {84, 18, 3},    {93, 18, 3},    {94, 18, 3},    {95, 18, 3},    {96, 18, 3},    {97, 18, 3},
    {98, 18, 3},    {99, 18, 3},    {100, 18, 3},   {101, 18, 3},   {102, 18, 3},   {103, 18, 3},   {104, 18, 3},   {105, 18, 3},   {106, 18, 3},
    {108, 18, 3},   {109, 18, 3},   {110, 18, 3},   {120, 18, 3},   {121, 18, 3},   {122, 18, 3},   {145, 18, 3},   {72, 19, 3},    {73, 19, 3},
    {74, 19, 3},    {75, 19, 3},    {76, 19, 3},    {77, 19, 3},    {78, 19, 3},    {79, 19, 3},    {80, 19, 3},    {81, 19, 3},    {82, 19, 3},
    {83, 19, 3},    {84, 19, 3},    {85, 19, 3},    {86, 19, 3},    {92, 19, 3},    {93, 19, 3},    {94, 19, 3},    {95, 19, 3},    {96, 19, 3},
    {97, 19, 3},    {98, 19, 3},    {99, 19, 3},    {100, 19, 3},   {101, 19, 3},   {102, 19, 3},   {103, 19, 3},   {104, 19, 3},   {105, 19, 3},
    {106, 19, 3},   {108, 19, 3},   {109, 19, 3},   {110, 19, 3},   {111, 19, 3},   {121, 19, 3},   {122, 19, 3},   {145, 19, 3},   {166, 19, 3},
    {70, 20, 3},    {71, 20, 3},    {72, 20, 3},    {73, 20, 3},    {74, 20, 3},    {75, 20, 3},    {76, 20, 3},    {77, 20, 3},    {78, 20, 3},
    {79, 20, 3},    {80, 20, 3},    {81, 20, 3},    {82, 20, 3},    {83, 20, 3},    {84, 20, 3},    {85, 20, 3},    {86, 20, 3},    {87, 20, 3},
    {92, 20, 3},    {93, 20, 3},    {94, 20, 3},    {95, 20, 3},    {96, 20, 3},    {97, 20, 3},    {98, 20, 3},    {99, 20, 3},    {100, 20, 3},
    {101, 20, 3},   {102, 20, 3},   {103, 20, 3},   {104, 20, 3},   {105, 20, 3},   {106, 20, 3},   {107, 20, 3},   {109, 20, 3},   {110, 20, 3},
    {116, 20, 3},   {121, 20, 3},   {122, 20, 3},   {136, 20, 3},   {144, 20, 3},   {145, 20, 3},   {69, 21, 3},    {70, 21, 3},    {71, 21, 3},
    {72, 21, 3},    {73, 21, 3},    {74, 21, 3},    {75, 21, 3},    {76, 21, 3},    {77, 21, 3},    {78, 21, 3},    {79, 21, 3},    {80, 21, 3},
    {81, 21, 3},    {82, 21, 3},    {83, 21, 3},    {84, 21, 3},    {85, 21, 3},    {86, 21, 3},    {87, 21, 3},    {88, 21, 3},    {89, 21, 3},
    {90, 21, 3},    {91, 21, 3},    {92, 21, 3},    {93, 21, 3},    {94, 21, 3},    {95, 21, 3},    {96, 21, 3},    {97, 21, 3},    {98, 21, 3},
    {99, 21, 3},    {100, 21, 3},   {101, 21, 3},   {102, 21, 3},   {103, 21, 3},   {104, 21, 3},   {105, 21, 3},   {106, 21, 3},   {107, 21, 3},
    {108, 21, 3},   {109, 21, 3},   {110, 21, 3},   {111, 21, 3},   {112, 21, 3},   {113, 21, 3},   {114, 21, 3},   {120, 21, 3},   {121, 21, 3},
    {68, 22, 3},    {69, 22, 3},    {70, 22, 3},    {71, 22, 3},    {72, 22, 3},    {73, 22, 3},    {74, 22, 3},    {75, 22, 3},    {76, 22, 3},
    {77, 22, 3},    {78, 22, 3},    {79, 22, 3},    {80, 22, 3},    {81, 22, 3},    {82, 22, 3},    {83, 22, 3},    {84, 22, 3},    {85, 22, 3},
    {86, 22, 3},    {87, 22, 3},    {88, 22, 3},    {89, 22, 3},    {90, 22, 3},    {91, 22, 3},    {92, 22, 3},    {93, 22, 3},    {94, 22, 3},
    {95, 22, 3},    {96, 22, 3},    {97, 22, 3},    {98, 22, 3},    {99, 22, 3},    {100, 22, 3},   {101, 22, 3},   {102, 22, 3},   {103, 22, 3},
    {104, 22, 3},   {105, 22, 3},   {106, 22, 3},   {107, 22, 3},   {108, 22, 3},   {109, 22, 3},   {110, 22, 3},   {111, 22, 3},   {112, 22, 3},
    {113, 22, 3},   {114, 22, 3},   {115, 22, 3},   {116, 22, 3},   {120, 22, 3},   {121, 22, 3},   {67, 23, 3},    {68, 23, 3},    {69, 23, 3},
    {70, 23, 3},    {71, 23, 3},    {72, 23, 3},    {73, 23, 3},    {74, 23, 3},    {75, 23, 3},    {76, 23, 3},    {77, 23, 3},    {78, 23, 3},
    {79, 23, 3},    {80, 23, 3},    {81, 23, 3},    {82, 23, 3},    {83, 23, 3},    {84, 23, 3},    {85, 23, 3},    {86, 23, 3},    {87, 23, 3},
    {88, 23, 3},    {89, 23, 3},    {90, 23, 3},    {91, 23, 3},    {92, 23, 3},    {93, 23, 3},    {94, 23, 3},    {95, 23, 3},    {96, 23, 3},
    {97, 23, 3},    {98, 23, 3},    {99, 23, 3},    {100, 23, 3},   {101, 23, 3},   {102, 23, 3},   {103, 23, 3},   {104, 23, 3},   {105, 23, 3},
    {106, 23, 3},   {107, 23, 3},   {108, 23, 3},   {109, 23, 3},   {110, 23, 3},   {111, 23, 3},   {112, 23, 3},   {113, 23, 3},   {114, 23, 3},
    {115, 23, 3},   {116, 23, 3},   {117, 23, 3},   {119, 23, 3},   {120, 23, 3},   {121, 23, 3},   {66, 24, 3},    {67, 24, 3},    {68, 24, 3},
    {69, 24, 3},    {70, 24, 3},    {71, 24, 3},    {72, 24, 3},    {73, 24, 3},    {74, 24, 3},    {75, 24, 3},    {76, 24, 3},    {77, 24, 3},
    {78, 24, 3},    {79, 24, 3},    {80, 24, 3},    {81, 24, 3},    {82, 24, 3},    {83, 24, 3},    {84, 24, 3},    {85, 24, 3},    {86, 24, 3},
    {87, 24, 3},    {88, 24, 3},    {89, 24, 3},    {90, 24, 3},    {91, 24, 3},    {92, 24, 3},    {93, 24, 3},    {94, 24, 3},    {95, 24, 3},
    {96, 24, 3},    {97, 24, 3},    {98, 24, 3},    {99, 24, 3},    {100, 24, 3},   {101, 24, 3},   {102, 24, 3},   {103, 24, 3},   {104, 24, 3},
    {105, 24, 3},   {106, 24, 3},   {107, 24, 3},   {108, 24, 3},   {109, 24, 3},   {110, 24, 3},   {111, 24, 3},   {112, 24, 3},   {113, 24, 3},
    {114, 24, 3},   {115, 24, 3},   {116, 24, 3},   {117, 24, 3},   {118, 24, 3},   {119, 24, 3},   {120, 24, 3},   {121, 24, 3},   {122, 24, 3},
    {123, 24, 3},   {124, 24, 3},   {125, 24, 3},   {131, 24, 3},   {141, 24, 3},   {153, 24, 3},   {60, 25, 3},    {61, 25, 3},    {62, 25, 3},
    {63, 25, 3},    {64, 25, 3},    {65, 25, 3},    {66, 25, 3},    {67, 25, 3},    {68, 25, 3},    {69, 25, 3},    {70, 25, 3},    {71, 25, 3},
    {72, 25, 3},    {73, 25, 3},    {74, 25, 3},    {75, 25, 3},    {76, 25, 3},    {77, 25, 3},    {78, 25, 3},    {79, 25, 3},    {80, 25, 3},
    {81, 25, 3},    {82, 25, 3},    {83, 25, 3},    {84, 25, 3},    {85, 25, 3},    {86, 25, 3},    {87, 25, 3},    {88, 25, 3},    {89, 25, 3},
    {90, 25, 3},    {91, 25, 3},    {92, 25, 3},    {93, 25, 3},    {94, 25, 3},    {95, 25, 3},    {96, 25, 3},    {97, 25, 3},    {98, 25, 3},
    {99, 25, 3},    {100, 25, 3},   {101, 25, 3},   {102, 25, 3},   {103, 25, 3},   {104, 25, 3},   {105, 25, 3},   {106, 25, 3},   {107, 25, 3},
    {108, 25, 3},   {109, 25, 3},   {110, 25, 3},   {111, 25, 3},   {112, 25, 3},   {113, 25, 3},   {114, 25, 3},   {115, 25, 3},   {116, 25, 3},
    {117, 25, 3},   {118, 25, 3},   {119, 25, 3},   {121, 25, 3},   {122, 25, 3},   {123, 25, 3},   {124, 25, 3},   {131, 25, 3},   {141, 25, 3},
    {60, 26, 3},    {61, 26, 3},    {62, 26, 3},    {63, 26, 3},    {64, 26, 3},    {65, 26, 3},    {66, 26, 3},    {67, 26, 3},    {68, 26, 3},
    {69, 26, 3},    {70, 26, 3},    {71, 26, 3},    {72, 26, 3},    {73, 26, 3},    {74, 26, 3},    {75, 26, 3},    {76, 26, 3},    {77, 26, 3},
    {78, 26, 3},    {79, 26, 3},    {80, 26, 3},    {81, 26, 3},    {82, 26, 3},    {83, 26, 3},    {84, 26, 3},    {85, 26, 3},    {86, 26, 3},
    {87, 26, 3},    {88, 26, 3},    {89, 26, 3},    {90, 26, 3},    {91, 26, 3},    {92, 26, 3},    {93, 26, 3},    {94, 26, 3},    {95, 26, 3},
    {96, 26, 3},    {97, 26, 3},    {98, 26, 3},    {99, 26, 3},    {100, 26, 3},   {101, 26, 3},   {102, 26, 3},   {103, 26, 3},   {104, 26, 3},
    {105, 26, 3},   {106, 26, 3},   {107, 26, 3},   {108, 26, 3},   {109, 26, 3},   {110, 26, 3},   {111, 26, 3},   {112, 26, 3},   {113, 26, 3},
    {114, 26, 3},   {115, 26, 3},   {116, 26, 3},   {117, 26, 3},   {118, 26, 3},   {119, 26, 3},   {120, 26, 3},   {126, 26, 3},   {127, 26, 3},
    {128, 26, 3},   {142, 26, 3},   {60, 27, 3},    {61, 27, 3},    {62, 27, 3},    {63, 27, 3},    {64, 27, 3},    {65, 27, 3},    {66, 27, 3},
    {67, 27, 3},    {68, 27, 3},    {69, 27, 3},    {70, 27, 3},    {71, 27, 3},    {72, 27, 3},    {73, 27, 3},    {74, 27, 3},    {75, 27, 3},
    {76, 27, 3},    {77, 27, 3},    {78, 27, 3},    {79, 27, 3},    {80, 27, 3},    {81, 27, 3},    {82, 27, 3},    {83, 27, 3},    {84, 27, 3},
    {85, 27, 3},    {86, 27, 3},    {87, 27, 3},    {88, 27, 3},    {89, 27, 3},    {90, 27, 3},    {91, 27, 3},    {92, 27, 3},    {93, 27, 3},
    {94, 27, 3},    {95, 27, 3},    {96, 27, 3},    {97, 27, 3},    {98, 27, 3},    {99, 27, 3},    {100, 27, 3},   {101, 27, 3},   {102, 27, 3},
    {103, 27, 3},   {104, 27, 3},   {105, 27, 3},   {106, 27, 3},   {107, 27, 3},   {108, 27, 3},   {109, 27, 3},   {110, 27, 3},   {111, 27, 3},
    {112, 27, 3},   {113, 27, 3},   {114, 27, 3},   {115, 27, 3},   {116, 27, 3},   {117, 27, 3},   {118, 27, 3},   {119, 27, 3},   {120, 27, 3},
    {121, 27, 3},   {127, 27, 3},   {128, 27, 3},   {129, 27, 3},   {140, 27, 3},   {142, 27, 3},   {60, 28, 3},    {61, 28, 3},    {62, 28, 3},
    {63, 28, 3},    {64, 28, 3},    {65, 28, 3},    {66, 28, 3},    {67, 28, 3},    {68, 28, 3},    {69, 28, 3},    {70, 28, 3},    {71, 28, 3},
    {72, 28, 3},    {73, 28, 3},    {74, 28, 3},    {75, 28, 3},    {76, 28, 3},    {77, 28, 3},    {78, 28, 3},    {79, 28, 3},    {80, 28, 3},
    {81, 28, 3},    {82, 28, 3},    {83, 28, 3},    {84, 28, 3},    {85, 28, 3},    {86, 28, 3},    {87, 28, 3},    {88, 28, 3},    {89, 28, 3},
    {90, 28, 3},    {91, 28, 3},    {92, 28, 3},    {93, 28, 3},    {94, 28, 3},    {95, 28, 3},    {96, 28, 3},    {97, 28, 3},    {98, 28, 3},
    {99, 28, 3},    {100, 28, 3},   {101, 28, 3},   {102, 28, 3},   {103, 28, 3},   {104, 28, 3},   {105, 28, 3},   {106, 28, 3},   {107, 28, 3},
    {108, 28, 3},   {109, 28, 3},   {110, 28, 3},   {111, 28, 3},   {112, 28, 3},   {113, 28, 3},   {114, 28, 3},   {115, 28, 3},   {116, 28, 3},
    {117, 28, 3},   {118, 28, 3},   {119, 28, 3},   {120, 28, 3},   {121, 28, 3},   {122, 28, 3},   {128, 28, 3},   {129, 28, 3},   {130, 28, 3},
    {60, 29, 3},    {61, 29, 3},    {62, 29, 3},    {63, 29, 3},    {64, 29, 3},    {65, 29, 3},    {66, 29, 3},    {67, 29, 3},    {68, 29, 3},
    {69, 29, 3},    {70, 29, 3},    {71, 29, 3},    {72, 29, 3},    {73, 29, 3},    {74, 29, 3},    {75, 29, 3},    {76, 29, 3},    {77, 29, 3},
    {78, 29, 3},    {79, 29, 3},    {80, 29, 3},    {81, 29, 3},    {82, 29, 3},    {83, 29, 3},    {84, 29, 3},    {85, 29, 3},    {86, 29, 3},
    {87, 29, 3},    {88, 29, 3},    {89, 29, 3},    {90, 29, 3},    {91, 29, 3},    {92, 29, 3},    {93, 29, 3},    {94, 29, 3},    {95, 29, 3},
    {96, 29, 3},    {97, 29, 3},    {98, 29, 3},    {99, 29, 3},    {100, 29, 3},   {101, 29, 3},   {102, 29, 3},   {103, 29, 3},   {104, 29, 3},
    {105, 29, 3},   {106, 29, 3},   {107, 29, 3},   {108, 29, 3},   {109, 29, 3},   {110, 29, 3},   {111, 29, 3},   {112, 29, 3},   {113, 29, 3},
    {114, 29, 3},   {115, 29, 3},   {116, 29, 3},   {117, 29, 3},   {118, 29, 3},   {119, 29, 3},   {120, 29, 3},   {121, 29, 3},   {122, 29, 3},
    {129, 29, 3},   {140, 29, 3},   {60, 30, 3},    {61, 30, 3},    {62, 30, 3},    {63, 30, 3},    {64, 30, 3},    {65, 30, 3},    {66, 30, 3},
    {67, 30, 3},    {68, 30, 3},    {69, 30, 3},    {70, 30, 3},    {71, 30, 3},    {72, 30, 3},    {73, 30, 3},    {74, 30, 3},    {75, 30, 3},
    {76, 30, 3},    {77, 30, 3},    {78, 30, 3},    {79, 30, 3},    {80, 30, 3},    {81, 30, 3},    {82, 30, 3},    {83, 30, 3},    {84, 30, 3},
    {85, 30, 3},    {86, 30, 3},    {87, 30, 3},    {88, 30, 3},    {89, 30, 3},    {90, 30, 3},    {91, 30, 3},    {92, 30, 3},    {93, 30, 3},
    {94, 30, 3},    {95, 30, 3},    {96, 30, 3},    {97, 30, 3},    {98, 30, 3},    {99, 30, 3},    {100, 30, 3},   {101, 30, 3},   {102, 30, 3},
    {103, 30, 3},   {104, 30, 3},   {105, 30, 3},   {106, 30, 3},   {107, 30, 3},   {108, 30, 3},   {109, 30, 3},   {110, 30, 3},   {111, 30, 3},
    {112, 30, 3},   {113, 30, 3},   {114, 30, 3},   {115, 30, 3},   {116, 30, 3},   {117, 30, 3},   {118, 30, 3},   {119, 30, 3},   {120, 30, 3},
    {121, 30, 3},   {122, 30, 3},   {129, 30, 3},   {130, 30, 3},   {131, 30, 3},   {140, 30, 3},   {60, 31, 3},    {61, 31, 3},    {62, 31, 3},
    {63, 31, 3},    {64, 31, 3},    {65, 31, 3},    {66, 31, 3},    {67, 31, 3},    {68, 31, 3},    {69, 31, 3},    {70, 31, 3},    {71, 31, 3},
    {72, 31, 3},    {73, 31, 3},    {74, 31, 3},    {75, 31, 3},    {76, 31, 3},    {77, 31, 3},    {78, 31, 3},    {79, 31, 3},    {80, 31, 3},
    {81, 31, 3},    {82, 31, 3},    {83, 31, 3},    {84, 31, 3},    {85, 31, 3},    {86, 31, 3},    {87, 31, 3},    {88, 31, 3},    {89, 31, 3},
    {90, 31, 3},    {91, 31, 3},    {92, 31, 3},    {93, 31, 3},    {94, 31, 3},    {95, 31, 3},    {96, 31, 3},    {97, 31, 3},    {98, 31, 3},
    {99, 31, 3},    {100, 31, 3},   {101, 31, 3},   {102, 31, 3},   {103, 31, 3},   {104, 31, 3},   {105, 31, 3},   {106, 31, 3},   {107, 31, 3},
    {108, 31, 3},   {109, 31, 3},   {110, 31, 3},   {111, 31, 3},   {112, 31, 3},   {113, 31, 3},   {114, 31, 3},   {115, 31, 3},   {116, 31, 3},
    {117, 31, 3},   {118, 31, 3},   {119, 31, 3},   {120, 31, 3},   {121, 31, 3},   {122, 31, 3},   {128, 31, 3},   {129, 31, 3},   {130, 31, 3},
    {131, 31, 3},   {139, 31, 3},   {140, 31, 3},   {60, 32, 3},    {61, 32, 3},    {62, 32, 3},    {63, 32, 3},    {64, 32, 3},    {65, 32, 3},
    {66, 32, 3},    {67, 32, 3},    {68, 32, 3},    {69, 32, 3},    {70, 32, 3},    {71, 32, 3},    {72, 32, 3},    {73, 32, 3},    {74, 32, 3},
    {75, 32, 3},    {76, 32, 3},    {77, 32, 3},    {78, 32, 3},    {79, 32, 3},    {80, 32, 3},    {81, 32, 3},    {82, 32, 3},    {83, 32, 3},
    {84, 32, 3},    {85, 32, 3},    {86, 32, 3},    {87, 32, 3},    {88, 32, 3},    {89, 32, 3},    {90, 32, 3},    {91, 32, 3},    {92, 32, 3},
    {93, 32, 3},    {94, 32, 3},    {95, 32, 3},    {96, 32, 3},    {97, 32, 3},    {98, 32, 3},    {99, 32, 3},    {100, 32, 3},   {101, 32, 3},
    {102, 32, 3},   {103, 32, 3},   {104, 32, 3},   {105, 32, 3},   {106, 32, 3},   {107, 32, 3},   {108, 32, 3},   {109, 32, 3},   {110, 32, 3},
    {111, 32, 3},   {112, 32, 3},   {113, 32, 3},   {114, 32, 3},   {115, 32, 3},   {116, 32, 3},   {117, 32, 3},   {118, 32, 3},   {119, 32, 3},
    {120, 32, 3},   {121, 32, 3},   {128, 32, 3},   {129, 32, 3},   {130, 32, 3},   {131, 32, 3},   {132, 32, 3},   {133, 32, 3},   {139, 32, 3},
    {60, 33, 3},    {61, 33, 3},    {62, 33, 3},    {63, 33, 3},    {64, 33, 3},    {65, 33, 3},    {66, 33, 3},    {67, 33, 3},    {68, 33, 3},
    {69, 33, 3},    {70, 33, 3},    {71, 33, 3},    {72, 33, 3},    {73, 33, 3},    {74, 33, 3},    {75, 33, 3},    {76, 33, 3},    {77, 33, 3},
    {78, 33, 3},    {79, 33, 3},    {80, 33, 3},    {81, 33, 3},    {82, 33, 3},    {83, 33, 3},    {84, 33, 3},    {85, 33, 3},    {86, 33, 3},
    {87, 33, 3},    {88, 33, 3},    {89, 33, 3},    {90, 33, 3},    {91, 33, 3},    {92, 33, 3},    {93, 33, 3},    {94, 33, 3},    {95, 33, 3},
    {96, 33, 3},    {97, 33, 3},    {98, 33, 3},    {99, 33, 3},    {100, 33, 3},   {101, 33, 3},   {102, 33, 3},   {103, 33, 3},   {104, 33, 3},
    {105, 33, 3},   {106, 33, 3},   {107, 33, 3},   {108, 33, 3},   {109, 33, 3},   {110, 33, 3},   {111, 33, 3},   {112, 33, 3},   {113, 33, 3},
    {114, 33, 3},   {115, 33, 3},   {116, 33, 3},   {117, 33, 3},   {118, 33, 3},   {119, 33, 3},   {120, 33, 3},   {126, 33, 3},   {128, 33, 3},
    {129, 33, 3},   {130, 33, 3},   {131, 33, 3},   {132, 33, 3},   {133, 33, 3},   {134, 33, 3},   {135, 33, 3},   {136, 33, 3},   {138, 33, 3},
    {139, 33, 3},   {60, 34, 3},    {61, 34, 3},    {62, 34, 3},    {63, 34, 3},    {64, 34, 3},    {65, 34, 3},    {66, 34, 3},    {67, 34, 3},
    {68, 34, 3},    {69, 34, 3},    {70, 34, 3},    {71, 34, 3},    {72, 34, 3},    {73, 34, 3},    {74, 34, 3},    {75, 34, 3},    {76, 34, 3},
    {77, 34, 3},    {78, 34, 3},    {79, 34, 3},    {80, 34, 3},    {81, 34, 3},    {82, 34, 3},    {83, 34, 3},    {84, 34, 3},    {85, 34, 3},
    {86, 34, 3},    {87, 34, 3},    {88, 34, 3},    {89, 34, 3},    {90, 34, 3},    {91, 34, 3},    {92, 34, 3},    {93, 34, 3},    {94, 34, 3},
    {95, 34, 3},    {96, 34, 3},    {97, 34, 3},    {98, 34, 3},    {99, 34, 3},    {100, 34, 3},   {101, 34, 3},   {102, 34, 3},   {103, 34, 3},
    {104, 34, 3},   {105, 34, 3},   {106, 34, 3},   {107, 34, 3},   {108, 34, 3},   {109, 34, 3},   {110, 34, 3},   {111, 34, 3},   {112, 34, 3},
    {113, 34, 3},   {114, 34, 3},   {115, 34, 3},   {116, 34, 3},   {117, 34, 3},   {118, 34, 3},   {119, 34, 3},   {120, 34, 3},   {125, 34, 3},
    {126, 34, 3},   {127, 34, 3},   {128, 34, 3},   {129, 34, 3},   {130, 34, 3},   {131, 34, 3},   {132, 34, 3},   {133, 34, 3},   {134, 34, 3},
    {135, 34, 3},   {136, 34, 3},   {137, 34, 3},   {138, 34, 3},   {139, 34, 3},   {0, 35, 3},     {1, 35, 3},     {2, 35, 3},     {3, 35, 3},
    {4, 35, 3},     {5, 35, 3},     {6, 35, 3},     {7, 35, 3},     {8, 35, 3},     {9, 35, 3},     {10, 35, 3},    {11, 35, 3},    {12, 35, 3},
    {14, 35, 3},    {23, 35, 3},    {24, 35, 3},    {25, 35, 3},    {26, 35, 3},    {27, 35, 3},    {32, 35, 3},    {33, 35, 3},    {34, 35, 3},
    {35, 35, 3},    {36, 35, 3},    {37, 35, 3},    {38, 35, 3},    {39, 35, 3},    {40, 35, 3},    {41, 35, 3},    {42, 35, 3},    {43, 35, 3},
    {44, 35, 3},    {45, 35, 3},    {46, 35, 3},    {47, 35, 3},    {48, 35, 3},    {49, 35, 3},    {50, 35, 3},    {51, 35, 3},    {52, 35, 3},
    {53, 35, 3},    {54, 35, 3},    {55, 35, 3},    {56, 35, 3},    {57, 35, 3},    {58, 35, 3},    {59, 35, 3},    {60, 35, 3},    {61, 35, 3},
    {62, 35, 3},    {63, 35, 3},    {64, 35, 3},    {65, 35, 3},    {66, 35, 3},    {67, 35, 3},    {68, 35, 3},    {69, 35, 3},    {70, 35, 3},
    {71, 35, 3},    {72, 35, 3},    {73, 35, 3},    {74, 35, 3},    {75, 35, 3},    {76, 35, 3},    {77, 35, 3},    {78, 35, 3},    {79, 35, 3},
    {80, 35, 3},    {81, 35, 3},    {82, 35, 3},    {83, 35, 3},    {84, 35, 3},    {85, 35, 3},    {86, 35, 3},    {87, 35, 3},    {88, 35, 3},
    {89, 35, 3},    {90, 35, 3},    {91, 35, 3},    {92, 35, 3},    {93, 35, 3},    {94, 35, 3},    {95, 35, 3},    {96, 35, 3},    {97, 35, 3},
    {98, 35, 3},    {99, 35, 3},    {100, 35, 3},   {101, 35, 3},   {102, 35, 3},   {103, 35, 3},   {104, 35, 3},   {105, 35, 3},   {106, 35, 3},
    {107, 35, 3},   {108, 35, 3},   {109, 35, 3},   {110, 35, 3},   {111, 35, 3},   {112, 35, 3},   {113, 35, 3},   {114, 35, 3},   {115, 35, 3},
    {116, 35, 3},   {117, 35, 3},   {118, 35, 3},   {119, 35, 3},   {120, 35, 3},   {125, 35, 3},   {126, 35, 3},   {127, 35, 3},   {128, 35, 3},
    {129, 35, 3},   {132, 35, 3},   {133, 35, 3},   {134, 35, 3},   {135, 35, 3},   {136, 35, 3},   {137, 35, 3},   {138, 35, 3},   {139, 35, 3},
    {140, 35, 3},   {-1, 35, 3},    {-2, 35, 3},    {-3, 35, 3},    {-4, 35, 3},    {-5, 35, 3},    {-6, 35, 3},    {-7, 35, 3},    {0, 36, 3},
    {1, 36, 3},     {2, 36, 3},     {3, 36, 3},     {4, 36, 3},     {5, 36, 3},     {6, 36, 3},     {7, 36, 3},     {8, 36, 3},     {9, 36, 3},
    {10, 36, 3},    {11, 36, 3},    {12, 36, 3},    {14, 36, 3},    {15, 36, 3},    {21, 36, 3},    {22, 36, 3},    {23, 36, 3},    {24, 36, 3},
    {25, 36, 3},    {26, 36, 3},    {27, 36, 3},    {28, 36, 3},    {29, 36, 3},    {30, 36, 3},    {31, 36, 3},    {32, 36, 3},    {33, 36, 3},
    {34, 36, 3},    {35, 36, 3},    {36, 36, 3},    {37, 36, 3},    {38, 36, 3},    {39, 36, 3},    {40, 36, 3},    {41, 36, 3},    {42, 36, 3},
    {43, 36, 3},    {44, 36, 3},    {45, 36, 3},    {46, 36, 3},    {47, 36, 3},    {48, 36, 3},    {49, 36, 3},    {50, 36, 3},    {51, 36, 3},
    {52, 36, 3},    {53, 36, 3},    {54, 36, 3},    {55, 36, 3},    {56, 36, 3},    {57, 36, 3},    {58, 36, 3},    {59, 36, 3},    {60, 36, 3},
    {61, 36, 3},    {62, 36, 3},    {63, 36, 3},    {64, 36, 3},    {65, 36, 3},    {66, 36, 3},    {67, 36, 3},    {68, 36, 3},    {69, 36, 3},
    {70, 36, 3},    {71, 36, 3},    {72, 36, 3},    {73, 36, 3},    {74, 36, 3},    {75, 36, 3},    {76, 36, 3},    {77, 36, 3},    {78, 36, 3},
    {79, 36, 3},    {80, 36, 3},    {81, 36, 3},    {82, 36, 3},    {83, 36, 3},    {84, 36, 3},    {85, 36, 3},    {86, 36, 3},    {87, 36, 3},
    {88, 36, 3},    {89, 36, 3},    {90, 36, 3},    {91, 36, 3},    {92, 36, 3},    {93, 36, 3},    {94, 36, 3},    {95, 36, 3},    {96, 36, 3},
    {97, 36, 3},    {98, 36, 3},    {99, 36, 3},    {100, 36, 3},   {101, 36, 3},   {102, 36, 3},   {103, 36, 3},   {104, 36, 3},   {105, 36, 3},
    {106, 36, 3},   {107, 36, 3},   {108, 36, 3},   {109, 36, 3},   {110, 36, 3},   {111, 36, 3},   {112, 36, 3},   {113, 36, 3},   {114, 36, 3},
    {115, 36, 3},   {116, 36, 3},   {117, 36, 3},   {118, 36, 3},   {119, 36, 3},   {120, 36, 3},   {121, 36, 3},   {122, 36, 3},   {125, 36, 3},
    {126, 36, 3},   {127, 36, 3},   {128, 36, 3},   {129, 36, 3},   {132, 36, 3},   {133, 36, 3},   {135, 36, 3},   {136, 36, 3},   {137, 36, 3},
    {138, 36, 3},   {139, 36, 3},   {140, 36, 3},   {-2, 36, 3},    {-3, 36, 3},    {-4, 36, 3},    {-5, 36, 3},    {-6, 36, 3},    {-7, 36, 3},
    {-8, 36, 3},    {-9, 36, 3},    {6, 37, 3},     {7, 37, 3},     {8, 37, 3},     {9, 37, 3},     {10, 37, 3},    {11, 37, 3},    {12, 37, 3},
    {13, 37, 3},    {14, 37, 3},    {15, 37, 3},    {16, 37, 3},    {20, 37, 3},    {21, 37, 3},    {22, 37, 3},    {23, 37, 3},    {24, 37, 3},
    {25, 37, 3},    {26, 37, 3},    {27, 37, 3},    {28, 37, 3},    {29, 37, 3},    {30, 37, 3},    {31, 37, 3},    {32, 37, 3},    {33, 37, 3},
    {34, 37, 3},    {35, 37, 3},    {36, 37, 3},    {37, 37, 3},    {38, 37, 3},    {39, 37, 3},    {40, 37, 3},    {41, 37, 3},    {42, 37, 3},
    {43, 37, 3},    {44, 37, 3},    {45, 37, 3},    {46, 37, 3},    {47, 37, 3},    {48, 37, 3},    {49, 37, 3},    {50, 37, 3},    {53, 37, 3},
    {54, 37, 3},    {55, 37, 3},    {56, 37, 3},    {57, 37, 3},    {58, 37, 3},    {59, 37, 3},    {60, 37, 3},    {61, 37, 3},    {62, 37, 3},
    {63, 37, 3},    {64, 37, 3},    {65, 37, 3},    {66, 37, 3},    {67, 37, 3},    {68, 37, 3},    {69, 37, 3},    {70, 37, 3},    {71, 37, 3},
    {72, 37, 3},    {73, 37, 3},    {74, 37, 3},    {75, 37, 3},    {76, 37, 3},    {77, 37, 3},    {78, 37, 3},    {79, 37, 3},    {80, 37, 3},
    {81, 37, 3},    {82, 37, 3},    {83, 37, 3},    {84, 37, 3},    {85, 37, 3},    {86, 37, 3},    {87, 37, 3},    {88, 37, 3},    {89, 37, 3},
    {90, 37, 3},    {91, 37, 3},    {92, 37, 3},    {93, 37, 3},    {94, 37, 3},    {95, 37, 3},    {96, 37, 3},    {97, 37, 3},    {98, 37, 3},
    {99, 37, 3},    {100, 37, 3},   {101, 37, 3},   {102, 37, 3},   {103, 37, 3},   {104, 37, 3},   {105, 37, 3},   {106, 37, 3},   {107, 37, 3},
    {108, 37, 3},   {109, 37, 3},   {110, 37, 3},   {111, 37, 3},   {112, 37, 3},   {113, 37, 3},   {114, 37, 3},   {115, 37, 3},   {116, 37, 3},
    {117, 37, 3},   {118, 37, 3},   {119, 37, 3},   {120, 37, 3},   {121, 37, 3},   {122, 37, 3},   {124, 37, 3},   {125, 37, 3},   {126, 37, 3},
    {127, 37, 3},   {128, 37, 3},   {129, 37, 3},   {130, 37, 3},   {131, 37, 3},   {136, 37, 3},   {137, 37, 3},   {138, 37, 3},   {139, 37, 3},
    {140, 37, 3},   {141, 37, 3},   {-1, 37, 3},    {-2, 37, 3},    {-3, 37, 3},    {-4, 37, 3},    {-5, 37, 3},    {-6, 37, 3},    {-7, 37, 3},
    {-8, 37, 3},    {-9, 37, 3},    {0, 38, 3},     {1, 38, 3},     {8, 38, 3},     {9, 38, 3},     {12, 38, 3},    {13, 38, 3},    {14, 38, 3},
    {15, 38, 3},    {16, 38, 3},    {17, 38, 3},    {20, 38, 3},    {21, 38, 3},    {22, 38, 3},    {23, 38, 3},    {24, 38, 3},    {25, 38, 3},
    {26, 38, 3},    {27, 38, 3},    {28, 38, 3},    {29, 38, 3},    {30, 38, 3},    {31, 38, 3},    {32, 38, 3},    {33, 38, 3},    {34, 38, 3},
    {35, 38, 3},    {36, 38, 3},    {37, 38, 3},    {38, 38, 3},    {39, 38, 3},    {40, 38, 3},    {41, 38, 3},    {42, 38, 3},    {43, 38, 3},
    {44, 38, 3},    {45, 38, 3},    {46, 38, 3},    {47, 38, 3},    {48, 38, 3},    {49, 38, 3},    {53, 38, 3},    {54, 38, 3},    {55, 38, 3},
    {56, 38, 3},    {57, 38, 3},    {58, 38, 3},    {59, 38, 3},    {60, 38, 3},    {61, 38, 3},    {62, 38, 3},    {63, 38, 3},    {64, 38, 3},
    {65, 38, 3},    {66, 38, 3},    {67, 38, 3},    {68, 38, 3},    {69, 38, 3},    {70, 38, 3},    {71, 38, 3},    {72, 38, 3},    {73, 38, 3},
    {74, 38, 3},    {75, 38, 3},    {76, 38, 3},    {77, 38, 3},    {78, 38, 3},    {79, 38, 3},    {80, 38, 3},    {81, 38, 3},    {82, 38, 3},
    {83, 38, 3},    {84, 38, 3},    {85, 38, 3},    {86, 38, 3},    {87, 38, 3},    {88, 38, 3},    {89, 38, 3},    {90, 38, 3},    {91, 38, 3},
    {92, 38, 3},    {93, 38, 3},    {94, 38, 3},    {95, 38, 3},    {96, 38, 3},    {97, 38, 3},    {98, 38, 3},    {99, 38, 3},    {100, 38, 3},
    {101, 38, 3},   {102, 38, 3},   {103, 38, 3},   {104, 38, 3},   {105, 38, 3},   {106, 38, 3},   {107, 38, 3},   {108, 38, 3},   {109, 38, 3},
    {110, 38, 3},   {111, 38, 3},   {112, 38, 3},   {113, 38, 3},   {114, 38, 3},   {115, 38, 3},   {116, 38, 3},   {117, 38, 3},   {118, 38, 3},
    {120, 38, 3},   {121, 38, 3},   {124, 38, 3},   {125, 38, 3},   {126, 38, 3},   {127, 38, 3},   {128, 38, 3},   {138, 38, 3},   {139, 38, 3},
    {140, 38, 3},   {141, 38, 3},   {-1, 38, 3},    {-2, 38, 3},    {-3, 38, 3},    {-4, 38, 3},    {-5, 38, 3},    {-6, 38, 3},    {-7, 38, 3},
    {-8, 38, 3},    {-9, 38, 3},    {-10, 38, 3},   {0, 39, 3},     {1, 39, 3},     {2, 39, 3},     {3, 39, 3},     {4, 39, 3},     {8, 39, 3},
    {9, 39, 3},     {15, 39, 3},    {16, 39, 3},    {17, 39, 3},    {18, 39, 3},    {19, 39, 3},    {20, 39, 3},    {21, 39, 3},    {22, 39, 3},
    {23, 39, 3},    {24, 39, 3},    {25, 39, 3},    {26, 39, 3},    {27, 39, 3},    {28, 39, 3},    {29, 39, 3},    {30, 39, 3},    {31, 39, 3},
    {32, 39, 3},    {33, 39, 3},    {34, 39, 3},    {35, 39, 3},    {36, 39, 3},    {37, 39, 3},    {38, 39, 3},    {39, 39, 3},    {40, 39, 3},
    {41, 39, 3},    {42, 39, 3},    {43, 39, 3},    {44, 39, 3},    {45, 39, 3},    {46, 39, 3},    {47, 39, 3},    {48, 39, 3},    {49, 39, 3},
    {52, 39, 3},    {53, 39, 3},    {54, 39, 3},    {55, 39, 3},    {56, 39, 3},    {57, 39, 3},    {58, 39, 3},    {59, 39, 3},    {60, 39, 3},
    {61, 39, 3},    {62, 39, 3},    {63, 39, 3},    {64, 39, 3},    {65, 39, 3},    {66, 39, 3},    {67, 39, 3},    {68, 39, 3},    {69, 39, 3},
    {70, 39, 3},    {71, 39, 3},    {72, 39, 3},    {73, 39, 3},    {74, 39, 3},    {75, 39, 3},    {76, 39, 3},    {77, 39, 3},    {78, 39, 3},
    {79, 39, 3},    {80, 39, 3},    {81, 39, 3},    {82, 39, 3},    {83, 39, 3},    {84, 39, 3},    {85, 39, 3},    {86, 39, 3},    {87, 39, 3},
    {88, 39, 3},    {89, 39, 3},    {90, 39, 3},    {91, 39, 3},    {92, 39, 3},    {93, 39, 3},    {94, 39, 3},    {95, 39, 3},    {96, 39, 3},
    {97, 39, 3},    {98, 39, 3},    {99, 39, 3},    {100, 39, 3},   {101, 39, 3},   {102, 39, 3},   {103, 39, 3},   {104, 39, 3},   {105, 39, 3},
    {106, 39, 3},   {107, 39, 3},   {108, 39, 3},   {109, 39, 3},   {110, 39, 3},   {111, 39, 3},   {112, 39, 3},   {113, 39, 3},   {114, 39, 3},
    {115, 39, 3},   {116, 39, 3},   {117, 39, 3},   {118, 39, 3},   {119, 39, 3},   {121, 39, 3},   {122, 39, 3},   {123, 39, 3},   {124, 39, 3},
    {125, 39, 3},   {126, 39, 3},   {127, 39, 3},   {128, 39, 3},   {139, 39, 3},   {140, 39, 3},   {141, 39, 3},   {142, 39, 3},   {-1, 39, 3},
    {-2, 39, 3},    {-3, 39, 3},    {-4, 39, 3},    {-5, 39, 3},    {-6, 39, 3},    {-7, 39, 3},    {-8, 39, 3},    {-9, 39, 3},    {-10, 39, 3},
    {0, 40, 3},     {3, 40, 3},     {4, 40, 3},     {8, 40, 3},     {9, 40, 3},     {12, 40, 3},    {13, 40, 3},    {14, 40, 3},    {15, 40, 3},
    {16, 40, 3},    {17, 40, 3},    {18, 40, 3},    {19, 40, 3},    {20, 40, 3},    {21, 40, 3},    {22, 40, 3},    {23, 40, 3},    {24, 40, 3},
    {25, 40, 3},    {26, 40, 3},    {27, 40, 3},    {28, 40, 3},    {29, 40, 3},    {30, 40, 3},    {31, 40, 3},    {32, 40, 3},    {33, 40, 3},
    {34, 40, 3},    {35, 40, 3},    {36, 40, 3},    {37, 40, 3},    {38, 40, 3},    {39, 40, 3},    {40, 40, 3},    {41, 40, 3},    {42, 40, 3},
    {43, 40, 3},    {44, 40, 3},    {45, 40, 3},    {46, 40, 3},    {47, 40, 3},    {48, 40, 3},    {49, 40, 3},    {50, 40, 3},    {52, 40, 3},
    {53, 40, 3},    {54, 40, 3},    {55, 40, 3},    {56, 40, 3},    {57, 40, 3},    {58, 40, 3},    {59, 40, 3},    {60, 40, 3},    {61, 40, 3},
    {62, 40, 3},    {63, 40, 3},    {64, 40, 3},    {65, 40, 3},    {66, 40, 3},    {67, 40, 3},    {68, 40, 3},    {69, 40, 3},    {70, 40, 3},
    {71, 40, 3},    {72, 40, 3},    {73, 40, 3},    {74, 40, 3},    {75, 40, 3},    {76, 40, 3},    {77, 40, 3},    {78, 40, 3},    {79, 40, 3},
    {80, 40, 3},    {81, 40, 3},    {82, 40, 3},    {83, 40, 3},    {84, 40, 3},    {85, 40, 3},    {86, 40, 3},    {87, 40, 3},    {88, 40, 3},
    {89, 40, 3},    {90, 40, 3},    {91, 40, 3},    {92, 40, 3},    {93, 40, 3},    {94, 40, 3},    {95, 40, 3},    {96, 40, 3},    {97, 40, 3},
    {98, 40, 3},    {99, 40, 3},    {100, 40, 3},   {101, 40, 3},   {102, 40, 3},   {103, 40, 3},   {104, 40, 3},   {105, 40, 3},   {106, 40, 3},
    {107, 40, 3},   {108, 40, 3},   {109, 40, 3},   {110, 40, 3},   {111, 40, 3},   {112, 40, 3},   {113, 40, 3},   {114, 40, 3},   {115, 40, 3},
    {116, 40, 3},   {117, 40, 3},   {118, 40, 3},   {119, 40, 3},   {120, 40, 3},   {121, 40, 3},   {122, 40, 3},   {123, 40, 3},   {124, 40, 3},
    {125, 40, 3},   {126, 40, 3},   {127, 40, 3},   {128, 40, 3},   {129, 40, 3},   {139, 40, 3},   {140, 40, 3},   {141, 40, 3},   {-1, 40, 3},
    {-2, 40, 3},    {-3, 40, 3},    {-4, 40, 3},    {-5, 40, 3},    {-6, 40, 3},    {-7, 40, 3},    {-8, 40, 3},    {-9, 40, 3},    {0, 41, 3},
    {1, 41, 3},     {2, 41, 3},     {3, 41, 3},     {8, 41, 3},     {9, 41, 3},     {11, 41, 3},    {12, 41, 3},    {13, 41, 3},    {14, 41, 3},
    {15, 41, 3},    {16, 41, 3},    {17, 41, 3},    {19, 41, 3},    {20, 41, 3},    {21, 41, 3},    {22, 41, 3},    {23, 41, 3},    {24, 41, 3},
    {25, 41, 3},    {26, 41, 3},    {27, 41, 3},    {28, 41, 3},    {29, 41, 3},    {30, 41, 3},    {31, 41, 3},    {32, 41, 3},    {33, 41, 3},
    {34, 41, 3},    {35, 41, 3},    {36, 41, 3},    {37, 41, 3},    {38, 41, 3},    {39, 41, 3},    {40, 41, 3},    {41, 41, 3},    {42, 41, 3},
    {43, 41, 3},    {44, 41, 3},    {45, 41, 3},    {46, 41, 3},    {47, 41, 3},    {48, 41, 3},    {49, 41, 3},    {52, 41, 3},    {53, 41, 3},
    {54, 41, 3},    {55, 41, 3},    {56, 41, 3},    {57, 41, 3},    {58, 41, 3},    {59, 41, 3},    {60, 41, 3},    {61, 41, 3},    {62, 41, 3},
    {63, 41, 3},    {64, 41, 3},    {65, 41, 3},    {66, 41, 3},    {67, 41, 3},    {68, 41, 3},    {69, 41, 3},    {70, 41, 3},    {71, 41, 3},
    {72, 41, 3},    {73, 41, 3},    {74, 41, 3},    {75, 41, 3},    {76, 41, 3},    {77, 41, 3},    {78, 41, 3},    {79, 41, 3},    {80, 41, 3},
    {81, 41, 3},    {82, 41, 3},    {83, 41, 3},    {84, 41, 3},    {85, 41, 3},    {86, 41, 3},    {87, 41, 3},    {88, 41, 3},    {89, 41, 3},
    {90, 41, 3},    {91, 41, 3},    {92, 41, 3},    {93, 41, 3},    {94, 41, 3},    {95, 41, 3},    {96, 41, 3},    {97, 41, 3},    {98, 41, 3},
    {99, 41, 3},    {100, 41, 3},   {101, 41, 3},   {102, 41, 3},   {103, 41, 3},   {104, 41, 3},   {105, 41, 3},   {106, 41, 3},   {107, 41, 3},
    {108, 41, 3},   {109, 41, 3},   {110, 41, 3},   {111, 41, 3},   {112, 41, 3},   {113, 41, 3},   {114, 41, 3},   {115, 41, 3},   {116, 41, 3},
    {117, 41, 3},   {118, 41, 3},   {119, 41, 3},   {120, 41, 3},   {121, 41, 3},   {122, 41, 3},   {123, 41, 3},   {124, 41, 3},   {125, 41, 3},
    {126, 41, 3},   {127, 41, 3},   {128, 41, 3},   {129, 41, 3},   {130, 41, 3},   {139, 41, 3},   {140, 41, 3},   {141, 41, 3},   {143, 41, 3},
    {-1, 41, 3},    {-2, 41, 3},    {-3, 41, 3},    {-4, 41, 3},    {-5, 41, 3},    {-6, 41, 3},    {-7, 41, 3},    {-8, 41, 3},    {-9, 41, 3},
    {0, 42, 3},     {1, 42, 3},     {2, 42, 3},     {3, 42, 3},     {6, 42, 3},     {8, 42, 3},     {9, 42, 3},     {10, 42, 3},    {11, 42, 3},
    {12, 42, 3},    {13, 42, 3},    {14, 42, 3},    {15, 42, 3},    {16, 42, 3},    {17, 42, 3},    {18, 42, 3},    {19, 42, 3},    {20, 42, 3},
    {21, 42, 3},    {22, 42, 3},    {23, 42, 3},    {24, 42, 3},    {25, 42, 3},    {26, 42, 3},    {27, 42, 3},    {28, 42, 3},    {33, 42, 3},
    {34, 42, 3},    {35, 42, 3},    {40, 42, 3},    {41, 42, 3},    {42, 42, 3},    {43, 42, 3},    {44, 42, 3},    {45, 42, 3},    {46, 42, 3},
    {47, 42, 3},    {48, 42, 3},    {51, 42, 3},    {52, 42, 3},    {53, 42, 3},    {54, 42, 3},    {55, 42, 3},    {56, 42, 3},    {57, 42, 3},
    {58, 42, 3},    {59, 42, 3},    {60, 42, 3},    {61, 42, 3},    {62, 42, 3},    {63, 42, 3},    {64, 42, 3},    {65, 42, 3},    {66, 42, 3},
    {67, 42, 3},    {68, 42, 3},    {69, 42, 3},    {70, 42, 3},    {71, 42, 3},    {72, 42, 3},    {73, 42, 3},    {74, 42, 3},    {75, 42, 3},
    {76, 42, 3},    {77, 42, 3},    {78, 42, 3},    {79, 42, 3},    {80, 42, 3},    {81, 42, 3},    {82, 42, 3},    {83, 42, 3},    {84, 42, 3},
    {85, 42, 3},    {86, 42, 3},    {87, 42, 3},    {88, 42, 3},    {89, 42, 3},    {90, 42, 3},    {91, 42, 3},    {92, 42, 3},    {93, 42, 3},
    {94, 42, 3},    {95, 42, 3},    {96, 42, 3},    {97, 42, 3},    {98, 42, 3},    {99, 42, 3},    {100, 42, 3},   {101, 42, 3},   {102, 42, 3},
    {103, 42, 3},   {104, 42, 3},   {105, 42, 3},   {106, 42, 3},   {107, 42, 3},   {108, 42, 3},   {109, 42, 3},   {110, 42, 3},   {111, 42, 3},
    {112, 42, 3},   {113, 42, 3},   {114, 42, 3},   {115, 42, 3},   {116, 42, 3},   {117, 42, 3},   {118, 42, 3},   {119, 42, 3},   {120, 42, 3},
    {121, 42, 3},   {122, 42, 3},   {123, 42, 3},   {124, 42, 3},   {125, 42, 3},   {126, 42, 3},   {127, 42, 3},   {128, 42, 3},   {129, 42, 3},
    {130, 42, 3},   {131, 42, 3},   {132, 42, 3},   {133, 42, 3},   {134, 42, 3},   {139, 42, 3},   {140, 42, 3},   {141, 42, 3},   {142, 42, 3},
    {143, 42, 3},   {144, 42, 3},   {145, 42, 3},   {-1, 42, 3},    {-2, 42, 3},    {-3, 42, 3},    {-4, 42, 3},    {-5, 42, 3},    {-6, 42, 3},
    {-7, 42, 3},    {-8, 42, 3},    {-9, 42, 3},    {-10, 42, 3},   {0, 43, 3},     {1, 43, 3},     {2, 43, 3},     {3, 43, 3},     {4, 43, 3},
    {5, 43, 3},     {6, 43, 3},     {7, 43, 3},     {8, 43, 3},     {9, 43, 3},     {10, 43, 3},    {11, 43, 3},    {12, 43, 3},    {13, 43, 3},
    {15, 43, 3},    {16, 43, 3},    {17, 43, 3},    {18, 43, 3},    {19, 43, 3},    {20, 43, 3},    {21, 43, 3},    {22, 43, 3},    {23, 43, 3},
    {24, 43, 3},    {25, 43, 3},    {26, 43, 3},    {27, 43, 3},    {28, 43, 3},    {39, 43, 3},    {40, 43, 3},    {41, 43, 3},    {42, 43, 3},
    {43, 43, 3},    {44, 43, 3},    {45, 43, 3},    {46, 43, 3},    {47, 43, 3},    {50, 43, 3},    {51, 43, 3},    {52, 43, 3},    {53, 43, 3},
    {54, 43, 3},    {55, 43, 3},    {56, 43, 3},    {57, 43, 3},    {58, 43, 3},    {59, 43, 3},    {60, 43, 3},    {61, 43, 3},    {62, 43, 3},
    {63, 43, 3},    {64, 43, 3},    {65, 43, 3},    {66, 43, 3},    {67, 43, 3},    {68, 43, 3},    {69, 43, 3},    {70, 43, 3},    {71, 43, 3},
    {72, 43, 3},    {73, 43, 3},    {74, 43, 3},    {75, 43, 3},    {76, 43, 3},    {77, 43, 3},    {78, 43, 3},    {79, 43, 3},    {80, 43, 3},
    {81, 43, 3},    {82, 43, 3},    {83, 43, 3},    {84, 43, 3},    {85, 43, 3},    {86, 43, 3},    {87, 43, 3},    {88, 43, 3},    {89, 43, 3},
    {90, 43, 3},    {91, 43, 3},    {92, 43, 3},    {93, 43, 3},    {94, 43, 3},    {95, 43, 3},    {96, 43, 3},    {97, 43, 3},    {98, 43, 3},
    {99, 43, 3},    {100, 43, 3},   {101, 43, 3},   {102, 43, 3},   {103, 43, 3},   {104, 43, 3},   {105, 43, 3},   {106, 43, 3},   {107, 43, 3},
    {108, 43, 3},   {109, 43, 3},   {110, 43, 3},   {111, 43, 3},   {112, 43, 3},   {113, 43, 3},   {114, 43, 3},   {115, 43, 3},   {116, 43, 3},
    {117, 43, 3},   {118, 43, 3},   {119, 43, 3},   {120, 43, 3},   {121, 43, 3},   {122, 43, 3},   {123, 43, 3},   {124, 43, 3},   {125, 43, 3},
    {126, 43, 3},   {127, 43, 3},   {128, 43, 3},   {129, 43, 3},   {130, 43, 3},   {131, 43, 3},   {132, 43, 3},   {133, 43, 3},   {134, 43, 3},
    {135, 43, 3},   {140, 43, 3},   {141, 43, 3},   {142, 43, 3},   {143, 43, 3},   {144, 43, 3},   {145, 43, 3},   {146, 43, 3},   {-1, 43, 3},
    {-2, 43, 3},    {-3, 43, 3},    {-4, 43, 3},    {-5, 43, 3},    {-6, 43, 3},    {-7, 43, 3},    {-8, 43, 3},    {-9, 43, 3},    {-10, 43, 3},
    {0, 44, 3},     {1, 44, 3},     {2, 44, 3},     {3, 44, 3},     {4, 44, 3},     {5, 44, 3},     {6, 44, 3},     {7, 44, 3},     {8, 44, 3},
    {9, 44, 3},     {10, 44, 3},    {11, 44, 3},    {12, 44, 3},    {13, 44, 3},    {14, 44, 3},    {15, 44, 3},    {16, 44, 3},    {17, 44, 3},
    {18, 44, 3},    {19, 44, 3},    {20, 44, 3},    {21, 44, 3},    {22, 44, 3},    {23, 44, 3},    {24, 44, 3},    {25, 44, 3},    {26, 44, 3},
    {27, 44, 3},    {28, 44, 3},    {29, 44, 3},    {33, 44, 3},    {34, 44, 3},    {35, 44, 3},    {37, 44, 3},    {38, 44, 3},    {39, 44, 3},
    {40, 44, 3},    {41, 44, 3},    {42, 44, 3},    {43, 44, 3},    {44, 44, 3},    {45, 44, 3},    {46, 44, 3},    {47, 44, 3},    {50, 44, 3},
    {51, 44, 3},    {52, 44, 3},    {53, 44, 3},    {54, 44, 3},    {55, 44, 3},    {56, 44, 3},    {57, 44, 3},    {58, 44, 3},    {59, 44, 3},
    {60, 44, 3},    {61, 44, 3},    {62, 44, 3},    {63, 44, 3},    {64, 44, 3},    {65, 44, 3},    {66, 44, 3},    {67, 44, 3},    {68, 44, 3},
    {69, 44, 3},    {70, 44, 3},    {71, 44, 3},    {72, 44, 3},    {73, 44, 3},    {74, 44, 3},    {75, 44, 3},    {76, 44, 3},    {77, 44, 3},
    {78, 44, 3},    {79, 44, 3},    {80, 44, 3},    {81, 44, 3},    {82, 44, 3},    {83, 44, 3},    {84, 44, 3},    {85, 44, 3},    {86, 44, 3},
    {87, 44, 3},    {88, 44, 3},    {89, 44, 3},    {90, 44, 3},    {91, 44, 3},    {92, 44, 3},    {93, 44, 3},    {94, 44, 3},    {95, 44, 3},
    {96, 44, 3},    {97, 44, 3},    {98, 44, 3},    {99, 44, 3},    {100, 44, 3},   {101, 44, 3},   {102, 44, 3},   {103, 44, 3},   {104, 44, 3},
    {105, 44, 3},   {106, 44, 3},   {107, 44, 3},   {108, 44, 3},   {109, 44, 3},   {110, 44, 3},   {111, 44, 3},   {112, 44, 3},   {113, 44, 3},
    {114, 44, 3},   {115, 44, 3},   {116, 44, 3},   {117, 44, 3},   {118, 44, 3},   {119, 44, 3},   {120, 44, 3},   {121, 44, 3},   {122, 44, 3},
    {123, 44, 3},   {124, 44, 3},   {125, 44, 3},   {126, 44, 3},   {127, 44, 3},   {128, 44, 3},   {129, 44, 3},   {130, 44, 3},   {131, 44, 3},
    {132, 44, 3},   {133, 44, 3},   {134, 44, 3},   {135, 44, 3},   {136, 44, 3},   {141, 44, 3},   {142, 44, 3},   {143, 44, 3},   {144, 44, 3},
    {145, 44, 3},   {146, 44, 3},   {147, 44, 3},   {-1, 44, 3},    {-2, 44, 3},    {0, 45, 3},     {1, 45, 3},     {2, 45, 3},     {3, 45, 3},
    {4, 45, 3},     {5, 45, 3},     {6, 45, 3},     {7, 45, 3},     {8, 45, 3},     {9, 45, 3},     {10, 45, 3},    {11, 45, 3},    {12, 45, 3},
    {13, 45, 3},    {14, 45, 3},    {15, 45, 3},    {16, 45, 3},    {17, 45, 3},    {18, 45, 3},    {19, 45, 3},    {20, 45, 3},    {21, 45, 3},
    {22, 45, 3},    {23, 45, 3},    {24, 45, 3},    {25, 45, 3},    {26, 45, 3},    {27, 45, 3},    {28, 45, 3},    {29, 45, 3},    {30, 45, 3},
    {32, 45, 3},    {33, 45, 3},    {34, 45, 3},    {35, 45, 3},    {36, 45, 3},    {37, 45, 3},    {38, 45, 3},    {39, 45, 3},    {40, 45, 3},
    {41, 45, 3},    {42, 45, 3},    {43, 45, 3},    {44, 45, 3},    {45, 45, 3},    {46, 45, 3},    {47, 45, 3},    {48, 45, 3},    {49, 45, 3},
    {50, 45, 3},    {51, 45, 3},    {52, 45, 3},    {53, 45, 3},    {54, 45, 3},    {55, 45, 3},    {56, 45, 3},    {57, 45, 3},    {58, 45, 3},
    {59, 45, 3},    {60, 45, 3},    {61, 45, 3},    {62, 45, 3},    {63, 45, 3},    {64, 45, 3},    {65, 45, 3},    {66, 45, 3},    {67, 45, 3},
    {68, 45, 3},    {69, 45, 3},    {70, 45, 3},    {71, 45, 3},    {72, 45, 3},    {73, 45, 3},    {74, 45, 3},    {75, 45, 3},    {76, 45, 3},
    {77, 45, 3},    {78, 45, 3},    {79, 45, 3},    {80, 45, 3},    {81, 45, 3},    {82, 45, 3},    {83, 45, 3},    {84, 45, 3},    {85, 45, 3},
    {86, 45, 3},    {87, 45, 3},    {88, 45, 3},    {89, 45, 3},    {90, 45, 3},    {91, 45, 3},    {92, 45, 3},    {93, 45, 3},    {94, 45, 3},
    {95, 45, 3},    {96, 45, 3},    {97, 45, 3},    {98, 45, 3},    {99, 45, 3},    {100, 45, 3},   {101, 45, 3},   {102, 45, 3},   {103, 45, 3},
    {104, 45, 3},   {105, 45, 3},   {106, 45, 3},   {107, 45, 3},   {108, 45, 3},   {109, 45, 3},   {110, 45, 3},   {111, 45, 3},   {112, 45, 3},
    {113, 45, 3},   {114, 45, 3},   {115, 45, 3},   {116, 45, 3},   {117, 45, 3},   {118, 45, 3},   {119, 45, 3},   {120, 45, 3},   {121, 45, 3},
    {122, 45, 3},   {123, 45, 3},   {124, 45, 3},   {125, 45, 3},   {126, 45, 3},   {127, 45, 3},   {128, 45, 3},   {129, 45, 3},   {130, 45, 3},
    {131, 45, 3},   {132, 45, 3},   {133, 45, 3},   {134, 45, 3},   {135, 45, 3},   {136, 45, 3},   {137, 45, 3},   {140, 45, 3},   {141, 45, 3},
    {142, 45, 3},   {147, 45, 3},   {148, 45, 3},   {149, 45, 3},   {150, 45, 3},   {-1, 45, 3},    {-2, 45, 3},    {0, 46, 3},     {1, 46, 3},
    {2, 46, 3},     {3, 46, 3},     {4, 46, 3},     {5, 46, 3},     {6, 46, 3},     {7, 46, 3},     {8, 46, 3},     {9, 46, 3},     {10, 46, 3},
    {11, 46, 3},    {12, 46, 3},    {13, 46, 3},    {14, 46, 3},    {15, 46, 3},    {16, 46, 3},    {17, 46, 3},    {18, 46, 3},    {19, 46, 3},
    {20, 46, 3},    {21, 46, 3},    {22, 46, 3},    {23, 46, 3},    {24, 46, 3},    {25, 46, 3},    {26, 46, 3},    {27, 46, 3},    {28, 46, 3},
    {29, 46, 3},    {30, 46, 3},    {31, 46, 3},    {32, 46, 3},    {33, 46, 3},    {34, 46, 3},    {35, 46, 3},    {36, 46, 3},    {37, 46, 3},
    {38, 46, 3},    {39, 46, 3},    {40, 46, 3},    {41, 46, 3},    {42, 46, 3},    {43, 46, 3},    {44, 46, 3},    {45, 46, 3},    {46, 46, 3},
    {47, 46, 3},    {48, 46, 3},    {49, 46, 3},    {50, 46, 3},    {51, 46, 3},    {52, 46, 3},    {53, 46, 3},    {54, 46, 3},    {55, 46, 3},
    {56, 46, 3},    {57, 46, 3},    {58, 46, 3},    {59, 46, 3},    {60, 46, 3},    {61, 46, 3},    {62, 46, 3},    {63, 46, 3},    {64, 46, 3},
    {65, 46, 3},    {66, 46, 3},    {67, 46, 3},    {68, 46, 3},    {69, 46, 3},    {70, 46, 3},    {71, 46, 3},    {72, 46, 3},    {73, 46, 3},
    {74, 46, 3},    {75, 46, 3},    {76, 46, 3},    {77, 46, 3},    {78, 46, 3},    {79, 46, 3},    {80, 46, 3},    {81, 46, 3},    {82, 46, 3},
    {83, 46, 3},    {84, 46, 3},    {85, 46, 3},    {86, 46, 3},    {87, 46, 3},    {88, 46, 3},    {89, 46, 3},    {90, 46, 3},    {91, 46, 3},
    {92, 46, 3},    {93, 46, 3},    {94, 46, 3},    {95, 46, 3},    {96, 46, 3},    {97, 46, 3},    {98, 46, 3},    {99, 46, 3},    {100, 46, 3},
    {101, 46, 3},   {102, 46, 3},   {103, 46, 3},   {104, 46, 3},   {105, 46, 3},   {106, 46, 3},   {107, 46, 3},   {108, 46, 3},   {109, 46, 3},
    {110, 46, 3},   {111, 46, 3},   {112, 46, 3},   {113, 46, 3},   {114, 46, 3},   {115, 46, 3},   {116, 46, 3},   {117, 46, 3},   {118, 46, 3},
    {119, 46, 3},   {120, 46, 3},   {121, 46, 3},   {122, 46, 3},   {123, 46, 3},   {124, 46, 3},   {125, 46, 3},   {126, 46, 3},   {127, 46, 3},
    {128, 46, 3},   {129, 46, 3},   {130, 46, 3},   {131, 46, 3},   {132, 46, 3},   {133, 46, 3},   {134, 46, 3},   {135, 46, 3},   {136, 46, 3},
    {137, 46, 3},   {138, 46, 3},   {141, 46, 3},   {142, 46, 3},   {143, 46, 3},   {149, 46, 3},   {150, 46, 3},   {151, 46, 3},   {152, 46, 3},
    {-1, 46, 3},    {-2, 46, 3},    {-3, 46, 3},    {0, 47, 3},     {1, 47, 3},     {2, 47, 3},     {3, 47, 3},     {4, 47, 3},     {5, 47, 3},
    {6, 47, 3},     {7, 47, 3},     {8, 47, 3},     {9, 47, 3},     {10, 47, 3},    {11, 47, 3},    {12, 47, 3},    {13, 47, 3},    {14, 47, 3},
    {15, 47, 3},    {16, 47, 3},    {17, 47, 3},    {18, 47, 3},    {19, 47, 3},    {20, 47, 3},    {21, 47, 3},    {22, 47, 3},    {23, 47, 3},
    {24, 47, 3},    {25, 47, 3},    {26, 47, 3},    {27, 47, 3},    {28, 47, 3},    {29, 47, 3},    {30, 47, 3},    {31, 47, 3},    {32, 47, 3},
    {33, 47, 3},    {34, 47, 3},    {35, 47, 3},    {36, 47, 3},    {37, 47, 3},    {38, 47, 3},    {39, 47, 3},    {40, 47, 3},    {41, 47, 3},
    {42, 47, 3},    {43, 47, 3},    {44, 47, 3},    {45, 47, 3},    {46, 47, 3},    {47, 47, 3},    {48, 47, 3},    {49, 47, 3},    {50, 47, 3},
    {51, 47, 3},    {52, 47, 3},    {53, 47, 3},    {54, 47, 3},    {55, 47, 3},    {56, 47, 3},    {57, 47, 3},    {58, 47, 3},    {59, 47, 3},
    {60, 47, 3},    {61, 47, 3},    {62, 47, 3},    {63, 47, 3},    {64, 47, 3},    {65, 47, 3},    {66, 47, 3},    {67, 47, 3},    {68, 47, 3},
    {69, 47, 3},    {70, 47, 3},    {71, 47, 3},    {72, 47, 3},    {73, 47, 3},    {74, 47, 3},    {75, 47, 3},    {76, 47, 3},    {77, 47, 3},
    {78, 47, 3},    {79, 47, 3},    {80, 47, 3},    {81, 47, 3},    {82, 47, 3},    {83, 47, 3},    {84, 47, 3},    {85, 47, 3},    {86, 47, 3},
    {87, 47, 3},    {88, 47, 3},    {89, 47, 3},    {90, 47, 3},    {91, 47, 3},    {92, 47, 3},    {93, 47, 3},    {94, 47, 3},    {95, 47, 3},
    {96, 47, 3},    {97, 47, 3},    {98, 47, 3},    {99, 47, 3},    {100, 47, 3},   {101, 47, 3},   {102, 47, 3},   {103, 47, 3},   {104, 47, 3},
    {105, 47, 3},   {106, 47, 3},   {107, 47, 3},   {108, 47, 3},   {109, 47, 3},   {110, 47, 3},   {111, 47, 3},   {112, 47, 3},   {113, 47, 3},
    {114, 47, 3},   {115, 47, 3},   {116, 47, 3},   {117, 47, 3},   {118, 47, 3},   {119, 47, 3},   {120, 47, 3},   {121, 47, 3},   {122, 47, 3},
    {123, 47, 3},   {124, 47, 3},   {125, 47, 3},   {126, 47, 3},   {127, 47, 3},   {128, 47, 3},   {129, 47, 3},   {130, 47, 3},   {131, 47, 3},
    {132, 47, 3},   {133, 47, 3},   {134, 47, 3},   {135, 47, 3},   {136, 47, 3},   {137, 47, 3},   {138, 47, 3},   {139, 47, 3},   {141, 47, 3},
    {142, 47, 3},   {143, 47, 3},   {152, 47, 3},   {153, 47, 3},   {-1, 47, 3},    {-2, 47, 3},    {-3, 47, 3},    {-4, 47, 3},    {-5, 47, 3},
    {0, 48, 3},     {1, 48, 3},     {2, 48, 3},     {3, 48, 3},     {4, 48, 3},     {5, 48, 3},     {6, 48, 3},     {7, 48, 3},     {8, 48, 3},
    {9, 48, 3},     {10, 48, 3},    {11, 48, 3},    {12, 48, 3},    {13, 48, 3},    {14, 48, 3},    {15, 48, 3},    {16, 48, 3},    {17, 48, 3},
    {18, 48, 3},    {19, 48, 3},    {20, 48, 3},    {21, 48, 3},    {22, 48, 3},    {23, 48, 3},    {24, 48, 3},    {25, 48, 3},    {26, 48, 3},
    {27, 48, 3},    {28, 48, 3},    {29, 48, 3},    {30, 48, 3},    {31, 48, 3},    {32, 48, 3},    {33, 48, 3},    {34, 48, 3},    {35, 48, 3},
    {36, 48, 3},    {37, 48, 3},    {38, 48, 3},    {39, 48, 3},    {40, 48, 3},    {41, 48, 3},    {42, 48, 3},    {43, 48, 3},    {44, 48, 3},
    {45, 48, 3},    {46, 48, 3},    {47, 48, 3},    {48, 48, 3},    {49, 48, 3},    {50, 48, 3},    {51, 48, 3},    {52, 48, 3},    {53, 48, 3},
    {54, 48, 3},    {55, 48, 3},    {56, 48, 3},    {57, 48, 3},    {58, 48, 3},    {59, 48, 3},    {60, 48, 3},    {61, 48, 3},    {62, 48, 3},
    {63, 48, 3},    {64, 48, 3},    {65, 48, 3},    {66, 48, 3},    {67, 48, 3},    {68, 48, 3},    {69, 48, 3},    {70, 48, 3},    {71, 48, 3},
    {72, 48, 3},    {73, 48, 3},    {74, 48, 3},    {75, 48, 3},    {76, 48, 3},    {77, 48, 3},    {78, 48, 3},    {79, 48, 3},    {80, 48, 3},
    {81, 48, 3},    {82, 48, 3},    {83, 48, 3},    {84, 48, 3},    {85, 48, 3},    {86, 48, 3},    {87, 48, 3},    {88, 48, 3},    {89, 48, 3},
    {90, 48, 3},    {91, 48, 3},    {92, 48, 3},    {93, 48, 3},    {94, 48, 3},    {95, 48, 3},    {96, 48, 3},    {97, 48, 3},    {98, 48, 3},
    {99, 48, 3},    {100, 48, 3},   {101, 48, 3},   {102, 48, 3},   {103, 48, 3},   {104, 48, 3},   {105, 48, 3},   {106, 48, 3},   {107, 48, 3},
    {108, 48, 3},   {109, 48, 3},   {110, 48, 3},   {111, 48, 3},   {112, 48, 3},   {113, 48, 3},   {114, 48, 3},   {115, 48, 3},   {116, 48, 3},
    {117, 48, 3},   {118, 48, 3},   {119, 48, 3},   {120, 48, 3},   {121, 48, 3},   {122, 48, 3},   {123, 48, 3},   {124, 48, 3},   {125, 48, 3},
    {126, 48, 3},   {127, 48, 3},   {128, 48, 3},   {129, 48, 3},   {130, 48, 3},   {131, 48, 3},   {132, 48, 3},   {133, 48, 3},   {134, 48, 3},
    {135, 48, 3},   {136, 48, 3},   {137, 48, 3},   {138, 48, 3},   {139, 48, 3},   {140, 48, 3},   {141, 48, 3},   {142, 48, 3},   {144, 48, 3},
    {153, 48, 3},   {154, 48, 3},   {-1, 48, 3},    {-2, 48, 3},    {-3, 48, 3},    {-4, 48, 3},    {-5, 48, 3},    {-6, 48, 3},    {0, 49, 3},
    {1, 49, 3},     {2, 49, 3},     {3, 49, 3},     {4, 49, 3},     {5, 49, 3},     {6, 49, 3},     {7, 49, 3},     {8, 49, 3},     {9, 49, 3},
    {10, 49, 3},    {11, 49, 3},    {12, 49, 3},    {13, 49, 3},    {14, 49, 3},    {15, 49, 3},    {16, 49, 3},    {17, 49, 3},    {18, 49, 3},
    {19, 49, 3},    {20, 49, 3},    {21, 49, 3},    {22, 49, 3},    {23, 49, 3},    {24, 49, 3},    {25, 49, 3},    {26, 49, 3},    {27, 49, 3},
    {28, 49, 3},    {29, 49, 3},    {30, 49, 3},    {31, 49, 3},    {32, 49, 3},    {33, 49, 3},    {34, 49, 3},    {35, 49, 3},    {36, 49, 3},
    {37, 49, 3},    {38, 49, 3},    {39, 49, 3},    {40, 49, 3},    {41, 49, 3},    {42, 49, 3},    {43, 49, 3},    {44, 49, 3},    {45, 49, 3},
    {46, 49, 3},    {47, 49, 3},    {48, 49, 3},    {49, 49, 3},    {50, 49, 3},    {51, 49, 3},    {52, 49, 3},    {53, 49, 3},    {54, 49, 3},
    {55, 49, 3},    {56, 49, 3},    {57, 49, 3},    {58, 49, 3},    {59, 49, 3},    {60, 49, 3},    {61, 49, 3},    {62, 49, 3},    {63, 49, 3},
    {64, 49, 3},    {65, 49, 3},    {66, 49, 3},    {67, 49, 3},    {68, 49, 3},    {69, 49, 3},    {70, 49, 3},    {71, 49, 3},    {72, 49, 3},
    {73, 49, 3},    {74, 49, 3},    {75, 49, 3},    {76, 49, 3},    {77, 49, 3},    {78, 49, 3},    {79, 49, 3},    {80, 49, 3},    {81, 49, 3},
    {82, 49, 3},    {83, 49, 3},    {84, 49, 3},    {85, 49, 3},    {86, 49, 3},    {87, 49, 3},    {88, 49, 3},    {89, 49, 3},    {90, 49, 3},
    {91, 49, 3},    {92, 49, 3},    {93, 49, 3},    {94, 49, 3},    {95, 49, 3},    {96, 49, 3},    {97, 49, 3},    {98, 49, 3},    {99, 49, 3},
    {100, 49, 3},   {101, 49, 3},   {102, 49, 3},   {103, 49, 3},   {104, 49, 3},   {105, 49, 3},   {106, 49, 3},   {107, 49, 3},   {108, 49, 3},
    {109, 49, 3},   {110, 49, 3},   {111, 49, 3},   {112, 49, 3},   {113, 49, 3},   {114, 49, 3},   {115, 49, 3},   {116, 49, 3},   {117, 49, 3},
    {118, 49, 3},   {119, 49, 3},   {120, 49, 3},   {121, 49, 3},   {122, 49, 3},   {123, 49, 3},   {124, 49, 3},   {125, 49, 3},   {126, 49, 3},
    {127, 49, 3},   {128, 49, 3},   {129, 49, 3},   {130, 49, 3},   {131, 49, 3},   {132, 49, 3},   {133, 49, 3},   {134, 49, 3},   {135, 49, 3},
    {136, 49, 3},   {137, 49, 3},   {138, 49, 3},   {139, 49, 3},   {140, 49, 3},   {142, 49, 3},   {143, 49, 3},   {144, 49, 3},   {154, 49, 3},
    {155, 49, 3},   {-1, 49, 3},    {-2, 49, 3},    {-3, 49, 3},    {-6, 49, 3},    {-7, 49, 3},    {0, 50, 3},     {1, 50, 3},     {2, 50, 3},
    {3, 50, 3},     {4, 50, 3},     {5, 50, 3},     {6, 50, 3},     {7, 50, 3},     {8, 50, 3},     {9, 50, 3},     {10, 50, 3},    {11, 50, 3},
    {12, 50, 3},    {13, 50, 3},    {14, 50, 3},    {15, 50, 3},    {16, 50, 3},    {17, 50, 3},    {18, 50, 3},    {19, 50, 3},    {20, 50, 3},
    {21, 50, 3},    {22, 50, 3},    {23, 50, 3},    {24, 50, 3},    {25, 50, 3},    {26, 50, 3},    {27, 50, 3},    {28, 50, 3},    {29, 50, 3},
    {30, 50, 3},    {31, 50, 3},    {32, 50, 3},    {33, 50, 3},    {34, 50, 3},    {35, 50, 3},    {36, 50, 3},    {37, 50, 3},    {38, 50, 3},
    {39, 50, 3},    {40, 50, 3},    {41, 50, 3},    {42, 50, 3},    {43, 50, 3},    {44, 50, 3},    {45, 50, 3},    {46, 50, 3},    {47, 50, 3},
    {48, 50, 3},    {49, 50, 3},    {50, 50, 3},    {51, 50, 3},    {52, 50, 3},    {53, 50, 3},    {54, 50, 3},    {55, 50, 3},    {56, 50, 3},
    {57, 50, 3},    {58, 50, 3},    {59, 50, 3},    {60, 50, 3},    {61, 50, 3},    {62, 50, 3},    {63, 50, 3},    {64, 50, 3},    {65, 50, 3},
    {66, 50, 3},    {67, 50, 3},    {68, 50, 3},    {69, 50, 3},    {70, 50, 3},    {71, 50, 3},    {72, 50, 3},    {73, 50, 3},    {74, 50, 3},
    {75, 50, 3},    {76, 50, 3},    {77, 50, 3},    {78, 50, 3},    {79, 50, 3},    {80, 50, 3},    {81, 50, 3},    {82, 50, 3},    {83, 50, 3},
    {84, 50, 3},    {85, 50, 3},    {86, 50, 3},    {87, 50, 3},    {88, 50, 3},    {89, 50, 3},    {90, 50, 3},    {91, 50, 3},    {92, 50, 3},
    {93, 50, 3},    {94, 50, 3},    {95, 50, 3},    {96, 50, 3},    {97, 50, 3},    {98, 50, 3},    {99, 50, 3},    {100, 50, 3},   {101, 50, 3},
    {102, 50, 3},   {103, 50, 3},   {104, 50, 3},   {105, 50, 3},   {106, 50, 3},   {107, 50, 3},   {108, 50, 3},   {109, 50, 3},   {110, 50, 3},
    {111, 50, 3},   {112, 50, 3},   {113, 50, 3},   {114, 50, 3},   {115, 50, 3},   {116, 50, 3},   {117, 50, 3},   {118, 50, 3},   {119, 50, 3},
    {120, 50, 3},   {121, 50, 3},   {122, 50, 3},   {123, 50, 3},   {124, 50, 3},   {125, 50, 3},   {126, 50, 3},   {127, 50, 3},   {128, 50, 3},
    {129, 50, 3},   {130, 50, 3},   {131, 50, 3},   {132, 50, 3},   {133, 50, 3},   {134, 50, 3},   {135, 50, 3},   {136, 50, 3},   {137, 50, 3},
    {138, 50, 3},   {139, 50, 3},   {140, 50, 3},   {142, 50, 3},   {143, 50, 3},   {154, 50, 3},   {155, 50, 3},   {156, 50, 3},   {-1, 50, 3},
    {-2, 50, 3},    {-3, 50, 3},    {-4, 50, 3},    {-5, 50, 3},    {-6, 50, 3},    {0, 51, 3},     {1, 51, 3},     {2, 51, 3},     {3, 51, 3},
    {4, 51, 3},     {5, 51, 3},     {6, 51, 3},     {7, 51, 3},     {8, 51, 3},     {9, 51, 3},     {10, 51, 3},    {11, 51, 3},    {12, 51, 3},
    {13, 51, 3},    {14, 51, 3},    {15, 51, 3},    {16, 51, 3},    {17, 51, 3},    {18, 51, 3},    {19, 51, 3},    {20, 51, 3},    {21, 51, 3},
    {22, 51, 3},    {23, 51, 3},    {24, 51, 3},    {25, 51, 3},    {26, 51, 3},    {27, 51, 3},    {28, 51, 3},    {29, 51, 3},    {30, 51, 3},
    {31, 51, 3},    {32, 51, 3},    {33, 51, 3},    {34, 51, 3},    {35, 51, 3},    {36, 51, 3},    {37, 51, 3},    {38, 51, 3},    {39, 51, 3},
    {40, 51, 3},    {41, 51, 3},    {42, 51, 3},    {43, 51, 3},    {44, 51, 3},    {45, 51, 3},    {46, 51, 3},    {47, 51, 3},    {48, 51, 3},
    {49, 51, 3},    {50, 51, 3},    {51, 51, 3},    {52, 51, 3},    {53, 51, 3},    {54, 51, 3},    {55, 51, 3},    {56, 51, 3},    {57, 51, 3},
    {58, 51, 3},    {59, 51, 3},    {60, 51, 3},    {61, 51, 3},    {62, 51, 3},    {63, 51, 3},    {64, 51, 3},    {65, 51, 3},    {66, 51, 3},
    {67, 51, 3},    {68, 51, 3},    {69, 51, 3},    {70, 51, 3},    {71, 51, 3},    {72, 51, 3},    {73, 51, 3},    {74, 51, 3},    {75, 51, 3},
    {76, 51, 3},    {77, 51, 3},    {78, 51, 3},    {79, 51, 3},    {80, 51, 3},    {81, 51, 3},    {82, 51, 3},    {83, 51, 3},    {84, 51, 3},
    {85, 51, 3},    {86, 51, 3},    {87, 51, 3},    {88, 51, 3},    {89, 51, 3},    {90, 51, 3},    {91, 51, 3},    {92, 51, 3},    {93, 51, 3},
    {94, 51, 3},    {95, 51, 3},    {96, 51, 3},    {97, 51, 3},    {98, 51, 3},    {99, 51, 3},    {100, 51, 3},   {101, 51, 3},   {102, 51, 3},
    {103, 51, 3},   {104, 51, 3},   {105, 51, 3},   {106, 51, 3},   {107, 51, 3},   {108, 51, 3},   {109, 51, 3},   {110, 51, 3},   {111, 51, 3},
    {112, 51, 3},   {113, 51, 3},   {114, 51, 3},   {115, 51, 3},   {116, 51, 3},   {117, 51, 3},   {118, 51, 3},   {119, 51, 3},   {120, 51, 3},
    {121, 51, 3},   {122, 51, 3},   {123, 51, 3},   {124, 51, 3},   {125, 51, 3},   {126, 51, 3},   {127, 51, 3},   {128, 51, 3},   {129, 51, 3},
    {130, 51, 3},   {131, 51, 3},   {132, 51, 3},   {133, 51, 3},   {134, 51, 3},   {135, 51, 3},   {136, 51, 3},   {137, 51, 3},   {138, 51, 3},
    {139, 51, 3},   {140, 51, 3},   {141, 51, 3},   {142, 51, 3},   {143, 51, 3},   {156, 51, 3},   {157, 51, 3},   {158, 51, 3},   {-1, 51, 3},
    {-2, 51, 3},    {-3, 51, 3},    {-4, 51, 3},    {-5, 51, 3},    {-6, 51, 3},    {-8, 51, 3},    {-9, 51, 3},    {-10, 51, 3},   {-11, 51, 3},
    {0, 52, 3},     {1, 52, 3},     {4, 52, 3},     {5, 52, 3},     {6, 52, 3},     {7, 52, 3},     {8, 52, 3},     {9, 52, 3},     {10, 52, 3},
    {11, 52, 3},    {12, 52, 3},    {13, 52, 3},    {14, 52, 3},    {15, 52, 3},    {16, 52, 3},    {17, 52, 3},    {18, 52, 3},    {19, 52, 3},
    {20, 52, 3},    {21, 52, 3},    {22, 52, 3},    {23, 52, 3},    {24, 52, 3},    {25, 52, 3},    {26, 52, 3},    {27, 52, 3},    {28, 52, 3},
    {29, 52, 3},    {30, 52, 3},    {31, 52, 3},    {32, 52, 3},    {33, 52, 3},    {34, 52, 3},    {35, 52, 3},    {36, 52, 3},    {37, 52, 3},
    {38, 52, 3},    {39, 52, 3},    {40, 52, 3},    {41, 52, 3},    {42, 52, 3},    {43, 52, 3},    {44, 52, 3},    {45, 52, 3},    {46, 52, 3},
    {47, 52, 3},    {48, 52, 3},    {49, 52, 3},    {50, 52, 3},    {51, 52, 3},    {52, 52, 3},    {53, 52, 3},    {54, 52, 3},    {55, 52, 3},
    {56, 52, 3},    {57, 52, 3},    {58, 52, 3},    {59, 52, 3},    {60, 52, 3},    {61, 52, 3},    {62, 52, 3},    {63, 52, 3},    {64, 52, 3},
    {65, 52, 3},    {66, 52, 3},    {67, 52, 3},    {68, 52, 3},    {69, 52, 3},    {70, 52, 3},    {71, 52, 3},    {72, 52, 3},    {73, 52, 3},
    {74, 52, 3},    {75, 52, 3},    {76, 52, 3},    {77, 52, 3},    {78, 52, 3},    {79, 52, 3},    {80, 52, 3},    {81, 52, 3},    {82, 52, 3},
    {83, 52, 3},    {84, 52, 3},    {85, 52, 3},    {86, 52, 3},    {87, 52, 3},    {88, 52, 3},    {89, 52, 3},    {90, 52, 3},    {91, 52, 3},
    {92, 52, 3},    {93, 52, 3},    {94, 52, 3},    {95, 52, 3},    {96, 52, 3},    {97, 52, 3},    {98, 52, 3},    {99, 52, 3},    {100, 52, 3},
    {101, 52, 3},   {102, 52, 3},   {103, 52, 3},   {104, 52, 3},   {105, 52, 3},   {106, 52, 3},   {107, 52, 3},   {108, 52, 3},   {109, 52, 3},
    {110, 52, 3},   {111, 52, 3},   {112, 52, 3},   {113, 52, 3},   {114, 52, 3},   {115, 52, 3},   {116, 52, 3},   {117, 52, 3},   {118, 52, 3},
    {119, 52, 3},   {120, 52, 3},   {121, 52, 3},   {122, 52, 3},   {123, 52, 3},   {124, 52, 3},   {125, 52, 3},   {126, 52, 3},   {127, 52, 3},
    {128, 52, 3},   {129, 52, 3},   {130, 52, 3},   {131, 52, 3},   {132, 52, 3},   {133, 52, 3},   {134, 52, 3},   {135, 52, 3},   {136, 52, 3},
    {137, 52, 3},   {138, 52, 3},   {139, 52, 3},   {140, 52, 3},   {141, 52, 3},   {142, 52, 3},   {143, 52, 3},   {156, 52, 3},   {157, 52, 3},
    {158, 52, 3},   {-1, 52, 3},    {-2, 52, 3},    {-3, 52, 3},    {-4, 52, 3},    {-5, 52, 3},    {-6, 52, 3},    {-7, 52, 3},    {-8, 52, 3},
    {-9, 52, 3},    {-10, 52, 3},   {-11, 52, 3},   {0, 53, 3},     {4, 53, 3},     {5, 53, 3},     {6, 53, 3},     {7, 53, 3},     {8, 53, 3},
    {9, 53, 3},     {10, 53, 3},    {11, 53, 3},    {12, 53, 3},    {13, 53, 3},    {14, 53, 3},    {15, 53, 3},    {16, 53, 3},    {17, 53, 3},
    {18, 53, 3},    {19, 53, 3},    {20, 53, 3},    {21, 53, 3},    {22, 53, 3},    {23, 53, 3},    {24, 53, 3},    {25, 53, 3},    {26, 53, 3},
    {27, 53, 3},    {28, 53, 3},    {29, 53, 3},    {30, 53, 3},    {31, 53, 3},    {32, 53, 3},    {33, 53, 3},    {34, 53, 3},    {35, 53, 3},
    {36, 53, 3},    {37, 53, 3},    {38, 53, 3},    {39, 53, 3},    {40, 53, 3},    {41, 53, 3},    {42, 53, 3},    {43, 53, 3},    {44, 53, 3},
    {45, 53, 3},    {46, 53, 3},    {47, 53, 3},    {48, 53, 3},    {49, 53, 3},    {50, 53, 3},    {51, 53, 3},    {52, 53, 3},    {53, 53, 3},
    {54, 53, 3},    {55, 53, 3},    {56, 53, 3},    {57, 53, 3},    {58, 53, 3},    {59, 53, 3},    {60, 53, 3},    {61, 53, 3},    {62, 53, 3},
    {63, 53, 3},    {64, 53, 3},    {65, 53, 3},    {66, 53, 3},    {67, 53, 3},    {68, 53, 3},    {69, 53, 3},    {70, 53, 3},    {71, 53, 3},
    {72, 53, 3},    {73, 53, 3},    {74, 53, 3},    {75, 53, 3},    {76, 53, 3},    {77, 53, 3},    {78, 53, 3},    {79, 53, 3},    {80, 53, 3},
    {81, 53, 3},    {82, 53, 3},    {83, 53, 3},    {84, 53, 3},    {85, 53, 3},    {86, 53, 3},    {87, 53, 3},    {88, 53, 3},    {89, 53, 3},
    {90, 53, 3},    {91, 53, 3},    {92, 53, 3},    {93, 53, 3},    {94, 53, 3},    {95, 53, 3},    {96, 53, 3},    {97, 53, 3},    {98, 53, 3},
    {99, 53, 3},    {100, 53, 3},   {101, 53, 3},   {102, 53, 3},   {103, 53, 3},   {104, 53, 3},   {105, 53, 3},   {106, 53, 3},   {107, 53, 3},
    {108, 53, 3},   {109, 53, 3},   {110, 53, 3},   {111, 53, 3},   {112, 53, 3},   {113, 53, 3},   {114, 53, 3},   {115, 53, 3},   {116, 53, 3},
    {117, 53, 3},   {118, 53, 3},   {119, 53, 3},   {120, 53, 3},   {121, 53, 3},   {122, 53, 3},   {123, 53, 3},   {124, 53, 3},   {125, 53, 3},
    {126, 53, 3},   {127, 53, 3},   {128, 53, 3},   {129, 53, 3},   {130, 53, 3},   {131, 53, 3},   {132, 53, 3},   {133, 53, 3},   {134, 53, 3},
    {135, 53, 3},   {136, 53, 3},   {137, 53, 3},   {138, 53, 3},   {139, 53, 3},   {140, 53, 3},   {141, 53, 3},   {142, 53, 3},   {143, 53, 3},
    {155, 53, 3},   {156, 53, 3},   {157, 53, 3},   {158, 53, 3},   {159, 53, 3},   {160, 53, 3},   {-1, 53, 3},    {-2, 53, 3},    {-3, 53, 3},
    {-4, 53, 3},    {-5, 53, 3},    {-6, 53, 3},    {-7, 53, 3},    {-8, 53, 3},    {-9, 53, 3},    {-10, 53, 3},   {-11, 53, 3},   {7, 54, 3},
    {8, 54, 3},     {9, 54, 3},     {10, 54, 3},    {11, 54, 3},    {12, 54, 3},    {13, 54, 3},    {14, 54, 3},    {15, 54, 3},    {16, 54, 3},
    {17, 54, 3},    {18, 54, 3},    {19, 54, 3},    {20, 54, 3},    {21, 54, 3},    {22, 54, 3},    {23, 54, 3},    {24, 54, 3},    {25, 54, 3},
    {26, 54, 3},    {27, 54, 3},    {28, 54, 3},    {29, 54, 3},    {30, 54, 3},    {31, 54, 3},    {32, 54, 3},    {33, 54, 3},    {34, 54, 3},
    {35, 54, 3},    {36, 54, 3},    {37, 54, 3},    {38, 54, 3},    {39, 54, 3},    {40, 54, 3},    {41, 54, 3},    {42, 54, 3},    {43, 54, 3},
    {44, 54, 3},    {45, 54, 3},    {46, 54, 3},    {47, 54, 3},    {48, 54, 3},    {49, 54, 3},    {50, 54, 3},    {51, 54, 3},    {52, 54, 3},
    {53, 54, 3},    {54, 54, 3},    {55, 54, 3},    {56, 54, 3},    {57, 54, 3},    {58, 54, 3},    {59, 54, 3},    {60, 54, 3},    {61, 54, 3},
    {62, 54, 3},    {63, 54, 3},    {64, 54, 3},    {65, 54, 3},    {66, 54, 3},    {67, 54, 3},    {68, 54, 3},    {69, 54, 3},    {70, 54, 3},
    {71, 54, 3},    {72, 54, 3},    {73, 54, 3},    {74, 54, 3},    {75, 54, 3},    {76, 54, 3},    {77, 54, 3},    {78, 54, 3},    {79, 54, 3},
    {80, 54, 3},    {81, 54, 3},    {82, 54, 3},    {83, 54, 3},    {84, 54, 3},    {85, 54, 3},    {86, 54, 3},    {87, 54, 3},    {88, 54, 3},
    {89, 54, 3},    {90, 54, 3},    {91, 54, 3},    {92, 54, 3},    {93, 54, 3},    {94, 54, 3},    {95, 54, 3},    {96, 54, 3},    {97, 54, 3},
    {98, 54, 3},    {99, 54, 3},    {100, 54, 3},   {101, 54, 3},   {102, 54, 3},   {103, 54, 3},   {104, 54, 3},   {105, 54, 3},   {106, 54, 3},
    {107, 54, 3},   {108, 54, 3},   {109, 54, 3},   {110, 54, 3},   {111, 54, 3},   {112, 54, 3},   {113, 54, 3},   {114, 54, 3},   {115, 54, 3},
    {116, 54, 3},   {117, 54, 3},   {118, 54, 3},   {119, 54, 3},   {120, 54, 3},   {121, 54, 3},   {122, 54, 3},   {123, 54, 3},   {124, 54, 3},
    {125, 54, 3},   {126, 54, 3},   {127, 54, 3},   {128, 54, 3},   {129, 54, 3},   {130, 54, 3},   {131, 54, 3},   {132, 54, 3},   {133, 54, 3},
    {134, 54, 3},   {135, 54, 3},   {136, 54, 3},   {137, 54, 3},   {138, 54, 3},   {139, 54, 3},   {140, 54, 3},   {142, 54, 3},   {155, 54, 3},
    {156, 54, 3},   {157, 54, 3},   {158, 54, 3},   {159, 54, 3},   {160, 54, 3},   {161, 54, 3},   {162, 54, 3},   {166, 54, 3},   {167, 54, 3},
    {168, 54, 3},   {-1, 54, 3},    {-2, 54, 3},    {-3, 54, 3},    {-4, 54, 3},    {-5, 54, 3},    {-6, 54, 3},    {-7, 54, 3},    {-8, 54, 3},
    {-9, 54, 3},    {-10, 54, 3},   {-11, 54, 3},   {8, 55, 3},     {9, 55, 3},     {10, 55, 3},    {11, 55, 3},    {12, 55, 3},    {13, 55, 3},
    {14, 55, 3},    {15, 55, 3},    {20, 55, 3},    {21, 55, 3},    {22, 55, 3},    {23, 55, 3},    {24, 55, 3},    {25, 55, 3},    {26, 55, 3},
    {27, 55, 3},    {28, 55, 3},    {29, 55, 3},    {30, 55, 3},    {31, 55, 3},    {32, 55, 3},    {33, 55, 3},    {34, 55, 3},    {35, 55, 3},
    {36, 55, 3},    {37, 55, 3},    {38, 55, 3},    {39, 55, 3},    {40, 55, 3},    {41, 55, 3},    {42, 55, 3},    {43, 55, 3},    {44, 55, 3},
    {45, 55, 3},    {46, 55, 3},    {47, 55, 3},    {48, 55, 3},    {49, 55, 3},    {50, 55, 3},    {51, 55, 3},    {52, 55, 3},    {53, 55, 3},
    {54, 55, 3},    {55, 55, 3},    {56, 55, 3},    {57, 55, 3},    {58, 55, 3},    {59, 55, 3},    {60, 55, 3},    {61, 55, 3},    {62, 55, 3},
    {63, 55, 3},    {64, 55, 3},    {65, 55, 3},    {66, 55, 3},    {67, 55, 3},    {68, 55, 3},    {69, 55, 3},    {70, 55, 3},    {71, 55, 3},
    {72, 55, 3},    {73, 55, 3},    {74, 55, 3},    {75, 55, 3},    {76, 55, 3},    {77, 55, 3},    {78, 55, 3},    {79, 55, 3},    {80, 55, 3},
    {81, 55, 3},    {82, 55, 3},    {83, 55, 3},    {84, 55, 3},    {85, 55, 3},    {86, 55, 3},    {87, 55, 3},    {88, 55, 3},    {89, 55, 3},
    {90, 55, 3},    {91, 55, 3},    {92, 55, 3},    {93, 55, 3},    {94, 55, 3},    {95, 55, 3},    {96, 55, 3},    {97, 55, 3},    {98, 55, 3},
    {99, 55, 3},    {100, 55, 3},   {101, 55, 3},   {102, 55, 3},   {103, 55, 3},   {104, 55, 3},   {105, 55, 3},   {106, 55, 3},   {107, 55, 3},
    {108, 55, 3},   {109, 55, 3},   {110, 55, 3},   {111, 55, 3},   {112, 55, 3},   {113, 55, 3},   {114, 55, 3},   {115, 55, 3},   {116, 55, 3},
    {117, 55, 3},   {118, 55, 3},   {119, 55, 3},   {120, 55, 3},   {121, 55, 3},   {122, 55, 3},   {123, 55, 3},   {124, 55, 3},   {125, 55, 3},
    {126, 55, 3},   {127, 55, 3},   {128, 55, 3},   {129, 55, 3},   {130, 55, 3},   {131, 55, 3},   {132, 55, 3},   {133, 55, 3},   {134, 55, 3},
    {135, 55, 3},   {136, 55, 3},   {137, 55, 3},   {138, 55, 3},   {155, 55, 3},   {156, 55, 3},   {157, 55, 3},   {158, 55, 3},   {159, 55, 3},
    {160, 55, 3},   {161, 55, 3},   {162, 55, 3},   {165, 55, 3},   {166, 55, 3},   {-2, 55, 3},    {-3, 55, 3},    {-4, 55, 3},    {-5, 55, 3},
    {-6, 55, 3},    {-7, 55, 3},    {-8, 55, 3},    {-9, 55, 3},    {8, 56, 3},     {9, 56, 3},     {10, 56, 3},    {11, 56, 3},    {12, 56, 3},
    {13, 56, 3},    {14, 56, 3},    {15, 56, 3},    {16, 56, 3},    {18, 56, 3},    {20, 56, 3},    {21, 56, 3},    {22, 56, 3},    {23, 56, 3},
    {24, 56, 3},    {25, 56, 3},    {26, 56, 3},    {27, 56, 3},    {28, 56, 3},    {29, 56, 3},    {30, 56, 3},    {31, 56, 3},    {32, 56, 3},
    {33, 56, 3},    {34, 56, 3},    {35, 56, 3},    {36, 56, 3},    {37, 56, 3},    {38, 56, 3},    {39, 56, 3},    {40, 56, 3},    {41, 56, 3},
    {42, 56, 3},    {43, 56, 3},    {44, 56, 3},    {45, 56, 3},    {46, 56, 3},    {47, 56, 3},    {48, 56, 3},    {49, 56, 3},    {50, 56, 3},
    {51, 56, 3},    {52, 56, 3},    {53, 56, 3},    {54, 56, 3},    {55, 56, 3},    {56, 56, 3},    {57, 56, 3},    {58, 56, 3},    {59, 56, 3},
    {60, 56, 3},    {61, 56, 3},    {62, 56, 3},    {63, 56, 3},    {64, 56, 3},    {65, 56, 3},    {66, 56, 3},    {67, 56, 3},    {68, 56, 3},
    {69, 56, 3},    {70, 56, 3},    {71, 56, 3},    {72, 56, 3},    {73, 56, 3},    {74, 56, 3},    {75, 56, 3},    {76, 56, 3},    {77, 56, 3},
    {78, 56, 3},    {79, 56, 3},    {80, 56, 3},    {81, 56, 3},    {82, 56, 3},    {83, 56, 3},    {84, 56, 3},    {85, 56, 3},    {86, 56, 3},
    {87, 56, 3},    {88, 56, 3},    {89, 56, 3},    {90, 56, 3},    {91, 56, 3},    {92, 56, 3},    {93, 56, 3},    {94, 56, 3},    {95, 56, 3},
    {96, 56, 3},    {97, 56, 3},    {98, 56, 3},    {99, 56, 3},    {100, 56, 3},   {101, 56, 3},   {102, 56, 3},   {103, 56, 3},   {104, 56, 3},
    {105, 56, 3},   {106, 56, 3},   {107, 56, 3},   {108, 56, 3},   {109, 56, 3},   {110, 56, 3},   {111, 56, 3},   {112, 56, 3},   {113, 56, 3},
    {114, 56, 3},   {115, 56, 3},   {116, 56, 3},   {117, 56, 3},   {118, 56, 3},   {119, 56, 3},   {120, 56, 3},   {121, 56, 3},   {122, 56, 3},
    {123, 56, 3},   {124, 56, 3},   {125, 56, 3},   {126, 56, 3},   {127, 56, 3},   {128, 56, 3},   {129, 56, 3},   {130, 56, 3},   {131, 56, 3},
    {132, 56, 3},   {133, 56, 3},   {134, 56, 3},   {135, 56, 3},   {136, 56, 3},   {137, 56, 3},   {138, 56, 3},   {143, 56, 3},   {155, 56, 3},
    {156, 56, 3},   {157, 56, 3},   {158, 56, 3},   {159, 56, 3},   {160, 56, 3},   {161, 56, 3},   {162, 56, 3},   {163, 56, 3},   {-3, 56, 3},
    {-4, 56, 3},    {-5, 56, 3},    {-6, 56, 3},    {-7, 56, 3},    {-8, 56, 3},    {6, 57, 3},     {7, 57, 3},     {8, 57, 3},     {9, 57, 3},
    {10, 57, 3},    {11, 57, 3},    {12, 57, 3},    {13, 57, 3},    {14, 57, 3},    {15, 57, 3},    {16, 57, 3},    {17, 57, 3},    {18, 57, 3},
    {19, 57, 3},    {21, 57, 3},    {22, 57, 3},    {23, 57, 3},    {24, 57, 3},    {25, 57, 3},    {26, 57, 3},    {27, 57, 3},    {28, 57, 3},
    {29, 57, 3},    {30, 57, 3},    {31, 57, 3},    {32, 57, 3},    {33, 57, 3},    {34, 57, 3},    {35, 57, 3},    {36, 57, 3},    {37, 57, 3},
    {38, 57, 3},    {39, 57, 3},    {40, 57, 3},    {41, 57, 3},    {42, 57, 3},    {43, 57, 3},    {44, 57, 3},    {45, 57, 3},    {46, 57, 3},
    {47, 57, 3},    {48, 57, 3},    {49, 57, 3},    {50, 57, 3},    {51, 57, 3},    {52, 57, 3},    {53, 57, 3},    {54, 57, 3},    {55, 57, 3},
    {56, 57, 3},    {57, 57, 3},    {58, 57, 3},    {59, 57, 3},    {60, 57, 3},    {61, 57, 3},    {62, 57, 3},    {63, 57, 3},    {64, 57, 3},
    {65, 57, 3},    {66, 57, 3},    {67, 57, 3},    {68, 57, 3},    {69, 57, 3},    {70, 57, 3},    {71, 57, 3},    {72, 57, 3},    {73, 57, 3},
    {74, 57, 3},    {75, 57, 3},    {76, 57, 3},    {77, 57, 3},    {78, 57, 3},    {79, 57, 3},    {80, 57, 3},    {81, 57, 3},    {82, 57, 3},
    {83, 57, 3},    {84, 57, 3},    {85, 57, 3},    {86, 57, 3},    {87, 57, 3},    {88, 57, 3},    {89, 57, 3},    {90, 57, 3},    {91, 57, 3},
    {92, 57, 3},    {93, 57, 3},    {94, 57, 3},    {95, 57, 3},    {96, 57, 3},    {97, 57, 3},    {98, 57, 3},    {99, 57, 3},    {100, 57, 3},
    {101, 57, 3},   {102, 57, 3},   {103, 57, 3},   {104, 57, 3},   {105, 57, 3},   {106, 57, 3},   {107, 57, 3},   {108, 57, 3},   {109, 57, 3},
    {110, 57, 3},   {111, 57, 3},   {112, 57, 3},   {113, 57, 3},   {114, 57, 3},   {115, 57, 3},   {116, 57, 3},   {117, 57, 3},   {118, 57, 3},
    {119, 57, 3},   {120, 57, 3},   {121, 57, 3},   {122, 57, 3},   {123, 57, 3},   {124, 57, 3},   {125, 57, 3},   {126, 57, 3},   {127, 57, 3},
    {128, 57, 3},   {129, 57, 3},   {130, 57, 3},   {131, 57, 3},   {132, 57, 3},   {133, 57, 3},   {134, 57, 3},   {135, 57, 3},   {136, 57, 3},
    {137, 57, 3},   {138, 57, 3},   {139, 57, 3},   {140, 57, 3},   {156, 57, 3},   {157, 57, 3},   {158, 57, 3},   {159, 57, 3},   {160, 57, 3},
    {161, 57, 3},   {162, 57, 3},   {163, 57, 3},   {-2, 57, 3},    {-3, 57, 3},    {-4, 57, 3},    {-5, 57, 3},    {-6, 57, 3},    {-7, 57, 3},
    {-8, 57, 3},    {-9, 57, 3},    {-14, 57, 3},   {5, 58, 3},     {6, 58, 3},     {7, 58, 3},     {8, 58, 3},     {9, 58, 3},     {10, 58, 3},
    {11, 58, 3},    {12, 58, 3},    {13, 58, 3},    {14, 58, 3},    {15, 58, 3},    {16, 58, 3},    {17, 58, 3},    {18, 58, 3},    {19, 58, 3},
    {21, 58, 3},    {22, 58, 3},    {23, 58, 3},    {24, 58, 3},    {25, 58, 3},    {26, 58, 3},    {27, 58, 3},    {28, 58, 3},    {29, 58, 3},
    {30, 58, 3},    {31, 58, 3},    {32, 58, 3},    {33, 58, 3},    {34, 58, 3},    {35, 58, 3},    {36, 58, 3},    {37, 58, 3},    {38, 58, 3},
    {39, 58, 3},    {40, 58, 3},    {41, 58, 3},    {42, 58, 3},    {43, 58, 3},    {44, 58, 3},    {45, 58, 3},    {46, 58, 3},    {47, 58, 3},
    {48, 58, 3},    {49, 58, 3},    {50, 58, 3},    {51, 58, 3},    {52, 58, 3},    {53, 58, 3},    {54, 58, 3},    {55, 58, 3},    {56, 58, 3},
    {57, 58, 3},    {58, 58, 3},    {59, 58, 3},    {60, 58, 3},    {61, 58, 3},    {62, 58, 3},    {63, 58, 3},    {64, 58, 3},    {65, 58, 3},
    {66, 58, 3},    {67, 58, 3},    {68, 58, 3},    {69, 58, 3},    {70, 58, 3},    {71, 58, 3},    {72, 58, 3},    {73, 58, 3},    {74, 58, 3},
    {75, 58, 3},    {76, 58, 3},    {77, 58, 3},    {78, 58, 3},    {79, 58, 3},    {80, 58, 3},    {81, 58, 3},    {82, 58, 3},    {83, 58, 3},
    {84, 58, 3},    {85, 58, 3},    {86, 58, 3},    {87, 58, 3},    {88, 58, 3},    {89, 58, 3},    {90, 58, 3},    {91, 58, 3},    {92, 58, 3},
    {93, 58, 3},    {94, 58, 3},    {95, 58, 3},    {96, 58, 3},    {97, 58, 3},    {98, 58, 3},    {99, 58, 3},    {100, 58, 3},   {101, 58, 3},
    {102, 58, 3},   {103, 58, 3},   {104, 58, 3},   {105, 58, 3},   {106, 58, 3},   {107, 58, 3},   {108, 58, 3},   {109, 58, 3},   {110, 58, 3},
    {111, 58, 3},   {112, 58, 3},   {113, 58, 3},   {114, 58, 3},   {115, 58, 3},   {116, 58, 3},   {117, 58, 3},   {118, 58, 3},   {119, 58, 3},
    {120, 58, 3},   {121, 58, 3},   {122, 58, 3},   {123, 58, 3},   {124, 58, 3},   {125, 58, 3},   {126, 58, 3},   {127, 58, 3},   {128, 58, 3},
    {129, 58, 3},   {130, 58, 3},   {131, 58, 3},   {132, 58, 3},   {133, 58, 3},   {134, 58, 3},   {135, 58, 3},   {136, 58, 3},   {137, 58, 3},
    {138, 58, 3},   {139, 58, 3},   {140, 58, 3},   {141, 58, 3},   {142, 58, 3},   {150, 58, 3},   {151, 58, 3},   {152, 58, 3},   {157, 58, 3},
    {158, 58, 3},   {159, 58, 3},   {160, 58, 3},   {161, 58, 3},   {162, 58, 3},   {163, 58, 3},   {164, 58, 3},   {-3, 58, 3},    {-4, 58, 3},
    {-5, 58, 3},    {-6, 58, 3},    {-7, 58, 3},    {-8, 58, 3},    {4, 59, 3},     {5, 59, 3},     {6, 59, 3},     {7, 59, 3},     {8, 59, 3},
    {9, 59, 3},     {10, 59, 3},    {11, 59, 3},    {12, 59, 3},    {13, 59, 3},    {14, 59, 3},    {15, 59, 3},    {16, 59, 3},    {17, 59, 3},
    {18, 59, 3},    {19, 59, 3},    {20, 59, 3},    {21, 59, 3},    {22, 59, 3},    {23, 59, 3},    {24, 59, 3},    {25, 59, 3},    {26, 59, 3},
    {27, 59, 3},    {28, 59, 3},    {29, 59, 3},    {30, 59, 3},    {31, 59, 3},    {32, 59, 3},    {33, 59, 3},    {34, 59, 3},    {35, 59, 3},
    {36, 59, 3},    {37, 59, 3},    {38, 59, 3},    {39, 59, 3},    {40, 59, 3},    {41, 59, 3},    {42, 59, 3},    {43, 59, 3},    {44, 59, 3},
    {45, 59, 3},    {46, 59, 3},    {47, 59, 3},    {48, 59, 3},    {49, 59, 3},    {50, 59, 3},    {51, 59, 3},    {52, 59, 3},    {53, 59, 3},
    {54, 59, 3},    {55, 59, 3},    {56, 59, 3},    {57, 59, 3},    {58, 59, 3},    {59, 59, 3},    {60, 59, 3},    {61, 59, 3},    {62, 59, 3},
    {63, 59, 3},    {64, 59, 3},    {65, 59, 3},    {66, 59, 3},    {67, 59, 3},    {68, 59, 3},    {69, 59, 3},    {70, 59, 3},    {71, 59, 3},
    {72, 59, 3},    {73, 59, 3},    {74, 59, 3},    {75, 59, 3},    {76, 59, 3},    {77, 59, 3},    {78, 59, 3},    {79, 59, 3},    {80, 59, 3},
    {81, 59, 3},    {82, 59, 3},    {83, 59, 3},    {84, 59, 3},    {85, 59, 3},    {86, 59, 3},    {87, 59, 3},    {88, 59, 3},    {89, 59, 3},
    {90, 59, 3},    {91, 59, 3},    {92, 59, 3},    {93, 59, 3},    {94, 59, 3},    {95, 59, 3},    {96, 59, 3},    {97, 59, 3},    {98, 59, 3},
    {99, 59, 3},    {100, 59, 3},   {101, 59, 3},   {102, 59, 3},   {103, 59, 3},   {104, 59, 3},   {105, 59, 3},   {106, 59, 3},   {107, 59, 3},
    {108, 59, 3},   {109, 59, 3},   {110, 59, 3},   {111, 59, 3},   {112, 59, 3},   {113, 59, 3},   {114, 59, 3},   {115, 59, 3},   {116, 59, 3},
    {117, 59, 3},   {118, 59, 3},   {119, 59, 3},   {120, 59, 3},   {121, 59, 3},   {122, 59, 3},   {123, 59, 3},   {124, 59, 3},   {125, 59, 3},
    {126, 59, 3},   {127, 59, 3},   {128, 59, 3},   {129, 59, 3},   {130, 59, 3},   {131, 59, 3},   {132, 59, 3},   {133, 59, 3},   {134, 59, 3},
    {135, 59, 3},   {136, 59, 3},   {137, 59, 3},   {138, 59, 3},   {139, 59, 3},   {140, 59, 3},   {141, 59, 3},   {142, 59, 3},   {143, 59, 3},
    {144, 59, 3},   {145, 59, 3},   {146, 59, 3},   {147, 59, 3},   {148, 59, 3},   {149, 59, 3},   {150, 59, 3},   {151, 59, 3},   {152, 59, 3},
    {153, 59, 3},   {154, 59, 3},   {155, 59, 3},   {159, 59, 3},   {160, 59, 3},   {161, 59, 3},   {162, 59, 3},   {163, 59, 3},   {164, 59, 3},
    {165, 59, 3},   {166, 59, 3},   {-2, 59, 3},    {-3, 59, 3},    {-4, 59, 3},    {-5, 59, 3},    {-6, 59, 3},    {-7, 59, 3},    {4, 60, 3},
    {5, 60, 3},     {6, 60, 3},     {7, 60, 3},     {8, 60, 3},     {9, 60, 3},     {10, 60, 3},    {11, 60, 3},    {12, 60, 3},    {13, 60, 3},
    {14, 60, 3},    {15, 60, 3},    {16, 60, 3},    {17, 60, 3},    {18, 60, 3},    {19, 60, 3},    {20, 60, 3},    {21, 60, 3},    {22, 60, 3},
    {23, 60, 3},    {24, 60, 3},    {25, 60, 3},    {26, 60, 3},    {27, 60, 3},    {28, 60, 3},    {29, 60, 3},    {30, 60, 3},    {31, 60, 3},
    {32, 60, 3},    {33, 60, 3},    {34, 60, 3},    {35, 60, 3},    {36, 60, 3},    {37, 60, 3},    {38, 60, 3},    {39, 60, 3},    {40, 60, 3},
    {41, 60, 3},    {42, 60, 3},    {43, 60, 3},    {44, 60, 3},    {45, 60, 3},    {46, 60, 3},    {47, 60, 3},    {48, 60, 3},    {49, 60, 3},
    {50, 60, 3},    {51, 60, 3},    {52, 60, 3},    {53, 60, 3},    {54, 60, 3},    {55, 60, 3},    {56, 60, 3},    {57, 60, 3},    {58, 60, 3},
    {59, 60, 3},    {60, 60, 3},    {61, 60, 3},    {62, 60, 3},    {63, 60, 3},    {64, 60, 3},    {65, 60, 3},    {66, 60, 3},    {67, 60, 3},
    {68, 60, 3},    {69, 60, 3},    {70, 60, 3},    {71, 60, 3},    {72, 60, 3},    {73, 60, 3},    {74, 60, 3},    {75, 60, 3},    {76, 60, 3},
    {77, 60, 3},    {78, 60, 3},    {79, 60, 3},    {80, 60, 3},    {81, 60, 3},    {82, 60, 3},    {83, 60, 3},    {84, 60, 3},    {85, 60, 3},
    {86, 60, 3},    {87, 60, 3},    {88, 60, 3},    {89, 60, 3},    {90, 60, 3},    {91, 60, 3},    {92, 60, 3},    {93, 60, 3},    {94, 60, 3},
    {95, 60, 3},    {96, 60, 3},    {97, 60, 3},    {98, 60, 3},    {99, 60, 3},    {100, 60, 3},   {101, 60, 3},   {102, 60, 3},   {103, 60, 3},
    {104, 60, 3},   {105, 60, 3},   {106, 60, 3},   {107, 60, 3},   {108, 60, 3},   {109, 60, 3},   {110, 60, 3},   {111, 60, 3},   {112, 60, 3},
    {113, 60, 3},   {114, 60, 3},   {115, 60, 3},   {116, 60, 3},   {117, 60, 3},   {118, 60, 3},   {119, 60, 3},   {120, 60, 3},   {121, 60, 3},
    {122, 60, 3},   {123, 60, 3},   {124, 60, 3},   {125, 60, 3},   {126, 60, 3},   {127, 60, 3},   {128, 60, 3},   {129, 60, 3},   {130, 60, 3},
    {131, 60, 3},   {132, 60, 3},   {133, 60, 3},   {134, 60, 3},   {135, 60, 3},   {136, 60, 3},   {137, 60, 3},   {138, 60, 3},   {139, 60, 3},
    {140, 60, 3},   {141, 60, 3},   {142, 60, 3},   {143, 60, 3},   {144, 60, 3},   {145, 60, 3},   {146, 60, 3},   {147, 60, 3},   {148, 60, 3},
    {149, 60, 3},   {150, 60, 3},   {151, 60, 3},   {152, 60, 3},   {153, 60, 3},   {154, 60, 3},   {155, 60, 3},   {156, 60, 3},   {159, 60, 3},
    {160, 60, 3},   {161, 60, 3},   {162, 60, 3},   {163, 60, 3},   {164, 60, 3},   {165, 60, 3},   {166, 60, 3},   {167, 60, 3},   {168, 60, 3},
    {169, 60, 3},   {170, 60, 3},   {171, 60, 3},   {172, 60, 3},   {-1, 60, 3},    {-2, 60, 3},    {-3, 60, 3},    {73, -1, 3},    {98, -1, 3},
    {99, -1, 3},    {100, -1, 3},   {101, -1, 3},   {102, -1, 3},   {103, -1, 3},   {104, -1, 3},   {105, -1, 3},   {109, -1, 3},   {110, -1, 3},
    {111, -1, 3},   {112, -1, 3},   {113, -1, 3},   {114, -1, 3},   {115, -1, 3},   {116, -1, 3},   {117, -1, 3},   {119, -1, 3},   {120, -1, 3},
    {121, -1, 3},   {122, -1, 3},   {123, -1, 3},   {127, -1, 3},   {128, -1, 3},   {129, -1, 3},   {130, -1, 3},   {131, -1, 3},   {132, -1, 3},
    {133, -1, 3},   {134, -1, 3},   {135, -1, 3},   {136, -1, 3},   {145, -1, 3},   {166, -1, 3},   {169, -1, 3},   {174, -1, 3},   {-161, -1, 3},
    {98, -2, 3},    {99, -2, 3},    {100, -2, 3},   {101, -2, 3},   {102, -2, 3},   {103, -2, 3},   {104, -2, 3},   {105, -2, 3},   {106, -2, 3},
    {108, -2, 3},   {109, -2, 3},   {110, -2, 3},   {111, -2, 3},   {112, -2, 3},   {113, -2, 3},   {114, -2, 3},   {115, -2, 3},   {116, -2, 3},
    {117, -2, 3},   {119, -2, 3},   {120, -2, 3},   {121, -2, 3},   {122, -2, 3},   {123, -2, 3},   {124, -2, 3},   {125, -2, 3},   {126, -2, 3},
    {127, -2, 3},   {128, -2, 3},   {129, -2, 3},   {130, -2, 3},   {131, -2, 3},   {132, -2, 3},   {133, -2, 3},   {134, -2, 3},   {135, -2, 3},
    {136, -2, 3},   {137, -2, 3},   {138, -2, 3},   {139, -2, 3},   {142, -2, 3},   {143, -2, 3},   {144, -2, 3},   {145, -2, 3},   {146, -2, 3},
    {147, -2, 3},   {148, -2, 3},   {149, -2, 3},   {150, -2, 3},   {174, -2, 3},   {175, -2, 3},   {176, -2, 3},   {99, -3, 3},    {100, -3, 3},
    {101, -3, 3},   {102, -3, 3},   {103, -3, 3},   {104, -3, 3},   {105, -3, 3},   {106, -3, 3},   {107, -3, 3},   {108, -3, 3},   {110, -3, 3},
    {111, -3, 3},   {112, -3, 3},   {113, -3, 3},   {114, -3, 3},   {115, -3, 3},   {116, -3, 3},   {117, -3, 3},   {118, -3, 3},   {119, -3, 3},
    {120, -3, 3},   {121, -3, 3},   {122, -3, 3},   {123, -3, 3},   {124, -3, 3},   {125, -3, 3},   {126, -3, 3},   {127, -3, 3},   {128, -3, 3},
    {129, -3, 3},   {130, -3, 3},   {131, -3, 3},   {132, -3, 3},   {133, -3, 3},   {134, -3, 3},   {135, -3, 3},   {136, -3, 3},   {137, -3, 3},
    {138, -3, 3},   {139, -3, 3},   {140, -3, 3},   {141, -3, 3},   {142, -3, 3},   {145, -3, 3},   {146, -3, 3},   {147, -3, 3},   {148, -3, 3},
    {149, -3, 3},   {150, -3, 3},   {151, -3, 3},   {152, -3, 3},   {175, -3, 3},   {176, -3, 3},   {-172, -3, 3},  {100, -4, 3},   {101, -4, 3},
    {102, -4, 3},   {103, -4, 3},   {104, -4, 3},   {105, -4, 3},   {106, -4, 3},   {107, -4, 3},   {108, -4, 3},   {110, -4, 3},   {111, -4, 3},
    {112, -4, 3},   {113, -4, 3},   {114, -4, 3},   {115, -4, 3},   {116, -4, 3},   {117, -4, 3},   {118, -4, 3},   {119, -4, 3},   {120, -4, 3},
    {121, -4, 3},   {122, -4, 3},   {123, -4, 3},   {125, -4, 3},   {126, -4, 3},   {127, -4, 3},   {128, -4, 3},   {129, -4, 3},   {130, -4, 3},
    {131, -4, 3},   {132, -4, 3},   {133, -4, 3},   {134, -4, 3},   {135, -4, 3},   {136, -4, 3},   {137, -4, 3},   {138, -4, 3},   {139, -4, 3},
    {140, -4, 3},   {141, -4, 3},   {142, -4, 3},   {143, -4, 3},   {144, -4, 3},   {150, -4, 3},   {151, -4, 3},   {152, -4, 3},   {153, -4, 3},
    {154, -4, 3},   {-155, -4, 3},  {-171, -4, 3},  {-172, -4, 3},  {-175, -4, 3},  {101, -5, 3},   {102, -5, 3},   {103, -5, 3},   {104, -5, 3},
    {105, -5, 3},   {114, -5, 3},   {115, -5, 3},   {116, -5, 3},   {119, -5, 3},   {120, -5, 3},   {121, -5, 3},   {122, -5, 3},   {123, -5, 3},
    {129, -5, 3},   {130, -5, 3},   {131, -5, 3},   {132, -5, 3},   {133, -5, 3},   {134, -5, 3},   {135, -5, 3},   {136, -5, 3},   {137, -5, 3},
    {138, -5, 3},   {139, -5, 3},   {140, -5, 3},   {141, -5, 3},   {142, -5, 3},   {143, -5, 3},   {144, -5, 3},   {145, -5, 3},   {146, -5, 3},
    {149, -5, 3},   {150, -5, 3},   {151, -5, 3},   {152, -5, 3},   {153, -5, 3},   {154, -5, 3},   {155, -5, 3},   {156, -5, 3},   {157, -5, 3},
    {159, -5, 3},   {-155, -5, 3},  {-172, -5, 3},  {-173, -5, 3},  {-175, -5, 3},  {71, -6, 3},    {72, -6, 3},    {102, -6, 3},   {103, -6, 3},
    {104, -6, 3},   {105, -6, 3},   {106, -6, 3},   {107, -6, 3},   {108, -6, 3},   {110, -6, 3},   {112, -6, 3},   {114, -6, 3},   {117, -6, 3},
    {118, -6, 3},   {119, -6, 3},   {120, -6, 3},   {121, -6, 3},   {122, -6, 3},   {123, -6, 3},   {124, -6, 3},   {127, -6, 3},   {130, -6, 3},
    {131, -6, 3},   {132, -6, 3},   {133, -6, 3},   {134, -6, 3},   {137, -6, 3},   {138, -6, 3},   {139, -6, 3},   {140, -6, 3},   {141, -6, 3},
    {142, -6, 3},   {143, -6, 3},   {144, -6, 3},   {145, -6, 3},   {146, -6, 3},   {147, -6, 3},   {148, -6, 3},   {149, -6, 3},   {150, -6, 3},
    {151, -6, 3},   {152, -6, 3},   {154, -6, 3},   {155, -6, 3},   {159, -6, 3},   {176, -6, 3},   {-156, -6, 3},  {71, -7, 3},    {105, -7, 3},
    {106, -7, 3},   {107, -7, 3},   {108, -7, 3},   {109, -7, 3},   {110, -7, 3},   {111, -7, 3},   {112, -7, 3},   {113, -7, 3},   {114, -7, 3},
    {115, -7, 3},   {116, -7, 3},   {118, -7, 3},   {119, -7, 3},   {120, -7, 3},   {121, -7, 3},   {122, -7, 3},   {124, -7, 3},   {126, -7, 3},
    {129, -7, 3},   {130, -7, 3},   {131, -7, 3},   {132, -7, 3},   {134, -7, 3},   {138, -7, 3},   {139, -7, 3},   {140, -7, 3},   {141, -7, 3},
    {142, -7, 3},   {143, -7, 3},   {144, -7, 3},   {145, -7, 3},   {146, -7, 3},   {147, -7, 3},   {148, -7, 3},   {149, -7, 3},   {150, -7, 3},
    {151, -7, 3},   {154, -7, 3},   {155, -7, 3},   {156, -7, 3},   {157, -7, 3},   {176, -7, 3},   {177, -7, 3},   {72, -8, 3},    {105, -8, 3},
    {106, -8, 3},   {107, -8, 3},   {108, -8, 3},   {109, -8, 3},   {110, -8, 3},   {111, -8, 3},   {112, -8, 3},   {113, -8, 3},   {114, -8, 3},
    {115, -8, 3},   {117, -8, 3},   {118, -8, 3},   {120, -8, 3},   {121, -8, 3},   {122, -8, 3},   {123, -8, 3},   {125, -8, 3},   {126, -8, 3},
    {127, -8, 3},   {128, -8, 3},   {129, -8, 3},   {130, -8, 3},   {131, -8, 3},   {134, -8, 3},   {137, -8, 3},   {138, -8, 3},   {139, -8, 3},
    {140, -8, 3},   {141, -8, 3},   {142, -8, 3},   {143, -8, 3},   {144, -8, 3},   {145, -8, 3},   {146, -8, 3},   {147, -8, 3},   {155, -8, 3},
    {156, -8, 3},   {157, -8, 3},   {158, -8, 3},   {159, -8, 3},   {160, -8, 3},   {177, -8, 3},   {178, -8, 3},   {-141, -8, 3},  {110, -9, 3},
    {111, -9, 3},   {112, -9, 3},   {113, -9, 3},   {114, -9, 3},   {115, -9, 3},   {116, -9, 3},   {117, -9, 3},   {118, -9, 3},   {119, -9, 3},
    {120, -9, 3},   {121, -9, 3},   {122, -9, 3},   {123, -9, 3},   {124, -9, 3},   {125, -9, 3},   {126, -9, 3},   {127, -9, 3},   {128, -9, 3},
    {129, -9, 3},   {130, -9, 3},   {131, -9, 3},   {137, -9, 3},   {138, -9, 3},   {139, -9, 3},   {140, -9, 3},   {141, -9, 3},   {142, -9, 3},
    {143, -9, 3},   {145, -9, 3},   {146, -9, 3},   {147, -9, 3},   {148, -9, 3},   {149, -9, 3},   {150, -9, 3},   {151, -9, 3},   {152, -9, 3},
    {156, -9, 3},   {157, -9, 3},   {158, -9, 3},   {159, -9, 3},   {160, -9, 3},   {161, -9, 3},   {178, -9, 3},   {179, -9, 3},   {-140, -9, 3},
    {-141, -9, 3},  {-158, -9, 3},  {-159, -9, 3},  {-173, -9, 3},  {116, -10, 3},  {117, -10, 3},  {118, -10, 3},  {119, -10, 3},  {120, -10, 3},
    {123, -10, 3},  {124, -10, 3},  {125, -10, 3},  {126, -10, 3},  {140, -10, 3},  {141, -10, 3},  {142, -10, 3},  {143, -10, 3},  {144, -10, 3},
    {146, -10, 3},  {147, -10, 3},  {148, -10, 3},  {149, -10, 3},  {150, -10, 3},  {151, -10, 3},  {152, -10, 3},  {153, -10, 3},  {158, -10, 3},
    {159, -10, 3},  {160, -10, 3},  {161, -10, 3},  {167, -10, 3},  {179, -10, 3},  {-139, -10, 3}, {-140, -10, 3}, {-141, -10, 3}, {-151, -10, 3},
    {-158, -10, 3}, {-159, -10, 3}, {-162, -10, 3}, {-172, -10, 3}, {105, -11, 3},  {96, -12, 3},   {96, -13, 3},   {-170, 16, 4},  {-156, 18, 4},
    {-155, 19, 4},  {-156, 19, 4},  {-157, 19, 4},  {-156, 20, 4},  {-157, 20, 4},  {-158, 20, 4},  {-157, 21, 4},  {-158, 21, 4},  {-159, 21, 4},
    {-160, 21, 4},  {-161, 21, 4},  {-160, 22, 4},  {-161, 22, 4},  {-162, 23, 4},  {-165, 23, 4},  {-167, 23, 4},  {-168, 24, 4},  {-168, 25, 4},
    {-172, 25, 4},  {-174, 26, 4},  {-176, 27, 4},  {-178, 28, 4},  {-179, 28, 4},  {-15, -8, 4},   {-6, -16, 4},   {-6, -17, 4},   {-29, -21, 4},
    {-30, -21, 4},  {167, -29, 4},  {167, -30, 4},  {-178, -30, 4}, {-179, -31, 4}, {-179, -32, 4}, {172, -35, 4},  {173, -35, 4},  {173, -36, 4},
    {174, -36, 4},  {175, -36, 4},  {173, -37, 4},  {174, -37, 4},  {175, -37, 4},  {176, -37, 4},  {77, -38, 4},   {174, -38, 4},  {175, -38, 4},
    {176, -38, 4},  {177, -38, 4},  {178, -38, 4},  {-13, -38, 4},  {77, -39, 4},   {174, -39, 4},  {175, -39, 4},  {176, -39, 4},  {177, -39, 4},
    {178, -39, 4},  {173, -40, 4},  {174, -40, 4},  {175, -40, 4},  {176, -40, 4},  {177, -40, 4},  {178, -40, 4},  {172, -41, 4},  {173, -41, 4},
    {174, -41, 4},  {175, -41, 4},  {176, -41, 4},  {-10, -41, 4},  {-11, -41, 4},  {171, -42, 4},  {172, -42, 4},  {173, -42, 4},  {174, -42, 4},
    {175, -42, 4},  {176, -42, 4},  {170, -43, 4},  {171, -43, 4},  {172, -43, 4},  {173, -43, 4},  {174, -43, 4},  {168, -44, 4},  {169, -44, 4},
    {170, -44, 4},  {171, -44, 4},  {172, -44, 4},  {173, -44, 4},  {-176, -44, 4}, {-177, -44, 4}, {167, -45, 4},  {168, -45, 4},  {169, -45, 4},
    {170, -45, 4},  {171, -45, 4},  {-176, -45, 4}, {-177, -45, 4}, {50, -46, 4},   {166, -46, 4},  {167, -46, 4},  {168, -46, 4},  {169, -46, 4},
    {170, -46, 4},  {171, -46, 4},  {37, -47, 4},   {38, -47, 4},   {50, -47, 4},   {51, -47, 4},   {52, -47, 4},   {166, -47, 4},  {167, -47, 4},
    {168, -47, 4},  {169, -47, 4},  {170, -47, 4},  {167, -48, 4},  {168, -48, 4},  {179, -48, 4},  {68, -49, 4},   {69, -49, 4},   {166, -49, 4},
    {68, -50, 4},   {69, -50, 4},   {70, -50, 4},   {178, -50, 4},  {68, -51, 4},   {165, -51, 4},  {166, -51, 4},  {73, -53, 4},   {168, -53, 4},
    {169, -53, 4},  {72, -54, 4},   {73, -54, 4},   {-38, -54, 4},  {-39, -54, 4},  {3, -55, 4},    {158, -55, 4},  {-36, -55, 4},  {-37, -55, 4},
    {-38, -55, 4},  {-39, -55, 4},  {158, -56, 4},  {-35, -56, 4},  {-110, 10, 5},  {-62, 15, 5},   {-64, 15, 5},   {-79, 15, 5},   {-80, 15, 5},
    {-83, 15, 5},   {-84, 15, 5},   {-85, 15, 5},   {-86, 15, 5},   {-87, 15, 5},   {-88, 15, 5},   {-89, 15, 5},   {-90, 15, 5},   {-91, 15, 5},
    {-92, 15, 5},   {-93, 15, 5},   {-94, 15, 5},   {-96, 15, 5},   {-97, 15, 5},   {-98, 15, 5},   {-62, 16, 5},   {-63, 16, 5},   {-86, 16, 5},
    {-87, 16, 5},   {-88, 16, 5},   {-89, 16, 5},   {-90, 16, 5},   {-91, 16, 5},   {-92, 16, 5},   {-93, 16, 5},   {-94, 16, 5},   {-95, 16, 5},
    {-96, 16, 5},   {-97, 16, 5},   {-98, 16, 5},   {-99, 16, 5},   {-100, 16, 5},  {-101, 16, 5},  {-62, 17, 5},   {-63, 17, 5},   {-64, 17, 5},
    {-65, 17, 5},   {-66, 17, 5},   {-67, 17, 5},   {-68, 17, 5},   {-72, 17, 5},   {-76, 17, 5},   {-77, 17, 5},   {-78, 17, 5},   {-84, 17, 5},
    {-88, 17, 5},   {-89, 17, 5},   {-90, 17, 5},   {-91, 17, 5},   {-92, 17, 5},   {-93, 17, 5},   {-94, 17, 5},   {-95, 17, 5},   {-96, 17, 5},
    {-97, 17, 5},   {-98, 17, 5},   {-99, 17, 5},   {-100, 17, 5},  {-101, 17, 5},  {-102, 17, 5},  {-103, 17, 5},  {-63, 18, 5},   {-64, 18, 5},
    {-65, 18, 5},   {-66, 18, 5},   {-67, 18, 5},   {-68, 18, 5},   {-69, 18, 5},   {-70, 18, 5},   {-71, 18, 5},   {-72, 18, 5},   {-73, 18, 5},
    {-74, 18, 5},   {-75, 18, 5},   {-76, 18, 5},   {-77, 18, 5},   {-78, 18, 5},   {-79, 18, 5},   {-88, 18, 5},   {-89, 18, 5},   {-90, 18, 5},
    {-91, 18, 5},   {-92, 18, 5},   {-93, 18, 5},   {-94, 18, 5},   {-95, 18, 5},   {-96, 18, 5},   {-97, 18, 5},   {-98, 18, 5},   {-99, 18, 5},
    {-100, 18, 5},  {-101, 18, 5},  {-102, 18, 5},  {-103, 18, 5},  {-104, 18, 5},  {-105, 18, 5},  {-111, 18, 5},  {-112, 18, 5},  {-115, 18, 5},
    {-69, 19, 5},   {-70, 19, 5},   {-71, 19, 5},   {-72, 19, 5},   {-73, 19, 5},   {-74, 19, 5},   {-75, 19, 5},   {-76, 19, 5},   {-77, 19, 5},
    {-78, 19, 5},   {-80, 19, 5},   {-81, 19, 5},   {-82, 19, 5},   {-88, 19, 5},   {-89, 19, 5},   {-90, 19, 5},   {-91, 19, 5},   {-92, 19, 5},
    {-96, 19, 5},   {-97, 19, 5},   {-98, 19, 5},   {-99, 19, 5},   {-100, 19, 5},  {-101, 19, 5},  {-102, 19, 5},  {-103, 19, 5},  {-104, 19, 5},
    {-105, 19, 5},  {-106, 19, 5},  {-111, 19, 5},  {-73, 20, 5},   {-74, 20, 5},   {-75, 20, 5},   {-76, 20, 5},   {-77, 20, 5},   {-78, 20, 5},
    {-79, 20, 5},   {-80, 20, 5},   {-87, 20, 5},   {-88, 20, 5},   {-89, 20, 5},   {-90, 20, 5},   {-91, 20, 5},   {-92, 20, 5},   {-93, 20, 5},
    {-97, 20, 5},   {-98, 20, 5},   {-99, 20, 5},   {-100, 20, 5},  {-101, 20, 5},  {-102, 20, 5},  {-103, 20, 5},  {-104, 20, 5},  {-105, 20, 5},
    {-106, 20, 5},  {-72, 21, 5},   {-73, 21, 5},   {-74, 21, 5},   {-76, 21, 5},   {-77, 21, 5},   {-78, 21, 5},   {-79, 21, 5},   {-80, 21, 5},
    {-81, 21, 5},   {-82, 21, 5},   {-83, 21, 5},   {-84, 21, 5},   {-85, 21, 5},   {-87, 21, 5},   {-88, 21, 5},   {-89, 21, 5},   {-90, 21, 5},
    {-91, 21, 5},   {-98, 21, 5},   {-99, 21, 5},   {-100, 21, 5},  {-101, 21, 5},  {-102, 21, 5},  {-103, 21, 5},  {-104, 21, 5},  {-105, 21, 5},
    {-106, 21, 5},  {-107, 21, 5},  {-73, 22, 5},   {-74, 22, 5},   {-75, 22, 5},   {-76, 22, 5},   {-78, 22, 5},   {-79, 22, 5},   {-80, 22, 5},
    {-81, 22, 5},   {-82, 22, 5},   {-83, 22, 5},   {-84, 22, 5},   {-85, 22, 5},   {-90, 22, 5},   {-92, 22, 5},   {-98, 22, 5},   {-99, 22, 5},
    {-100, 22, 5},  {-101, 22, 5},  {-102, 22, 5},  {-103, 22, 5},  {-104, 22, 5},  {-105, 22, 5},  {-106, 22, 5},  {-107, 22, 5},  {-110, 22, 5},
    {-111, 22, 5},  {-74, 23, 5},   {-75, 23, 5},   {-76, 23, 5},   {-77, 23, 5},   {-78, 23, 5},   {-80, 23, 5},   {-81, 23, 5},   {-82, 23, 5},
    {-83, 23, 5},   {-84, 23, 5},   {-98, 23, 5},   {-99, 23, 5},   {-100, 23, 5},  {-101, 23, 5},  {-102, 23, 5},  {-103, 23, 5},  {-104, 23, 5},
    {-105, 23, 5},  {-106, 23, 5},  {-107, 23, 5},  {-108, 23, 5},  {-110, 23, 5},  {-111, 23, 5},  {-75, 24, 5},   {-76, 24, 5},   {-77, 24, 5},
    {-78, 24, 5},   {-79, 24, 5},   {-80, 24, 5},   {-81, 24, 5},   {-82, 24, 5},   {-83, 24, 5},   {-98, 24, 5},   {-99, 24, 5},   {-100, 24, 5},
    {-101, 24, 5},  {-102, 24, 5},  {-103, 24, 5},  {-104, 24, 5},  {-105, 24, 5},  {-106, 24, 5},  {-107, 24, 5},  {-108, 24, 5},  {-109, 24, 5},
    {-110, 24, 5},  {-111, 24, 5},  {-112, 24, 5},  {-113, 24, 5},  {-116, 24, 5},  {-77, 25, 5},   {-78, 25, 5},   {-79, 25, 5},   {-80, 25, 5},
    {-81, 25, 5},   {-82, 25, 5},   {-98, 25, 5},   {-99, 25, 5},   {-100, 25, 5},  {-101, 25, 5},  {-102, 25, 5},  {-103, 25, 5},  {-104, 25, 5},
    {-105, 25, 5},  {-106, 25, 5},  {-107, 25, 5},  {-108, 25, 5},  {-109, 25, 5},  {-110, 25, 5},  {-111, 25, 5},  {-112, 25, 5},  {-113, 25, 5},
    {-77, 26, 5},   {-78, 26, 5},   {-79, 26, 5},   {-80, 26, 5},   {-81, 26, 5},   {-82, 26, 5},   {-83, 26, 5},   {-98, 26, 5},   {-99, 26, 5},
    {-100, 26, 5},  {-101, 26, 5},  {-102, 26, 5},  {-103, 26, 5},  {-104, 26, 5},  {-105, 26, 5},  {-106, 26, 5},  {-107, 26, 5},  {-108, 26, 5},
    {-109, 26, 5},  {-110, 26, 5},  {-112, 26, 5},  {-113, 26, 5},  {-114, 26, 5},  {-115, 26, 5},  {-78, 27, 5},   {-79, 27, 5},   {-81, 27, 5},
    {-82, 27, 5},   {-83, 27, 5},   {-97, 27, 5},   {-98, 27, 5},   {-99, 27, 5},   {-100, 27, 5},  {-101, 27, 5},  {-102, 27, 5},  {-103, 27, 5},
    {-104, 27, 5},  {-105, 27, 5},  {-106, 27, 5},  {-107, 27, 5},  {-108, 27, 5},  {-109, 27, 5},  {-110, 27, 5},  {-111, 27, 5},  {-112, 27, 5},
    {-113, 27, 5},  {-114, 27, 5},  {-115, 27, 5},  {-116, 27, 5},  {-81, 28, 5},   {-82, 28, 5},   {-83, 28, 5},   {-90, 28, 5},   {-96, 28, 5},
    {-97, 28, 5},   {-98, 28, 5},   {-99, 28, 5},   {-100, 28, 5},  {-101, 28, 5},  {-102, 28, 5},  {-103, 28, 5},  {-104, 28, 5},  {-105, 28, 5},
    {-106, 28, 5},  {-107, 28, 5},  {-108, 28, 5},  {-109, 28, 5},  {-110, 28, 5},  {-111, 28, 5},  {-112, 28, 5},  {-113, 28, 5},  {-114, 28, 5},
    {-115, 28, 5},  {-116, 28, 5},  {-119, 28, 5},  {-81, 29, 5},   {-82, 29, 5},   {-83, 29, 5},   {-84, 29, 5},   {-85, 29, 5},   {-86, 29, 5},
    {-89, 29, 5},   {-90, 29, 5},   {-91, 29, 5},   {-92, 29, 5},   {-93, 29, 5},   {-94, 29, 5},   {-95, 29, 5},   {-96, 29, 5},   {-97, 29, 5},
    {-98, 29, 5},   {-99, 29, 5},   {-100, 29, 5},  {-101, 29, 5},  {-102, 29, 5},  {-103, 29, 5},  {-104, 29, 5},  {-105, 29, 5},  {-106, 29, 5},
    {-107, 29, 5},  {-108, 29, 5},  {-109, 29, 5},  {-110, 29, 5},  {-111, 29, 5},  {-112, 29, 5},  {-113, 29, 5},  {-114, 29, 5},  {-115, 29, 5},
    {-116, 29, 5},  {-119, 29, 5},  {-82, 30, 5},   {-83, 30, 5},   {-84, 30, 5},   {-85, 30, 5},   {-86, 30, 5},   {-87, 30, 5},   {-88, 30, 5},
    {-89, 30, 5},   {-90, 30, 5},   {-91, 30, 5},   {-92, 30, 5},   {-93, 30, 5},   {-94, 30, 5},   {-95, 30, 5},   {-96, 30, 5},   {-97, 30, 5},
    {-98, 30, 5},   {-99, 30, 5},   {-100, 30, 5},  {-101, 30, 5},  {-102, 30, 5},  {-103, 30, 5},  {-104, 30, 5},  {-105, 30, 5},  {-106, 30, 5},
    {-107, 30, 5},  {-108, 30, 5},  {-109, 30, 5},  {-110, 30, 5},  {-111, 30, 5},  {-112, 30, 5},  {-113, 30, 5},  {-114, 30, 5},  {-115, 30, 5},
    {-116, 30, 5},  {-117, 30, 5},  {-81, 31, 5},   {-82, 31, 5},   {-83, 31, 5},   {-84, 31, 5},   {-85, 31, 5},   {-86, 31, 5},   {-87, 31, 5},
    {-88, 31, 5},   {-89, 31, 5},   {-90, 31, 5},   {-91, 31, 5},   {-92, 31, 5},   {-93, 31, 5},   {-94, 31, 5},   {-95, 31, 5},   {-96, 31, 5},
    {-97, 31, 5},   {-98, 31, 5},   {-99, 31, 5},   {-100, 31, 5},  {-101, 31, 5},  {-102, 31, 5},  {-103, 31, 5},  {-104, 31, 5},  {-105, 31, 5},
    {-106, 31, 5},  {-107, 31, 5},  {-108, 31, 5},  {-109, 31, 5},  {-110, 31, 5},  {-111, 31, 5},  {-112, 31, 5},  {-113, 31, 5},  {-114, 31, 5},
    {-115, 31, 5},  {-116, 31, 5},  {-117, 31, 5},  {-65, 32, 5},   {-80, 32, 5},   {-81, 32, 5},   {-82, 32, 5},   {-83, 32, 5},   {-84, 32, 5},
    {-85, 32, 5},   {-86, 32, 5},   {-87, 32, 5},   {-88, 32, 5},   {-89, 32, 5},   {-90, 32, 5},   {-91, 32, 5},   {-92, 32, 5},   {-93, 32, 5},
    {-94, 32, 5},   {-95, 32, 5},   {-96, 32, 5},   {-97, 32, 5},   {-98, 32, 5},   {-99, 32, 5},   {-100, 32, 5},  {-101, 32, 5},  {-102, 32, 5},
    {-103, 32, 5},  {-104, 32, 5},  {-105, 32, 5},  {-106, 32, 5},  {-107, 32, 5},  {-108, 32, 5},  {-109, 32, 5},  {-110, 32, 5},  {-111, 32, 5},
    {-112, 32, 5},  {-113, 32, 5},  {-114, 32, 5},  {-115, 32, 5},  {-116, 32, 5},  {-117, 32, 5},  {-118, 32, 5},  {-119, 32, 5},  {-78, 33, 5},
    {-79, 33, 5},   {-80, 33, 5},   {-81, 33, 5},   {-82, 33, 5},   {-83, 33, 5},   {-84, 33, 5},   {-85, 33, 5},   {-86, 33, 5},   {-87, 33, 5},
    {-88, 33, 5},   {-89, 33, 5},   {-90, 33, 5},   {-91, 33, 5},   {-92, 33, 5},   {-93, 33, 5},   {-94, 33, 5},   {-95, 33, 5},   {-96, 33, 5},
    {-97, 33, 5},   {-98, 33, 5},   {-99, 33, 5},   {-100, 33, 5},  {-101, 33, 5},  {-102, 33, 5},  {-103, 33, 5},  {-104, 33, 5},  {-105, 33, 5},
    {-106, 33, 5},  {-107, 33, 5},  {-108, 33, 5},  {-109, 33, 5},  {-110, 33, 5},  {-111, 33, 5},  {-112, 33, 5},  {-113, 33, 5},  {-114, 33, 5},
    {-115, 33, 5},  {-116, 33, 5},  {-117, 33, 5},  {-118, 33, 5},  {-119, 33, 5},  {-120, 33, 5},  {-121, 33, 5},  {-77, 34, 5},   {-78, 34, 5},
    {-79, 34, 5},   {-80, 34, 5},   {-81, 34, 5},   {-82, 34, 5},   {-83, 34, 5},   {-84, 34, 5},   {-85, 34, 5},   {-86, 34, 5},   {-87, 34, 5},
    {-88, 34, 5},   {-89, 34, 5},   {-90, 34, 5},   {-91, 34, 5},   {-92, 34, 5},   {-93, 34, 5},   {-94, 34, 5},   {-95, 34, 5},   {-96, 34, 5},
    {-97, 34, 5},   {-98, 34, 5},   {-99, 34, 5},   {-100, 34, 5},  {-101, 34, 5},  {-102, 34, 5},  {-103, 34, 5},  {-104, 34, 5},  {-105, 34, 5},
    {-106, 34, 5},  {-107, 34, 5},  {-108, 34, 5},  {-109, 34, 5},  {-110, 34, 5},  {-111, 34, 5},  {-112, 34, 5},  {-113, 34, 5},  {-114, 34, 5},
    {-115, 34, 5},  {-116, 34, 5},  {-117, 34, 5},  {-118, 34, 5},  {-119, 34, 5},  {-120, 34, 5},  {-121, 34, 5},  {-76, 35, 5},   {-77, 35, 5},
    {-78, 35, 5},   {-79, 35, 5},   {-80, 35, 5},   {-81, 35, 5},   {-82, 35, 5},   {-83, 35, 5},   {-84, 35, 5},   {-85, 35, 5},   {-86, 35, 5},
    {-87, 35, 5},   {-88, 35, 5},   {-89, 35, 5},   {-90, 35, 5},   {-91, 35, 5},   {-92, 35, 5},   {-93, 35, 5},   {-94, 35, 5},   {-95, 35, 5},
    {-96, 35, 5},   {-97, 35, 5},   {-98, 35, 5},   {-99, 35, 5},   {-100, 35, 5},  {-101, 35, 5},  {-102, 35, 5},  {-103, 35, 5},  {-104, 35, 5},
    {-105, 35, 5},  {-106, 35, 5},  {-107, 35, 5},  {-108, 35, 5},  {-109, 35, 5},  {-110, 35, 5},  {-111, 35, 5},  {-112, 35, 5},  {-113, 35, 5},
    {-114, 35, 5},  {-115, 35, 5},  {-116, 35, 5},  {-117, 35, 5},  {-118, 35, 5},  {-119, 35, 5},  {-120, 35, 5},  {-121, 35, 5},  {-122, 35, 5},
    {-76, 36, 5},   {-77, 36, 5},   {-78, 36, 5},   {-79, 36, 5},   {-80, 36, 5},   {-81, 36, 5},   {-82, 36, 5},   {-83, 36, 5},   {-84, 36, 5},
    {-85, 36, 5},   {-86, 36, 5},   {-87, 36, 5},   {-88, 36, 5},   {-89, 36, 5},   {-90, 36, 5},   {-91, 36, 5},   {-92, 36, 5},   {-93, 36, 5},
    {-94, 36, 5},   {-95, 36, 5},   {-96, 36, 5},   {-97, 36, 5},   {-98, 36, 5},   {-99, 36, 5},   {-100, 36, 5},  {-101, 36, 5},  {-102, 36, 5},
    {-103, 36, 5},  {-104, 36, 5},  {-105, 36, 5},  {-106, 36, 5},  {-107, 36, 5},  {-108, 36, 5},  {-109, 36, 5},  {-110, 36, 5},  {-111, 36, 5},
    {-112, 36, 5},  {-113, 36, 5},  {-114, 36, 5},  {-115, 36, 5},  {-116, 36, 5},  {-117, 36, 5},  {-118, 36, 5},  {-119, 36, 5},  {-120, 36, 5},
    {-121, 36, 5},  {-122, 36, 5},  {-123, 36, 5},  {-76, 37, 5},   {-77, 37, 5},   {-78, 37, 5},   {-79, 37, 5},   {-80, 37, 5},   {-81, 37, 5},
    {-82, 37, 5},   {-83, 37, 5},   {-84, 37, 5},   {-85, 37, 5},   {-86, 37, 5},   {-87, 37, 5},   {-88, 37, 5},   {-89, 37, 5},   {-90, 37, 5},
    {-91, 37, 5},   {-92, 37, 5},   {-93, 37, 5},   {-94, 37, 5},   {-95, 37, 5},   {-96, 37, 5},   {-97, 37, 5},   {-98, 37, 5},   {-99, 37, 5},
    {-100, 37, 5},  {-101, 37, 5},  {-102, 37, 5},  {-103, 37, 5},  {-104, 37, 5},  {-105, 37, 5},  {-106, 37, 5},  {-107, 37, 5},  {-108, 37, 5},
    {-109, 37, 5},  {-110, 37, 5},  {-111, 37, 5},  {-112, 37, 5},  {-113, 37, 5},  {-114, 37, 5},  {-115, 37, 5},  {-116, 37, 5},  {-117, 37, 5},
    {-118, 37, 5},  {-119, 37, 5},  {-120, 37, 5},  {-121, 37, 5},  {-122, 37, 5},  {-123, 37, 5},  {-124, 37, 5},  {-75, 38, 5},   {-76, 38, 5},
    {-77, 38, 5},   {-78, 38, 5},   {-79, 38, 5},   {-80, 38, 5},   {-81, 38, 5},   {-82, 38, 5},   {-83, 38, 5},   {-84, 38, 5},   {-85, 38, 5},
    {-86, 38, 5},   {-87, 38, 5},   {-88, 38, 5},   {-89, 38, 5},   {-90, 38, 5},   {-91, 38, 5},   {-92, 38, 5},   {-93, 38, 5},   {-94, 38, 5},
    {-95, 38, 5},   {-96, 38, 5},   {-97, 38, 5},   {-98, 38, 5},   {-99, 38, 5},   {-100, 38, 5},  {-101, 38, 5},  {-102, 38, 5},  {-103, 38, 5},
    {-104, 38, 5},  {-105, 38, 5},  {-106, 38, 5},  {-107, 38, 5},  {-108, 38, 5},  {-109, 38, 5},  {-110, 38, 5},  {-111, 38, 5},  {-112, 38, 5},
    {-113, 38, 5},  {-114, 38, 5},  {-115, 38, 5},  {-116, 38, 5},  {-117, 38, 5},  {-118, 38, 5},  {-119, 38, 5},  {-120, 38, 5},  {-121, 38, 5},
    {-122, 38, 5},  {-123, 38, 5},  {-124, 38, 5},  {-75, 39, 5},   {-76, 39, 5},   {-77, 39, 5},   {-78, 39, 5},   {-79, 39, 5},   {-80, 39, 5},
    {-81, 39, 5},   {-82, 39, 5},   {-83, 39, 5},   {-84, 39, 5},   {-85, 39, 5},   {-86, 39, 5},   {-87, 39, 5},   {-88, 39, 5},   {-89, 39, 5},
    {-90, 39, 5},   {-91, 39, 5},   {-92, 39, 5},   {-93, 39, 5},   {-94, 39, 5},   {-95, 39, 5},   {-96, 39, 5},   {-97, 39, 5},   {-98, 39, 5},
    {-99, 39, 5},   {-100, 39, 5},  {-101, 39, 5},  {-102, 39, 5},  {-103, 39, 5},  {-104, 39, 5},  {-105, 39, 5},  {-106, 39, 5},  {-107, 39, 5},
    {-108, 39, 5},  {-109, 39, 5},  {-110, 39, 5},  {-111, 39, 5},  {-112, 39, 5},  {-113, 39, 5},  {-114, 39, 5},  {-115, 39, 5},  {-116, 39, 5},
    {-117, 39, 5},  {-118, 39, 5},  {-119, 39, 5},  {-120, 39, 5},  {-121, 39, 5},  {-122, 39, 5},  {-123, 39, 5},  {-124, 39, 5},  {-125, 39, 5},
    {-73, 40, 5},   {-74, 40, 5},   {-75, 40, 5},   {-76, 40, 5},   {-77, 40, 5},   {-78, 40, 5},   {-79, 40, 5},   {-80, 40, 5},   {-81, 40, 5},
    {-82, 40, 5},   {-83, 40, 5},   {-84, 40, 5},   {-85, 40, 5},   {-86, 40, 5},   {-87, 40, 5},   {-88, 40, 5},   {-89, 40, 5},   {-90, 40, 5},
    {-91, 40, 5},   {-92, 40, 5},   {-93, 40, 5},   {-94, 40, 5},   {-95, 40, 5},   {-96, 40, 5},   {-97, 40, 5},   {-98, 40, 5},   {-99, 40, 5},
    {-100, 40, 5},  {-101, 40, 5},  {-102, 40, 5},  {-103, 40, 5},  {-104, 40, 5},  {-105, 40, 5},  {-106, 40, 5},  {-107, 40, 5},  {-108, 40, 5},
    {-109, 40, 5},  {-110, 40, 5},  {-111, 40, 5},  {-112, 40, 5},  {-113, 40, 5},  {-114, 40, 5},  {-115, 40, 5},  {-116, 40, 5},  {-117, 40, 5},
    {-118, 40, 5},  {-119, 40, 5},  {-120, 40, 5},  {-121, 40, 5},  {-122, 40, 5},  {-123, 40, 5},  {-124, 40, 5},  {-125, 40, 5},  {-70, 41, 5},
    {-71, 41, 5},   {-72, 41, 5},   {-73, 41, 5},   {-74, 41, 5},   {-75, 41, 5},   {-76, 41, 5},   {-77, 41, 5},   {-78, 41, 5},   {-79, 41, 5},
    {-80, 41, 5},   {-81, 41, 5},   {-82, 41, 5},   {-83, 41, 5},   {-84, 41, 5},   {-85, 41, 5},   {-86, 41, 5},   {-87, 41, 5},   {-88, 41, 5},
    {-89, 41, 5},   {-90, 41, 5},   {-91, 41, 5},   {-92, 41, 5},   {-93, 41, 5},   {-94, 41, 5},   {-95, 41, 5},   {-96, 41, 5},   {-97, 41, 5},
    {-98, 41, 5},   {-99, 41, 5},   {-100, 41, 5},  {-101, 41, 5},  {-102, 41, 5},  {-103, 41, 5},  {-104, 41, 5},  {-105, 41, 5},  {-106, 41, 5},
    {-107, 41, 5},  {-108, 41, 5},  {-109, 41, 5},  {-110, 41, 5},  {-111, 41, 5},  {-112, 41, 5},  {-113, 41, 5},  {-114, 41, 5},  {-115, 41, 5},
    {-116, 41, 5},  {-117, 41, 5},  {-118, 41, 5},  {-119, 41, 5},  {-120, 41, 5},  {-121, 41, 5},  {-122, 41, 5},  {-123, 41, 5},  {-124, 41, 5},
    {-125, 41, 5},  {-71, 42, 5},   {-72, 42, 5},   {-73, 42, 5},   {-74, 42, 5},   {-75, 42, 5},   {-76, 42, 5},   {-77, 42, 5},   {-78, 42, 5},
    {-79, 42, 5},   {-80, 42, 5},   {-81, 42, 5},   {-82, 42, 5},   {-83, 42, 5},   {-84, 42, 5},   {-85, 42, 5},   {-86, 42, 5},   {-87, 42, 5},
    {-88, 42, 5},   {-89, 42, 5},   {-90, 42, 5},   {-91, 42, 5},   {-92, 42, 5},   {-93, 42, 5},   {-94, 42, 5},   {-95, 42, 5},   {-96, 42, 5},
    {-97, 42, 5},   {-98, 42, 5},   {-99, 42, 5},   {-100, 42, 5},  {-101, 42, 5},  {-102, 42, 5},  {-103, 42, 5},  {-104, 42, 5},  {-105, 42, 5},
    {-106, 42, 5},  {-107, 42, 5},  {-108, 42, 5},  {-109, 42, 5},  {-110, 42, 5},  {-111, 42, 5},  {-112, 42, 5},  {-113, 42, 5},  {-114, 42, 5},
    {-115, 42, 5},  {-116, 42, 5},  {-117, 42, 5},  {-118, 42, 5},  {-119, 42, 5},  {-120, 42, 5},  {-121, 42, 5},  {-122, 42, 5},  {-123, 42, 5},
    {-124, 42, 5},  {-125, 42, 5},  {-60, 43, 5},   {-61, 43, 5},   {-65, 43, 5},   {-66, 43, 5},   {-67, 43, 5},   {-69, 43, 5},   {-70, 43, 5},
    {-71, 43, 5},   {-72, 43, 5},   {-73, 43, 5},   {-74, 43, 5},   {-75, 43, 5},   {-76, 43, 5},   {-77, 43, 5},   {-78, 43, 5},   {-79, 43, 5},
    {-80, 43, 5},   {-81, 43, 5},   {-82, 43, 5},   {-83, 43, 5},   {-84, 43, 5},   {-85, 43, 5},   {-86, 43, 5},   {-87, 43, 5},   {-88, 43, 5},
    {-89, 43, 5},   {-90, 43, 5},   {-91, 43, 5},   {-92, 43, 5},   {-93, 43, 5},   {-94, 43, 5},   {-95, 43, 5},   {-96, 43, 5},   {-97, 43, 5},
    {-98, 43, 5},   {-99, 43, 5},   {-100, 43, 5},  {-101, 43, 5},  {-102, 43, 5},  {-103, 43, 5},  {-104, 43, 5},  {-105, 43, 5},  {-106, 43, 5},
    {-107, 43, 5},  {-108, 43, 5},  {-109, 43, 5},  {-110, 43, 5},  {-111, 43, 5},  {-112, 43, 5},  {-113, 43, 5},  {-114, 43, 5},  {-115, 43, 5},
    {-116, 43, 5},  {-117, 43, 5},  {-118, 43, 5},  {-119, 43, 5},  {-120, 43, 5},  {-121, 43, 5},  {-122, 43, 5},  {-123, 43, 5},  {-124, 43, 5},
    {-125, 43, 5},  {-60, 44, 5},   {-62, 44, 5},   {-63, 44, 5},   {-64, 44, 5},   {-65, 44, 5},   {-66, 44, 5},   {-67, 44, 5},   {-68, 44, 5},
    {-69, 44, 5},   {-70, 44, 5},   {-71, 44, 5},   {-72, 44, 5},   {-73, 44, 5},   {-74, 44, 5},   {-75, 44, 5},   {-76, 44, 5},   {-77, 44, 5},
    {-78, 44, 5},   {-79, 44, 5},   {-80, 44, 5},   {-81, 44, 5},   {-82, 44, 5},   {-83, 44, 5},   {-84, 44, 5},   {-85, 44, 5},   {-86, 44, 5},
    {-87, 44, 5},   {-88, 44, 5},   {-89, 44, 5},   {-90, 44, 5},   {-91, 44, 5},   {-92, 44, 5},   {-93, 44, 5},   {-94, 44, 5},   {-95, 44, 5},
    {-96, 44, 5},   {-97, 44, 5},   {-98, 44, 5},   {-99, 44, 5},   {-100, 44, 5},  {-101, 44, 5},  {-102, 44, 5},  {-103, 44, 5},  {-104, 44, 5},
    {-105, 44, 5},  {-106, 44, 5},  {-107, 44, 5},  {-108, 44, 5},  {-109, 44, 5},  {-110, 44, 5},  {-111, 44, 5},  {-112, 44, 5},  {-113, 44, 5},
    {-114, 44, 5},  {-115, 44, 5},  {-116, 44, 5},  {-117, 44, 5},  {-118, 44, 5},  {-119, 44, 5},  {-120, 44, 5},  {-121, 44, 5},  {-122, 44, 5},
    {-123, 44, 5},  {-124, 44, 5},  {-125, 44, 5},  {-60, 45, 5},   {-61, 45, 5},   {-62, 45, 5},   {-63, 45, 5},   {-64, 45, 5},   {-65, 45, 5},
    {-66, 45, 5},   {-67, 45, 5},   {-68, 45, 5},   {-69, 45, 5},   {-70, 45, 5},   {-71, 45, 5},   {-72, 45, 5},   {-73, 45, 5},   {-74, 45, 5},
    {-75, 45, 5},   {-76, 45, 5},   {-77, 45, 5},   {-78, 45, 5},   {-79, 45, 5},   {-80, 45, 5},   {-81, 45, 5},   {-82, 45, 5},   {-83, 45, 5},
    {-84, 45, 5},   {-85, 45, 5},   {-86, 45, 5},   {-87, 45, 5},   {-88, 45, 5},   {-89, 45, 5},   {-90, 45, 5},   {-91, 45, 5},   {-92, 45, 5},
    {-93, 45, 5},   {-94, 45, 5},   {-95, 45, 5},   {-96, 45, 5},   {-97, 45, 5},   {-98, 45, 5},   {-99, 45, 5},   {-100, 45, 5},  {-101, 45, 5},
    {-102, 45, 5},  {-103, 45, 5},  {-104, 45, 5},  {-105, 45, 5},  {-106, 45, 5},  {-107, 45, 5},  {-108, 45, 5},  {-109, 45, 5},  {-110, 45, 5},
    {-111, 45, 5},  {-112, 45, 5},  {-113, 45, 5},  {-114, 45, 5},  {-115, 45, 5},  {-116, 45, 5},  {-117, 45, 5},  {-118, 45, 5},  {-119, 45, 5},
    {-120, 45, 5},  {-121, 45, 5},  {-122, 45, 5},  {-123, 45, 5},  {-124, 45, 5},  {-125, 45, 5},  {-53, 46, 5},   {-54, 46, 5},   {-55, 46, 5},
    {-56, 46, 5},   {-57, 46, 5},   {-60, 46, 5},   {-61, 46, 5},   {-62, 46, 5},   {-63, 46, 5},   {-64, 46, 5},   {-65, 46, 5},   {-66, 46, 5},
    {-67, 46, 5},   {-68, 46, 5},   {-69, 46, 5},   {-70, 46, 5},   {-71, 46, 5},   {-72, 46, 5},   {-73, 46, 5},   {-74, 46, 5},   {-75, 46, 5},
    {-76, 46, 5},   {-77, 46, 5},   {-78, 46, 5},   {-79, 46, 5},   {-80, 46, 5},   {-81, 46, 5},   {-82, 46, 5},   {-83, 46, 5},   {-84, 46, 5},
    {-85, 46, 5},   {-86, 46, 5},   {-87, 46, 5},   {-88, 46, 5},   {-89, 46, 5},   {-90, 46, 5},   {-91, 46, 5},   {-92, 46, 5},   {-93, 46, 5},
    {-94, 46, 5},   {-95, 46, 5},   {-96, 46, 5},   {-97, 46, 5},   {-98, 46, 5},   {-99, 46, 5},   {-100, 46, 5},  {-101, 46, 5},  {-102, 46, 5},
    {-103, 46, 5},  {-104, 46, 5},  {-105, 46, 5},  {-106, 46, 5},  {-107, 46, 5},  {-108, 46, 5},  {-109, 46, 5},  {-110, 46, 5},  {-111, 46, 5},
    {-112, 46, 5},  {-113, 46, 5},  {-114, 46, 5},  {-115, 46, 5},  {-116, 46, 5},  {-117, 46, 5},  {-118, 46, 5},  {-119, 46, 5},  {-120, 46, 5},
    {-121, 46, 5},  {-122, 46, 5},  {-123, 46, 5},  {-124, 46, 5},  {-125, 46, 5},  {-53, 47, 5},   {-54, 47, 5},   {-55, 47, 5},   {-56, 47, 5},
    {-57, 47, 5},   {-58, 47, 5},   {-59, 47, 5},   {-60, 47, 5},   {-61, 47, 5},   {-62, 47, 5},   {-63, 47, 5},   {-64, 47, 5},   {-65, 47, 5},
    {-66, 47, 5},   {-67, 47, 5},   {-68, 47, 5},   {-69, 47, 5},   {-70, 47, 5},   {-71, 47, 5},   {-72, 47, 5},   {-73, 47, 5},   {-74, 47, 5},
    {-75, 47, 5},   {-76, 47, 5},   {-77, 47, 5},   {-78, 47, 5},   {-79, 47, 5},   {-80, 47, 5},   {-81, 47, 5},   {-82, 47, 5},   {-83, 47, 5},
    {-84, 47, 5},   {-85, 47, 5},   {-86, 47, 5},   {-88, 47, 5},   {-89, 47, 5},   {-90, 47, 5},   {-91, 47, 5},   {-92, 47, 5},   {-93, 47, 5},
    {-94, 47, 5},   {-95, 47, 5},   {-96, 47, 5},   {-97, 47, 5},   {-98, 47, 5},   {-99, 47, 5},   {-100, 47, 5},  {-101, 47, 5},  {-102, 47, 5},
    {-103, 47, 5},  {-104, 47, 5},  {-105, 47, 5},  {-106, 47, 5},  {-107, 47, 5},  {-108, 47, 5},  {-109, 47, 5},  {-110, 47, 5},  {-111, 47, 5},
    {-112, 47, 5},  {-113, 47, 5},  {-114, 47, 5},  {-115, 47, 5},  {-116, 47, 5},  {-117, 47, 5},  {-118, 47, 5},  {-119, 47, 5},  {-120, 47, 5},
    {-121, 47, 5},  {-122, 47, 5},  {-123, 47, 5},  {-124, 47, 5},  {-125, 47, 5},  {-53, 48, 5},   {-54, 48, 5},   {-55, 48, 5},   {-56, 48, 5},
    {-57, 48, 5},   {-58, 48, 5},   {-59, 48, 5},   {-60, 48, 5},   {-65, 48, 5},   {-66, 48, 5},   {-67, 48, 5},   {-68, 48, 5},   {-69, 48, 5},
    {-70, 48, 5},   {-71, 48, 5},   {-72, 48, 5},   {-73, 48, 5},   {-74, 48, 5},   {-75, 48, 5},   {-76, 48, 5},   {-77, 48, 5},   {-78, 48, 5},
    {-79, 48, 5},   {-80, 48, 5},   {-81, 48, 5},   {-82, 48, 5},   {-83, 48, 5},   {-84, 48, 5},   {-85, 48, 5},   {-86, 48, 5},   {-87, 48, 5},
    {-88, 48, 5},   {-89, 48, 5},   {-90, 48, 5},   {-91, 48, 5},   {-92, 48, 5},   {-93, 48, 5},   {-94, 48, 5},   {-95, 48, 5},   {-96, 48, 5},
    {-97, 48, 5},   {-98, 48, 5},   {-99, 48, 5},   {-100, 48, 5},  {-101, 48, 5},  {-102, 48, 5},  {-103, 48, 5},  {-104, 48, 5},  {-105, 48, 5},
    {-106, 48, 5},  {-107, 48, 5},  {-108, 48, 5},  {-109, 48, 5},  {-110, 48, 5},  {-111, 48, 5},  {-112, 48, 5},  {-113, 48, 5},  {-114, 48, 5},
    {-115, 48, 5},  {-116, 48, 5},  {-117, 48, 5},  {-118, 48, 5},  {-119, 48, 5},  {-120, 48, 5},  {-121, 48, 5},  {-122, 48, 5},  {-123, 48, 5},
    {-124, 48, 5},  {-125, 48, 5},  {-126, 48, 5},  {-54, 49, 5},   {-55, 49, 5},   {-56, 49, 5},   {-57, 49, 5},   {-58, 49, 5},   {-59, 49, 5},
    {-62, 49, 5},   {-63, 49, 5},   {-64, 49, 5},   {-65, 49, 5},   {-66, 49, 5},   {-67, 49, 5},   {-68, 49, 5},   {-69, 49, 5},   {-70, 49, 5},
    {-71, 49, 5},   {-72, 49, 5},   {-73, 49, 5},   {-74, 49, 5},   {-75, 49, 5},   {-76, 49, 5},   {-77, 49, 5},   {-78, 49, 5},   {-79, 49, 5},
    {-80, 49, 5},   {-81, 49, 5},   {-82, 49, 5},   {-83, 49, 5},   {-84, 49, 5},   {-85, 49, 5},   {-86, 49, 5},   {-87, 49, 5},   {-88, 49, 5},
    {-89, 49, 5},   {-90, 49, 5},   {-91, 49, 5},   {-92, 49, 5},   {-93, 49, 5},   {-94, 49, 5},   {-95, 49, 5},   {-96, 49, 5},   {-97, 49, 5},
    {-98, 49, 5},   {-99, 49, 5},   {-100, 49, 5},  {-101, 49, 5},  {-102, 49, 5},  {-103, 49, 5},  {-104, 49, 5},  {-105, 49, 5},  {-106, 49, 5},
    {-107, 49, 5},  {-108, 49, 5},  {-109, 49, 5},  {-110, 49, 5},  {-111, 49, 5},  {-112, 49, 5},  {-113, 49, 5},  {-114, 49, 5},  {-115, 49, 5},
    {-116, 49, 5},  {-117, 49, 5},  {-118, 49, 5},  {-119, 49, 5},  {-120, 49, 5},  {-121, 49, 5},  {-122, 49, 5},  {-123, 49, 5},  {-124, 49, 5},
    {-125, 49, 5},  {-126, 49, 5},  {-127, 49, 5},  {-128, 49, 5},  {-56, 50, 5},   {-57, 50, 5},   {-58, 50, 5},   {-59, 50, 5},   {-60, 50, 5},
    {-61, 50, 5},   {-62, 50, 5},   {-63, 50, 5},   {-64, 50, 5},   {-65, 50, 5},   {-66, 50, 5},   {-67, 50, 5},   {-68, 50, 5},   {-69, 50, 5},
    {-70, 50, 5},   {-71, 50, 5},   {-72, 50, 5},   {-73, 50, 5},   {-74, 50, 5},   {-75, 50, 5},   {-76, 50, 5},   {-77, 50, 5},   {-78, 50, 5},
    {-79, 50, 5},   {-80, 50, 5},   {-81, 50, 5},   {-82, 50, 5},   {-83, 50, 5},   {-84, 50, 5},   {-85, 50, 5},   {-86, 50, 5},   {-87, 50, 5},
    {-88, 50, 5},   {-89, 50, 5},   {-90, 50, 5},   {-91, 50, 5},   {-92, 50, 5},   {-93, 50, 5},   {-94, 50, 5},   {-95, 50, 5},   {-96, 50, 5},
    {-97, 50, 5},   {-98, 50, 5},   {-99, 50, 5},   {-100, 50, 5},  {-101, 50, 5},  {-102, 50, 5},  {-103, 50, 5},  {-104, 50, 5},  {-105, 50, 5},
    {-106, 50, 5},  {-107, 50, 5},  {-108, 50, 5},  {-109, 50, 5},  {-110, 50, 5},  {-111, 50, 5},  {-112, 50, 5},  {-113, 50, 5},  {-114, 50, 5},
    {-115, 50, 5},  {-116, 50, 5},  {-117, 50, 5},  {-118, 50, 5},  {-119, 50, 5},  {-120, 50, 5},  {-121, 50, 5},  {-122, 50, 5},  {-123, 50, 5},
    {-124, 50, 5},  {-125, 50, 5},  {-126, 50, 5},  {-127, 50, 5},  {-128, 50, 5},  {-129, 50, 5},  {-130, 50, 5},  {177, 51, 5},   {178, 51, 5},
    {179, 51, 5},   {-56, 51, 5},   {-57, 51, 5},   {-58, 51, 5},   {-59, 51, 5},   {-60, 51, 5},   {-61, 51, 5},   {-62, 51, 5},   {-63, 51, 5},
    {-64, 51, 5},   {-65, 51, 5},   {-66, 51, 5},   {-67, 51, 5},   {-68, 51, 5},   {-69, 51, 5},   {-70, 51, 5},   {-71, 51, 5},   {-72, 51, 5},
    {-73, 51, 5},   {-74, 51, 5},   {-75, 51, 5},   {-76, 51, 5},   {-77, 51, 5},   {-78, 51, 5},   {-79, 51, 5},   {-80, 51, 5},   {-81, 51, 5},
    {-82, 51, 5},   {-83, 51, 5},   {-84, 51, 5},   {-85, 51, 5},   {-86, 51, 5},   {-87, 51, 5},   {-88, 51, 5},   {-89, 51, 5},   {-90, 51, 5},
    {-91, 51, 5},   {-92, 51, 5},   {-93, 51, 5},   {-94, 51, 5},   {-95, 51, 5},   {-96, 51, 5},   {-97, 51, 5},   {-98, 51, 5},   {-99, 51, 5},
    {-100, 51, 5},  {-101, 51, 5},  {-102, 51, 5},  {-103, 51, 5},  {-104, 51, 5},  {-105, 51, 5},  {-106, 51, 5},  {-107, 51, 5},  {-108, 51, 5},
    {-109, 51, 5},  {-110, 51, 5},  {-111, 51, 5},  {-112, 51, 5},  {-113, 51, 5},  {-114, 51, 5},  {-115, 51, 5},  {-116, 51, 5},  {-117, 51, 5},
    {-118, 51, 5},  {-119, 51, 5},  {-120, 51, 5},  {-121, 51, 5},  {-122, 51, 5},  {-123, 51, 5},  {-124, 51, 5},  {-125, 51, 5},  {-126, 51, 5},
    {-127, 51, 5},  {-128, 51, 5},  {-129, 51, 5},  {-131, 51, 5},  {-132, 51, 5},  {-176, 51, 5},  {-177, 51, 5},  {-178, 51, 5},  {-179, 51, 5},
    {-180, 51, 5},  {172, 52, 5},   {173, 52, 5},   {174, 52, 5},   {175, 52, 5},   {177, 52, 5},   {178, 52, 5},   {179, 52, 5},   {-56, 52, 5},
    {-57, 52, 5},   {-58, 52, 5},   {-59, 52, 5},   {-60, 52, 5},   {-61, 52, 5},   {-62, 52, 5},   {-63, 52, 5},   {-64, 52, 5},   {-65, 52, 5},
    {-66, 52, 5},   {-67, 52, 5},   {-68, 52, 5},   {-69, 52, 5},   {-70, 52, 5},   {-71, 52, 5},   {-72, 52, 5},   {-73, 52, 5},   {-74, 52, 5},
    {-75, 52, 5},   {-76, 52, 5},   {-77, 52, 5},   {-78, 52, 5},   {-79, 52, 5},   {-80, 52, 5},   {-81, 52, 5},   {-82, 52, 5},   {-83, 52, 5},
    {-84, 52, 5},   {-85, 52, 5},   {-86, 52, 5},   {-87, 52, 5},   {-88, 52, 5},   {-89, 52, 5},   {-90, 52, 5},   {-91, 52, 5},   {-92, 52, 5},
    {-93, 52, 5},   {-94, 52, 5},   {-95, 52, 5},   {-96, 52, 5},   {-97, 52, 5},   {-98, 52, 5},   {-99, 52, 5},   {-100, 52, 5},  {-101, 52, 5},
    {-102, 52, 5},  {-103, 52, 5},  {-104, 52, 5},  {-105, 52, 5},  {-106, 52, 5},  {-107, 52, 5},  {-108, 52, 5},  {-109, 52, 5},  {-110, 52, 5},
    {-111, 52, 5},  {-112, 52, 5},  {-113, 52, 5},  {-114, 52, 5},  {-115, 52, 5},  {-116, 52, 5},  {-117, 52, 5},  {-118, 52, 5},  {-119, 52, 5},
    {-120, 52, 5},  {-121, 52, 5},  {-122, 52, 5},  {-123, 52, 5},  {-124, 52, 5},  {-125, 52, 5},  {-126, 52, 5},  {-127, 52, 5},  {-128, 52, 5},
    {-129, 52, 5},  {-130, 52, 5},  {-131, 52, 5},  {-132, 52, 5},  {-133, 52, 5},  {-169, 52, 5},  {-170, 52, 5},  {-171, 52, 5},  {-172, 52, 5},
    {-173, 52, 5},  {-174, 52, 5},  {-175, 52, 5},  {-176, 52, 5},  {-177, 52, 5},  {172, 53, 5},   {-56, 53, 5},   {-57, 53, 5},   {-58, 53, 5},
    {-59, 53, 5},   {-60, 53, 5},   {-61, 53, 5},   {-62, 53, 5},   {-63, 53, 5},   {-64, 53, 5},   {-65, 53, 5},   {-66, 53, 5},   {-67, 53, 5},
    {-68, 53, 5},   {-69, 53, 5},   {-70, 53, 5},   {-71, 53, 5},   {-72, 53, 5},   {-73, 53, 5},   {-74, 53, 5},   {-75, 53, 5},   {-76, 53, 5},
    {-77, 53, 5},   {-78, 53, 5},   {-79, 53, 5},   {-80, 53, 5},   {-81, 53, 5},   {-82, 53, 5},   {-83, 53, 5},   {-84, 53, 5},   {-85, 53, 5},
    {-86, 53, 5},   {-87, 53, 5},   {-88, 53, 5},   {-89, 53, 5},   {-90, 53, 5},   {-91, 53, 5},   {-92, 53, 5},   {-93, 53, 5},   {-94, 53, 5},
    {-95, 53, 5},   {-96, 53, 5},   {-97, 53, 5},   {-98, 53, 5},   {-99, 53, 5},   {-100, 53, 5},  {-101, 53, 5},  {-102, 53, 5},  {-103, 53, 5},
    {-104, 53, 5},  {-105, 53, 5},  {-106, 53, 5},  {-107, 53, 5},  {-108, 53, 5},  {-109, 53, 5},  {-110, 53, 5},  {-111, 53, 5},  {-112, 53, 5},
    {-113, 53, 5},  {-114, 53, 5},  {-115, 53, 5},  {-116, 53, 5},  {-117, 53, 5},  {-118, 53, 5},  {-119, 53, 5},  {-120, 53, 5},  {-121, 53, 5},
    {-122, 53, 5},  {-123, 53, 5},  {-124, 53, 5},  {-125, 53, 5},  {-126, 53, 5},  {-127, 53, 5},  {-128, 53, 5},  {-129, 53, 5},  {-130, 53, 5},
    {-131, 53, 5},  {-132, 53, 5},  {-133, 53, 5},  {-134, 53, 5},  {-167, 53, 5},  {-168, 53, 5},  {-169, 53, 5},  {-170, 53, 5},  {-57, 54, 5},
    {-58, 54, 5},   {-59, 54, 5},   {-60, 54, 5},   {-61, 54, 5},   {-62, 54, 5},   {-63, 54, 5},   {-64, 54, 5},   {-65, 54, 5},   {-66, 54, 5},
    {-67, 54, 5},   {-68, 54, 5},   {-69, 54, 5},   {-70, 54, 5},   {-71, 54, 5},   {-72, 54, 5},   {-73, 54, 5},   {-74, 54, 5},   {-75, 54, 5},
    {-76, 54, 5},   {-77, 54, 5},   {-78, 54, 5},   {-79, 54, 5},   {-80, 54, 5},   {-81, 54, 5},   {-82, 54, 5},   {-83, 54, 5},   {-84, 54, 5},
    {-85, 54, 5},   {-86, 54, 5},   {-87, 54, 5},   {-88, 54, 5},   {-89, 54, 5},   {-90, 54, 5},   {-91, 54, 5},   {-92, 54, 5},   {-93, 54, 5},
    {-94, 54, 5},   {-95, 54, 5},   {-96, 54, 5},   {-97, 54, 5},   {-98, 54, 5},   {-99, 54, 5},   {-100, 54, 5},  {-101, 54, 5},  {-102, 54, 5},
    {-103, 54, 5},  {-104, 54, 5},  {-105, 54, 5},  {-106, 54, 5},  {-107, 54, 5},  {-108, 54, 5},  {-109, 54, 5},  {-110, 54, 5},  {-111, 54, 5},
    {-112, 54, 5},  {-113, 54, 5},  {-114, 54, 5},  {-115, 54, 5},  {-116, 54, 5},  {-117, 54, 5},  {-118, 54, 5},  {-119, 54, 5},  {-120, 54, 5},
    {-121, 54, 5},  {-122, 54, 5},  {-123, 54, 5},  {-124, 54, 5},  {-125, 54, 5},  {-126, 54, 5},  {-127, 54, 5},  {-128, 54, 5},  {-129, 54, 5},
    {-130, 54, 5},  {-131, 54, 5},  {-132, 54, 5},  {-133, 54, 5},  {-134, 54, 5},  {-160, 54, 5},  {-161, 54, 5},  {-162, 54, 5},  {-163, 54, 5},
    {-164, 54, 5},  {-165, 54, 5},  {-166, 54, 5},  {-167, 54, 5},  {-50, 0, 6},    {-51, 0, 6},    {-52, 0, 6},    {-53, 0, 6},    {-54, 0, 6},
    {-55, 0, 6},    {-56, 0, 6},    {-57, 0, 6},    {-58, 0, 6},    {-59, 0, 6},    {-60, 0, 6},    {-61, 0, 6},    {-62, 0, 6},    {-63, 0, 6},
    {-64, 0, 6},    {-65, 0, 6},    {-66, 0, 6},    {-67, 0, 6},    {-68, 0, 6},    {-69, 0, 6},    {-70, 0, 6},    {-71, 0, 6},    {-72, 0, 6},
    {-73, 0, 6},    {-74, 0, 6},    {-75, 0, 6},    {-76, 0, 6},    {-77, 0, 6},    {-78, 0, 6},    {-79, 0, 6},    {-80, 0, 6},    {-81, 0, 6},
    {-90, 0, 6},    {-91, 0, 6},    {-92, 0, 6},    {-50, 1, 6},    {-51, 1, 6},    {-52, 1, 6},    {-53, 1, 6},    {-54, 1, 6},    {-55, 1, 6},
    {-56, 1, 6},    {-57, 1, 6},    {-58, 1, 6},    {-59, 1, 6},    {-60, 1, 6},    {-61, 1, 6},    {-62, 1, 6},    {-63, 1, 6},    {-64, 1, 6},
    {-65, 1, 6},    {-66, 1, 6},    {-67, 1, 6},    {-68, 1, 6},    {-69, 1, 6},    {-70, 1, 6},    {-71, 1, 6},    {-72, 1, 6},    {-73, 1, 6},
    {-74, 1, 6},    {-75, 1, 6},    {-76, 1, 6},    {-77, 1, 6},    {-78, 1, 6},    {-79, 1, 6},    {-80, 1, 6},    {-92, 1, 6},    {-51, 2, 6},
    {-52, 2, 6},    {-53, 2, 6},    {-54, 2, 6},    {-55, 2, 6},    {-56, 2, 6},    {-57, 2, 6},    {-58, 2, 6},    {-59, 2, 6},    {-60, 2, 6},
    {-61, 2, 6},    {-62, 2, 6},    {-63, 2, 6},    {-64, 2, 6},    {-65, 2, 6},    {-66, 2, 6},    {-67, 2, 6},    {-68, 2, 6},    {-69, 2, 6},
    {-70, 2, 6},    {-71, 2, 6},    {-72, 2, 6},    {-73, 2, 6},    {-74, 2, 6},    {-75, 2, 6},    {-76, 2, 6},    {-77, 2, 6},    {-78, 2, 6},
    {-79, 2, 6},    {-51, 3, 6},    {-52, 3, 6},    {-53, 3, 6},    {-54, 3, 6},    {-55, 3, 6},    {-56, 3, 6},    {-57, 3, 6},    {-58, 3, 6},
    {-59, 3, 6},    {-60, 3, 6},    {-61, 3, 6},    {-62, 3, 6},    {-63, 3, 6},    {-64, 3, 6},    {-65, 3, 6},    {-66, 3, 6},    {-67, 3, 6},
    {-68, 3, 6},    {-69, 3, 6},    {-70, 3, 6},    {-71, 3, 6},    {-72, 3, 6},    {-73, 3, 6},    {-74, 3, 6},    {-75, 3, 6},    {-76, 3, 6},
    {-77, 3, 6},    {-78, 3, 6},    {-79, 3, 6},    {-82, 3, 6},    {-52, 4, 6},    {-53, 4, 6},    {-54, 4, 6},    {-55, 4, 6},    {-56, 4, 6},
    {-57, 4, 6},    {-58, 4, 6},    {-59, 4, 6},    {-60, 4, 6},    {-61, 4, 6},    {-62, 4, 6},    {-63, 4, 6},    {-64, 4, 6},    {-65, 4, 6},
    {-66, 4, 6},    {-67, 4, 6},    {-68, 4, 6},    {-69, 4, 6},    {-70, 4, 6},    {-71, 4, 6},    {-72, 4, 6},    {-73, 4, 6},    {-74, 4, 6},
    {-75, 4, 6},    {-76, 4, 6},    {-77, 4, 6},    {-78, 4, 6},    {-82, 4, 6},    {-53, 5, 6},    {-54, 5, 6},    {-55, 5, 6},    {-56, 5, 6},
    {-57, 5, 6},    {-58, 5, 6},    {-59, 5, 6},    {-60, 5, 6},    {-61, 5, 6},    {-62, 5, 6},    {-63, 5, 6},    {-64, 5, 6},    {-65, 5, 6},
    {-66, 5, 6},    {-67, 5, 6},    {-68, 5, 6},    {-69, 5, 6},    {-70, 5, 6},    {-71, 5, 6},    {-72, 5, 6},    {-73, 5, 6},    {-74, 5, 6},
    {-75, 5, 6},    {-76, 5, 6},    {-77, 5, 6},    {-78, 5, 6},    {-88, 5, 6},    {-56, 6, 6},    {-57, 6, 6},    {-58, 6, 6},    {-59, 6, 6},
    {-60, 6, 6},    {-61, 6, 6},    {-62, 6, 6},    {-63, 6, 6},    {-64, 6, 6},    {-65, 6, 6},    {-66, 6, 6},    {-67, 6, 6},    {-68, 6, 6},
    {-69, 6, 6},    {-70, 6, 6},    {-71, 6, 6},    {-72, 6, 6},    {-73, 6, 6},    {-74, 6, 6},    {-75, 6, 6},    {-76, 6, 6},    {-77, 6, 6},
    {-78, 6, 6},    {-59, 7, 6},    {-60, 7, 6},    {-61, 7, 6},    {-62, 7, 6},    {-63, 7, 6},    {-64, 7, 6},    {-65, 7, 6},    {-66, 7, 6},
    {-67, 7, 6},    {-68, 7, 6},    {-69, 7, 6},    {-70, 7, 6},    {-71, 7, 6},    {-72, 7, 6},    {-73, 7, 6},    {-74, 7, 6},    {-75, 7, 6},
    {-76, 7, 6},    {-77, 7, 6},    {-78, 7, 6},    {-79, 7, 6},    {-80, 7, 6},    {-81, 7, 6},    {-82, 7, 6},    {-83, 7, 6},    {-60, 8, 6},
    {-61, 8, 6},    {-62, 8, 6},    {-63, 8, 6},    {-64, 8, 6},    {-65, 8, 6},    {-66, 8, 6},    {-67, 8, 6},    {-68, 8, 6},    {-69, 8, 6},
    {-70, 8, 6},    {-71, 8, 6},    {-72, 8, 6},    {-73, 8, 6},    {-74, 8, 6},    {-75, 8, 6},    {-76, 8, 6},    {-77, 8, 6},    {-78, 8, 6},
    {-79, 8, 6},    {-80, 8, 6},    {-81, 8, 6},    {-82, 8, 6},    {-83, 8, 6},    {-84, 8, 6},    {-61, 9, 6},    {-62, 9, 6},    {-63, 9, 6},
    {-64, 9, 6},    {-65, 9, 6},    {-66, 9, 6},    {-67, 9, 6},    {-68, 9, 6},    {-69, 9, 6},    {-70, 9, 6},    {-71, 9, 6},    {-72, 9, 6},
    {-73, 9, 6},    {-74, 9, 6},    {-75, 9, 6},    {-76, 9, 6},    {-77, 9, 6},    {-78, 9, 6},    {-79, 9, 6},    {-80, 9, 6},    {-81, 9, 6},
    {-82, 9, 6},    {-83, 9, 6},    {-84, 9, 6},    {-85, 9, 6},    {-86, 9, 6},    {-61, 10, 6},   {-62, 10, 6},   {-63, 10, 6},   {-64, 10, 6},
    {-65, 10, 6},   {-66, 10, 6},   {-67, 10, 6},   {-68, 10, 6},   {-69, 10, 6},   {-70, 10, 6},   {-71, 10, 6},   {-72, 10, 6},   {-73, 10, 6},
    {-74, 10, 6},   {-75, 10, 6},   {-76, 10, 6},   {-84, 10, 6},   {-85, 10, 6},   {-86, 10, 6},   {-61, 11, 6},   {-62, 11, 6},   {-64, 11, 6},
    {-65, 11, 6},   {-67, 11, 6},   {-68, 11, 6},   {-69, 11, 6},   {-70, 11, 6},   {-71, 11, 6},   {-72, 11, 6},   {-73, 11, 6},   {-74, 11, 6},
    {-75, 11, 6},   {-84, 11, 6},   {-85, 11, 6},   {-86, 11, 6},   {-87, 11, 6},   {-62, 12, 6},   {-69, 12, 6},   {-70, 12, 6},   {-71, 12, 6},
    {-72, 12, 6},   {-73, 12, 6},   {-82, 12, 6},   {-83, 12, 6},   {-84, 12, 6},   {-85, 12, 6},   {-86, 12, 6},   {-87, 12, 6},   {-88, 12, 6},
    {-60, 13, 6},   {-61, 13, 6},   {-62, 13, 6},   {-81, 13, 6},   {-82, 13, 6},   {-84, 13, 6},   {-85, 13, 6},   {-86, 13, 6},   {-87, 13, 6},
    {-88, 13, 6},   {-89, 13, 6},   {-90, 13, 6},   {-91, 13, 6},   {-92, 13, 6},   {-61, 14, 6},   {-62, 14, 6},   {-81, 14, 6},   {-83, 14, 6},
    {-84, 14, 6},   {-85, 14, 6},   {-86, 14, 6},   {-87, 14, 6},   {-88, 14, 6},   {-89, 14, 6},   {-90, 14, 6},   {-91, 14, 6},   {-92, 14, 6},
    {-93, 14, 6},   {-47, -1, 6},   {-48, -1, 6},   {-49, -1, 6},   {-50, -1, 6},   {-51, -1, 6},   {-52, -1, 6},   {-53, -1, 6},   {-54, -1, 6},
    {-55, -1, 6},   {-56, -1, 6},   {-57, -1, 6},   {-58, -1, 6},   {-59, -1, 6},   {-60, -1, 6},   {-61, -1, 6},   {-62, -1, 6},   {-63, -1, 6},
    {-64, -1, 6},   {-65, -1, 6},   {-66, -1, 6},   {-67, -1, 6},   {-68, -1, 6},   {-69, -1, 6},   {-70, -1, 6},   {-71, -1, 6},   {-72, -1, 6},
    {-73, -1, 6},   {-74, -1, 6},   {-75, -1, 6},   {-76, -1, 6},   {-77, -1, 6},   {-78, -1, 6},   {-79, -1, 6},   {-80, -1, 6},   {-81, -1, 6},
    {-90, -1, 6},   {-91, -1, 6},   {-92, -1, 6},   {-45, -2, 6},   {-46, -2, 6},   {-47, -2, 6},   {-48, -2, 6},   {-49, -2, 6},   {-50, -2, 6},
    {-51, -2, 6},   {-52, -2, 6},   {-53, -2, 6},   {-54, -2, 6},   {-55, -2, 6},   {-56, -2, 6},   {-57, -2, 6},   {-58, -2, 6},   {-59, -2, 6},
    {-60, -2, 6},   {-61, -2, 6},   {-62, -2, 6},   {-63, -2, 6},   {-64, -2, 6},   {-65, -2, 6},   {-66, -2, 6},   {-67, -2, 6},   {-68, -2, 6},
    {-69, -2, 6},   {-70, -2, 6},   {-71, -2, 6},   {-72, -2, 6},   {-73, -2, 6},   {-74, -2, 6},   {-75, -2, 6},   {-76, -2, 6},   {-77, -2, 6},
    {-78, -2, 6},   {-79, -2, 6},   {-80, -2, 6},   {-81, -2, 6},   {-82, -2, 6},   {-90, -2, 6},   {-91, -2, 6},   {-92, -2, 6},   {-40, -3, 6},
    {-41, -3, 6},   {-42, -3, 6},   {-43, -3, 6},   {-44, -3, 6},   {-45, -3, 6},   {-46, -3, 6},   {-47, -3, 6},   {-48, -3, 6},   {-49, -3, 6},
    {-50, -3, 6},   {-51, -3, 6},   {-52, -3, 6},   {-53, -3, 6},   {-54, -3, 6},   {-55, -3, 6},   {-56, -3, 6},   {-57, -3, 6},   {-58, -3, 6},
    {-59, -3, 6},   {-60, -3, 6},   {-61, -3, 6},   {-62, -3, 6},   {-63, -3, 6},   {-64, -3, 6},   {-65, -3, 6},   {-66, -3, 6},   {-67, -3, 6},
    {-68, -3, 6},   {-69, -3, 6},   {-70, -3, 6},   {-71, -3, 6},   {-72, -3, 6},   {-73, -3, 6},   {-74, -3, 6},   {-75, -3, 6},   {-76, -3, 6},
    {-77, -3, 6},   {-78, -3, 6},   {-79, -3, 6},   {-80, -3, 6},   {-81, -3, 6},   {-82, -3, 6},   {-33, -4, 6},   {-34, -4, 6},   {-39, -4, 6},
    {-40, -4, 6},   {-41, -4, 6},   {-42, -4, 6},   {-43, -4, 6},   {-44, -4, 6},   {-45, -4, 6},   {-46, -4, 6},   {-47, -4, 6},   {-48, -4, 6},
    {-49, -4, 6},   {-50, -4, 6},   {-51, -4, 6},   {-52, -4, 6},   {-53, -4, 6},   {-54, -4, 6},   {-55, -4, 6},   {-56, -4, 6},   {-57, -4, 6},
    {-58, -4, 6},   {-59, -4, 6},   {-60, -4, 6},   {-61, -4, 6},   {-62, -4, 6},   {-63, -4, 6},   {-64, -4, 6},   {-65, -4, 6},   {-66, -4, 6},
    {-67, -4, 6},   {-68, -4, 6},   {-69, -4, 6},   {-70, -4, 6},   {-71, -4, 6},   {-72, -4, 6},   {-73, -4, 6},   {-74, -4, 6},   {-75, -4, 6},
    {-76, -4, 6},   {-77, -4, 6},   {-78, -4, 6},   {-79, -4, 6},   {-80, -4, 6},   {-81, -4, 6},   {-37, -5, 6},   {-38, -5, 6},   {-39, -5, 6},
    {-40, -5, 6},   {-41, -5, 6},   {-42, -5, 6},   {-43, -5, 6},   {-44, -5, 6},   {-45, -5, 6},   {-46, -5, 6},   {-47, -5, 6},   {-48, -5, 6},
    {-49, -5, 6},   {-50, -5, 6},   {-51, -5, 6},   {-52, -5, 6},   {-53, -5, 6},   {-54, -5, 6},   {-55, -5, 6},   {-56, -5, 6},   {-57, -5, 6},
    {-58, -5, 6},   {-59, -5, 6},   {-60, -5, 6},   {-61, -5, 6},   {-62, -5, 6},   {-63, -5, 6},   {-64, -5, 6},   {-65, -5, 6},   {-66, -5, 6},
    {-67, -5, 6},   {-68, -5, 6},   {-69, -5, 6},   {-70, -5, 6},   {-71, -5, 6},   {-72, -5, 6},   {-73, -5, 6},   {-74, -5, 6},   {-75, -5, 6},
    {-76, -5, 6},   {-77, -5, 6},   {-78, -5, 6},   {-79, -5, 6},   {-80, -5, 6},   {-81, -5, 6},   {-82, -5, 6},   {-36, -6, 6},   {-37, -6, 6},
    {-38, -6, 6},   {-39, -6, 6},   {-40, -6, 6},   {-41, -6, 6},   {-42, -6, 6},   {-43, -6, 6},   {-44, -6, 6},   {-45, -6, 6},   {-46, -6, 6},
    {-47, -6, 6},   {-48, -6, 6},   {-49, -6, 6},   {-50, -6, 6},   {-51, -6, 6},   {-52, -6, 6},   {-53, -6, 6},   {-54, -6, 6},   {-55, -6, 6},
    {-56, -6, 6},   {-57, -6, 6},   {-58, -6, 6},   {-59, -6, 6},   {-60, -6, 6},   {-61, -6, 6},   {-62, -6, 6},   {-63, -6, 6},   {-64, -6, 6},
    {-65, -6, 6},   {-66, -6, 6},   {-67, -6, 6},   {-68, -6, 6},   {-69, -6, 6},   {-70, -6, 6},   {-71, -6, 6},   {-72, -6, 6},   {-73, -6, 6},
    {-74, -6, 6},   {-75, -6, 6},   {-76, -6, 6},   {-77, -6, 6},   {-78, -6, 6},   {-79, -6, 6},   {-80, -6, 6},   {-81, -6, 6},   {-82, -6, 6},
    {-35, -7, 6},   {-36, -7, 6},   {-37, -7, 6},   {-38, -7, 6},   {-39, -7, 6},   {-40, -7, 6},   {-41, -7, 6},   {-42, -7, 6},   {-43, -7, 6},
    {-44, -7, 6},   {-45, -7, 6},   {-46, -7, 6},   {-47, -7, 6},   {-48, -7, 6},   {-49, -7, 6},   {-50, -7, 6},   {-51, -7, 6},   {-52, -7, 6},
    {-53, -7, 6},   {-54, -7, 6},   {-55, -7, 6},   {-56, -7, 6},   {-57, -7, 6},   {-58, -7, 6},   {-59, -7, 6},   {-60, -7, 6},   {-61, -7, 6},
    {-62, -7, 6},   {-63, -7, 6},   {-64, -7, 6},   {-65, -7, 6},   {-66, -7, 6},   {-67, -7, 6},   {-68, -7, 6},   {-69, -7, 6},   {-70, -7, 6},
    {-71, -7, 6},   {-72, -7, 6},   {-73, -7, 6},   {-74, -7, 6},   {-75, -7, 6},   {-76, -7, 6},   {-77, -7, 6},   {-78, -7, 6},   {-79, -7, 6},
    {-80, -7, 6},   {-81, -7, 6},   {-82, -7, 6},   {-35, -8, 6},   {-36, -8, 6},   {-37, -8, 6},   {-38, -8, 6},   {-39, -8, 6},   {-40, -8, 6},
    {-41, -8, 6},   {-42, -8, 6},   {-43, -8, 6},   {-44, -8, 6},   {-45, -8, 6},   {-46, -8, 6},   {-47, -8, 6},   {-48, -8, 6},   {-49, -8, 6},
    {-50, -8, 6},   {-51, -8, 6},   {-52, -8, 6},   {-53, -8, 6},   {-54, -8, 6},   {-55, -8, 6},   {-56, -8, 6},   {-57, -8, 6},   {-58, -8, 6},
    {-59, -8, 6},   {-60, -8, 6},   {-61, -8, 6},   {-62, -8, 6},   {-63, -8, 6},   {-64, -8, 6},   {-65, -8, 6},   {-66, -8, 6},   {-67, -8, 6},
    {-68, -8, 6},   {-69, -8, 6},   {-70, -8, 6},   {-71, -8, 6},   {-72, -8, 6},   {-73, -8, 6},   {-74, -8, 6},   {-75, -8, 6},   {-76, -8, 6},
    {-77, -8, 6},   {-78, -8, 6},   {-79, -8, 6},   {-80, -8, 6},   {-35, -9, 6},   {-36, -9, 6},   {-37, -9, 6},   {-38, -9, 6},   {-39, -9, 6},
    {-40, -9, 6},   {-41, -9, 6},   {-42, -9, 6},   {-43, -9, 6},   {-44, -9, 6},   {-45, -9, 6},   {-46, -9, 6},   {-47, -9, 6},   {-48, -9, 6},
    {-49, -9, 6},   {-50, -9, 6},   {-51, -9, 6},   {-52, -9, 6},   {-53, -9, 6},   {-54, -9, 6},   {-55, -9, 6},   {-56, -9, 6},   {-57, -9, 6},
    {-58, -9, 6},   {-59, -9, 6},   {-60, -9, 6},   {-61, -9, 6},   {-62, -9, 6},   {-63, -9, 6},   {-64, -9, 6},   {-65, -9, 6},   {-66, -9, 6},
    {-67, -9, 6},   {-68, -9, 6},   {-69, -9, 6},   {-70, -9, 6},   {-71, -9, 6},   {-72, -9, 6},   {-73, -9, 6},   {-74, -9, 6},   {-75, -9, 6},
    {-76, -9, 6},   {-77, -9, 6},   {-78, -9, 6},   {-79, -9, 6},   {-80, -9, 6},   {-36, -10, 6},  {-37, -10, 6},  {-38, -10, 6},  {-39, -10, 6},
    {-40, -10, 6},  {-41, -10, 6},  {-42, -10, 6},  {-43, -10, 6},  {-44, -10, 6},  {-45, -10, 6},  {-46, -10, 6},  {-47, -10, 6},  {-48, -10, 6},
    {-49, -10, 6},  {-50, -10, 6},  {-51, -10, 6},  {-52, -10, 6},  {-53, -10, 6},  {-54, -10, 6},  {-55, -10, 6},  {-56, -10, 6},  {-57, -10, 6},
    {-58, -10, 6},  {-59, -10, 6},  {-60, -10, 6},  {-61, -10, 6},  {-62, -10, 6},  {-63, -10, 6},  {-64, -10, 6},  {-65, -10, 6},  {-66, -10, 6},
    {-67, -10, 6},  {-68, -10, 6},  {-69, -10, 6},  {-70, -10, 6},  {-71, -10, 6},  {-72, -10, 6},  {-73, -10, 6},  {-74, -10, 6},  {-75, -10, 6},
    {-76, -10, 6},  {-77, -10, 6},  {-78, -10, 6},  {-79, -10, 6},  {-37, -11, 6},  {-38, -11, 6},  {-39, -11, 6},  {-40, -11, 6},  {-41, -11, 6},
    {-42, -11, 6},  {-43, -11, 6},  {-44, -11, 6},  {-45, -11, 6},  {-46, -11, 6},  {-47, -11, 6},  {-48, -11, 6},  {-49, -11, 6},  {-50, -11, 6},
    {-51, -11, 6},  {-52, -11, 6},  {-53, -11, 6},  {-54, -11, 6},  {-55, -11, 6},  {-56, -11, 6},  {-57, -11, 6},  {-58, -11, 6},  {-59, -11, 6},
    {-60, -11, 6},  {-61, -11, 6},  {-62, -11, 6},  {-63, -11, 6},  {-64, -11, 6},  {-65, -11, 6},  {-66, -11, 6},  {-67, -11, 6},  {-68, -11, 6},
    {-69, -11, 6},  {-70, -11, 6},  {-71, -11, 6},  {-72, -11, 6},  {-73, -11, 6},  {-74, -11, 6},  {-75, -11, 6},  {-76, -11, 6},  {-77, -11, 6},
    {-78, -11, 6},  {-79, -11, 6},  {-38, -12, 6},  {-39, -12, 6},  {-40, -12, 6},  {-41, -12, 6},  {-42, -12, 6},  {-43, -12, 6},  {-44, -12, 6},
    {-45, -12, 6},  {-46, -12, 6},  {-47, -12, 6},  {-48, -12, 6},  {-49, -12, 6},  {-50, -12, 6},  {-51, -12, 6},  {-52, -12, 6},  {-53, -12, 6},
    {-54, -12, 6},  {-55, -12, 6},  {-56, -12, 6},  {-57, -12, 6},  {-58, -12, 6},  {-59, -12, 6},  {-60, -12, 6},  {-61, -12, 6},  {-62, -12, 6},
    {-63, -12, 6},  {-64, -12, 6},  {-65, -12, 6},  {-66, -12, 6},  {-67, -12, 6},  {-68, -12, 6},  {-69, -12, 6},  {-70, -12, 6},  {-71, -12, 6},
    {-72, -12, 6},  {-73, -12, 6},  {-74, -12, 6},  {-75, -12, 6},  {-76, -12, 6},  {-77, -12, 6},  {-78, -12, 6},  {-38, -13, 6},  {-39, -13, 6},
    {-40, -13, 6},  {-41, -13, 6},  {-42, -13, 6},  {-43, -13, 6},  {-44, -13, 6},  {-45, -13, 6},  {-46, -13, 6},  {-47, -13, 6},  {-48, -13, 6},
    {-49, -13, 6},  {-50, -13, 6},  {-51, -13, 6},  {-52, -13, 6},  {-53, -13, 6},  {-54, -13, 6},  {-55, -13, 6},  {-56, -13, 6},  {-57, -13, 6},
    {-58, -13, 6},  {-59, -13, 6},  {-60, -13, 6},  {-61, -13, 6},  {-62, -13, 6},  {-63, -13, 6},  {-64, -13, 6},  {-65, -13, 6},  {-66, -13, 6},
    {-67, -13, 6},  {-68, -13, 6},  {-69, -13, 6},  {-70, -13, 6},  {-71, -13, 6},  {-72, -13, 6},  {-73, -13, 6},  {-74, -13, 6},  {-75, -13, 6},
    {-76, -13, 6},  {-77, -13, 6},  {-78, -13, 6},  {-39, -14, 6},  {-40, -14, 6},  {-41, -14, 6},  {-42, -14, 6},  {-43, -14, 6},  {-44, -14, 6},
    {-45, -14, 6},  {-46, -14, 6},  {-47, -14, 6},  {-48, -14, 6},  {-49, -14, 6},  {-50, -14, 6},  {-51, -14, 6},  {-52, -14, 6},  {-53, -14, 6},
    {-54, -14, 6},  {-55, -14, 6},  {-56, -14, 6},  {-57, -14, 6},  {-58, -14, 6},  {-59, -14, 6},  {-60, -14, 6},  {-61, -14, 6},  {-62, -14, 6},
    {-63, -14, 6},  {-64, -14, 6},  {-65, -14, 6},  {-66, -14, 6},  {-67, -14, 6},  {-68, -14, 6},  {-69, -14, 6},  {-70, -14, 6},  {-71, -14, 6},
    {-72, -14, 6},  {-73, -14, 6},  {-74, -14, 6},  {-75, -14, 6},  {-76, -14, 6},  {-77, -14, 6},  {-39, -15, 6},  {-40, -15, 6},  {-41, -15, 6},
    {-42, -15, 6},  {-43, -15, 6},  {-44, -15, 6},  {-45, -15, 6},  {-46, -15, 6},  {-47, -15, 6},  {-48, -15, 6},  {-49, -15, 6},  {-50, -15, 6},
    {-51, -15, 6},  {-52, -15, 6},  {-53, -15, 6},  {-54, -15, 6},  {-55, -15, 6},  {-56, -15, 6},  {-57, -15, 6},  {-58, -15, 6},  {-59, -15, 6},
    {-60, -15, 6},  {-61, -15, 6},  {-62, -15, 6},  {-63, -15, 6},  {-64, -15, 6},  {-65, -15, 6},  {-66, -15, 6},  {-67, -15, 6},  {-68, -15, 6},
    {-69, -15, 6},  {-70, -15, 6},  {-71, -15, 6},  {-72, -15, 6},  {-73, -15, 6},  {-74, -15, 6},  {-75, -15, 6},  {-76, -15, 6},  {-77, -15, 6},
    {-39, -16, 6},  {-40, -16, 6},  {-41, -16, 6},  {-42, -16, 6},  {-43, -16, 6},  {-44, -16, 6},  {-45, -16, 6},  {-46, -16, 6},  {-47, -16, 6},
    {-48, -16, 6},  {-49, -16, 6},  {-50, -16, 6},  {-51, -16, 6},  {-52, -16, 6},  {-53, -16, 6},  {-54, -16, 6},  {-55, -16, 6},  {-56, -16, 6},
    {-57, -16, 6},  {-58, -16, 6},  {-59, -16, 6},  {-60, -16, 6},  {-61, -16, 6},  {-62, -16, 6},  {-63, -16, 6},  {-64, -16, 6},  {-65, -16, 6},
    {-66, -16, 6},  {-67, -16, 6},  {-68, -16, 6},  {-69, -16, 6},  {-70, -16, 6},  {-71, -16, 6},  {-72, -16, 6},  {-73, -16, 6},  {-74, -16, 6},
    {-75, -16, 6},  {-76, -16, 6},  {-39, -17, 6},  {-40, -17, 6},  {-41, -17, 6},  {-42, -17, 6},  {-43, -17, 6},  {-44, -17, 6},  {-45, -17, 6},
    {-46, -17, 6},  {-47, -17, 6},  {-48, -17, 6},  {-49, -17, 6},  {-50, -17, 6},  {-51, -17, 6},  {-52, -17, 6},  {-53, -17, 6},  {-54, -17, 6},
    {-55, -17, 6},  {-56, -17, 6},  {-57, -17, 6},  {-58, -17, 6},  {-59, -17, 6},  {-60, -17, 6},  {-61, -17, 6},  {-62, -17, 6},  {-63, -17, 6},
    {-64, -17, 6},  {-65, -17, 6},  {-66, -17, 6},  {-67, -17, 6},  {-68, -17, 6},  {-69, -17, 6},  {-70, -17, 6},  {-71, -17, 6},  {-72, -17, 6},
    {-73, -17, 6},  {-74, -17, 6},  {-75, -17, 6},  {-39, -18, 6},  {-40, -18, 6},  {-41, -18, 6},  {-42, -18, 6},  {-43, -18, 6},  {-44, -18, 6},
    {-45, -18, 6},  {-46, -18, 6},  {-47, -18, 6},  {-48, -18, 6},  {-49, -18, 6},  {-50, -18, 6},  {-51, -18, 6},  {-52, -18, 6},  {-53, -18, 6},
    {-54, -18, 6},  {-55, -18, 6},  {-56, -18, 6},  {-57, -18, 6},  {-58, -18, 6},  {-59, -18, 6},  {-60, -18, 6},  {-61, -18, 6},  {-62, -18, 6},
    {-63, -18, 6},  {-64, -18, 6},  {-65, -18, 6},  {-66, -18, 6},  {-67, -18, 6},  {-68, -18, 6},  {-69, -18, 6},  {-70, -18, 6},  {-71, -18, 6},
    {-72, -18, 6},  {-73, -18, 6},  {-40, -19, 6},  {-41, -19, 6},  {-42, -19, 6},  {-43, -19, 6},  {-44, -19, 6},  {-45, -19, 6},  {-46, -19, 6},
    {-47, -19, 6},  {-48, -19, 6},  {-49, -19, 6},  {-50, -19, 6},  {-51, -19, 6},  {-52, -19, 6},  {-53, -19, 6},  {-54, -19, 6},  {-55, -19, 6},
    {-56, -19, 6},  {-57, -19, 6},  {-58, -19, 6},  {-59, -19, 6},  {-60, -19, 6},  {-61, -19, 6},  {-62, -19, 6},  {-63, -19, 6},  {-64, -19, 6},
    {-65, -19, 6},  {-66, -19, 6},  {-67, -19, 6},  {-68, -19, 6},  {-69, -19, 6},  {-70, -19, 6},  {-71, -19, 6},  {-40, -20, 6},  {-41, -20, 6},
    {-42, -20, 6},  {-43, -20, 6},  {-44, -20, 6},  {-45, -20, 6},  {-46, -20, 6},  {-47, -20, 6},  {-48, -20, 6},  {-49, -20, 6},  {-50, -20, 6},
    {-51, -20, 6},  {-52, -20, 6},  {-53, -20, 6},  {-54, -20, 6},  {-55, -20, 6},  {-56, -20, 6},  {-57, -20, 6},  {-58, -20, 6},  {-59, -20, 6},
    {-60, -20, 6},  {-61, -20, 6},  {-62, -20, 6},  {-63, -20, 6},  {-64, -20, 6},  {-65, -20, 6},  {-66, -20, 6},  {-67, -20, 6},  {-68, -20, 6},
    {-69, -20, 6},  {-70, -20, 6},  {-71, -20, 6},  {-41, -21, 6},  {-42, -21, 6},  {-43, -21, 6},  {-44, -21, 6},  {-45, -21, 6},  {-46, -21, 6},
    {-47, -21, 6},  {-48, -21, 6},  {-49, -21, 6},  {-50, -21, 6},  {-51, -21, 6},  {-52, -21, 6},  {-53, -21, 6},  {-54, -21, 6},  {-55, -21, 6},
    {-56, -21, 6},  {-57, -21, 6},  {-58, -21, 6},  {-59, -21, 6},  {-60, -21, 6},  {-61, -21, 6},  {-62, -21, 6},  {-63, -21, 6},  {-64, -21, 6},
    {-65, -21, 6},  {-66, -21, 6},  {-67, -21, 6},  {-68, -21, 6},  {-69, -21, 6},  {-70, -21, 6},  {-71, -21, 6},  {-41, -22, 6},  {-42, -22, 6},
    {-43, -22, 6},  {-44, -22, 6},  {-45, -22, 6},  {-46, -22, 6},  {-47, -22, 6},  {-48, -22, 6},  {-49, -22, 6},  {-50, -22, 6},  {-51, -22, 6},
    {-52, -22, 6},  {-53, -22, 6},  {-54, -22, 6},  {-55, -22, 6},  {-56, -22, 6},  {-57, -22, 6},  {-58, -22, 6},  {-59, -22, 6},  {-60, -22, 6},
    {-61, -22, 6},  {-62, -22, 6},  {-63, -22, 6},  {-64, -22, 6},  {-65, -22, 6},  {-66, -22, 6},  {-67, -22, 6},  {-68, -22, 6},  {-69, -22, 6},
    {-70, -22, 6},  {-71, -22, 6},  {-41, -23, 6},  {-42, -23, 6},  {-43, -23, 6},  {-44, -23, 6},  {-45, -23, 6},  {-46, -23, 6},  {-47, -23, 6},
    {-48, -23, 6},  {-49, -23, 6},  {-50, -23, 6},  {-51, -23, 6},  {-52, -23, 6},  {-53, -23, 6},  {-54, -23, 6},  {-55, -23, 6},  {-56, -23, 6},
    {-57, -23, 6},  {-58, -23, 6},  {-59, -23, 6},  {-60, -23, 6},  {-61, -23, 6},  {-62, -23, 6},  {-63, -23, 6},  {-64, -23, 6},  {-65, -23, 6},
    {-66, -23, 6},  {-67, -23, 6},  {-68, -23, 6},  {-69, -23, 6},  {-70, -23, 6},  {-71, -23, 6},  {-42, -24, 6},  {-43, -24, 6},  {-44, -24, 6},
    {-45, -24, 6},  {-46, -24, 6},  {-47, -24, 6},  {-48, -24, 6},  {-49, -24, 6},  {-50, -24, 6},  {-51, -24, 6},  {-52, -24, 6},  {-53, -24, 6},
    {-54, -24, 6},  {-55, -24, 6},  {-56, -24, 6},  {-57, -24, 6},  {-58, -24, 6},  {-59, -24, 6},  {-60, -24, 6},  {-61, -24, 6},  {-62, -24, 6},
    {-63, -24, 6},  {-64, -24, 6},  {-65, -24, 6},  {-66, -24, 6},  {-67, -24, 6},  {-68, -24, 6},  {-69, -24, 6},  {-70, -24, 6},  {-71, -24, 6},
    {-46, -25, 6},  {-47, -25, 6},  {-48, -25, 6},  {-49, -25, 6},  {-50, -25, 6},  {-51, -25, 6},  {-52, -25, 6},  {-53, -25, 6},  {-54, -25, 6},
    {-55, -25, 6},  {-56, -25, 6},  {-57, -25, 6},  {-58, -25, 6},  {-59, -25, 6},  {-60, -25, 6},  {-61, -25, 6},  {-62, -25, 6},  {-63, -25, 6},
    {-64, -25, 6},  {-65, -25, 6},  {-66, -25, 6},  {-67, -25, 6},  {-68, -25, 6},  {-69, -25, 6},  {-70, -25, 6},  {-71, -25, 6},  {-48, -26, 6},
    {-49, -26, 6},  {-50, -26, 6},  {-51, -26, 6},  {-52, -26, 6},  {-53, -26, 6},  {-54, -26, 6},  {-55, -26, 6},  {-56, -26, 6},  {-57, -26, 6},
    {-58, -26, 6},  {-59, -26, 6},  {-60, -26, 6},  {-61, -26, 6},  {-62, -26, 6},  {-63, -26, 6},  {-64, -26, 6},  {-65, -26, 6},  {-66, -26, 6},
    {-67, -26, 6},  {-68, -26, 6},  {-69, -26, 6},  {-70, -26, 6},  {-71, -26, 6},  {-49, -27, 6},  {-50, -27, 6},  {-51, -27, 6},  {-52, -27, 6},
    {-53, -27, 6},  {-54, -27, 6},  {-55, -27, 6},  {-56, -27, 6},  {-57, -27, 6},  {-58, -27, 6},  {-59, -27, 6},  {-60, -27, 6},  {-61, -27, 6},
    {-62, -27, 6},  {-63, -27, 6},  {-64, -27, 6},  {-65, -27, 6},  {-66, -27, 6},  {-67, -27, 6},  {-68, -27, 6},  {-69, -27, 6},  {-70, -27, 6},
    {-71, -27, 6},  {-80, -27, 6},  {-81, -27, 6},  {-49, -28, 6},  {-50, -28, 6},  {-51, -28, 6},  {-52, -28, 6},  {-53, -28, 6},  {-54, -28, 6},
    {-55, -28, 6},  {-56, -28, 6},  {-57, -28, 6},  {-58, -28, 6},  {-59, -28, 6},  {-60, -28, 6},  {-61, -28, 6},  {-62, -28, 6},  {-63, -28, 6},
    {-64, -28, 6},  {-65, -28, 6},  {-66, -28, 6},  {-67, -28, 6},  {-68, -28, 6},  {-69, -28, 6},  {-70, -28, 6},  {-71, -28, 6},  {-72, -28, 6},
    {-49, -29, 6},  {-50, -29, 6},  {-51, -29, 6},  {-52, -29, 6},  {-53, -29, 6},  {-54, -29, 6},  {-55, -29, 6},  {-56, -29, 6},  {-57, -29, 6},
    {-58, -29, 6},  {-59, -29, 6},  {-60, -29, 6},  {-61, -29, 6},  {-62, -29, 6},  {-63, -29, 6},  {-64, -29, 6},  {-65, -29, 6},  {-66, -29, 6},
    {-67, -29, 6},  {-68, -29, 6},  {-69, -29, 6},  {-70, -29, 6},  {-71, -29, 6},  {-72, -29, 6},  {-50, -30, 6},  {-51, -30, 6},  {-52, -30, 6},
    {-53, -30, 6},  {-54, -30, 6},  {-55, -30, 6},  {-56, -30, 6},  {-57, -30, 6},  {-58, -30, 6},  {-59, -30, 6},  {-60, -30, 6},  {-61, -30, 6},
    {-62, -30, 6},  {-63, -30, 6},  {-64, -30, 6},  {-65, -30, 6},  {-66, -30, 6},  {-67, -30, 6},  {-68, -30, 6},  {-69, -30, 6},  {-70, -30, 6},
    {-71, -30, 6},  {-72, -30, 6},  {-51, -31, 6},  {-52, -31, 6},  {-53, -31, 6},  {-54, -31, 6},  {-55, -31, 6},  {-56, -31, 6},  {-57, -31, 6},
    {-58, -31, 6},  {-59, -31, 6},  {-60, -31, 6},  {-61, -31, 6},  {-62, -31, 6},  {-63, -31, 6},  {-64, -31, 6},  {-65, -31, 6},  {-66, -31, 6},
    {-67, -31, 6},  {-68, -31, 6},  {-69, -31, 6},  {-70, -31, 6},  {-71, -31, 6},  {-72, -31, 6},  {-51, -32, 6},  {-52, -32, 6},  {-53, -32, 6},
    {-54, -32, 6},  {-55, -32, 6},  {-56, -32, 6},  {-57, -32, 6},  {-58, -32, 6},  {-59, -32, 6},  {-60, -32, 6},  {-61, -32, 6},  {-62, -32, 6},
    {-63, -32, 6},  {-64, -32, 6},  {-65, -32, 6},  {-66, -32, 6},  {-67, -32, 6},  {-68, -32, 6},  {-69, -32, 6},  {-70, -32, 6},  {-71, -32, 6},
    {-72, -32, 6},  {-52, -33, 6},  {-53, -33, 6},  {-54, -33, 6},  {-55, -33, 6},  {-56, -33, 6},  {-57, -33, 6},  {-58, -33, 6},  {-59, -33, 6},
    {-60, -33, 6},  {-61, -33, 6},  {-62, -33, 6},  {-63, -33, 6},  {-64, -33, 6},  {-65, -33, 6},  {-66, -33, 6},  {-67, -33, 6},  {-68, -33, 6},
    {-69, -33, 6},  {-70, -33, 6},  {-71, -33, 6},  {-72, -33, 6},  {-53, -34, 6},  {-54, -34, 6},  {-55, -34, 6},  {-56, -34, 6},  {-57, -34, 6},
    {-58, -34, 6},  {-59, -34, 6},  {-60, -34, 6},  {-61, -34, 6},  {-62, -34, 6},  {-63, -34, 6},  {-64, -34, 6},  {-65, -34, 6},  {-66, -34, 6},
    {-67, -34, 6},  {-68, -34, 6},  {-69, -34, 6},  {-70, -34, 6},  {-71, -34, 6},  {-72, -34, 6},  {-79, -34, 6},  {-81, -34, 6},  {-54, -35, 6},
    {-55, -35, 6},  {-56, -35, 6},  {-57, -35, 6},  {-58, -35, 6},  {-59, -35, 6},  {-60, -35, 6},  {-61, -35, 6},  {-62, -35, 6},  {-63, -35, 6},
    {-64, -35, 6},  {-65, -35, 6},  {-66, -35, 6},  {-67, -35, 6},  {-68, -35, 6},  {-69, -35, 6},  {-70, -35, 6},  {-71, -35, 6},  {-72, -35, 6},
    {-73, -35, 6},  {-58, -36, 6},  {-59, -36, 6},  {-60, -36, 6},  {-61, -36, 6},  {-62, -36, 6},  {-63, -36, 6},  {-64, -36, 6},  {-65, -36, 6},
    {-66, -36, 6},  {-67, -36, 6},  {-68, -36, 6},  {-69, -36, 6},  {-70, -36, 6},  {-71, -36, 6},  {-72, -36, 6},  {-73, -36, 6},  {-57, -37, 6},
    {-58, -37, 6},  {-59, -37, 6},  {-60, -37, 6},  {-61, -37, 6},  {-62, -37, 6},  {-63, -37, 6},  {-64, -37, 6},  {-65, -37, 6},  {-66, -37, 6},
    {-67, -37, 6},  {-68, -37, 6},  {-69, -37, 6},  {-70, -37, 6},  {-71, -37, 6},  {-72, -37, 6},  {-73, -37, 6},  {-74, -37, 6},  {-57, -38, 6},
    {-58, -38, 6},  {-59, -38, 6},  {-60, -38, 6},  {-61, -38, 6},  {-62, -38, 6},  {-63, -38, 6},  {-64, -38, 6},  {-65, -38, 6},  {-66, -38, 6},
    {-67, -38, 6},  {-68, -38, 6},  {-69, -38, 6},  {-70, -38, 6},  {-71, -38, 6},  {-72, -38, 6},  {-73, -38, 6},  {-74, -38, 6},  {-58, -39, 6},
    {-59, -39, 6},  {-60, -39, 6},  {-61, -39, 6},  {-62, -39, 6},  {-63, -39, 6},  {-64, -39, 6},  {-65, -39, 6},  {-66, -39, 6},  {-67, -39, 6},
    {-68, -39, 6},  {-69, -39, 6},  {-70, -39, 6},  {-71, -39, 6},  {-72, -39, 6},  {-73, -39, 6},  {-74, -39, 6},  {-62, -40, 6},  {-63, -40, 6},
    {-64, -40, 6},  {-65, -40, 6},  {-66, -40, 6},  {-67, -40, 6},  {-68, -40, 6},  {-69, -40, 6},  {-70, -40, 6},  {-71, -40, 6},  {-72, -40, 6},
    {-73, -40, 6},  {-74, -40, 6},  {-63, -41, 6},  {-64, -41, 6},  {-65, -41, 6},  {-66, -41, 6},  {-67, -41, 6},  {-68, -41, 6},  {-69, -41, 6},
    {-70, -41, 6},  {-71, -41, 6},  {-72, -41, 6},  {-73, -41, 6},  {-74, -41, 6},  {-63, -42, 6},  {-64, -42, 6},  {-65, -42, 6},  {-66, -42, 6},
    {-67, -42, 6},  {-68, -42, 6},  {-69, -42, 6},  {-70, -42, 6},  {-71, -42, 6},  {-72, -42, 6},  {-73, -42, 6},  {-74, -42, 6},  {-75, -42, 6},
    {-64, -43, 6},  {-65, -43, 6},  {-66, -43, 6},  {-67, -43, 6},  {-68, -43, 6},  {-69, -43, 6},  {-70, -43, 6},  {-71, -43, 6},  {-72, -43, 6},
    {-73, -43, 6},  {-74, -43, 6},  {-75, -43, 6},  {-65, -44, 6},  {-66, -44, 6},  {-67, -44, 6},  {-68, -44, 6},  {-69, -44, 6},  {-70, -44, 6},
    {-71, -44, 6},  {-72, -44, 6},  {-73, -44, 6},  {-74, -44, 6},  {-75, -44, 6},  {-66, -45, 6},  {-67, -45, 6},  {-68, -45, 6},  {-69, -45, 6},
    {-70, -45, 6},  {-71, -45, 6},  {-72, -45, 6},  {-73, -45, 6},  {-74, -45, 6},  {-75, -45, 6},  {-76, -45, 6},  {-66, -46, 6},  {-67, -46, 6},
    {-68, -46, 6},  {-69, -46, 6},  {-70, -46, 6},  {-71, -46, 6},  {-72, -46, 6},  {-73, -46, 6},  {-74, -46, 6},  {-75, -46, 6},  {-76, -46, 6},
    {-67, -47, 6},  {-68, -47, 6},  {-69, -47, 6},  {-70, -47, 6},  {-71, -47, 6},  {-72, -47, 6},  {-73, -47, 6},  {-74, -47, 6},  {-75, -47, 6},
    {-76, -47, 6},  {-66, -48, 6},  {-67, -48, 6},  {-68, -48, 6},  {-69, -48, 6},  {-70, -48, 6},  {-71, -48, 6},  {-72, -48, 6},  {-73, -48, 6},
    {-74, -48, 6},  {-75, -48, 6},  {-76, -48, 6},  {-66, -49, 6},  {-67, -49, 6},  {-68, -49, 6},  {-69, -49, 6},  {-70, -49, 6},  {-71, -49, 6},
    {-72, -49, 6},  {-73, -49, 6},  {-74, -49, 6},  {-75, -49, 6},  {-76, -49, 6},  {-68, -50, 6},  {-69, -50, 6},  {-70, -50, 6},  {-71, -50, 6},
    {-72, -50, 6},  {-73, -50, 6},  {-74, -50, 6},  {-75, -50, 6},  {-76, -50, 6},  {-62, -51, 6},  {-68, -51, 6},  {-69, -51, 6},  {-70, -51, 6},
    {-71, -51, 6},  {-72, -51, 6},  {-73, -51, 6},  {-74, -51, 6},  {-75, -51, 6},  {-76, -51, 6},  {-58, -52, 6},  {-59, -52, 6},  {-60, -52, 6},
    {-61, -52, 6},  {-62, -52, 6},  {-69, -52, 6},  {-70, -52, 6},  {-71, -52, 6},  {-72, -52, 6},  {-73, -52, 6},  {-74, -52, 6},  {-75, -52, 6},
    {-76, -52, 6},  {-59, -53, 6},  {-60, -53, 6},  {-61, -53, 6},  {-62, -53, 6},  {-69, -53, 6},  {-70, -53, 6},  {-71, -53, 6},  {-72, -53, 6},
    {-73, -53, 6},  {-74, -53, 6},  {-75, -53, 6},  {-76, -53, 6},  {-68, -54, 6},  {-69, -54, 6},  {-70, -54, 6},  {-71, -54, 6},  {-72, -54, 6},
    {-73, -54, 6},  {-74, -54, 6},  {-75, -54, 6},  {-64, -55, 6},  {-65, -55, 6},  {-66, -55, 6},  {-67, -55, 6},  {-68, -55, 6},  {-69, -55, 6},
    {-70, -55, 6},  {-71, -55, 6},  {-72, -55, 6},  {-73, -55, 6},  {-74, -55, 6},  {-67, -56, 6},  {-68, -56, 6},  {-69, -56, 6},  {-70, -56, 6}};

otb::Wrapper::DownloadSRTMTiles::DownloadSRTMTiles() : m_SRTMTileList(std::begin(staticTileList), std::end(staticTileList))
{
}

OTB_APPLICATION_EXPORT(otb::Wrapper::DownloadSRTMTiles)
